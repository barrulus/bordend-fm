#include "nordend/views/settings_panel.hpp"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <xkbcommon/xkbcommon-keysyms.h>

namespace nordend::views {
namespace {

constexpr float kCardWidth = 560.0F;
constexpr float kMargin = 32.0F;   // around the card, at least
constexpr float kPadding = 28.0F;  // inside the card
constexpr float kTitleSize = 16.0F;
constexpr float kFileSize = 12.0F;
constexpr float kHeadHeight = 64.0F;
constexpr float kGroupSize = 11.0F;
constexpr float kGroupHeight = 30.0F;
constexpr float kGroupGap = 14.0F;
constexpr float kRowHeight = 40.0F;
constexpr float kBoxHeight = 30.0F;
constexpr float kBoxShare = 0.55F;  // of the card's inside, for the values
constexpr float kTextSize = 13.0F;
constexpr float kNoteSize = 12.0F;
constexpr float kNoteHeight = 20.0F;
constexpr float kFootHeight = 36.0F;
constexpr float kSuggestionHeight = 26.0F;
constexpr std::size_t kSuggestionsShown = 8;

// The names fontconfig resolves itself, offered before the installed families.
constexpr std::array<std::string_view, 3> kGeneric{"sans-serif", "serif", "monospace"};

using Value = SettingsPanel::Value;

struct Row {
    Value value;
    std::string_view label;
    std::string_view section;
    std::string_view key;
    bool number;
};

constexpr std::array<Row, SettingsPanel::kValues> kRows{{
    {Value::font, "Interface", "font", "family", false},
    {Value::monospace, "Monospace (text previews)", "font", "monospace", false},
    {Value::double_click_ms, "Double-click time (ms)", "pointer", "double_click_ms", true},
    {Value::double_click_distance, "Double-click distance (px)", "pointer", "double_click_distance",
     true},
}};

std::string_view trim(std::string_view text) {
    constexpr std::string_view blank = " \t";
    const std::size_t first = text.find_first_not_of(blank);
    if (first == std::string_view::npos) {
        return {};
    }
    return text.substr(first, text.find_last_not_of(blank) - first + 1);
}

bool plain(const ui::KeyEvent& event) { return event.modifiers == ui::Modifiers{}; }

char lower(char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; }

std::string lowered(std::string_view text) {
    std::string out{text};
    std::ranges::transform(out, out.begin(), lower);
    return out;
}

// The family part of a fontconfig pattern: before any ':' (properties) or '-' size.
std::string_view family_of(std::string_view typed) {
    return trim(typed.substr(0, typed.find(':')));
}

}  // namespace

std::vector<ui::FontFamily> font_suggestions(std::span<const ui::FontFamily> families,
                                             std::string_view typed, bool monospace_first) {
    const std::string wanted = lowered(family_of(typed));
    // Lower ranks first: generic names, then (when asked) fixed-width, then names starting with
    // what is typed; the families' own order (by name) after that.
    const auto rank = [&](const ui::FontFamily& family, bool generic) {
        const std::string name = lowered(family.name);
        int r = generic ? 0 : 4;
        if (monospace_first && !family.monospace) {
            r += 2;
        }
        if (!name.starts_with(wanted)) {
            r += 1;
        }
        return r;
    };
    std::vector<std::pair<int, ui::FontFamily>> ranked;
    for (const std::string_view generic : kGeneric) {
        if (lowered(generic).find(wanted) != std::string::npos) {
            const ui::FontFamily family{.name = std::string{generic},
                                        .monospace = generic == "monospace"};
            ranked.emplace_back(rank(family, true), family);
        }
    }
    for (const ui::FontFamily& family : families) {
        if (lowered(family.name).find(wanted) != std::string::npos) {
            ranked.emplace_back(rank(family, false), family);
        }
    }
    std::ranges::stable_sort(ranked, {}, &std::pair<int, ui::FontFamily>::first);
    std::vector<ui::FontFamily> out;
    out.reserve(ranked.size());
    for (auto& [r, family] : ranked) {
        out.push_back(std::move(family));
    }
    return out;
}

bool font_known(std::span<const ui::FontFamily> families, std::string_view typed) {
    if (typed.find(':') != std::string_view::npos) {
        return true;  // a pattern with properties: fontconfig's to resolve
    }
    const std::string wanted = lowered(trim(typed));
    return std::ranges::any_of(kGeneric,
                               [&](std::string_view generic) { return generic == wanted; }) ||
           std::ranges::any_of(families, [&](const ui::FontFamily& family) {
               return lowered(family.name) == wanted;
           });
}

SettingsPanel::SettingsPanel(std::function<void()> redraw) : redraw_{std::move(redraw)} {}

void SettingsPanel::open() {
    if (open_) {
        return;
    }
    open_ = true;
    chosen_ = 0;
    editing_ = false;
    problem_.clear();
    refresh();
}

void SettingsPanel::close() {
    if (!open_) {
        return;
    }
    open_ = false;
    editing_ = false;
    problem_.clear();
    redraw_();
}

void SettingsPanel::refresh() {
    if (settings_.read) {
        config_ = settings_.read();
    }
    redraw_();
}

std::string SettingsPanel::text_of(Value value) const {
    switch (value) {
        case Value::font:
            return config_.font.family;
        case Value::monospace:
            return config_.font.monospace;
        case Value::double_click_ms:
            return std::to_string(config_.pointer.double_click_ms);
        case Value::double_click_distance:
            return std::to_string(config_.pointer.double_click_distance);
    }
    return {};
}

void SettingsPanel::set_families(std::vector<ui::FontFamily> families) {
    families_ = std::move(families);
    suggest();
}

bool SettingsPanel::font_chosen() const noexcept {
    return kRows.at(chosen_).value == Value::font || kRows.at(chosen_).value == Value::monospace;
}

void SettingsPanel::suggest() {
    suggestions_.clear();
    if (editing_ && font_chosen() && !families_.empty()) {
        // Until something is typed, every family: the current one is only a starting point.
        suggestions_ = font_suggestions(families_, typed_ ? field_.text() : std::string{},
                                        kRows.at(chosen_).value == Value::monospace);
    }
    picked_.reset();
    first_ = 0;
    redraw_();
}

void SettingsPanel::pick(std::optional<std::size_t> index) {
    picked_ = index;
    if (picked_) {
        // Kept in sight as the list scrolls.
        if (*picked_ < first_) {
            first_ = *picked_;
        } else if (*picked_ >= first_ + kSuggestionsShown) {
            first_ = *picked_ + 1 - kSuggestionsShown;
        }
    }
    redraw_();
}

void SettingsPanel::choose(std::size_t value) {
    if (value != chosen_) {
        chosen_ = value;
        problem_.clear();
    }
    redraw_();
}

void SettingsPanel::start_editing() {
    field_.set_text(text_of(kRows.at(chosen_).value));
    field_.select_all();
    editing_ = true;
    typed_ = false;
    problem_.clear();
    suggest();
}

void SettingsPanel::stop_editing() {
    editing_ = false;
    suggest();
}

void SettingsPanel::confirm() {
    const Row& row = kRows.at(chosen_);
    const std::string_view text = trim(field_.text());
    std::string value;
    if (row.number) {
        std::int64_t number = 0;
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), number);
        if (text.empty() || error != std::errc{} || end != text.data() + text.size()) {
            problem_ = "a whole number, please";
            redraw_();
            return;
        }
        value = std::to_string(number);
    } else {
        if (!families_.empty() && !font_known(families_, text)) {
            problem_ = "not installed: choose one from the list";
            typed_ = true;
            suggest();
            return;
        }
        value = core::toml_string(text);
    }
    if (settings_.write) {
        problem_ = settings_.write(row.section, row.key, std::move(value));
    }
    if (problem_.empty()) {
        stop_editing();
        refresh();
    } else {
        redraw_();
    }
}

