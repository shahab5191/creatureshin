#include "network.hpp"

#include <cmath>
#include <limits>

using namespace ANNNetwork;
namespace cfg = annshin::config;
using annshin::math::cap;   // §4 core helpers, shared via math.hpp
using annshin::math::decay;

namespace {

// Never-fired sentinel: far enough in the past that is_recent() is always false,
// but safe from signed overflow when subtracted from a small positive tick.
constexpr tick_t NEVER = std::numeric_limits<tick_t>::min() / 2;

// Swap-remove a value from an unordered index list (order doesn't matter).
void remove_value(std::vector<int> &v, int val) {
  for (std::size_t i = 0; i < v.size(); ++i)
    if (v[i] == val) {
      v[i] = v.back();
      v.pop_back();
      return;
    }
}

} // namespace

Network::Network(int num_neurons, int fanout) {
  neurons.resize(num_neurons);
  in_edges_.resize(num_neurons);
  out_edges_.resize(num_neurons);
  touched_mark.assign(num_neurons, NEVER);

  std::uniform_real_distribution<double> unit(0.0, 1.0);
  std::uniform_int_distribution<int> pick(0, num_neurons - 1);

  // --- neurons + spatial grid (positions are fixed hereafter, §11.1) ---
  for (int i = 0; i < num_neurons; ++i) {
    Neuron &n = neurons[i];
    n.v = 0.0;
    n.last_tick = 0;
    n.last_spike_time = NEVER;
    n.threshold = cfg::THETA0;
    n.threshold_tick = 0;
    n.refactory_time = 0;
    n.bias = 0.0;
    n.polarity = (unit(rng_) < cfg::INHIB_FRAC) ? -1 : +1;
    n.pos = {unit(rng_), unit(rng_), unit(rng_)};
    grid_.insert(i, n.pos.x, n.pos.y, n.pos.z);
  }

  // --- initial random synapses: `fanout` distinct targets per source ---
  for (int i = 0; i < num_neurons; ++i) {
    int made = 0, guard = 0;
    while (made < fanout && guard++ < fanout * 8) {
      int j = pick(rng_);
      if (j == i || edge_exists(i, j))
        continue;
      float w = static_cast<float>(cfg::W_INIT * (0.5 + unit(rng_)));
      create_edge(i, j, w, 0);
      ++made;
    }
  }
}

bool Network::is_recent(tick_t spike_time, tick_t t) const {
  return (t - spike_time) <= cfg::STDP_WINDOW;
}

void Network::mark_touched(int j, tick_t t) {
  if (touched_mark[j] != t) {
    touched_mark[j] = t;
    touched.push_back(j);
  }
}

// SPEC §5.5 — lazy trace decay + slow forgetting (LTD). Deletion is deferred:
// call sites push weak synapses to to_prune_, flushed at end of the tick.
void Network::touch_synapse(Synapse &s, tick_t t) {
  tick_t dt = t - s.touch_time;
  s.elig_tag = decay(s.elig_tag, dt, cfg::TAU_E);
  s.w = static_cast<float>(decay(s.w, dt, cfg::TAU_W));
  s.touch_time = t;
}

// --- structural plasticity (§11) -------------------------------------------

int Network::alloc_synapse(int i, int j, float w, tick_t t) {
  Synapse s;
  s.source = i;
  s.target = j;
  s.w = w;
  s.elig_tag = 0.0;
  s.touch_time = t;
  s.alive = true;
  if (!free_list_.empty()) {
    int gidx = free_list_.back();
    free_list_.pop_back();
    pool_[gidx] = s;
    return gidx;
  }
  pool_.push_back(s);
  return static_cast<int>(pool_.size()) - 1;
}

int Network::create_edge(int i, int j, float w, tick_t t) {
  int gidx = alloc_synapse(i, j, w, t);
  out_edges_[i].push_back(gidx);
  in_edges_[j].push_back(gidx);
  return gidx;
}

void Network::delete_edge(int gidx) {
  Synapse &s = pool_[gidx];
  if (!s.alive)
    return; // idempotent (a gidx may be marked twice in one tick)
  s.alive = false;
  remove_value(out_edges_[s.source], gidx);
  remove_value(in_edges_[s.target], gidx);
  free_list_.push_back(gidx);
}

bool Network::edge_exists(int i, int j) const {
  for (int gidx : out_edges_[i])
    if (pool_[gidx].target == j)
      return true;
  return false;
}

