// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/android/synchronous_compositor.h"

namespace content {

SynchronousCompositorMemoryPolicy::SynchronousCompositorMemoryPolicy()
    : bytes_limit(0), num_resources_limit(0) {}

bool SynchronousCompositorMemoryPolicy::operator==(
    const SynchronousCompositorMemoryPolicy& other) const {
  return bytes_limit == other.bytes_limit &&
      num_resources_limit == other.num_resources_limit;
}

bool SynchronousCompositorMemoryPolicy::operator!=(
    const SynchronousCompositorMemoryPolicy& other) const {
  return !(*this == other);
}

}  // namespace content
