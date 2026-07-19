#pragma once
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace annshin {

// Uniform spatial hash for O(local-density) neighbor queries (SPEC §11.1).
// Cell size ≈ r_reach, so a query visits a 3×3×3 block of cells around a point.
// Neuron positions are fixed after init (no movement / neurogenesis yet), so the
// grid is built once and never updated — see the caveat in Network's ctor.
class SpatialGrid {
public:
  explicit SpatialGrid(double cell) : cell_(cell) {}

  void insert(int id, double x, double y, double z) {
    buckets_[key(coord(x), coord(y), coord(z))].push_back(id);
  }

  // Invoke f(id) for every neuron in the 27 cells around (x,y,z). The caller
  // does the exact distance / recency / dedup checks.
  template <class F> void for_neighbors(double x, double y, double z, F &&f) const {
    int cx = coord(x), cy = coord(y), cz = coord(z);
    for (int dx = -1; dx <= 1; ++dx)
      for (int dy = -1; dy <= 1; ++dy)
        for (int dz = -1; dz <= 1; ++dz) {
          auto it = buckets_.find(key(cx + dx, cy + dy, cz + dz));
          if (it == buckets_.end())
            continue;
          for (int id : it->second)
            f(id);
        }
  }

private:
  int coord(double x) const { return static_cast<int>(std::floor(x / cell_)); }
  static std::int64_t key(int cx, int cy, int cz) {
    // pack three 21-bit cell coords into one 64-bit key (positions are ≥ 0)
    return (static_cast<std::int64_t>(cx & 0x1FFFFF) << 42) |
           (static_cast<std::int64_t>(cy & 0x1FFFFF) << 21) |
           static_cast<std::int64_t>(cz & 0x1FFFFF);
  }

  double cell_;
  std::unordered_map<std::int64_t, std::vector<int>> buckets_;
};

} // namespace annshin
