#pragma once
#include "annshin/brain/spatial_grid.hpp"
#include "annshin/config.hpp"
#include "annshin/math.hpp"
#include <cstddef>
#include <cstdint>
#include <deque>
#include <random>
#include <unordered_set>
#include <vector>

namespace ANNNetwork {

using tick_t = annshin::config::tick_t;

struct Vec3 {
  double x, y, z;
};

struct Neuron {
  double v = 0.0;            // membrane potential (decays to 0)
  tick_t last_tick = 0;      // potential timestamp (lazy decay anchor)
  tick_t last_spike_time;    // last tick this neuron fired (drives STDP)
  double threshold;          // fire level; rises on firing, decays back
  tick_t threshold_tick = 0; // last tick threshold was updated (θ decay anchor)
  tick_t refactory_time = 0; // cannot fire before this tick
  double bias = 0.0;         // tonic drive added each active tick
  int8_t polarity;           // +1 excitatory / -1 inhibitory (sign of outgoing)
  Vec3 pos;                  // position in 3D space (gates growth, §11)
  double rate_trace = 0.0;   // leaky spike-rate estimate (motor readout, §7)
  tick_t rate_tick = 0;      // last tick rate_trace was updated (lazy anchor)
};

// Stored in a stable pool `pool_`; per-neuron out_edges_/in_edges_ hold indices
// into it. The pool never shifts (freed slots are recycled), so those indices
// stay valid across add/delete — that's what makes the reverse index (in_edges_)
// survive structural plasticity (§11).
struct Synapse {
  int source;            // presynaptic neuron i
  int target;            // postsynaptic neuron j
  float w;               // weight magnitude (effective signal = σ_i · w)
  double elig_tag = 0.0; // eligibility trace; decays with τ_e (§5.4)
  tick_t touch_time = 0; // last tick e/w were updated (lazy anchor, §5.5)
  bool alive = true;     // false once pruned (slot on the free list)
  bool pinned = false;   // innate pathway: never pruned (learning tunes weight)
};

// Recently-fired entry: (neuron id, tick it fired). Working set for reward.
struct Spike {
  int id;
  tick_t tick;
};

class Network {
public:
  Network(int num_neurons, int fanout);

  // Sensor input (§5.0): add `amount` to a neuron's potential for the current
  // tick. Applies lazy decay and marks the neuron touched — v stays private so
  // the decay discipline lives here, not in the body/sensor code. Inline: it's
  // on the hot per-tick sense path.
  void increase_v(int neuron, double amount) {
    Neuron &nj = neurons[neuron];
    nj.v = annshin::math::decay(nj.v, current_tick - nj.last_tick,
                                annshin::config::TAU_V) +
           amount;
    nj.last_tick = current_tick;
    mark_touched(neuron, current_tick);
  }

  void step();                      // one brain tick (§5.1–5.2)
  void apply_reward(double reward);  // §5.4: commit tags → weights (R from Body)

  // Aversive conditioning: on pain, depress the INCOMING synapses of a motor
  // neuron in proportion to each source's current activity — i.e. weaken
  // whatever was actively driving this motor into the harm (the action pathway),
  // not the sensor itself. `except_source` is left untouched (protects the
  // withdrawal reflex from being weakened by the very pain it responds to).
  void aversive_depress_inputs(int neuron, double gain, int except_source = -1);

  // Wire an innate (but plastic) pathway, e.g. sensor→motor. It carries NO
  // built-in policy (small weights); reward-STDP learns the mapping. Pinned so
  // punishment can reshape its weights but never delete the wire.
  void add_synapse(int i, int j, float w) {
    int g = create_edge(i, j, w, current_tick);
    pool_[g].pinned = true;
  }
  // Force a neuron excitatory (sensory afferents / motor drivers must excite).
  void force_excitatory(int i) { neurons[i].polarity = 1; }

  // --- observation helpers ---
  std::size_t frontier_size() const { return frontier.size(); }
  std::size_t edge_count() const { return pool_.size() - free_list_.size(); }
  double total_weight() const;
  tick_t tick() const { return current_tick; }

  // §7 motor readout: leaky-integrator estimate of a neuron's recent firing
  // rate (∝ spikes over ~TAU_MOTOR ms). Read-only: decays the trace to now
  // without mutating it. Exact scale folds into the motor's gain.
  double spike_rate(int neuron) const {
    const Neuron &n = neurons[neuron];
    return annshin::math::decay(n.rate_trace, current_tick - n.rate_tick,
                                annshin::config::TAU_MOTOR);
  }

  // --- brain-viz read accessors (positions are static; only activation moves) ---
  std::size_t neuron_count() const { return neurons.size(); }
  Vec3 neuron_pos(int i) const { return neurons[i].pos; }
  int8_t neuron_polarity(int i) const { return neurons[i].polarity; }
  tick_t last_spike_time(int i) const { return neurons[i].last_spike_time; }

  struct EdgeView {
    int src, tgt;
    float w;
  };
  void collect_edges(std::vector<EdgeView> &out) const; // alive synapses (graph)

private:
  void fire(int j, tick_t t);
  void touch_synapse(Synapse &s, tick_t t); // §5.5: lazy trace + forgetting
  void mark_touched(int j, tick_t t);
  bool is_recent(tick_t spike_time, tick_t t) const;

  // --- structural plasticity (§11) ---
  int alloc_synapse(int i, int j, float w, tick_t t); // pool slot (reuse/append)
  int create_edge(int i, int j, float w, tick_t t);   // + register out/in edges
  void delete_edge(int gidx);                         // unregister + free slot
  bool edge_exists(int i, int j) const;               // scan out_edges_[i]
  void try_form(int j, tick_t t);   // §11.2 grow inputs to a just-fired neuron
  void flush_pruned();              // apply deferred deletions (end of tick)

private:
  std::vector<Neuron> neurons;

  // synapse storage: stable pool + recycled free slots (§11.5)
  std::vector<Synapse> pool_;            // indexed by stable synapse id (gidx)
  std::vector<int> free_list_;           // freed pool slots, reused by alloc
  std::vector<std::vector<int>> out_edges_; // source i → gidx list (forward)
  std::vector<std::vector<int>> in_edges_;  // target j → gidx list (reverse)
  std::vector<int> to_prune_;               // gidx marked weak this tick

  std::vector<int> frontier;        // fired last tick → propagate this tick
  std::vector<int> frontier_next;   // being filled this tick
  std::deque<Spike> recently_fired; // spikes within horizon H (§5.4, §5.7)

  std::vector<int> touched;         // neurons touched this tick
  std::vector<tick_t> touched_mark; // touched_mark[j] == t iff j touched at t

  annshin::SpatialGrid grid_{annshin::config::R_REACH}; // neighbor index (§11.1)
  std::mt19937 rng_{annshin::config::RNG_SEED}; // formation rolls + init
  std::uniform_real_distribution<double> uni01_{0.0, 1.0};
  std::unordered_set<int> presyn_scratch_; // reused set: j's current inputs

  tick_t current_tick = 0;
};

} // namespace ANNNetwork
