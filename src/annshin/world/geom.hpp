#pragma once
#include <cmath>

namespace annshin::world {

// The world is 3D (y = up). v1 movement is planar on the ground xz-plane via a
// yaw heading; the renderer projects to 2D top-down (x,z) for now. Keeping this
// 3D means the eventual 3D renderer / flight / terrain need no world changes.
struct Vec3 {
  double x = 0.0, y = 0.0, z = 0.0;
};

inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 operator*(Vec3 a, double s) { return {a.x * s, a.y * s, a.z * s}; }
inline double dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline double length(Vec3 a) { return std::sqrt(dot(a, a)); }

// Ground-plane (xz) basis vectors from a yaw heading (y is up).
inline Vec3 forward_xz(double yaw) { return {std::cos(yaw), 0.0, std::sin(yaw)}; }
inline Vec3 right_xz(double yaw) { return {std::sin(yaw), 0.0, -std::cos(yaw)}; }

struct Pose {
  Vec3 pos;
  double heading = 0.0; // yaw about +y, radians
};

} // namespace annshin::world
