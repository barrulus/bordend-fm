#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <lcms2.h>
#include <libheif/heif.h>
#include <limits>
#include <memory>
#include <vector>

#include "decoders.hpp"

namespace rummage::formats::image::detail {
namespace {

template <typename T, auto Free>
using Owned = std::unique_ptr<T, decltype(Free)>;

bool failed(heif_error error, Decoded& result) {
    if (error.code == heif_error_Ok) {
        return false;
    }
    result.error = std::string{"HEIF: "} + error.message;
    if (error.subcode == heif_suberror_Security_limit_exceeded) {
        result.problem = Problem::too_large;
    } else if (error.code == heif_error_Unsupported_filetype ||
               error.code == heif_error_Unsupported_feature ||
               error.subcode == heif_suberror_No_matching_decoder_installed ||
               error.code == heif_error_Plugin_loading_error) {
        result.problem = Problem::unknown_format;
    }
    return true;
}

bool check_size(int width, int height, const DecodeOptions& options, Decoded& result) {
    if (width <= 0 || height <= 0) {
        result.error = "HEIF: invalid image dimensions";
        return false;
    }
    const auto pixels = static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height);
    if (pixels > options.max_pixels || pixels > std::numeric_limits<std::size_t>::max() / 4) {
        result.error = kTooLarge;
        return false;
    }
    return true;
}

// NCLX describes RGB primaries and a transfer curve, independently of the YCbCr
// matrix libheif already applied. Keep the source values until we convert here:
// libheif's 8-bit conversion alone does not guarantee transfer/primary conversion.
double linear_sample(double value, int transfer) {
    switch (transfer) {
        case 1:  // BT.709 / BT.601 / BT.2020
        case 6:
        case 14:
        case 15:
            return value < 0.081 ? value / 4.5 : std::pow((value + 0.099) / 1.099, 1.0 / 0.45);
        case 4:
            return std::pow(value, 2.2);
        case 5:
            return std::pow(value, 2.8);
        case 8:
            return value;
        case 16: {  // ST 2084 PQ: absolute luminance, relative to 203-nit SDR white.
            constexpr double m1 = 2610.0 / 16384.0;
            constexpr double m2 = 2523.0 / 32.0;
            const double p = std::pow(value, 1.0 / m2);
            return (10000.0 / 203.0) * std::pow(std::max(p - 3424.0 / 4096.0, 0.0) /
                                                    (2413.0 / 128.0 - (2392.0 / 128.0) * p),
                                                1.0 / m1);
        }
        case 18:  // HLG inverse OETF; the luminance-dependent OOTF follows the RGB matrix.
            return value <= 0.5 ? value * value / 3.0
                                : (std::exp((value - 0.55991073) / 0.17883277) + 0.28466892) / 12.0;
        default:  // sRGB, or unspecified (assumed sRGB).
            return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
    }
}

std::uint8_t srgb_byte(double linear) {
    // 16-bit linear lookup: bounded error below one 8-bit code, and avoids three
    // pow() calls per output pixel on large phone photos.
    static const auto table = [] {
        std::array<std::uint8_t, 65536> values{};
        for (std::size_t i = 0; i < values.size(); ++i) {
            const double v = static_cast<double>(i) / 65535.0;
            const double encoded =
                v <= 0.0031308 ? 12.92 * v : 1.055 * std::pow(v, 1.0 / 2.4) - 0.055;
            values[i] = static_cast<std::uint8_t>(std::lround(encoded * 255.0));
        }
        return values;
    }();
    return table[static_cast<std::size_t>(std::lround(std::clamp(linear, 0.0, 1.0) * 65535.0))];
}

bool convert_colour(const heif_image_handle* handle, heif_image* decoded, Image& image,
                    Decoded& result) {
    const std::size_t icc_size = heif_image_handle_get_raw_color_profile_size(handle);
    Owned<void, cmsCloseProfile> input{nullptr, cmsCloseProfile};
    Owned<void, cmsCloseProfile> output{nullptr, cmsCloseProfile};
    int transfer = 13;
    bool srgb_source = false;
    const bool eight_bit = heif_image_get_chroma_format(decoded) == heif_chroma_interleaved_RGBA;
    const bool premultiplied = heif_image_is_premultiplied_alpha(decoded);
    if (icc_size != 0) {
        if (icc_size > 4 * 1024 * 1024) {
            result.error = "HEIF: ICC profile too large";
            result.problem = Problem::too_large;
            return false;
        }
        std::vector<std::uint8_t> bytes(icc_size);
        if (failed(heif_image_handle_get_raw_color_profile(handle, bytes.data()), result)) {
            return false;
        }
        input.reset(cmsOpenProfileFromMem(bytes.data(), static_cast<cmsUInt32Number>(icc_size)));
        output.reset(cmsCreate_sRGBProfile());
    } else {
        heif_color_profile_nclx* raw_nclx = nullptr;
        auto error = heif_image_handle_get_nclx_color_profile(handle, &raw_nclx);
        if (error.code == heif_error_Color_profile_does_not_exist) {
            error = heif_image_get_nclx_color_profile(decoded, &raw_nclx);
        }
        Owned<heif_color_profile_nclx, heif_nclx_color_profile_free> nclx{
            raw_nclx, heif_nclx_color_profile_free};
        if (error.code != heif_error_Color_profile_does_not_exist && failed(error, result)) {
            return false;
        }
        const cmsCIExyY white{0.3127, 0.3290, 1.0};
        const cmsCIExyYTRIPLE srgb{{0.64, 0.33, 1.0}, {0.30, 0.60, 1.0}, {0.15, 0.06, 1.0}};
        cmsCIExyY source_white = white;
        cmsCIExyYTRIPLE primaries = srgb;
        if (nclx) {
            transfer = nclx->transfer_characteristics;
            if (nclx->color_primaries != heif_color_primaries_unspecified) {
                source_white = {nclx->color_primary_white_x, nclx->color_primary_white_y, 1.0};
                primaries = {{nclx->color_primary_red_x, nclx->color_primary_red_y, 1.0},
                             {nclx->color_primary_green_x, nclx->color_primary_green_y, 1.0},
                             {nclx->color_primary_blue_x, nclx->color_primary_blue_y, 1.0}};
            }
        }
        srgb_source = (!nclx || nclx->color_primaries == heif_color_primaries_ITU_R_BT_709_5 ||
                       nclx->color_primaries == heif_color_primaries_unspecified) &&
                      (transfer == 2 || transfer == 13);
        switch (transfer) {
            case 1:
            case 2:
            case 4:
            case 5:
            case 6:
            case 8:
            case 13:
            case 14:
            case 15:
            case 16:
            case 18:
                break;
            default:
                result.error = "HEIF: unsupported NCLX transfer curve";
                result.problem = Problem::unknown_format;
                return false;
        }
        const Owned<cmsToneCurve, cmsFreeToneCurve> curve{cmsBuildGamma(nullptr, 1.0),
                                                          cmsFreeToneCurve};
        if (!curve) {
            result.error = "HEIF: cannot allocate colour curve";
            return false;
        }
        cmsToneCurve* curves[]{curve.get(), curve.get(), curve.get()};
        input.reset(cmsCreateRGBProfile(&source_white, &primaries, curves));
        output.reset(cmsCreateRGBProfile(&white, &srgb, curves));
    }
    if (!input || !output || cmsGetColorSpace(input.get()) != cmsSigRgbData) {
        result.error = "HEIF: invalid or unsupported RGB colour profile";
        return false;
    }
    const Owned<void, cmsDeleteTransform> transform{
        cmsCreateTransform(input.get(),
                           icc_size && eight_bit && !premultiplied ? TYPE_RGBA_8 : TYPE_RGBA_FLT,
                           output.get(), icc_size ? TYPE_RGBA_8 : TYPE_RGBA_FLT,
                           INTENT_RELATIVE_COLORIMETRIC, cmsFLAGS_COPY_ALPHA),
        cmsDeleteTransform};
    if (!transform) {
        result.error = "HEIF: cannot convert colour profile to sRGB";
        return false;
    }
    int stride = 0;
    const auto* pixels = heif_image_get_plane_readonly(decoded, heif_channel_interleaved, &stride);
    const int bits = heif_image_get_bits_per_pixel_range(decoded, heif_channel_interleaved);
    const std::size_t sample_bytes = eight_bit ? 1 : 2;
    const std::size_t row_bytes = std::size_t{image.width} * 4 * sample_bytes;
    if (pixels == nullptr || stride <= 0 || static_cast<std::size_t>(stride) < row_bytes ||
        bits < 8 || bits > 16) {
        result.error = "HEIF: invalid RGBA plane";
        return false;
    }
    const double maximum = static_cast<double>((1U << static_cast<unsigned>(bits)) - 1);
    std::vector<float> linear;
    if (!icc_size) {
        linear.resize(static_cast<std::size_t>(maximum) + 1);
        for (std::size_t i = 0; i < linear.size(); ++i) {
            linear[i] =
                static_cast<float>(linear_sample(static_cast<double>(i) / maximum, transfer));
        }
    }
    std::vector<float> row(std::size_t{image.width} * 4);
    for (std::size_t y = 0; y < image.height; ++y) {
        const auto* source = pixels + y * static_cast<std::size_t>(stride);
        auto* destination = image.rgba.data() + y * std::size_t{image.width} * 4;
        // Already sRGB (including untagged phone photos): preserve the bytes and
        // avoid a redundant float colour round trip on multi-megapixel grids.
        if (srgb_source && eight_bit && !premultiplied) {
            std::memcpy(destination, source, std::size_t{image.width} * 4);
            continue;
        }
        if (icc_size && eight_bit && !premultiplied) {
            cmsDoTransform(transform.get(), source, destination, image.width);
            continue;
        }
        for (std::size_t x = 0; x < image.width; ++x) {
            std::array<double, 4> rgba{};
            std::array<unsigned, 4> samples{};
            for (std::size_t c = 0; c < 4; ++c) {
                const auto offset = (x * 4 + c) * sample_bytes;
                samples[c] = source[offset];
                if (!eight_bit) {
                    samples[c] += static_cast<unsigned>(source[offset + 1]) << 8U;
                }
                samples[c] = std::min(samples[c], static_cast<unsigned>(maximum));
                rgba[c] = samples[c] / maximum;
            }
            for (std::size_t c = 0; c < 3; ++c) {
                if (premultiplied) {
                    rgba[c] = rgba[3] == 0 ? 0 : std::clamp(rgba[c] / rgba[3], 0.0, 1.0);
                }
                row[x * 4 + c] =
                    icc_size ? static_cast<float>(rgba[c])
                             : (premultiplied ? static_cast<float>(linear_sample(rgba[c], transfer))
                                              : linear[samples[c]]);
            }
            row[x * 4 + 3] = static_cast<float>(rgba[3]);
        }
        if (icc_size) {
            cmsDoTransform(transform.get(), row.data(), destination, image.width);
        } else {
            cmsDoTransform(transform.get(), row.data(), row.data(), image.width);
            for (std::size_t x = 0; x < image.width; ++x) {
                const std::size_t at = x * 4;
                // BT.2100 HLG at a fixed 1000-nit peak (system gamma 1.2), mapped to
                // 203-nit SDR white. No adaptive tone mapping; highlights/gamut clip.
                const double luminance =
                    std::max(0.0, 0.2126 * static_cast<double>(row[at]) +
                                      0.7152 * static_cast<double>(row[at + 1]) +
                                      0.0722 * static_cast<double>(row[at + 2]));
                const double scale =
                    transfer == 18 ? (1000.0 / 203.0) * std::pow(luminance, 0.2) : 1.0;
                for (std::size_t c = 0; c < 3; ++c) {
                    destination[at + c] = srgb_byte(static_cast<double>(row[at + c]) * scale);
                }
                destination[at + 3] = static_cast<std::uint8_t>(
                    std::lround(std::clamp(row[at + 3], 0.0F, 1.0F) * 255.0F));
            }
        }
    }
    return true;
}

}  // namespace

