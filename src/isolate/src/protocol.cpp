#include "protocol.hpp"

#include <cerrno>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>

namespace nordend::isolate::protocol {

bool send_packet(int socket, std::span<const std::byte> bytes, int fd) {
    iovec part{.iov_base = const_cast<std::byte*>(bytes.data()),  // NOLINT: sendmsg's iovec
               .iov_len = bytes.size()};
    msghdr message{};
    message.msg_iov = &part;
    message.msg_iovlen = 1;
    alignas(cmsghdr) std::array<char, CMSG_SPACE(sizeof(int))> control{};
    if (fd >= 0) {
        message.msg_control = control.data();
        message.msg_controllen = control.size();
        cmsghdr* header = CMSG_FIRSTHDR(&message);
        header->cmsg_level = SOL_SOCKET;
        header->cmsg_type = SCM_RIGHTS;
        header->cmsg_len = CMSG_LEN(sizeof(int));
        std::memcpy(CMSG_DATA(header), &fd, sizeof(int));
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
    alignas(cmsghdr) std::array<char, CMSG_SPACE(sizeof(int) * 4)> control{};
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
            if (packet.fd < 0) {
                packet.fd = fd;
            } else {
                ::close(fd);
            }
        }
    }
    if (got <= 0 || (message.msg_flags & (MSG_TRUNC | MSG_CTRUNC)) != 0) {
        if (packet.fd >= 0) {
            ::close(packet.fd);
        }
        if (got == 0) {
            errno = 0;
        }
        return std::nullopt;
    }
    packet.length = static_cast<std::size_t>(got);
    return packet;
}

}  // namespace nordend::isolate::protocol
