#include "nordend/core/actions.hpp"

#include <chrono>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include "nordend/core/config.hpp"
#include "nordend/platform/action_handoff.hpp"
#include "temp_dir.hpp"

using namespace std::chrono_literals;
using nordend::core::Action;
using nordend::core::action_arguments;
using nordend::core::action_matches;
using nordend::core::action_registry;
using nordend::core::ActionContext;

TEST_CASE("actions: arguments preserve filename bytes and selection boundaries") {
    Action action{.id = "convert",
                  .label = "Convert",
                  .argv = {"tool", "{files}", "{directory}"},
                  .mime = {"image/*"},
                  .selection = Action::Selection::many};
    ActionContext context{.directory = "/photos",
                          .targets = {{"/photos/a b.png", "image/png"},
                                      {"/photos/$(touch bad);'\n.png", "image/png"}}};
    REQUIRE(action_matches(action, context));
    CHECK(action_arguments(action, context) ==
          std::vector<std::string>{"tool", "/photos/a b.png", "/photos/$(touch bad);'\n.png",
                                   "/photos"});
    context.targets.push_back({"/photos/readme", "text/plain"});
    CHECK_FALSE(action_matches(action, context));
    CHECK(action_arguments(action, context).empty());
    context.targets.clear();
    CHECK_FALSE(action_matches(action, context));
    action.selection = Action::Selection::background;
    action.argv = {"tool", "{directory}"};
    action.mime = {"inode/directory"};
    CHECK(action_matches(action, context));
    context.trash = true;
    CHECK_FALSE(action_matches(action, context));
}

TEST_CASE("actions: bundled extraction requires only supported archive types") {
    const auto actions = action_registry();
    REQUIRE(actions.size() == 3);
    ActionContext context{.directory = "/tmp", .targets = {{"/tmp/a.zip", "application/zip"}}};
    CHECK(action_matches(actions[1], context));
    CHECK(actions[1].kind == Action::Kind::extract_here);
    CHECK(actions[2].kind == Action::Kind::extract_to);
    context.targets.push_back({"/tmp/b", "inode/directory"});
    CHECK_FALSE(action_matches(actions[1], context));
    CHECK(action_matches(actions[0], context));
    auto replacement = actions[1];
    replacement.enabled = false;
    CHECK_FALSE(action_matches(action_registry(std::vector{replacement})[1], context));
}

TEST_CASE("actions: config validates the entire action set") {
    const std::string valid = R"(
[[actions]]
id = "upscale"
label = "Upscale Image"
argv = ["upscaler", "{file}"]
mime = ["image/*"]
selection = "one"
execution = "handoff"
[keybindings]
"action:upscale" = "Ctrl+Alt+u"
)";
    const auto result = nordend::core::parse_config(valid, "actions.toml");
    REQUIRE(result.problems.empty());
    REQUIRE(result.config.actions.size() == 1);
    CHECK(result.config.actions.front().placement == Action::Placement::actions);
    CHECK(result.config.keybindings.at("action:upscale") == "Ctrl+Alt+u");
    for (const auto* invalid :
         {"actions = 1", "[[actions]]\nid = 'incomplete'",
          "[[actions]]\nid='bad'\nlabel='Bad'\nargv=['tool','{unknown}']",
          "[[actions]]\nid='bad'\nlabel='Bad'\nargv=['tool','prefix{files}']",
          "[[actions]]\nid='bad'\nlabel='Bad'\nargv=['tool','{file}']\nselection='many'",
          "[[actions]]\nid='bad'\nlabel='Bad'\nargv=['tool','{files}']\nselection='background'",
          "[[actions]]\nid='bad'\nlabel='Bad'\nargv=['tool']\nexecution='job'",
          "[[actions]]\nid='bad'\nlabel='Bad'\nargv=['tool']\nplacement='toolbar'",
          "[[actions]]\nid='bad'\nlabel='Bad'\nargv=['tool']\nplacement=true",
          "[[actions]]\nid='bad'\nlabel='Bad'\nargv=['tool']\nconfirm=true",
          "[[actions]]\nid='copy_location'\nlabel='Bad'\nargv=['tool']",
          "[[actions]]\nid='bad'\nlabel='Bad'\nargv=['tool',7]",
          "[[actions]]\nid='bad'\nlabel='Bad'\nargv=['tool']\nmime=['*/*']"}) {
        CAPTURE(invalid);
        const auto bad = nordend::core::parse_config(invalid, "actions.toml");
        CHECK_FALSE(bad.problems.empty());
        CHECK_FALSE(bad.actions_valid);
        CHECK(bad.config.actions.empty());
    }
    const auto preset = nordend::core::parse_config(
        "[[actions]]\nid='extract_here'\nenabled=false\n"
        "[[actions]]\nid='extract_to'\nargv=['other-archiver','{files}']",
        "actions.toml");
    REQUIRE(preset.problems.empty());
    const auto registry = action_registry(preset.config.actions);
    CHECK_FALSE(registry[1].enabled);
    CHECK(registry[2].label == "Extract To…");
    CHECK(registry[2].argv == std::vector<std::string>{"other-archiver", "{files}"});
    CHECK(registry[2].selection == Action::Selection::many);
    CHECK(registry[2].placement == Action::Placement::menu);
    const auto duplicate = nordend::core::parse_config(
        valid + "\n[[actions]]\nid='upscale'\nlabel='Again'\nargv=['tool']", "actions.toml");
    CHECK_FALSE(duplicate.actions_valid);
    CHECK(duplicate.config.actions.empty());
}

