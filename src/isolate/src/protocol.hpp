#pragma once

// What Nordend and its preview helper say to each other, on a private SOCK_SEQPACKET socket
// (one message per packet). Nordend sends a request with the file's descriptor; the helper
// answers with the result and, when there is a picture, a sealed memfd holding its pixels. A
// helper leaving after its idle time first sends a goodbye, so that a request racing with its
// exit is sent again to a new one.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace nordend::isolate::protocol {

// The descriptor the helper finds its socket on.
inline constexpr int kSocketFd = 3;

inline constexpr std::uint32_t kRequestMagic = 0x4E505251;   // "NPRQ"
inline constexpr std::uint32_t kResponseMagic = 0x4E505253;  // "NPRS"
inline constexpr std::uint32_t kGoodbyeMagic = 0x4E504259;   // "NPBY"

inline constexpr std::size_t kMaxDetail = 1024;

// The helper's exit status when it could not build its cage: it never works without one.
inline constexpr int kSandboxFailedExit = 3;

struct Request {
    std::uint32_t magic{kRequestMagic};
    std::uint32_t max_width{0};
    std::uint32_t max_height{0};
};

struct Response {
    std::uint32_t magic{kResponseMagic};
    std::uint8_t failure{0};  // image::Failure
    std::uint8_t from_preview{0};
    std::uint16_t detail_length{0};  // bytes of detail after this header, at most kMaxDetail
    std::uint32_t width{0};          // the picture in the memfd: width x height RGBA
    std::uint32_t height{0};
    std::uint32_t full_width{0};
    std::uint32_t full_height{0};
};

// A packet received: its bytes and the descriptor that came with it (-1 when none). The
// descriptor is the receiver's to close.
struct Packet {
    std::array<std::byte, sizeof(Response) + kMaxDetail> bytes{};
    std::size_t length{0};
    int fd{-1};
};

// Sends one packet with an optional descriptor (fd < 0: none). False on an error (errno set);
// never raises SIGPIPE.
[[nodiscard]] bool send_packet(int socket, std::span<const std::byte> bytes, int fd);

// Receives one packet; nullopt at the end of the stream (the other side closed) or on an error
// (errno set; 0 at the end). Descriptors beyond the first are closed.
[[nodiscard]] std::optional<Packet> receive_packet(int socket);

}  // namespace nordend::isolate::protocol
