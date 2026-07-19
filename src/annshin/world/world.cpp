#include "annshin/world/world.hpp"
#include "annshin/world/sense.hpp"

#include <cmath>

namespace annshin::world {
namespace cfg = annshin::config;

World::World(double half_extent, unsigned seed)
    : half_(half_extent), spawner_(seed) {
  creature_.pose = {{0.0, 0.0}, 0.0};
  creature_.radius = 0.5;
}

int World::define_stimulus(StimulusDef def) {
  def.kind = static_cast<int>(defs_.size());
  defs_.push_back(def);
  return def.kind;
}

void World::add_spawn_rule(SpawnRule r) {
  spawner_.add_rule(r);
  spawner_.refill(objects_, half_, next_id_); // initial populate
}

StepResult World::step(double thrust, double turn) {
  if (movement_)
    movement_->advance(creature_, thrust, turn, *this);

  StepResult res;
  const Vec3 cp = creature_.pose.pos;
  for (Entity &e : objects_) {
    if (!e.alive)
      continue;
    double dx = e.pos.x - cp.x, dy = e.pos.y - cp.y, dz = e.pos.z - cp.z;
    double rr = e.radius + creature_.radius;
    if (dx * dx + dy * dy + dz * dz <= rr * rr) {
      res.contacts.push_back({e.kind});
      if (defs_[e.kind].consumed_on_contact)
        e.alive = false;
    }
  }
  spawner_.refill(objects_, half_, next_id_); // respawn consumed
  return res;
}

void World::odor_at(Vec3 p, double *out) const {
  for (int k = 0; k < cfg::N_ODOR; ++k)
    out[k] = 0.0;
  for (const Entity &e : objects_) {
    if (!e.alive)
      continue;
    const StimulusDef &d = defs_[e.kind];
    if (d.smell_strength <= 0.0)
      continue;
    double dx = e.pos.x - p.x, dy = e.pos.y - p.y, dz = e.pos.z - p.z;
    double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
    double intensity = d.smell_strength * std::exp(-dist / cfg::SMELL_SCALE);
    for (int k = 0; k < cfg::N_ODOR; ++k)
      out[k] += intensity * d.scent[k];
  }
}

// --- KinematicMovement (v1 physics) ---
void KinematicMovement::advance(Creature &c, double thrust, double turn,
                                const World &w) {
  c.pose.heading += cfg::TURN_GAIN * turn;
  double th = thrust > 0.0 ? thrust : 0.0;
  // move along the ground plane; y (height) is untouched in v1
  c.pose.pos = c.pose.pos + forward_xz(c.pose.heading) * (cfg::MOVE_GAIN * th);
  const double half = w.half_extent();
  auto wrap = [&](double v) {
    if (v > half)
      v -= 2 * half;
    else if (v < -half)
      v += 2 * half;
    return v;
  };
  c.pose.pos.x = wrap(c.pose.pos.x);
  c.pose.pos.z = wrap(c.pose.pos.z);
}

// --- OdorSense: N_ODOR receptors × 2 nostrils channels ---
void OdorSense::sample(const World &w, const Pose &obs,
                       std::vector<double> &out) const {
  const int NR = cfg::N_ODOR;
  out.assign(2 * NR, 0.0);
  Vec3 nose = obs.pos + forward_xz(obs.heading) * nose_fwd;
  Vec3 right = right_xz(obs.heading) * nose_sep;
  std::vector<double> l(NR), r(NR);
  w.odor_at(nose - right, l.data()); // left nostril
  w.odor_at(nose + right, r.data()); // right nostril
  for (int k = 0; k < NR; ++k) {
    out[k] = l[k];
    out[NR + k] = r[k];
  }
}

} // namespace annshin::world
