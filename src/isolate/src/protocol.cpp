#include "protocol.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <limits>
#include <sys/socket.h>
#include <unistd.h>

namespace nordend::isolate::protocol {

void Packet::close_fds() {
    for (int& fd : fds) {
        if (fd >= 0) {
            ::close(fd);
            fd = -1;
        }
    }
}

bool send_packet(int socket, std::span<const std::byte> bytes, std::span<const int> fds) {
    iovec part{.iov_base = const_cast<std::byte*>(bytes.data()),  // NOLINT: sendmsg's iovec
               .iov_len = bytes.size()};
    msghdr message{};
    message.msg_iov = &part;
    message.msg_iovlen = 1;
    alignas(cmsghdr) std::array<char, CMSG_SPACE(sizeof(int) * kMaxFds)> control{};
    if (!fds.empty()) {
        if (fds.size() > kMaxFds) {
            errno = EINVAL;
            return false;
        }
        message.msg_control = control.data();
        message.msg_controllen = CMSG_SPACE(sizeof(int) * fds.size());
        cmsghdr* header = CMSG_FIRSTHDR(&message);
        header->cmsg_level = SOL_SOCKET;
        header->cmsg_type = SCM_RIGHTS;
        header->cmsg_len = CMSG_LEN(sizeof(int) * fds.size());
        std::memcpy(CMSG_DATA(header), fds.data(), sizeof(int) * fds.size());
    }
    while (true) {
        const ssize_t sent = ::sendmsg(socket, &message, MSG_NOSIGNAL);
        if (sent < 0 && errno == EINTR) {
            continue;
        }
        return sent >= 0 && static_cast<std::size_t>(sent) == bytes.size();
    }
}

std::optional<Packet> receive_packet(int socket) {
    Packet packet;
    iovec part{.iov_base = packet.bytes.data(), .iov_len = packet.bytes.size()};
    msghdr message{};
    message.msg_iov = &part;
    message.msg_iovlen = 1;
    alignas(cmsghdr) std::array<char, CMSG_SPACE(sizeof(int) * (kMaxFds + 2))> control{};
    message.msg_control = control.data();
    message.msg_controllen = control.size();
    ssize_t got = ::recvmsg(socket, &message, MSG_CMSG_CLOEXEC);
    while (got < 0 && errno == EINTR) {
        got = ::recvmsg(socket, &message, MSG_CMSG_CLOEXEC);
    }
    // The descriptors first: they must be closed whatever else happens.
    for (cmsghdr* header = CMSG_FIRSTHDR(&message); header != nullptr;
         header = CMSG_NXTHDR(&message, header)) {
        if (header->cmsg_level != SOL_SOCKET || header->cmsg_type != SCM_RIGHTS) {
            continue;
        }
        const std::size_t count = (header->cmsg_len - CMSG_LEN(0)) / sizeof(int);
        for (std::size_t i = 0; i < count; ++i) {
            int fd = -1;
            std::memcpy(&fd, CMSG_DATA(header) + (i * sizeof(int)), sizeof(int));
            auto* const free_slot = std::ranges::find(packet.fds, -1);
            if (free_slot != packet.fds.end()) {
                *free_slot = fd;
            } else {
                ::close(fd);
            }
        }
    }
    if (got <= 0 || (message.msg_flags & (MSG_TRUNC | MSG_CTRUNC)) != 0) {
        packet.close_fds();
        if (got == 0) {
            errno = 0;
        }
        return std::nullopt;
    }
    packet.length = static_cast<std::size_t>(got);
    return packet;
}

namespace {

// The text fields of a PhotoInfo, in the order they travel.
constexpr std::array kFields{
    &picture::PhotoInfo::camera,   &picture::PhotoInfo::lens,    &picture::PhotoInfo::exposure_time,
    &picture::PhotoInfo::aperture, &picture::PhotoInfo::iso,     &picture::PhotoInfo::focal_length,
    &picture::PhotoInfo::taken,    &picture::PhotoInfo::program, &picture::PhotoInfo::exposure_bias,
    &picture::PhotoInfo::metering, &picture::PhotoInfo::flash,   &picture::PhotoInfo::white_balance,
    &picture::PhotoInfo::software,
};

struct InfoHeader {
    std::uint32_t magic{kInfoMagic};
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::int8_t rating{0};
    std::uint8_t has_rating{0};
    std::array<std::uint8_t, kFields.size()> lengths{};
};

}  // namespace

std::vector<std::byte> encode_info(const picture::PhotoInfo& info) {
    InfoHeader header;
    header.width = info.width;
    header.height = info.height;
    header.has_rating = info.rating ? 1 : 0;
    header.rating = static_cast<std::int8_t>(info.rating.value_or(0));
    std::vector<std::byte> out(sizeof(header));
    for (std::size_t i = 0; i < kFields.size(); ++i) {
        const std::string& text = info.*kFields.at(i);
        const std::size_t length = std::min(text.size(), kMaxField);
        header.lengths.at(i) = static_cast<std::uint8_t>(length);
        const auto* bytes = reinterpret_cast<const std::byte*>(text.data());  // NOLINT: bytes
        out.insert(out.end(), bytes, bytes + length);
    }
    std::memcpy(out.data(), &header, sizeof(header));
    return out;
}

std::optional<picture::PhotoInfo> decode_info(std::span<const std::byte> bytes) {
    InfoHeader header;
    if (bytes.size() < sizeof(header)) {
        return std::nullopt;
    }
    std::memcpy(&header, bytes.data(), sizeof(header));
    if (header.magic != kInfoMagic || header.has_rating > 1) {
        return std::nullopt;
    }
    std::size_t at = sizeof(header);
    picture::PhotoInfo info;
    info.width = header.width;
    info.height = header.height;
    if (header.has_rating == 1) {
        info.rating = header.rating;
    }
    for (std::size_t i = 0; i < kFields.size(); ++i) {
        const std::size_t length = header.lengths.at(i);
        if (bytes.size() - at < length) {
            return std::nullopt;
        }
        (info.*kFields.at(i))
            .assign(reinterpret_cast<const char*>(bytes.data() + at), length);  // NOLINT: bytes
        at += length;
    }
    if (at != bytes.size()) {
        return std::nullopt;
    }
    return info;
}

std::vector<std::byte> encode_text_request(const text::Limits& limits, std::string_view name,
                                           std::string_view mime) {
    name = name.substr(0, kMaxField);
    mime = mime.substr(0, kMaxField);
    const TextRequest request{
        .magic = kRequestMagic,
        .kind = Kind::text,
        .max_bytes = static_cast<std::uint32_t>(std::min(limits.max_bytes, kMaxTextBytes)),
        .max_line_bytes =
            static_cast<std::uint32_t>(std::min(limits.max_line_bytes, kMaxTextLineBytes)),
        .max_lines = static_cast<std::uint32_t>(
            std::min<std::size_t>(limits.max_lines, std::numeric_limits<std::uint32_t>::max())),
        .tab_width = static_cast<std::uint16_t>(std::min(limits.tab_width, kMaxTabWidth)),
        .name_length = static_cast<std::uint8_t>(name.size()),
        .mime_length = static_cast<std::uint8_t>(mime.size())};
    std::vector<std::byte> bytes(sizeof(request) + name.size() + mime.size());
    std::memcpy(bytes.data(), &request, sizeof(request));
    std::memcpy(bytes.data() + sizeof(request), name.data(), name.size());
    std::memcpy(bytes.data() + sizeof(request) + name.size(), mime.data(), mime.size());
    return bytes;
}

std::optional<TextAsk> decode_text_request(std::span<const std::byte> bytes) {
    TextRequest request;
    if (bytes.size() < sizeof(request)) {
        return std::nullopt;
    }
    std::memcpy(&request, bytes.data(), sizeof(request));
    if (request.magic != kRequestMagic || request.kind != Kind::text ||
        bytes.size() != sizeof(request) + request.name_length + request.mime_length ||
        request.max_bytes == 0 || request.max_bytes > kMaxTextBytes ||
        request.max_line_bytes == 0 || request.max_line_bytes > kMaxTextLineBytes ||
        request.tab_width == 0 || request.tab_width > kMaxTabWidth) {
        return std::nullopt;
    }
    const auto* chars = reinterpret_cast<const char*>(bytes.data());  // NOLINT: bytes
    TextAsk ask;
    ask.limits = {.max_bytes = request.max_bytes,
                  .max_line_bytes = request.max_line_bytes,
                  .max_lines = request.max_lines,
                  .tab_width = request.tab_width};
    ask.name.assign(chars + sizeof(request), request.name_length);
    ask.mime.assign(chars + sizeof(request) + request.name_length, request.mime_length);
    return ask;
}

}  // namespace nordend::isolate::protocol
