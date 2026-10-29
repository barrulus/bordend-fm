// Default applications check (ROADMAP 6.1). Not installed.
//
//   nordend-open [--dry-run] FILE...   open files as Nordend does, or only print how
//   nordend-open --default TYPE...     the default application of each type
//   nordend-open --types               every type a mimeapps.list or desktop entry names
//
// --dry-run prints "<type>\t<file>" for each file, then each process: its application, folder
// and command line. --default prints "<type>\t<desktop ID>" ("-" for none). Compare with GLib:
//   gio mime TYPE
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "nordend/core/event_loop.hpp"
#include "nordend/core/worker_pool.hpp"
#include "nordend/fs/entry.hpp"
#include "nordend/fs/mime.hpp"
#include "nordend/platform/applications.hpp"
#include "nordend/platform/desktop_entry.hpp"
#include "nordend/platform/opener.hpp"

namespace {

namespace core = nordend::core;
namespace fs = nordend::fs;
namespace platform = nordend::platform;

constexpr const char* kUsage =
    "usage: nordend-open [--dry-run] FILE...\n"
    "       nordend-open --default TYPE...\n"
    "       nordend-open --types\n";

// A file as a directory listing would give it (the type left to detection).
fs::Entry entry_of(const std::filesystem::path& file) {
    std::error_code error;
    const std::filesystem::file_status status = std::filesystem::status(file, error);
    fs::Entry entry{.name = file.filename().string()};
    if (std::filesystem::is_directory(status)) {
        entry.kind = fs::EntryKind::directory;
    } else if (std::filesystem::is_regular_file(status)) {
        entry.kind = fs::EntryKind::file;
        entry.size = std::filesystem::file_size(file, error);
    } else {
        entry.kind = fs::EntryKind::other;
    }
    entry.is_link = std::filesystem::is_symlink(std::filesystem::symlink_status(file, error));
    return entry;
}

// argv as a shell would need it typed: words with odd characters in single quotes.
std::string shell_line(const std::vector<std::string>& argv) {
    std::string line;
    for (const std::string& arg : argv) {
        line += line.empty() ? "" : " ";
        if (!arg.empty() && arg.find_first_of(" \t\n'\"\\$`*?[]{}()<>|&;#~") == std::string::npos) {
            line += arg;
            continue;
        }
        line += '\'';
        for (const char c : arg) {
            line += c == '\'' ? std::string{"'\\''"} : std::string{c};
        }
        line += '\'';
    }
    return line;
}

int defaults(std::span<char*> types, const platform::Applications& apps,
             const fs::MimeDatabase& database) {
    for (const char* const type : types) {
        const platform::DesktopEntry* const app = apps.default_for(type, database);
        std::printf("%s\t%s\n", type, app != nullptr ? app->id.c_str() : "-");
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    const std::span args{argv + 1, static_cast<std::size_t>(argc > 0 ? argc - 1 : 0)};
    if (args.empty()) {
        std::fputs(kUsage, stderr);
        return 2;
    }
    try {
        const std::string_view first{args.front()};
        const auto database = fs::MimeDatabase::load();
        const platform::Environment environment = platform::Environment::from_environment();
        const auto started = std::chrono::steady_clock::now();
        const platform::Applications apps = platform::Applications::load(environment, *database);
        std::fprintf(
            stderr, "applications: %zu desktop entries in %.2f ms\n", apps.entry_count(),
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started)
                .count());
        if (first == "--types") {
            for (const std::string& type : apps.known_types()) {
                std::printf("%s\n", type.c_str());
            }
            return 0;
        }
        if (first == "--default") {
            return defaults(args.subspan(1), apps, *database);
        }
        const bool dry_run = first == "--dry-run";
        const std::span<char*> files = dry_run ? args.subspan(1) : args;
        if (files.empty() || (!dry_run && first.starts_with("-"))) {
            std::fputs(kUsage, stderr);
            return 2;
        }

        if (dry_run) {
            std::vector<platform::OpenFile> open_files;
            for (const char* const file : files) {
                const std::filesystem::path path = std::filesystem::absolute(file);
                const fs::Entry entry = entry_of(path);
                const std::string_view type =
                    database->name(database->detect(path.parent_path(), entry));
                std::printf("%.*s\t%s\n", static_cast<int>(type.size()), type.data(), path.c_str());
                if (!entry.is_directory()) {
                    open_files.push_back({.path = path, .mime = std::string{type}});
                }
            }
            const platform::OpenPlan plan =
                platform::plan_open(open_files, apps, *database, environment);
            for (const platform::OpenPlan::Launch& launch : plan.launches) {
                std::printf("\n%s (%s), %zu file(s), in %s\n  %s\n", launch.app_name.c_str(),
                            launch.app_id.c_str(), launch.files, launch.directory.c_str(),
                            shell_line(launch.argv).c_str());
            }
            for (const std::string& problem : plan.problems) {
                std::printf("\nproblem: %s\n", problem.c_str());
            }
            return plan.problems.empty() ? 0 : 1;
        }

        // Opens them for real, without an activation token, and waits a moment for quick
        // failures.
        core::EventLoop loop;
        core::WorkerPool pool{loop};
        int status = 0;
        platform::Opener opener{loop, pool, database, environment,
                                [&status](const std::string& message, bool error) {
                                    std::printf("%s\n", message.c_str());
                                    status = error ? 1 : status;
                                }};
        for (const char* const file : files) {
            const std::filesystem::path path = std::filesystem::absolute(file);
            opener.open(path.parent_path(), {entry_of(path)});
        }
        loop.add_timer(platform::Opener::kQuickExit, std::chrono::milliseconds{0},
                       [&loop] { loop.quit(0); });
        loop.run();
        return status;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "nordend-open: %s\n", e.what());
        return 1;
    }
}
