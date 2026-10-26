#include "nordend/ui/seat.hpp"

#include <array>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <sys/mman.h>
#include <sys/timerfd.h>
#include <unistd.h>
#include <utility>

#include "nordend/core/log.hpp"

namespace nordend::ui {
namespace {

// xkbcommon numbers keys as evdev codes + 8.
constexpr xkb_keycode_t kEvdevOffset = 8;
// Repeats delivered per timer wake-up at most, so a stalled loop does not burst-type.
constexpr std::uint64_t kMaxRepeatsPerWake = 2;

// Locale for compose tables: the first of LC_ALL, LC_CTYPE, LANG that is set, else "C".
const char* compose_locale() {
    for (const char* name : {"LC_ALL", "LC_CTYPE", "LANG"}) {
        const char* value = std::getenv(name);
        if (value != nullptr && *value != '\0') {
            return value;
        }
    }
    return "C";
}

itimerspec milliseconds_to_timer(std::int32_t first, std::int32_t interval) {
    const auto to_timespec = [](std::int32_t ms) {
        return timespec{.tv_sec = ms / 1000, .tv_nsec = static_cast<long>(ms % 1000) * 1'000'000L};
    };
    return {.it_interval = to_timespec(interval), .it_value = to_timespec(first)};
}

}  // namespace

Seat::Seat(Display& display)
    : repeat_fd_{timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC)},
      context_{xkb_context_new(XKB_CONTEXT_NO_FLAGS)} {
    if (repeat_fd_ < 0) {
        throw std::runtime_error{std::string{"timerfd_create: "} + std::strerror(errno)};
    }
    if (!context_) {
        close(repeat_fd_);
        throw std::runtime_error{"xkb_context_new failed"};
    }
    compose_table_.reset(xkb_compose_table_new_from_locale(context_.get(), compose_locale(),
                                                           XKB_COMPOSE_COMPILE_NO_FLAGS));
    if (compose_table_) {
        compose_state_.reset(
            xkb_compose_state_new(compose_table_.get(), XKB_COMPOSE_STATE_NO_FLAGS));
    } else {
        log::warn("no compose table for locale '{}': dead keys disabled", compose_locale());
    }

    if (display.seat() == nullptr) {
        log::warn("compositor offers no wl_seat: no keyboard input");
        return;
    }
    static constexpr wl_seat_listener kSeatListener{
        .capabilities = &Seat::on_capabilities,
        .name = &Seat::on_name,
    };
    wl_seat_add_listener(display.seat(), &kSeatListener, this);
}

Seat::~Seat() {
    keyboard_.reset();
    if (repeat_fd_ >= 0) {
        close(repeat_fd_);
    }
}

void Seat::on_capabilities(void* data, wl_seat* seat, std::uint32_t capabilities) {
    auto& self = *static_cast<Seat*>(data);
    const bool has_keyboard = (capabilities & WL_SEAT_CAPABILITY_KEYBOARD) != 0;
    if (has_keyboard && !self.keyboard_) {
        static constexpr wl_keyboard_listener kKeyboardListener{
            .keymap = &Seat::on_keymap,
            .enter = &Seat::on_keyboard_enter,
            .leave = &Seat::on_keyboard_leave,
            .key = &Seat::on_key,
            .modifiers = &Seat::on_modifiers,
            .repeat_info = &Seat::on_repeat_info,
        };
        self.keyboard_.reset(wl_seat_get_keyboard(seat));
        wl_keyboard_add_listener(self.keyboard_.get(), &kKeyboardListener, &self);
    } else if (!has_keyboard && self.keyboard_) {
        self.disarm_repeat();
        self.keyboard_.reset();
    }
}

void Seat::on_name(void* /*data*/, wl_seat* /*seat*/, const char* name) {
    log::debug("seat: {}", name);
}

void Seat::on_keymap(void* data, wl_keyboard* /*keyboard*/, std::uint32_t format, std::int32_t fd,
                     std::uint32_t size) {
    auto& self = *static_cast<Seat*>(data);
    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
        log::warn("unsupported keymap format {}", format);
        close(fd);
        return;
    }
    // Since wl_seat v7 the fd must be mapped MAP_PRIVATE.
    void* map = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (map == MAP_FAILED) {
        log::error("cannot map the keymap: {}", std::strerror(errno));
        return;
    }
    XkbPtr<xkb_keymap, xkb_keymap_unref> keymap{
        xkb_keymap_new_from_string(self.context_.get(), static_cast<const char*>(map),
                                   XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS)};
    munmap(map, size);
    if (!keymap) {
        log::error("cannot compile the compositor's keymap");
        return;
    }
    self.state_.reset(xkb_state_new(keymap.get()));
    self.keymap_ = std::move(keymap);
    log::debug("keymap: layout '{}'", xkb_keymap_layout_get_name(self.keymap_.get(), 0));
}

void Seat::on_keyboard_enter(void* /*data*/, wl_keyboard* /*keyboard*/, std::uint32_t /*serial*/,
                             wl_surface* /*surface*/, wl_array* /*keys*/) {}

