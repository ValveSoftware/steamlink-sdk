// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GENERIC_SHARED_MEMORY_ID_GENERATOR_H_
#define CONTENT_COMMON_GENERIC_SHARED_MEMORY_ID_GENERATOR_H_

#include "ui/gfx/generic_shared_memory_id.h"
#include "content/common/content_export.h"

namespace content {

// Returns the next GenericSharedMemoryId for the current process. This should
// be used anywhere a new GenericSharedMemoryId is needed.
CONTENT_EXPORT gfx::GenericSharedMemoryId GetNextGenericSharedMemoryId();

}  // namespace content

#endif  // CONTENT_COMMON_GENERIC_SHARED_MEMORY_ID_GENERATOR_H_
