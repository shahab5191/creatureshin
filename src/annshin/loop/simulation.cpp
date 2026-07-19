#include "annshin/loop/simulation.hpp"
#include "annshin/config.hpp"

#include <random>

namespace annshin::loop {
namespace cfg = annshin::config;
using namespace annshin::body;
using namespace annshin::world;

namespace {
constexpr int N = 200; // neuron count
} // namespace

Simulation::Simulation() : net_(N, cfg::FANOUT), world_(cfg::WORLD_HALF) {
  // Neuron map: [0 .. 2·N_ODOR-1] = odor receptors (left nostril first, then
  // right), then hunger, pain, motor-L, motor-R.
  const int NR = cfg::N_ODOR;
  const int R = 2 * NR;   // receptor neurons 0 .. R-1
  const int HUNGER = R;   // interoceptive
  const int PAIN = R + 1;
  const int ML = R + 2, MR = R + 3;

  // sensory + motor cells excitatory (afferents/drivers excite)
  for (int i = 0; i <= MR; ++i)
    net_.force_excitatory(i);

  std::vector<int> receptor_neurons(R);
  for (int i = 0; i < R; ++i)
    receptor_neurons[i] = i;
  int odor = body_.add_sensor(Sensor{receptor_neurons, /*gain=*/1.5});

  // gentle hunger drift: slow starvation so the negative-R gradient doesn't
  // punish the creature into paralysis before it learns to eat.
  energy_drive_ = body_.add_drive(Drive{100, 100, 1.0, -0.008, {HUNGER}, 0.05});
  // health regenerates & caps at setpoint → each burn is a fresh punishment.
  health_drive_ =
      body_.add_drive(Drive{100, 100, 2.0, +0.02, {PAIN}, 0.05, 0.0, 100.0});

  // Dedicated motor neurons — no innate behavior, driven only by the learned
  // receptor→motor weights + ambient noise (exploration).
  body_.add_motor(Motor{ML, 1.0});
  body_.add_motor(Motor{MR, 1.0});

  // --- world objects (§8) with distinct sparse scents ---
  StimulusDef food;
  food.radius = cfg::FOOD_RADIUS;
  food.consumed_on_contact = true;
  food.smell_strength = 1.0;
  set_scent(food, "food");
  food_kind_ = world_.define_stimulus(food);

  StimulusDef fire;
  fire.radius = cfg::FIRE_RADIUS;
  fire.consumed_on_contact = false;
  fire.smell_strength = 1.0;
  set_scent(fire, "fire");
  fire_kind_ = world_.define_stimulus(fire);
  fire_radius_ = cfg::FIRE_RADIUS;
  fire_count_ = cfg::FIRE_COUNT;

  // CURRICULUM: spawn food now; fire is withheld until after the warmup so the
  // creature first learns to approach food, then learns fire on top.
  world_.add_spawn_rule(SpawnRule{food_kind_, cfg::FOOD_COUNT, cfg::FOOD_RADIUS});
  world_.set_movement(std::make_unique<KinematicMovement>());

  body_.register_stimulus(food_kind_, {{energy_drive_, cfg::FOOD_ENERGY}});
  body_.register_stimulus(fire_kind_, {{health_drive_, cfg::FIRE_HEALTH}});

  bindings_.push_back({std::make_unique<OdorSense>(), odor});

  // Learnable, pinned, policy-free pathway: every receptor → both motors.
  // Init strong enough that the few (K_SCENT) active receptors can drive a motor
  // to threshold, so the creature moves and can bootstrap eating.
  // SYMMETRIC init (same weight to L and R) → no innate turn bias, so the
  // creature goes roughly straight and *explores* instead of spinning; learning
  // breaks the symmetry to steer. (Not a hardwired policy — straight = no choice.)
  std::mt19937 rng(777);
  std::uniform_real_distribution<double> u(0.30, 0.55);
  for (int r = 0; r < R; ++r) {
    float w = static_cast<float>(u(rng));
    net_.add_synapse(r, ML, w);
    net_.add_synapse(r, MR, w);
  }
}

void Simulation::tick() {
  // curriculum: introduce fire once the food-only warmup is over
  if (!fire_added_ && net_.tick() >= cfg::CURRICULUM_WARMUP) {
    world_.add_spawn_rule(SpawnRule{fire_kind_, fire_count_, fire_radius_});
    fire_added_ = true;
  }

  if (net_.tick() % cfg::TICKS_PER_WORLD_STEP == 0) {
    DriveCommand cmd = body_.decode_drive(net_);
    StepResult res = world_.step(cmd.thrust, cmd.turn);
    for (const Contact &c : res.contacts) {
      body_.apply_contact(c.kind);
      if (c.kind == food_kind_)
        ++meals_;
      else if (c.kind == fire_kind_)
        ++burns_;
    }
    for (Binding &b : bindings_) {
      scratch_.assign(b.model->channels(), 0.0);
      b.model->sample(world_, world_.creature().pose, scratch_);
      body_.set_sensor_readings(b.sensor_id, scratch_);
    }
  }
  body_.on_tick(net_);
  net_.step();
}

} // namespace annshin::loop
