#pragma once
#include "annshin/world/geom.hpp"
#include <vector>

namespace annshin::world {

// A world object. Carries no behavior — only a `kind` index into the
// StimulusDef table. New object types are new table rows, never new C++ types.
struct Entity {
  int id = 0;
  int kind = 0;
  Vec3 pos;
  double radius = 0.5;
  bool alive = true;
};

// Physical/emission properties of an object *type* (SPEC §8: "adding a stimulus
// = one table entry"). The drive-effect half lives in body/ (register_stimulus).
// Grow by adding columns (sound_channel, albedo, blocks_movement, …).
struct StimulusDef {
  int kind = 0; // == its index; assigned by World::define_stimulus
  double radius = 0.5;
  bool consumed_on_contact = false;
  int smell_channel = -1;      // -1 = does not emit smell
  double smell_strength = 0.0;
};

struct Contact {
  int kind = 0;
};
struct StepResult {
  std::vector<Contact> contacts; // touched this step (consumed ones removed)
};

// The creature's *physical* state (distinct from the neural body::Body).
struct Creature {
  Pose pose;
  double radius = 0.5;
};

} // namespace annshin::world
