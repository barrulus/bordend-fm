#include "fd_file.hpp"

#include <algorithm>
#include <cerrno>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace nordend::image::detail {

std::optional<FdFile> FdFile::of(int fd) {
    struct stat info{};
    if (fd < 0 || ::fstat(fd, &info) != 0 || info.st_size < 0) {
        return std::nullopt;
    }
    return FdFile{fd, static_cast<std::uint64_t>(info.st_size)};
}

std::optional<std::size_t> FdFile::read_into(std::uint64_t offset,
                                             std::span<std::uint8_t> to) const {
    if (offset >= size_) {
        return 0;
    }
    const auto length =
        static_cast<std::size_t>(std::min<std::uint64_t>(to.size(), size_ - offset));
    std::size_t done = 0;
    while (done < length) {
        const ssize_t got =
            ::pread(fd_, to.data() + done, length - done, static_cast<off_t>(offset + done));
        if (got < 0 && errno == EINTR) {
            continue;
        }
        if (got < 0) {
            return std::nullopt;
        }
        if (got == 0) {
            break;  // shorter than fstat said: it shrank meanwhile
        }
        done += static_cast<std::size_t>(got);
    }
    return done;
}

std::vector<std::uint8_t> FdFile::read(std::uint64_t offset, std::size_t length) const {
    if (offset >= size_) {
        return {};
    }
    std::vector<std::uint8_t> data(
        static_cast<std::size_t>(std::min<std::uint64_t>(length, size_ - offset)));
    const std::optional<std::size_t> got = read_into(offset, data);
    if (!got) {
        return {};
    }
    data.resize(*got);
    return data;
}

std::vector<std::uint8_t> FdFile::read_exactly(std::uint64_t offset, std::size_t length) const {
    std::vector<std::uint8_t> data = read(offset, length);
    if (data.size() != length) {
        return {};
    }
    return data;
}

FdIo::~FdIo() { munmap(); }

int FdIo::open() {
    position_ = 0;
    eof_ = false;
    error_ = 0;
    return 0;
}

int FdIo::close() {
    munmap();
    return 0;
}

size_t FdIo::write(const Exiv2::byte* /*data*/, size_t /*count*/) { return 0; }

size_t FdIo::write(Exiv2::BasicIo& /*source*/) { return 0; }

int FdIo::putb(Exiv2::byte /*data*/) { return EOF; }

Exiv2::DataBuf FdIo::read(size_t count) {
    Exiv2::DataBuf buffer{count};
    const size_t got = read(buffer.data(), count);
    buffer.resize(got);
    return buffer;
}

size_t FdIo::read(Exiv2::byte* buffer, size_t count) {
    if (position_ >= file_.size()) {
        eof_ = true;
        return 0;
    }
    const std::optional<std::size_t> got = file_.read_into(position_, {buffer, count});
    if (!got) {
        error_ = 1;
        return 0;
    }
    position_ += *got;
    eof_ = *got < count;
    return *got;
}

int FdIo::getb() {
    Exiv2::byte byte = 0;
    return read(&byte, 1) == 1 ? byte : EOF;
}

void FdIo::transfer(Exiv2::BasicIo& /*source*/) { error_ = 1; }

int FdIo::seek(int64_t offset, Position from) {
    std::int64_t base = 0;
    if (from == cur) {
        base = static_cast<std::int64_t>(position_);
    } else if (from == end) {
        base = static_cast<std::int64_t>(file_.size());
    }
    const std::int64_t target = base + offset;
    if (target < 0) {
        return 1;
    }
    position_ = static_cast<std::uint64_t>(target);
    eof_ = position_ > file_.size();
    return 0;
}

Exiv2::byte* FdIo::mmap(bool writable) {
    if (writable || file_.size() == 0) {
        return nullptr;
    }
    if (mapping_ == nullptr) {
        void* mapped = ::mmap(nullptr, file_.size(), PROT_READ, MAP_PRIVATE, file_.fd(), 0);
        if (mapped == MAP_FAILED) {
            error_ = 1;
            return nullptr;
        }
        mapping_ = mapped;
    }
    return static_cast<Exiv2::byte*>(mapping_);
}

int FdIo::munmap() {
    if (mapping_ != nullptr) {
        ::munmap(mapping_, file_.size());
        mapping_ = nullptr;
    }
    return 0;
}

size_t FdIo::tell() const { return static_cast<size_t>(position_); }

size_t FdIo::size() const { return static_cast<size_t>(file_.size()); }

bool FdIo::isopen() const { return true; }

int FdIo::error() const { return error_; }

bool FdIo::eof() const { return eof_; }

const std::string& FdIo::path() const noexcept {
    static const std::string kName{"(descriptor)"};
    return kName;
}

}  // namespace nordend::image::detail
