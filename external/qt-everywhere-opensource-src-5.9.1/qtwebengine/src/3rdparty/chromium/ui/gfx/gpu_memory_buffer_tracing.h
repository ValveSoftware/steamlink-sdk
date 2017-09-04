// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/memory_allocator_dump_guid.h"
#include "ui/gfx/gfx_export.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace gfx {

base::trace_event::MemoryAllocatorDumpGuid GFX_EXPORT
GetGpuMemoryBufferGUIDForTracing(uint64_t tracing_process_id,
                                 GpuMemoryBufferId buffer_id);

}  // namespace gfx
