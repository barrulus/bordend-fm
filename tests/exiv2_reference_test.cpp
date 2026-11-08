// Nordend's own metadata readers (ROADMAP 2e) against the exiv2 code they replace
// (exiv2_reference.cpp): the same embedded previews (a JPEG's bytes, a TIFF one's pixels, size,
// crop, orientation, full size; why none was found is only a log line, not compared)
// and the same photo information, file by file. On the fixtures here, every file of the
// NORDEND_RAW_SAMPLES folder and of the folders in NORDEND_EXTRA_SAMPLES (colon-separated: the
// author's own photos, never in the repository), when they are set. Built only when exiv2 is
// installed (tests only: never linked into Nordend or its helper).
#include "exiv2_reference.hpp"

#include <cstdint>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>
#include <vector>

#include <doctest/doctest.h>

#include "nordend/formats/image/photo.hpp"
#include "nordend/formats/image/preview.hpp"

namespace fs = std::filesystem;
namespace image = nordend::formats::image;

namespace {

const fs::path kFixtures = fs::path{NORDEND_TEST_DATA} / "image";

// The files to compare: the fixtures, then every regular file of the sample folders.
std::vector<fs::path> files() {
    std::vector<fs::path> out;
    std::vector<fs::path> folders{kFixtures};
    if (const char* raw = std::getenv("NORDEND_RAW_SAMPLES"); raw != nullptr && *raw != '\0') {
        folders.emplace_back(raw);
    }
    if (const char* extra = std::getenv("NORDEND_EXTRA_SAMPLES"); extra != nullptr) {
        std::string_view rest{extra};
        while (!rest.empty()) {
            const std::size_t colon = rest.find(':');
            if (colon != 0) {
                folders.emplace_back(std::string{rest.substr(0, colon)});
            }
            rest = colon == std::string_view::npos ? std::string_view{} : rest.substr(colon + 1);
        }
    }
    for (const fs::path& folder : folders) {
        std::error_code error;
        for (const auto& entry : fs::directory_iterator{folder, error}) {
            const std::string extension = entry.path().extension().string();
            if (entry.is_regular_file() && extension != ".xmp" && extension != ".sh" &&
                extension != ".7z") {
                out.push_back(entry.path());
            }
        }
    }
    return out;
}

struct Fd {
    explicit Fd(const fs::path& path) : fd{::open(path.c_str(), O_RDONLY | O_CLOEXEC)} {}

    ~Fd() {
        if (fd >= 0) {
            ::close(fd);
        }
    }

    Fd(const Fd&) = delete;
    Fd& operator=(const Fd&) = delete;
    Fd(Fd&&) = delete;
    Fd& operator=(Fd&&) = delete;

    int fd;
};

void same_preview(const image::EmbeddedPreview& ours, const image::EmbeddedPreview& theirs) {
    // A JPEG is the file's bytes, the same; a TIFF preview is copied out of the file as a TIFF
    // of its own, by each in its way: the same pixels.
    if (ours.tiff && theirs.tiff) {
        const image::Decoded a = image::decode_preview(ours);
        const image::Decoded b = image::decode_preview(theirs);
        CHECK(a.error == b.error);
        CHECK(a.image.width == b.image.width);
        CHECK(a.image.height == b.image.height);
        CHECK(a.image.rgba == b.image.rgba);
    } else {
        CHECK(ours.data == theirs.data);
    }
    CHECK(ours.tiff == theirs.tiff);
    CHECK(ours.width == theirs.width);
    CHECK(ours.height == theirs.height);
    CHECK(ours.crop.has_value() == theirs.crop.has_value());
    if (ours.crop && theirs.crop) {
        CHECK(ours.crop->x == theirs.crop->x);
        CHECK(ours.crop->y == theirs.crop->y);
        CHECK(ours.crop->width == theirs.crop->width);
        CHECK(ours.crop->height == theirs.crop->height);
    }
    CHECK(ours.orientation == theirs.orientation);
    CHECK(ours.full_width == theirs.full_width);
    CHECK(ours.full_height == theirs.full_height);
}

void same_info(const image::PhotoInfo& ours, const image::PhotoInfo& theirs) {
    CHECK(ours.width == theirs.width);
    CHECK(ours.height == theirs.height);
    CHECK(ours.camera == theirs.camera);
    CHECK(ours.lens == theirs.lens);
    CHECK(ours.exposure_time == theirs.exposure_time);
    CHECK(ours.aperture == theirs.aperture);
    CHECK(ours.iso == theirs.iso);
    CHECK(ours.focal_length == theirs.focal_length);
    CHECK(ours.taken == theirs.taken);
    CHECK(ours.program == theirs.program);
    CHECK(ours.exposure_bias == theirs.exposure_bias);
    CHECK(ours.metering == theirs.metering);
    CHECK(ours.flash == theirs.flash);
    CHECK(ours.white_balance == theirs.white_balance);
    CHECK(ours.software == theirs.software);
    CHECK(ours.rating == theirs.rating);
}

}  // namespace

TEST_CASE("exiv2 reference: embedded previews and photo information as exiv2 gave them") {
    const std::vector<fs::path> all = files();
    REQUIRE_FALSE(all.empty());
    for (const fs::path& path : all) {
        INFO(path.string());
        const Fd file{path};
        REQUIRE(file.fd >= 0);
        // A thumbnail, Quick Look on a small and on a large screen.
        for (const std::uint32_t side : {160U, 1600U, 4000U}) {
            INFO("long side ", side);
            same_preview(image::embedded_preview(file.fd, side),
                         image::reference::embedded_preview(file.fd, side));
        }
        fs::path sidecar_path = path;
        sidecar_path += ".xmp";
        const Fd sidecar{sidecar_path};
        same_info(image::read_photo_info(file.fd, sidecar.fd),
                  image::reference::read_photo_info(file.fd, sidecar.fd));
    }
}
