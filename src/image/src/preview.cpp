#include "nordend/image/preview.hpp"

#include <algorithm>
#include <exiv2/exiv2.hpp>
#include <fstream>
#include <mutex>
#include <optional>
#include <utility>

#include "jpeg_segments.hpp"
#include "nordend/image/decode.hpp"

namespace nordend::image {
namespace {

// Bytes read to find a JPEG's EXIF segment (at most 64 KB, after the few segments before it),
// and to find a preview's size in its header.
constexpr std::size_t kExifHead = std::size_t{256} * 1024;
constexpr std::size_t kPreviewHead = std::size_t{64} * 1024;

// Where a preview is and how big it is; data read only for the one chosen.
struct Candidate {
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::optional<std::size_t> property;  // exiv2's preview, by index; else read from the file:
    std::size_t offset{0};
    std::size_t length{0};

    [[nodiscard]] std::uint32_t long_side() const { return std::max(width, height); }
};

void init_exiv2() {
    static std::once_flag once;
    std::call_once(once, [] {
        Exiv2::XmpParser::initialize();  // makes exiv2 safe to use from several threads
        Exiv2::LogMsg::setLevel(Exiv2::LogMsg::mute);
    });
}

std::optional<std::int64_t> number(const Exiv2::ExifData& exif, const char* key) {
    const auto it = exif.findKey(Exiv2::ExifKey{key});
    if (it == exif.end() || it->count() == 0) {
        return std::nullopt;
    }
    return it->toInt64();
}

std::vector<std::uint8_t> read_range(const std::filesystem::path& path, std::size_t offset,
                                     std::size_t length) {
    std::vector<std::uint8_t> data(length);
    std::ifstream file{path, std::ios::binary};
    if (!file.seekg(static_cast<std::streamoff>(offset)) ||
        !file.read(reinterpret_cast<char*>(data.data()),  // NOLINT: bytes to char for read
                   static_cast<std::streamsize>(length))) {
        return {};
    }
    return data;
}

// The Olympus / OM System maker-note preview: its offset counts from the maker note, which
// counts from the TIFF data (at the start of a raw file, after "Exif\0\0" in a JPEG).
std::optional<Candidate> olympus_preview(const std::filesystem::path& path,
                                         const Exiv2::Image& image) {
    const Exiv2::ExifData& exif = image.exifData();
    const auto valid = number(exif, "Exif.OlympusCs.PreviewImageValid");
    const auto start = number(exif, "Exif.OlympusCs.PreviewImageStart");
    const auto length = number(exif, "Exif.OlympusCs.PreviewImageLength");
    const auto maker_note = number(exif, "Exif.MakerNote.Offset");
    if (valid.value_or(0) != 1 || !start || !length || !maker_note || *start <= 0 || *length <= 4 ||
        *maker_note < 0) {
        return std::nullopt;
    }
    std::size_t tiff = 0;
    if (image.imageType() == Exiv2::ImageType::jpeg) {
        const std::vector<std::uint8_t> head =
            read_range(path, 0, std::min<std::size_t>(kExifHead, std::filesystem::file_size(path)));
        const std::optional<detail::JpegExif> segment = detail::jpeg_exif(head);
        if (!segment) {
            return std::nullopt;
        }
        tiff = segment->offset;
    }
    Candidate candidate;
    candidate.offset =
        tiff + static_cast<std::size_t>(*maker_note) + static_cast<std::size_t>(*start);
    candidate.length = static_cast<std::size_t>(*length);
    // Its size is in its own header: read the start of it.
    const std::vector<std::uint8_t> head =
        read_range(path, candidate.offset, std::min(candidate.length, kPreviewHead));
    const std::optional<detail::JpegSize> size = detail::jpeg_size(head);
    if (!size) {
        return std::nullopt;  // not a JPEG where the tags say
    }
    candidate.width = size->width;
    candidate.height = size->height;
    return candidate;
}

}  // namespace

EmbeddedPreview embedded_preview(const std::filesystem::path& path, std::uint32_t min_long_side) {
    init_exiv2();
    EmbeddedPreview result;
    try {
        const Exiv2::Image::UniquePtr image = Exiv2::ImageFactory::open(path.string());
        image->readMetadata();
        std::vector<Candidate> candidates;
        Exiv2::PreviewManager previews{*image};
        const Exiv2::PreviewPropertiesList properties = previews.getPreviewProperties();
        for (std::size_t i = 0; i < properties.size(); ++i) {
            if (properties[i].mimeType_ == "image/jpeg") {
                candidates.push_back({.width = static_cast<std::uint32_t>(properties[i].width_),
                                      .height = static_cast<std::uint32_t>(properties[i].height_),
                                      .property = i,
                                      .offset = 0,
                                      .length = properties[i].size_});
            }
        }
        if (std::optional<Candidate> olympus = olympus_preview(path, *image)) {
            candidates.push_back(*olympus);
        }
        if (candidates.empty()) {
            return result;
        }
        // The smallest that is big enough, else the largest.
        std::size_t best = 0;
        for (std::size_t i = 1; i < candidates.size(); ++i) {
            const std::uint32_t side = candidates[i].long_side();
            const std::uint32_t best_side = candidates[best].long_side();
            const bool enough = side >= min_long_side;
            const bool best_enough = best_side >= min_long_side;
            if ((enough && (!best_enough || side < best_side)) ||
                (!enough && !best_enough && side > best_side)) {
                best = i;
            }
        }
        const Candidate& chosen = candidates[best];
        std::vector<std::uint8_t> data;
        if (chosen.property) {
            const Exiv2::PreviewImage preview =
                previews.getPreviewImage(properties.at(*chosen.property));
            data.assign(preview.pData(), preview.pData() + preview.size());
        } else {
            data = read_range(path, chosen.offset, chosen.length);
        }
        if (data.size() < 4 || data[0] != 0xFF || data[1] != 0xD8) {
            return result;  // not a JPEG after all
        }
        result.jpeg = std::move(data);
        result.width = chosen.width;
        result.height = chosen.height;
        result.full_width = image->pixelWidth();
        result.full_height = image->pixelHeight();
        const auto orientation = number(image->exifData(), "Exif.Image.Orientation");
        if (orientation && *orientation >= 1 && *orientation <= 8) {
            result.orientation = static_cast<std::uint8_t>(*orientation);
        }
    } catch (const Exiv2::Error& error) {
        result = {};
        result.error = error.what();
    }
    return result;
}

}  // namespace nordend::image
