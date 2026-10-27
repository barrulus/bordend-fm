#include "nordend/views/quick_look.hpp"

#include <algorithm>
#include <chrono>
#include <format>
#include <unordered_set>

#include "draw_helpers.hpp"
#include "nordend/fs/icon.hpp"
#include "nordend/views/format.hpp"

namespace nordend::views {
namespace {

constexpr float kMargin = 32.0F;         // around the image
constexpr float kCaptionHeight = 44.0F;  // under it
constexpr float kRadius = 6.0F;
constexpr float kCardWidth = 340.0F;
constexpr float kCardIcon = 96.0F;
constexpr auto kFade = std::chrono::milliseconds{140};

// Where a shadow falls: a little below what casts it.
ui::Rect shifted_down(ui::Rect rect) {
    rect.y += 6.0F;
    return rect;
}

float fade_at(QuickLook::Clock::time_point opened, QuickLook::Clock::time_point now) {
    const auto elapsed = std::chrono::duration<float>(now - opened).count();
    return std::clamp(elapsed / std::chrono::duration<float>(kFade).count(), 0.0F, 1.0F);
}

}  // namespace

QuickLook::QuickLook(core::WorkerPool& pool, std::shared_ptr<const fs::MimeDatabase> mime,
                     std::shared_ptr<const thumbs::Cache> thumbnails, std::function<void()> redraw)
    : pool_{pool},
      mime_{std::move(mime)},
      thumbnails_{std::move(thumbnails)},
      redraw_{std::move(redraw)} {}

QuickLook::~QuickLook() {
    *alive_ = false;
    for (auto& [key, slot] : slots_) {
        drop(slot);
    }
}

bool QuickLook::previewable(const fs::Entry& entry) const {
    return !entry.is_directory() && thumbs::can_make(mime_->name(folder_->mime_of(entry)));
}

std::string QuickLook::key_of(const fs::Entry& entry) const {
    return std::format("{}\n{}", (folder_->path() / entry.name).string(), entry.modified_ns);
}

void QuickLook::drop(Slot& slot) {
    for (std::optional<core::CancelToken>* job : {&slot.thumbnail_job, &slot.image_job}) {
        if (*job) {
            (*job)->cancel();
            job->reset();
        }
    }
    for (std::unique_ptr<ui::Texture>* texture : {&slot.thumbnail_texture, &slot.image_texture}) {
        if (*texture) {
            retired_.push_back(std::move(*texture));
        }
    }
}

void QuickLook::open(std::shared_ptr<Folder> folder, Clock::time_point now) {
    opened_ = now;
    follow(std::move(folder));
}

void QuickLook::close() {
    for (auto& [key, slot] : slots_) {
        drop(slot);
    }
    slots_.clear();
    folder_.reset();
    shown_name_.clear();
    shown_key_.clear();
    redraw_();
}

QuickLook::Slot& QuickLook::ensure(const fs::Entry& entry, bool current) {
    const std::string key = key_of(entry);
    Slot& slot = slots_[key];
    slot.path = folder_->path() / entry.name;
    if (!slot.image && !slot.image_job) {
        slot.image_job = pool_.submit(
            current ? core::Priority::visible : core::Priority::nearby,
            [path = slot.path, width = target_width_,
             height = target_height_](const core::CancelToken& /*token*/) {
                return std::make_shared<image::Fitted>(image::load_to_fit(path, width, height));
            },
            [this, alive = alive_, key](std::shared_ptr<image::Fitted> fitted) {
                if (!*alive) {
                    return;
                }
                const auto it = slots_.find(key);
                if (it == slots_.end()) {
                    return;
                }
                it->second.image_job.reset();
                it->second.image = std::move(fitted);
                redraw_();
            });
    }
    // The first stage, while the sharp one is made: the thumbnail, usually cached already.
    if (current && thumbnails_ && !slot.image && !slot.thumbnail_job && !slot.thumbnail_pixels &&
        !slot.thumbnail_texture) {
        slot.thumbnail_job = pool_.submit(
            core::Priority::visible,
            [cache = thumbnails_, path = slot.path](const core::CancelToken& /*token*/) {
                return std::make_shared<image::Image>(cache->get(path, thumbs::Size::large).image);
            },
            [this, alive = alive_, key](std::shared_ptr<image::Image> pixels) {
                if (!*alive) {
                    return;
                }
                const auto it = slots_.find(key);
                if (it == slots_.end()) {
                    return;
                }
                it->second.thumbnail_job.reset();
                if (!pixels->empty()) {
                    it->second.thumbnail_pixels = std::move(pixels);
                    redraw_();
                }
            });
    }
    return slot;
}

void QuickLook::follow(std::shared_ptr<Folder> folder) {
    if (!folder) {
        close();
        return;
    }
    folder_ = std::move(folder);
    const fs::Entry* const entry = folder_->cursor_entry();
    const std::optional<std::size_t> cursor = folder_->cursor();
    if (entry == nullptr || !cursor) {
        close();
        return;
    }
    // Checked again on every call (each frame while open): an entry's type may only be known
    // once its content has been sniffed.
    const std::string key = key_of(*entry);
    const bool moved = key != shown_key_;
    shown_name_ = entry->name;
    shown_key_ = key;
    const std::vector<fs::Entry>& entries = folder_->entries();
    const std::size_t index = *cursor;
    if (moved) {
        folder_->sniff(index == 0 ? 0 : index - 1, index + 2);
    }
    std::unordered_set<std::string> keep{key};
    if (previewable(*entry)) {
        ensure(*entry, true);
    }
    // The neighbours, so that the next arrow finds them ready.
    for (const std::size_t neighbour : {index - 1, index + 1}) {
        if (neighbour < entries.size() && previewable(entries[neighbour])) {
            ensure(entries[neighbour], false);
            keep.insert(key_of(entries[neighbour]));
        }
    }
    for (auto it = slots_.begin(); it != slots_.end();) {
        if (keep.contains(it->first)) {
            ++it;
        } else {
            drop(it->second);
            it = slots_.erase(it);
        }
    }
    if (moved) {
        redraw_();
    }
}

bool QuickLook::animating(Clock::time_point now) const {
    return is_open() && fade_at(opened_, now) < 1.0F;
}

bool QuickLook::loading() const {
    const auto it = slots_.find(shown_key_);
    return it != slots_.end() && it->second.image_job.has_value();
}

std::optional<std::pair<std::uint32_t, std::uint32_t>> QuickLook::image_size() const {
    const auto it = slots_.find(shown_key_);
    if (it == slots_.end() || !it->second.image || it->second.image->image.empty()) {
        return std::nullopt;
    }
    return std::pair{it->second.image->image.width, it->second.image->image.height};
}

void QuickLook::draw(ui::Renderer& renderer, const ui::Rect& bounds, const theme::Palette& palette,
                     ui::IconSet& icons, Clock::time_point now) {
    retired_.clear();
    if (!is_open()) {
        return;
    }
    const fs::Entry* const entry = folder_->cursor_entry();
    if (entry == nullptr) {
        return;
    }
    const float fade = fade_at(opened_, now);
    renderer.fill_rect(bounds, palette.scrim.with_alpha(0.78F * fade));
    const ui::Rect area{bounds.x + kMargin, bounds.y + kMargin, bounds.width - (2.0F * kMargin),
                        bounds.height - (2.0F * kMargin) - kCaptionHeight};
    // Images are made for this area, in physical pixels (from the next one on).
    target_width_ = static_cast<std::uint32_t>(std::max(1.0F, area.width * renderer.scale()));
    target_height_ = static_cast<std::uint32_t>(std::max(1.0F, area.height * renderer.scale()));
    if (previewable(*entry)) {
        draw_image(renderer, area, palette, icons, *entry, fade);
    } else {
        draw_card(renderer, area, palette, icons, *entry, fade);
    }
}

void QuickLook::draw_image(ui::Renderer& renderer, const ui::Rect& area,
                           const theme::Palette& palette, ui::IconSet& icons,
                           const fs::Entry& entry, float fade) {
    const auto it = slots_.find(shown_key_);
    if (it == slots_.end()) {
        return;
    }
    Slot& slot = it->second;
    // Upload what arrived: the sharp image replaces the thumbnail.
    if (slot.image && !slot.image->image.empty() && !slot.image_texture) {
        slot.image_texture = std::make_unique<ui::Texture>(
            slot.image->image.width, slot.image->image.height, slot.image->image.rgba);
        if (slot.thumbnail_texture) {
            retired_.push_back(std::move(slot.thumbnail_texture));
        }
        slot.thumbnail_pixels.reset();
    }
    if (slot.thumbnail_pixels && !slot.image_texture) {
        slot.thumbnail_texture = std::make_unique<ui::Texture>(slot.thumbnail_pixels->width,
                                                               slot.thumbnail_pixels->height,
                                                               slot.thumbnail_pixels->rgba);
        slot.thumbnail_pixels.reset();
    }
    const ui::Texture* const texture =
        slot.image_texture ? slot.image_texture.get() : slot.thumbnail_texture.get();

    std::string caption = entry.name;
    if (slot.image && slot.image->full_width > 0) {
        caption += std::format("   {} × {}", slot.image->full_width, slot.image->full_height);
    }
    caption += "   " + format_size(entry.size);
    if (texture == nullptr) {
        // Nothing yet (or it failed): the file's icon, and why when there is a reason.
        fs::Entry typed = entry;
        typed.mime = folder_->mime_of(entry);
        const float x = area.x + ((area.width - kCardIcon) / 2.0F);
        const float y = area.y + ((area.height - kCardIcon) / 2.0F);
        icons.draw(renderer, fs::icon_name(*mime_, typed), x, y, kCardIcon,
                   palette.on_surface_variant.with_alpha(fade));
        if (slot.image && !slot.image->error.empty()) {
            draw_centered(renderer, slot.image->error, area, y + kCardIcon + 16.0F, 13.0F,
                          palette.error.with_alpha(fade));
        }
    } else {
        // The sharp image fitted to the area, never larger than its own pixels; a thumbnail
        // fills the area, where the sharp image is likely to land.
        const auto texture_width = static_cast<float>(texture->width());
        const auto texture_height = static_cast<float>(texture->height());
        float fit = std::min(area.width / texture_width, area.height / texture_height);
        if (texture == slot.image_texture.get()) {
            fit = std::min(fit, 1.0F / renderer.scale());
        }
        const float shown_width = texture_width * fit;
        const float shown_height = texture_height * fit;
        const ui::Rect rect{area.x + ((area.width - shown_width) / 2.0F),
                            area.y + ((area.height - shown_height) / 2.0F), shown_width,
                            shown_height};
        renderer.draw_shadow(shifted_down(rect), ui::Corners::all(kRadius), 18.0F,
                             palette.shadow.with_alpha(0.6F * fade));
        renderer.draw_image(*texture, rect, ui::Corners::all(kRadius), fade);
    }
    // The caption, in a pill under the image.
    const float text_width_px = std::min(text_width(renderer, caption, 13.0F), area.width - 32.0F);
    const ui::Rect pill{area.x + ((area.width - text_width_px) / 2.0F) - 14.0F,
                        area.bottom() + 12.0F, text_width_px + 28.0F, 26.0F};
    renderer.draw_box(pill, {.fill = palette.surface_container_high.with_alpha(0.92F * fade),
                             .radius = ui::Corners::all(13.0F)});
    renderer.draw_text(caption, pill.x + 14.0F, pill.y + 5.0F, 13.0F,
                       palette.on_surface.with_alpha(fade), text_width_px, ui::Ellipsis::middle);
}

void QuickLook::draw_card(ui::Renderer& renderer, const ui::Rect& area,
                          const theme::Palette& palette, ui::IconSet& icons, const fs::Entry& entry,
                          float fade) {
    fs::Entry typed = entry;
    typed.mime = folder_->mime_of(entry);
    const std::string_view type =
        typed.mime == fs::kUnknownMime ? std::string_view{"…"} : mime_->name(typed.mime);
    const float height = 32.0F + kCardIcon + 20.0F + 24.0F + 20.0F + 24.0F + 24.0F + 24.0F;
    const ui::Rect card{area.x + ((area.width - kCardWidth) / 2.0F),
                        area.y + ((area.height - height) / 2.0F), kCardWidth, height};
    renderer.draw_shadow(shifted_down(card), ui::Corners::all(14.0F), 18.0F,
                         palette.shadow.with_alpha(0.6F * fade));
    renderer.draw_box(card, {.fill = palette.surface_container_high.with_alpha(fade),
                             .radius = ui::Corners::all(14.0F)});
    float y = card.y + 32.0F;
    icons.draw(
        renderer, fs::icon_name(*mime_, typed), card.x + ((kCardWidth - kCardIcon) / 2.0F), y,
        kCardIcon,
        (entry.is_directory() ? palette.primary : palette.on_surface_variant).with_alpha(fade));
    y += kCardIcon + 20.0F;
    draw_centered(renderer, entry.name, card, y, 15.0F, palette.on_surface.with_alpha(fade));
    y += 24.0F;
    draw_centered(renderer, type, card, y, 12.0F, palette.on_surface_variant.with_alpha(fade));
    y += 20.0F + 4.0F;
    const auto row = [&](std::string_view label, const std::string& value) {
        renderer.draw_text(label, card.x + 20.0F, y, 13.0F,
                           palette.on_surface_variant.with_alpha(fade));
        draw_right_aligned(renderer, value, card.right() - 20.0F, y, 13.0F,
                           palette.on_surface.with_alpha(fade));
        y += 24.0F;
    };
    if (!entry.is_directory()) {
        row("Size", format_size(entry.size));
    }
    row("Modified", format_time(entry.modified_ns));
}

}  // namespace nordend::views
