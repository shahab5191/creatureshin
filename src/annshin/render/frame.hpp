#pragma once
#include <vector>

namespace annshin::render {

// The POD seam between sim and renderer. Renderers consume ONLY this — never
// live World/Network/Body refs — so we can swap ASCII→raylib→Godot, add a brain
// view, or move the brain to its own thread (publish a frame) without touching
// sim internals. v1 projects the 3D world top-down onto the x,z plane.

struct WorldObjectView {
  float x, z;              // world x,z (top-down)
  int kind;                // 0 = food, 1 = fire, …
  float smell_radius = 0.f; // scent reach in world units (0 = odorless)
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

// Brain graph: one node per neuron + synapse edges. Positions are static (unit
// cube); `excitation` (rate) and `flash` (just-fired highlight, 1→0) change each
// frame. `polarity` tints exc/inhib.
struct NeuronView {
  float x, y, z;    // unit-cube position
  float excitation; // spike_rate(i), decayed to now
  float flash;      // 1.0 right after firing, fades to 0 (highlight)
  int polarity;     // +1 excitatory / -1 inhibitory
  bool in_rf;       // in the reward/punish working set (fired within H_RECENT)
};

struct BrainEdgeView {
  int a, b;  // neuron indices (source → target)
  float w;   // weight
};

struct BrainFrame {
  std::vector<NeuronView> neurons;
  std::vector<BrainEdgeView> edges;
};

struct RenderFrame {
  WorldFrame world;
  BodyFrame body;
  BrainFrame brain;
  int meals = 0, burns = 0;
  long long tick = 0;
};

} // namespace annshin::render
