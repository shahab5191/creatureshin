#include "annshin/render/raylib_renderer.hpp"
#include "raylib.h"

#include <cmath>
#include <cstdio>

namespace annshin::render {

RaylibRenderer::RaylibRenderer(int width, int height) : w_(width), h_(height) {}

void RaylibRenderer::begin() {
  InitWindow(w_, h_, "ANNShin");
  SetTargetFPS(60);
}

bool RaylibRenderer::should_close() { return WindowShouldClose(); }

void RaylibRenderer::end() { CloseWindow(); }

void RaylibRenderer::draw(const RenderFrame &f) {
  const int margin = 10;
  const int map = h_ - 2 * margin;      // square top-down map
  const int ox = margin, oy = margin;
  const float half = f.world.half_extent > 0 ? f.world.half_extent : 1.f;

  auto sx = [&](float x) { return ox + (int)((x + half) / (2 * half) * map); };
  auto sy = [&](float z) { return oy + (int)((half - z) / (2 * half) * map); }; // +z = up

  BeginDrawing();
  ClearBackground(Color{18, 18, 26, 255});
  DrawRectangleLines(ox, oy, map, map, Color{60, 60, 84, 255});

  for (const auto &o : f.world.objects) {
    Color col = (o.kind == 0) ? GREEN : RED; // 0 = food, 1 = fire
    DrawCircle(sx(o.x), sy(o.z), 6.f, col);
  }

  int cx = sx(f.world.creature_x), cy = sy(f.world.creature_z);
  DrawCircleLines(cx, cy, 8.f, RAYWHITE);
  DrawCircle(cx, cy, 4.f, RAYWHITE);
  int hx = cx + (int)(16.f * std::cos(f.world.creature_heading));
  int hy = cy - (int)(16.f * std::sin(f.world.creature_heading)); // +z up on screen
  DrawLine(cx, cy, hx, hy, RAYWHITE);

  // --- HUD ---
  int hudx = ox + map + 20, y = 16;
  char buf[128];
  auto line = [&](const char *s, Color c) { DrawText(s, hudx, y, 20, c); y += 26; };
  std::snprintf(buf, sizeof buf, "tick   %lld", f.tick);       line(buf, RAYWHITE);
  std::snprintf(buf, sizeof buf, "meals  %d", f.meals);         line(buf, GREEN);
  std::snprintf(buf, sizeof buf, "burns  %d", f.burns);         line(buf, RED);
  std::snprintf(buf, sizeof buf, "wellbeing %.0f", f.body.wellbeing); line(buf, RAYWHITE);
  std::snprintf(buf, sizeof buf, "hormone %+.2f", f.body.hormone);    line(buf, SKYBLUE);
  std::snprintf(buf, sizeof buf, "thrust %.1f turn %+.1f", f.body.thrust, f.body.turn);
  line(buf, GRAY);
  y += 8;
  const char *names[2] = {"energy", "health"};
  for (std::size_t i = 0; i < f.body.drives.size(); ++i) {
    const auto &d = f.body.drives[i];
    const char *nm = i < 2 ? names[i] : "drive";
    std::snprintf(buf, sizeof buf, "%-7s %.0f / %.0f", nm, d.value, d.setpoint);
    line(buf, (i == 1) ? Color{255, 120, 120, 255} : GREEN);
  }

  EndDrawing();
}

} // namespace annshin::render
