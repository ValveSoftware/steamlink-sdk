// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/resource.h"

namespace cc {

size_t Resource::bytes() const {
  if (size_.IsEmpty())
    return 0;

  return MemorySizeBytes(size_, format_);
}


}  // namespace cc
