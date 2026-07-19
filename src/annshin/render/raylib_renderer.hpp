#pragma once
#include "annshin/render/renderer.hpp"

namespace annshin::render {

// v1 renderer: a raylib window drawing the 3D world top-down (x,z) as a 2D map —
// green food, red fire, white creature + heading — plus a HUD. Reads only
// RenderFrame, so a 3D world view / brain graph later is just another Renderer.
class RaylibRenderer : public Renderer {
public:
  explicit RaylibRenderer(int width = 1280, int height = 720);
  void begin() override;
  bool should_close() override;
  void draw(const RenderFrame &) override;
  void end() override;

private:
  int w_, h_;
  // view toggles (clickable checkboxes)
  bool show_edges_ = true;
  bool rf_only_ = false;    // show only the reward/punish working set
  bool flash_only_ = false; // show only neurons firing right now
};

} // namespace annshin::render
