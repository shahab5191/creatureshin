#pragma once
#include "annshin/config.hpp"
#include "annshin/world/entity.hpp"
#include "annshin/world/movement.hpp"
#include "annshin/world/spawner.hpp"
#include <memory>
#include <span>
#include <vector>

namespace annshin::world {

// Minimal top-down world (SPEC §8): a differential-drive creature on a bounded
// toroidal plane with random food/fire. No physics engine — kinematics +
// distance checks. Renderer-agnostic & headless. Grows by: swapping the
// MovementModel, adding ObjectKinds (define_stimulus) + sensory primitives.
class World {
public:
  explicit World(double half_extent, unsigned seed = annshin::config::RNG_SEED);

  // --- setup ---
  int define_stimulus(StimulusDef def); // returns kind (== table index)
  void add_spawn_rule(SpawnRule r);      // also populates immediately
  void set_movement(std::unique_ptr<MovementModel> m) { movement_ = std::move(m); }
  Creature &creature() { return creature_; }

  // --- one world-step (SPEC §5.7) ---
  StepResult step(double thrust, double turn);

  // --- read-only state (renderer + sensory models) ---
  double half_extent() const { return half_; }
  const Creature &creature() const { return creature_; }
  std::span<const Entity> objects() const { return objects_; }
  const StimulusDef &stimulus(int kind) const { return defs_[kind]; }

  // --- sensory primitive (OdorSense reads this) ---
  // Intensity-weighted blend of nearby scents at p → N_ODOR receptor values.
  void odor_at(Vec3 p, double *out) const;

private:
  double half_;
  Creature creature_;
  std::vector<Entity> objects_;
  std::vector<StimulusDef> defs_;
  std::unique_ptr<MovementModel> movement_;
  Spawner spawner_;
  int next_id_ = 0;
};

} // namespace annshin::world
