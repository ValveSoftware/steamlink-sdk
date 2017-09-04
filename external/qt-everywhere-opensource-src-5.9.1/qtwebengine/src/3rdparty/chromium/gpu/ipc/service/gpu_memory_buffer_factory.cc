// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_memory_buffer_factory.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"

#if defined(OS_MACOSX)
#include "gpu/ipc/service/gpu_memory_buffer_factory_io_surface.h"
#endif

#if defined(USE_OZONE)
#include "gpu/ipc/service/gpu_memory_buffer_factory_ozone_native_pixmap.h"
#endif

namespace gpu {

// static
std::unique_ptr<GpuMemoryBufferFactory>
GpuMemoryBufferFactory::CreateNativeType() {
#if defined(OS_MACOSX)
  return base::WrapUnique(new GpuMemoryBufferFactoryIOSurface);
#endif
#if defined(USE_OZONE)
  return base::WrapUnique(new GpuMemoryBufferFactoryOzoneNativePixmap);
#endif
  NOTREACHED();
  return nullptr;
}

}  // namespace gpu
