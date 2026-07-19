#pragma once
#include "annshin/body/drive.hpp"
#include "annshin/body/motor.hpp"
#include "annshin/body/sensor.hpp"
#include "annshin/body/stimulus.hpp"
#include "annshin/brain/network.hpp"
#include <span>
#include <unordered_map>
#include <vector>

namespace annshin::body {

// The creature's body (SPEC §9 body/). Owns the sensors and drives, and is the
// single interface the game world talks to. Shares the brain's clock: on_tick()
// runs once per brain tick, before Network::step().
//
// Responsibilities per tick:
//   1. drift drives                                     (§5.3 state update)
//   2. feed interoceptive + exteroceptive sensors       (§5.0 sense)
//   3. compute wellbeing W, derive hormone R, reward     (§5.3–5.4)
class Body {
public:
  // --- setup (returns index of the added sensor/drive/motor) ---
  int add_sensor(Sensor s);
  int add_drive(Drive d);
  int add_motor(Motor m);

  // --- world → body (called per world-step) ---
  void set_sensor_readings(int sensor_id, std::span<const double> readings);
  void apply_effect(int drive_id, double delta); // one-shot stimulus (§8)

  // Declarative stimulus table (§8): "adding a stimulus = one table entry".
  void register_stimulus(int kind, std::vector<DriveEffect> effects);
  void apply_contact(int kind, double magnitude = 1.0); // kind → drive effects

  // A source neuron whose synapses the aversive rule must NOT weaken (the
  // innate withdrawal-reflex pathway).
  void set_protected_source(int neuron) { protected_source_ = neuron; }

  // --- per brain tick, before net.step() ---
  void on_tick(ANNNetwork::Network &net);

  // --- brain → world (read on the world-step, §5.6/§7) ---
  double motor_force(int motor_id, const ANNNetwork::Network &net) const;
  // Differential drive from the first two motors (motor 0 = left, 1 = right).
  DriveCommand decode_drive(const ANNNetwork::Network &net) const;

  // --- observation ---
  double wellbeing() const { return wellbeing_prev_; }
  double last_hormone() const { return last_hormone_; }
  const Drive &drive(int id) const { return drives_[id]; }
  std::size_t drive_count() const { return drives_.size(); }

private:
  double compute_wellbeing() const; // §5.3

  std::vector<Sensor> sensors_;
  std::vector<Drive> drives_;
  std::vector<Motor> motors_;
  std::unordered_map<int, std::vector<DriveEffect>> stimulus_map_;
  bool pain_event_ = false;  // a harmful contact happened (independent of health clamp)
  int protected_source_ = -1; // reflex source the aversive rule won't weaken
  double pain_trace_ = 0.0; // lingering pain (decays); from one-sided drives
  double wellbeing_prev_ = 0.0;
  double last_hormone_ = 0.0;
  bool primed_ = false; // seed wellbeing_prev_ before first hormone
};

} // namespace annshin::body
