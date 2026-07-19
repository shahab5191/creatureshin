#pragma once
#include "annshin/world/entity.hpp"
#include <random>
#include <vector>

namespace annshin::world {

struct SpawnRule {
  int kind;
  int target_count;
  double radius;
};

// Keeps each kind topped up to its target count at random in-bounds positions.
class Spawner {
public:
  explicit Spawner(unsigned seed) : rng_(seed) {}
  void add_rule(SpawnRule r) { rules_.push_back(r); }

  void refill(std::vector<Entity> &entities, double half, int &next_id) {
    std::uniform_real_distribution<double> u(-half, half);
    for (const SpawnRule &rule : rules_) {
      int count = 0;
      for (const Entity &e : entities)
        if (e.alive && e.kind == rule.kind)
          ++count;
      while (count < rule.target_count) {
        Entity e;
        e.id = next_id++;
        e.kind = rule.kind;
        e.radius = rule.radius;
        e.pos = {u(rng_), 0.0, u(rng_)}; // on the ground plane (y = 0)
        e.alive = true;
        entities.push_back(e);
        ++count;
      }
    }
  }

private:
  std::vector<SpawnRule> rules_;
  std::mt19937 rng_;
};

} // namespace annshin::world
