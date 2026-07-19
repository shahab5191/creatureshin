#include "annshin/body/body.hpp"
#include "annshin/brain/network.hpp"
#include "annshin/config.hpp"

#include <array>
#include <cstdio>

using ANNNetwork::Network;
using namespace annshin::body;
namespace cfg = annshin::config;

int main() {
  constexpr int N = 200;
  Network net(N, cfg::FANOUT);
  Body body;

  // Exteroceptive: a 2-neuron "smell" sensor (left/right), driving neurons 0,1.
  int smell = body.add_sensor(Sensor{{0, 1}, /*gain=*/1.5});

  // Interoceptive drive: energy, felt as "hunger" on neuron 2.
  int energy = body.add_drive(Drive{/*value=*/100, /*setpoint=*/100,
                                     /*weight=*/1.0, /*drift=*/-0.02,
                                     /*neurons=*/{2}, /*intero_gain=*/0.05});

  // Motors (§7): left/right differential drive. NOTE: for this untuned demo we
  // read out the two smell neurons (which fire reliably) so the rate readout
  // shows real numbers — in a real creature motor neurons are downstream cells
  // the network learns to connect. Stronger smell on the right (reading 1.0 vs
  // 0.2) → right neuron fires more → the decoded `turn` steers accordingly.
  body.add_motor(Motor{/*neuron=*/0, /*gain=*/1.0}); // left
  body.add_motor(Motor{/*neuron=*/1, /*gain=*/1.0}); // right

  std::printf("start: %zu neurons, %zu synapses\n\n", (size_t)N,
              net.edge_count());
  std::puts("  tick |   energy | wellbeing |  hormone | fired | edges | thrust");
  std::puts("-------+----------+-----------+----------+-------+-------+-------");

  const int TICKS = 3000;
  for (int t = 0; t < TICKS; ++t) {
    // --- world step every 10 ticks: refresh readings, deliver stimuli (§5.7) ---
    if (t % cfg::TICKS_PER_WORLD_STEP == 0) {
      // pretend food scent is present on the right the whole time
      std::array<double, 2> scent{0.2, 1.0};
      body.set_sensor_readings(smell, scent);
      // at t=1500 the creature "eats": a one-shot energy stimulus (§8)
      if (t == 1500)
        body.apply_effect(energy, +40.0);
    }

    // --- per tick: body first (sense + hormone), then the brain ticks ---
    body.on_tick(net);
    net.step();

    if (t % 300 == 0 || t == 1500 || t == 1501) {
      DriveCommand cmd = body.decode_drive(net);
      std::printf(" %5d | %8.2f | %9.1f | %+8.4f | %5zu | %5zu | %6.3f\n", t,
                  body.drive(energy).value, body.wellbeing(),
                  body.last_hormone(), net.frontier_size(), net.edge_count(),
                  cmd.thrust);
    }
  }

  std::puts("\ndone.");
  return 0;
}
