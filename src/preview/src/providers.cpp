#include <utility>

#include "nordend/preview/preview.hpp"
#include "nordend/thumbs/cache.hpp"

namespace nordend::preview {
namespace {

// Images and camera raw files (from their embedded previews), fitted to the area.
class ImageProvider final : public Provider {
public:
    [[nodiscard]] Kind kind() const override { return Kind::image; }

    [[nodiscard]] bool can_handle(const fs::MimeDatabase& mime, fs::MimeId type) const override {
        return thumbs::can_make(mime.name(type));
    }

    [[nodiscard]] Content load(const Request& request,
                               const core::CancelToken& /*cancel*/) const override {
        return image::load_to_fit(request.path, request.width, request.height);
    }
};

// Text files: the text/plain family (source code, configuration, XML, JSON...). A file that
// turns out to be binary gets the information card.
class TextProvider final : public Provider {
public:
    [[nodiscard]] Kind kind() const override { return Kind::text; }

    [[nodiscard]] bool can_handle(const fs::MimeDatabase& mime, fs::MimeId type) const override {
        return mime.is_a(type, fs::MimeDatabase::kTextPlain);
    }

    [[nodiscard]] Content load(const Request& request,
                               const core::CancelToken& cancel) const override {
        std::string error;
        std::optional<Text> text = read_text(request.path, {}, error);
        if (!text || !error.empty()) {
            Info info = read_info(request.path, cancel);
            if (info.error.empty()) {
                info.error = error;
            }
            return info;
        }
        return std::move(*text);
    }
};

// Everything else: the information card.
class InfoProvider final : public Provider {
public:
    [[nodiscard]] Kind kind() const override { return Kind::info; }

    [[nodiscard]] bool can_handle(const fs::MimeDatabase& /*mime*/,
                                  fs::MimeId /*type*/) const override {
        return true;
    }

    [[nodiscard]] Content load(const Request& request,
                               const core::CancelToken& cancel) const override {
        return read_info(request.path, cancel);
    }
};

}  // namespace

const Provider& provider_for(const fs::MimeDatabase& mime, fs::MimeId type) {
    static const ImageProvider kImage;
    static const TextProvider kText;
    static const InfoProvider kInfo;
    if (kImage.can_handle(mime, type)) {
        return kImage;
    }
    if (kText.can_handle(mime, type)) {
        return kText;
    }
    return kInfo;
}

}  // namespace nordend::preview
