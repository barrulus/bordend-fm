#include "nordend/isolate/helpers.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <system_error>
#include <unistd.h>
#include <utility>

#include "protocol.hpp"

extern char** environ;  // NOLINT(readability-redundant-declaration): POSIX, not in a header

namespace nordend::isolate {
namespace {

image::Loaded failed(image::Failure failure, std::string detail) {
    image::Loaded result;
    result.failure = failure;
    result.detail = std::move(detail);
    return result;
}

std::string errno_text() { return std::error_code{errno, std::generic_category()}.message(); }

// The picture in a helper's memfd, checked against the size it said.
std::optional<image::Image> read_pixels(int fd, std::uint32_t width, std::uint32_t height) {
    const std::uint64_t bytes = std::uint64_t{width} * height * 4;
    struct stat info{};
    if (width == 0 || height == 0 || ::fstat(fd, &info) != 0 ||
        std::cmp_not_equal(info.st_size, bytes)) {
        return std::nullopt;
    }
    // Sealed against shrinking: the mapping cannot lose pages under us.
    const int seals = ::fcntl(fd, F_GET_SEALS);
    if (seals < 0 || (seals & F_SEAL_SHRINK) == 0) {
        return std::nullopt;
    }
    void* mapped = ::mmap(nullptr, bytes, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mapped == MAP_FAILED) {
        return std::nullopt;
    }
    image::Image picture{.width = width, .height = height, .rgba = {}};
    const auto* from = static_cast<const std::uint8_t*>(mapped);
    picture.rgba.assign(from, from + bytes);
    ::munmap(mapped, bytes);
    return picture;
}

}  // namespace

namespace {

// Closes a helper's socket (it leaves at its next read), makes sure it is gone, and waits for it:
// nothing left behind, not even a zombie.
// Returns its wait status.
int end(pid_t pid, int socket) {
    ::close(socket);
    // A helper that has already left keeps its exit status: killing a zombie changes nothing.
    ::kill(pid, SIGKILL);
    int status = 0;
    while (::waitpid(pid, &status, 0) < 0 && errno == EINTR) {
    }
    return status;
}

}  // namespace

Helpers::Helpers(HelperOptions options)
    : options_{std::move(options)}, reaper_{[this] { close_idle(); }} {
#if defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
    // Sanitizer builds (tests, snapshots) decode several times slower.
    options_.timeout *= 5;
#endif
}

Helpers::~Helpers() {
    {
        const std::scoped_lock lock{mutex_};
        stopping_ = true;
    }
    idle_changed_.notify_all();
    reaper_.join();
    const std::scoped_lock lock{mutex_};
    for (const Helper& helper : idle_) {
        end(helper.pid, helper.socket);
    }
}

void Helpers::close_idle() {
    std::unique_lock lock{mutex_};
    while (!stopping_) {
        if (idle_.empty()) {
            idle_changed_.wait(lock);
            continue;
        }
        const auto oldest = std::ranges::min_element(idle_, {}, &Helper::idle_since);
        const auto deadline = oldest->idle_since + options_.idle;
        if (std::chrono::steady_clock::now() < deadline) {
            idle_changed_.wait_until(lock, deadline);
            continue;
        }
        const Helper expired = *oldest;
        idle_.erase(oldest);
        --count_;
        lock.unlock();
        end(expired.pid, expired.socket);
        freed_.notify_one();
        lock.lock();
    }
}

std::size_t Helpers::running() const {
    const std::scoped_lock lock{mutex_};
    return count_;
}

std::optional<Helpers::Helper> Helpers::spawn() const {
    if (options_.program.empty()) {
        return std::nullopt;
    }
    std::array<int, 2> ends{-1, -1};
    if (::socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, ends.data()) != 0) {
        return std::nullopt;
    }
    posix_spawn_file_actions_t actions{};
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
    posix_spawn_file_actions_adddup2(&actions, ends[1], protocol::kSocketFd);
    posix_spawn_file_actions_addclosefrom_np(&actions, protocol::kSocketFd + 1);
    posix_spawnattr_t attr{};
    posix_spawnattr_init(&attr);
    sigset_t none{};
    sigemptyset(&none);
    sigset_t all{};
    sigfillset(&all);
    posix_spawnattr_setsigmask(&attr, &none);
    posix_spawnattr_setsigdefault(&attr, &all);
    posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETSIGMASK | POSIX_SPAWN_SETSIGDEF);
    // Nordend closes idle helpers itself; a helper's own idle time, longer, is only a fallback.
    std::vector<std::string> args{options_.program.string(), "--idle-ms",
                                  std::to_string((options_.idle.count() * 2) + 1000)};
    args.insert(args.end(), options_.args.begin(), options_.args.end());
    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (std::string& arg : args) {
        argv.push_back(arg.data());
    }
    argv.push_back(nullptr);
    pid_t pid = -1;
    const int spawned =
        ::posix_spawn(&pid, args.front().c_str(), &actions, &attr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    posix_spawnattr_destroy(&attr);
    ::close(ends[1]);
    if (spawned != 0) {
        ::close(ends[0]);
        return std::nullopt;
    }
    return Helper{.pid = pid, .socket = ends[0], .idle_since = {}};
}

