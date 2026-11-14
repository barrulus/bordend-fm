#include "rummage/platform/file_picker_portal.hpp"

#include <chrono>
#include <csignal>
#include <fstream>
#include <future>
#include <iostream>
#include <map>
#include <sdbus-c++/sdbus-c++.h>
#include <thread>
#include <unistd.h>

#include "rummage/core/file_picker.hpp"

namespace {
using namespace std::chrono_literals;
using Values = std::map<std::string, sdbus::Variant>;
using Response = std::pair<std::uint32_t, Values>;
const sdbus::ServiceName service{"org.freedesktop.impl.portal.desktop.rummage"};
const sdbus::ObjectPath path{"/org/freedesktop/portal/desktop"};
constexpr auto iface = "org.freedesktop.impl.portal.FileChooser";
volatile std::sig_atomic_t stopped = 0;

void require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error{message};
    }
}

int child() {
    auto request = rummage::core::read_picker_request(3);
    if (request.title == "graceful" || request.title == "stubborn") {
        std::signal(SIGTERM, request.title == "stubborn" ? SIG_IGN : +[](int) { stopped = 1; });
        const auto folder = std::filesystem::path{request.folder};
        std::ofstream{folder / "ready"} << "ready";
        while (!stopped) {
            usleep(1000);
        }
        std::ofstream{folder / "flushed"} << "state saved";
        return 0;
    }
    if (request.title == "crash") {
        return 1;
    }
    if (request.title == "wait") {
        for (;;) {
            pause();
        }
    }
    if (request.title == "malformed") {
        return 0;
    }
    rummage::core::PickerResult result;
    if (request.title != "cancel") {
        result.response = 0;
        result.paths = {"/tmp/picker space#\n.txt"};
        if (request.multiple || request.mode == rummage::core::PickerRequest::Mode::save_files) {
            result.paths.push_back("/tmp/second.txt");
        }
        result.filter = request.filter;
        for (const auto& c : request.choices) {
            result.choices.push_back(c.selected);
        }
    }
    rummage::core::write_picker_result(4, result);
    return 0;
}

std::future<Response> call(sdbus::IProxy& proxy, const std::string& method,
                           const std::string& handle, const std::string& title,
                           Values values = {}) {
    auto promise = std::make_shared<std::promise<Response>>();
    auto future = promise->get_future();
    proxy.callMethodAsync(method)
        .onInterface(iface)
        .withArguments(sdbus::ObjectPath{handle}, std::string{"test"}, std::string{}, title, values)
        .uponReplyInvoke(
            [promise](std::optional<sdbus::Error> error, std::uint32_t response, Values result) {
                if (error) {
                    promise->set_exception(std::make_exception_ptr(*error));
                } else {
                    promise->set_value({response, std::move(result)});
                }
            });
    return future;
}

Response get(std::future<Response>& future) {
    require(future.wait_for(5s) == std::future_status::ready, "Portal response timed out");
    return future.get();
}

