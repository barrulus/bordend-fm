#include "nordend/image/preview.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <exiv2/exiv2.hpp>
#include <fcntl.h>
#include <mutex>
#include <optional>
#include <system_error>
#include <tuple>
#include <unistd.h>
#include <utility>

#include "exiv2_init.hpp"
#include "fd_file.hpp"
#include "jpeg_segments.hpp"
#include "nordend/image/decode.hpp"
#include "tiff.hpp"

namespace nordend::image {
namespace {

// Bytes read to find a JPEG's EXIF segment (at most 64 KB, after the few segments before it),
// to find a preview's size in its header, and to read a raw file's first IFD.
constexpr std::size_t kExifHead = std::size_t{256} * 1024;
constexpr std::size_t kPreviewHead = std::size_t{64} * 1024;
constexpr std::size_t kIfdHead = std::size_t{64} * 1024;

// Where a preview is and how big it is (as shown, after the crop); data read only for the one
// chosen.
struct Candidate {
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::optional<std::size_t> property;  // exiv2's preview, by index; else read from the file:
    std::size_t offset{0};
    std::size_t length{0};
    bool tiff{false};
    std::optional<PreviewCrop> crop;

    [[nodiscard]] std::uint32_t long_side() const { return std::max(width, height); }
};

std::optional<std::int64_t> number(const Exiv2::ExifData& exif, const char* key) {
    const auto it = exif.findKey(Exiv2::ExifKey{key});
    if (it == exif.end() || it->count() == 0) {
        return std::nullopt;
    }
    return it->toInt64();
}

// The Olympus / OM System maker-note preview: its offset counts from the maker note, which
// counts from the TIFF data (at the start of a raw file, after "Exif\0\0" in a JPEG).
std::optional<Candidate> olympus_preview(const detail::FdFile& file, const Exiv2::Image& image) {
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
        const std::vector<std::uint8_t> head = file.read(0, kExifHead);
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
        file.read_exactly(candidate.offset, std::min(candidate.length, kPreviewHead));
    const std::optional<detail::JpegSize> size = detail::jpeg_size(head);
    if (!size) {
        return std::nullopt;  // not a JPEG where the tags say
    }
    candidate.width = size->width;
    candidate.height = size->height;
    return candidate;
}

// The Panasonic crop: the GX8 stores a 4:3 preview whatever the aspect ratio set in the camera,
// and the frame shot in its first IFD, in sensor pixels (CropTop/Left/Bottom/Right, tags
// 0x2F-0x32, which exiv2 does not read), within the sensor's borders (tags 0x04-0x07).
struct SensorCrop {
    std::uint32_t left{0};
    std::uint32_t top{0};
    std::uint32_t right{0};
    std::uint32_t bottom{0};
    std::uint32_t border_left{0};
    std::uint32_t border_top{0};
    std::uint32_t border_right{0};
    std::uint32_t border_bottom{0};

