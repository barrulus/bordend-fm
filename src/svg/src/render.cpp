#include <algorithm>
#include <cmath>

#include "internal.hpp"
#include "nordend/svg/svg.hpp"

namespace nordend::svg {
namespace {
void draw(const std::vector<Node>& nodes, std::size_t index, Raster& raster, Canvas& canvas,
          double scale, Colour fill) {
    const auto& node = nodes[index];
    if (node.attr("display") == "none") {
        return;
    }
    if (!node.attr("fill").empty()) {
        fill = colour(node.attr("fill"));
    }
    if (node.name == "svg" || node.name == "g") {
        for (const auto child : node.children) {
            draw(nodes, child, raster, canvas, scale, fill);
        }
    } else if (node.name == "rect") {
        const double x = number(node.attr("x")) * scale;
        const double y = number(node.attr("y")) * scale;
        const double width = number(node.attr("width")) * scale;
        const double height = number(node.attr("height")) * scale;
        require(std::isfinite(width) && std::isfinite(height) && width >= 0 && height >= 0);
        fill[3] *= std::clamp(number(node.attr("opacity"), 1), 0.0, 1.0) *
                   std::clamp(number(node.attr("fill-opacity"), 1), 0.0, 1.0);
        if (width > 0 && height > 0 && fill[3] > 0 && node.attr("visibility") != "hidden") {
            raster.rectangle(canvas, x, y, width, height, fill);
        }
    }
}
}  // namespace

picture::Image render(std::span<const std::uint8_t> data, int pixels) noexcept {
    try {
        require(!data.empty() && data.size() <= kMaxBytes && pixels > 0 && pixels <= 1024);
        const auto nodes = xml({reinterpret_cast<const char*>(data.data()), data.size()});
        const double width = number(nodes[0].attr("width"));
        const double height = number(nodes[0].attr("height"));
        require(width > 0 && height > 0);
        const double scale = static_cast<double>(pixels) / std::max(width, height);
        require(std::isfinite(scale));
        Canvas canvas;
        canvas.image.width = static_cast<std::uint32_t>(std::max(1.0, std::round(width * scale)));
        canvas.image.height = static_cast<std::uint32_t>(std::max(1.0, std::round(height * scale)));
        canvas.image.rgba.resize(static_cast<std::size_t>(canvas.image.width) *
                                 canvas.image.height * 4);
        Raster raster;
        draw(nodes, 0, raster, canvas, scale, {0, 0, 0, 1});
        straighten(canvas.image);
        return std::move(canvas.image);
    } catch (...) {
        return {};
    }
}
}  // namespace nordend::svg
