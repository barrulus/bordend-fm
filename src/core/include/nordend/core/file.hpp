#pragma once

#include <filesystem>
#include <string>

namespace nordend::core {

struct TextFile {
    bool exists{false};  // false: no such file, which callers usually treat as "use defaults"
    std::string text;
    std::string error;  // set when the file exists but cannot be read; text is then empty
};

// Reads a whole regular file. Never throws for I/O problems: they land in error.
[[nodiscard]] TextFile read_text_file(const std::filesystem::path& path);

}  // namespace nordend::core
