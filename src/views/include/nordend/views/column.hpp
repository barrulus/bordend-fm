#pragma once

#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>

#include "nordend/theme/palette.hpp"
#include "nordend/ui/geometry.hpp"
#include "nordend/ui/icons.hpp"
#include "nordend/ui/renderer.hpp"
#include "nordend/ui/scroller.hpp"
#include "nordend/views/cut_marks.hpp"
#include "nordend/views/edit_slot.hpp"
#include "nordend/views/folder.hpp"

namespace nordend::views {

// One folder in the column view, drawn as a virtualized, scrolling list of icon and name. Each
// frame draws only the rows inside the viewport and asks the folder to sniff their types. The
// folder (cursor, selection, listing) is shared: the grid can show the same one.
//
// Loop thread only.
class Column {
public:
    using Clock = ui::Scroller::Clock;

    static constexpr float kRowHeight = 28.0F;

    explicit Column(std::shared_ptr<Folder> folder);

    [[nodiscard]] Folder& folder() noexcept { return *folder_; }

    [[nodiscard]] const Folder& folder() const noexcept { return *folder_; }

    [[nodiscard]] const std::shared_ptr<Folder>& shared_folder() const noexcept { return folder_; }

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return folder_->path(); }

    // Rows a page key moves: what fits in the last drawn viewport, less one.
    [[nodiscard]] std::size_t page_rows() const;

    // Lays out and draws into bounds at time now (advancing the scroll animation). The cursor is
    // strong in the focused column, quiet in the others (the path that leads to it). A name
    // being edited (edit) is left out and its place noted there; cut entries are faded.
    void draw(ui::Renderer& renderer, const ui::Rect& bounds, const theme::Palette& palette,
              ui::IconSet& icons, bool focused, Clock::time_point now, EditSlot* edit = nullptr,
              const CutMarks* cut = nullptr);

    // A scroll animation is running: the window should keep drawing frames.
    [[nodiscard]] bool animating() const noexcept { return scroller_.animating(); }

    [[nodiscard]] ui::Scroller& scroller() noexcept { return scroller_; }

    // Row at a window point, from the last drawn bounds; nullopt outside the rows.
    [[nodiscard]] std::optional<std::size_t> row_at(float x, float y) const;

    // Hover highlight; returns true if it changed.
    bool set_hovered(std::optional<std::size_t> row);

private:
    void reveal_cursor(Folder::Reveal reveal, Clock::time_point now);

    std::shared_ptr<Folder> folder_;
    ui::Scroller scroller_;
    ui::Rect rows_area_{};  // where the rows were last drawn, for hit testing
    std::optional<std::size_t> hovered_;
};

}  // namespace nordend::views
