#include "annshin/body/body.hpp"
#include "annshin/config.hpp"
#include <cmath>

namespace annshin::body {
namespace cfg = annshin::config;

int Body::add_sensor(Sensor s) {
  sensors_.push_back(std::move(s));
  return static_cast<int>(sensors_.size()) - 1;
}

int Body::add_drive(Drive d) {
  d.prev_value = d.value; // so the first tick's ΔW ≈ 0
  drives_.push_back(std::move(d));
  return static_cast<int>(drives_.size()) - 1;
}

int Body::add_motor(Motor m) {
  motors_.push_back(m);
  return static_cast<int>(motors_.size()) - 1;
}

double Body::motor_force(int motor_id, const ANNNetwork::Network &net) const {
  return motors_[motor_id].force(net);
}

DriveCommand Body::decode_drive(const ANNNetwork::Network &net) const {
  double left = motors_.size() > 0 ? motors_[0].force(net) : 0.0;
  double right = motors_.size() > 1 ? motors_[1].force(net) : 0.0;
  return differential(left, right);
}

void Body::set_sensor_readings(int sensor_id, std::span<const double> readings) {
  sensors_[sensor_id].set_readings(readings);
}

void Body::apply_effect(int drive_id, double delta) {
  drives_[drive_id].value += delta; // §8 stimulus, one-shot
  drives_[drive_id].clamp();
}

void Body::register_stimulus(int kind, std::vector<DriveEffect> effects) {
  stimulus_map_[kind] = std::move(effects);
}

void Body::apply_contact(int kind, double magnitude) {
  auto it = stimulus_map_.find(kind);
  if (it == stimulus_map_.end())
    return;
  for (const DriveEffect &e : it->second) {
    apply_effect(e.drive_id, e.delta * magnitude);
    if (e.delta < 0.0)
      pain_event_ = true; // harmful contact — fires aversive even at 0 health
  }
}

double Body::compute_wellbeing() const {
  // W = -Σ_c weight·(value - setpoint)²   (SPEC §5.3)
  double W = 0.0;
  for (const Drive &d : drives_) {
    double e = d.error();
    W -= d.weight * e * e;
  }
  return W;
}

void Body::on_tick(ANNNetwork::Network &net) {
  // 1. drift drives, and project each drive's deficit onto its neurons (§5.3/§6)
  for (Drive &d : drives_) {
    d.value += d.drift;
    d.clamp();
    double current = d.intero_gain * d.deficit();
    if (current > 0.0)
      for (int n : d.neurons)
        net.increase_v(n, current);
  }

  // 2. feed exteroceptive sensors — sustained rate coding (§5.0/§6)
  for (const Sensor &s : sensors_)
    s.feed(net);

  // 3. hormone. Reversible drives (energy) contribute their ΔW directly.
  //    One-sided drives (pain) only feed a LINGERING pain trace when they
  //    worsen — so a burn keeps punishing for a while (decays), and healing
  //    never rewards.
  double dW = 0.0;      // reversible reward (energy/hunger)
  double damage = 0.0;  // pain magnitude this tick (from one-sided drives)
  for (Drive &d : drives_) {
    double err = d.error();
    double perr = d.prev_value - d.setpoint;
    double dWc = -d.weight * (err * err - perr * perr);
    if (!d.reward_on_improve) {
      if (dWc < 0.0)
        damage += -dWc; // worsening → pain (magnitude); improving → nothing
    } else {
      dW += dWc;
    }
    d.prev_value = d.value;
  }
  pain_trace_ = pain_trace_ * cfg::PAIN_LINGER + damage; // linger + this tick

  // Aversive conditioning: on a harmful CONTACT (not the health-derivative, so
  // it keeps firing even when health is floored at 0 and can't drop further),
  // depress each motor's active INPUTS — weaken whatever drove the motors into
  // the harm. Targeted, unlike the global hormone.
  if (pain_event_) {
    for (const Motor &m : motors_)
      net.aversive_depress_inputs(m.neuron, cfg::AVERSIVE_GAIN, protected_source_);
    pain_event_ = false;
  }

  double R = cfg::K_REWARD * dW - cfg::PAIN_GAIN * pain_trace_;
  R = R > cfg::R_MAX_POS ? cfg::R_MAX_POS
                         : (R < -cfg::R_MAX_NEG ? -cfg::R_MAX_NEG : R);
  wellbeing_prev_ = compute_wellbeing(); // for the HUD/observation only
  last_hormone_ = R;
  if (std::fabs(R) > cfg::R_EPS)
    net.apply_reward(R);
}

} // namespace annshin::body
