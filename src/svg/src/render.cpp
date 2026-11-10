#include "internal.hpp"
#include "nordend/svg/svg.hpp"

namespace nordend::svg {
Document::Document(std::string_view input) : nodes(xml(input)), sheet(stylesheet(nodes)) {
    styles.resize(nodes.size());
    paths.resize(nodes.size());
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        auto id = nodes[i].attr("id");
        if (!id.empty())
            ids.try_emplace(id, i);
        const Style parent = nodes[i].parent < nodes.size() ? styles[nodes[i].parent] : Style{};
        styles[i] = style(nodes[i], parent, sheet, style_work);
    }
    const auto box = numbers(nodes[0].attr("viewBox"));
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
        auto bytes = target.image.rgba.size();
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

struct Reference {
    Document& doc;
    std::size_t index;

    Reference(Document& d, std::size_t i) : doc(d), index(i) {
        require(doc.active.size() < kMaxDepth && doc.active.insert(index).second);
    }

    ~Reference() { doc.active.erase(index); }
};

void charge(Canvas& canvas, std::size_t work) {
    canvas.work += work;
    require(canvas.work <= 64U * 1024U * 1024U);
}

void over(Canvas& target, const Canvas& source, double opacity) {
    charge(target, source.work + target.image.rgba.size() / 4);
    for (std::size_t i = 0; i < target.image.rgba.size(); i += 4) {
        if (source.image.rgba[i + 3] == 0)
            continue;
        double alpha = source.image.rgba[i + 3] * opacity / 255;
        for (std::size_t c = 0; c < 4; ++c)
            target.image.rgba[i + c] = static_cast<std::uint8_t>(
                std::clamp(std::lround(source.image.rgba[i + c] * opacity +
                                       target.image.rgba[i + c] * (1 - alpha)),
                           0L, 255L));
    }
}

void multiply(Canvas& target, const Canvas& mask, bool luminance) {
    charge(target, mask.work + target.image.rgba.size() / 4);
    for (std::size_t i = 0; i < target.image.rgba.size(); i += 4) {
        const double alpha = mask.image.rgba[i + 3] / 255.0;
        double factor = alpha;
        if (luminance && alpha > 0) {
            const double red = mask.image.rgba[i] / (255 * alpha),
                         green = mask.image.rgba[i + 1] / (255 * alpha),
                         blue = mask.image.rgba[i + 2] / (255 * alpha);
            factor = (.2126 * red + .7152 * green + .0722 * blue) * alpha;
        }
        for (std::size_t c = 0; c < 4; ++c)
            target.image.rgba[i + c] = static_cast<std::uint8_t>(
                std::clamp(std::lround(target.image.rgba[i + c] * factor), 0L, 255L));
    }
}

std::optional<std::size_t> reference(const Document& doc, std::string_view value) {
    value = trim(value);
    if (!value.starts_with("url("))
        return {};
    const auto end = value.find(')');
    if (end == value.npos)
        return {};
    auto id = trim(value.substr(4, end - 4));
    if (id.size() > 1 &&
        ((id.front() == '\'' && id.back() == '\'') || (id.front() == '"' && id.back() == '"')))
        id = id.substr(1, id.size() - 2);
    if (!id.starts_with('#'))
        return {};
    auto it = doc.ids.find(id.substr(1));
    return it == doc.ids.end() ? std::nullopt : std::optional{it->second};
}

struct Engine {
    Document& doc;
    Raster raster;
    std::vector<std::pair<double, double>> path_extents;
    std::size_t bounds_work{};

    explicit Engine(Document& d) : doc(d), path_extents(d.nodes.size()) {}

    const Path& path(std::size_t index, double w, double h) {
        if (!doc.paths[index] || path_extents[index] != std::pair{w, h}) {
            doc.paths[index] = geometry(doc.nodes[index], w, h, doc.segments);
            path_extents[index] = {w, h};
        }
        return *doc.paths[index];
    }

