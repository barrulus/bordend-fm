#include "nordend/views/shortcuts.hpp"

#include <algorithm>
#include <xkbcommon/xkbcommon-keysyms.h>

namespace nordend::views {
namespace {

constexpr ui::Modifiers kCtrl{.ctrl = true};
constexpr ui::Modifiers kShift{.shift = true};
constexpr ui::Modifiers kAlt{.alt = true};

// The keys that move the cursor; Left and Right move it only in the grid (in the columns and the
// details they go up and in), but Shift and Ctrl work with all of them.
constexpr std::array<xkb_keysym_t, 8> kMoves{XKB_KEY_Up,    XKB_KEY_Down,    XKB_KEY_Left,
                                             XKB_KEY_Right, XKB_KEY_Page_Up, XKB_KEY_Page_Down,
                                             XKB_KEY_Home,  XKB_KEY_End};

constexpr std::array kShortcuts{
    // General
    Shortcut{.group = "General",
             .keys = "Ctrl+1",
             .action = "Columns",
             .scope = scope::kViews | scope::kSidebar | scope::kQuickLook,
             .modifiers = kCtrl,
             .keysyms = {XKB_KEY_1, XKB_KEY_KP_1},
             .command = Command::columns_view},
    Shortcut{.group = "General",
             .keys = "Ctrl+2",
             .action = "Grid",
             .scope = scope::kViews | scope::kSidebar | scope::kQuickLook,
             .modifiers = kCtrl,
             .keysyms = {XKB_KEY_2, XKB_KEY_KP_2},
             .command = Command::grid_view},
    Shortcut{.group = "General",
             .keys = "Ctrl+3",
             .action = "Details",
             .scope = scope::kViews | scope::kSidebar | scope::kQuickLook,
             .modifiers = kCtrl,
             .keysyms = {XKB_KEY_3, XKB_KEY_KP_3},
             .command = Command::details_view},
    Shortcut{.group = "General",
             .keys = "Space",
             .action = "Quick Look",
             .scope = scope::kViews | scope::kQuickLook,
             .keysyms = {XKB_KEY_space},
             .command = Command::quick_look},
    Shortcut{.group = "General",
             .keys = "Ctrl+I",
             .action = "Info pane (grid, details)",
             .scope = scope::kViews | scope::kSidebar | scope::kQuickLook,
             .modifiers = kCtrl,
             .keysyms = {XKB_KEY_i, XKB_KEY_I},
             .command = Command::toggle_info},
    Shortcut{.group = "General",
             .keys = "Ctrl+B",
             .action = "Sidebar",
             .scope = scope::kViews | scope::kSidebar | scope::kQuickLook,
             .modifiers = kCtrl,
             .keysyms = {XKB_KEY_b, XKB_KEY_B},
             .command = Command::toggle_sidebar},
    Shortcut{.group = "General",
             .keys = "F6",
             .action = "Into the sidebar and out",
             .scope = scope::kViews | scope::kSidebar,
             .keysyms = {XKB_KEY_F6},
             .command = Command::focus_sidebar},
    Shortcut{.group = "General",
             .keys = "F1  ?",
             .action = "These shortcuts",
             .scope = scope::kViews | scope::kSidebar | scope::kQuickLook,
             .any_shift = true,
             .keysyms = {XKB_KEY_F1, XKB_KEY_question},
             .command = Command::show_shortcuts},
    Shortcut{.group = "General",
             .keys = "Ctrl+Q",
             .action = "Quit",
             .scope = scope::kWindow,
             .modifiers = kCtrl,
             .keysyms = {XKB_KEY_q, XKB_KEY_Q}},

    // Moving
    Shortcut{.group = "Moving",
             .keys = "↑  ↓",
             .action = "Previous, next",
             .scope = scope::kViews,
             .keysyms = {XKB_KEY_Up, XKB_KEY_Down}},
    Shortcut{.group = "Moving",
             .keys = "←  →",
             .action = "Previous, next (grid)",
             .scope = scope::kGrid,
             .keysyms = {XKB_KEY_Left, XKB_KEY_Right}},
    Shortcut{.group = "Moving",
             .keys = "PgUp  PgDn",
             .action = "A screenful",
             .scope = scope::kViews,
             .keysyms = {XKB_KEY_Page_Up, XKB_KEY_Page_Down}},
    Shortcut{.group = "Moving",
             .keys = "Home  End",
             .action = "First, last",
             .scope = scope::kViews,
             .keysyms = {XKB_KEY_Home, XKB_KEY_End}},
    Shortcut{.group = "Moving",
             .keys = "Enter",
             .action = "Open (files in their app)",
             .scope = scope::kViews,
             .keysyms = {XKB_KEY_Return, XKB_KEY_KP_Enter}},
    Shortcut{.group = "Moving",
             .keys = "→",
             .action = "Open (columns, details)",
             .scope = scope::kColumns | scope::kDetails,
             .keysyms = {XKB_KEY_Right}},
    Shortcut{.group = "Moving",
             .keys = "Alt+↓",
             .action = "Open (grid, details)",
             .scope = scope::kGrid | scope::kDetails,
             .modifiers = kAlt,
             .keysyms = {XKB_KEY_Down}},
    Shortcut{.group = "Moving",
             .keys = "Backspace",
             .action = "Parent folder",
             .scope = scope::kViews,
             .keysyms = {XKB_KEY_BackSpace}},
    Shortcut{.group = "Moving",
             .keys = "←",
             .action = "Parent (columns, details)",
             .scope = scope::kColumns | scope::kDetails,
             .keysyms = {XKB_KEY_Left}},
    Shortcut{.group = "Moving",
             .keys = "Alt+↑",
             .action = "Parent folder",
             .scope = scope::kViews,
             .modifiers = kAlt,
             .keysyms = {XKB_KEY_Up}},

    // Selecting
    Shortcut{.group = "Selecting",
             .keys = "Shift+moves",
             .action = "Select a range",
             .scope = scope::kViews,
             .modifiers = kShift,
             .keysyms = kMoves},
    Shortcut{.group = "Selecting",
             .keys = "Ctrl+moves",
             .action = "Move without selecting",
             .scope = scope::kViews,
             .modifiers = kCtrl,
             .keysyms = kMoves},
    Shortcut{.group = "Selecting",
             .keys = "Ctrl+Space",
             .action = "Select or unselect",
             .scope = scope::kViews,
             .modifiers = kCtrl,
             .keysyms = {XKB_KEY_space}},
    Shortcut{.group = "Selecting",
             .keys = "Ctrl+A",
             .action = "Select all",
             .scope = scope::kViews,
             .modifiers = kCtrl,
             .keysyms = {XKB_KEY_a, XKB_KEY_A}},
    Shortcut{.group = "Selecting",
             .keys = "Escape",
             .action = "Back to one entry",
             .scope = scope::kViews,
             .keysyms = {XKB_KEY_Escape}},

    // Files
    Shortcut{.group = "Files",
             .keys = "F2",
             .action = "Rename",
             .scope = scope::kViews | scope::kQuickLook,
             .keysyms = {XKB_KEY_F2},
             .command = Command::rename},
    Shortcut{.group = "Files",
             .keys = "Ctrl+Shift+N",
             .action = "New folder",
             .scope = scope::kViews | scope::kQuickLook,
             .modifiers = {.ctrl = true, .shift = true},
             .keysyms = {XKB_KEY_n, XKB_KEY_N},
             .command = Command::new_folder},
    Shortcut{.group = "Files",
             .keys = "Enter  Escape",
             .action = "Renaming: keep, give up",
             .scope = scope::kEditor,
             .keysyms = {XKB_KEY_Return, XKB_KEY_KP_Enter, XKB_KEY_Escape}},
    Shortcut{.group = "Files",
             .keys = "Ctrl+←  Ctrl+→",
             .action = "Renaming: by words",
             .scope = scope::kEditor,
             .modifiers = kCtrl,
             .keysyms = {XKB_KEY_Left, XKB_KEY_Right}},

    // Grid
    Shortcut{.group = "Grid",
             .keys = "Ctrl++  Ctrl+−",
             .action = "Larger, smaller",
             .scope = scope::kGrid,
             .modifiers = kCtrl,
             .any_shift = true,
             .keysyms = {XKB_KEY_plus, XKB_KEY_equal, XKB_KEY_KP_Add, XKB_KEY_minus,
                         XKB_KEY_KP_Subtract}},
    Shortcut{.group = "Grid",
             .keys = "Ctrl+0",
             .action = "Usual size",
             .scope = scope::kGrid,
             .modifiers = kCtrl,
             .keysyms = {XKB_KEY_0, XKB_KEY_KP_0}},

    // Sidebar
    Shortcut{.group = "Sidebar",
             .keys = "↑  ↓  Home  End",
             .action = "Choose a place",
             .scope = scope::kSidebar,
             .keysyms = {XKB_KEY_Up, XKB_KEY_Down, XKB_KEY_Home, XKB_KEY_End}},
    Shortcut{.group = "Sidebar",
             .keys = "Enter",
             .action = "Open it",
             .scope = scope::kSidebar,
             .keysyms = {XKB_KEY_Return, XKB_KEY_KP_Enter}},
    Shortcut{.group = "Sidebar",
             .keys = "Escape",
             .action = "Back to the view",
             .scope = scope::kSidebar,
             .keysyms = {XKB_KEY_Escape}},

    // Quick Look
    Shortcut{.group = "Quick Look",
             .keys = "Space  Escape",
             .action = "Close",
             .scope = scope::kQuickLook,
             .keysyms = {XKB_KEY_space, XKB_KEY_Escape}},
    Shortcut{.group = "Quick Look",
             .keys = "Arrows",
             .action = "The next file, the previous",
             .scope = scope::kQuickLook,
             .keysyms = {XKB_KEY_Up, XKB_KEY_Down, XKB_KEY_Left, XKB_KEY_Right}},
};

}  // namespace

bool Shortcut::matches(const ui::KeyEvent& event) const {
    ui::Modifiers pressed = event.modifiers;
    if (any_shift) {
        pressed.shift = false;
    }
    return pressed == modifiers && event.keysym != XKB_KEY_NoSymbol &&
           std::ranges::find(keysyms, event.keysym) != keysyms.end();
}

std::span<const Shortcut> shortcuts() { return kShortcuts; }

Command command_for(const ui::KeyEvent& event) {
    for (const Shortcut& shortcut : kShortcuts) {
        if (shortcut.command != Command::none && shortcut.matches(event)) {
            return shortcut.command;
        }
    }
    return Command::none;
}

bool listed(const ui::KeyEvent& event, std::uint8_t where) {
    return std::ranges::any_of(kShortcuts, [&](const Shortcut& shortcut) {
        return (shortcut.scope & where) != 0U && shortcut.matches(event);
    });
}

}  // namespace nordend::views
