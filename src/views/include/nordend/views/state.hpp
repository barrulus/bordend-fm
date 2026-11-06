#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "nordend/core/event_loop.hpp"
#include "nordend/core/worker_pool.hpp"
#include "nordend/fs/sort.hpp"

// What Nordend remembers from one start to the next (ROADMAP 7.5a): how the window was left, not
// preferences (those are config.toml's). Kept in $XDG_STATE_HOME/nordend/state.toml, written by
// Nordend; a person need not edit it, and a broken one only means the defaults.
namespace nordend::views {

struct State {
    enum class Mode : std::uint8_t { columns, grid, details };

    Mode mode{Mode::columns};
    bool show_hidden{false};
    bool sidebar_visible{true};
    float sidebar_width{200.0F};
    bool info_visible{true};
    float info_width{300.0F};
    bool info_expanded{false};
    // The details' order and the widths of size, modified and type (the name takes the rest).
    fs::SortKey sort_key{fs::SortKey::name};
    bool sort_descending{false};
    std::array<float, 3> details_widths{96.0F, 150.0F, 220.0F};
    // The lists' row size (an index into kListSizes, 1 the usual) and the grid's icon size in
    // pixels (one of GridView::kIconSizes).
    std::size_t list_size{1};
    std::int32_t grid_icon{96};
    // The window's size when it was last neither maximized, fullscreen nor tiled; logical pixels.
    std::int32_t window_width{960};
    std::int32_t window_height{640};
    bool maximized{false};

    // The tabs when there were two or more (ROADMAP 9.1d), restored when Nordend starts without
    // a folder; empty with one tab. Each has its folder and view; in the columns, the first
    // column's folder too, so they unfold again from there.
    struct Tab {
        std::filesystem::path folder;
        Mode mode{Mode::columns};
        std::filesystem::path root;  // the columns' first folder; empty in the other views
        std::string cursor;          // the name under the cursor in folder; empty: none

        bool operator==(const Tab&) const = default;
    };

    static constexpr std::size_t kMaxTabs = 64;

    std::vector<Tab> tabs;
    std::size_t shown_tab{0};  // which of tabs was shown

    bool operator==(const State&) const = default;
};

struct StateResult {
    State state;
    // One line per problem, ready to log: "<file>:<line>:<column>: <what>".
    std::vector<std::string> problems;
};

// state.toml under $XDG_STATE_HOME (or ~/.local/state); empty without a home.
[[nodiscard]] std::filesystem::path default_state_path();

// Reads state.toml text. A value of the wrong type or out of range keeps its default and is a
// problem; unknown keys are ignored (a newer Nordend may have written them). A file that does not
// parse gives the defaults.
[[nodiscard]] StateResult parse_state(std::string_view text, std::string_view source);

// A missing file is not a problem: the defaults.
[[nodiscard]] StateResult load_state(const std::filesystem::path& path);

[[nodiscard]] std::string format_state(const State& state);

// Keeps state.toml current: a change is written a moment after the last one (a dragged edge
// writes once), on a worker, one write at a time so an older state never lands after a newer
// one. flush() writes what is left, on the calling thread (at exit); a worker's write still
// running then cannot land after it.
//
// Loop thread only. Must not outlive the loop or the pool.
class StateKeeper {
public:
    static constexpr std::chrono::milliseconds kDelay{1000};

    // saved: what the file holds now (what was loaded at start).
    StateKeeper(core::EventLoop& loop, core::WorkerPool& pool, std::filesystem::path path,
                State saved, std::chrono::milliseconds delay = kDelay);
    ~StateKeeper();

    StateKeeper(const StateKeeper&) = delete;
    StateKeeper& operator=(const StateKeeper&) = delete;
    StateKeeper(StateKeeper&&) = delete;
    StateKeeper& operator=(StateKeeper&&) = delete;

    // The state now; cheap when nothing changed (main calls it after every frame).
    void note(const State& state);

    void flush();

    // What was written last (tests).
    [[nodiscard]] const State& saved() const noexcept { return saved_; }

private:
    struct File;

    void write();

    core::EventLoop& loop_;
    core::WorkerPool& pool_;
    std::filesystem::path path_;
    std::chrono::milliseconds delay_;
    State saved_;    // in the file, or being written there
    State current_;  // the latest noted
    core::EventLoop::SourceId timer_{0};
    bool writing_{false};
    std::uint64_t generation_{0};  // of current_
    std::shared_ptr<File> file_;   // shared with the worker's write
};

}  // namespace nordend::views
