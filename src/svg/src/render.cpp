#include "internal.hpp"
#include "nordend/svg/svg.hpp"

namespace nordend::svg {
Document::Document(std::string_view input) : nodes(xml(input)), sheet(stylesheet(nodes)) {
    styles.resize(nodes.size());
    paths.resize(nodes.size());
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        auto id = nodes[i].attr("id");
        if (!id.empty()) {
            ids.try_emplace(id, i);
        }
        const Style parent = nodes[i].parent < nodes.size() ? styles[nodes[i].parent] : Style{};
        styles[i] = style(nodes[i], parent, sheet, style_work);
    }
    auto box = numbers(nodes[0].attr("viewBox"));
    require(box.empty() || (box.size() == 4 && box[2] > 0 && box[3] > 0));
    width = length(nodes[0].attr("width"), box.empty() ? 0 : box[2], box.empty() ? 0 : box[2]);
    height = length(nodes[0].attr("height"), box.empty() ? 0 : box[3], box.empty() ? 0 : box[3]);
    require(width > 0 && height > 0);
}

namespace {
struct Layer {
    Document& doc;
    Canvas canvas;

    Layer(Document& document, const Canvas& target) : doc(document) {
        const auto bytes = target.image.rgba.size();
        require(doc.live_layers < 16 && doc.layer_bytes + bytes <= 64U * 1024U * 1024U);
        canvas.image.width = target.image.width;
        canvas.image.height = target.image.height;
        canvas.image.rgba.resize(bytes);
        ++doc.live_layers;
        doc.layer_bytes += bytes;
    }

