#pragma once

#include "nordend/core/worker_pool.hpp"
#include "nordend/picture/load.hpp"

namespace nordend::thumbs {

// Decode on a worker, in a separate, cancellable process with a 15 second deadline.
// The helper accepts local file protocols only; no shell interprets the filename.
[[nodiscard]] picture::Loaded load_video(const picture::LoadRequest& request,
                                         const core::CancelToken& token);

}  // namespace nordend::thumbs
