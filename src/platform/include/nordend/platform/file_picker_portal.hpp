#pragma once
#include <filesystem>

#include "nordend/core/event_loop.hpp"

namespace nordend::platform {
// Runs the FileChooser backend on the session bus, without connecting to Wayland.
int run_file_picker_portal(core::EventLoop& loop, const std::filesystem::path& picker_program = {});
}  // namespace nordend::platform
