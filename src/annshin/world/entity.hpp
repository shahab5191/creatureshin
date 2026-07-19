#pragma once
#include "annshin/config.hpp"
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
// Grow by adding columns (sound, albedo, blocks_movement, …).
struct StimulusDef {
  int kind = 0; // == its index; assigned by World::define_stimulus
  double radius = 0.5;
  bool consumed_on_contact = false;
  // Odor signature: a SPARSE code over N_ODOR receptors. Each smell activates
  // K_SCENT receptors (a hashed fingerprint), so different smells occupy mostly
  // *different* receptors → distinct weights → punishing one smell doesn't
  // cannibalize another. A new item = a new scent string.
  double scent[annshin::config::N_ODOR] = {0};
  double smell_strength = 0.0; // 0 = odorless
};

// Hash a scent string to a sparse binary pattern: K_SCENT of the N_ODOR
// receptors set to 1. Deterministic (a given string always maps the same way).
inline void set_scent(StimulusDef &d, const char *s) {
  namespace cfg = annshin::config;
  for (int k = 0; k < cfg::N_ODOR; ++k)
    d.scent[k] = 0.0;
  unsigned h = 2166136261u; // FNV-1a
  for (const char *p = s; *p; ++p) {
    h ^= static_cast<unsigned char>(*p);
    h *= 16777619u;
  }
  unsigned state = h ? h : 1u;
  for (int picked = 0; picked < cfg::K_SCENT && picked < cfg::N_ODOR;) {
    state = state * 1664525u + 1013904223u; // LCG
    int idx = static_cast<int>(state % cfg::N_ODOR);
    if (d.scent[idx] == 0.0) {
      d.scent[idx] = 1.0;
      ++picked;
    }
  }
}

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
