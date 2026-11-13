#include "rummage/views/undo.hpp"

#include <format>

namespace rummage::views {

std::string describe(const FileOperation& operation) {
    const std::size_t count = operation.items.size();
    std::string what = count == 1
                           ? std::format("“{}”", operation.items.front().to.filename().string())
                           : std::format("{} items", count);
    switch (operation.kind) {
        case FileOperation::Kind::rename:
            if (count == 1) {
                return std::format("“{}” is “{}” again",
                                   operation.items.front().from.filename().string(),
                                   operation.items.front().to.filename().string());
            }
            break;
        case FileOperation::Kind::trash:
            if (count == 1) {
                return std::format("“{}” moved to the trash",
                                   operation.items.front().from.filename().string());
            }
            return std::format("{} moved to the trash", what);
        case FileOperation::Kind::restore:
            return std::format("{} restored", what);
        case FileOperation::Kind::copy:
        case FileOperation::Kind::move:
            if (count > 0) {
                return std::format("{} back in {}", what,
                                   operation.items.front().to.parent_path().filename().string());
            }
            break;
        case FileOperation::Kind::create:
            return std::format("{} made", what);
        case FileOperation::Kind::erase:
            break;
    }
    return what;
}

}  // namespace rummage::views