bool SettingsPanel::key(const ui::KeyEvent& event) {
    if (!open_) {
        return false;
    }
    const bool enter = event.keysym == XKB_KEY_Return || event.keysym == XKB_KEY_KP_Enter;
    if (editing_) {
        const bool listed = !suggestions_.empty();
        if (event.keysym == XKB_KEY_Escape && plain(event)) {
            problem_.clear();
            stop_editing();
        } else if (listed && event.keysym == XKB_KEY_Down && plain(event)) {
            pick(picked_ ? std::min(*picked_ + 1, suggestions_.size() - 1) : 0);
        } else if (listed && event.keysym == XKB_KEY_Up && plain(event)) {
            // Above the first: back to what is typed.
            pick(picked_ && *picked_ > 0 ? std::optional{*picked_ - 1} : std::nullopt);
        } else if (enter && plain(event)) {
            if (picked_ && *picked_ < suggestions_.size()) {
                field_.set_text(suggestions_.at(*picked_).name);
            }
            confirm();
        } else {
            const std::string before = field_.text();
            if (field_.key(event)) {
                if (field_.text() != before) {
                    typed_ = true;
                    suggest();
                }
                redraw_();
            }
        }
        return true;  // every key is the value's meanwhile
    }
    const bool ctrl_comma =
        event.keysym == XKB_KEY_comma && event.modifiers == ui::Modifiers{.ctrl = true};
    if (ctrl_comma || (event.keysym == XKB_KEY_Escape && plain(event))) {
        close();
    } else if (event.keysym == XKB_KEY_Down || (event.keysym == XKB_KEY_Tab && plain(event))) {
        choose((chosen_ + 1) % kValues);
    } else if (event.keysym == XKB_KEY_Up || event.keysym == XKB_KEY_ISO_Left_Tab) {
        choose((chosen_ + kValues - 1) % kValues);
    } else if ((enter || event.keysym == XKB_KEY_F2) && plain(event)) {
        start_editing();
    }
    return true;  // nothing under the card reacts meanwhile
}

