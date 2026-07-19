#pragma once
#include "annshin/config.hpp"
#include <algorithm>
#include <cmath>

// Core math helpers — see SPEC.md §4.
namespace annshin::math {

using config::tick_t;

// Exponential decay for all lazy timestamp updates: x(t) = x(t₀)·exp(-Δt/τ).
inline double decay(double x, tick_t dt, double tau) {
  return x * std::exp(-static_cast<double>(dt) / tau);
}

// Hard saturating weight cap (prevents runaway potentiation).
inline double cap(double w) { return std::clamp(w, 0.0, config::W_MAX); }

} // namespace annshin::math
