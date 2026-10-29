#include "nordend/ui/text_field.hpp"

#include <algorithm>
#include <cmath>
#include <xkbcommon/xkbcommon-keysyms.h>

#include "nordend/ui/text.hpp"

namespace nordend::ui {
namespace {

constexpr float kCaretWidth = 1.5F;

bool continuation(char c) { return (static_cast<unsigned char>(c) & 0xC0U) == 0x80U; }

// Characters that end a word for Ctrl+arrows: in a file name, the dots, dashes and underscores
// as well as spaces.
bool separator(char c) {
    return c == ' ' || c == '.' || c == '_' || c == '-' || c == '\t' || c == ',' || c == '(' ||
           c == ')' || c == '[' || c == ']';
}

// Text a key may type: no control characters.
bool typable(std::string_view text) {
    return !text.empty() && std::ranges::none_of(text, [](char c) {
        const auto byte = static_cast<unsigned char>(c);
        return byte < 0x20U || byte == 0x7FU;
    });
}

}  // namespace

TextField::TextField(std::string text) { set_text(std::move(text)); }

void TextField::set_text(std::string text) {
    text_ = std::move(text);
    caret_ = text_.size();
    anchor_ = caret_;
    scroll_ = 0.0F;
}

std::size_t TextField::previous(std::size_t at) const noexcept {
    if (at == 0) {
        return 0;
    }
    --at;
    while (at > 0 && continuation(text_[at])) {
        --at;
    }
    return at;
}

std::size_t TextField::next(std::size_t at) const noexcept {
    if (at >= text_.size()) {
        return text_.size();
    }
    ++at;
    while (at < text_.size() && continuation(text_[at])) {
        ++at;
    }
    return at;
}

std::size_t TextField::previous_word(std::size_t at) const noexcept {
    while (at > 0 && separator(text_[previous(at)])) {
        at = previous(at);
    }
    while (at > 0 && !separator(text_[previous(at)])) {
        at = previous(at);
    }
    return at;
}

std::size_t TextField::next_word(std::size_t at) const noexcept {
    while (at < text_.size() && separator(text_[at])) {
        at = next(at);
    }
    while (at < text_.size() && !separator(text_[at])) {
        at = next(at);
    }
    return at;
}

void TextField::select(std::size_t begin, std::size_t end) {
    const auto boundary = [this](std::size_t at) {
        at = std::min(at, text_.size());
        while (at > 0 && at < text_.size() && continuation(text_[at])) {
            --at;
        }
        return at;
    };
    anchor_ = boundary(begin);
    caret_ = boundary(end);
}

std::pair<std::size_t, std::size_t> TextField::selection() const noexcept {
    return {std::min(caret_, anchor_), std::max(caret_, anchor_)};
}

void TextField::move_to(std::size_t at, bool extend) {
    caret_ = at;
    if (!extend) {
        anchor_ = at;
    }
}

void TextField::erase(std::size_t first, std::size_t last) {
    text_.erase(first, last - first);
    caret_ = first;
    anchor_ = first;
}

void TextField::insert(std::string_view text) {
    const auto [first, last] = selection();
    text_.replace(first, last - first, text);
    caret_ = first + text.size();
    anchor_ = caret_;
}

bool TextField::key(const KeyEvent& event) {
    const bool shift = event.modifiers.shift;
    const bool ctrl = event.modifiers.ctrl;
    const auto [first, last] = selection();
    const bool selected = first != last;
    switch (event.keysym) {
        case XKB_KEY_Left:
            if (selected && !shift) {
                move_to(first, false);  // to the selection's start
            } else {
                move_to(ctrl ? previous_word(caret_) : previous(caret_), shift);
            }
            return true;
        case XKB_KEY_Right:
            if (selected && !shift) {
                move_to(last, false);
            } else {
                move_to(ctrl ? next_word(caret_) : next(caret_), shift);
            }
            return true;
        case XKB_KEY_Home:
        case XKB_KEY_Up:
            move_to(0, shift);
            return true;
        case XKB_KEY_End:
        case XKB_KEY_Down:
            move_to(text_.size(), shift);
            return true;
        case XKB_KEY_BackSpace:
            if (selected) {
                erase(first, last);
            } else {
                erase(ctrl ? previous_word(caret_) : previous(caret_), caret_);
            }
            return true;
        case XKB_KEY_Delete:
            if (selected) {
                erase(first, last);
            } else {
                erase(caret_, ctrl ? next_word(caret_) : next(caret_));
            }
            return true;
        default:
            break;
    }
    if (ctrl && !event.modifiers.alt && (event.keysym == XKB_KEY_a || event.keysym == XKB_KEY_A)) {
        select_all();
        return true;
    }
    if (!ctrl && !event.modifiers.alt && typable(event.text)) {
        insert(event.text);
        return true;
    }
    return false;
}

float TextField::width_of(FontSystem& fonts, std::size_t bytes) const {
    if (bytes == 0) {
        return 0.0F;
    }
    return fonts.layout(std::string_view{text_}.substr(0, bytes), size_, scale_).width / scale_;
}

void TextField::click(float x, bool extend) {
    if (fonts_ == nullptr) {
        return;
    }
    const float wanted = x - (bounds_.x + kPadding) + scroll_;
    std::size_t best = 0;
    float best_distance = wanted < 0.0F ? 0.0F : wanted;
    for (std::size_t at = next(0); at <= text_.size(); at = next(at)) {
        const float distance = std::abs(width_of(*fonts_, at) - wanted);
        if (distance < best_distance) {
            best = at;
            best_distance = distance;
        }
        if (at == text_.size()) {
            break;
        }
    }
    move_to(best, extend);
}

void TextField::draw(Renderer& renderer, const Rect& rect, float size, const FieldColors& colors) {
    bounds_ = rect;
    size_ = size;
    scale_ = renderer.scale();
    fonts_ = &renderer.fonts();
    renderer.draw_box(rect, {.fill = colors.background,
                             .radius = Corners::all(4.0F),
                             .border_width = 1.5F,
                             .border = colors.border});

    // Sideways scroll: the caret stays inside, and a text that fits is not scrolled.
    const Rect inner = rect.inset(kPadding);
    const float room = std::max(1.0F, inner.width - kCaretWidth);
    const float total = width_of(*fonts_, text_.size());
    const float caret_x = width_of(*fonts_, caret_);
    if (total <= room) {
        scroll_ = 0.0F;
    } else {
        scroll_ = std::clamp(scroll_, caret_x - room, caret_x);
        scroll_ = std::clamp(scroll_, 0.0F, total - room);
    }

    renderer.push_clip({inner.x, rect.y, inner.width, rect.height});  // sideways: descenders stay
    const float x = inner.x - scroll_;
    const float line = size + 6.0F;
    const float line_y = rect.y + ((rect.height - line) / 2.0F);
    const auto [first, last] = selection();
    if (first != last) {
        const float from = width_of(*fonts_, first);
        const float to = width_of(*fonts_, last);
        renderer.fill_rect({x + from, line_y, to - from, line}, colors.selection);
    }
    renderer.draw_text(text_, x, rect.y + ((rect.height - size) / 2.0F) - 2.0F, size, colors.text);
    renderer.fill_rect({x + caret_x, line_y, kCaretWidth, line}, colors.caret);
    renderer.pop_clip();
}

}  // namespace nordend::ui
