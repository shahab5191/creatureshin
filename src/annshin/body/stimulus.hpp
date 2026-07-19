#pragma once

namespace annshin::body {

// The motivational half of a stimulus (SPEC §8): a contact of some world `kind`
// changes one drive by `delta`. Registered per kind in Body; the physical half
// (radius, smell, …) lives in world/StimulusDef. Keeps body free of world types.
struct DriveEffect {
  int drive_id;
  double delta;
};

} // namespace annshin::body
