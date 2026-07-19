#pragma once
#include "annshin/body/body.hpp"
#include "annshin/brain/network.hpp"
#include "annshin/render/frame.hpp"
#include "annshin/world/world.hpp"

namespace annshin::render {

// The only render files that read sim state. Build POD frames from live refs;
// (later, on a brain thread, these fill a triple-buffered slot instead.)
WorldFrame build_world_frame(const annshin::world::World &w);
BodyFrame build_body_frame(const annshin::body::Body &b,
                           const ANNNetwork::Network &net);

} // namespace annshin::render
