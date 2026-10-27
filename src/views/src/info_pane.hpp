#pragma once

// The info pane: what the selection of a folder is, drawn by every view (the columns after their
// last column, the grid and the details at their right).

#include "nordend/theme/palette.hpp"
#include "nordend/ui/geometry.hpp"
#include "nordend/ui/icons.hpp"
#include "nordend/ui/renderer.hpp"
#include "nordend/views/folder.hpp"

namespace nordend::views {

inline constexpr float kInfoPaneWidth = 300.0F;

// The pane is left out when the view would keep less than this for its content.
inline constexpr float kInfoPaneMinContent = 320.0F;

// Whether a view of this width has room for the pane.
[[nodiscard]] inline bool info_pane_fits(float width) {
    return width >= kInfoPaneWidth + kInfoPaneMinContent;
}

// The entry under the cursor when it is the whole selection; a summary when several are
// selected; the folder itself when nothing is. The content keeps to the pane's left
// kInfoPaneWidth pixels when bounds is wider.
void draw_info_pane(ui::Renderer& renderer, const ui::Rect& bounds, const theme::Palette& palette,
                    ui::IconSet& icons, const Folder& folder);

}  // namespace nordend::views