std::optional<Helpers::Helper> Helpers::take() {
    std::unique_lock lock{mutex_};
    while (true) {
        // Idle ones that left meanwhile (their idle time) are let go of here.
        std::erase_if(idle_, [this](const Helper& helper) {
            int status = 0;
            if (::waitpid(helper.pid, &status, WNOHANG) != helper.pid) {
                return false;
            }
            ::close(helper.socket);
            --count_;
            return true;
        });
        if (!idle_.empty()) {
            const Helper helper = idle_.back();
            idle_.pop_back();
            return helper;
        }
        if (count_ < options_.max_helpers) {
            ++count_;
            lock.unlock();
            std::optional<Helper> started = spawn();
            if (!started) {
                lock.lock();
                --count_;
                freed_.notify_one();
            }
            return started;
        }
        freed_.wait(lock);
    }
}

void Helpers::give_back(Helper helper) {
    helper.idle_since = std::chrono::steady_clock::now();
    {
        const std::scoped_lock lock{mutex_};
        idle_.push_back(helper);
    }
    freed_.notify_one();
    idle_changed_.notify_one();
}

int Helpers::discard(Helper helper) {
    const int status = end(helper.pid, helper.socket);
    {
        const std::scoped_lock lock{mutex_};
        --count_;
    }
    freed_.notify_one();
    return status;
}

