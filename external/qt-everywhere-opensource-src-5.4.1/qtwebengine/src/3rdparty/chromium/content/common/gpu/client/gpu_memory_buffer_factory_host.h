// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_CLIENT_GPU_MEMORY_BUFFER_FACTORY_HOST_H_
#define CONTENT_COMMON_GPU_CLIENT_GPU_MEMORY_BUFFER_FACTORY_HOST_H_

#include "base/callback.h"

namespace gfx {
class Size;
struct GpuMemoryBufferHandle;
}

namespace content {

class CONTENT_EXPORT GpuMemoryBufferFactoryHost {
 public:
  typedef base::Callback<void(const gfx::GpuMemoryBufferHandle& handle)>
      CreateGpuMemoryBufferCallback;

  virtual void CreateGpuMemoryBuffer(
      const gfx::GpuMemoryBufferHandle& handle,
      const gfx::Size& size,
      unsigned internalformat,
      unsigned usage,
      const CreateGpuMemoryBufferCallback& callback) = 0;
  virtual void DestroyGpuMemoryBuffer(const gfx::GpuMemoryBufferHandle& handle,
                                      int32 sync_point) = 0;

 protected:
  virtual ~GpuMemoryBufferFactoryHost() {}
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_CLIENT_GPU_MEMORY_BUFFER_FACTORY_HOST_H_