    void viewport_clip(Canvas& canvas, const Matrix& matrix, double w, double h) {
        Layer mask{doc, canvas};
        Path rectangle{{{'M', {0, 0}, {}, {}},
                        {'L', {w, 0}, {}, {}},
                        {'L', {w, h}, {}, {}},
                        {'L', {0, h}, {}, {}},
                        {'Z', {}, {}, {}}}};
        Paint white;
        white.solid = {1, 1, 1, 1};
        raster.draw(mask.canvas, rectangle, matrix, white, 1, false, nullptr, w, h);
        multiply(canvas, mask.canvas, false);
    }

    std::optional<Box> box(std::size_t index, double w, double h, Matrix matrix = {},
                           std::size_t depth = 0, bool first = true, bool mapped = false) {
        require(depth < kMaxDepth && ++bounds_work <= kMaxSegments);
        const auto& node = doc.nodes[index];
        if (!doc.styles[index].display)
            return {};
        if (!first)
            matrix = matrix * transform(node.attr("transform"));
        if (node.name == "use") {
            auto href = node.attr("href");
            if (!href.starts_with('#'))
                return {};
            auto it = doc.ids.find(href.substr(1));
            if (it == doc.ids.end())
                return {};
            Reference guard{doc, it->second};
            matrix =
                matrix * Matrix{1, 0, 0, 1, length(node.attr("x"), w), length(node.attr("y"), h)};
            const auto& target = doc.nodes[it->second];
            if (target.name == "symbol" || target.name == "svg") {
                double vw = length(node.attr("width"), w, w),
                       vh = length(node.attr("height"), h, h);
                matrix = matrix * viewport(target.attr("viewBox"),
                                           target.attr("preserveAspectRatio"), vw, vh);
                auto vb = numbers(target.attr("viewBox"));
                w = vb.empty() ? vw : vb[2];
                h = vb.empty() ? vh : vb[3];
            }
            return box(it->second, w, h, matrix, depth + 1, false, true);
        }
        if (node.name == "g" || node.name == "svg" || node.name == "symbol" ||
            node.name == "clipPath" || node.name == "mask") {
            if (node.name == "svg" && !first && !mapped) {
                double vw = length(node.attr("width"), w, w),
                       vh = length(node.attr("height"), h, h);
                matrix = matrix *
                         Matrix{1, 0, 0, 1, length(node.attr("x"), w), length(node.attr("y"), h)} *
                         viewport(node.attr("viewBox"), node.attr("preserveAspectRatio"), vw, vh);
                auto vb = numbers(node.attr("viewBox"));
                w = vb.empty() ? vw : vb[2];
                h = vb.empty() ? vh : vb[3];
            }
            std::optional<Box> result;
            for (auto child : node.children)
                if (auto b = box(child, w, h, matrix, depth + 1, false)) {
                    if (!result)
                        result = b;
                    else {
                        double right = std::max(result->x + result->width, b->x + b->width),
                               bottom = std::max(result->y + result->height, b->y + b->height);
                        result->x = std::min(result->x, b->x);
                        result->y = std::min(result->y, b->y);
                        result->width = right - result->x;
                        result->height = bottom - result->y;
                    }
                }
            return result;
        }
        const auto& original = path(index, w, h);
        if (original.segments.empty())
            return {};
        Path converted = original;
        for (auto& s : converted.segments) {
            s.p = matrix.apply(s.p);
            s.c1 = matrix.apply(s.c1);
            s.c2 = matrix.apply(s.c2);
            for (auto p : {s.p, s.c1, s.c2})
                require(std::isfinite(p.x) && std::isfinite(p.y));
        }
        return bounds(converted);
    }

    void clip(Canvas& canvas, std::size_t index, const Box& bbox, const Matrix& matrix, double w,
              double h, std::size_t depth) {
        if (doc.nodes[index].name != "clipPath")
            return;
        Reference guard{doc, index};
        Layer mask{doc, canvas};
        Matrix units = matrix;
        if (doc.nodes[index].attr("clipPathUnits") == "objectBoundingBox") {
            if (bbox.width <= 0 || bbox.height <= 0) {
                std::fill(canvas.image.rgba.begin(), canvas.image.rgba.end(), 0);
                return;
            }
            units = units * Matrix{bbox.width, 0, 0, bbox.height, bbox.x, bbox.y};
            w = h = 1;
        }
        draw(index, {}, units, w, h, mask.canvas, depth + 1, false, true, true);
        multiply(canvas, mask.canvas, false);
    }

