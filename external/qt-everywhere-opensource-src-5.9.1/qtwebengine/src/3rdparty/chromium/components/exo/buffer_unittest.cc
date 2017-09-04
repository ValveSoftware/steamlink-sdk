// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GLES2/gl2extchromium.h>

#include "base/bind.h"
#include "cc/output/context_provider.h"
#include "cc/resources/single_release_callback.h"
#include "components/exo/buffer.h"
#include "components/exo/surface.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/exo_test_helper.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/aura/env.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace exo {
namespace {

using BufferTest = test::ExoTestBase;

void Release(int* release_call_count) {
  (*release_call_count)++;
}

TEST_F(BufferTest, ReleaseCallback) {
  gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));

  // Set the release callback.
  int release_call_count = 0;
  buffer->set_release_callback(
      base::Bind(&Release, base::Unretained(&release_call_count)));

  buffer->OnAttach();
  // Produce a texture mailbox for the contents of the buffer.
  cc::TextureMailbox texture_mailbox;
  std::unique_ptr<cc::SingleReleaseCallback> buffer_release_callback =
      buffer->ProduceTextureMailbox(&texture_mailbox, false, true);
  ASSERT_TRUE(buffer_release_callback);

  // Release buffer.
  buffer_release_callback->Run(gpu::SyncToken(), false);

  ASSERT_EQ(release_call_count, 0);

  buffer->OnDetach();

  // Release() should have been called exactly once.
  ASSERT_EQ(release_call_count, 1);
}

TEST_F(BufferTest, IsLost) {
  gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));

  buffer->OnAttach();
  // Acquire a texture mailbox for the contents of the buffer.
  cc::TextureMailbox texture_mailbox;
  std::unique_ptr<cc::SingleReleaseCallback> buffer_release_callback =
      buffer->ProduceTextureMailbox(&texture_mailbox, false, true);
  ASSERT_TRUE(buffer_release_callback);

  scoped_refptr<cc::ContextProvider> context_provider =
      aura::Env::GetInstance()
          ->context_factory()
          ->SharedMainThreadContextProvider();
  if (context_provider) {
    gpu::gles2::GLES2Interface* gles2 = context_provider->ContextGL();
    gles2->LoseContextCHROMIUM(GL_GUILTY_CONTEXT_RESET_ARB,
                               GL_INNOCENT_CONTEXT_RESET_ARB);
  }

  // Release buffer.
  bool is_lost = true;
  buffer_release_callback->Run(gpu::SyncToken(), is_lost);

  // Producing a new texture mailbox for the contents of the buffer.
  std::unique_ptr<cc::SingleReleaseCallback> buffer_release_callback2 =
      buffer->ProduceTextureMailbox(&texture_mailbox, false, false);
  ASSERT_TRUE(buffer_release_callback2);
  buffer->OnDetach();

  buffer_release_callback2->Run(gpu::SyncToken(), false);
}

}  // namespace
}  // namespace exo
