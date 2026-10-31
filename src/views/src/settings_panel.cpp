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

}  // namespace

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
    problem_.clear();
    redraw_();
}

void SettingsPanel::stop_editing() {
    editing_ = false;
    redraw_();
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
        if (event.keysym == XKB_KEY_Escape && plain(event)) {
            problem_.clear();
            stop_editing();
        } else if (enter && plain(event)) {
            confirm();
        } else if (field_.key(event)) {
            redraw_();
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
    if (!open_ || event.kind != ui::PointerEvent::Kind::press ||
        event.button != ui::PointerButton::left) {
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
    redraw_();
}

bool SettingsPanel::clickable_at(float x, float y) const {
    return open_ && !typing_at(x, y) &&
           std::ranges::any_of(boxes_, [x, y](const ui::Rect& box) { return box.contains(x, y); });
}

bool SettingsPanel::typing_at(float x, float y) const {
    return open_ && editing_ && field_.bounds().contains(x, y);
}

void SettingsPanel::draw(ui::Renderer& renderer, const ui::Rect& bounds,
                         const theme::Palette& palette) {
    boxes_ = {};
    card_ = {};
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
}

}  // namespace nordend::views