Helpers::Outcome Helpers::ask(Helper helper, int file, const image::LoadRequest& request,
                              image::Loaded& out) const {
    const protocol::Request message{.magic = protocol::kRequestMagic,
                                    .max_width = request.max_width,
                                    .max_height = request.max_height};
    if (!protocol::send_packet(helper.socket, std::as_bytes(std::span{&message, 1}), file)) {
        return Outcome::again;  // it had already left
    }
    pollfd watched{.fd = helper.socket, .events = POLLIN, .revents = 0};
    const auto deadline = std::chrono::steady_clock::now() + options_.timeout;
    int ready = 0;
    while (true) {
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now());
        ready = ::poll(&watched, 1, static_cast<int>(std::max<std::int64_t>(left.count(), 0)));
        if (ready < 0 && errno == EINTR) {
            continue;
        }
        break;
    }
    if (ready == 0) {
        out = failed(image::Failure::unavailable, "the preview helper took longer than " +
                                                      std::to_string(options_.timeout.count()) +
                                                      " ms");
        return Outcome::failed;
    }
    const std::optional<protocol::Packet> packet = protocol::receive_packet(helper.socket);
    if (!packet) {
        out = failed(image::Failure::unavailable, "the preview helper stopped");
        return Outcome::failed;
    }
    std::uint32_t magic = 0;
    if (packet->length >= sizeof(magic)) {
        std::memcpy(&magic, packet->bytes.data(), sizeof(magic));
    }
    if (magic == protocol::kGoodbyeMagic) {
        if (packet->fd >= 0) {
            ::close(packet->fd);
        }
        return Outcome::again;  // it was leaving when the request came
    }
    protocol::Response response;
    const bool well_formed =
        magic == protocol::kResponseMagic && packet->length >= sizeof(response) && [&] {
            std::memcpy(&response, packet->bytes.data(), sizeof(response));
            return packet->length == sizeof(response) + response.detail_length &&
                   response.failure <= static_cast<std::uint8_t>(image::Failure::unavailable);
        }();
    if (!well_formed) {
        if (packet->fd >= 0) {
            ::close(packet->fd);
        }
        out = failed(image::Failure::unavailable, "the preview helper said something else");
        return Outcome::failed;
    }
    out = {};
    out.failure = static_cast<image::Failure>(response.failure);
    out.from_preview = response.from_preview != 0;
    out.full_width = response.full_width;
    out.full_height = response.full_height;
    out.detail.assign(reinterpret_cast<const char*>(  // NOLINT: bytes to chars
                          packet->bytes.data() + sizeof(response)),
                      response.detail_length);
    if (out.failure == image::Failure::none) {
        std::optional<image::Image> picture =
            packet->fd >= 0 ? read_pixels(packet->fd, response.width, response.height)
                            : std::nullopt;
        if (!picture) {
            if (packet->fd >= 0) {
                ::close(packet->fd);
            }
            out = failed(image::Failure::unavailable, "the preview helper's pixels are wrong");
            return Outcome::failed;
        }
        out.image = std::move(*picture);
    }
    if (packet->fd >= 0) {
        ::close(packet->fd);
    }
    return Outcome::done;
}

image::Loaded Helpers::load(const image::LoadRequest& request) {
    // Opened here, read there: the helper gets the descriptor, never the path. O_NONBLOCK so
    // that a FIFO named like a photo cannot hang the worker.
    const int file = ::open(request.path.c_str(), O_RDONLY | O_CLOEXEC | O_NOCTTY | O_NONBLOCK);
    if (file < 0) {
        return failed(image::Failure::unreadable, errno_text());
    }
    struct stat info{};
    if (::fstat(file, &info) != 0 || !S_ISREG(info.st_mode)) {
        ::close(file);
        return failed(image::Failure::unreadable, "not a regular file");
    }
    image::Loaded result =
        failed(image::Failure::unavailable, "the preview helper could not start");
    // Twice at most: a helper that had just left gets the request again in a new one.
    for (int attempt = 0; attempt < 2; ++attempt) {
        const std::optional<Helper> helper = take();
        if (!helper) {
            result = failed(image::Failure::unavailable,
                            options_.program.empty() ? "nordend-preview not found"
                                                     : "the preview helper could not start: " +
                                                           options_.program.string());
            break;
        }
        const Outcome outcome = ask(*helper, file, request, result);
        if (outcome == Outcome::done) {
            give_back(*helper);
            break;
        }
        const int status = discard(*helper);
        if (WIFEXITED(status) && WEXITSTATUS(status) == protocol::kSandboxFailedExit) {
            result = failed(image::Failure::unavailable,
                            "the preview helper could not set up its sandbox");
            break;
        }
        if (outcome == Outcome::failed) {
            break;
        }
    }
    ::close(file);
    return result;
}

std::filesystem::path default_program() {
    std::error_code error;
    const std::filesystem::path self = std::filesystem::read_symlink("/proc/self/exe", error);
    if (error) {
        return {};
    }
    const std::filesystem::path here = self.parent_path();
    for (const std::filesystem::path& candidate :
         {here / "nordend-preview",
          here.parent_path() / "libexec" / "nordend" / "nordend-preview"}) {
        if (::access(candidate.c_str(), X_OK) == 0) {
            return candidate;
        }
    }
    return {};
}

}  // namespace nordend::isolate
