#pragma once
#include "annshin/brain/network.hpp"
#include <span>
#include <vector>

namespace annshin::body {

// A rate-coded transducer (SPEC §6). Stateless w.r.t. the brain: it just maps
// its current readings → injected potential on a fixed set of input neurons.
// Exteroceptive sensors (sight, smell, touch) get readings from the game world;
// the values it injects are computed here, not by the brain.
struct Sensor {
  std::vector<int> neurons;     // input neurons this sensor drives
  std::vector<double> readings; // latest reading per neuron (set by the world)
  double gain = 1.0;            // reading → current scale
  double gate = 0.0;            // deadband: ignore readings at/below this

  explicit Sensor(std::vector<int> ns, double gain = 1.0, double gate = 0.0)
      : neurons(std::move(ns)), readings(neurons.size(), 0.0), gain(gain),
        gate(gate) {}

  // Called by the world when new data arrives (per world-step).
  void set_readings(std::span<const double> r) {
    for (std::size_t i = 0; i < readings.size() && i < r.size(); ++i)
      readings[i] = r[i];
  }

  // Called every brain tick: re-inject the held readings so the neuron
  // integrates a sustained firing rate (§6 rate coding). The neuron's own
  // threshold decides whether it fires.
  void feed(ANNNetwork::Network &net) const {
    for (std::size_t i = 0; i < neurons.size(); ++i) {
      double x = readings[i] - gate;
      if (x > 0.0)
        net.increase_v(neurons[i], gain * x);
    }
  }
};

} // namespace annshin::body
