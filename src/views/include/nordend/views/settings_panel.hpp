#pragma once

#include <array>
#include <cstddef>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "nordend/core/config.hpp"
#include "nordend/theme/palette.hpp"
#include "nordend/ui/geometry.hpp"
#include "nordend/ui/renderer.hpp"
#include "nordend/ui/seat.hpp"
#include "nordend/ui/text.hpp"
#include "nordend/ui/text_field.hpp"

namespace nordend::views {

// What the settings panel reads and changes, wired by main (faked in tests).
struct Settings {
    // config.toml's values as they are now.
    std::function<core::Config()> read;
    // Sets one value in config.toml (value written as TOML, see core::toml_string): the problem
    // when it is refused and nothing was written, empty when it was.
    std::function<std::string(std::string_view section, std::string_view key, std::string value)>
        write;
    // Where the colours come from, in a few words.
    std::function<std::string()> colours;
    // The fonts in use: they change at the next start, and the panel says so when the file
    // names others.
    std::function<core::Config::Font()> fonts_in_use;
    // Where config.toml is, as shown.
    std::string file;
    // The installed font families, offered while a font is typed; called on a worker.
    std::function<std::vector<ui::FontFamily>()> families;
    // "About": what Nordend runs on, label and value per line; called on a worker.
    std::function<std::vector<std::pair<std::string, std::string>>()> about;
};

// The families offered while a font is typed (ROADMAP 7.5c-bis): those whose name holds what is
// typed before any ':' (case aside), the generic names ("sans-serif", "monospace") first, then
// with monospace_first the fixed-width families, names starting with it before the others.
[[nodiscard]] std::vector<ui::FontFamily> font_suggestions(std::span<const ui::FontFamily> families,
                                                           std::string_view typed,
                                                           bool monospace_first);

// typed names an installed family or a generic one (case aside), or is a fontconfig pattern
// with properties ("Inter:weight=medium"), which fontconfig takes as it is.
[[nodiscard]] bool font_known(std::span<const ui::FontFamily> families, std::string_view typed);

// The settings (Ctrl+, or the status bar's gear, ROADMAP 7.5c), on a card over the window: the
// fonts and the double click, changed in place and written to config.toml; where the colours come
// from. Up, Down and Tab choose a value, Enter or a click changes it, Enter keeps the change and
// Escape drops it; Escape, Ctrl+, or a click outside the card close it. Left and Right (or a click
// on its title) turn to "About" (ROADMAP 7.5d): what Nordend runs on, and Enter or "Copy" puts
// it on the clipboard for a bug report.
//
// Loop thread only.
class SettingsPanel {
public:
    explicit SettingsPanel(std::function<void()> redraw);

    void set_settings(Settings settings) { settings_ = std::move(settings); }

    [[nodiscard]] const Settings& settings() const noexcept { return settings_; }

    // Copies text to the clipboard (the browser's).
    void set_copy(std::function<void(std::string)> copy) { copy_ = std::move(copy); }

    enum class Page : std::uint8_t { settings, about };

    [[nodiscard]] Page page() const noexcept { return page_; }

    void turn(Page page);

    // "About"'s lines, once read; the text "Copy" puts on the clipboard.
    void set_about(std::vector<std::pair<std::string, std::string>> lines);

    [[nodiscard]] bool has_about() const noexcept { return !about_.empty(); }

    [[nodiscard]] std::string about_text() const;
    void copy_about();

    [[nodiscard]] bool copied() const noexcept { return copied_; }

    // The installed families, once read.
    void set_families(std::vector<ui::FontFamily> families);

    [[nodiscard]] bool has_families() const noexcept { return !families_.empty(); }

    void open();
    void close();

    [[nodiscard]] bool is_open() const noexcept { return open_; }

    // A value is being changed.
    [[nodiscard]] bool editing() const noexcept { return editing_; }

    // config.toml changed (by the panel or by hand): shows what it says now.
    void refresh();

    // While open every key is the panel's: true.
    bool key(const ui::KeyEvent& event);

    void pointer(const ui::PointerEvent& event);

    // Text pasted into the value being changed (its first line).
    void paste(std::string_view text);

    // The value under a window point can be clicked (a hand), or is the one being typed in (the
    // text cursor), as last drawn.
    [[nodiscard]] bool clickable_at(float x, float y) const;
    [[nodiscard]] bool typing_at(float x, float y) const;

    // The value chosen (tests).
    [[nodiscard]] std::size_t chosen() const noexcept { return chosen_; }

    // The problem shown under it, when a change was refused.
    [[nodiscard]] const std::string& problem() const noexcept { return problem_; }

    [[nodiscard]] const ui::TextField& field() const noexcept { return field_; }

    // The families offered under the font being changed, and the one picked with the arrows.
    [[nodiscard]] const std::vector<ui::FontFamily>& suggestions() const noexcept {
        return suggestions_;
    }

    [[nodiscard]] std::optional<std::size_t> picked() const noexcept { return picked_; }

    void draw(ui::Renderer& renderer, const ui::Rect& bounds, const theme::Palette& palette);

    // The values, in the order they are shown.
    enum class Value : std::uint8_t { font, monospace, double_click_ms, double_click_distance };
    static constexpr std::size_t kValues = 4;

private:
    void choose(std::size_t value);
    void start_editing();
    void stop_editing();
    void confirm();
    [[nodiscard]] std::string text_of(Value value) const;
    [[nodiscard]] bool font_chosen() const noexcept;
    void suggest();
    void pick(std::optional<std::size_t> index);
    void draw_about(ui::Renderer& renderer, const theme::Palette& palette, float left, float top,
                    float inner);

    std::function<void()> redraw_;
    Settings settings_;
    core::Config config_;
    bool open_{false};
    bool editing_{false};
    std::size_t chosen_{0};
    std::string problem_;
    ui::TextField field_;
    std::array<ui::Rect, kValues> boxes_{};  // the values' boxes, as last drawn
    ui::Rect card_{};
    std::vector<ui::FontFamily> families_;
    std::vector<ui::FontFamily> suggestions_;
    std::optional<std::size_t> picked_;
    std::size_t first_{0};        // the first suggestion shown (the list scrolls)
    bool typed_{false};           // typed since the change began: the list follows the text
    std::vector<ui::Rect> rows_;  // the suggestions shown, as last drawn (from first_)
    ui::Rect list_{};
    std::function<void(std::string)> copy_;
    Page page_{Page::settings};
    std::vector<std::pair<std::string, std::string>> about_;
    bool copied_{false};
    std::array<ui::Rect, 2> tabs_{};  // the pages' titles, as last drawn
    ui::Rect copy_button_{};
};

}  // namespace nordend::views
