#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "nordend/core/event_loop.hpp"
#include "nordend/core/worker_pool.hpp"
#include "nordend/fs/directory_model.hpp"
#include "nordend/fs/directory_watcher.hpp"
#include "nordend/fs/mime.hpp"

namespace nordend::views {

// How moving the cursor changes the selection.
enum class SelectMode : std::uint8_t {
    replace,  // select only the new entry (plain click, arrows)
    extend,   // select from the anchor to the new entry (Shift)
    keep,     // leave the selection as it is (Ctrl+arrows)
    toggle,   // add or remove the new entry, which becomes the anchor (Ctrl+click)
};

// What a selection holds (the info pane's summary).
struct SelectionSummary {
    std::size_t folders{0};
    std::size_t files{0};    // everything that is not a folder
    std::uint64_t bytes{0};  // sizes of the files; folders are not counted
};

// One directory as the views see it: its listing, a cursor and a selection, and the types of its
// entries sniffed so far. It does not draw: a column or the grid shows it, and switching between
// them hands the same Folder over, so cursor and selection stay.
//
// The cursor is the entry the keyboard is on; the selection is a set of entries, usually just the
// cursor's. Both are kept by name, so they follow their entries when rows move or are renamed,
// and lose them when they are deleted. The cursor can name an entry that has not been loaded yet
// (pending): it lands when the entry arrives, or is dropped when the load ends without it.
//
// Where the cursor sits on screen is the view's business: a Folder only says, through
// take_reveal(), that the cursor moved and how the view should bring it into sight.
//
// Loop thread only. Must not outlive the loop, pool or watcher.
class Folder {
public:
    // How the view should bring the cursor into sight at its next draw.
    enum class Reveal : std::uint8_t {
        none,
        glide,  // moved by the user: just enough to show it, animated
        jump,   // landed or asked for by name: centered, at once
    };

    // redraw runs when anything shown changes.
    Folder(core::EventLoop& loop, core::WorkerPool& pool, fs::DirectoryWatcher& watcher,
           std::shared_ptr<const fs::MimeDatabase> mime, std::filesystem::path directory,
           std::function<void()> redraw);

    ~Folder();

    Folder(const Folder&) = delete;
    Folder& operator=(const Folder&) = delete;
    Folder(Folder&&) = delete;
    Folder& operator=(Folder&&) = delete;

    // changed runs when the cursor or the selection changes, lands, loses its entry, or that
    // entry changes. One handler: the view that owns the Folder's place sets it.
    void set_changed_handler(std::function<void()> changed) { changed_ = std::move(changed); }

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return model_.path(); }

    [[nodiscard]] const fs::DirectoryModel& model() const noexcept { return model_; }

    [[nodiscard]] const std::vector<fs::Entry>& entries() const noexcept {
        return model_.listing().entries();
    }

    // The entry under the cursor; nullptr when there is none or it is still pending.
    [[nodiscard]] const fs::Entry* cursor_entry() const;

    [[nodiscard]] std::optional<std::size_t> cursor() const noexcept { return cursor_index_; }

    // A cursor was asked for by name and its entry has not arrived yet.
    [[nodiscard]] bool cursor_pending() const noexcept {
        return !cursor_name_.empty() && !cursor_index_;
    }

    // Puts the cursor on an entry (clamped) and updates the selection as mode says. nullopt
    // removes the cursor and clears the selection.
    void set_cursor(std::optional<std::size_t> index, SelectMode mode = SelectMode::replace);

    // Moves the cursor by delta entries (to the first entry when there is none).
    void move_cursor(std::ptrdiff_t delta, SelectMode mode = SelectMode::replace);

    // Puts the cursor on the entry called name and selects it, now or when it arrives.
    void select_name(std::string name);

    void select_all();

    // Back to the cursor's entry alone (Escape).
    void collapse_selection();

    [[nodiscard]] std::size_t selected_count() const noexcept { return selected_.size(); }

    [[nodiscard]] bool is_selected(const std::string& name) const {
        return selected_.contains(name);
    }

    // The selected names, in listing order.
    [[nodiscard]] std::vector<std::string> selected_names() const;

    // The entry under the cursor when it is the whole selection; nullptr otherwise.
    [[nodiscard]] const fs::Entry* single_selection() const;

    [[nodiscard]] const SelectionSummary& summary() const;

    // How the view should show the cursor, once: the request is cleared.
    [[nodiscard]] Reveal take_reveal() noexcept;

    // Asks the view to jump to the cursor at its next draw (a view that starts showing it).
    void request_jump() noexcept { reveal_ = Reveal::jump; }

    // The entry's type: from the name, else sniffed from the content, else unknown (yet).
    [[nodiscard]] fs::MimeId mime_of(const fs::Entry& entry) const;

    [[nodiscard]] const fs::MimeDatabase& mime() const noexcept { return *mime_; }

    // Sniffs the types of entries [first, last) that the name does not decide, on a worker (the
    // view asks for what it shows).
    void sniff(std::size_t first, std::size_t last);

private:
    void on_change(const fs::ModelChange& change);
    void selection_changed();

    core::WorkerPool& pool_;
    std::shared_ptr<const fs::MimeDatabase> mime_;
    std::function<void()> redraw_;
    std::function<void()> changed_;
    fs::DirectoryModel model_;

    std::string cursor_name_;                  // empty: no cursor
    std::optional<std::size_t> cursor_index_;  // nullopt while pending
    Reveal reveal_{Reveal::none};
    std::unordered_set<std::string> selected_;
    std::string anchor_;                               // where a Shift range starts
    mutable std::optional<SelectionSummary> summary_;  // computed on demand

    // Types sniffed from content, by name.
    std::unordered_map<std::string, fs::MimeId> sniffed_;
    std::unordered_set<std::string> sniffing_;
    // False once destroyed: sniff results still on their way find out here.
    std::shared_ptr<bool> alive_{std::make_shared<bool>(true)};
};

}  // namespace nordend::views
