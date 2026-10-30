#pragma once

#include <chrono>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "nordend/core/event_loop.hpp"
#include "nordend/core/worker_pool.hpp"
#include "nordend/fs/directory_model.hpp"
#include "nordend/fs/directory_watcher.hpp"
#include "nordend/fs/mime.hpp"
#include "nordend/fs/places.hpp"
#include "nordend/fs/transfer_queue.hpp"
#include "nordend/theme/palette.hpp"
#include "nordend/ui/geometry.hpp"
#include "nordend/ui/icons.hpp"
#include "nordend/ui/renderer.hpp"
#include "nordend/ui/scroller.hpp"
#include "nordend/ui/seat.hpp"
#include "nordend/views/breadcrumb.hpp"
#include "nordend/views/column_view.hpp"
#include "nordend/views/confirm_dialog.hpp"
#include "nordend/views/conflict_dialog.hpp"
#include "nordend/views/context_menu.hpp"
#include "nordend/views/cut_marks.hpp"
#include "nordend/views/details_view.hpp"
#include "nordend/views/file_clipboard.hpp"
#include "nordend/views/grid_view.hpp"
#include "nordend/views/info_pane.hpp"
#include "nordend/views/name_editor.hpp"
#include "nordend/views/quick_look.hpp"
#include "nordend/views/shortcuts.hpp"
#include "nordend/views/shortcuts_overlay.hpp"
#include "nordend/views/sidebar.hpp"
#include "nordend/views/status_bar.hpp"
#include "nordend/views/thumbnails.hpp"
#include "nordend/views/undo.hpp"

namespace nordend::views {

// The window's content: the column view, the grid or the details table, one at a time, and Quick
// Look over them (Space opens it on the entry under the cursor; Space, Escape or a click close it;
// the other keys move the view underneath). Ctrl+1 shows the columns, Ctrl+2 the grid, Ctrl+3
// the details. The info pane shows what is selected in all three: after the last column, at the
// right of the grid and the details, where Ctrl+I shows or hides it. The grid and the details show
// the folder of the focused column, and the columns come back with that folder focused: the same
// folder object passes between them, so the cursor, the selection and the types sniffed so far
// stay. Above the view, the breadcrumb shows the folder's path, every step a way up. The sidebar at
// the left lists the places (set_places); Ctrl+B shows or hides it, F6 moves the keyboard into it
// and back, and a place chosen there opens in the view shown. Enter or a double click on a file
// hands the selected files to the open handler (their default applications); on a folder, the
// view goes into it. F2 renames the entry under the cursor in place, Ctrl+Shift+N makes a new
// folder and edits its name; both on a worker, never replacing anything. Ctrl+C and Ctrl+X take
// the selection (cut entries fade until pasted; Escape keeps them), Ctrl+V copies or moves it
// into the folder shown, on the transfer thread, with progress in the status bar and a question
// on a card for every name taken. Delete moves the selection to the trash (freedesktop), on the
// same thread; the cursor goes on to the entry after it. Shift+Delete (Delete in the trash folder)
// deletes it for good and "Empty Trash" empties every trash, after a card that says how much goes.
// A right click (or the Menu key, Shift+F10)
// opens a context menu with the same actions, "Open With" and "New File". Alt+Left and Alt+Right
// (the mouse's back and forward buttons) go through the folders opened, Ctrl+H shows the hidden
// files.
//
// Loop thread only. Must not outlive the loop, pool or watcher.
class Browser {
public:
    using Clock = ui::Scroller::Clock;

    enum class Mode : std::uint8_t { columns, grid, details };

    // thumbnails: the cache the grid takes its thumbnails from; nullptr shows icons only.
    Browser(core::EventLoop& loop, core::WorkerPool& pool, fs::DirectoryWatcher& watcher,
            std::shared_ptr<const fs::MimeDatabase> mime, std::function<void()> redraw,
            std::function<void(ui::CursorShape)> set_cursor,
            std::shared_ptr<const thumbs::Cache> thumbnails = nullptr);
    ~Browser();

    Browser(const Browser&) = delete;
    Browser& operator=(const Browser&) = delete;
    Browser(Browser&&) = delete;
    Browser& operator=(Browser&&) = delete;

