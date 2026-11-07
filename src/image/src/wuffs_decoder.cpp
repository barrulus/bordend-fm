#include <cstring>
#include <limits>
#include <string>
#include <utility>

#include "decoders.hpp"
#include "wuffs-v0.4.c"  // NOLINT(bugprone-suspicious-include): a header without WUFFS_IMPLEMENTATION
#include "wuffs_config.hpp"

namespace nordend::image::detail {
namespace {

// Decodes into the Image's own vector (no copy afterwards), as RGBA with straight alpha, and
// reads the EXIF orientation on the way.
class Callbacks : public wuffs_aux::DecodeImageCallbacks {
public:
    Callbacks(Image& image, std::uint64_t max_pixels, std::uint8_t& orientation)
        : image_{image}, max_pixels_{max_pixels}, orientation_{orientation} {}

    wuffs_base__pixel_format SelectPixfmt(const wuffs_base__image_config& /*config*/) override {
        return wuffs_base__make_pixel_format(WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL);
    }

    std::string HandleMetadata(const wuffs_base__more_information& minfo,
                               wuffs_base__slice_u8 raw) override {
        if (minfo.metadata__fourcc() == WUFFS_BASE__FOURCC__EXIF) {
            orientation_ = exif_orientation({raw.ptr, raw.len});
        }
        return {};
    }

    AllocPixbufResult AllocPixbuf(const wuffs_base__image_config& config,
                                  bool /*allow_uninitialized_memory*/) override {
        const std::uint32_t width = config.pixcfg.width();
        const std::uint32_t height = config.pixcfg.height();
        if (width == 0 || height == 0) {
            return AllocPixbufResult(std::string{"the image has no pixels"});
        }
        if (std::uint64_t{width} * height > max_pixels_) {
            return AllocPixbufResult(std::string{kTooLarge});
        }
        const std::uint64_t length = config.pixcfg.pixbuf_len();
        if (length != std::uint64_t{width} * height * 4) {
            return AllocPixbufResult(std::string{wuffs_aux::DecodeImage_UnsupportedPixelFormat});
        }
        image_.width = width;
        image_.height = height;
        image_.rgba.assign(length, 0);  // transparent where a GIF frame does not reach
        wuffs_base__pixel_buffer pixbuf;
        const wuffs_base__status status = pixbuf.set_from_slice(
            &config.pixcfg, wuffs_base__make_slice_u8(image_.rgba.data(), image_.rgba.size()));
        if (!status.is_ok()) {
            return AllocPixbufResult(std::string{status.message()});
        }
        return {wuffs_aux::MemOwner{nullptr, &free}, pixbuf};
    }

private:
    Image& image_;
    std::uint64_t max_pixels_;
    std::uint8_t& orientation_;
};

}  // namespace

void decode_wuffs(std::span<const std::uint8_t> data, const DecodeOptions& options,
                  Decoded& result) {
    Callbacks callbacks{result.image, options.max_pixels, result.orientation};
    wuffs_aux::sync_io::MemoryInput input{data.data(), data.size()};
    const wuffs_aux::DecodeImageResult decoded = wuffs_aux::DecodeImage(
        callbacks, input, wuffs_aux::DecodeImageArgQuirks::DefaultValue(),
        wuffs_aux::DecodeImageArgFlags{wuffs_aux::DecodeImageArgFlags::REPORT_METADATA_EXIF},
        wuffs_aux::DecodeImageArgPixelBlend::DefaultValue(),
        wuffs_aux::DecodeImageArgBackgroundColor::DefaultValue(),
        wuffs_aux::DecodeImageArgMaxInclDimension{std::numeric_limits<std::uint16_t>::max()});
    if (!decoded.error_message.empty()) {
        result.error = decoded.error_message;
    }
}

}  // namespace nordend::image::detail
