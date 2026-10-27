#include "nordend/thumbs/cache.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <cstdlib>
#include <fcntl.h>
#include <fstream>
#include <optional>
#include <sys/stat.h>
#include <system_error>
#include <unistd.h>
#include <utility>
#include <vector>

#include "nordend/image/png.hpp"
#include "nordend/image/preview.hpp"
#include "nordend/image/scale.hpp"
#include "nordend/thumbs/md5.hpp"

namespace nordend::thumbs {
namespace {

constexpr std::string_view kUriKey = "Thumb::URI";
constexpr std::string_view kMtimeKey = "Thumb::MTime";
constexpr std::string_view kHex = "0123456789ABCDEF";

// GLib leaves these unescaped in the path of a file URI (g_filename_to_uri, checked with gio).
bool unescaped(unsigned char c) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
        return true;
    }
    return std::string_view{"!&'()*+,=:@$~_-./"}.find(static_cast<char>(c)) !=
           std::string_view::npos;
}

std::vector<std::uint8_t> read_file(const std::filesystem::path& path) {
    std::error_code error;
    const std::uintmax_t size = std::filesystem::file_size(path, error);
    if (error) {
        return {};
    }
    std::vector<std::uint8_t> data(size);
    std::ifstream file{path, std::ios::binary};
    if (!file.read(reinterpret_cast<char*>(data.data()),  // NOLINT: bytes to char for read
                   static_cast<std::streamsize>(size))) {
        return {};
    }
    return data;
}

// The PNG at path was written for this URI and modification time (spec: both must match).
bool matches(std::span<const std::uint8_t> png, const std::string& uri, std::int64_t mtime) {
    bool uri_ok = false;
    bool mtime_ok = false;
    for (const auto& [key, value] : image::png_text(png)) {
        if (key == kUriKey) {
            uri_ok = value == uri;
        } else if (key == kMtimeKey) {
            std::int64_t stored = 0;
            const auto [end, error] =
                std::from_chars(value.data(), value.data() + value.size(), stored);
            mtime_ok =
                error == std::errc{} && end == value.data() + value.size() && stored == mtime;
        }
    }
    return uri_ok && mtime_ok;
}

// Creates the directories down to path, the new ones private (0700, as the spec asks).
bool make_private_directories(const std::filesystem::path& path) {
    std::error_code error;
    std::filesystem::path partial;
    for (const std::filesystem::path& part : path) {
        partial /= part;
        if (!std::filesystem::is_directory(partial, error) && ::mkdir(partial.c_str(), 0700) != 0 &&
            errno != EEXIST) {
            return false;
        }
    }
    return std::filesystem::is_directory(path, error);
}

// Writes the PNG of image with the spec's text to target: to a private temporary file next to
// it, then renamed, so that readers never see half a thumbnail and two writers do not mix.
bool store(const std::filesystem::path& target, const image::Image& image, const std::string& uri,
           std::int64_t mtime) {
    const std::vector<std::uint8_t> png =
        image::encode_png(image, {{std::string{kUriKey}, uri},
                                  {std::string{kMtimeKey}, std::to_string(mtime)},
                                  {"Software", "Nordend"}});
    if (png.empty() || !make_private_directories(target.parent_path())) {
        return false;
    }
    std::string temporary = target.string() + ".nordend-XXXXXX";
    const int fd = ::mkstemp(temporary.data());  // mode 0600
    if (fd < 0) {
        return false;
    }
    std::size_t written = 0;
    while (written < png.size()) {
        const ssize_t n = ::write(fd, png.data() + written, png.size() - written);
        if (n <= 0) {
            break;
        }
        written += static_cast<std::size_t>(n);
    }
    const bool ok = ::close(fd) == 0 && written == png.size() &&
                    ::rename(temporary.c_str(), target.c_str()) == 0;
    if (!ok) {
        ::unlink(temporary.c_str());
    }
    return ok;
}

Thumbnail failure(std::string error) {
    Thumbnail thumbnail;
    thumbnail.error = std::move(error);
    return thumbnail;
}

}  // namespace

std::uint32_t pixels(Size size) noexcept {
    switch (size) {
        case Size::normal:
            return 128;
        case Size::large:
            return 256;
        case Size::x_large:
            return 512;
        case Size::xx_large:
            break;
    }
    return 1024;
}

std::string_view directory_name(Size size) noexcept {
    switch (size) {
        case Size::normal:
            return "normal";
        case Size::large:
            return "large";
        case Size::x_large:
            return "x-large";
        case Size::xx_large:
            break;
    }
    return "xx-large";
}

Size size_for(std::uint32_t side) noexcept {
    for (const Size size : {Size::normal, Size::large, Size::x_large}) {
        if (side <= pixels(size)) {
            return size;
        }
    }
    return Size::xx_large;
}

bool can_make(std::string_view mime) noexcept {
    static constexpr std::array<std::string_view, 27> kTypes{"image/png",
                                                             "image/jpeg",
                                                             "image/gif",
                                                             "image/webp",
                                                             "image/jxl",
                                                             "image/x-adobe-dng",
                                                             "image/x-canon-cr2",
                                                             "image/x-canon-cr3",
                                                             "image/x-canon-crw",
                                                             "image/x-dcraw",
                                                             "image/x-fuji-raf",
                                                             "image/x-kodak-dcr",
                                                             "image/x-kodak-k25",
                                                             "image/x-kodak-kdc",
                                                             "image/x-minolta-mrw",
                                                             "image/x-nikon-nef",
                                                             "image/x-nikon-nrw",
                                                             "image/x-olympus-orf",
                                                             "image/x-panasonic-raw",
                                                             "image/x-panasonic-raw2",
                                                             "image/x-panasonic-rw",
                                                             "image/x-panasonic-rw2",
                                                             "image/x-pentax-pef",
                                                             "image/x-sigma-x3f",
                                                             "image/x-sony-arw",
                                                             "image/x-sony-sr2",
                                                             "image/x-sony-srf"};
    return std::ranges::find(kTypes, mime) != kTypes.end();
}

