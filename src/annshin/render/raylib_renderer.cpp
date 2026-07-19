#include "annshin/render/raylib_renderer.hpp"
#include "raylib.h"

#include <cmath>
#include <cstdio>

namespace annshin::render {

namespace {
Color lerp_col(Color a, Color b, float t) {
  auto L = [&](unsigned char x, unsigned char y) {
    return (unsigned char)(x + (y - x) * t);
  };
  return Color{L(a.r, b.r), L(a.g, b.g), L(a.b, b.b), 255};
}
} // namespace

RaylibRenderer::RaylibRenderer(int width, int height) : w_(width), h_(height) {}

void RaylibRenderer::begin() {
  InitWindow(w_, h_, "ANNShin");
  SetTargetFPS(60);
}

bool RaylibRenderer::should_close() { return WindowShouldClose(); }

void RaylibRenderer::end() { CloseWindow(); }

void RaylibRenderer::draw(const RenderFrame &f) {
  const int m = 10;
  const int world = h_ - 2 * m; // left square: the world map
  const int wx = m, wy = m;
  const float half = f.world.half_extent > 0 ? f.world.half_extent : 1.f;
  auto wsx = [&](float x) { return wx + (int)((x + half) / (2 * half) * world); };
  auto wsy = [&](float z) { return wy + (int)((half - z) / (2 * half) * world); };

  const int brain = 480; // right square: the brain map
  const int bx = wx + world + 20, by = m;
  auto bsx = [&](float x) { return bx + (int)(x * brain); };
  auto bsy = [&](float y) { return by + (int)((1.f - y) * brain); };

  BeginDrawing();
  ClearBackground(Color{18, 18, 26, 255});

  // ---- world map ----
  DrawRectangleLines(wx, wy, world, world, Color{60, 60, 84, 255});
  DrawText("world", wx + 6, wy + 6, 18, Color{120, 120, 150, 255});
  for (const auto &o : f.world.objects)
    DrawCircle(wsx(o.x), wsy(o.z), 6.f, (o.kind == 0) ? GREEN : RED);
  int cx = wsx(f.world.creature_x), cy = wsy(f.world.creature_z);
  DrawCircleLines(cx, cy, 8.f, RAYWHITE);
  DrawCircle(cx, cy, 4.f, RAYWHITE);
  DrawLine(cx, cy, cx + (int)(16 * std::cos(f.world.creature_heading)),
           cy - (int)(16 * std::sin(f.world.creature_heading)), RAYWHITE);

  // ---- brain graph (synapse edges + neuron nodes; flash on firing) ----
  DrawRectangleLines(bx, by, brain, brain, Color{60, 60, 84, 255});
  DrawText("brain", bx + 6, by + 6, 18, Color{120, 120, 150, 255});
  const auto &ns = f.brain.neurons;

  // edges first (so nodes draw on top). A synapse lights up when its SOURCE
  // just fired — that's the spike travelling down the axon.
  for (const auto &e : f.brain.edges) {
    if (e.a < 0 || e.b < 0 || (std::size_t)e.a >= ns.size() ||
        (std::size_t)e.b >= ns.size())
      continue;
    const auto &a = ns[e.a];
    const auto &b = ns[e.b];
    Vector2 pa{(float)bsx(a.x), (float)bsy(a.y)};
    Vector2 pb{(float)bsx(b.x), (float)bsy(b.y)};
    if (a.flash > 0.05f) { // spike travelling down the axon
      unsigned char al = (unsigned char)(90 + 165 * a.flash);
      DrawLineEx(pa, pb, 1.0f + 3.0f * a.flash, Color{150, 210, 255, al});
    } else { // resting: thickness AND opacity ∝ weight, so strong synapses show
      unsigned char al = (unsigned char)(18 + 150 * e.w);
      DrawLineEx(pa, pb, 0.6f + 2.6f * e.w, Color{95, 100, 140, al});
    }
  }

  // nodes
  const Color rest{40, 42, 70, 255};
  const Color hot_exc{255, 238, 120, 255}; // excitatory baseline heat
  const Color hot_inh{255, 110, 110, 255}; // inhibitory baseline heat
  for (const auto &nv : ns) {
    float t = nv.excitation / (nv.excitation + 4.f); // saturating 0..1
    Color c = lerp_col(rest, nv.polarity < 0 ? hot_inh : hot_exc, t);
    int x = bsx(nv.x), y = bsy(nv.y);
    DrawCircle(x, y, 2.f + 2.5f * t, c);
    if (nv.flash > 0.05f) { // just-fired highlight
      unsigned char al = (unsigned char)(255 * nv.flash);
      DrawCircle(x, y, 3.f + 5.f * nv.flash, Color{255, 255, 255, al});
      DrawCircleLines(x, y, 6.f + 7.f * nv.flash, Color{255, 255, 255, al});
    }
  }

  // ---- HUD (below the brain map) ----
  int hx = bx, y = by + brain + 14;
  char buf[128];
  auto line = [&](const char *s, Color c) { DrawText(s, hx, y, 20, c); y += 26; };
  std::snprintf(buf, sizeof buf, "tick %lld   meals %d   burns %d", f.tick,
                f.meals, f.burns);
  line(buf, RAYWHITE);
  std::snprintf(buf, sizeof buf, "wellbeing %.0f   hormone %+.2f",
                f.body.wellbeing, f.body.hormone);
  line(buf, SKYBLUE);
  const char *names[2] = {"energy", "health"};
  for (std::size_t i = 0; i < f.body.drives.size(); ++i) {
    const auto &d = f.body.drives[i];
    std::snprintf(buf, sizeof buf, "%-7s %.0f / %.0f", i < 2 ? names[i] : "drive",
                  d.value, d.setpoint);
    line(buf, (i == 1) ? Color{255, 120, 120, 255} : GREEN);
  }

  EndDrawing();
}

} // namespace annshin::render
