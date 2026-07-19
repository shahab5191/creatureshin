#pragma once
#include "annshin/world/geom.hpp"
#include <vector>

namespace annshin::world {

class World; // fwd

// A sensory modality: maps world state at an observer pose → one reading per
// target sensor neuron. Adding a modality = a new subclass + one loop binding.
struct SensoryModel {
  virtual ~SensoryModel() = default;
  virtual int channels() const = 0;
  virtual void sample(const World &w, const Pose &observer,
                      std::vector<double> &out) const = 0;
};

// v1 olfaction: samples smell concentration at two nostrils (left/right of the
// heading) for objects on `channel`. Reads World::smell_at, so swapping the
// nearest-object smell for a diffusing field later needs no change here.
struct SmellSense : SensoryModel {
  int channel;
  double nose_fwd, nose_sep;
  explicit SmellSense(int channel, double nose_fwd = 0.5, double nose_sep = 0.5)
      : channel(channel), nose_fwd(nose_fwd), nose_sep(nose_sep) {}
  int channels() const override { return 2; }
  void sample(const World &w, const Pose &obs,
              std::vector<double> &out) const override;
};

} // namespace annshin::world