void SettingsPanel::pointer(const ui::PointerEvent& event) {
    if (!open_) {
        return;
    }
    if (event.kind == ui::PointerEvent::Kind::scroll && list_.contains(event.x, event.y)) {
        const float steps = event.wheel_y != 0.0F ? event.wheel_y : event.scroll_y / 20.0F;
        const std::size_t last =
            suggestions_.size() > kSuggestionsShown ? suggestions_.size() - kSuggestionsShown : 0;
        if (steps > 0.0F) {
            first_ = std::min(last, first_ + static_cast<std::size_t>(std::max(1.0F, steps)));
        } else if (steps < 0.0F) {
            const auto back = static_cast<std::size_t>(std::max(1.0F, -steps));
            first_ = first_ > back ? first_ - back : 0;
        }
        redraw_();
        return;
    }
    if (event.kind != ui::PointerEvent::Kind::press || event.button != ui::PointerButton::left) {
        return;
    }
    for (std::size_t i = 0; i < rows_.size(); ++i) {
        if (rows_.at(i).contains(event.x, event.y) && first_ + i < suggestions_.size()) {
            field_.set_text(suggestions_.at(first_ + i).name);
            confirm();
            return;
        }
    }
    if (list_.contains(event.x, event.y)) {
        return;
    }
    if (editing_ && field_.bounds().contains(event.x, event.y)) {
        field_.click(event.x, false);
        redraw_();
        return;
    }
    for (std::size_t i = 0; i < boxes_.size(); ++i) {
        if (boxes_.at(i).contains(event.x, event.y)) {
            choose(i);
            start_editing();
            return;
        }
    }
    if (!card_.contains(event.x, event.y)) {
        close();
    }
}

void SettingsPanel::paste(std::string_view text) {
    if (!editing_) {
        return;
    }
    field_.insert(text.substr(0, text.find_first_of("\r\n")));
    typed_ = true;
    suggest();
}

bool SettingsPanel::clickable_at(float x, float y) const {
    const auto inside = [x, y](const ui::Rect& rect) { return rect.contains(x, y); };
    return open_ && !typing_at(x, y) &&
           (std::ranges::any_of(rows_, inside) ||
            (!list_.contains(x, y) && std::ranges::any_of(boxes_, inside)));
}

bool SettingsPanel::typing_at(float x, float y) const {
    return open_ && editing_ && field_.bounds().contains(x, y);
}

