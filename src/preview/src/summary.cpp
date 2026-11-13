#include <string>
#include <string_view>
#include <utility>

#include "rummage/picture/load.hpp"
#include "rummage/preview/preview.hpp"

namespace rummage::preview {

Summary summarize(const std::filesystem::path& path, std::string_view mime, Kind kind,
                  const core::CancelToken& cancel) {
    Summary summary;
    summary.info = read_info(path, cancel);
    if (kind == Kind::image && !mime.starts_with("video/")) {
        summary.photo = picture::load_photo_info(path);
    } else if (kind == Kind::text) {
        // As much as Quick Look reads: the pane's page scrolls through it. A log's newest lines
        // are at its end.
        text::Loaded loaded = text::load_text(
            {.path = path, .mime = std::string{mime}, .limits = {.tail = mime == "text/x-log"}});
        if (loaded.failure == text::Failure::none) {
            summary.text = std::move(loaded.text);
        }
    }
    return summary;
}

}  // namespace rummage::preview
