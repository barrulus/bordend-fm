#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "nordend/core/actions.hpp"
#include "nordend/platform/desktop_entry.hpp"

namespace nordend::platform {

struct ArchiveBackend {
    enum class Kind { file_roller, seven_zip, unzip, gzip, tar };
    Kind kind{};
    std::filesystem::path program;
    bool tar_layer{false};  // 7z needs a separate pass for compressed tar
    bool single_file{false};
};

// File Roller first, then a format-specific CLI, then 7zz/7z. All programs are optional.
[[nodiscard]] std::optional<ArchiveBackend> archive_backend(const core::ActionTarget& target,
                                                            const Environment& environment);
[[nodiscard]] std::string archive_output_name(const std::filesystem::path& archive);
// CLI commands operate in a fresh directory. stdout is redirected for gzip / first tar pass.
[[nodiscard]] std::vector<std::string> archive_command(const ArchiveBackend& backend,
                                                       const std::filesystem::path& archive,
                                                       bool stream = false);

}  // namespace nordend::platform
