#pragma once

#include <array>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include "nordend/core/config.hpp"
#include "nordend/theme/palette.hpp"
#include "nordend/ui/geometry.hpp"
#include "nordend/ui/renderer.hpp"
#include "nordend/ui/seat.hpp"
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
};

// The settings (Ctrl+, or the status bar's gear, ROADMAP 7.5c), on a card over the window: the
// fonts and the double click, changed in place and written to config.toml; where the colours come
// from. Up, Down and Tab choose a value, Enter or a click changes it, Enter keeps the change and
// Escape drops it; Escape, Ctrl+, or a click outside the card close it.
//
// Loop thread only.
class SettingsPanel {
public:
    explicit SettingsPanel(std::function<void()> redraw);

    void set_settings(Settings settings) { settings_ = std::move(settings); }

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
};

}  // namespace nordend::views
