#include "develop.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <span>
#include <vector>

#include "colour.hpp"
#include "raw.hpp"

#if defined(__SSE__)
#  include <xmmintrin.h>
#endif

namespace nordend::formats::image::dng {
namespace {

constexpr std::size_t kCurveSize = 16384;  // linear 0-1 in steps of 1/16383

// The larger and the smaller of two floats, without a branch (with SSE one instruction; GCC
// makes std::max and std::min of floats into compares and jumps, which the data mispredicts).
inline float larger(float a, float b) {
#if defined(__SSE__)
    // NOLINTNEXTLINE(portability-simd-intrinsics): with a portable version below
    return _mm_cvtss_f32(_mm_max_ss(_mm_set_ss(a), _mm_set_ss(b)));
#else
    return a > b ? a : b;
#endif
}

inline float smaller(float a, float b) {
#if defined(__SSE__)
    // NOLINTNEXTLINE(portability-simd-intrinsics): with a portable version below
    return _mm_cvtss_f32(_mm_min_ss(_mm_set_ss(a), _mm_set_ss(b)));
#else
    return a < b ? a : b;
#endif
}

// v between low and high (low for NaN). With SSE one instruction each way: GCC makes std::min and
// std::max of floats into compares and masks (a dozen instructions), for NaN's sake.
inline float within(float v, float low, float high) {
#if defined(__SSE__)
    // NOLINTNEXTLINE(portability-simd-intrinsics): with a portable version below
    return _mm_cvtss_f32(_mm_min_ss(_mm_max_ss(_mm_set_ss(v), _mm_set_ss(low)), _mm_set_ss(high)));
#else
    return v > low ? (v < high ? v : high) : low;
#endif
}

// The table index nearest to v, within 0 to 16383: through a 32-bit integer (one instruction; an
// unsigned 64-bit one takes ten, NaN gives the lowest integer) limited as an integer (no
// branch). Adding a half rounds right where it matters, from 0 up.
inline std::size_t index_of(float v) {
    const auto i = static_cast<std::int32_t>(v + 0.5F);  // NOLINT(bugprone-incorrect-roundings)
    return static_cast<std::size_t>(std::clamp(i, 0, static_cast<std::int32_t>(kCurveSize - 1)));
}

double srgb_encode(double linear) {
    return linear <= 0.0031308 ? linear * 12.92 : (1.055 * std::pow(linear, 1.0 / 2.4)) - 0.055;
}

// Nordend's light curve, on sRGB-encoded values: a little more contrast in the middle, ends
// untouched (to be settled by eye next to darktable).
double light_curve(double v) {
    constexpr double strength = 0.3;
    const double s = (3.0 * v * v) - (2.0 * v * v * v);
    return v + (strength * (s - v));
}

// The tone curve's value at x, between its points.
double tone_at(const std::vector<std::array<double, 2>>& curve, double x) {
    const auto upper = std::ranges::lower_bound(curve, x, {}, [](const auto& p) { return p[0]; });
    if (upper == curve.begin()) {
        return curve.front()[1];
    }
    if (upper == curve.end()) {
        return curve.back()[1];
    }
    const auto& b = *upper;
    const auto& a = *(upper - 1);
    const double t = (x - a[0]) / (b[0] - a[0]);
    return a[1] + (t * (b[1] - a[1]));
}

double srgb_decode(double encoded) {
    return encoded <= 0.04045 ? encoded / 12.92 : std::pow((encoded + 0.055) / 1.055, 2.4);
}

// The look's tone curve, linear light to linear light, both in steps of 1/16383 (the table's
// index and values): the file's ProfileToneCurve, else Nordend's light curve.
std::vector<float> tone_table(const Colour& colour) {
    std::vector<float> out(kCurveSize);
    for (std::size_t i = 0; i < kCurveSize; ++i) {
        const double v = static_cast<double>(i) / static_cast<double>(kCurveSize - 1);
        const double toned = colour.tone_curve.empty() ? srgb_decode(light_curve(srgb_encode(v)))
                                                       : tone_at(colour.tone_curve, v);
        out[i] = static_cast<float>(std::clamp(toned, 0.0, 1.0) * (kCurveSize - 1));
    }
    return out;
}

// Linear light in steps of 1/16383 to 8-bit sRGB.
std::vector<std::uint8_t> encode_table() {
    std::vector<std::uint8_t> out(kCurveSize);
    for (std::size_t i = 0; i < kCurveSize; ++i) {
        const double v = srgb_encode(static_cast<double>(i) / static_cast<double>(kCurveSize - 1));
        out[i] = static_cast<std::uint8_t>(std::lround(std::clamp(v, 0.0, 1.0) * 255.0));
    }
    return out;
}

// Where a block goes when blocks are averaged by area into fewer outputs: block i covers [i,
// i + 1), output o covers [o r, (o + 1) r) with r = blocks / outputs (1 or more), so a block
// lands in at most two: share first of it in to, the rest in to + 1.
struct Share {
    std::uint32_t to{0};
    float first{1.0F};
    float second{0.0F};
};

std::vector<Share> shares(std::uint32_t blocks, std::uint32_t outputs) {
    std::vector<Share> out(blocks);
    const double ratio = static_cast<double>(blocks) / static_cast<double>(outputs);
    for (std::uint32_t i = 0; i < blocks; ++i) {
        const auto to = std::min(outputs - 1, static_cast<std::uint32_t>(i / ratio));
        double first = std::min(1.0, ((to + 1) * ratio) - i);
        if (to + 1 >= outputs || first <= 0.0) {
            first = 1.0;
        }
        out[i] = {.to = to,
                  .first = static_cast<float>(first),
                  .second = static_cast<float>(1.0 - first)};
    }
    return out;
}

bool fail(std::string& error, Problem& problem, Problem kind, std::string why) {
    error = std::move(why);
    problem = kind;
    return false;
}

}  // namespace

std::optional<Developed> develop(const tiff::Container& container, const DevelopOptions& options,
                                 std::string& error, Problem& problem) {
    const std::optional<Raw> found = find_raw(container, error, problem);
    if (!found) {
        return std::nullopt;
    }
    const Raw& raw = *found;
    // A 2x2 pattern of red, two greens and blue; or a linear raw of three colours.
    std::array<std::size_t, 4> site_colour{};  // of the 2x2 block's sites, row by row
    if (raw.cfa) {
        std::array<int, 3> counts{};
        for (std::size_t i = 0; i < 4; ++i) {
            const std::uint8_t c = raw.pattern.at(i);
            if (c > 2) {
                break;
            }
            site_colour.at(i) = c;
            ++counts.at(c);
        }
        if (raw.pattern_width != 2 || raw.pattern_height != 2 || counts != std::array{1, 2, 1}) {
            fail(error, problem, Problem::unknown_format,
                 "a DNG whose colour filter is not a 2x2 Bayer pattern");
            return std::nullopt;
        }
    } else if (raw.samples != 3) {
        fail(error, problem, Problem::unknown_format,
             "a linear DNG of " + std::to_string(raw.samples) + " colours");
        return std::nullopt;
    }
    const std::vector<tiff::Ifd> chain = container.chain(1);
    const std::optional<Colour> colour = colour_of(chain.front(), error);
    if (!colour) {
        problem = Problem::unknown_format;
        return std::nullopt;
    }

    // The default crop from an even offset in the active area (so that the pattern's phase is
    // the active area's), in blocks of 2x2 sites (one pixel at half resolution), averaged by area
    // into the size asked (as picture::scale_to_fit would, but on linear light, and the colour
    // then worked out for the pixels shown only).
    const std::uint32_t top = raw.active[0];
    const std::uint32_t left = raw.active[1];
    const auto crop_x = static_cast<std::uint32_t>(raw.crop[0]);
    const auto crop_y = static_cast<std::uint32_t>(raw.crop[1]);
    const auto crop_width = static_cast<std::uint32_t>(raw.crop[2]);
    const auto crop_height = static_cast<std::uint32_t>(raw.crop[3]);
    const std::uint32_t x0 = left + (crop_x & ~1U);
    const std::uint32_t y0 = top + (crop_y & ~1U);
    const std::uint32_t blocks_across = crop_width / 2;
    const std::uint32_t blocks_down = crop_height / 2;
    if (blocks_across == 0 || blocks_down == 0) {
        fail(error, problem, Problem::damaged, "a DNG with an empty crop");
        return std::nullopt;
    }
    // Fitted into the box as scale_to_fit fits a picture: proportions kept, never enlarged.
    std::uint32_t width = blocks_across;
    std::uint32_t height = blocks_down;
    if (options.max_width > 0 && options.max_height > 0 &&
        (width > options.max_width || height > options.max_height)) {
        const double fit = std::min(static_cast<double>(options.max_width) / blocks_across,
                                    static_cast<double>(options.max_height) / blocks_down);
        width = std::max(1U, static_cast<std::uint32_t>(std::lround(blocks_across * fit)));
        height = std::max(1U, static_cast<std::uint32_t>(std::lround(blocks_down * fit)));
    }
    const std::vector<Share> columns = shares(blocks_across, width);
    const std::vector<Share> rows_into = shares(blocks_down, height);
    // The blocks an output pixel averages, in area.
    const auto area = static_cast<float>((static_cast<double>(blocks_across) / width) *
                                         (static_cast<double>(blocks_down) / height));

    // Black and the scale to 0-1 of each site, worked out site by site (the general case).
    const std::size_t samples = raw.samples;
    const auto black_at = [&](std::uint32_t row, std::uint32_t column, std::size_t sample) {
        const std::uint32_t r = row - top;
        const std::uint32_t c = column - left;
        double b = raw.black.at(
            (((std::size_t{r % raw.black_rows} * raw.black_columns) + (c % raw.black_columns)) *
             samples) +
            sample);
        if (!raw.black_delta_h.empty()) {
            b += raw.black_delta_h.at(c);
        }
        if (!raw.black_delta_v.empty()) {
            b += raw.black_delta_v.at(r);
        }
        return b;
    };

    // White balance (clipped at 1, so that a sensor full in every colour stays white), then the
    // colour matrix with the neutral and the exposure in it.
    std::array<float, 3> balance{};
    for (std::size_t c = 0; c < 3; ++c) {
        balance.at(c) = static_cast<float>(1.0 / colour->neutral.at(c));
    }
    const double gain = options.look ? colour->exposure : 1.0;
    std::array<float, 9> matrix{};
    for (std::size_t r = 0; r < 3; ++r) {
        for (std::size_t c = 0; c < 3; ++c) {
            matrix.at((r * 3) + c) = static_cast<float>(colour->camera_to_srgb.at((r * 3) + c) *
                                                        colour->neutral.at(c) * gain);
        }
    }
    const std::vector<float> tone = options.look ? tone_table(*colour) : std::vector<float>{};
    const std::vector<std::uint8_t> encode = encode_table();
    constexpr auto last = static_cast<float>(kCurveSize - 1);

    Developed out;
    out.image.width = width;
    out.image.height = height;
    out.image.rgba.resize(std::size_t{width} * height * 4);
    out.full_width = crop_width;
    out.full_height = crop_height;
    const std::uint64_t orientation = chain.front().number(tiff::tag::kOrientation).value_or(1);
    out.orientation =
        orientation >= 1 && orientation <= 8 ? static_cast<std::uint8_t>(orientation) : 1;

    // For the usual case: each site's repeating black, and where red, the greens and blue are.
    std::array<float, 4> site_black{};
    std::size_t red_site = 0;
    std::size_t blue_site = 0;
    std::array<std::size_t, 2> green_sites{};
    const bool repeat_fits = 2 % raw.black_rows == 0 && 2 % raw.black_columns == 0;
    for (std::size_t site = 0, greens = 0; site < 4; ++site) {
        const std::size_t r = (y0 - top + (site / 2)) % raw.black_rows;
        const std::size_t c = (x0 - left + (site % 2)) % raw.black_columns;
        site_black.at(site) =
            raw.cfa ? static_cast<float>(raw.black.at((r * raw.black_columns) + c)) : 0.0F;
        if (site_colour.at(site) == 0) {
            red_site = site;
        } else if (site_colour.at(site) == 2) {
            blue_site = site;
        } else if (greens < 2) {
            green_sites.at(greens++) = site;
        }
    }
    const auto white = static_cast<float>(raw.white[0]);
    const std::vector<float> delta_h(raw.black_delta_h.begin(), raw.black_delta_h.end());
    const std::vector<float> delta_v(raw.black_delta_v.begin(), raw.black_delta_v.end());

    // The camera values of one row of blocks, averaged across into the output's width (and a
    // pixel more, never shown: the last block's empty second share lands there).
    const bool same_size = width == blocks_across && height == blocks_down;
    std::vector<float> sums((std::size_t{width} + 1) * 3);
    // Puts a block's camera values where they go: straight when the picture is not reduced, else
    // shared between the one or two output pixels it covers (no branch: the second share may be
    // 0).
    const auto add = [&](std::uint32_t x, float red, float green, float blue) {
        if (same_size) {
            float* to = sums.data() + (std::size_t{x} * 3);
            to[0] = red;
            to[1] = green;
            to[2] = blue;
            return;
        }
        const Share& share = columns[x];  // NOLINT: x < blocks_across
        float* to = sums.data() + (std::size_t{share.to} * 3);
        to[0] += red * share.first;
        to[1] += green * share.first;
        to[2] += blue * share.first;
        to[3] += red * share.second;
        to[4] += green * share.second;
        to[5] += blue * share.second;
    };
    // Adds the blocks of rows row and row + 1 (a and b) to sums.
    const auto add_blocks = [&](std::uint32_t row, std::span<const std::uint16_t> a,
                                std::span<const std::uint16_t> b) {
        if (raw.cfa && repeat_fits) {
            // The usual case: the repeating black is the same in every block; the black of a
            // row (BlackLevelDeltaV) is added here, of a column (BlackLevelDeltaH) block by block.
            const float upper_delta = delta_v.empty() ? 0.0F : delta_v[row - top];
            const float lower_delta = delta_v.empty() ? 0.0F : delta_v[row + 1 - top];
            const std::array<float, 4> black{
                site_black[0] + upper_delta, site_black[1] + upper_delta,
                site_black[2] + lower_delta, site_black[3] + lower_delta};
            std::array<float, 4> scale{};
            for (std::size_t i = 0; i < 4; ++i) {
                scale.at(i) = black.at(i) < white ? 1.0F / (white - black.at(i)) : 0.0F;
            }
            const std::uint16_t* top_row = a.data() + x0;
            const std::uint16_t* bottom_row = b.data() + x0;
            const float* column_delta = delta_h.empty() ? nullptr : delta_h.data() + (x0 - left);
            for (std::uint32_t x = 0; x < blocks_across; ++x, top_row += 2, bottom_row += 2) {
                std::array<float, 4> v{
                    static_cast<float>(top_row[0]), static_cast<float>(top_row[1]),
                    static_cast<float>(bottom_row[0]), static_cast<float>(bottom_row[1])};
                if (column_delta == nullptr) {
                    for (std::size_t i = 0; i < 4; ++i) {
                        v.at(i) = (v.at(i) - black.at(i)) * scale.at(i);
                    }
                } else {
                    for (std::size_t i = 0; i < 4; ++i) {
                        const float level =
                            black.at(i) + column_delta[(std::size_t{2} * x) + (i % 2)];
                        v.at(i) = level < white ? (v.at(i) - level) / (white - level) : 0.0F;
                    }
                }
                add(x, v[red_site], (v[green_sites[0]] + v[green_sites[1]]) * 0.5F,  // NOLINT
                    v[blue_site]);  // NOLINT: sites < 4
            }
            return;
        }
        for (std::uint32_t x = 0; x < blocks_across; ++x) {
            const std::uint32_t column = x0 + (2 * x);
            const std::size_t at = std::size_t{column} * samples;
            std::array<float, 3> camera{};
            const std::array<const std::uint16_t*, 4> sites{a.data() + at, a.data() + at + samples,
                                                            b.data() + at, b.data() + at + samples};
            for (std::size_t site = 0; site < 4; ++site) {
                for (std::size_t s = 0; s < samples; ++s) {
                    const double level = black_at(row + static_cast<std::uint32_t>(site / 2),
                                                  column + static_cast<std::uint32_t>(site % 2), s);
                    const double range = raw.white.at(s) - level;
                    const double v = range > 0.0
                                         ? (static_cast<double>(sites.at(site)[s]) - level) / range
                                         : 0.0;  // NOLINT: sites within the row
                    camera.at(raw.cfa ? site_colour.at(site) : s) += static_cast<float>(v);
                }
            }
            const float one = raw.cfa ? 1.0F : 0.25F;  // a CFA's block has one red, one blue
            add(x, camera[0] * one, camera[1] * (raw.cfa ? 0.5F : 0.25F), camera[2] * one);
        }
    };
    // Output rows still being summed (at most two: a row of blocks lands in one or two of them).
    std::array<std::vector<float>, 2> open{std::vector<float>(same_size ? 0 : sums.size()),
                                           std::vector<float>(same_size ? 0 : sums.size())};
    // The white balance and the average over the blocks in one factor per colour.
    std::array<float, 3> factor{};
    for (std::size_t c = 0; c < 3; ++c) {
        factor.at(c) = balance.at(c) / area;
    }
    const std::array<float, 9> to_index = [&] {
        std::array<float, 9> scaled{};
        for (std::size_t i = 0; i < 9; ++i) {
            scaled.at(i) = matrix.at(i) * last;
        }
        return scaled;
    }();
    // Turns the sums of output row y into its pixels, and empties them.
    const auto finish_row = [&](std::uint32_t y) {
        std::uint8_t* pixel = out.image.rgba.data() + (std::size_t{y} * width * 4);
        float* sum = same_size ? sums.data() : open.at(y % 2).data();
        for (std::uint32_t x = 0; x < width; ++x, pixel += 4, sum += 3) {
            const float red = within(sum[0] * factor[0], 0.0F, 1.0F);
            const float green = within(sum[1] * factor[1], 0.0F, 1.0F);
            const float blue = within(sum[2] * factor[2], 0.0F, 1.0F);
            sum[0] = 0.0F;
            sum[1] = 0.0F;
            sum[2] = 0.0F;
            std::array<float, 3> v{};
            for (std::size_t c = 0; c < 3; ++c) {
                v.at(c) = (to_index.at(c * 3) * red) + (to_index.at((c * 3) + 1) * green) +
                          (to_index.at((c * 3) + 2) * blue);
            }
            if (!tone.empty()) {
                // As the DNG SDK applies a tone curve (RefBaselineRGBTone): to the largest and
                // the smallest colour, the middle one kept between them where it was, so that
                // the hue stays (a curve on each colour alone would saturate). Written as one
                // line through the two for all three colours: no sorting, no branch.
                const float high = within(larger(v[0], larger(v[1], v[2])), 0.0F, last);
                const float low = within(smaller(v[0], smaller(v[1], v[2])), 0.0F, high);
                const float toned_high = tone[index_of(high)];  // NOLINT: within the table
                const float toned_low = tone[index_of(low)];    // NOLINT: within the table
                // Equal colours give an equal pair, 0 over the least denominator: no branch.
                const float slope = (toned_high - toned_low) / larger(high - low, 1e-6F);
                for (float& c : v) {
                    c = toned_low + (slope * (c - low));
                }
            }
            for (std::size_t c = 0; c < 3; ++c) {
                pixel[c] = encode[index_of(v.at(c))];  // NOLINT: within the table
            }
            pixel[3] = 255;
        }
    };

    const std::size_t row_values = std::size_t{raw.width} * samples;
    std::vector<std::uint16_t> upper(row_values);
    const std::uint32_t rows_used = 2 * blocks_down;
    std::uint32_t block_rows = 0;  // read
    std::uint32_t finished = 0;    // output rows made
    // Adds the row of blocks in sums to the output rows it lands in, finishing those above.
    const auto add_row = [&](std::uint32_t block_row) {
        ++block_rows;
        if (same_size) {
            finish_row(finished++);  // straight from sums
            return;
        }
        const Share& share = rows_into[block_row];  // NOLINT: block_row < blocks_down
        while (finished < share.to) {
            finish_row(finished++);
        }
        float* first = open.at(share.to % 2).data();
        for (std::size_t i = 0; i < sums.size(); ++i) {
            first[i] += sums[i] * share.first;
        }
        if (share.second > 0.0F) {
            float* second = open.at((share.to + 1) % 2).data();
            for (std::size_t i = 0; i < sums.size(); ++i) {
                second[i] += sums[i] * share.second;
            }
        }
        std::ranges::fill(sums, 0.0F);
    };
    const bool read = read_rows(
        raw,
        [&](std::uint32_t first, std::uint32_t rows, std::span<const std::uint16_t> values) {
            for (std::uint32_t r = 0; r < rows; ++r) {
                const std::uint32_t row = first + r;
                if (row < y0) {
                    continue;
                }
                if (row >= y0 + rows_used) {
                    return false;
                }
                const std::span<const std::uint16_t> current =
                    values.subspan(std::size_t{r} * row_values, row_values);
                if ((row - y0) % 2 == 0) {
                    if (r + 1 == rows) {
                        std::ranges::copy(current, upper.begin());  // its pair is in the next band
                    }
                    continue;
                }
                add_blocks(row - 1,
                           r == 0 ? std::span<const std::uint16_t>{upper}
                                  : values.subspan(std::size_t{r - 1} * row_values, row_values),
                           current);
                add_row((row - y0) / 2);
            }
            return true;
        },
        error);
    if (read && block_rows == blocks_down) {
        while (finished < height) {
            finish_row(finished++);
        }
    }
    if (!read || block_rows != blocks_down) {
        if (error.empty()) {
            error = "a DNG whose rows ended early";
        }
        problem = Problem::damaged;
        return std::nullopt;
    }
    problem = Problem::none;
    return out;
}

}  // namespace nordend::formats::image::dng