    void mask(Canvas& canvas, std::size_t index, const Box& bbox, const Matrix& matrix, double w,
              double h, std::size_t depth) {
        const auto& node = doc.nodes[index];
        if (node.name != "mask")
            return;
        Reference guard{doc, index};
        Layer coverage{doc, canvas};
        const bool object = node.attr("maskUnits") != "userSpaceOnUse";
        double x = length(node.attr("x"), object ? 1 : w, object ? -.1 : -.1 * w),
               y = length(node.attr("y"), object ? 1 : h, object ? -.1 : -.1 * h);
        double rw = length(node.attr("width"), object ? 1 : w, object ? 1.2 : 1.2 * w),
               rh = length(node.attr("height"), object ? 1 : h, object ? 1.2 : 1.2 * h);
        if (object) {
            x = bbox.x + x * bbox.width;
            y = bbox.y + y * bbox.height;
            rw *= bbox.width;
            rh *= bbox.height;
        }
        if (rw <= 0 || rh <= 0) {
            std::fill(canvas.image.rgba.begin(), canvas.image.rgba.end(), 0);
            return;
        }
        Matrix content = matrix;
        double cw = w, ch = h;
        if (node.attr("maskContentUnits") == "objectBoundingBox") {
            content = content * Matrix{bbox.width, 0, 0, bbox.height, bbox.x, bbox.y};
            cw = ch = 1;
        }
        draw(index, {}, content, cw, ch, coverage.canvas, depth + 1, false, true, false);
        viewport_clip(coverage.canvas, matrix * Matrix{1, 0, 0, 1, x, y}, rw, rh);
        multiply(canvas, coverage.canvas, doc.styles[index].mask_type != "alpha");
    }

    void content(std::size_t index, const Style& st, const Matrix& matrix, double w, double h,
                 Canvas& canvas, std::size_t depth, bool instance, bool referenced, bool clipping) {
        const auto& node = doc.nodes[index];
        if (node.name == "use") {
            auto href = node.attr("href");
            if (!href.starts_with('#'))
                return;
            auto it = doc.ids.find(href.substr(1));
            if (it == doc.ids.end())
                return;
            Reference guard{doc, it->second};
            const Matrix placement =
                matrix * Matrix{1, 0, 0, 1, length(node.attr("x"), w), length(node.attr("y"), h)};
            const auto& target = doc.nodes[it->second];
            if (target.name == "symbol" || target.name == "svg") {
                const double vw = length(node.attr("width"), w, w),
                             vh = length(node.attr("height"), h, h);
                require(vw >= 0 && vh >= 0);
                if (vw == 0 || vh == 0)
                    return;
                const Matrix mapped =
                    placement *
                    viewport(target.attr("viewBox"), target.attr("preserveAspectRatio"), vw, vh);
                auto vb = numbers(target.attr("viewBox"));
                Layer layer{doc, canvas};
                draw(it->second, st, mapped, vb.empty() ? vw : vb[2], vb.empty() ? vh : vb[3],
                     layer.canvas, depth + 1, true, true, clipping);
                viewport_clip(layer.canvas, placement, vw, vh);
                over(canvas, layer.canvas, 1);
            } else
                draw(it->second, st, placement, w, h, canvas, depth + 1, true, true, clipping);
            return;
        }
        if (node.name == "g" || node.name == "svg" ||
            (referenced &&
             (node.name == "symbol" || node.name == "mask" || node.name == "clipPath"))) {
            if (node.name == "svg" && index != 0 && !referenced) {
                const double vw = length(node.attr("width"), w, w),
                             vh = length(node.attr("height"), h, h);
                require(vw >= 0 && vh >= 0);
                if (vw == 0 || vh == 0)
                    return;
                const Matrix placement =
                    matrix *
                    Matrix{1, 0, 0, 1, length(node.attr("x"), w), length(node.attr("y"), h)};
                const Matrix mapped =
                    placement *
                    viewport(node.attr("viewBox"), node.attr("preserveAspectRatio"), vw, vh);
                auto vb = numbers(node.attr("viewBox"));
                Layer layer{doc, canvas};
                for (auto child : node.children)
                    draw(child, st, mapped, vb.empty() ? vw : vb[2], vb.empty() ? vh : vb[3],
                         layer.canvas, depth + 1, instance, false, clipping);
                viewport_clip(layer.canvas, placement, vw, vh);
                over(canvas, layer.canvas, 1);
            } else
                for (auto child : node.children)
                    draw(child, st, matrix, w, h, canvas, depth + 1, instance, false, clipping);
            return;
        }
        if (!st.visible)
            return;
        const auto& shape = path(index, w, h);
        if (shape.segments.empty())
            return;
        if (clipping) {
            Paint white;
            white.solid = {1, 1, 1, 1};
            raster.draw(canvas, shape, matrix, white, 1, st.clip_rule == "evenodd", nullptr, w, h);
            return;
        }
        const auto bbox = bounds(shape);
        const auto fill = doc.paint(st.fill, st.color, bbox, matrix, w, h),
                   stroke = doc.paint(st.stroke, st.color, bbox, matrix, w, h);
        raster.draw(canvas, shape, matrix, fill, st.fill_opacity, st.fill_rule == "evenodd",
                    nullptr, w, h);
        raster.draw(canvas, shape, matrix, stroke, st.stroke_opacity, false, &st, w, h);
    }

