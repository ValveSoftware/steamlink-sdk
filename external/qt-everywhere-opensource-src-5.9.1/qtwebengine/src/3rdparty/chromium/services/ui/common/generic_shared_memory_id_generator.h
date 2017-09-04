// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_COMMON_GENERIC_SHARED_MEMORY_ID_GENERATOR_H_
#define SERVICES_UI_COMMON_GENERIC_SHARED_MEMORY_ID_GENERATOR_H_

#include "ui/gfx/generic_shared_memory_id.h"

namespace ui {

// Returns the next GenericSharedMemoryId for the current process. This should
// be used anywhere a new GenericSharedMemoryId is needed.
gfx::GenericSharedMemoryId GetNextGenericSharedMemoryId();

}  // namespace ui

#endif  // SERVICES_UI_COMMON_GENERIC_SHARED_MEMORY_ID_GENERATOR_H_
