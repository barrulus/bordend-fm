#include "nordend/formats/image/photo.hpp"

#include <exception>
#include <exiv2/exiv2.hpp>
#include <fcntl.h>
#include <memory>
#include <mutex>
#include <optional>
#include <string_view>
#include <system_error>
#include <unistd.h>
#include <utility>

#include "exiv2_init.hpp"
#include "fd_file.hpp"

namespace nordend::formats::image {
namespace {

// A value as exiv2 prints it (interpreted: "1/2000 s", "Spot"), trimmed; empty when absent.
std::string printed(const Exiv2::ExifData& exif, const char* key) {
    const auto it = exif.findKey(Exiv2::ExifKey{key});
    if (it == exif.end() || it->count() == 0) {
        return {};
    }
    std::string text = it->print(&exif);
    const std::size_t end = text.find_last_not_of(" \t");
    text.resize(end == std::string::npos ? 0 : end + 1);
    return text;
}

// "2026:08:31 16:34:10" -> "2026-08-31 16:34".
std::string date_of(std::string exif) {
    if (exif.size() < 16 || exif[4] != ':' || exif[7] != ':') {
        return exif;
    }
    exif[4] = '-';
    exif[7] = '-';
    exif.resize(16);
    return exif;
}

// Model alone when it names the maker ("Canon EOS 5DS") or the maker is long ("OM Digital
// Solutions" makes "OM-1"); else both ("NIKON CORPORATION" + "NIKON D800" -> the model).
std::string camera_of(const std::string& make, const std::string& model) {
    if (model.empty()) {
        return make;
    }
    const std::string_view first_word = std::string_view{make}.substr(0, make.find(' '));
    if (make.empty() || model.find(first_word) != std::string::npos ||
        make.find(' ') != std::string::npos) {
        return model;
    }
    return make + " " + model;
}

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
    detail::init_exiv2();
    PhotoInfo info;
    try {
        const std::optional<detail::FdFile> file = detail::FdFile::of(fd);
        const Exiv2::Image::UniquePtr image =
            file ? Exiv2::ImageFactory::open(std::make_unique<detail::FdIo>(*file)) : nullptr;
        if (!image) {
            // A type exiv2 does not read: no EXIF, but the sidecar may still rate it.
            info.rating = sidecar_fd >= 0 ? sidecar_rating(sidecar_fd) : std::nullopt;
            return info;
        }
        image->readMetadata();
        const Exiv2::ExifData& exif = image->exifData();
        info.width = image->pixelWidth();
        info.height = image->pixelHeight();
        const auto orientation = exif.findKey(Exiv2::ExifKey{"Exif.Image.Orientation"});
        if (orientation != exif.end() && orientation->count() > 0 && orientation->toInt64() >= 5 &&
            orientation->toInt64() <= 8) {
            std::swap(info.width, info.height);
        }
        info.camera =
            camera_of(printed(exif, "Exif.Image.Make"), printed(exif, "Exif.Image.Model"));
        info.lens = printed(exif, "Exif.Photo.LensModel");
        info.exposure_time = printed(exif, "Exif.Photo.ExposureTime");
        info.aperture = printed(exif, "Exif.Photo.FNumber");
        info.iso = printed(exif, "Exif.Photo.ISOSpeedRatings");
        info.focal_length = printed(exif, "Exif.Photo.FocalLength");
        info.taken = date_of(printed(exif, "Exif.Photo.DateTimeOriginal"));
        info.program = printed(exif, "Exif.Photo.ExposureProgram");
        info.exposure_bias = printed(exif, "Exif.Photo.ExposureBiasValue");
        info.metering = printed(exif, "Exif.Photo.MeteringMode");
        info.flash = printed(exif, "Exif.Photo.Flash");
        info.white_balance = printed(exif, "Exif.Photo.WhiteBalance");
        info.software = printed(exif, "Exif.Image.Software");
    } catch (const std::exception&) {
        info = {};
    }
    info.rating = sidecar_fd >= 0 ? sidecar_rating(sidecar_fd) : std::nullopt;
    return info;
}

}  // namespace nordend::formats::image
