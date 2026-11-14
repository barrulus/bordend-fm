#pragma once
#include <filesystem>

#include "rummage/core/event_loop.hpp"

namespace rummage::platform {
struct FilePickerPortalOptions {
    std::filesystem::path picker_program;
    std::chrono::milliseconds idle_timeout{30'000};
    std::chrono::milliseconds stop_timeout{3'000};
};

// Runs the FileChooser backend on the session bus, without connecting to Wayland.
int run_file_picker_portal(core::EventLoop& loop, FilePickerPortalOptions options = {});
}  // namespace rummage::platform
