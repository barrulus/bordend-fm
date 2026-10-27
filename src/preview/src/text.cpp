#include <algorithm>
#include <fstream>
#include <string_view>

#include "nordend/preview/preview.hpp"

namespace nordend::preview {
namespace {

constexpr std::string_view kByteOrderMark = "\xEF\xBB\xBF";

bool continuation(char byte) { return (static_cast<unsigned char>(byte) & 0xC0U) == 0x80U; }

// line with tabs expanded to the next multiple of tab_width columns (one column per character),
// cut at max_bytes on a character boundary.
std::string expand(std::string_view line, const TextLimits& limits) {
    std::string out;
    out.reserve(std::min(line.size(), limits.max_line_bytes));
    std::size_t column = 0;
    for (const char byte : line) {
        if (byte == '\t') {
            const std::size_t spaces = limits.tab_width - (column % limits.tab_width);
            out.append(spaces, ' ');
            column += spaces;
        } else {
            out.push_back(byte);
            column += continuation(byte) ? 0U : 1U;
        }
        if (out.size() >= limits.max_line_bytes) {
            break;
        }
    }
    if (out.size() > limits.max_line_bytes) {
        out.resize(limits.max_line_bytes);
    }
    // Never end inside a character: a lead byte whose continuation bytes were cut goes too.
    std::size_t start = out.size();
    while (start > 0 && continuation(out[start - 1])) {
        --start;
    }
    if (start > 0) {
        const auto lead = static_cast<unsigned char>(out[start - 1]);
        std::size_t length = 1;
        if (lead >= 0xF0U) {
            length = 4;
        } else if (lead >= 0xE0U) {
            length = 3;
        } else if (lead >= 0xC0U) {
            length = 2;
        }
        if (out.size() - (start - 1) < length) {
            out.resize(start - 1);
        }
    }
    return out;
}

}  // namespace

std::optional<Text> read_text(const std::filesystem::path& path, const TextLimits& limits,
                              std::string& error) {
    std::ifstream file{path, std::ios::binary};
    if (!file) {
        error = "cannot open the file";
        return Text{};
    }
    // One byte more than the limit tells whether the file goes on.
    std::string data(limits.max_bytes + 1, '\0');
    file.read(data.data(), static_cast<std::streamsize>(data.size()));
    if (file.bad()) {
        error = "cannot read the file";
        return Text{};
    }
    data.resize(static_cast<std::size_t>(file.gcount()));
    if (data.find('\0') != std::string::npos) {
        return std::nullopt;
    }
    Text text;
    std::string_view rest{data};
    if (rest.size() > limits.max_bytes) {
        text.truncated = true;
        rest = rest.substr(0, limits.max_bytes);
        // The last line is incomplete: shown only when it is the only one.
        const std::size_t last = rest.rfind('\n');
        if (last != std::string_view::npos) {
            rest = rest.substr(0, last + 1);
        }
    }
    if (rest.starts_with(kByteOrderMark)) {
        rest.remove_prefix(kByteOrderMark.size());
    }
    while (!rest.empty()) {
        const std::size_t end = rest.find('\n');
        std::string_view line = rest.substr(0, end);
        if (line.ends_with('\r')) {
            line.remove_suffix(1);
        }
        text.lines.push_back(expand(line, limits));
        rest = end == std::string_view::npos ? std::string_view{} : rest.substr(end + 1);
    }
    return text;
}

}  // namespace nordend::preview