std::string_view source_name(Source source) noexcept {
    switch (source) {
        case Source::cache:
            return "cache";
        case Source::preview:
            return "preview";
        case Source::decoded:
            return "decoded";
        case Source::failed:
            break;
    }
    return "failed";
}

std::string file_uri(const std::filesystem::path& absolute) {
    std::string uri = "file://";
    for (const char ch : absolute.native()) {
        const auto c = static_cast<unsigned char>(ch);
        if (unescaped(c)) {
            uri.push_back(ch);
        } else {
            uri.push_back('%');
            uri.push_back(kHex[c >> 4]);
            uri.push_back(kHex[c & 0x0F]);
        }
    }
    return uri;
}

std::filesystem::path default_root() {
    // NOLINTBEGIN(concurrency-mt-unsafe): read at startup, before any thread looks at it
    const char* cache = std::getenv("XDG_CACHE_HOME");
    if (cache != nullptr && *cache == '/') {
        return std::filesystem::path{cache} / "thumbnails";
    }
    const char* home = std::getenv("HOME");
    // NOLINTEND(concurrency-mt-unsafe)
    return std::filesystem::path{home != nullptr ? home : "/"} / ".cache" / "thumbnails";
}

Cache::Cache(std::filesystem::path root, std::string app)
    : root_{std::move(root)}, app_{std::move(app)} {}

std::filesystem::path Cache::path_for(const std::filesystem::path& file, Size size) const {
    return root_ / directory_name(size) / (md5_hex(file_uri(file)) + ".png");
}

std::filesystem::path Cache::fail_path(const std::filesystem::path& file) const {
    return root_ / "fail" / app_ / (md5_hex(file_uri(file)) + ".png");
}

Thumbnail Cache::get(const std::filesystem::path& file, Size size) const {
    struct stat info{};
    if (::stat(file.c_str(), &info) != 0) {
        return failure(std::error_code{errno, std::generic_category()}.message());
    }
    const std::string uri = file_uri(file);
    const std::vector<std::uint8_t> cached = read_file(path_for(file, size));
    if (!cached.empty() && matches(cached, uri, info.st_mtime)) {
        image::Decoded decoded = image::decode(cached);
        if (decoded.error.empty()) {
            return {.image = std::move(decoded.image), .source = Source::cache, .error = {}};
        }
    }
    const std::vector<std::uint8_t> failed = read_file(fail_path(file));
    if (!failed.empty() && matches(failed, uri, info.st_mtime)) {
        return failure("failed before, and unchanged since");
    }
    return make(file, size, info.st_mtime);
}

Thumbnail Cache::make(const std::filesystem::path& file, Size size) const {
    struct stat info{};
    if (::stat(file.c_str(), &info) != 0) {
        return failure(std::error_code{errno, std::generic_category()}.message());
    }
    return make(file, size, info.st_mtime);
}

Thumbnail Cache::make(const std::filesystem::path& file, Size size, std::int64_t mtime) const {
    const std::string uri = file_uri(file);
    const std::filesystem::path relative = file.lexically_relative(root_);
    if (!relative.empty() && *relative.begin() != "..") {
        return failure("a thumbnail itself");  // the spec: never thumbnail the cache
    }
    const std::uint32_t side = pixels(size);

    // A preview the file carries, when it is big enough or the file cannot be decoded at all
    // (a raw file); else the whole image.
    image::Image picture;
    std::uint8_t orientation = 1;
    Source source = Source::failed;
    std::string error;
    const image::EmbeddedPreview preview = image::embedded_preview(file, side);
    const bool preview_enough = std::max(preview.width, preview.height) >= side;
    if (!preview.jpeg.empty() && preview_enough) {
        image::Decoded decoded = image::decode(preview.jpeg, {.apply_orientation = false});
        if (decoded.error.empty()) {
            picture = std::move(decoded.image);
            orientation = preview.orientation;
            source = Source::preview;
        }
    }
    if (picture.empty()) {
        image::Decoded decoded = image::decode_file(file, {.apply_orientation = false});
        if (decoded.error.empty()) {
            picture = std::move(decoded.image);
            orientation = decoded.orientation;
            source = Source::decoded;
        } else if (!preview.jpeg.empty()) {
            // Not decodable (a raw file): the largest preview there is will do.
            image::Decoded small = image::decode(preview.jpeg, {.apply_orientation = false});
            if (small.error.empty()) {
                picture = std::move(small.image);
                orientation = preview.orientation;
                source = Source::preview;
            }
        }
        if (picture.empty()) {
            error = decoded.error;
        }
    }
    if (picture.empty()) {
        // Remembered, so that it is not tried again until the file changes.
        const image::Image blank{.width = 1, .height = 1, .rgba = {0, 0, 0, 0}};
        static_cast<void>(store(fail_path(file), blank, uri, mtime));
        return failure(error.empty() ? "no image in the file" : error);
    }
    // Scaled first, turned after: the turn costs little at thumbnail size.
    image::Image thumbnail =
        image::apply_orientation(image::scale_to_fit(picture, side, side), orientation);
    static_cast<void>(store(path_for(file, size), thumbnail, uri, mtime));
    return {.image = std::move(thumbnail), .source = source, .error = {}};
}

}  // namespace nordend::thumbs
