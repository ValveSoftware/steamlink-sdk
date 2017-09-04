// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/gpu/DrawingBuffer.h"

#include "cc/resources/single_release_callback.h"
#include "cc/resources/texture_mailbox.h"
#include "gpu/command_buffer/client/gles2_interface_stub.h"
#include "platform/graphics/gpu/DrawingBufferTestHelpers.h"
#include "testing/gtest/include/gtest/gtest.h"

// These unit tests are separate from DrawingBufferTests.cpp because they are
// built as a part of webkit_unittests instead blink_platform_unittests. This is
// because the software rendering mode has a dependency on the blink::Platform
// interface for buffer allocations.

namespace blink {
namespace {

using namespace testing;

class WebGraphicsContext3DProviderSoftwareRenderingForTests
    : public WebGraphicsContext3DProvider {
 public:
  WebGraphicsContext3DProviderSoftwareRenderingForTests(
      std::unique_ptr<gpu::gles2::GLES2Interface> gl)
      : m_gl(std::move(gl)) {}

  gpu::gles2::GLES2Interface* contextGL() override { return m_gl.get(); }
  bool isSoftwareRendering() const override { return true; }

  // Not used by WebGL code.
  GrContext* grContext() override { return nullptr; }
  bool bindToCurrentThread() override { return false; }
  gpu::Capabilities getCapabilities() override { return gpu::Capabilities(); }
  void setLostContextCallback(const base::Closure&) {}
  void setErrorMessageCallback(
      const base::Callback<void(const char*, int32_t id)>&) {}
  void signalQuery(uint32_t, const base::Closure&) override {}

 private:
  std::unique_ptr<gpu::gles2::GLES2Interface> m_gl;
};

class DrawingBufferSoftwareRenderingTest : public Test {
 protected:
  void SetUp() override {
    IntSize initialSize(InitialWidth, InitialHeight);
    std::unique_ptr<GLES2InterfaceForTests> gl =
        wrapUnique(new GLES2InterfaceForTests);
    std::unique_ptr<WebGraphicsContext3DProviderSoftwareRenderingForTests>
        provider = wrapUnique(
            new WebGraphicsContext3DProviderSoftwareRenderingForTests(
                std::move(gl)));
    m_drawingBuffer = DrawingBufferForTests::create(
        std::move(provider), nullptr, initialSize, DrawingBuffer::Preserve);
    CHECK(m_drawingBuffer);
  }

  RefPtr<DrawingBufferForTests> m_drawingBuffer;
  bool m_isSoftwareRendering = false;
};

TEST_F(DrawingBufferSoftwareRenderingTest, bitmapRecycling) {
  cc::TextureMailbox textureMailbox;
  std::unique_ptr<cc::SingleReleaseCallback> releaseCallback1;
  std::unique_ptr<cc::SingleReleaseCallback> releaseCallback2;
  std::unique_ptr<cc::SingleReleaseCallback> releaseCallback3;
  IntSize initialSize(InitialWidth, InitialHeight);
  IntSize alternateSize(InitialWidth, AlternateHeight);

  m_drawingBuffer->resize(initialSize);
  m_drawingBuffer->markContentsChanged();
  m_drawingBuffer->PrepareTextureMailbox(
      &textureMailbox, &releaseCallback1);  // create a bitmap.
  EXPECT_EQ(0, m_drawingBuffer->recycledBitmapCount());
  releaseCallback1->Run(
      gpu::SyncToken(),
      false /* lostResource */);  // release bitmap to the recycling queue
  EXPECT_EQ(1, m_drawingBuffer->recycledBitmapCount());
  m_drawingBuffer->markContentsChanged();
  m_drawingBuffer->PrepareTextureMailbox(
      &textureMailbox, &releaseCallback2);  // recycle a bitmap.
  EXPECT_EQ(0, m_drawingBuffer->recycledBitmapCount());
  releaseCallback2->Run(
      gpu::SyncToken(),
      false /* lostResource */);  // release bitmap to the recycling queue
  EXPECT_EQ(1, m_drawingBuffer->recycledBitmapCount());
  m_drawingBuffer->resize(alternateSize);
  m_drawingBuffer->markContentsChanged();
  // Regression test for crbug.com/647896 - Next line must not crash
  m_drawingBuffer->PrepareTextureMailbox(
      &textureMailbox,
      &releaseCallback3);  // cause recycling queue to be purged due to resize
  EXPECT_EQ(0, m_drawingBuffer->recycledBitmapCount());
  releaseCallback3->Run(gpu::SyncToken(), false /* lostResource */);
  EXPECT_EQ(1, m_drawingBuffer->recycledBitmapCount());

  m_drawingBuffer->beginDestruction();
}

}  // unnamed namespace
}  // blink
