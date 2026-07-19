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
  for (const DriveEffect &e : it->second)
    apply_effect(e.drive_id, e.delta * magnitude);
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

  // 3. wellbeing → hormone → reward (§5.3–5.4)
  double W = compute_wellbeing();
  if (!primed_) { // avoid a spurious spike on the very first tick
    wellbeing_prev_ = W;
    primed_ = true;
  }
  double R = cfg::K_REWARD * (W - wellbeing_prev_);
  wellbeing_prev_ = W;
  last_hormone_ = R;
  if (std::fabs(R) > cfg::R_EPS)
    net.apply_reward(R);
}

} // namespace annshin::body
