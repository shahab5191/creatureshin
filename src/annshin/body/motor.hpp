#pragma once
#include "annshin/brain/network.hpp"

namespace annshin::body {

// The mirror image of a Sensor (SPEC §7): instead of injecting current, it
// reads a motor neuron's recent firing rate and turns it into an actuator
// force. Stateless — the rate lives in the neuron (Network::spike_rate).
struct Motor {
  int neuron;
  double gain = 1.0;
  double force(const ANNNetwork::Network &net) const {
    return gain * net.spike_rate(neuron);
  }
};

// Differential drive decode (§7): two antagonistic motors → thrust + turn.
struct DriveCommand {
  double thrust; // sum → forward
  double turn;   // difference → steer
};
inline DriveCommand differential(double left, double right) {
  return {left + right, left - right};
}

} // namespace annshin::body
