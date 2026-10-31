#include <cstddef>
#include <format>
#include <optional>
#include <string>
#include <string_view>

#include "nordend/core/config.hpp"

namespace nordend::core {
namespace {

constexpr std::string_view kBlank = " \t";

std::string_view trim(std::string_view text) {
    const std::size_t first = text.find_first_not_of(kBlank);
    if (first == std::string_view::npos) {
        return {};
    }
    return text.substr(first, text.find_last_not_of(kBlank) - first + 1);
}

// A key as written: bare, or quoted ("family" and 'family' name the same key).
std::string_view unquote(std::string_view key) {
    if (key.size() >= 2 && (key.front() == '"' || key.front() == '\'') &&
        key.back() == key.front()) {
        return key.substr(1, key.size() - 2);
    }
    return key;
}

// The table a header line opens ("[font]" -> "font"); nullopt when the line is no header.
std::optional<std::string> header_of(std::string_view line) {
    line = trim(line);
    if (line.size() < 2 || line.front() != '[' || line.starts_with("[[")) {
        return std::nullopt;
    }
    const std::size_t close = line.find(']');
    if (close == std::string_view::npos) {
        return std::nullopt;
    }
    std::string name;
    std::string_view rest = line.substr(1, close - 1);
    // Dotted table names, each part maybe quoted, spaces around the dots allowed.
    while (!rest.empty()) {
        const std::size_t dot = rest.find('.');
        name += (name.empty() ? "" : ".");
        name += unquote(trim(rest.substr(0, dot)));
        rest = dot == std::string_view::npos ? std::string_view{} : rest.substr(dot + 1);
    }
    return name;
}

// The key a "key = value" line sets, dotted parts joined; nullopt for any other line.
std::optional<std::string> key_of(std::string_view line) {
    const std::string_view text = trim(line);
    if (text.empty() || text.front() == '#' || text.front() == '[') {
        return std::nullopt;
    }
    const std::size_t equals = text.find('=');
    if (equals == std::string_view::npos) {
        return std::nullopt;
    }
    std::string name;
    std::string_view rest = trim(text.substr(0, equals));
    while (!rest.empty()) {
        const std::size_t dot = rest.find('.');
        name += (name.empty() ? "" : ".");
        name += unquote(trim(rest.substr(0, dot)));
        rest = dot == std::string_view::npos ? std::string_view{} : rest.substr(dot + 1);
    }
    return name;
}

// Where the value starting at text[start] ends (one past its last character): a string to its
// closing quote (three for a multi-line one, across lines), anything else to a comment or the
// end of the line.
std::size_t value_end(std::string_view text, std::size_t start) {
    const std::string_view rest = text.substr(start);
    for (const std::string_view triple : {std::string_view{R"(""")"}, std::string_view{"'''"}}) {
        if (rest.starts_with(triple)) {
            const std::size_t close = text.find(triple, start + 3);
            return close == std::string_view::npos ? text.size() : close + 3;
        }
    }
    if (!rest.empty() && (rest.front() == '"' || rest.front() == '\'')) {
        const char quote = rest.front();
        for (std::size_t i = start + 1; i < text.size() && text[i] != '\n'; ++i) {
            if (quote == '"' && text[i] == '\\') {
                ++i;  // an escaped character, a quote included
            } else if (text[i] == quote) {
                return i + 1;
            }
        }
    }
    std::size_t end = start;
    while (end < text.size() && text[end] != '\n' && text[end] != '#') {
        ++end;
    }
    while (end > start &&
           (text[end - 1] == ' ' || text[end - 1] == '\t' || text[end - 1] == '\r')) {
        --end;
    }
    return end;
}

struct Line {
    std::size_t begin;
    std::size_t end;  // the newline's position, or the text's size
};

}  // namespace

std::string config_problem(const ConfigResult& result, std::string_view section,
                           std::string_view key) {
    const std::string quoted = std::format("'{}.{}' ", section, key);
    for (const std::string& problem : result.problems) {
        std::string_view what = problem;
        if (const std::size_t at = what.find(quoted); at != std::string_view::npos) {
            what = what.substr(at + quoted.size());
        } else if (!result.from_file) {
            // "config.toml:3:9: <what>; the whole file is ignored...": after the position.
            const std::size_t colon = what.find(": ");
            what = colon == std::string_view::npos ? what : what.substr(colon + 2);
        } else {
            continue;
        }
        return std::string{what.substr(0, what.find("; "))};
    }
    return {};
}

std::string toml_string(std::string_view text) {
    std::string out = "\"";
    for (const char c : text) {
        switch (c) {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                if (static_cast<unsigned char>(c) < 0x20 || c == 0x7F) {
                    out += std::format("\\u{:04X}", static_cast<unsigned char>(c));
                } else {
                    out += c;
                }
        }
    }
    return out + "\"";
}

std::string set_config_value(std::string_view text, std::string_view section, std::string_view key,
                             std::string_view value) {
    const std::string dotted = std::format("{}.{}", section, key);
    std::string table;                 // the table the line is in
    bool section_seen = false;         // its header was met
    std::optional<std::size_t> after;  // the end of the section's last key line
    std::size_t begin = 0;
    while (begin <= text.size()) {
        const std::size_t newline = text.find('\n', begin);
        const Line line{begin, newline == std::string_view::npos ? text.size() : newline};
        const std::string_view content = text.substr(line.begin, line.end - line.begin);
        if (const std::optional<std::string> header = header_of(content)) {
            table = *header;
            section_seen = section_seen || table == section;
        } else if (const std::optional<std::string> name = key_of(content)) {
            const bool here =
                (table == section && *name == key) || (table.empty() && *name == dotted);
            if (here) {
                std::size_t start = text.find('=', line.begin) + 1;
                while (start < text.size() && (text[start] == ' ' || text[start] == '\t')) {
                    ++start;
                }
                const std::size_t end = value_end(text, start);
                std::string out{text.substr(0, start)};
                out += value;
                out += text.substr(end);
                return out;
            }
            if (table == section) {
                after = line.end;
            }
        }
        if (newline == std::string_view::npos) {
            break;
        }
        begin = newline + 1;
    }
    const std::string assignment = std::format("{} = {}", key, value);
    if (section_seen && after) {
        std::string out{text.substr(0, *after)};
        out += "\n" + assignment;
        out += text.substr(*after);
        return out;
    }
    if (section_seen) {
        // An empty section: right under its header.
        std::size_t at = 0;
        for (std::size_t pos = 0; pos <= text.size();) {
            const std::size_t newline = text.find('\n', pos);
            const std::size_t end = newline == std::string_view::npos ? text.size() : newline;
            if (const auto header = header_of(text.substr(pos, end - pos)); header == section) {
                at = end;
                break;
            }
            if (newline == std::string_view::npos) {
                break;
            }
            pos = newline + 1;
        }
        std::string out{text.substr(0, at)};
        out += "\n" + assignment;
        out += text.substr(at);
        return out;
    }
    std::string out{text};
    if (!out.empty() && !out.ends_with('\n')) {
        out += '\n';
    }
    if (!out.empty()) {
        out += '\n';
    }
    out += std::format("[{}]\n{}\n", section, assignment);
    return out;
}

}  // namespace nordend::core
