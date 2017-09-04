// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines tests that implementations of GpuMemoryBufferFactory should
// pass in order to be conformant.

#ifndef GPU_IPC_SERVICE_GPU_MEMORY_BUFFER_FACTORY_TEST_TEMPLATE_H_
#define GPU_IPC_SERVICE_GPU_MEMORY_BUFFER_FACTORY_TEST_TEMPLATE_H_

#if defined(OS_ANDROID)
// TODO(markdittmer): Service code shouldn't depend on client code.
// See crbug.com/608800.
#include "gpu/ipc/client/android/in_process_surface_texture_manager.h"

#include "gpu/ipc/common/android/surface_texture_manager.h"
#endif

#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/buffer_format_util.h"

namespace gpu {

template <typename GpuMemoryBufferFactoryType>
class GpuMemoryBufferFactoryTest : public testing::Test {
 protected:
  GpuMemoryBufferFactoryType factory_;
};

TYPED_TEST_CASE_P(GpuMemoryBufferFactoryTest);

TYPED_TEST_P(GpuMemoryBufferFactoryTest, CreateGpuMemoryBuffer) {
#if defined(OS_ANDROID)
  SurfaceTextureManager::SetInstance(
      InProcessSurfaceTextureManager::GetInstance());
#endif

  const gfx::GpuMemoryBufferId kBufferId(1);
  const int kClientId = 1;

  gfx::Size buffer_size(2, 2);

  for (auto format : gfx::GetBufferFormatsForTesting()) {
    gfx::BufferUsage usages[] = {
        gfx::BufferUsage::GPU_READ, gfx::BufferUsage::SCANOUT,
        gfx::BufferUsage::GPU_READ_CPU_READ_WRITE,
        gfx::BufferUsage::GPU_READ_CPU_READ_WRITE_PERSISTENT};
    for (auto usage : usages) {
      if (!IsNativeGpuMemoryBufferConfigurationSupported(format, usage))
        continue;

      gfx::GpuMemoryBufferHandle handle =
          TestFixture::factory_.CreateGpuMemoryBuffer(kBufferId, buffer_size,
                                                      format, usage, kClientId,
                                                      gpu::kNullSurfaceHandle);
      EXPECT_NE(handle.type, gfx::EMPTY_BUFFER);
      TestFixture::factory_.DestroyGpuMemoryBuffer(kBufferId, kClientId);
    }
  }
}

// The GpuMemoryBufferFactoryTest test case verifies behavior that is expected
// from a GpuMemoryBuffer factory in order to be conformant.
REGISTER_TYPED_TEST_CASE_P(GpuMemoryBufferFactoryTest, CreateGpuMemoryBuffer);

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_GPU_MEMORY_BUFFER_FACTORY_TEST_TEMPLATE_H_