void client() {
    auto connection = sdbus::createSessionBusConnection();
    connection->enterEventLoopAsync();
    auto proxy = sdbus::createProxy(*connection, service, path);
    bool ready = false;
    for (int i = 0; i < 100 && !ready; ++i) {
        try {
            ready = proxy->getProperty("version").onInterface(iface).get<std::uint32_t>() == 4;
        } catch (const sdbus::Error&) {
            std::this_thread::sleep_for(10ms);
        }
    }
    require(ready, "Backend did not acquire its bus name");
    using Pattern = sdbus::Struct<std::uint32_t, std::string>;
    using Filter = sdbus::Struct<std::string, std::vector<Pattern>>;
    using Option = sdbus::Struct<std::string, std::string>;
    using Choice = sdbus::Struct<std::string, std::string, std::vector<Option>, std::string>;
    Filter filter{"Text", std::vector<Pattern>{{0, "*.txt"}}};
    Values options{
        {"multiple", sdbus::Variant{true}},
        {"current_filter", sdbus::Variant{filter}},
        {"choices", sdbus::Variant{std::vector<Choice>{{"readonly", "Read only", {}, "true"}}}}};
    auto open = call(*proxy, "OpenFile", "/test/open", "open", options);
    auto [status, values] = get(open);
    require(status == 0, "OpenFile failed");
    const auto uris = values.at("uris").get<std::vector<std::string>>();
    require(uris == std::vector<std::string>{"file:///tmp/picker%20space%23%0A.txt",
                                             "file:///tmp/second.txt"},
            "Wrong or unescaped URIs");
    require(values.at("current_filter").get<Filter>() == filter, "Filter not returned");
    require(values.at("choices").get<std::vector<Option>>() ==
                std::vector<Option>{{"readonly", "true"}},
            "Choices not returned");
    auto save = call(*proxy, "SaveFile", "/test/save", "save");
    require(get(save).first == 0, "SaveFile failed");
    Values batch{
        {"files", sdbus::Variant{std::vector<std::vector<std::uint8_t>>{{'a', 0}, {'b', 0}}}}};
    auto saves = call(*proxy, "SaveFiles", "/test/saves", "save", batch);
    require(get(saves).first == 0, "SaveFiles failed");
    auto cancel = call(*proxy, "OpenFile", "/test/cancel", "cancel");
    require(get(cancel).first == 1, "Cancel reported success");
    for (const auto* title : {"crash", "malformed"}) {
        auto failed = call(*proxy, "OpenFile", "/test/failed", title);
        require(get(failed).first == 2, "Child failure did not report an error");
    }
    Values invalid{{"current_folder", sdbus::Variant{std::vector<std::uint8_t>{'x'}}}};
    auto bad = call(*proxy, "OpenFile", "/test/bad", "open", invalid);
    require(get(bad).first == 2, "Unterminated path accepted");
    auto waiting = call(*proxy, "OpenFile", "/test/waiting", "wait");
    auto concurrent = call(*proxy, "OpenFile", "/test/concurrent", "open");
    require(get(concurrent).first == 0, "Blocked picker stopped concurrent requests");
    auto closer = sdbus::createProxy(*connection, service, sdbus::ObjectPath{"/test/waiting"});
    closer->callMethod("Close").onInterface("org.freedesktop.impl.portal.Request");
    require(get(waiting).first == 1, "Request.Close did not cancel");
    bool removed = false;
    try {
        closer->callMethod("Close").onInterface("org.freedesktop.impl.portal.Request");
    } catch (const sdbus::Error&) {
        removed = true;
    }
    require(removed, "Finished Request object was not removed");

    for (const auto* title : {"graceful", "stubborn"}) {
        struct Scratch {
            std::string directory{"/tmp/rummage-portal-test-XXXXXX"};

            Scratch() { require(mkdtemp(directory.data()) != nullptr, "Cannot make test folder"); }

            ~Scratch() { std::filesystem::remove_all(directory); }
        } scratch;

        const auto folder = std::filesystem::path{scratch.directory};
        std::vector<std::uint8_t> bytes(scratch.directory.begin(), scratch.directory.end());
        bytes.push_back(0);
        auto pending = call(*proxy, "OpenFile", "/test/lifecycle", title,
                            {{"current_folder", sdbus::Variant{bytes}}});
        const auto deadline = std::chrono::steady_clock::now() + 5s;
        while (!std::filesystem::exists(folder / "ready") &&
               std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(1ms);
        }
        require(std::filesystem::exists(folder / "ready"), "Picker did not become ready");
        auto close_request =
            sdbus::createProxy(*connection, service, sdbus::ObjectPath{"/test/lifecycle"});
        // Another D-Bus connection may not cancel this caller's picker.
        auto stranger = sdbus::createSessionBusConnection();
        auto foreign = sdbus::createProxy(*stranger, service, sdbus::ObjectPath{"/test/lifecycle"});
        bool denied = false;
        try {
            foreign->callMethod("Close").onInterface("org.freedesktop.impl.portal.Request");
        } catch (const sdbus::Error&) {
            denied = true;
        }
        require(denied, "Another sender could cancel the request");
        // Stay busy longer than the test service's idle timeout.
        std::this_thread::sleep_for(1100ms);
        require(pending.wait_for(0ms) != std::future_status::ready, "Active request was stopped");
        close_request->callMethod("Close").onInterface("org.freedesktop.impl.portal.Request");
        require(get(pending).first == 1, "Lifecycle cancellation was not reported as cancelled");
        require(
            std::filesystem::exists(folder / "flushed") == (std::string_view{title} == "graceful"),
            "Graceful picker did not flush, or stubborn picker was not killed");
    }
    // With no outstanding requests the backend should leave the bus by itself.
    auto daemon = sdbus::createProxy(*connection, sdbus::ServiceName{"org.freedesktop.DBus"},
                                     sdbus::ObjectPath{"/org/freedesktop/DBus"});
    bool owned = true;
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (owned && std::chrono::steady_clock::now() < deadline) {
        daemon->callMethod("NameHasOwner")
            .onInterface("org.freedesktop.DBus")
            .withArguments(std::string{service})
            .storeResultsTo(owned);
        if (owned) {
            std::this_thread::sleep_for(10ms);
        }
    }
    require(!owned, "Idle backend did not exit");
    std::cout << "FileChooser D-Bus integration passed\n";
}
}  // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::string_view{argv[1]} == "--picker-channel") {
        return child();
    }
    rummage::core::EventLoop loop;
    std::exception_ptr error;
    std::exception_ptr backend_error;
    std::thread caller{[&] {
        try {
            client();
        } catch (...) {
            error = std::current_exception();
            loop.post([&] { loop.quit(); });
        }
    }};
    try {
        rummage::platform::run_file_picker_portal(
            loop, {.picker_program = std::filesystem::read_symlink("/proc/self/exe"),
                   .idle_timeout = 1s,
                   .stop_timeout = 100ms});
    } catch (...) {
        backend_error = std::current_exception();
    }
    caller.join();
    if (backend_error) {
        error = backend_error;
    }
    if (error) {
        try {
            std::rethrow_exception(error);
        } catch (const std::exception& e) {
            std::cerr << e.what() << '\n';
        }
        return 1;
    }
    return 0;
}
