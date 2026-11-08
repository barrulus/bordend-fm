#pragma once

// What Nordend and its preview helper say to each other, on a private SOCK_SEQPACKET socket
// (one message per packet). Nordend sends a request with the file's descriptor; the helper
// answers a picture with the result and a sealed memfd holding its pixels, or the information as
// text fields. A
// helper leaving after its idle time first sends a goodbye, so that a request racing with its
// exit is sent again to a new one.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "nordend/picture/photo_info.hpp"

namespace nordend::isolate::protocol {

// The descriptor the helper finds its socket on.
inline constexpr int kSocketFd = 3;

inline constexpr std::uint32_t kRequestMagic = 0x4E505251;   // "NPRQ"
inline constexpr std::uint32_t kResponseMagic = 0x4E505253;  // "NPRS"
inline constexpr std::uint32_t kGoodbyeMagic = 0x4E504259;   // "NPBY"
inline constexpr std::uint32_t kInfoMagic = 0x4E50494E;      // "NPIN"

inline constexpr std::size_t kMaxDetail = 1024;
inline constexpr std::size_t kMaxPacket = 8192;
inline constexpr std::size_t kMaxField = 255;  // bytes of one information field, at most

// The helper's exit status when it could not build its cage: it never works without one.
inline constexpr int kSandboxFailedExit = 3;

// What Nordend asks for.
// Four bytes, for a request laid out without padding.
enum class Kind : std::uint32_t {  // NOLINT(performance-enum-size)
    picture = 0,                   // the file's picture, max_width x max_height at most
    photo_info = 1,                // what the file says about itself (EXIF)
};

struct Request {
    std::uint32_t magic{kRequestMagic};
    Kind kind{Kind::picture};
    std::uint32_t max_width{0};
    std::uint32_t max_height{0};
};

struct Response {
    std::uint32_t magic{kResponseMagic};
    std::uint8_t failure{0};  // picture::Failure
    std::uint8_t from_preview{0};
    std::uint16_t detail_length{0};  // bytes of detail after this header, at most kMaxDetail
    std::uint32_t width{0};          // the picture in the memfd: width x height RGBA
    std::uint32_t height{0};
    std::uint32_t full_width{0};
    std::uint32_t full_height{0};
};

inline constexpr std::size_t kMaxFds = 2;

// A packet received: its bytes and the descriptors that came with it (-1 where none). The
// descriptors are the receiver's to close (close_fds).
struct Packet {
    std::array<std::byte, kMaxPacket> bytes{};
    std::size_t length{0};
    std::array<int, kMaxFds> fds{-1, -1};

    void close_fds();
};

// Sends one packet with up to kMaxFds descriptors. False on an error (errno set); never raises
// SIGPIPE.
[[nodiscard]] bool send_packet(int socket, std::span<const std::byte> bytes,
                               std::span<const int> fds = {});

// Receives one packet; nullopt at the end of the stream (the other side closed) or on an error
// (errno set; 0 at the end). Descriptors beyond kMaxFds are closed.
[[nodiscard]] std::optional<Packet> receive_packet(int socket);

// A photo's information as one packet (kInfoMagic, the numbers, then each text field, cut at
// kMaxField bytes), and back; nullopt when the bytes are not that.
[[nodiscard]] std::vector<std::byte> encode_info(const picture::PhotoInfo& info);
[[nodiscard]] std::optional<picture::PhotoInfo> decode_info(std::span<const std::byte> bytes);

}  // namespace nordend::isolate::protocol
