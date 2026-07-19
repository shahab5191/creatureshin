#pragma once
#include "annshin/world/entity.hpp"

namespace annshin::world {

class World; // fwd (advance queries bounds/obstacles)

// The seam for "kinematic now, physics later." A physics backend is just another
// MovementModel subclass swapped in via World::set_movement.
struct MovementModel {
  virtual ~MovementModel() = default;
  virtual void advance(Creature &c, double thrust, double turn,
                       const World &w) = 0;
};

// v1: frictionless kinematic point on a toroidal plane. No obstacle resolution.
struct KinematicMovement : MovementModel {
  void advance(Creature &c, double thrust, double turn, const World &w) override;
};

} // namespace annshin::world