    // directory, no cursor, in the current mode.
    void open(const std::filesystem::path& directory);

    // folder in the current mode with the cursor on child (a name; empty: none), as when going
    // up: the columns focus the column showing folder when there is one.
    void go_to(const std::filesystem::path& folder, const std::string& child);

    // The columns from root down to target (see ColumnView::reveal); switches to the columns.
    void reveal(const std::filesystem::path& root, const std::filesystem::path& target);

    void set_mode(Mode mode);

    // A folder opened, as the history keeps it: where the cursor was when it was left.
    struct Step {
        std::filesystem::path folder;
        std::string cursor;
    };

    static constexpr std::size_t kHistorySize = 100;

    // Alt+Left, Alt+Right and the mouse's side buttons: the folder opened before this one, or
    // after it, with the cursor where it was. Every folder the keyboard goes into is a step
    // (in the columns, the focused column's); opening one after going back drops the steps
    // ahead. A folder that no longer exists is left out of the history, with a message.
    void back();
    void forward();

    [[nodiscard]] const std::vector<Step>& history() const noexcept { return history_; }

    // Where history() is now.
    [[nodiscard]] std::size_t history_index() const noexcept { return here_; }

    // Ctrl+H: dotfiles listed in every view, or left out (at start). Left out, they leave the
    // selection, so nothing acts on what is not seen, and the cursor moves off them; the hidden
    // folders on the way to the one shown stay.
    void set_show_hidden(bool show);

    [[nodiscard]] bool shows_hidden() const noexcept { return show_hidden_; }

    [[nodiscard]] Mode mode() const noexcept { return mode_; }

    // The info pane in the grid and the details, one setting for both (shown at start); the
    // columns always show it.
    void set_info_visible(bool visible);

    [[nodiscard]] bool info_visible() const noexcept { return info_visible_; }

    // The sidebar (shown at start; left out when the window is too narrow for it).
    void set_sidebar_visible(bool visible);

    [[nodiscard]] bool sidebar_visible() const noexcept { return sidebar_visible_; }

    // What the sidebar lists.
    void set_places(std::vector<fs::Place> places);

    // Opens files (directory's entries, never folders, types filled in where known). Without
    // one, Enter on a file does nothing.
    using OpenHandler =
        std::function<void(const std::filesystem::path& directory, std::vector<fs::Entry> files)>;

    void set_open_handler(OpenHandler open) { open_ = std::move(open); }

    // An application "Open With" offers.
    struct AppChoice {
        std::string id;
        std::string name;
        bool is_default{false};
    };

    // The applications for entry, handed to done later on the loop thread; and opening files
    // with one of them. Without them the menu has no "Open With".
    using ChoicesHandler =
        std::function<void(const std::filesystem::path& directory, fs::Entry entry,
                           std::function<void(std::vector<AppChoice>)> done)>;
    using OpenWithHandler = std::function<void(const std::filesystem::path& directory,
                                               std::vector<fs::Entry> files, std::string app_id)>;

    void set_open_with_handlers(ChoicesHandler choices, OpenWithHandler open_with) {
        choices_ = std::move(choices);
        open_with_ = std::move(open_with);
    }

    // Where "New File" takes its templates from: fs::templates_dir() unless set (tests).
    void set_templates_dir(std::filesystem::path directory) {
        templates_dir_ = std::move(directory);
    }

    // A line in the status bar for a few seconds (what opening a file did).
    void show_message(std::string text, bool error) {
        status_bar_.show_message(std::move(text), error);
    }

    void draw(ui::Renderer& renderer, const ui::Rect& bounds, const theme::Palette& palette,
              ui::IconSet& icons, Clock::time_point now);

    [[nodiscard]] bool animating() const;

    void pointer(const ui::PointerEvent& event, Clock::time_point now);
    // false when event did nothing: no shortcut (shortcuts()) where the keyboard is.
    bool key(const ui::KeyEvent& event, Clock::time_point now);

    // The view shown is still loading, its thumbnails are being made, or a new folder or a rename
    // is on its way (snapshots, tests).
    [[nodiscard]] bool loading() const;

    [[nodiscard]] ColumnView& columns() noexcept { return columns_; }

    [[nodiscard]] GridView& grid() noexcept { return grid_; }

    [[nodiscard]] DetailsView& details() noexcept { return details_; }

