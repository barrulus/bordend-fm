#include "nordend/ui/clipboard.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

#include "nordend/core/log.hpp"

namespace nordend::ui {

Clipboard::Clipboard(Display& display, core::EventLoop& loop) : display_{display}, loop_{loop} {
    if (display.data_device_manager() == nullptr || display.seat() == nullptr) {
        log::warn("compositor offers no clipboard: copy and paste stay inside Nordend");
        return;
    }
    static constexpr wl_data_device_listener kDeviceListener{
        .data_offer = &Clipboard::on_data_offer,
        .enter = &Clipboard::on_enter,
        .leave = &Clipboard::on_leave,
        .motion = &Clipboard::on_motion,
        .drop = &Clipboard::on_drop,
        .selection = &Clipboard::on_selection,
    };
    device_.reset(
        wl_data_device_manager_get_data_device(display.data_device_manager(), display.seat()));
    wl_data_device_add_listener(device_.get(), &kDeviceListener, this);
}

Clipboard::~Clipboard() {
    for (const Writer& writer : writers_) {
        loop_.remove(writer.source);
        close(writer.fd);
    }
    for (const Reader& reader : readers_) {
        loop_.remove(reader.source);
        loop_.remove(reader.timer);
        close(reader.fd);
    }
    for (const auto& [offer, types] : offers_) {
        wl_data_offer_destroy(offer);
    }
}

void Clipboard::offer(std::vector<Format> formats, std::uint32_t serial) {
    if (!device_) {
        return;
    }
    static constexpr wl_data_source_listener kSourceListener{
        .target = [](void*, wl_data_source*, const char*) {},
        .send = &Clipboard::on_source_send,
        .cancelled = &Clipboard::on_source_cancelled,
        .dnd_drop_performed = [](void*, wl_data_source*) {},
        .dnd_finished = [](void*, wl_data_source*) {},
        .action = [](void*, wl_data_source*, std::uint32_t) {},
    };
    WlPtr<wl_data_source> source{
        wl_data_device_manager_create_data_source(display_.data_device_manager())};
    wl_data_source_add_listener(source.get(), &kSourceListener, this);
    for (const Format& format : formats) {
        wl_data_source_offer(source.get(), format.first.c_str());
    }
    wl_data_device_set_selection(device_.get(), source.get(), serial);
    source_ = std::move(source);  // the old one goes: no cancelled event for it, nor lost_
    formats_ = std::move(formats);
}

void Clipboard::clear(std::uint32_t serial) {
    if (!device_) {
        return;
    }
    wl_data_device_set_selection(device_.get(), nullptr, serial);
    source_.reset();
    formats_.clear();
}

std::vector<std::string> Clipboard::types() const {
    if (selection_ == nullptr) {
        return {};
    }
    const auto it = offers_.find(selection_);
    return it == offers_.end() ? std::vector<std::string>{} : it->second;
}

void Clipboard::read(const std::string& type, Done done) {
    std::array<int, 2> fds{-1, -1};
    if (selection_ == nullptr || pipe2(fds.data(), O_CLOEXEC | O_NONBLOCK) != 0) {
        loop_.post([done = std::move(done)] { done(std::nullopt); });
        return;
    }
    wl_data_offer_receive(selection_, type.c_str(), fds[1]);
    wl_display_flush(display_.native());
    close(fds[1]);
    readers_.push_back(
        {.fd = fds[0], .data = {}, .done = std::move(done), .source = 0, .timer = 0});
    const auto reader = std::prev(readers_.end());
    reader->source = loop_.add_fd(reader->fd, POLLIN, [this, reader](short /*revents*/) {
        std::array<char, 65536> buffer{};
        for (;;) {
            const ssize_t got = ::read(reader->fd, buffer.data(), buffer.size());
            if (got > 0) {
                reader->data.append(buffer.data(), static_cast<std::size_t>(got));
                if (reader->data.size() > kMaxRead) {
                    log::warn("clipboard: more than {} bytes, given up", kMaxRead);
                    finish_read(reader, std::nullopt);
                    return;
                }
                continue;
            }
            if (got == 0) {
                finish_read(reader, std::move(reader->data));
                return;
            }
            if (errno == EINTR) {
                continue;
            }
            if (errno != EAGAIN) {
                finish_read(reader, std::nullopt);
            }
            return;
        }
    });
    reader->timer = loop_.add_timer(kReadTimeout, std::chrono::milliseconds{0}, [this, reader] {
        reader->timer = 0;  // a one-shot timer is gone once it fired
        log::warn("clipboard: the program offering it did not answer in time");
        finish_read(reader, std::nullopt);
    });
}

void Clipboard::finish_read(std::list<Reader>::iterator reader, std::optional<std::string> data) {
    loop_.remove(reader->source);
    if (reader->timer != 0) {
        loop_.remove(reader->timer);
    }
    close(reader->fd);
    Done done = std::move(reader->done);
    readers_.erase(reader);
    done(std::move(data));
}

void Clipboard::write_some(std::list<Writer>::iterator writer) {
    while (writer->written < writer->data.size()) {
        const ssize_t put = ::write(writer->fd, writer->data.data() + writer->written,
                                    writer->data.size() - writer->written);
        if (put > 0) {
            writer->written += static_cast<std::size_t>(put);
            continue;
        }
        if (put < 0 && errno == EINTR) {
            continue;
        }
        if (put < 0 && errno == EAGAIN) {
            return;  // the pipe is full: when the reader takes some
        }
        break;  // EPIPE: the reader went away
    }
    loop_.remove(writer->source);
    close(writer->fd);
    writers_.erase(writer);
}

void Clipboard::forget_offer(wl_data_offer* offer) {
    if (offer == nullptr) {
        return;
    }
    if (offers_.erase(offer) > 0) {
        wl_data_offer_destroy(offer);
    }
}

void Clipboard::on_data_offer(void* data, wl_data_device* /*device*/, wl_data_offer* offer) {
    auto& self = *static_cast<Clipboard*>(data);
    static constexpr wl_data_offer_listener kOfferListener{
        .offer = &Clipboard::on_offer_type,
        .source_actions = [](void*, wl_data_offer*, std::uint32_t) {},
        .action = [](void*, wl_data_offer*, std::uint32_t) {},
    };
    self.offers_.emplace(offer, std::vector<std::string>{});
    wl_data_offer_add_listener(offer, &kOfferListener, &self);
}

void Clipboard::on_offer_type(void* data, wl_data_offer* offer, const char* type) {
    auto& self = *static_cast<Clipboard*>(data);
    if (const auto it = self.offers_.find(offer); it != self.offers_.end()) {
        it->second.emplace_back(type);
    }
}

void Clipboard::on_enter(void* data, wl_data_device* /*device*/, std::uint32_t /*serial*/,
                         wl_surface* /*surface*/, wl_fixed_t /*x*/, wl_fixed_t /*y*/,
                         wl_data_offer* offer) {
    // Drag and drop is not taken yet: the offer is kept only until the drag leaves.
    auto& self = *static_cast<Clipboard*>(data);
    if (self.dragged_ != self.selection_) {
        self.forget_offer(self.dragged_);
    }
    self.dragged_ = offer;
}

void Clipboard::on_leave(void* data, wl_data_device* /*device*/) {
    auto& self = *static_cast<Clipboard*>(data);
    if (self.dragged_ != self.selection_) {
        self.forget_offer(self.dragged_);
    }
    self.dragged_ = nullptr;
}

void Clipboard::on_motion(void* /*data*/, wl_data_device* /*device*/, std::uint32_t /*time*/,
                          wl_fixed_t /*x*/, wl_fixed_t /*y*/) {}

void Clipboard::on_drop(void* /*data*/, wl_data_device* /*device*/) {}

void Clipboard::on_selection(void* data, wl_data_device* /*device*/, wl_data_offer* offer) {
    auto& self = *static_cast<Clipboard*>(data);
    if (self.selection_ != offer && self.selection_ != self.dragged_) {
        self.forget_offer(self.selection_);
    }
    if (self.selection_ != offer || offer == nullptr) {
        ++self.generation_;
    }
    self.selection_ = offer;
}

void Clipboard::on_source_send(void* data, wl_data_source* /*source*/, const char* type, int fd) {
    auto& self = *static_cast<Clipboard*>(data);
    const auto format = std::ranges::find(self.formats_, std::string_view{type}, &Format::first);
    if (format == self.formats_.end() || fcntl(fd, F_SETFL, O_NONBLOCK) != 0) {
        close(fd);
        return;
    }
    self.writers_.push_back({.fd = fd, .data = format->second, .written = 0, .source = 0});
    const auto writer = std::prev(self.writers_.end());
    writer->source = self.loop_.add_fd(
        fd, POLLOUT, [&self, writer](short /*revents*/) { self.write_some(writer); });
}

void Clipboard::on_source_cancelled(void* data, wl_data_source* source) {
    auto& self = *static_cast<Clipboard*>(data);
    if (source != self.source_.get()) {
        return;  // a replaced source is destroyed at once: it never gets here
    }
    self.source_.reset();
    self.formats_.clear();
    if (self.lost_) {
        self.lost_();
    }
}

}  // namespace nordend::ui
