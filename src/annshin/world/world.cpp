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
  spawner_.refill(objects_, creature_.pose.pos, half_, next_id_);
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
  // procedural (infinite) fire contacts
  if (fire_kind_ >= 0) {
    double fr = defs_[fire_kind_].radius + creature_.radius;
    for_fires_near(cp, fr, [&](Vec3) { res.contacts.push_back({fire_kind_}); });
  }
  // procedural food contacts (not consumed; contact yields energy, which caps —
  // so no infinite camping, and food is everywhere so foraging can bootstrap).
  if (pfood_kind_ >= 0) {
    double fr = defs_[pfood_kind_].radius + creature_.radius;
    for_food_near(cp, fr, [&](Vec3) { res.contacts.push_back({pfood_kind_}); });
  }
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
  // procedural fire scent (infinite field)
  if (fire_kind_ >= 0 && defs_[fire_kind_].smell_strength > 0.0) {
    const StimulusDef &d = defs_[fire_kind_];
    for_fires_near(p, cfg::SMELL_SCALE * 4.0, [&](Vec3 fp) {
      double dx = fp.x - p.x, dz = fp.z - p.z;
      double dist = std::sqrt(dx * dx + dz * dz);
      double intensity = d.smell_strength * std::exp(-dist / cfg::SMELL_SCALE);
      for (int k = 0; k < cfg::N_ODOR; ++k)
        out[k] += intensity * d.scent[k];
    });
  }
  // procedural food scent (infinite field)
  if (pfood_kind_ >= 0 && defs_[pfood_kind_].smell_strength > 0.0) {
    const StimulusDef &d = defs_[pfood_kind_];
    for_food_near(p, cfg::SMELL_SCALE * 4.0, [&](Vec3 fp) {
      double dx = fp.x - p.x, dz = fp.z - p.z;
      double dist = std::sqrt(dx * dx + dz * dz);
      double intensity = d.smell_strength * std::exp(-dist / cfg::SMELL_SCALE);
      for (int k = 0; k < cfg::N_ODOR; ++k)
        out[k] += intensity * d.scent[k];
    });
  }
}

// --- KinematicMovement (v1 physics) ---
void KinematicMovement::advance(Creature &c, double thrust, double turn,
                                const World &w) {
  c.pose.heading += cfg::TURN_GAIN * turn;
  double th = thrust > 0.0 ? thrust : 0.0;
  // infinite world: move along the ground plane, no bounds
  c.pose.pos = c.pose.pos + forward_xz(c.pose.heading) * (cfg::MOVE_GAIN * th);
  (void)w;
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
