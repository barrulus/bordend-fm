#pragma once

#include <functional>
#include <memory>
#include <string>

#include "nordend/core/actions.hpp"
#include "nordend/platform/archives.hpp"
#include "nordend/platform/desktop_entry.hpp"
#include "nordend/platform/launcher.hpp"

namespace nordend::platform {

// External applications own their dialogs and work. Built-in CLI archive actions instead
// track extraction and publish completed output without replacing existing files.
class ActionHandoff {
public:
    using Report = std::function<void(std::string, bool)>;
    using Activate = std::function<void(std::function<void(std::string)>)>;
    using DirectoryChosen = std::function<void(std::optional<std::filesystem::path>)>;
    using ChooseDirectory = std::function<void(const std::filesystem::path&, DirectoryChosen)>;
    ActionHandoff(core::EventLoop& loop, Environment environment, Report report,
                  Activate activate = {}, ChooseDirectory choose = {});
    ~ActionHandoff();
    [[nodiscard]] bool available(const std::string& program) const;
    [[nodiscard]] bool available(const core::Action& action,
                                 const core::ActionContext& context) const;
    void run(const core::Action& action, const core::ActionContext& context);

private:
    struct Extraction;
    void run_archive(const core::Action& action, const core::ActionContext& context);
    void choose_destination(const std::filesystem::path& initial, DirectoryChosen chosen);
    void extract(const core::ActionTarget& target, const ArchiveBackend& backend,
                 const std::filesystem::path& destination);
    void extract_step(std::shared_ptr<Extraction> extraction, bool second_pass = false);
    Launcher launcher_;
    Environment environment_;
    Report report_;
    Activate activate_;
    ChooseDirectory choose_;
    std::shared_ptr<bool> alive_{std::make_shared<bool>(true)};
};

}  // namespace nordend::platform