// SPEC §11.2 — when j fires, sprout inputs from nearby, recently-co-fired,
// not-yet-connected neurons. O(local density) via the spatial grid.
void Network::try_form(int j, tick_t t) {
  if (static_cast<int>(in_edges_[j].size()) >= cfg::SYN_BUDGET)
    return; // fan-in cap (§11.4); evict-weakest refinement is a TODO

  presyn_scratch_.clear();
  for (int gidx : in_edges_[j])
    presyn_scratch_.insert(pool_[gidx].source);

  const Vec3 pj = neurons[j].pos;
  const double r2 = cfg::R_REACH * cfg::R_REACH;
  grid_.for_neighbors(pj.x, pj.y, pj.z, [&](int k) {
    if (k == j)
      return;
    if (static_cast<int>(in_edges_[j].size()) >= cfg::SYN_BUDGET)
      return;
    const Neuron &nk = neurons[k];
    if (!is_recent(nk.last_spike_time, t)) // must have co-fired recently
      return;
    double dx = nk.pos.x - pj.x, dy = nk.pos.y - pj.y, dz = nk.pos.z - pj.z;
    if (dx * dx + dy * dy + dz * dz > r2) // exact reach (§11.1)
      return;
    if (presyn_scratch_.count(k)) // not already connected
      return;
    if (uni01_(rng_) < cfg::P_FORM) { // probabilistic formation (§11.2)
      create_edge(k, j, static_cast<float>(cfg::W_INIT), t);
      presyn_scratch_.insert(k); // avoid a duplicate within this call
    }
  });
}

void Network::flush_pruned() {
  // Re-check w at flush time: a synapse marked weak earlier this tick may have
  // been rescued by a later potentiation/reward update.
  for (int gidx : to_prune_) {
    Synapse &s = pool_[gidx];
    if (s.alive && !s.pinned && s.w < cfg::W_PRUNE) // pinned wires never deleted
      delete_edge(gidx);
  }
  to_prune_.clear();
}

// ---------------------------------------------------------------------------

void Network::fire(int j, tick_t t) {
  Neuron &nj = neurons[j];
  nj.v = cfg::V_RESET;
  nj.refactory_time = t + cfg::TAU_REF;
  nj.last_spike_time = t;
  nj.threshold += cfg::THETA_ADAPT; // fatigue: harder to fire again soon
  nj.threshold_tick = t;
  // leaky spike-rate estimate for motor readout (§7)
  nj.rate_trace = decay(nj.rate_trace, t - nj.rate_tick, cfg::TAU_MOTOR) + 1.0;
  nj.rate_tick = t;
  frontier_next.push_back(j);
  recently_fired.push_back(Spike{j, t});

  // post-side STDP = POTENTIATION (pre fired before this post spike)
  for (int gidx : in_edges_[j]) {
    Synapse &s = pool_[gidx];
    const Neuron &ni = neurons[s.source];
    if (is_recent(ni.last_spike_time, t)) {
      touch_synapse(s, t);
      double dstdp =
          +cfg::A_PLUS * std::exp(-static_cast<double>(t - ni.last_spike_time) /
                                  cfg::TAU_PLUS);
      s.elig_tag += dstdp;                 // tag for reward commit (§5.4)
      s.w = static_cast<float>(cap(s.w + cfg::ALPHA * dstdp)); // unsupervised
      if (s.w < cfg::W_PRUNE)
        to_prune_.push_back(gidx);
    }
  }

  // structural growth: sprout new inputs from nearby co-firing neurons (§11.2)
  try_form(j, t);
}

void Network::step() {
  const tick_t t = current_tick;
  // NB: `touched` is NOT cleared here — sensor input injected via increase_v()
  // during the sense phase (before this call) has already populated it. It is
  // cleared at the end of the tick instead.

  // Ambient background activity: real cortex is never silent. A sparse Poisson
  // kick keeps the net spontaneously firing so STDP has correlations to shape.
  for (std::size_t i = 0; i < neurons.size(); ++i)
    if (uni01_(rng_) < cfg::NOISE_PROB)
      increase_v(static_cast<int>(i), cfg::NOISE_AMP);

  // --- 5.1 Propagate frontier (scatter + pre-side STDP = depression) ---
  for (int i : frontier) {
    const int8_t sign = neurons[i].polarity;
    // inhibitory synapses are ×INHIB_GAIN stronger → E/I balance (no runaway)
    const double eff = sign > 0 ? 1.0 : -cfg::INHIB_GAIN;
    for (int gidx : out_edges_[i]) {
      Synapse &s = pool_[gidx];
      Neuron &nj = neurons[s.target];

      nj.v = decay(nj.v, t - nj.last_tick, cfg::TAU_V) + eff * s.w;
      nj.last_tick = t;
      mark_touched(s.target, t);

      if (is_recent(nj.last_spike_time, t)) { // post fired before this pre
        touch_synapse(s, t);
        double dstdp =
            -cfg::A_MINUS *
            std::exp(-static_cast<double>(t - nj.last_spike_time) /
                     cfg::TAU_MINUS);
        s.elig_tag += dstdp;
        s.w = static_cast<float>(cap(s.w + cfg::ALPHA * dstdp));
        if (s.w < cfg::W_PRUNE)
          to_prune_.push_back(gidx);
      }
    }
  }

  // --- 5.2 Integrate & fire (touched neurons not in refractory) ---
  // Touched = sensor-injected neurons (§5.0) ∪ frontier targets (§5.1). Fold in
  // the tonic bias via the same setter (decay-to-now + add). Safe inside this
  // loop: j is already marked touched, so increase_v won't grow `touched`.
  frontier_next.clear();
  for (int j : touched) {
    Neuron &nj = neurons[j];
    if (t < nj.refactory_time)
      continue;
    increase_v(j, nj.bias);
    nj.threshold = cfg::THETA0 + decay(nj.threshold - cfg::THETA0,
                                       t - nj.threshold_tick, cfg::TAU_THETA);
    nj.threshold_tick = t;
    if (nj.v >= nj.threshold)
      fire(j, t);
  }

  frontier.swap(frontier_next);
  touched.clear();  // ready for next tick's sense + propagate
  flush_pruned();   // apply this tick's deferred deletions (§11.3)

  // --- 5.7 (partial) evict recently-fired older than horizon H ---
  while (!recently_fired.empty() &&
         t - recently_fired.front().tick > cfg::H_RECENT)
    recently_fired.pop_front();

  ++current_tick;
}

