#pragma once
#include "annshin/render/frame.hpp"

namespace annshin::render {

// Renderer interface. A driver builds a RenderFrame each display frame and calls
// draw(). Swapping backends (raylib now, Godot later) never touches the sim.
struct Renderer {
  virtual ~Renderer() = default;
  virtual void begin() {}
  virtual bool should_close() { return true; }
  virtual void draw(const RenderFrame &) = 0;
  virtual void end() {}
};

} // namespace annshin::render
