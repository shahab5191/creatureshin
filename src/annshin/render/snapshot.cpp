#include "annshin/render/snapshot.hpp"

namespace annshin::render {

WorldFrame build_world_frame(const annshin::world::World &w) {
  WorldFrame f;
  const auto &c = w.creature();
  f.creature_x = static_cast<float>(c.pose.pos.x);
  f.creature_z = static_cast<float>(c.pose.pos.z);
  f.creature_heading = static_cast<float>(c.pose.heading);
  f.half_extent = static_cast<float>(w.half_extent());
  for (const auto &e : w.objects()) {
    if (!e.alive)
      continue;
    f.objects.push_back({static_cast<float>(e.pos.x),
                         static_cast<float>(e.pos.z), e.kind});
  }
  return f;
}

BrainFrame build_brain_frame(const ANNNetwork::Network &net) {
  namespace cfg = annshin::config;
  BrainFrame f;
  std::size_t n = net.neuron_count();
  auto now = net.tick();
  f.neurons.reserve(n);
  for (std::size_t i = 0; i < n; ++i) {
    int id = static_cast<int>(i);
    auto p = net.neuron_pos(id);
    double age = static_cast<double>(now - net.last_spike_time(id));
    float flash = (age >= 0 && age < cfg::FLASH_TICKS)
                      ? static_cast<float>(1.0 - age / cfg::FLASH_TICKS)
                      : 0.f;
    f.neurons.push_back({static_cast<float>(p.x), static_cast<float>(p.y),
                         static_cast<float>(p.z),
                         static_cast<float>(net.spike_rate(id)), flash,
                         net.neuron_polarity(id)});
  }
  std::vector<ANNNetwork::Network::EdgeView> es;
  net.collect_edges(es);
  f.edges.reserve(es.size());
  for (const auto &e : es)
    f.edges.push_back({e.src, e.tgt, e.w});
  return f;
}

BodyFrame build_body_frame(const annshin::body::Body &b,
                           const ANNNetwork::Network &net) {
  BodyFrame f;
  f.wellbeing = static_cast<float>(b.wellbeing());
  f.hormone = static_cast<float>(b.last_hormone());
  auto cmd = b.decode_drive(net);
  f.thrust = static_cast<float>(cmd.thrust);
  f.turn = static_cast<float>(cmd.turn);
  for (std::size_t i = 0; i < b.drive_count(); ++i) {
    const auto &d = b.drive(static_cast<int>(i));
    f.drives.push_back({static_cast<float>(d.value),
                        static_cast<float>(d.setpoint),
                        static_cast<float>(d.weight)});
  }
  return f;
}

} // namespace annshin::render
