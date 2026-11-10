#include <type_traits>

#include "internal.hpp"

namespace nordend::svg {
namespace {
using Tag = std::remove_pointer_t<decltype(FT_Outline::tags)>;
using Contour = std::remove_pointer_t<decltype(FT_Outline::contours)>;

FT_Pos fixed(double n) {
    require(std::isfinite(n) && std::abs(n) <= 32767);
    return static_cast<FT_Pos>(std::lround(n * 64));
}

FT_Vector vector(Point p) { return {fixed(p.x), fixed(p.y)}; }

struct Outline {
    std::vector<FT_Vector> points;
    std::vector<Tag> tags;
    std::vector<Contour> contours;

    void point(Point p, Tag tag) {
        require(points.size() < 32767);
        points.push_back(vector(p));
        tags.push_back(tag);
    }

    void close() {
        if (!points.empty() && (contours.empty() || contours.back() != points.size() - 1)) {
            contours.push_back(static_cast<Contour>(points.size() - 1));
        }
    }

    FT_Outline get(bool evenodd) {
        return {static_cast<decltype(FT_Outline::n_contours)>(contours.size()),
                static_cast<decltype(FT_Outline::n_points)>(points.size()),
                points.data(),
                tags.data(),
                contours.data(),
                FT_OUTLINE_OVERLAP | (evenodd ? FT_OUTLINE_EVEN_ODD_FILL : 0)};
    }
};

struct Spans {
    std::vector<std::uint16_t>& coverage;
    int left, top, width, height;
};

void spans(int y, int count, const FT_Span* data, void* user) {
    auto& ctx = *static_cast<Spans*>(user);
    const int row = y / 4 - ctx.top;
    if (row < 0 || row >= ctx.height) {
        return;
    }
    for (int i = 0; i < count; ++i) {
        int x = data[i].x;
        const int end = x + data[i].len;
        while (x < end) {
            const int next = std::min(end, (x / 4 + 1) * 4);
            const int column = x / 4 - ctx.left;
            if (column >= 0 && column < ctx.width) {
                const auto at = static_cast<std::size_t>(row) * static_cast<unsigned>(ctx.width) +
                                static_cast<unsigned>(column);
                ctx.coverage[at] =
                    static_cast<std::uint16_t>(ctx.coverage[at] + (next - x) * data[i].coverage);
            }
            x = next;
        }
    }
}

void render(FT_Library library, Canvas& canvas, FT_Outline& outline, const Paint& paint,
            double opacity) {
    if (outline.n_points == 0) {
        return;
    }
    FT_BBox box{};
    FT_Outline_Get_CBox(&outline, &box);
    const int left = std::max(0, static_cast<int>(std::floor(static_cast<double>(box.xMin) / 64)));
    const int top = std::max(0, static_cast<int>(std::floor(static_cast<double>(box.yMin) / 64)));
    const int right = std::min(static_cast<int>(canvas.image.width),
                               static_cast<int>(std::ceil(static_cast<double>(box.xMax) / 64)));
    const int bottom = std::min(static_cast<int>(canvas.image.height),
                                static_cast<int>(std::ceil(static_cast<double>(box.yMax) / 64)));
    if (right <= left || bottom <= top) {
        return;
    }
    const int width = right - left, height = bottom - top;
    std::vector<std::uint16_t> coverage(static_cast<std::size_t>(width) *
                                        static_cast<unsigned>(height));
    Spans context{coverage, left, top, width, height};
    for (int i = 0; i < outline.n_points; ++i) {
        outline.points[i].x *= 4;
        outline.points[i].y *= 4;
    }
    FT_Raster_Params params{};
    params.flags = FT_RASTER_FLAG_AA | FT_RASTER_FLAG_DIRECT | FT_RASTER_FLAG_CLIP;
    params.gray_spans = spans;
    params.user = &context;
    params.clip_box = {left * 4, top * 4, right * 4, bottom * 4};
    require(FT_Outline_Render(library, &outline, &params) == 0);
    for (int y = 0; y < height; ++y) {
        int x = 0;
        while (x < width) {
            const auto row = static_cast<std::size_t>(y) * static_cast<unsigned>(width);
            const unsigned value = (coverage[row + static_cast<unsigned>(x)] + 8U) / 16U;
            int end = x + 1;
            while (end < width &&
                   (coverage[row + static_cast<unsigned>(end)] + 8U) / 16U == value) {
                ++end;
            }
            if (value > 0) {
                composite(canvas, left + x, top + y, end - x, value, paint, opacity);
            }
            x = end;
        }
    }
    require(!canvas.exhausted);
}

// Flatten only for dash lengths. Undashed curves go directly to FreeType's stroker.
void flatten(std::vector<Point>& out, Point a, Point b, Point c, Point d, double tolerance,
             int depth = 0) {
    require(out.size() < kMaxSegments);
    Point chord = d - a;
    const double len = std::hypot(chord.x, chord.y);
    const double error =
        len > 1e-20 ? std::max(std::abs(chord.x * (b.y - a.y) - chord.y * (b.x - a.x)),
                               std::abs(chord.x * (c.y - a.y) - chord.y * (c.x - a.x))) /
                          len
                    : std::max(std::hypot(b.x - a.x, b.y - a.y), std::hypot(c.x - a.x, c.y - a.y));
    if (error <= tolerance || depth >= 20) {
        out.push_back(d);
        return;
    }
    Point ab = (a + b) * .5, bc = (b + c) * .5, cd = (c + d) * .5, abc = (ab + bc) * .5,
          bcd = (bc + cd) * .5, mid = (abc + bcd) * .5;
    flatten(out, a, ab, abc, mid, tolerance, depth + 1);
    flatten(out, mid, bcd, cd, d, tolerance, depth + 1);
}

Path dashed(const Path& path, std::string_view pattern, double offset, double extent,
            double scale) {
    Numbers input{pattern};
    std::vector<double> lengths;
    while (input.more()) {
        const auto start = input.at;
        input.get();
        while (
            input.at < pattern.size() &&
            ((pattern[input.at] >= 'a' && pattern[input.at] <= 'z') || pattern[input.at] == '%')) {
            ++input.at;
        }
        const double value = length(pattern.substr(start, input.at - start), extent);
        require(value >= 0);
        lengths.push_back(value);
    }
    if (lengths.empty()) {
        return path;
    }
    if (lengths.size() % 2 != 0) {
        const auto copy = lengths;
        lengths.insert(lengths.end(), copy.begin(), copy.end());
    }
    double total = 0;
    for (double value : lengths) {
        total += value;
    }
    require(std::isfinite(total));
    if (total <= 0) {
        return path;
    }
    Path result;
    Point current{}, start{};
    std::size_t dash = 0;
    double remaining = 0;
    bool open = false;
    std::size_t work = 0;
    auto append = [&](char kind, Point p) {
        require(++work <= kMaxSegments);
        result.segments.push_back({kind, p, {}, {}});
    };
    auto advance = [&]() {
        for (std::size_t i = 0; i <= lengths.size(); ++i) {
            dash = (dash + 1) % lengths.size();
            remaining = lengths[dash];
            if (remaining > 0) {
                return;
            }
        }
        throw Failure{};
    };
    auto reset = [&]() {
        dash = 0;
        double phase = std::fmod(offset, total);
        if (phase < 0) {
            phase += total;
        }
        remaining = lengths[0];
        while (phase >= remaining) {
            phase -= remaining;
            advance();
        }
        remaining -= phase;
        open = false;
    };
    auto line = [&](Point end) {
        double distance = std::hypot(end.x - current.x, end.y - current.y);
        require(std::isfinite(distance));
        if (distance <= 1e-15) {
            return;
        }
        Point unit = (end - current) * (1 / distance);
        while (distance > 1e-12) {
            require(++work <= kMaxSegments);
            if (remaining <= 1e-12) {
                advance();
                open = false;
            }
            const double step = std::min(distance, remaining);
            Point next = step >= distance ? end : current + unit * step;
            if (dash % 2 == 0) {
                if (!open) {
                    append('M', current);
                }
                append('L', next);
                open = true;
            } else {
                open = false;
            }
            current = next;
            distance -= step;
            remaining -= step;
        }
        current = end;
    };
    std::size_t subpath_begin = 0;
    for (const auto& segment : path.segments) {
        if (segment.kind == 'M') {
            subpath_begin = result.segments.size();
            current = start = segment.p;
            reset();
        } else if (segment.kind == 'L') {
            line(segment.p);
        } else if (segment.kind == 'C') {
            std::vector<Point> poly;
            flatten(poly, current, segment.c1, segment.c2, segment.p, .03 / std::max(scale, 1e-9));
            for (auto p : poly) {
                line(p);
            }
        } else if (segment.kind == 'Z') {
            line(start);
            if (result.segments.size() > subpath_begin + 1 &&
                result.segments[subpath_begin].p.x == start.x &&
                result.segments[subpath_begin].p.y == start.y &&
                result.segments.back().p.x == start.x && result.segments.back().p.y == start.y) {
                std::size_t last = result.segments.size() - 1;
                while (last > subpath_begin && result.segments[last].kind != 'M') {
                    --last;
                }
                if (last == subpath_begin) {
                    append('Z', {});
                } else {
                    std::size_t first_end = subpath_begin + 1;
                    while (first_end < last && result.segments[first_end].kind != 'M') {
                        ++first_end;
                    }
                    std::vector<Segment> joined(
                        result.segments.begin() + static_cast<std::ptrdiff_t>(last),
                        result.segments.end());
                    joined.insert(
                        joined.end(),
                        result.segments.begin() + static_cast<std::ptrdiff_t>(subpath_begin + 1),
                        result.segments.begin() + static_cast<std::ptrdiff_t>(first_end));
                    joined.insert(joined.end(),
                                  result.segments.begin() + static_cast<std::ptrdiff_t>(first_end),
                                  result.segments.begin() + static_cast<std::ptrdiff_t>(last));
                    result.segments.resize(subpath_begin);
                    result.segments.insert(result.segments.end(), joined.begin(), joined.end());
                }
            }
        }
    }
    return result;
}
}  // namespace

Raster::Raster() {
    require(FT_Init_FreeType(&library) == 0);
    if (FT_Stroker_New(library, &stroker) != 0) {
        FT_Done_FreeType(library);
        library = nullptr;
        throw Failure{};
    }
}

Raster::~Raster() {
    FT_Stroker_Done(stroker);
    FT_Done_FreeType(library);
}

void Raster::draw(Canvas& canvas, const Path& source, const Matrix& matrix, const Paint& paint,
                  double opacity, bool evenodd, const Style* stroke, double width, double height) {
    if (source.segments.empty() || opacity <= 0 || (paint.stops.empty() && paint.solid[3] == 0)) {
        return;
    }
    if (!stroke) {
        Outline result;
        for (const auto& s : source.segments) {
            if (s.kind == 'M') {
                result.close();
                result.point(matrix.apply(s.p), FT_CURVE_TAG_ON);
            } else if (s.kind == 'L') {
                result.point(matrix.apply(s.p), FT_CURVE_TAG_ON);
            } else if (s.kind == 'C') {
                result.point(matrix.apply(s.c1), FT_CURVE_TAG_CUBIC);
                result.point(matrix.apply(s.c2), FT_CURVE_TAG_CUBIC);
                result.point(matrix.apply(s.p), FT_CURVE_TAG_ON);
            } else if (s.kind == 'Z') {
                result.close();
            }
        }
        result.close();
        auto outline = result.get(evenodd);
        render(library, canvas, outline, paint, opacity);
        return;
    }
    const double extent = std::hypot(width, height) / std::sqrt(2.0),
                 line_width = length(stroke->stroke_width, extent, 1);
    require(line_width >= 0);
    if (line_width == 0) {
        return;
    }
    const double scale = std::max(std::hypot(matrix.a, matrix.b), std::hypot(matrix.c, matrix.d));
    if (scale <= 1e-15) {
        return;
    }
    require(std::isfinite(scale));
    const auto box = bounds(source);
    const Point origin{box.x, box.y};
    const Path dash =
        stroke->dasharray != "none"
            ? dashed(source, stroke->dasharray, length(stroke->dashoffset, extent), extent, scale)
            : Path{};
    const Path& path = stroke->dasharray != "none" ? dash : source;
    auto convert = [&](Point p) { return vector((p - origin) * scale); };
    require(stroke->miterlimit <= 32767);
    const auto cap = stroke->linecap == "round"    ? FT_STROKER_LINECAP_ROUND
                     : stroke->linecap == "square" ? FT_STROKER_LINECAP_SQUARE
                                                   : FT_STROKER_LINECAP_BUTT;
    const auto join = stroke->linejoin == "round"   ? FT_STROKER_LINEJOIN_ROUND
                      : stroke->linejoin == "bevel" ? FT_STROKER_LINEJOIN_BEVEL
                                                    : FT_STROKER_LINEJOIN_MITER_FIXED;
    const auto radius = fixed(line_width * scale / 2);
    if (radius == 0)
        return;
    FT_Stroker_Set(stroker, radius, cap, join,
                   static_cast<FT_Fixed>(std::lround(stroke->miterlimit * 65536)));
    FT_Stroker_Rewind(stroker);
    bool active = false;
    for (std::size_t i = 0; i < path.segments.size(); ++i) {
        const auto& s = path.segments[i];
        if (s.kind == 'M') {
            if (active) {
                require(FT_Stroker_EndSubPath(stroker) == 0);
            }
            bool has_edge = false;
            FT_Vector previous = convert(s.p);
            std::size_t last = i;
            for (std::size_t j = i + 1; j < path.segments.size() && path.segments[j].kind != 'M';
                 ++j) {
                last = j;
                const auto& part = path.segments[j];
                if (part.kind == 'L' || part.kind == 'C') {
                    const auto p = convert(part.p);
                    has_edge = has_edge || p.x != previous.x || p.y != previous.y;
                    if (part.kind == 'C') {
                        const auto c1 = convert(part.c1), c2 = convert(part.c2);
                        has_edge = has_edge || c1.x != previous.x || c1.y != previous.y ||
                                   c2.x != previous.x || c2.y != previous.y;
                    }
                    previous = p;
                }
            }
            if (!has_edge) {
                active = false;
                if (stroke->linecap == "round" || stroke->linecap == "square") {
                    auto p = convert(s.p);
                    require(FT_Stroker_BeginSubPath(stroker, &p, true) == 0);
                    ++p.x;
                    require(FT_Stroker_LineTo(stroker, &p) == 0);
                    require(FT_Stroker_EndSubPath(stroker) == 0);
                }
                i = last;
                continue;
            }
            bool closed = false;
            for (std::size_t j = i + 1; j < path.segments.size() && path.segments[j].kind != 'M';
                 ++j) {
                if (path.segments[j].kind == 'Z') {
                    closed = true;
                    break;
                }
            }
            auto p = convert(s.p);
            require(FT_Stroker_BeginSubPath(stroker, &p, !closed) == 0);
            active = true;
        } else if (s.kind == 'L') {
            auto p = convert(s.p);
            require(FT_Stroker_LineTo(stroker, &p) == 0);
        } else if (s.kind == 'C') {
            auto p = convert(s.p), c1 = convert(s.c1), c2 = convert(s.c2);
            require(FT_Stroker_CubicTo(stroker, &c1, &c2, &p) == 0);
        } else if (s.kind == 'Z') {
            if (active) {
                require(FT_Stroker_EndSubPath(stroker) == 0);
            }
            active = false;
        }
    }
    if (active) {
        require(FT_Stroker_EndSubPath(stroker) == 0);
    }
    FT_UInt points{}, contours{};
    require(FT_Stroker_GetCounts(stroker, &points, &contours) == 0 && points <= 32767 &&
            contours <= 32767);
    if (points == 0) {
        return;
    }
    Outline result;
    result.points.resize(points);
    result.tags.resize(points);
    result.contours.resize(contours);
    auto outline = result.get(false);
    outline.n_points = 0;
    outline.n_contours = 0;
    FT_Stroker_Export(stroker, &outline);
    for (auto& p : result.points) {
        Point local{static_cast<double>(p.x) / (64 * scale) + origin.x,
                    static_cast<double>(p.y) / (64 * scale) + origin.y};
        p = vector(matrix.apply(local));
    }
    render(library, canvas, outline, paint, opacity);
}
}  // namespace nordend::svg