TEST_CASE("actions: config supports menu placement and explicit submenu placement") {
    const auto result = nordend::core::parse_config(R"(
[[actions]]
id = "compress"
label = "Compress…"
argv = ["file-roller", "--add", "{files}"]
selection = "many"
placement = "menu"
[[actions]]
id = "goblin"
label = "Consult the File Goblin"
argv = ["notify-send", "Quack"]
placement = "actions"
[[actions]]
id = "extract_here"
placement = "actions"
)",
                                                    "actions.toml");
    REQUIRE(result.problems.empty());
    REQUIRE(result.config.actions.size() == 3);
    CHECK(result.config.actions[0].placement == Action::Placement::menu);
    CHECK(result.config.actions[1].placement == Action::Placement::actions);
    const auto registry = action_registry(result.config.actions);
    CHECK(registry[0].placement == Action::Placement::menu);
    CHECK(registry[1].placement == Action::Placement::actions);
    CHECK(registry[2].placement == Action::Placement::menu);
}

TEST_CASE("actions: handoff passes arguments and cwd literally and reports failures") {
    nordend::test::TempDir dir;
    nordend::core::EventLoop loop;
    std::vector<std::string> errors;
    nordend::platform::ActionHandoff handoff{loop,
                                             nordend::platform::Environment::from_environment(),
                                             [&](std::string text, bool error) {
                                                 if (error) {
                                                     errors.push_back(std::move(text));
                                                     loop.quit(0);
                                                 }
                                             }};
    Action action{.id = "record",
                  .label = "Record",
                  .argv = {"sh", "-c", "printf '%s\\0' \"$PWD\" \"$@\" > arguments; exit 7",
                           "nordend-test", "{files}"},
                  .mime = {},
                  .selection = Action::Selection::many};
    const std::string tricky = (dir.path / "a b;'$(touch BAD)\n.png").string();
    const ActionContext context{.directory = dir.path, .targets = {{tricky, "image/png"}}};
    handoff.run(action, context);
    const auto timeout = loop.add_timer(5s, 0ms, [&] { loop.quit(1); });
    REQUIRE(loop.run() == 0);
    loop.remove(timeout);
    REQUIRE(errors.size() == 1);
    CHECK(errors.front().find("status 7") != std::string::npos);
    std::ifstream input{dir.path / "arguments", std::ios::binary};
    const std::string bytes{std::istreambuf_iterator<char>{input},
                            std::istreambuf_iterator<char>{}};
    CHECK(bytes == dir.path.string() + '\0' + tricky + '\0');
    CHECK_FALSE(std::filesystem::exists(dir.path / "BAD"));
    action.argv = {"nordend-nonexistent-action-program", "{files}"};
    handoff.run(action, context);
    REQUIRE(errors.size() == 2);
    CHECK(errors.back().find("not installed") != std::string::npos);
}

TEST_CASE("actions: pending activation cannot use a destroyed handoff") {
    nordend::core::EventLoop loop;
    std::function<void(std::string)> pending;
    unsigned reports = 0;
    {
        nordend::platform::ActionHandoff handoff{
            loop, nordend::platform::Environment::from_environment(),
            [&](std::string, bool) { ++reports; }, [&](auto done) { pending = std::move(done); }};
        const Action action{.id = "noop",
                            .label = "Noop",
                            .argv = {"sh", "-c", "exit 0"},
                            .mime = {},
                            .selection = Action::Selection::background};
        handoff.run(action, {.directory = "/tmp", .targets = {}});
    }
    REQUIRE(pending);
    pending("token");
    CHECK(reports == 0);
}
