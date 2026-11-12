#include "nordend/platform/archives.hpp"

#include <algorithm>
#include <array>
#include <string_view>

namespace nordend::platform {
namespace {

std::string lower_name(const std::filesystem::path& path) {
    auto name = path.filename().string();
    for (char& c : name) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c + ('a' - 'A'));
        }
    }
    return name;
}

}  // namespace

std::optional<ArchiveBackend> archive_backend(const core::ActionTarget& target,
                                              const Environment& environment) {
    using Kind = ArchiveBackend::Kind;
    const auto find = [&](std::string_view program) {
        return find_program(program, environment.path);
    };
    if (const auto program = find("file-roller")) {
        return ArchiveBackend{.kind = Kind::file_roller, .program = *program};
    }
    const auto& mime = target.mime;
    const auto name = lower_name(target.path);
    const bool zip = mime == "application/zip";
    const bool tgz = mime == "application/x-compressed-tar" || name.ends_with(".tar.gz") ||
                     name.ends_with(".tgz");
    const bool tbz = mime == "application/x-bzip-compressed-tar" || name.ends_with(".tar.bz2") ||
                     name.ends_with(".tbz2") || name.ends_with(".tbz");
    const bool txz = mime == "application/x-xz-compressed-tar" || name.ends_with(".tar.xz") ||
                     name.ends_with(".txz");
    const bool tzst = mime == "application/x-zstd-compressed-tar" || name.ends_with(".tar.zst") ||
                      name.ends_with(".tzst");
    const bool tar = mime == "application/x-tar" || tgz || tbz || txz || tzst;
    const bool gzip = !tar && (mime == "application/gzip" || mime == "application/x-gzip");
    if (zip) {
        if (const auto program = find("unzip")) {
            return ArchiveBackend{.kind = Kind::unzip, .program = *program};
        }
    }
    if (tar && (!tgz || find("gzip")) && (!tbz || find("bzip2")) && (!txz || find("xz")) &&
        (!tzst || find("zstd"))) {
        if (const auto program = find("tar")) {
            return ArchiveBackend{.kind = Kind::tar, .program = *program};
        }
    }
    if (gzip) {
        if (const auto program = find("gzip")) {
            return ArchiveBackend{.kind = Kind::gzip, .program = *program, .single_file = true};
        }
    }
    const bool zstd = mime == "application/zstd" || tzst;
    if (zip || tar || gzip || mime == "application/x-7z-compressed" ||
        mime == "application/vnd.rar" || mime == "application/x-rar" ||
        mime == "application/x-rar-compressed" || mime == "application/x-bzip2" ||
        mime == "application/x-xz" || zstd) {
        for (const auto* candidate : {"7zz", "7z"}) {
            // Older p7zip's 7z does not support Zstandard.
            if (zstd && std::string_view{candidate} == "7z") {
                continue;
            }
            if (const auto program = find(candidate)) {
                return ArchiveBackend{
                    .kind = Kind::seven_zip,
                    .program = *program,
                    .tar_layer = tgz || tbz || txz || tzst,
                    .single_file = !tar && (gzip || mime == "application/x-bzip2" ||
                                            mime == "application/x-xz" || zstd)};
            }
        }
    }
    return std::nullopt;
}

std::string archive_output_name(const std::filesystem::path& archive) {
    const auto lower = lower_name(archive);
    auto name = archive.filename().string();
    for (const auto* suffix :
         {".tar.gz", ".tar.bz2", ".tar.xz", ".tar.zst", ".tgz", ".tbz2", ".tbz", ".txz", ".tzst",
          ".zip", ".7z", ".rar", ".tar", ".gz", ".bz2", ".xz", ".zst"}) {
        if (lower.ends_with(suffix)) {
            name.resize(name.size() - std::string_view{suffix}.size());
            break;
        }
    }
    if (name.empty() || name == "." || name == "..") {
        name = "Extracted";
    }
    // Leave room for a collision suffix and the visible in-progress name.
    return name.substr(0, 180);
}

std::vector<std::string> archive_command(const ArchiveBackend& backend,
                                         const std::filesystem::path& archive, bool stream) {
    const auto program = backend.program.string();
    const auto path = archive.string();
    switch (backend.kind) {
        case ArchiveBackend::Kind::file_roller:
            return {program, "--extract-here", path};
        case ArchiveBackend::Kind::seven_zip: {
            // Supply a noninteractive password: encrypted inputs fail instead of waiting on stdin.
            std::vector<std::string> args{program, "x",     "-aos", "-y", "-bd",
                                          "-bso0", "-bsp0", "-spd", "-p-"};
            if (stream || backend.single_file) {
                args.push_back("-so");
            }
            args.insert(args.end(), {"--", path});
            return args;
        }
        case ArchiveBackend::Kind::unzip:
            // Info-ZIP also treats wildcards inside an argv as patterns. An inherited open
            // archive fd, supplied by the caller, gives it a literal /proc/self/fd path.
            return {program, "-n", "-qq", path};
        case ArchiveBackend::Kind::gzip:
            return {program, "-d", "-c", "--", path};
        case ArchiveBackend::Kind::tar:
            return {program,
                    "--extract",
                    "--keep-old-files",
                    "--no-same-owner",
                    "--no-same-permissions",
                    "--file",
                    path};
    }
    return {};
}

}  // namespace nordend::platform
