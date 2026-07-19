#pragma once
#include "annshin/config.hpp"
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

// Olfaction: samples the 4-receptor odor vector at two nostrils (left/right of
// the heading) → 8 neurons [L0 L1 L2 L3 | R0 R1 R2 R3]. The *pattern* says which
// smell (identity), left-vs-right says direction. Reads World::odor_at, so a
// diffusing odor field later is a drop-in with no change here.
struct OdorSense : SensoryModel {
  double nose_fwd, nose_sep;
  explicit OdorSense(double nose_fwd = 0.5, double nose_sep = 0.5)
      : nose_fwd(nose_fwd), nose_sep(nose_sep) {}
  int channels() const override { return 2 * annshin::config::N_ODOR; }
  void sample(const World &w, const Pose &obs,
              std::vector<double> &out) const override;
};

} // namespace annshin::world
