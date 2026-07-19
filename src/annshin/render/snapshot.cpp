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
