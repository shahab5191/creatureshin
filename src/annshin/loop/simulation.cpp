#include "annshin/loop/simulation.hpp"
#include "annshin/config.hpp"

#include <algorithm>
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
  const int NOCI = R + 4; // burn receptor (nociceptor), fires on fire contact
  noci_neuron_ = NOCI;
  motor_l_ = ML;
  motor_r_ = MR;

  // sensory + motor cells excitatory (afferents/drivers excite)
  for (int i = 0; i <= NOCI; ++i)
    net_.force_excitatory(i);

  std::vector<int> receptor_neurons(R);
  for (int i = 0; i < R; ++i)
    receptor_neurons[i] = i;
  int odor = body_.add_sensor(Sensor{receptor_neurons, /*gain=*/1.5});

  // gentle hunger drift: slow starvation so the negative-R gradient doesn't
  // punish the creature into paralysis before it learns to eat.
  energy_drive_ = body_.add_drive(Drive{100, 100, 1.0, -0.008, {HUNGER}, 0.05});
  // health regenerates (moderate) & caps at setpoint. reward_on_improve=false:
  // damage punishes, healing is neutral — so recovery never "refunds" the burn
  // and fire is unambiguously net-aversive. Heal only sets how fast it's ready
  // to be punished again (repeatable signal).
  health_drive_ = body_.add_drive(
      Drive{100, 100, 2.0, +0.01, {PAIN}, 0.05, 0.0, 100.0, /*reward_on_improve=*/false});

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
  // INFINITE procedural fire: any grid cell may hold a fire (probability-based),
  // so any (x,z) is a potential fire — no fixed count.
  world_.set_procedural_fire(fire_kind_, cfg::FIRE_CELL, cfg::FIRE_PROB,
                             cfg::FIRE_SEED);
  world_.set_procedural_food(food_kind_, cfg::FOOD_CELL, cfg::FOOD_PROB,
                             cfg::FOOD_SEED);
  world_.set_movement(std::make_unique<KinematicMovement>());

  body_.register_stimulus(food_kind_, {{energy_drive_, cfg::FOOD_ENERGY}});
  body_.register_stimulus(fire_kind_, {{health_drive_, cfg::FIRE_HEALTH}});
  // staying below this energy hurts (ongoing hunger pain), so 0 energy is not
  // merely neutral — the creature is driven to eat.
  body_.set_pain_threshold(energy_drive_, cfg::ENERGY_PAIN_BELOW);

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

  // Innate withdrawal reflex: burn receptor → both motors (equal → forward
  // thrust to flee the fire). Pinned; protected from the aversive rule so the
  // pain it responds to doesn't weaken it. Approach-avoidance stays learned.
  net_.add_synapse(NOCI, ML, static_cast<float>(cfg::NOCI_REFLEX_W));
  net_.add_synapse(NOCI, MR, static_cast<float>(cfg::NOCI_REFLEX_W));
  body_.set_protected_source(NOCI);
}

void Simulation::tick() {
  if (net_.tick() % cfg::TICKS_PER_WORLD_STEP == 0) {
    DriveCommand cmd = body_.decode_drive(net_);
    StepResult res = world_.step(cmd.thrust, cmd.turn);
    for (const Contact &c : res.contacts) {
      body_.apply_contact(c.kind);
      if (c.kind == food_kind_)
        ++meals_;
      else if (c.kind == fire_kind_) {
        ++burns_;
        net_.increase_v(noci_neuron_, cfg::NOCI_AMP); // burn receptor fires
      }
    }
    for (Binding &b : bindings_) {
      scratch_.assign(b.model->channels(), 0.0);
      b.model->sample(world_, world_.creature().pose, scratch_);
      body_.set_sensor_readings(b.sensor_id, scratch_);
    }

    // Smell-gradient "serotonin" learning: reinforce the current motor drive
    // when food smell is RISING (approaching food), punish it when fire smell is
    // rising (approaching fire). Dense signal → real taxis, not just contact.
    auto cp = world_.creature().pose.pos;
    double fs = world_.food_smell_at(cp), frs = world_.fire_smell_at(cp);
    double dfood = fs - prev_food_smell_, dfire = frs - prev_fire_smell_;
    prev_food_smell_ = fs;
    prev_fire_smell_ = frs;
    if (dfood > 0.0) {
      double g = cfg::APPETITIVE_GAIN *
                 std::min(1.0, dfood * cfg::SMELL_GRAD_GAIN);
      net_.appetitive_potentiate_inputs(motor_l_, g, noci_neuron_);
      net_.appetitive_potentiate_inputs(motor_r_, g, noci_neuron_);
    }
    if (dfire > 0.0) {
      double g = cfg::AVERSIVE_GAIN * std::min(1.0, dfire * cfg::SMELL_GRAD_GAIN);
      net_.aversive_depress_inputs(motor_l_, g, noci_neuron_);
      net_.aversive_depress_inputs(motor_r_, g, noci_neuron_);
    }
    // homeostasis: keep the two motors' total drive balanced → no permanent turn
    // bias (kills the endless-circling failure mode) while steering is preserved.
    net_.balance_motor_inputs(motor_l_, motor_r_, noci_neuron_);
  }
  body_.on_tick(net_);
  net_.step();
}

} // namespace annshin::loop
