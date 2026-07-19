#pragma once
#include <vector>

namespace annshin::body {

// A homeostatic drive (SPEC §2.4). Unlike a Sensor it holds *state* that
// persists and drifts over time, and it feeds wellbeing (→ hormone). Its
// deficit is also projected onto interoceptive input neurons so the creature
// can *feel* it (§6) — but that projection is a secondary readout; the drive's
// primary identity is its state + its contribution to wellbeing.
struct Drive {
  double value;    // current level
  double setpoint; // comfortable target
  double weight;   // importance in wellbeing
  double drift;    // passive change per tick (e.g. energy leak)

  // interoceptive projection: which input neurons feel this drive's deficit
  std::vector<int> neurons; // usually 1; more for population coding
  double intero_gain = 1.0; // deficit → current scale

  // bounds keep wellbeing (squared error) finite; clamp after every mutation
  double min_value = 0.0;
  double max_value = 200.0;
  // If false, IMPROVING this drive gives no reward (only worsening punishes) —
  // e.g. pain: getting hurt hurts, but healing shouldn't feel rewarding, else
  // recovery "refunds" the pain and the creature seeks the harm. (§ hormone
  // bug)
  bool reward_on_improve = true;
  double prev_value = 0.0; // set to value on add; used for per-drive ΔW

  void clamp() {
    value =
        value < min_value ? min_value : (value > max_value ? max_value : value);
  }

  double deficit() const { // one-sided shortfall below setpoint (§6)
    double d = setpoint - value;
    return d > 0.0 ? d : 0.0;
  }
  double error() const { return value - setpoint; } // signed, for wellbeing
};

} // namespace annshin::body