void decode_heif(std::span<const std::uint8_t> data, const DecodeOptions& options,
                 Decoded& result) {
    // First called by the helper's trusted warm-up fixture, before seccomp/Landlock. Keep
    // plugins loaded for the process lifetime: init/deinit per request would reopen files
    // inside the cage. libheif explicitly forbids deinit from a global destructor.
    static const heif_error initialized = heif_init(nullptr);
    if (failed(initialized, result)) {
        return;
    }
    const Owned<heif_context, heif_context_free> context{heif_context_alloc(), heif_context_free};
    if (!context) {
        result.error = "cannot start the HEIF decoder";
        return;
    }
    if (options.max_pixels == 0) {
        result.error = kTooLarge;
        return;
    }
    auto* limits = heif_context_get_security_limits(context.get());
    limits->max_image_size_pixels = std::min(limits->max_image_size_pixels, options.max_pixels);
    limits->max_color_profile_size = std::min(limits->max_color_profile_size, 4U * 1024 * 1024);
    // Other libheif security limits stay enabled, as do the helper's memory/time limits.
    heif_context_set_max_decoding_threads(context.get(), 0);
    if (failed(heif_context_read_from_memory_without_copy(context.get(), data.data(), data.size(),
                                                          nullptr),
               result)) {
        return;
    }
    heif_image_handle* raw_handle = nullptr;
    const heif_error handle_error =
        heif_context_get_primary_image_handle(context.get(), &raw_handle);
    const Owned<heif_image_handle, heif_image_handle_release> handle{raw_handle,
                                                                     heif_image_handle_release};
    if (failed(handle_error, result)) {
        return;
    }
    if (!check_size(heif_image_handle_get_width(handle.get()),
                    heif_image_handle_get_height(handle.get()), options, result) ||
        !check_size(heif_image_handle_get_ispe_width(handle.get()),
                    heif_image_handle_get_ispe_height(handle.get()), options, result)) {
        return;
    }
    const Owned<heif_decoding_options, heif_decoding_options_free> settings{
        heif_decoding_options_alloc(), heif_decoding_options_free};
    if (!settings) {
        result.error = "cannot allocate HEIF decoding options";
        return;
    }
    settings->strict_decoding = 1;
    // libheif maps 0 to the codec default (one background worker for libde265).
    // libde265 treats non-positive worker counts as synchronous; -1 passes through
    // libheif without selecting its default. Pin that decoder so this convention
    // is never sent to an unrelated codec. The sandbox tests guard this contract.
    settings->decoder_id = "libde265";
    settings->num_codec_threads = -1;
    settings->convert_hdr_to_8bit = 0;
    settings->output_image_nclx_profile_passthrough = 1;
    // Always apply HEIF irot/imir/clap together, including when the caller defers EXIF
    // orientation until after scaling. Never also apply the HEIF file's EXIF orientation.
    settings->ignore_transformations = 0;
    const auto chroma = heif_image_handle_get_luma_bits_per_pixel(handle.get()) <= 8 &&
                                heif_image_handle_get_chroma_bits_per_pixel(handle.get()) <= 8
                            ? heif_chroma_interleaved_RGBA
                            : heif_chroma_interleaved_RRGGBBAA_LE;
    heif_image* raw_image = nullptr;
    const heif_error decode_error =
        heif_decode_image(handle.get(), &raw_image, heif_colorspace_RGB, chroma, settings.get());
    const Owned<heif_image, heif_image_release> decoded{raw_image, heif_image_release};
    if (failed(decode_error, result)) {
        return;
    }
    const int width = heif_image_get_primary_width(decoded.get());
    const int height = heif_image_get_primary_height(decoded.get());
    if (!check_size(width, height, options, result)) {
        return;
    }
    Image& image = result.image;
    image.width = static_cast<std::uint32_t>(width);
    image.height = static_cast<std::uint32_t>(height);
    image.rgba.resize(std::size_t{image.width} * image.height * 4);
    static_cast<void>(convert_colour(handle.get(), decoded.get(), image, result));
}

}  // namespace rummage::formats::image::detail
