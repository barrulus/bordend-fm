#include "nordend/core/file.hpp"

#include <cerrno>
#include <cstddef>
#include <fstream>
#include <ios>
#include <system_error>
#include <utility>

namespace nordend::core {

TextFile read_text_file(const std::filesystem::path& path) {
    TextFile result;
    std::error_code error;
    const auto status = std::filesystem::status(path, error);
    if (status.type() == std::filesystem::file_type::not_found) {
        return result;
    }
    result.exists = true;
    if (error) {
        result.error = error.message();
        return result;
    }
    if (status.type() != std::filesystem::file_type::regular) {
        result.error = "not a regular file";
        return result;
    }
    std::ifstream file{path, std::ios::binary};
    if (!file.is_open()) {
        result.error = "cannot open: " + std::error_code{errno, std::generic_category()}.message();
        return result;
    }
    // Sized read: istreambuf_iterator trips GCC 14's -Wnull-dereference inside libstdc++.
    std::string text(static_cast<std::size_t>(std::filesystem::file_size(path, error)), '\0');
    file.read(text.data(), static_cast<std::streamsize>(text.size()));
    if (error || file.gcount() != static_cast<std::streamsize>(text.size())) {
        result.error = "read error";
        return result;
    }
    result.text = std::move(text);
    return result;
}

}  // namespace nordend::core
