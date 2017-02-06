// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_COMMON_GENERIC_SHARED_MEMORY_ID_GENERATOR_H_
#define COMPONENTS_MUS_COMMON_GENERIC_SHARED_MEMORY_ID_GENERATOR_H_

#include "components/mus/common/mus_common_export.h"
#include "ui/gfx/generic_shared_memory_id.h"

namespace mus {

// Returns the next GenericSharedMemoryId for the current process. This should
// be used anywhere a new GenericSharedMemoryId is needed.
MUS_COMMON_EXPORT gfx::GenericSharedMemoryId GetNextGenericSharedMemoryId();

}  // namespace mus

#endif  // COMPONENTS_MUS_COMMON_GENERIC_SHARED_MEMORY_ID_GENERATOR_H_