void SettingsPanel::draw(ui::Renderer& renderer, const ui::Rect& bounds,
                         const theme::Palette& palette) {
    boxes_ = {};
    card_ = {};
    rows_.clear();
    list_ = {};
    if (!open_) {
        return;
    }
    renderer.fill_rect(bounds, palette.scrim.with_alpha(0.55F));
    const core::Config::Font in_use =
        settings_.fonts_in_use ? settings_.fonts_in_use() : config_.font;
    const bool fonts_later =
        in_use.family != config_.font.family || in_use.monospace != config_.font.monospace;

    float content = kHeadHeight + (3.0F * kGroupHeight) + (2.0F * kGroupGap) +
                    (static_cast<float>(kValues + 1) * kRowHeight) + kFootHeight;
    content += problem_.empty() ? 0.0F : kNoteHeight;
    content += fonts_later ? kNoteHeight : 0.0F;
    const float width = std::min(kCardWidth, bounds.width - (2.0F * kMargin));
    const float height = std::min(bounds.height - (2.0F * kMargin), content + (2.0F * kPadding));
    card_ = {bounds.x + ((bounds.width - width) / 2.0F),
             bounds.y + ((bounds.height - height) / 2.0F), width, height};
    renderer.draw_box(card_, {.fill = palette.surface_container_high,
                              .radius = ui::Corners::all(14.0F),
                              .border_width = 1.0F,
                              .border = palette.outline_variant});
    renderer.push_clip(card_);

    const float left = card_.x + kPadding;
    const float inner = width - (2.0F * kPadding);
    const float box_width = inner * kBoxShare;
    const float box_left = left + inner - box_width;
    float y = card_.y + kPadding;
    renderer.draw_text("Settings", left, y, kTitleSize, palette.on_surface);
    renderer.draw_text(settings_.file, left, y + 26.0F, kFileSize, palette.on_surface_variant,
                       inner, ui::Ellipsis::middle);
    y += kHeadHeight;

    const auto group = [&](std::string_view name) {
        renderer.fill_rect({left, y, inner, 1.0F / renderer.scale()}, palette.outline_variant);
        renderer.draw_text(name, left, y + 10.0F, kGroupSize, palette.on_surface_variant);
        y += kGroupHeight;
    };
    const auto note = [&](std::string_view text, ui::Color color) {
        renderer.draw_text(text, box_left, y - 4.0F, kNoteSize, color, box_width);
        y += kNoteHeight;
    };
    const auto row = [&](std::size_t i) {
        const Row& r = kRows.at(i);
        const float text_y = y + ((kRowHeight - kTextSize) / 2.0F) - 2.0F;
        renderer.draw_text(r.label, left, text_y, kTextSize, palette.on_surface,
                           inner - box_width - 12.0F);
        const ui::Rect box{box_left, y + ((kRowHeight - kBoxHeight) / 2.0F), box_width, kBoxHeight};
        boxes_.at(i) = box;
        const bool chosen = i == chosen_;
        if (chosen && editing_) {
            field_.draw(renderer, box, kTextSize,
                        {.background = palette.surface_container_highest,
                         .border = problem_.empty() ? palette.primary : palette.error,
                         .text = palette.on_surface,
                         .selection = palette.secondary_container,
                         .caret = palette.on_surface});
        } else {
            renderer.draw_box(box, {.fill = palette.surface_container_highest,
                                    .radius = ui::Corners::all(6.0F),
                                    .border_width = chosen ? 1.5F : 0.0F,
                                    .border = palette.primary});
            renderer.draw_text(text_of(r.value), box.x + 10.0F, text_y, kTextSize,
                               palette.on_surface, box.width - 20.0F);
        }
        y += kRowHeight;
        if (chosen && !problem_.empty()) {
            note(problem_, palette.error);
        }
    };

    group("FONTS");
    row(0);
    row(1);
    if (fonts_later) {
        note("Fonts change at the next start", palette.on_surface_variant);
    }
    y += kGroupGap;
    group("POINTER");
    row(2);
    row(3);
    y += kGroupGap;
    group("COLOURS");
    renderer.draw_text("From", left, y + ((kRowHeight - kTextSize) / 2.0F) - 2.0F, kTextSize,
                       palette.on_surface);
    renderer.draw_text(settings_.colours ? settings_.colours() : std::string{}, box_left,
                       y + ((kRowHeight - kTextSize) / 2.0F) - 2.0F, kTextSize,
                       palette.on_surface_variant, box_width);
    y += kRowHeight;

    renderer.draw_text(
        editing_ ? "Enter keep  ·  Esc drop" : "↑ ↓ choose  ·  Enter change  ·  Esc close", left,
        card_.bottom() - kPadding - kTextSize, 12.0F, palette.on_surface_variant, inner);
    renderer.pop_clip();

    // Nothing installed matches what is typed: say so, where the list would be.
    if (editing_ && font_chosen() && !families_.empty() && suggestions_.empty()) {
        const ui::Rect& box = boxes_.at(chosen_);
        const float top = box.bottom() + 4.0F + (problem_.empty() ? 0.0F : kNoteHeight);
        list_ = {box.x, top, box.width, kSuggestionHeight + 8.0F};
        renderer.draw_box(list_, {.fill = palette.surface_container_highest,
                                  .radius = ui::Corners::all(8.0F),
                                  .border_width = 1.0F,
                                  .border = palette.outline_variant});
        renderer.draw_text("No installed font matches", list_.x + 12.0F,
                           list_.y + 4.0F + ((kSuggestionHeight - kTextSize) / 2.0F) - 2.0F,
                           kTextSize, palette.on_surface_variant, list_.width - 24.0F);
    }

    // The families offered, over the card's lower part, under the font being changed.
    if (editing_ && !suggestions_.empty()) {
        const ui::Rect& box = boxes_.at(chosen_);
        const std::size_t shown = std::min(kSuggestionsShown, suggestions_.size() - first_);
        const float top = box.bottom() + 4.0F + (problem_.empty() ? 0.0F : kNoteHeight);
        list_ = {box.x, top, box.width, (static_cast<float>(shown) * kSuggestionHeight) + 8.0F};
        renderer.draw_box(list_, {.fill = palette.surface_container_highest,
                                  .radius = ui::Corners::all(8.0F),
                                  .border_width = 1.0F,
                                  .border = palette.outline_variant});
        const bool mono_row = kRows.at(chosen_).value == Value::monospace;
        for (std::size_t i = 0; i < shown; ++i) {
            const ui::FontFamily& family = suggestions_.at(first_ + i);
            const ui::Rect line{list_.x + 4.0F,
                                list_.y + 4.0F + (static_cast<float>(i) * kSuggestionHeight),
                                list_.width - 8.0F, kSuggestionHeight};
            rows_.push_back(line);
            const bool picked = picked_ == first_ + i;
            if (picked) {
                renderer.draw_box(
                    line, {.fill = palette.primary_container, .radius = ui::Corners::all(6.0F)});
            }
            const ui::Color text = picked ? palette.on_primary_container : palette.on_surface;
            const float text_y = line.y + ((kSuggestionHeight - kTextSize) / 2.0F) - 2.0F;
            const bool generic = std::ranges::find(kGeneric, family.name) != kGeneric.end();
            std::string_view tag;
            if (generic) {
                tag = "default";
            } else if (mono_row && family.monospace) {
                tag = "mono";
            }
            const float tag_width = tag.empty() ? 0.0F : 56.0F;
            renderer.draw_text(family.name, line.x + 8.0F, text_y, kTextSize, text,
                               line.width - 16.0F - tag_width);
            if (!tag.empty()) {
                renderer.draw_text(tag, line.right() - tag_width, text_y + 1.0F, kNoteSize,
                                   picked ? text : palette.on_surface_variant, tag_width - 8.0F);
            }
        }
        if (suggestions_.size() > kSuggestionsShown) {
            // Where the shown part is in the whole list.
            const float track = list_.height - 8.0F;
            const auto count = static_cast<float>(suggestions_.size());
            renderer.draw_box({list_.right() - 5.0F,
                               list_.y + 4.0F + (track * static_cast<float>(first_) / count), 3.0F,
                               track * static_cast<float>(kSuggestionsShown) / count},
                              {.fill = palette.outline, .radius = ui::Corners::all(1.5F)});
        }
    }
}

}  // namespace nordend::views
