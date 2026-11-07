#pragma once

// A file read through a descriptor it was opened as, with pread: the isolated helper gets files
// this way (it may not open any), and the same code serves Nordend's own reads.

#include <cstddef>
#include <cstdint>
#include <exiv2/basicio.hpp>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace nordend::image::detail {

class FdFile {
public:
    // fd stays the caller's: never closed here. nullopt when fstat fails.
    [[nodiscard]] static std::optional<FdFile> of(int fd);

    [[nodiscard]] int fd() const { return fd_; }

    [[nodiscard]] std::uint64_t size() const { return size_; }

    // Up to length bytes from offset (fewer at the end of the file); empty on an error.
    [[nodiscard]] std::vector<std::uint8_t> read(std::uint64_t offset, std::size_t length) const;

    // Reads into to from offset; how many bytes (fewer at the end of the file), or nullopt on an
    // error.
    [[nodiscard]] std::optional<std::size_t> read_into(std::uint64_t offset,
                                                       std::span<std::uint8_t> to) const;

    // Exactly length bytes from offset, or empty.
    [[nodiscard]] std::vector<std::uint8_t> read_exactly(std::uint64_t offset,
                                                         std::size_t length) const;

private:
    FdFile(int fd, std::uint64_t size) : fd_{fd}, size_{size} {}

    int fd_;
    std::uint64_t size_;
};

// exiv2's input over a descriptor: reads with pread at its own position, maps the file when
// exiv2 asks for a mapping (inside the helper a file cut short while mapped kills only the
// helper). Read-only: every write fails.
class FdIo final : public Exiv2::BasicIo {
public:
    explicit FdIo(const FdFile& file) : file_{file} {}

    ~FdIo() override;
    FdIo(const FdIo&) = delete;
    FdIo& operator=(const FdIo&) = delete;
    FdIo(FdIo&&) = delete;
    FdIo& operator=(FdIo&&) = delete;

    int open() override;
    int close() override;
    size_t write(const Exiv2::byte* data, size_t count) override;
    size_t write(Exiv2::BasicIo& source) override;
    int putb(Exiv2::byte data) override;
    Exiv2::DataBuf read(size_t count) override;
    size_t read(Exiv2::byte* buffer, size_t count) override;
    int getb() override;
    void transfer(Exiv2::BasicIo& source) override;
    int seek(int64_t offset, Position from) override;
    Exiv2::byte* mmap(bool writable) override;
    int munmap() override;
    [[nodiscard]] size_t tell() const override;
    [[nodiscard]] size_t size() const override;
    [[nodiscard]] bool isopen() const override;
    [[nodiscard]] int error() const override;
    [[nodiscard]] bool eof() const override;
    [[nodiscard]] const std::string& path() const noexcept override;

    void populateFakeData() override {}

private:
    FdFile file_;
    std::uint64_t position_{0};
    bool eof_{false};
    int error_{0};
    void* mapping_{nullptr};
};

}  // namespace nordend::image::detail
