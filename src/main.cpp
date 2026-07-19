#include "annshin/config.hpp"
#include "annshin/loop/simulation.hpp"

#include <cstdio>
#include <cstring>

#ifdef ANNSHIN_HAVE_RAYLIB
#include "annshin/render/raylib_renderer.hpp"
#include "annshin/render/snapshot.hpp"
#endif

using namespace annshin;

static void run_headless(loop::Simulation &sim) {
  std::puts("headless run (no window) — pass no flag to open the raylib window");
  for (int t = 0; t < 20000; ++t) {
    sim.tick();
    if (t % 2000 == 0)
      std::printf("t=%5d  energy=%.0f health=%.0f  meals=%d burns=%d  "
                  "wellbeing=%.0f edges=%zu\n",
                  t, sim.body().drive(0).value, sim.body().drive(1).value,
                  sim.meals(), sim.burns(), sim.body().wellbeing(),
                  sim.net().edge_count());
  }
  std::printf("done. meals=%d burns=%d\n", sim.meals(), sim.burns());
}

int main(int argc, char **argv) {
  bool headless = false;
  for (int i = 1; i < argc; ++i)
    if (std::strcmp(argv[i], "--headless") == 0)
      headless = true;

  loop::Simulation sim;

#ifdef ANNSHIN_HAVE_RAYLIB
  if (!headless) {
    render::RaylibRenderer r;
    r.begin();
    while (!r.should_close()) {
      for (int k = 0; k < config::TICKS_PER_FRAME; ++k)
        sim.tick();
      render::RenderFrame f;
      f.world = render::build_world_frame(sim.world());
      f.body = render::build_body_frame(sim.body(), sim.net());
      f.meals = sim.meals();
      f.burns = sim.burns();
      f.tick = static_cast<long long>(sim.net().tick());
      r.draw(f);
    }
    r.end();
    return 0;
  }
#endif

  run_headless(sim);
  return 0;
}
