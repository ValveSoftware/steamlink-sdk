// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/surface_id_allocator.h"

#include <stdint.h>

#include "base/rand_util.h"
#include "base/unguessable_token.h"

namespace cc {

SurfaceIdAllocator::SurfaceIdAllocator() : next_id_(1u) {}

SurfaceIdAllocator::~SurfaceIdAllocator() {
}

LocalFrameId SurfaceIdAllocator::GenerateId() {
  LocalFrameId id(next_id_, base::UnguessableToken::Create());
  next_id_++;
  return id;
}

}  // namespace cc
