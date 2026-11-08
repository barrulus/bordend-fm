#include "nordend/formats/image/photo.hpp"

#include <exception>
#include <exiv2/exiv2.hpp>
#include <fcntl.h>
#include <memory>
#include <mutex>
#include <optional>
#include <system_error>
#include <unistd.h>
#include <utility>

#include "camera/info.hpp"
#include "exiv2_init.hpp"
#include "fd_file.hpp"

namespace nordend::formats::image {
namespace {

// The darktable-style sidecar's rating (Xmp.xmp.Rating): 1-5 stars, -1 rejected.
std::optional<int> sidecar_rating(int sidecar_fd) {
    const std::optional<detail::FdFile> file = detail::FdFile::of(sidecar_fd);
    if (!file) {
        return std::nullopt;
    }
    try {
        const Exiv2::Image::UniquePtr xmp =
            Exiv2::ImageFactory::open(std::make_unique<detail::FdIo>(*file));
        if (!xmp) {
            return std::nullopt;
        }
        xmp->readMetadata();
        const auto it = xmp->xmpData().findKey(Exiv2::XmpKey{"Xmp.xmp.Rating"});
        if (it == xmp->xmpData().end() || it->count() == 0) {
            return std::nullopt;
        }
        const auto rating = static_cast<int>(it->toInt64());
        if (rating >= -1 && rating <= 5 && rating != 0) {
            return rating;
        }
    } catch (const std::exception&) {
        return std::nullopt;  // an unreadable sidecar has no rating
    }
    return std::nullopt;
}

}  // namespace

void detail::init_exiv2() {
    static std::once_flag once;
    std::call_once(once, [] {
        Exiv2::XmpParser::initialize();  // makes exiv2 safe to use from several threads
        Exiv2::LogMsg::setLevel(Exiv2::LogMsg::mute);
    });
}

PhotoInfo read_photo_info(const std::filesystem::path& path) {
    const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        return {};
    }
    std::filesystem::path sidecar = path;
    sidecar += ".xmp";
    std::error_code error;
    const int sidecar_fd = std::filesystem::is_regular_file(sidecar, error)
                               ? ::open(sidecar.c_str(), O_RDONLY | O_CLOEXEC)
                               : -1;
    PhotoInfo info = read_photo_info(fd, sidecar_fd);
    ::close(fd);
    if (sidecar_fd >= 0) {
        ::close(sidecar_fd);
    }
    return info;
}

PhotoInfo read_photo_info(int fd, int sidecar_fd) {
    PhotoInfo info;
    if (const std::optional<detail::FdFile> file = detail::FdFile::of(fd)) {
        info = camera::read_info(*file);
    }
    info.rating = sidecar_fd >= 0 ? sidecar_rating(sidecar_fd) : std::nullopt;
    return info;
}

}  // namespace nordend::formats::image
