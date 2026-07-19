#pragma once
#include "annshin/config.hpp"
#include "annshin/world/entity.hpp"
#include "annshin/world/movement.hpp"
#include "annshin/world/spawner.hpp"
#include <cmath>
#include <cstdint>
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
  // Infinite procedural fire field of stimulus `kind`: any grid cell may hold a
  // fire (deterministic hash), so any (x,z) is a potential fire.
  void set_procedural_fire(int kind, double cell, double prob, unsigned seed) {
    fire_kind_ = kind;
    fire_cell_ = cell;
    fire_prob_ = prob;
    fire_seed_ = seed;
  }
  // Same for food: any grid cell may hold food (probability). Not consumed
  // (like fire) — contact yields energy; energy caps, so no infinite camping.
  void set_procedural_food(int kind, double cell, double prob, unsigned seed) {
    pfood_kind_ = kind;
    food_cell_ = cell;
    food_prob_ = prob;
    food_seed_ = seed;
  }
  Creature &creature() { return creature_; }

  // --- one world-step (SPEC §5.7) ---
  StepResult step(double thrust, double turn);

  // --- read-only state (renderer + sensory models) ---
  double half_extent() const { return half_; }
  const Creature &creature() const { return creature_; }
  std::span<const Entity> objects() const { return objects_; }
  const StimulusDef &stimulus(int kind) const { return defs_[kind]; }

  int fire_kind() const { return fire_kind_; }
  int food_kind() const { return pfood_kind_; }

  // Total smell intensity (magnitude, not pattern) at p — for the smell-gradient
  // "serotonin" signal (approaching food/fire drives learning before contact).
  double fire_smell_at(Vec3 p) const {
    if (fire_kind_ < 0 || defs_[fire_kind_].smell_strength <= 0.0)
      return 0.0;
    double str = defs_[fire_kind_].smell_strength, s = 0.0;
    for_fires_near(p, annshin::config::SMELL_SCALE * 4.0, [&](Vec3 fp) {
      double dx = fp.x - p.x, dz = fp.z - p.z;
      s += str * std::exp(-std::sqrt(dx * dx + dz * dz) / annshin::config::SMELL_SCALE);
    });
    return s;
  }
  double food_smell_at(Vec3 p) const {
    if (pfood_kind_ < 0 || defs_[pfood_kind_].smell_strength <= 0.0)
      return 0.0;
    double str = defs_[pfood_kind_].smell_strength, s = 0.0;
    for_food_near(p, annshin::config::SMELL_SCALE * 4.0, [&](Vec3 fp) {
      double dx = fp.x - p.x, dz = fp.z - p.z;
      s += str * std::exp(-std::sqrt(dx * dx + dz * dz) / annshin::config::SMELL_SCALE);
    });
    return s;
  }

  // --- sensory primitive (OdorSense reads this) ---
  // Intensity-weighted blend of nearby scents at p → N_ODOR receptor values.
  void odor_at(Vec3 p, double *out) const;

  // Visit every procedural fire within `radius` of p (used for contact, smell,
  // rendering). Iterates only the grid cells overlapping the query disc.
  template <class F> void for_fires_near(Vec3 p, double radius, F &&f) const {
    if (fire_kind_ < 0)
      return;
    long c0x = (long)std::floor((p.x - radius) / fire_cell_);
    long c1x = (long)std::floor((p.x + radius) / fire_cell_);
    long c0z = (long)std::floor((p.z - radius) / fire_cell_);
    long c1z = (long)std::floor((p.z + radius) / fire_cell_);
    double r2 = radius * radius;
    for (long cx = c0x; cx <= c1x; ++cx)
      for (long cz = c0z; cz <= c1z; ++cz) {
        Vec3 fp;
        if (cell_has(cx, cz, fire_seed_, fire_prob_, fire_cell_, fp)) {
          double dx = fp.x - p.x, dz = fp.z - p.z;
          if (dx * dx + dz * dz <= r2)
            f(fp);
        }
      }
  }

  // Same for procedural food.
  template <class F> void for_food_near(Vec3 p, double radius, F &&f) const {
    if (pfood_kind_ < 0)
      return;
    long c0x = (long)std::floor((p.x - radius) / food_cell_);
    long c1x = (long)std::floor((p.x + radius) / food_cell_);
    long c0z = (long)std::floor((p.z - radius) / food_cell_);
    long c1z = (long)std::floor((p.z + radius) / food_cell_);
    double r2 = radius * radius;
    for (long cx = c0x; cx <= c1x; ++cx)
      for (long cz = c0z; cz <= c1z; ++cz) {
        Vec3 fp;
        if (cell_has(cx, cz, food_seed_, food_prob_, food_cell_, fp)) {
          double dx = fp.x - p.x, dz = fp.z - p.z;
          if (dx * dx + dz * dz <= r2)
            f(fp);
        }
      }
  }

private:
  // Deterministic hash: does cell (cx,cz) contain a stimulus of this field, and
  // where (jittered within the cell)?
  static bool cell_has(long cx, long cz, unsigned seed, double prob, double cell,
                       Vec3 &out) {
    uint32_t h = seed;
    h ^= (uint32_t)(cx * 73856093);
    h ^= (uint32_t)(cz * 19349663);
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    if ((h & 0xFFFFu) / 65535.0 >= prob)
      return false;
    double jx = ((h >> 16) & 0xFFu) / 255.0; // jitter within the cell
    double jz = ((h >> 24) & 0xFFu) / 255.0;
    out = {cx * cell + jx * cell, 0.0, cz * cell + jz * cell};
    return true;
  }

  double half_;
  Creature creature_;
  std::vector<Entity> objects_;
  std::vector<StimulusDef> defs_;
  std::unique_ptr<MovementModel> movement_;
  Spawner spawner_;
  int next_id_ = 0;

  int fire_kind_ = -1; // procedural fire stimulus kind (-1 = none)
  double fire_cell_ = 12.0, fire_prob_ = 0.3;
  unsigned fire_seed_ = 12345;

  int pfood_kind_ = -1; // procedural food stimulus kind (-1 = none)
  double food_cell_ = 12.0, food_prob_ = 0.3;
  unsigned food_seed_ = 54321;
};

} // namespace annshin::world
