#pragma once
#include "annshin/body/body.hpp"
#include "annshin/brain/network.hpp"
#include "annshin/world/sense.hpp"
#include "annshin/world/world.hpp"
#include <memory>
#include <vector>

namespace annshin::loop {

// The one translation unit that wires brain + body + world (SPEC §9 loop/).
// Headless and renderer-free: a driver (main / a render thread) calls tick()
// and reads the accessors to visualize. Cross-boundary calls use only neutral
// types (thrust/turn doubles, int kind), so no module leaks into another.
class Simulation {
public:
  Simulation();

  void tick(); // one brain tick (world-steps internally every N ticks)

  // read-only accessors for a renderer / driver
  const ANNNetwork::Network &net() const { return net_; }
  const annshin::body::Body &body() const { return body_; }
  const annshin::world::World &world() const { return world_; }
  int meals() const { return meals_; }
  int burns() const { return burns_; }

private:
  struct Binding {
    std::unique_ptr<annshin::world::SensoryModel> model;
    int sensor_id;
  };

  ANNNetwork::Network net_;
  annshin::body::Body body_;
  annshin::world::World world_;
  std::vector<Binding> bindings_;
  std::vector<double> scratch_;

  int energy_drive_ = 0, health_drive_ = 0;
  int food_kind_ = 0, fire_kind_ = 0;
  int meals_ = 0, burns_ = 0;
};

} // namespace annshin::loop