    void draw(std::size_t index, const Style& parent, Matrix matrix, double w, double h,
              Canvas& canvas, std::size_t depth = 0, bool instance = false, bool referenced = false,
              bool clipping = false) {
        require(depth < kMaxDepth && ++doc.visited <= kMaxNodes + kMaxExpansion);
        if (instance)
            require(++doc.expansions <= kMaxExpansion);
        const auto& node = doc.nodes[index];
        const auto st =
            instance ? style(node, parent, doc.sheet, doc.style_work) : doc.styles[index];
        if (!st.display || (!clipping && st.opacity == 0))
            return;
        if (index != 0)
            matrix = matrix * transform(node.attr("transform"));
        if (!matrix.inverse())
            return;
        const auto clip_ref = reference(doc, st.clip),
                   mask_ref = clipping ? std::nullopt : reference(doc, st.mask);
        const double opacity = clipping ? 1 : st.opacity;
        if (opacity < 1 || clip_ref || mask_ref) {
            const auto bbox = (clip_ref || mask_ref) ? box(index, w, h).value_or(Box{}) : Box{};
            Layer layer{doc, canvas};
            content(index, st, matrix, w, h, layer.canvas, depth, instance, referenced, clipping);
            if (clip_ref)
                clip(layer.canvas, *clip_ref, bbox, matrix, w, h, depth);
            if (mask_ref)
                mask(layer.canvas, *mask_ref, bbox, matrix, w, h, depth);
            over(canvas, layer.canvas, opacity);
        } else
            content(index, st, matrix, w, h, canvas, depth, instance, referenced, clipping);
    }
};
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
        const auto& root = doc.nodes[0];
        Matrix matrix =
            Matrix{scale, 0, 0, scale, 0, 0} * transform(root.attr("transform")) *
            viewport(root.attr("viewBox"), root.attr("preserveAspectRatio"), doc.width, doc.height);
        auto vb = numbers(root.attr("viewBox"));
        Engine engine{doc};
        engine.draw(0, {}, matrix, vb.empty() ? doc.width : vb[2], vb.empty() ? doc.height : vb[3],
                    canvas);
        straighten(canvas.image);
        return std::move(canvas.image);
    } catch (...) {
        return {};
    }
}
}  // namespace nordend::svg
