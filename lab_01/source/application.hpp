#pragma once

#include "graphics_internal.hpp"
#include <string_view>

namespace application {

bool initialize();
void shutdown();

void update();
void render(const graphics::internal::FrameData& fd);
void configureView(std::string_view view);

} // namespace application