    ~Layer() {
        --doc.live_layers;
        doc.layer_bytes -= canvas.image.rgba.size();
    }
};

void clip_viewport(Document& doc, Raster& raster, Canvas& canvas, const Matrix& matrix,
                   double width, double height) {
    Layer mask{doc, canvas};
    Path path{{{'M', {0, 0}, {}, {}},
               {'L', {width, 0}, {}, {}},
               {'L', {width, height}, {}, {}},
               {'L', {0, height}, {}, {}},
               {'Z', {}, {}, {}}}};
    Paint white;
    white.solid = {1, 1, 1, 1};
    raster.draw(mask.canvas, path, matrix, white, 1, false, nullptr, width, height);
    canvas.work += canvas.image.rgba.size() / 4 + mask.canvas.work;
    require(canvas.work <= 64U * 1024U * 1024U);
    for (std::size_t i = 0; i < canvas.image.rgba.size(); i += 4) {
        for (std::size_t c = 0; c < 4; ++c) {
            canvas.image.rgba[i + c] = static_cast<std::uint8_t>(
                (canvas.image.rgba[i + c] * mask.canvas.image.rgba[i + 3] + 127) / 255);
        }
    }
}

void over(Canvas& target, const Canvas& source, double opacity) {
    target.work += source.work + target.image.rgba.size() / 4;
    require(target.work <= 64U * 1024U * 1024U);
    for (std::size_t i = 0; i < target.image.rgba.size(); i += 4) {
        double alpha = source.image.rgba[i + 3] * opacity / 255;
        for (std::size_t c = 0; c < 4; ++c) {
            target.image.rgba[i + c] = static_cast<std::uint8_t>(
                std::clamp(std::lround(source.image.rgba[i + c] * opacity +
                                       target.image.rgba[i + c] * (1 - alpha)),
                           0L, 255L));
        }
    }
}

void draw(Document& doc, std::size_t index, const Style& parent, Matrix matrix, double width,
          double height, Raster& raster, Canvas& canvas, std::size_t depth = 0,
          bool referenced = false) {
    require(depth < kMaxDepth && ++doc.visited <= kMaxNodes + kMaxExpansion);
    if (!doc.active.empty()) {
        require(++doc.expansions <= kMaxExpansion);
    }
    const auto& node = doc.nodes[index];
    const auto st =
        doc.active.empty() ? doc.styles[index] : style(node, parent, doc.sheet, doc.style_work);
    if (!st.display) {
        return;
    }
    if (index != 0) {
        matrix = matrix * transform(node.attr("transform"));
    }
    if (!matrix.inverse()) {
        return;
    }
    if (node.name == "use") {
        const auto href = node.attr("href");
        if (!href.starts_with('#')) {
            return;
        }
        auto target = doc.ids.find(href.substr(1));
        if (target == doc.ids.end()) {
            return;
        }
        require(doc.active.size() < kMaxDepth && doc.active.insert(target->second).second);
        matrix = matrix *
                 Matrix{1, 0, 0, 1, length(node.attr("x"), width), length(node.attr("y"), height)};
        const auto& original = doc.nodes[target->second];
        if (original.name == "symbol" || original.name == "svg") {
            double w = length(node.attr("width"), width, width),
                   h = length(node.attr("height"), height, height);
            require(w >= 0 && h >= 0);
            if (w == 0 || h == 0) {
                doc.active.erase(target->second);
                return;
            }
            const Matrix clip_matrix = matrix;
            const double viewport_width = w, viewport_height = h;
            matrix = matrix *
                     viewport(original.attr("viewBox"), original.attr("preserveAspectRatio"), w, h);
            auto vb = numbers(original.attr("viewBox"));
            if (!vb.empty()) {
                w = vb[2];
                h = vb[3];
            }
            Layer layer{doc, canvas};
            draw(doc, target->second, st, matrix, w, h, raster, layer.canvas, depth + 1, true);
            clip_viewport(doc, raster, layer.canvas, clip_matrix, viewport_width, viewport_height);
            over(canvas, layer.canvas, 1);
        } else {
            draw(doc, target->second, st, matrix, width, height, raster, canvas, depth + 1, true);
        }
        doc.active.erase(target->second);
        return;
    }
    if (node.name == "svg" || node.name == "g" || (referenced && node.name == "symbol")) {
        if (node.name == "svg" && index != 0 && !referenced) {
            double w = length(node.attr("width"), width, width),
                   h = length(node.attr("height"), height, height);
            require(w >= 0 && h >= 0);
            if (w == 0 || h == 0) {
                return;
            }
            const Matrix clip_matrix =
                matrix *
                Matrix{1, 0, 0, 1, length(node.attr("x"), width), length(node.attr("y"), height)};
            matrix = clip_matrix *
                     viewport(node.attr("viewBox"), node.attr("preserveAspectRatio"), w, h);
            auto vb = numbers(node.attr("viewBox"));
            width = vb.empty() ? w : vb[2];
            height = vb.empty() ? h : vb[3];
            Layer layer{doc, canvas};
            for (auto child : node.children) {
                draw(doc, child, st, matrix, width, height, raster, layer.canvas, depth + 1);
            }
            clip_viewport(doc, raster, layer.canvas, clip_matrix, w, h);
            over(canvas, layer.canvas, 1);
            return;
        }
        for (auto child : node.children) {
            draw(doc, child, st, matrix, width, height, raster, canvas, depth + 1);
        }
        return;
    }
    if (!st.visible || st.opacity == 0) {
        return;
    }
    const auto path = geometry(node, width, height, doc.segments);
    if (path.segments.empty()) {
        return;
    }
    const auto box = bounds(path);
    const auto fill = doc.paint(st.fill, st.color, box, matrix, width, height),
               stroke = doc.paint(st.stroke, st.color, box, matrix, width, height);
    if (st.opacity < 1) {
        Layer layer{doc, canvas};
        raster.draw(layer.canvas, path, matrix, fill, st.fill_opacity, st.fill_rule == "evenodd",
                    nullptr, width, height);
        raster.draw(layer.canvas, path, matrix, stroke, st.stroke_opacity, false, &st, width,
                    height);
        over(canvas, layer.canvas, st.opacity);
    } else {
        raster.draw(canvas, path, matrix, fill, st.fill_opacity, st.fill_rule == "evenodd", nullptr,
                    width, height);
        raster.draw(canvas, path, matrix, stroke, st.stroke_opacity, false, &st, width, height);
    }
}
}  // namespace

picture::Image render(std::span<const std::uint8_t> data, int pixels) noexcept {
    try {
        require(!data.empty() && data.size() <= kMaxBytes && pixels > 0 && pixels <= 1024);
        Document doc{{reinterpret_cast<const char*>(data.data()), data.size()}};
        const double scale = static_cast<double>(pixels) / std::max(doc.width, doc.height);
        require(std::isfinite(scale));
        Canvas canvas;
        canvas.image.width =
            static_cast<std::uint32_t>(std::max(1.0, std::round(doc.width * scale)));
        canvas.image.height =
            static_cast<std::uint32_t>(std::max(1.0, std::round(doc.height * scale)));
        canvas.image.rgba.resize(static_cast<std::size_t>(canvas.image.width) *
                                 canvas.image.height * 4);
        Matrix matrix = Matrix{scale, 0, 0, scale, 0, 0} *
                        transform(doc.nodes[0].attr("transform")) *
                        viewport(doc.nodes[0].attr("viewBox"),
                                 doc.nodes[0].attr("preserveAspectRatio"), doc.width, doc.height);
        auto vb = numbers(doc.nodes[0].attr("viewBox"));
        double width = vb.empty() ? doc.width : vb[2], height = vb.empty() ? doc.height : vb[3];
        Raster raster;
        draw(doc, 0, {}, matrix, width, height, raster, canvas);
        straighten(canvas.image);
        return std::move(canvas.image);
    } catch (...) {
        return {};
    }
}
}  // namespace nordend::svg