    [[nodiscard]] QuickLook& quick_look() noexcept { return quick_look_; }

    [[nodiscard]] InfoPane& info_pane() noexcept { return info_pane_; }

    [[nodiscard]] Sidebar& sidebar() noexcept { return sidebar_; }

    [[nodiscard]] Breadcrumb& breadcrumb() noexcept { return breadcrumb_; }

    [[nodiscard]] StatusBar& status_bar() noexcept { return status_bar_; }

    [[nodiscard]] ShortcutsOverlay& shortcuts_overlay() noexcept { return shortcuts_; }

    [[nodiscard]] NameEditor& name_editor() noexcept { return editor_; }

    // F2: edits the name of the entry under the cursor (nothing without one).
    void start_rename();

    // Ctrl+Shift+N: makes "New Folder" (or "New Folder 2"...) in the folder shown, puts the cursor
    // on it and edits its name.
    void new_folder();

    // Ctrl+C, Ctrl+X: the selection (or the entry under the cursor) is what Ctrl+V will copy or
    // move.
    void take_selection(bool cut);

    // Ctrl+V: copies or moves what was taken into the folder shown; when another program holds
    // the clipboard, the files it offers (x-special/gnome-copied-files, else text/uri-list).
    void paste();

    // The system clipboard: Ctrl+C and Ctrl+X offer the files there too, Ctrl+V reads it, and
    // Ctrl+V in a name's field pastes its text.
    void set_system_clipboard(SystemClipboard clipboard) { system_ = std::move(clipboard); }

    // Another program took the clipboard: what Ctrl+C or Ctrl+X took is forgotten.
    void clipboard_lost();

    // Delete: moves the selection (or the entry under the cursor) to the trash, and the cursor to
    // the entry after it.
    void trash();

    // Where Delete puts what is on the home trash's disk: fs::home_trash() unless set (tests).
    void set_home_trash(std::filesystem::path trash) { home_trash_ = std::move(trash); }

    // The context menu at (x, y): the selection's when there is one, else the folder's. From the
    // keyboard (Menu, Shift+F10), by the cursor's entry.
    void open_menu(float x, float y, bool keyboard);
    void open_menu_from_keyboard();

    [[nodiscard]] ContextMenu& context_menu() noexcept { return menu_; }

    // "New File": makes name (from a template when from is set) in the folder shown, puts the
    // cursor on it and edits its name.
    void new_file(const std::string& name, const std::filesystem::path& from);

    // Shift+Delete, and Delete in the trash folder: asks on a card (how many items, how many MB),
    // then deletes the selection (or the entry under the cursor) for good with its darktable
    // sidecars, on the transfer thread; the cursor goes on to the entry after it.
    void erase();

    // "Empty Trash": asks on a card, then deletes everything in every trash for good.
    void empty_trash();

    [[nodiscard]] ConfirmDialog& confirm_dialog() noexcept { return confirm_; }

    // In the trash folder: puts the selection (or the entry under the cursor) back where it was,
    // with its darktable sidecars, and the cursor on the entry after it.
    void restore();

    // Ctrl+Z: undoes the last operation on files (a rename, a new folder or file, a copy, a move,
    // a trashing, a restore) by doing its inverse: copies and new entries go to the trash, never
    // deleted. Ctrl+Shift+Z: undoes the undo. Only the last one, while Nordend runs.
    void undo();
    void redo();

    // What Ctrl+Z would undo, and Ctrl+Shift+Z redo.
    [[nodiscard]] const std::optional<FileOperation>& last_operation() const noexcept {
        return undo_;
    }

    [[nodiscard]] const std::optional<FileOperation>& undone_operation() const noexcept {
        return redo_;
    }

    // Stops the transfer running (the status bar's ×).
    void cancel_transfer();

    [[nodiscard]] ConflictDialog& conflict_dialog() noexcept { return conflict_; }

    [[nodiscard]] const CutMarks& cut_marks() const noexcept { return cut_marks_; }

    [[nodiscard]] std::size_t transfers_pending() const noexcept { return transfers_.pending(); }

