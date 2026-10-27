#include "nordend/views/status_bar.hpp"

#include <algorithm>
#include <format>
#include <string_view>
#include <sys/statvfs.h>
#include <utility>

#include "nordend/views/format.hpp"

namespace nordend::views {
namespace {

constexpr float kPadding = 16.0F;
constexpr float kTextSize = 12.0F;
constexpr float kButtonWidth = 30.0F;
constexpr float kButtonHeight = 22.0F;
constexpr float kButtonGap = 2.0F;
constexpr float kHelpGap = 10.0F;  // between the views' buttons and "?"
constexpr float kIconSize = 16.0F;
constexpr std::array<std::string_view, StatusBar::kViews + 1> kIcons{
    "layout-columns", "layout-grid", "list-details", "help"};

}  // namespace

std::optional<FreeSpace> free_space_of(const std::filesystem::path& path) {
    struct statvfs info{};
    if (statvfs(path.c_str(), &info) != 0) {
        return std::nullopt;
    }
    return FreeSpace{.free = static_cast<std::uint64_t>(info.f_bavail) * info.f_frsize,
                     .total = static_cast<std::uint64_t>(info.f_blocks) * info.f_frsize};
}

std::string status_of(const Folder& folder) {
    const fs::DirectoryModel& model = folder.model();
    const std::size_t count = folder.entries().size();
    if (model.error()) {
        return model.error().message();
    }
    if (folder.selected_count() > 1) {
        return std::format("{} of {} selected", folder.selected_count(), count);
    }
    if (model.loading()) {
        return std::format("{} items, loading…", count);
    }
    return std::format("{} {}", count, count == 1 ? "item" : "items");
}

StatusBar::StatusBar(core::WorkerPool& pool, std::function<void()> redraw,
                     std::function<void(std::size_t view)> choose_view, std::function<void()> help)
    : pool_{pool},
      redraw_{std::move(redraw)},
      choose_view_{std::move(choose_view)},
      help_{std::move(help)} {}

StatusBar::~StatusBar() {
    *alive_ = false;
    if (job_) {
        job_->cancel();
    }
}

void StatusBar::update(const Folder& folder) {
    if (job_ || (known_ && path_ == folder.path() && revision_ == folder.model().revision())) {
        return;  // on its way, or known: a draw after the result checks again
    }
    path_ = folder.path();
    revision_ = folder.model().revision();
    known_ = true;
    job_ = pool_.submit(
        core::Priority::visible,
        [path = path_](const core::CancelToken& /*token*/) { return free_space_of(path); },
        [this, alive = alive_](std::optional<FreeSpace> space) {
            if (!*alive) {
                return;
            }
            job_.reset();
            free_ = space;
            redraw_();
        });
}

void StatusBar::draw(ui::Renderer& renderer, const ui::Rect& bounds, const theme::Palette& palette,
                     ui::IconSet& icons, const Folder& folder, std::size_t view) {
    bounds_ = bounds;
    update(folder);
    renderer.fill_rect({bounds.x, bounds.y, bounds.width, 1.0F / renderer.scale()},
                       palette.outline_variant);

    // The buttons, from the right: "?", then the views.
    const float top = bounds.y + ((kHeight - kButtonHeight) / 2.0F);
    float x = bounds.right() - kPadding + kButtonGap;
    for (std::size_t i = kViews + 1; i-- > 0;) {
        x -= kButtonWidth + kButtonGap;
        if (i == kViews - 1) {
            x -= kHelpGap;
        }
        buttons_.at(i) = {x, top, kButtonWidth, kButtonHeight};
    }
    for (std::size_t i = 0; i <= kViews; ++i) {
        const ui::Rect& button = buttons_.at(i);
        const bool active = i == view;
        if (active || hovered_ == i) {
            renderer.draw_box(button, {.fill = active ? palette.surface_container_highest
                                                      : palette.surface_container_high,
                                       .radius = ui::Corners::all(6.0F)});
        }
        ui::Color color = hovered_ == i ? palette.on_surface : palette.on_surface_variant;
        if (active) {
            color = palette.primary;
        }
        icons.draw(renderer, kIcons.at(i), button.x + ((kButtonWidth - kIconSize) / 2.0F),
                   button.y + ((kButtonHeight - kIconSize) / 2.0F), kIconSize, color);
    }

    // The status and the free space, cut in the middle when the buttons leave too little room.
    std::string text = status_of(folder);
    if (free_ && !folder.model().error()) {
        text +=
            std::format("  ·  {} free of {}", format_size(free_->free), format_size(free_->total));
    }
    const float room = std::max(1.0F, buttons_.front().x - kPadding - (bounds.x + kPadding));
    renderer.draw_text(text, bounds.x + kPadding, bounds.y + ((kHeight - kTextSize) / 2.0F) - 2.0F,
                       kTextSize,
                       folder.model().error() ? palette.error : palette.on_surface_variant, room,
                       ui::Ellipsis::middle);
}

std::optional<std::size_t> StatusBar::button_at(float x, float y) const {
    for (std::size_t i = 0; i < buttons_.size(); ++i) {
        const ui::Rect& button = buttons_.at(i);
        if (x >= button.x && x < button.right() && y >= button.y && y < button.bottom()) {
            return i;
        }
    }
    return std::nullopt;
}

bool StatusBar::claims(float x, float y) const {
    return !bounds_.empty() && x >= bounds_.x && x < bounds_.right() && y >= bounds_.y &&
           y < bounds_.bottom();
}

void StatusBar::pointer(const ui::PointerEvent& event) {
    using Kind = ui::PointerEvent::Kind;
    const std::optional<std::size_t> over =
        event.kind == Kind::leave ? std::nullopt : button_at(event.x, event.y);
    if (over != hovered_) {
        hovered_ = over;
        redraw_();
    }
    if (event.kind != Kind::press || event.button != ui::PointerButton::left || !over) {
        return;
    }
    if (*over == kViews) {
        if (help_) {
            help_();
        }
    } else if (choose_view_) {
        choose_view_(*over);
    }
}

}  // namespace nordend::views
