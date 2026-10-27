#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "nordend/core/worker_pool.hpp"
#include "nordend/fs/mime.hpp"
#include "nordend/image/decode.hpp"
#include "nordend/image/fit.hpp"
#include "nordend/theme/palette.hpp"
#include "nordend/thumbs/cache.hpp"
#include "nordend/ui/geometry.hpp"
#include "nordend/ui/icons.hpp"
#include "nordend/ui/renderer.hpp"
#include "nordend/ui/scroller.hpp"
#include "nordend/ui/texture.hpp"
#include "nordend/views/folder.hpp"

namespace nordend::views {

// Quick Look: an overlay showing the entry under a folder's cursor, large. It follows the
// cursor: the view underneath keeps the keys, so arrows walk the folder and Quick Look shows
// where they land. Images and camera raw files are shown fitted to the window, first from their
// thumbnail (at once) then sharp; the entries before and after are prepared in advance. Other
// entries show an information card.
//
// Loop thread only. Textures are created while drawing.
class QuickLook {
public:
    using Clock = ui::Scroller::Clock;

    // thumbnails may be nullptr (no first, blurry stage).
    QuickLook(core::WorkerPool& pool, std::shared_ptr<const fs::MimeDatabase> mime,
              std::shared_ptr<const thumbs::Cache> thumbnails, std::function<void()> redraw);
    ~QuickLook();

    QuickLook(const QuickLook&) = delete;
    QuickLook& operator=(const QuickLook&) = delete;
    QuickLook(QuickLook&&) = delete;
    QuickLook& operator=(QuickLook&&) = delete;

    // Opens on folder's cursor entry; does nothing when there is none.
    void open(std::shared_ptr<Folder> folder, Clock::time_point now);
    void close();

    [[nodiscard]] bool is_open() const noexcept { return folder_ != nullptr; }

    // Shows what folder's cursor is on now (it may have moved, or the view may show another
    // folder); closes when it is on nothing.
    void follow(std::shared_ptr<Folder> folder);

    void draw(ui::Renderer& renderer, const ui::Rect& bounds, const theme::Palette& palette,
              ui::IconSet& icons, Clock::time_point now);

    // The fade-in is running.
    [[nodiscard]] bool animating(Clock::time_point now) const;

    // The shown entry's image is still being made (snapshots, tests).
    [[nodiscard]] bool loading() const;

    // The name of the entry shown; empty when closed.
    [[nodiscard]] const std::string& shown() const noexcept { return shown_name_; }

    // Size of the sharp image of the shown entry once made (tests).
    [[nodiscard]] std::optional<std::pair<std::uint32_t, std::uint32_t>> image_size() const;

private:
    struct Slot {
        std::filesystem::path path;
        std::optional<core::CancelToken> thumbnail_job;
        std::optional<core::CancelToken> image_job;
        std::shared_ptr<image::Image> thumbnail_pixels;
        std::shared_ptr<image::Fitted> image;  // the sharp stage, once made
        std::unique_ptr<ui::Texture> thumbnail_texture;
        std::unique_ptr<ui::Texture> image_texture;
    };

    [[nodiscard]] bool previewable(const fs::Entry& entry) const;
    [[nodiscard]] std::string key_of(const fs::Entry& entry) const;
    Slot& ensure(const fs::Entry& entry, bool current);
    void drop(Slot& slot);
    void draw_image(ui::Renderer& renderer, const ui::Rect& area, const theme::Palette& palette,
                    ui::IconSet& icons, const fs::Entry& entry, float fade);
    void draw_card(ui::Renderer& renderer, const ui::Rect& area, const theme::Palette& palette,
                   ui::IconSet& icons, const fs::Entry& entry, float fade);

    core::WorkerPool& pool_;
    std::shared_ptr<const fs::MimeDatabase> mime_;
    std::shared_ptr<const thumbs::Cache> thumbnails_;
    std::function<void()> redraw_;

    std::shared_ptr<Folder> folder_;  // nullptr: closed
    std::string shown_name_;
    std::string shown_key_;
    Clock::time_point opened_;
    std::unordered_map<std::string, Slot> slots_;  // the shown entry and its neighbours
    // Textures of slots dropped outside a draw: deleted at the next draw, with GL current.
    std::vector<std::unique_ptr<ui::Texture>> retired_;
    // The size images are made for: the last drawn image area, in physical pixels.
    std::uint32_t target_width_{2560};
    std::uint32_t target_height_{1440};
    // False once destroyed: results still on their way find out here.
    std::shared_ptr<bool> alive_{std::make_shared<bool>(true)};
};

}  // namespace nordend::views
