#pragma once
#include <vector>

namespace annshin::render {

// The POD seam between sim and renderer. Renderers consume ONLY this — never
// live World/Network/Body refs — so we can swap ASCII→raylib→Godot, add a brain
// view, or move the brain to its own thread (publish a frame) without touching
// sim internals. v1 projects the 3D world top-down onto the x,z plane.

struct WorldObjectView {
  float x, z; // world x,z (top-down)
  int kind;   // 0 = food, 1 = fire, …
};

struct WorldFrame {
  float creature_x = 0.f, creature_z = 0.f, creature_heading = 0.f;
  float half_extent = 1.f;
  std::vector<WorldObjectView> objects;
};

struct DriveView {
  float value, setpoint, weight;
};

struct BodyFrame {
  float wellbeing = 0.f, hormone = 0.f, thrust = 0.f, turn = 0.f;
  std::vector<DriveView> drives;
};

struct RenderFrame {
  WorldFrame world;
  BodyFrame body;
  int meals = 0, burns = 0;
  long long tick = 0;
  // Reserved: BrainTopology/BrainFrame for the future 3D brain-graph view.
};

} // namespace annshin::render
