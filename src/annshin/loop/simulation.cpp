#include "annshin/loop/simulation.hpp"
#include "annshin/config.hpp"

namespace annshin::loop {
namespace cfg = annshin::config;
using namespace annshin::body;
using namespace annshin::world;

namespace {
constexpr int N = 200; // neuron count
} // namespace

Simulation::Simulation() : net_(N, cfg::FANOUT), world_(cfg::WORLD_HALF) {
  // --- senses (v1: smell only), drives, motors ---
  int food_smell = body_.add_sensor(Sensor{{0, 1}, /*gain=*/1.5}); // L/R nostrils
  energy_drive_ = body_.add_drive(
      Drive{100, 100, 1.0, -0.02, /*hunger neuron*/ {2}, 0.05});
  health_drive_ = body_.add_drive(
      Drive{100, 100, 2.0, 0.0, /*pain neuron*/ {5}, 0.05});

  // Motors — v1 PLACEHOLDER: read the smell neurons so the creature visibly
  // reacts to scent before any motor pathway is learned. Real motor neurons are
  // downstream cells the network wires up during the (future) tuning pass.
  body_.add_motor(Motor{0, 1.0}); // left
  body_.add_motor(Motor{1, 1.0}); // right

  // --- world objects (§8): food smells on channel 0; fire is unseen in v1 ---
  food_kind_ = world_.define_stimulus(
      StimulusDef{0, cfg::FOOD_RADIUS, /*consumed=*/true, /*smell_ch=*/0, 1.0});
  fire_kind_ = world_.define_stimulus(
      StimulusDef{0, cfg::FIRE_RADIUS, /*consumed=*/false, /*smell_ch=*/-1, 0.0});
  world_.add_spawn_rule(SpawnRule{food_kind_, cfg::FOOD_COUNT, cfg::FOOD_RADIUS});
  world_.add_spawn_rule(SpawnRule{fire_kind_, cfg::FIRE_COUNT, cfg::FIRE_RADIUS});
  world_.set_movement(std::make_unique<KinematicMovement>());

  // --- stimulus → drive effects (§8: one entry per stimulus) ---
  body_.register_stimulus(food_kind_, {{energy_drive_, cfg::FOOD_ENERGY}});
  body_.register_stimulus(fire_kind_, {{health_drive_, cfg::FIRE_HEALTH}});

  // --- sensory binding: food smell model → food smell sensor ---
  bindings_.push_back({std::make_unique<SmellSense>(/*channel=*/0), food_smell});
}

void Simulation::tick() {
  if (net_.tick() % cfg::TICKS_PER_WORLD_STEP == 0) {
    // brain → motor → world
    DriveCommand cmd = body_.decode_drive(net_);
    StepResult res = world_.step(cmd.thrust, cmd.turn);
    // world contacts → drive effects (§8)
    for (const Contact &c : res.contacts) {
      body_.apply_contact(c.kind);
      if (c.kind == food_kind_)
        ++meals_;
      else if (c.kind == fire_kind_)
        ++burns_;
    }
    // world → sensors (refresh readings; Body feeds them each tick)
    for (Binding &b : bindings_) {
      scratch_.assign(b.model->channels(), 0.0);
      b.model->sample(world_, world_.creature().pose, scratch_);
      body_.set_sensor_readings(b.sensor_id, scratch_);
    }
  }
  body_.on_tick(net_); // drift + interoception + wellbeing→hormone→reward
  net_.step();         // §5.1–5.2 + structural plasticity
}

} // namespace annshin::loop
