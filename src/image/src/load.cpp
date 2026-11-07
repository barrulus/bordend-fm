#include "nordend/image/load.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <fstream>
#include <span>
#include <utility>
#include <vector>

#include "jpeg_segments.hpp"
#include "nordend/image/preview.hpp"
#include "nordend/image/scale.hpp"

namespace nordend::image {
namespace {

std::atomic<std::uint64_t> preview_reads{0};

// Bytes read to tell the format, and to find a JPEG's EXIF segment (after the few segments
// before it).
constexpr std::size_t kHead = std::size_t{64} * 1024;

// What a file is, from its first bytes.
enum class Kind : std::uint8_t {
    image,      // PNG, GIF, WebP, JPEG XL, PGM/PPM: decoded whole, never a stored preview
    jpeg,       // decoded whole; a camera's (with EXIF) may carry a larger preview first
    container,  // TIFF and the raw formats built like it, RAF, CR3, CRW, MRW: previews only
    no_reader,  // a raw format exiv2 does not read (Sigma X3F)
    unknown,    // nothing known: only a camera raw's MIME type led here, so exiv2 may know it
};

bool starts_with(std::span<const std::uint8_t> data, std::string_view prefix) {
    return data.size() >= prefix.size() &&
           std::equal(prefix.begin(), prefix.end(), data.begin(),
                      [](char a, std::uint8_t b) { return static_cast<std::uint8_t>(a) == b; });
}

Kind kind_of(std::span<const std::uint8_t> head) {
    switch (sniff(head)) {
        case Format::jpeg:
            return Kind::jpeg;
        case Format::png:
        case Format::gif:
        case Format::webp:
        case Format::jxl:
        case Format::pnm:
            return Kind::image;
        case Format::unknown:
            break;
    }
    using namespace std::string_view_literals;
    // TIFF (also DNG, NEF, CR2, ARW, PEF, SRW, 3FR, ERF), ORF, RW2 and old Panasonic RAW.
    for (const std::string_view magic :
         {"II*\0"sv, "MM\0*"sv, "IIRO"sv, "IIRS"sv, "MMOR"sv, "IIU\0"sv}) {
        if (starts_with(head, magic)) {
            return Kind::container;
        }
    }
    if (starts_with(head, "FUJIFILM") || starts_with(head, "\0MRM"sv) ||
        starts_with(head, "II\x1a\0\0\0HEAPCCDR"sv) ||
        (head.size() >= 12 && starts_with(head.subspan(4), "ftypcrx "))) {
        return Kind::container;
    }
    if (starts_with(head, "FOVb")) {
        return Kind::no_reader;
    }
    return Kind::unknown;
}

Failure failure_of(Problem problem) {
    switch (problem) {
        case Problem::none:
            return Failure::none;
        case Problem::unknown_format:
            return Failure::unknown_format;
        case Problem::too_large:
            return Failure::too_large;
        case Problem::damaged:
            return Failure::damaged;
        case Problem::unreadable:
            return Failure::unreadable;
    }
    return Failure::damaged;
}

Loaded failed(Failure failure, std::string detail) {
    Loaded result;
    result.failure = failure;
    result.detail = std::move(detail);
    return result;
}

}  // namespace

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
    }
    return {};
}

std::uint64_t embedded_preview_reads() noexcept {
    return preview_reads.load(std::memory_order_relaxed);
}

Loaded load(const LoadRequest& request) {
    std::vector<std::uint8_t> head(kHead);
    {
        std::ifstream file{request.path, std::ios::binary};
        if (!file) {
            return failed(Failure::unreadable, "cannot open the file");
        }
        file.read(reinterpret_cast<char*>(head.data()),  // NOLINT: bytes to char for read
                  static_cast<std::streamsize>(head.size()));
        if (file.bad()) {
            return failed(Failure::unreadable, "cannot read the file");
        }
        head.resize(static_cast<std::size_t>(file.gcount()));
    }
    if (head.empty()) {
        return failed(Failure::damaged, "empty file");
    }
    const Kind kind = kind_of(head);
    if (kind == Kind::no_reader) {
        return failed(Failure::no_readable_preview, "a raw format Nordend does not read");
    }

    Loaded result;
    const std::uint32_t side = std::max(request.max_width, request.max_height);
    Image picture;
    std::uint8_t orientation = 1;

    // A preview the file carries: camera JPEGs (only those with EXIF can have one) and raw files.
    EmbeddedPreview preview;
    const bool camera_jpeg = kind == Kind::jpeg && detail::jpeg_exif(head).has_value();
    if (kind == Kind::container || kind == Kind::unknown || camera_jpeg) {
        preview_reads.fetch_add(1, std::memory_order_relaxed);
        preview = embedded_preview(request.path, side);
    }
    const auto from_preview = [&] {
        Decoded decoded = decode_preview(preview);
        if (!decoded.error.empty()) {
            result.detail = decoded.error;
            return false;
        }
        picture = std::move(decoded.image);
        orientation = preview.orientation;
        result.from_preview = true;
        return true;
    };
    if (!preview.data.empty() && std::max(preview.width, preview.height) >= side) {
        from_preview();
    }
    if (picture.empty() && (kind == Kind::image || kind == Kind::jpeg)) {
        Decoded decoded = decode_file(request.path, {.apply_orientation = false});
        if (decoded.error.empty()) {
            picture = std::move(decoded.image);
            orientation = decoded.orientation;
            result.full_width = picture.width;
            result.full_height = picture.height;
        } else if (preview.data.empty() || !from_preview()) {
            return failed(failure_of(decoded.problem), std::move(decoded.error));
        }
    }
    if (picture.empty()) {
        // A raw file: the largest preview it has, whatever its size.
        if (preview.data.empty() || !from_preview()) {
            std::string detail = preview.error;
            if (detail.empty()) {
                detail = result.detail.empty() ? "no preview in the file" : result.detail;
            }
            return failed(
                kind == Kind::unknown ? Failure::unknown_format : Failure::no_readable_preview,
                std::move(detail));
        }
    }
    if (result.full_width == 0 && preview.full_width > 0) {
        result.full_width = preview.full_width;
        result.full_height = preview.full_height;
    }
    if (orientation >= 5) {
        std::swap(result.full_width, result.full_height);
    }
    result.detail.clear();
    // Scaled first, turned after: the turn then works on screen-sized pixels.
    result.image = apply_orientation(scale_to_fit(picture, request.max_width, request.max_height),
                                     orientation);
    return result;
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

}  // namespace nordend::image