// SPEC §5.4 — eager commit of eligibility tags into weights at reward time.
// Iterating the outgoing edges of every recently-fired neuron covers exactly
// the synapses whose *both* endpoints fired recently (i.e. the tagged ones):
// a potentiation tag on i→j is reached via source i, which is itself in RF.
void Network::apply_reward(double reward) {
  const tick_t t = current_tick;
  for (const Spike &sp : recently_fired) {
    for (int gidx : out_edges_[sp.id]) {
      Synapse &s = pool_[gidx];
      if (s.pinned)
        continue; // action pathway is shaped by targeted conditioning, not the
                  // confused global hormone (whose sign flips on these)
      touch_synapse(s, t); // decay e (and w) to now
      s.w = static_cast<float>(cap(s.w + cfg::ETA * reward * s.elig_tag));
      if (s.w < cfg::W_PRUNE)
        to_prune_.push_back(gidx);
    }
  }
}

double Network::total_weight() const {
  double sum = 0.0;
  for (const Synapse &s : pool_)
    if (s.alive)
      sum += s.w;
  return sum;
}

void Network::aversive_depress_inputs(int neuron, double gain,
                                      int except_source) {
  double post = spike_rate(neuron);
  double ap = post / (post + 4.0); // this motor's own activity (post-factor)
  if (ap <= 0.01)
    return; // motor wasn't firing → it didn't cause the harm → no blame
  for (int gidx : in_edges_[neuron]) {
    Synapse &s = pool_[gidx];
    if (s.source == except_source)
      continue; // protect the withdrawal reflex
    double a = spike_rate(s.source);
    a = a / (a + 4.0); // source (pre) activity
    if (a <= 0.01)
      continue;
    double factor = 1.0 - gain * a * ap; // pre×post: motor-specific credit
    if (factor < 0.0)
      factor = 0.0;
    s.w = static_cast<float>(s.w * factor);
  }
}

void Network::appetitive_potentiate_inputs(int neuron, double gain,
                                           int except_source) {
  double post = spike_rate(neuron);
  double ap = post / (post + 4.0); // this motor's own activity (post-factor)
  if (ap <= 0.01)
    return; // motor wasn't firing → it didn't earn the reward
  for (int gidx : in_edges_[neuron]) {
    Synapse &s = pool_[gidx];
    if (s.source == except_source)
      continue;
    double a = spike_rate(s.source);
    a = a / (a + 4.0); // source (pre) activity
    if (a <= 0.01)
      continue;
    s.w = static_cast<float>(cap(s.w * (1.0 + gain * a * ap))); // pre×post
  }
}

void Network::balance_motor_inputs(int mL, int mR, int except_source) {
  double sL = 0.0, sR = 0.0;
  for (int g : in_edges_[mL])
    if (pool_[g].source != except_source)
      sL += pool_[g].w;
  for (int g : in_edges_[mR])
    if (pool_[g].source != except_source)
      sR += pool_[g].w;
  if (sL <= 1e-6 || sR <= 1e-6)
    return;
  double target = 0.5 * (sL + sR);
  double fL = target / sL, fR = target / sR;
  for (int g : in_edges_[mL])
    if (pool_[g].source != except_source)
      pool_[g].w = static_cast<float>(cap(pool_[g].w * fL));
  for (int g : in_edges_[mR])
    if (pool_[g].source != except_source)
      pool_[g].w = static_cast<float>(cap(pool_[g].w * fR));
}

void Network::collect_edges(std::vector<EdgeView> &out) const {
  out.clear();
  out.reserve(pool_.size());
  for (const Synapse &s : pool_)
    if (s.alive)
      out.push_back({s.source, s.target, s.w});
}
