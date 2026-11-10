#include <cmath>
#include <type_traits>

#include "internal.hpp"

namespace nordend::svg {
namespace {
struct Spans {
    Canvas& canvas;
    Colour paint;
};

void spans(int y, int count, const FT_Span* data, void* user) {
    auto& context = *static_cast<Spans*>(user);
    for (int i = 0; i < count; ++i) {
        composite(context.canvas, data[i].x, y, data[i].len, data[i].coverage, context.paint);
    }
}

FT_Pos fixed(double value) {
    require(std::isfinite(value) && std::abs(value) <= 32767);
    return static_cast<FT_Pos>(std::lround(value * 64));
}
}  // namespace

Raster::Raster() { require(FT_Init_FreeType(&library) == 0); }

Raster::~Raster() { FT_Done_FreeType(library); }

void Raster::rectangle(Canvas& canvas, double x, double y, double width, double height,
                       Colour paint) {
    FT_Vector points[] = {{fixed(x), fixed(y)},
                          {fixed(x + width), fixed(y)},
                          {fixed(x + width), fixed(y + height)},
                          {fixed(x), fixed(y + height)}};
    std::remove_pointer_t<decltype(FT_Outline::tags)> tags[] = {FT_CURVE_TAG_ON, FT_CURVE_TAG_ON,
                                                                FT_CURVE_TAG_ON, FT_CURVE_TAG_ON};
    std::remove_pointer_t<decltype(FT_Outline::contours)> contours[] = {3};
    FT_Outline outline{1, 4, points, tags, contours, 0};
    Spans context{canvas, paint};
    FT_Raster_Params params{};
    params.flags = FT_RASTER_FLAG_AA | FT_RASTER_FLAG_DIRECT | FT_RASTER_FLAG_CLIP;
    params.gray_spans = spans;
    params.user = &context;
    params.clip_box = {0, 0, static_cast<FT_Pos>(canvas.image.width),
                       static_cast<FT_Pos>(canvas.image.height)};
    require(FT_Outline_Render(library, &outline, &params) == 0 && !canvas.exhausted);
}
}  // namespace nordend::svg