    // The same frame in a preview of width x height, which shows everything within the borders.
    [[nodiscard]] PreviewCrop in(std::uint32_t width, std::uint32_t height) const {
        // (at - from) * size / (to - from), rounded.
        const auto scale = [](std::uint32_t at, std::uint32_t from, std::uint32_t to,
                              std::uint32_t size) {
            const std::uint64_t span = to - from;
            return static_cast<std::uint32_t>(((std::uint64_t{at - from} * size * 2) + span) /
                                              (span * 2));
        };
        const std::uint32_t x0 = scale(left, border_left, border_right, width);
        const std::uint32_t x1 = scale(right, border_left, border_right, width);
        const std::uint32_t y0 = scale(top, border_top, border_bottom, height);
        const std::uint32_t y1 = scale(bottom, border_top, border_bottom, height);
        return {.x = x0, .y = y0, .width = x1 - x0, .height = y1 - y0};
    }
};

std::optional<SensorCrop> panasonic_crop(const detail::FdFile& file) {
    const std::vector<std::uint8_t> head = file.read(0, kIfdHead);
    const std::optional<detail::TiffReader> tiff = detail::TiffReader::open(head);
    if (!tiff) {
        return std::nullopt;
    }
    const std::size_t ifd = tiff->first_ifd();
    const auto tag = [&](std::uint16_t number) { return tiff->number(ifd, number); };
    const auto top = tag(0x2F);
    const auto left = tag(0x30);
    const auto bottom = tag(0x31);
    const auto right = tag(0x32);
    const auto border_top = tag(0x04);
    const auto border_left = tag(0x05);
    const auto border_bottom = tag(0x06);
    const auto border_right = tag(0x07);
    if (!top || !left || !bottom || !right || !border_top || !border_left || !border_bottom ||
        !border_right) {
        return std::nullopt;  // no crop tags (GX7 and older store the preview cropped)
    }
    const SensorCrop crop{.left = *left,
                          .top = *top,
                          .right = *right,
                          .bottom = *bottom,
                          .border_left = *border_left,
                          .border_top = *border_top,
                          .border_right = *border_right,
                          .border_bottom = *border_bottom};
    const bool inside = crop.border_left <= crop.left && crop.left < crop.right &&
                        crop.right <= crop.border_right && crop.border_top <= crop.top &&
                        crop.top < crop.bottom && crop.bottom <= crop.border_bottom;
    const bool whole = crop.left == crop.border_left && crop.right == crop.border_right &&
                       crop.top == crop.border_top && crop.bottom == crop.border_bottom;
    if (!inside || whole) {
        return std::nullopt;
    }
    return crop;
}

// The order the candidates are tried in: JPEGs large enough, smallest first; then TIFFs large
// enough (raw RGB, large on disk, and only some can be read), smallest first; then, when none is
// large enough, the largest, a JPEG before a TIFF of the same size. Ties keep the file's order.
void order_by_preference(std::vector<Candidate>& candidates, std::uint32_t min_long_side) {
    const auto key = [min_long_side](const Candidate& c) {
        const std::uint32_t side = c.long_side();
        if (side >= min_long_side) {
            return std::tuple{c.tiff ? 1 : 0, std::int64_t{side}, 0};
        }
        return std::tuple{2, -std::int64_t{side}, c.tiff ? 1 : 0};
    };
    std::ranges::stable_sort(candidates, {}, key);
}

}  // namespace

void detail::init_exiv2() {
    static std::once_flag once;
    std::call_once(once, [] {
        Exiv2::XmpParser::initialize();  // makes exiv2 safe to use from several threads
        Exiv2::LogMsg::setLevel(Exiv2::LogMsg::mute);
    });
}

EmbeddedPreview embedded_preview(const std::filesystem::path& path, std::uint32_t min_long_side) {
    const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        EmbeddedPreview result;
        result.error = std::error_code{errno, std::generic_category()}.message();
        return result;
    }
    EmbeddedPreview result = embedded_preview(fd, min_long_side);
    ::close(fd);
    return result;
}