    // What the views hold, as lines for the memory report (ROADMAP 6.3b).
    [[nodiscard]] std::vector<std::string> memory_lines() const;

private:
    // The folder shown into the history: a new step when it changed, else its cursor.
    void note_location();
    // Goes through the history by direction (-1 back, 1 forward).
    void travel(int direction);
    void trash_done(const fs::TransferResult& result);
    void restore_done(const fs::TransferResult& result);
    // Pastes files read from the system clipboard (at generation) into destination.
    void paste_files(const std::filesystem::path& destination, const ClipboardFiles& files,
                     std::uint64_t generation);
    // Why an operation runs: the user's, or an undo or redo of one.
    enum class Purpose : std::uint8_t { user, undo, redo };

    // A new batch: the transfers of one operation, recorded as one when the last is done.
    std::uint64_t open_batch(Purpose purpose, FileOperation::Kind kind);
    // Every transfer goes through here; batch 0: not recorded (a permanent delete).
    // empty_clipboard: another program's cut, read from the clipboard at that generation:
    // emptied once something arrived, unless it changed meanwhile.
    void submit(fs::TransferRequest request, std::uint64_t batch,
                std::optional<std::uint64_t> empty_clipboard = std::nullopt);
    // An operation done: what Ctrl+Z undoes now (or, for an undo, what Ctrl+Shift+Z redoes).
    void record(FileOperation operation, Purpose purpose, bool failed);
    // Starts the inverse of operation, for an undo or a redo.
    void start_inverse(const FileOperation& operation, Purpose purpose);
    // Moves paths to the trash as one operation of purpose.
    void trash_paths(const std::vector<std::filesystem::path>& paths, Purpose purpose);
    void report_done(const fs::TransferRequest& request, const fs::TransferResult& result);
    // Ctrl+V in the name's field: the clipboard's text.
    void paste_text();
    void erase_done(const fs::TransferRequest& request, const fs::TransferResult& result);
    // Opens the card for request (erase or empty_trash), adds up what it removes on a worker and
    // submits it once confirmed and added up; after runs when it is confirmed.
    void ask_erase(fs::TransferRequest request, std::string title, std::function<void()> after);
    // The delete confirmed and added up goes to the transfer thread.
    void submit_erase();
    // The sidebar's menu on a place: the trash's has "Empty Trash".
    void place_menu(const fs::Place& place, float x, float y);
    // The sidebar's places, the trash's icon full or empty.
    void show_places();
    // Puts the cursor on the entry after the last one of going that stays (else the one before).
    static void cursor_after(Folder& folder, const std::unordered_set<std::string>& going);
    // The folder the keyboard is in: the focused column's, or the grid's.
    [[nodiscard]] std::shared_ptr<Folder> current_folder() const;
    void run(Command command, Clock::time_point now);
    // Opens the selected files when the cursor is on a file (only the cursor's when it is not
    // selected); false when the cursor is on a folder or nowhere.
    bool open_selection();
    void rename(const std::shared_ptr<Folder>& folder, const std::string& from,
                const std::string& to);
    void transfer_progress(const fs::TransferRequest& request,
                           const fs::TransferProgress& progress);
    void transfer_done(const fs::TransferRequest& request, const fs::TransferResult& result);
    void ask(const fs::Conflict& conflict, std::function<void(fs::Resolution)> answer);
    bool take_key(const ui::KeyEvent& event, Clock::time_point now);
    void take_pointer(const ui::PointerEvent& event, Clock::time_point now);
    bool view_key(const ui::KeyEvent& event, Clock::time_point now);
    void dispatch_pointer(const ui::PointerEvent& event, Clock::time_point now);
    void view_pointer(const ui::PointerEvent& event, Clock::time_point now);

    // Who gets the pointer: the view, the sidebar or the breadcrumb above the view.
    enum class Target : std::uint8_t { view, sidebar, breadcrumb, status_bar };

    void send_pointer(Target target, const ui::PointerEvent& event, Clock::time_point now);
    void draw_view(ui::Renderer& renderer, const ui::Rect& bounds, const theme::Palette& palette,
                   ui::IconSet& icons, Clock::time_point now);

    // The names a command acts on: the selection, else the cursor's entry.
    [[nodiscard]] static std::vector<std::string> targets(const Folder& folder);
    [[nodiscard]] std::vector<MenuItem> entry_menu(const std::shared_ptr<Folder>& folder);
    [[nodiscard]] std::vector<MenuItem> folder_menu();

