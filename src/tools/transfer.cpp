// Copy and move engine from the command line (ROADMAP 6.3). Not installed.
//
//   nordend-transfer [--move] [--force-copy] SOURCE_DIR DESTINATION_DIR [NAME...]
//
// Copies (or moves) NAMEs (default: everything in SOURCE_DIR) as Ctrl+V does, conflicts
// skipped, and prints files, bytes, time and rate. Compare with: time cp -a SOURCE_DIR/. DEST.
#include "nordend/fs/transfer.hpp"

#include <chrono>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "nordend/core/worker_pool.hpp"

namespace fs = nordend::fs;

int main(int argc, char** argv) {
    const std::span args{argv + 1, static_cast<std::size_t>(argc > 0 ? argc - 1 : 0)};
    fs::TransferRequest request;
    std::vector<std::string> positional;
    for (const char* const arg : args) {
        const std::string_view option{arg};
        if (option == "--move") {
            request.kind = fs::TransferKind::move;
        } else if (option == "--force-copy") {
            request.force_copy = true;
        } else {
            positional.emplace_back(arg);
        }
    }
    if (positional.size() < 2) {
        std::fputs(
            "usage: nordend-transfer [--move] [--force-copy] SOURCE_DIR DESTINATION_DIR "
            "[NAME...]\n",
            stderr);
        return 2;
    }
    try {
        request.source = positional[0];
        request.destination = positional[1];
        request.names.assign(positional.begin() + 2, positional.end());
        if (request.names.empty()) {
            for (const auto& entry : std::filesystem::directory_iterator{request.source}) {
                request.names.push_back(entry.path().filename().string());
            }
        }
        const auto start = std::chrono::steady_clock::now();
        const fs::TransferResult result =
            fs::run_transfer(request, nordend::core::CancelToken{},
                             {.progress = nullptr,
                              .resolve = [](const fs::Conflict&) { return fs::Resolution::skip; }});
        const double seconds =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        if (!result.refused.empty()) {
            std::fprintf(stderr, "refused: %s\n", result.refused.c_str());
            return 1;
        }
        for (const std::string& error : result.errors) {
            std::fprintf(stderr, "error: %s\n", error.c_str());
        }
        std::printf("%zu files, %.1f MB in %.3f s: %.0f MB/s\n", result.files_done,
                    static_cast<double>(result.bytes_done) / 1e6, seconds,
                    static_cast<double>(result.bytes_done) / 1e6 / seconds);
        return result.errors.empty() ? 0 : 1;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "nordend-transfer: %s\n", e.what());
        return 1;
    }
}