EmbeddedPreview embedded_preview(int fd, std::uint32_t min_long_side) {
    detail::init_exiv2();
    EmbeddedPreview result;
    const std::optional<detail::FdFile> file = detail::FdFile::of(fd);
    if (!file) {
        result.error = "cannot read the file";
        return result;
    }
    try {
        // With an input of its own, exiv2 answers a type it does not know with nothing, not an
        // exception.
        const Exiv2::Image::UniquePtr image =
            Exiv2::ImageFactory::open(std::make_unique<detail::FdIo>(*file));
        if (!image) {
            result.error = "a file type exiv2 does not read";
            return result;
        }
        image->readMetadata();
        std::vector<Candidate> candidates;
        Exiv2::PreviewManager previews{*image};
        const Exiv2::PreviewPropertiesList properties = previews.getPreviewProperties();
        // Canon's TIFF previews (CR2's third IFD) are sensor data without white balance or
        // tone curve: green and dark (checked on the 1D Mark II, 1D X and 5DS samples).
        const bool tiffs = image->imageType() != Exiv2::ImageType::cr2;
        for (std::size_t i = 0; i < properties.size(); ++i) {
            const bool tiff = properties[i].mimeType_ == "image/tiff";
            if (properties[i].mimeType_ == "image/jpeg" || (tiff && tiffs)) {
                candidates.push_back({.width = static_cast<std::uint32_t>(properties[i].width_),
                                      .height = static_cast<std::uint32_t>(properties[i].height_),
                                      .property = i,
                                      .offset = 0,
                                      .length = properties[i].size_,
                                      .tiff = tiff,
                                      .crop = std::nullopt});
            }
        }
        if (std::optional<Candidate> olympus = olympus_preview(*file, *image)) {
            candidates.push_back(*olympus);
        }
        const std::optional<SensorCrop> sensor_crop =
            image->imageType() == Exiv2::ImageType::rw2 ? panasonic_crop(*file) : std::nullopt;
        if (sensor_crop) {
            for (Candidate& candidate : candidates) {
                candidate.crop = sensor_crop->in(candidate.width, candidate.height);
                candidate.width = candidate.crop->width;
                candidate.height = candidate.crop->height;
            }
        }
        // The EXIF thumbnail is 4:3 whatever the photo: a 3:2 or 16:9 one comes with black bars
        // (Canon, Nikon, Sony, Panasonic GX7 1:1). Previews shaped unlike the largest are dropped.
        const auto largest = std::ranges::max_element(
            candidates, {}, [](const Candidate& c) { return std::uint64_t{c.width} * c.height; });
        if (largest != candidates.end() && largest->width > 0 && largest->height > 0) {
            const std::uint64_t w = largest->width;
            const std::uint64_t h = largest->height;
            std::erase_if(candidates, [&](const Candidate& c) {
                // |c.width / c.height - w / h| > 2 % of w / h, in integers.
                const std::uint64_t a = std::uint64_t{c.width} * h;
                const std::uint64_t b = std::uint64_t{c.height} * w;
                return (a > b ? a - b : b - a) * 50 > b;
            });
        }
        // Tried in order; one that turns out broken (Samsung EX2F's 640x360 JPEG stops before
        // its end) is passed over for the next.
        order_by_preference(candidates, min_long_side);
        std::vector<std::uint8_t> data;
        std::optional<Candidate> picked;
        for (Candidate chosen : candidates) {
            if (chosen.property) {
                const Exiv2::PreviewImage preview =
                    previews.getPreviewImage(properties.at(*chosen.property));
                data.assign(preview.pData(), preview.pData() + preview.size());
            } else {
                data = file->read_exactly(chosen.offset, chosen.length);
            }
            if (chosen.tiff) {
                // A TIFF preview compressed as JPEG (DNG): the JPEG itself, read like any other.
                if (const auto inner = detail::jpeg_in_tiff(data)) {
                    const std::optional<detail::JpegSize> size = detail::jpeg_size(*inner);
                    if (!size) {
                        continue;
                    }
                    data = std::vector<std::uint8_t>(inner->begin(), inner->end());
                    chosen.tiff = false;
                    if (!chosen.crop) {
                        chosen.width = size->width;
                        chosen.height = size->height;
                    }
                }
            }
            const bool usable =
                chosen.tiff ? detail::TiffReader::open(data).has_value() : detail::whole_jpeg(data);
            if (usable) {
                picked = chosen;
                break;
            }
        }
        if (!picked) {
            return result;
        }
        const Candidate& chosen = *picked;
        result.data = std::move(data);
        result.tiff = chosen.tiff;
        result.width = chosen.width;
        result.height = chosen.height;
        result.crop = chosen.crop;
        if (sensor_crop) {
            result.full_width = sensor_crop->right - sensor_crop->left;
            result.full_height = sensor_crop->bottom - sensor_crop->top;
        } else {
            result.full_width = image->pixelWidth();
            result.full_height = image->pixelHeight();
        }
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

Decoded decode_preview(const EmbeddedPreview& preview) {
    Decoded result;
    if (preview.tiff) {
        detail::decode_tiff_rgb8(preview.data, {}, result);
    } else {
        result = decode(preview.data, {.apply_orientation = false});
    }
    result.orientation = preview.orientation;
    if (!result.error.empty() || !preview.crop) {
        return result;
    }
    const PreviewCrop& crop = *preview.crop;
    const Image& whole = result.image;
    if (crop.width == 0 || crop.height == 0 || crop.x + crop.width > whole.width ||
        crop.y + crop.height > whole.height) {
        return result;  // not the preview the crop was made for: shown whole
    }
    Image cropped{.width = crop.width, .height = crop.height, .rgba = {}};
    cropped.rgba.resize(std::size_t{crop.width} * crop.height * 4);
    const std::size_t row = std::size_t{crop.width} * 4;
    for (std::uint32_t y = 0; y < crop.height; ++y) {
        const std::size_t from = ((std::size_t{crop.y + y} * whole.width) + crop.x) * 4;
        std::memcpy(cropped.rgba.data() + (y * row), whole.rgba.data() + from, row);
    }
    result.image = std::move(cropped);
    return result;
}

}  // namespace nordend::image