void Seat::on_keyboard_leave(void* data, wl_keyboard* /*keyboard*/, std::uint32_t /*serial*/,
                             wl_surface* /*surface*/) {
    auto& self = *static_cast<Seat*>(data);
    self.disarm_repeat();
    if (self.compose_state_) {
        xkb_compose_state_reset(self.compose_state_.get());
    }
}

void Seat::on_key(void* data, wl_keyboard* /*keyboard*/, std::uint32_t /*serial*/,
                  std::uint32_t /*time*/, std::uint32_t key, std::uint32_t state) {
    auto& self = *static_cast<Seat*>(data);
    if (!self.state_) {
        return;
    }
    const xkb_keycode_t keycode = key + kEvdevOffset;
    if (state == WL_KEYBOARD_KEY_STATE_RELEASED) {
        if (keycode == self.repeat_key_) {
            self.disarm_repeat();
        }
        return;
    }
    self.press(keycode, false);
    if (xkb_keymap_key_repeats(self.keymap_.get(), keycode) != 0) {
        self.arm_repeat(keycode);
    } else {
        self.disarm_repeat();
    }
}

void Seat::on_modifiers(void* data, wl_keyboard* /*keyboard*/, std::uint32_t /*serial*/,
                        std::uint32_t depressed, std::uint32_t latched, std::uint32_t locked,
                        std::uint32_t group) {
    auto& self = *static_cast<Seat*>(data);
    if (self.state_) {
        xkb_state_update_mask(self.state_.get(), depressed, latched, locked, 0, 0, group);
    }
}

void Seat::on_repeat_info(void* data, wl_keyboard* /*keyboard*/, std::int32_t rate,
                          std::int32_t delay) {
    auto& self = *static_cast<Seat*>(data);
    self.repeat_rate_ = rate;
    self.repeat_delay_ = delay;
    log::debug("key repeat: {} per second after {} ms", rate, delay);
}

void Seat::press(xkb_keycode_t keycode, bool repeat) {
    KeyEvent event{.keysym = xkb_state_key_get_one_sym(state_.get(), keycode),
                   .text = {},
                   .modifiers = current_modifiers(),
                   .repeat = repeat};

    if (compose_state_ && !repeat) {
        xkb_compose_state_feed(compose_state_.get(), event.keysym);
    }
    const auto status =
        compose_state_ ? xkb_compose_state_get_status(compose_state_.get()) : XKB_COMPOSE_NOTHING;
    std::array<char, 64> buffer{};
    switch (status) {
        case XKB_COMPOSE_COMPOSING:
        case XKB_COMPOSE_CANCELLED:
            return;  // inside a dead-key sequence, or it was aborted: no key for the app
        case XKB_COMPOSE_COMPOSED:
            event.keysym = xkb_compose_state_get_one_sym(compose_state_.get());
            xkb_compose_state_get_utf8(compose_state_.get(), buffer.data(), buffer.size());
            xkb_compose_state_reset(compose_state_.get());
            break;
        case XKB_COMPOSE_NOTHING:
            xkb_state_key_get_utf8(state_.get(), keycode, buffer.data(), buffer.size());
            break;
    }
    event.text = buffer.data();
    // Control characters (Ctrl+letter, Escape, Backspace...) are not text.
    if (event.text.size() == 1 &&
        (static_cast<unsigned char>(event.text[0]) < 0x20U || event.text[0] == '\x7f')) {
        event.text.clear();
    }
    if (key_handler_) {
        key_handler_(event);
    }
}

void Seat::arm_repeat(xkb_keycode_t keycode) {
    if (repeat_rate_ <= 0) {
        return;
    }
    repeat_key_ = keycode;
    const itimerspec timer = milliseconds_to_timer(repeat_delay_, std::max(1, 1000 / repeat_rate_));
    timerfd_settime(repeat_fd_, 0, &timer, nullptr);
}

void Seat::disarm_repeat() {
    repeat_key_ = 0;
    const itimerspec timer{};
    timerfd_settime(repeat_fd_, 0, &timer, nullptr);
}

void Seat::dispatch_repeat() {
    std::uint64_t expirations = 0;
    if (read(repeat_fd_, &expirations, sizeof(expirations)) != sizeof(expirations) ||
        repeat_key_ == 0) {
        return;
    }
    for (std::uint64_t i = 0; i < std::min(expirations, kMaxRepeatsPerWake); ++i) {
        press(repeat_key_, true);
    }
}

Modifiers Seat::current_modifiers() const {
    const auto active = [&](const char* name) {
        return xkb_state_mod_name_is_active(state_.get(), name, XKB_STATE_MODS_EFFECTIVE) > 0;
    };
    return {.ctrl = active(XKB_MOD_NAME_CTRL),
            .shift = active(XKB_MOD_NAME_SHIFT),
            .alt = active(XKB_MOD_NAME_ALT),
            .super = active(XKB_MOD_NAME_LOGO)};
}

}  // namespace nordend::ui