    core::WorkerPool& pool_;
    std::function<void()> redraw_;
    OpenHandler open_;
    ChoicesHandler choices_;
    OpenWithHandler open_with_;
    std::optional<std::filesystem::path> templates_dir_;
    fs::PlaceSources place_sources_{
        fs::PlaceSources::from_environment()};  // read at start: where the templates folder is
                                                // found
    std::unique_ptr<Thumbnails> thumbnails_;    // before the views that draw them
    InfoPane info_pane_;                        // the same: every view draws it
    ColumnView columns_;                        // before grid_: grid folders may report to it
    GridView grid_;
    DetailsView details_;
    QuickLook quick_look_;
    Sidebar sidebar_;
    Breadcrumb breadcrumb_;
    StatusBar status_bar_;
    ShortcutsOverlay shortcuts_;
    NameEditor editor_;
    ConflictDialog conflict_;
    ConfirmDialog confirm_;
    ContextMenu menu_;
    std::uint64_t menu_generation_{0};  // a submenu found late fills only the menu it was for
    std::size_t menu_loads_{0};         // applications and templates being looked for
    std::vector<std::function<void()>> menu_loaders_;  // started once the menu is open
    ui::Rect view_area_{};                             // where the view was last drawn
    Mode mode_{Mode::columns};
    bool info_visible_{true};
    bool sidebar_visible_{true};
    bool show_hidden_{false};
    std::vector<Step> history_;
    std::size_t here_{0};
    bool sidebar_drawn_{false};  // in the last draw: the window had room for it
    Target pointer_target_{Target::view};
    bool button_held_{false};  // since a press: the pointer stays with its target

    // The last pointer position, handed to the view that comes on screen.
    float pointer_x_{0.0F};
    float pointer_y_{0.0F};
    bool pointer_inside_{false};
    // False once destroyed: renames and new folders still on their way find out here.
    std::shared_ptr<bool> alive_{std::make_shared<bool>(true)};
    std::size_t operations_{0};  // new folders and renames on workers

    // What Ctrl+C or Ctrl+X took.
    struct Clipboard {
        std::filesystem::path directory;
        std::vector<std::string> names;
        bool cut{false};
    };

    Clipboard clipboard_;
    SystemClipboard system_;

    // For each transfer submitted and not done, in order (the queue's): its batch and the
    // clipboard generation to empty after it (submit()).
    struct Submitted {
        std::uint64_t batch{0};
        std::optional<std::uint64_t> empty_clipboard;
    };

    std::deque<Submitted> submitted_;

    struct Batch {
        Purpose purpose{Purpose::user};
        FileOperation::Kind kind{FileOperation::Kind::copy};
        std::size_t pending{0};
        std::vector<fs::PlacedItem> items;
        bool failed{false};  // something could not be done, or it was stopped
    };

    std::map<std::uint64_t, Batch> batches_;
    std::uint64_t batch_ids_{0};
    std::optional<FileOperation> undo_;  // what Ctrl+Z undoes
    std::optional<FileOperation> redo_;  // what Ctrl+Shift+Z undoes again
    bool undoing_{false};                // an undo or redo on its way
    std::filesystem::path home_trash_;
    std::vector<fs::Place> places_;
    fs::DirectoryModel trash_model_;  // every trash, for the sidebar's icon
    bool trash_full_{false};
    CutMarks cut_marks_;
    std::optional<fs::Resolution> for_the_rest_;

    // The delete the card asks about.
    struct PendingErase {
        std::uint64_t id{0};
        fs::TransferRequest request;
        bool planned{false};    // added up: request is complete (empty_trash: its trashes)
        bool confirmed{false};  // submitted once planned
    };

    std::optional<PendingErase> erase_;
    std::uint64_t erase_ids_{0};  // the running transfer's "the same for the rest"

    // Where the rate is measured from: a time and the bytes copied by then (after a question the
    // rate starts again).
    struct RateStart {
        std::chrono::steady_clock::time_point time;
        std::uint64_t bytes{0};
    };

    std::optional<RateStart> copy_started_;
    // Last: its thread stops first, while everything its callbacks touch is still there.
    fs::TransferQueue transfers_;
};

}  // namespace nordend::views
