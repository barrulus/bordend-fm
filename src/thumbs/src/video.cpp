#include "nordend/thumbs/video.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

#include "nordend/picture/wuffs.hpp"

extern char** environ;

namespace nordend::thumbs {

picture::Loaded load_video(const picture::LoadRequest& request, const core::CancelToken& token) {
    picture::Loaded result;
    result.failure = picture::Failure::unavailable;
    result.detail = "video thumbnailer unavailable";
    std::error_code file_error;
    if (token.cancelled() || !request.path.is_absolute() ||
        !std::filesystem::is_regular_file(request.path, file_error)) {
        return result;
    }
    std::array<int, 2> pipe{};
    if (::pipe2(pipe.data(), O_CLOEXEC) != 0) {
        return result;
    }
    posix_spawn_file_actions_t actions{};
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, pipe[1], STDOUT_FILENO);
    posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
    posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_addclose(&actions, pipe[0]);
    posix_spawn_file_actions_addclose(&actions, pipe[1]);
    const auto side = std::clamp(std::max(request.max_width, request.max_height), 1U, 1024U);
    const std::string filter = "scale=" + std::to_string(side) + ":" + std::to_string(side) +
                               ":force_original_aspect_ratio=decrease:reset_sar=1,thumbnail=30";
    std::vector<std::string> arguments{NORDEND_VIDEO_THUMBNAILER,
                                       "-nostdin",
                                       "-v",
                                       "error",
                                       "-threads",
                                       "1",
                                       "-filter_threads",
                                       "1",
                                       "-protocol_whitelist",
                                       "file,pipe",
                                       "-i",
                                       request.path.string(),
                                       "-map",
                                       "0:v:0",
                                       "-an",
                                       "-sn",
                                       "-dn",
                                       "-vf",
                                       filter,
                                       "-frames:v",
                                       "1",
                                       "-threads",
                                       "1",
                                       "-c:v",
                                       "png",
                                       "-f",
                                       "image2pipe",
                                       "pipe:1"};
    std::vector<char*> argv(arguments.size() + 1, nullptr);
    for (std::size_t i = 0; i < arguments.size(); ++i) {
        argv[i] = arguments[i].data();
    }
    pid_t child = -1;
    const int spawned = ::posix_spawn(&child, argv[0], &actions, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    ::close(pipe[1]);
    if (spawned != 0) {
        ::close(pipe[0]);
        return result;
    }
    ::fcntl(pipe[0], F_SETFL, O_NONBLOCK);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{15};
    std::vector<std::uint8_t> png;
    std::array<std::uint8_t, 16384> buffer{};
    int status = 0;
    bool finished = false;
    bool eof = false;
    bool aborted = false;
    while (!finished || !eof) {
        if (token.cancelled() || std::chrono::steady_clock::now() >= deadline ||
            png.size() > 8U * 1024U * 1024U) {
            aborted = true;
            break;
        }
        pollfd descriptor{pipe[0], POLLIN, 0};
        ::poll(&descriptor, 1, 20);
        const ssize_t count = ::read(pipe[0], buffer.data(), buffer.size());
        if (count > 0) {
            png.insert(png.end(), buffer.begin(), buffer.begin() + count);
        } else if (count == 0) {
            eof = true;
        } else if (errno != EINTR && errno != EAGAIN) {
            aborted = true;
            break;
        }
        if (!finished) {
            const pid_t waited = ::waitpid(child, &status, WNOHANG);
            finished = waited == child;
            if (waited < 0 && errno != EINTR) {
                aborted = true;
                break;
            }
        }
    }
    ::close(pipe[0]);
    if (!finished) {
        ::kill(child, SIGKILL);
        while (::waitpid(child, &status, 0) < 0 && errno == EINTR) {
        }
    }
    if (aborted) {
        result.detail = "video thumbnail cancelled, too large or timed out";
        return result;
    }
    result.failure = picture::Failure::damaged;
    result.detail = "video thumbnailer could not decode a frame";
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        return result;
    }
    auto decoded = picture::decode_wuffs(png, std::uint64_t{side} * side);
    if (decoded.image.empty()) {
        result.detail = decoded.error;
        return result;
    }
    result.image = std::move(decoded.image);
    result.failure = picture::Failure::none;
    result.detail.clear();
    return result;
}

}  // namespace nordend::thumbs
