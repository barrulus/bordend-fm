#include "nordend/picture/load.hpp"

#include <algorithm>
#include <array>
#include <mutex>
#include <utility>

namespace nordend::picture {

std::string_view failure_message(Failure failure) noexcept {
    switch (failure) {
        case Failure::none:
            return {};
        case Failure::unknown_format:
            return "Nordend can't show this kind of file";
        case Failure::no_readable_preview:
            return "No preview Nordend can show in this file";
        case Failure::too_large:
            return "This image is too large to show";
        case Failure::damaged:
            return "This image looks damaged";
        case Failure::unreadable:
            return "This file can't be read";
        case Failure::unavailable:
            return "Preview unavailable";
    }
    return {};
}

namespace {
std::mutex loader_mutex;
Loader current_loader;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables): set once
}  // namespace

void set_loader(Loader loader) {
    const std::scoped_lock lock{loader_mutex};
    current_loader = std::move(loader);
}

Loaded load_picture(const LoadRequest& request) {
    Loader loader;
    {
        const std::scoped_lock lock{loader_mutex};
        loader = current_loader;
    }
    if (!loader) {
        Loaded none;
        none.failure = Failure::unavailable;
        none.detail = "no picture loader";
        return none;
    }
    return loader(request);
}

namespace {
std::mutex info_loader_mutex;
InfoLoader current_info_loader;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
}  // namespace

void set_info_loader(InfoLoader loader) {
    const std::scoped_lock lock{info_loader_mutex};
    current_info_loader = std::move(loader);
}

PhotoInfo load_photo_info(const std::filesystem::path& path) {
    InfoLoader loader;
    {
        const std::scoped_lock lock{info_loader_mutex};
        loader = current_info_loader;
    }
    return loader ? loader(path) : PhotoInfo{};
}

bool may_show(std::string_view mime) noexcept {
    static constexpr std::array<std::string_view, 30> kTypes{"image/png",
                                                             "image/jpeg",
                                                             "image/gif",
                                                             "image/webp",
                                                             "image/jxl",
                                                             "image/tiff",
                                                             "image/x-adobe-dng",
                                                             "image/x-canon-cr2",
                                                             "image/x-canon-cr3",
                                                             "image/x-canon-crw",
                                                             "image/x-dcraw",
                                                             "image/x-fuji-raf",
                                                             "image/x-kodak-dcr",
                                                             "image/x-kodak-k25",
                                                             "image/x-kodak-kdc",
                                                             "image/x-minolta-mrw",
                                                             "image/x-nikon-nef",
                                                             "image/x-nikon-nrw",
                                                             "image/x-olympus-orf",
                                                             "image/x-panasonic-raw",
                                                             "image/x-panasonic-raw2",
                                                             "image/x-panasonic-rw",
                                                             "image/x-panasonic-rw2",
                                                             "image/x-pentax-pef",
                                                             "image/x-portable-graymap",
                                                             "image/x-portable-pixmap",
                                                             "image/x-sigma-x3f",
                                                             "image/x-sony-arw",
                                                             "image/x-sony-sr2",
                                                             "image/x-sony-srf"};
    return std::ranges::find(kTypes, mime) != kTypes.end();
}

}  // namespace nordend::picture
