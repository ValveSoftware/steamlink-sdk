/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/graphics/gpu/DrawingBuffer.h"

#include "cc/resources/single_release_callback.h"
#include "cc/resources/texture_mailbox.h"
#include "gpu/command_buffer/client/gles2_interface_stub.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/UnacceleratedImageBufferSurface.h"
#include "platform/graphics/gpu/DrawingBufferTestHelpers.h"
#include "public/platform/Platform.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/PtrUtil.h"
#include "wtf/RefPtr.h"
#include <memory>

using testing::Test;
using testing::_;

namespace blink {

class DrawingBufferTest : public Test {
 protected:
  void SetUp() override {
    IntSize initialSize(InitialWidth, InitialHeight);
    std::unique_ptr<GLES2InterfaceForTests> gl =
        wrapUnique(new GLES2InterfaceForTests);
    m_gl = gl.get();
    SetAndSaveRestoreState(false);
    std::unique_ptr<WebGraphicsContext3DProviderForTests> provider =
        wrapUnique(new WebGraphicsContext3DProviderForTests(std::move(gl)));
    m_drawingBuffer = DrawingBufferForTests::create(
        std::move(provider), m_gl, initialSize, DrawingBuffer::Preserve);
    CHECK(m_drawingBuffer);
  }

  // Initialize GL state with unusual values, to verify that they are restored.
  // The |invert| parameter will reverse all boolean parameters, so that all
  // values are tested.
  void SetAndSaveRestoreState(bool invert) {
    GLboolean scissorEnabled = !invert;
    GLfloat clearColor[4] = {0.1, 0.2, 0.3, 0.4};
    GLfloat clearDepth = 0.8;
    GLint clearStencil = 37;
    GLboolean colorMask[4] = {invert, !invert, !invert, invert};
    GLboolean depthMask = invert;
    GLboolean stencilMask = invert;
    GLint packAlignment = 7;
    GLuint activeTexture2DBinding = 0xbeef1;
    GLuint renderbufferBinding = 0xbeef2;
    GLuint drawFramebufferBinding = 0xbeef3;
    GLuint readFramebufferBinding = 0xbeef4;
    GLuint pixelUnpackBufferBinding = 0xbeef5;

    if (scissorEnabled)
      m_gl->Enable(GL_SCISSOR_TEST);
    else
      m_gl->Disable(GL_SCISSOR_TEST);

    m_gl->ClearColor(clearColor[0], clearColor[1], clearColor[2],
                     clearColor[3]);
    m_gl->ClearDepthf(clearDepth);
    m_gl->ClearStencil(clearStencil);
    m_gl->ColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
    m_gl->DepthMask(depthMask);
    m_gl->StencilMask(stencilMask);
    m_gl->PixelStorei(GL_PACK_ALIGNMENT, packAlignment);
    m_gl->BindTexture(GL_TEXTURE_2D, activeTexture2DBinding);
    m_gl->BindRenderbuffer(GL_RENDERBUFFER, renderbufferBinding);
    m_gl->BindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFramebufferBinding);
    m_gl->BindFramebuffer(GL_READ_FRAMEBUFFER, readFramebufferBinding);
    m_gl->BindBuffer(GL_PIXEL_UNPACK_BUFFER, pixelUnpackBufferBinding);

    m_gl->SaveState();
  }

  void VerifyStateWasRestored() { m_gl->VerifyStateHasNotChangedSinceSave(); }

  GLES2InterfaceForTests* m_gl;
  RefPtr<DrawingBufferForTests> m_drawingBuffer;
};

TEST_F(DrawingBufferTest, verifyResizingProperlyAffectsMailboxes) {
  VerifyStateWasRestored();
  cc::TextureMailbox textureMailbox;
  std::unique_ptr<cc::SingleReleaseCallback> releaseCallback;

  IntSize initialSize(InitialWidth, InitialHeight);
  IntSize alternateSize(InitialWidth, AlternateHeight);

  // Produce one mailbox at size 100x100.
  m_drawingBuffer->markContentsChanged();
  EXPECT_TRUE(m_drawingBuffer->PrepareTextureMailbox(&textureMailbox,
                                                     &releaseCallback));
  VerifyStateWasRestored();
  EXPECT_EQ(initialSize, m_gl->mostRecentlyProducedSize());

  // Resize to 100x50.
  m_drawingBuffer->resize(alternateSize);
  VerifyStateWasRestored();
  releaseCallback->Run(gpu::SyncToken(), false /* lostResource */);
  VerifyStateWasRestored();

  // Produce a mailbox at this size.
  m_drawingBuffer->markContentsChanged();
  EXPECT_TRUE(m_drawingBuffer->PrepareTextureMailbox(&textureMailbox,
                                                     &releaseCallback));
  EXPECT_EQ(alternateSize, m_gl->mostRecentlyProducedSize());
  VerifyStateWasRestored();

  // Reset to initial size.
  m_drawingBuffer->resize(initialSize);
  VerifyStateWasRestored();
  SetAndSaveRestoreState(true);
  releaseCallback->Run(gpu::SyncToken(), false /* lostResource */);
  VerifyStateWasRestored();

  // Prepare another mailbox and verify that it's the correct size.
  m_drawingBuffer->markContentsChanged();
  EXPECT_TRUE(m_drawingBuffer->PrepareTextureMailbox(&textureMailbox,
                                                     &releaseCallback));
  EXPECT_EQ(initialSize, m_gl->mostRecentlyProducedSize());
  VerifyStateWasRestored();

  // Prepare one final mailbox and verify that it's the correct size.
  releaseCallback->Run(gpu::SyncToken(), false /* lostResource */);
  m_drawingBuffer->markContentsChanged();
  EXPECT_TRUE(m_drawingBuffer->PrepareTextureMailbox(&textureMailbox,
                                                     &releaseCallback));
  VerifyStateWasRestored();
  EXPECT_EQ(initialSize, m_gl->mostRecentlyProducedSize());
  releaseCallback->Run(gpu::SyncToken(), false /* lostResource */);
  m_drawingBuffer->beginDestruction();
}

TEST_F(DrawingBufferTest, verifyDestructionCompleteAfterAllMailboxesReleased) {
  bool live = true;
  m_drawingBuffer->m_live = &live;

  cc::TextureMailbox textureMailbox1;
  std::unique_ptr<cc::SingleReleaseCallback> releaseCallback1;
  cc::TextureMailbox textureMailbox2;
  std::unique_ptr<cc::SingleReleaseCallback> releaseCallback2;
  cc::TextureMailbox textureMailbox3;
  std::unique_ptr<cc::SingleReleaseCallback> releaseCallback3;

  IntSize initialSize(InitialWidth, InitialHeight);

  // Produce mailboxes.
  m_drawingBuffer->markContentsChanged();
  m_drawingBuffer->clearFramebuffers(GL_STENCIL_BUFFER_BIT);
  VerifyStateWasRestored();
  EXPECT_TRUE(m_drawingBuffer->PrepareTextureMailbox(&textureMailbox1,
                                                     &releaseCallback1));
  m_drawingBuffer->markContentsChanged();
  m_drawingBuffer->clearFramebuffers(GL_DEPTH_BUFFER_BIT);
  VerifyStateWasRestored();
  EXPECT_TRUE(m_drawingBuffer->PrepareTextureMailbox(&textureMailbox2,
                                                     &releaseCallback2));
  m_drawingBuffer->markContentsChanged();
  m_drawingBuffer->clearFramebuffers(GL_COLOR_BUFFER_BIT);
  VerifyStateWasRestored();
  EXPECT_TRUE(m_drawingBuffer->PrepareTextureMailbox(&textureMailbox3,
                                                     &releaseCallback3));

  m_drawingBuffer->markContentsChanged();
  releaseCallback1->Run(gpu::SyncToken(), false /* lostResource */);

  m_drawingBuffer->beginDestruction();
  ASSERT_EQ(live, true);

  DrawingBufferForTests* rawPointer = m_drawingBuffer.get();
  m_drawingBuffer.clear();
  ASSERT_EQ(live, true);

  rawPointer->markContentsChanged();
  releaseCallback2->Run(gpu::SyncToken(), false /* lostResource */);
  ASSERT_EQ(live, true);

  rawPointer->markContentsChanged();
  releaseCallback3->Run(gpu::SyncToken(), false /* lostResource */);
  ASSERT_EQ(live, false);
}

TEST_F(DrawingBufferTest, verifyDrawingBufferStaysAliveIfResourcesAreLost) {
  bool live = true;
  m_drawingBuffer->m_live = &live;

  cc::TextureMailbox textureMailbox1;
  std::unique_ptr<cc::SingleReleaseCallback> releaseCallback1;
  cc::TextureMailbox textureMailbox2;
  std::unique_ptr<cc::SingleReleaseCallback> releaseCallback2;
  cc::TextureMailbox textureMailbox3;
  std::unique_ptr<cc::SingleReleaseCallback> releaseCallback3;

  m_drawingBuffer->markContentsChanged();
  EXPECT_TRUE(m_drawingBuffer->PrepareTextureMailbox(&textureMailbox1,
                                                     &releaseCallback1));
  VerifyStateWasRestored();
  m_drawingBuffer->markContentsChanged();
  EXPECT_TRUE(m_drawingBuffer->PrepareTextureMailbox(&textureMailbox2,
                                                     &releaseCallback2));
  VerifyStateWasRestored();
  m_drawingBuffer->markContentsChanged();
  EXPECT_TRUE(m_drawingBuffer->PrepareTextureMailbox(&textureMailbox3,
                                                     &releaseCallback3));
  VerifyStateWasRestored();

  m_drawingBuffer->markContentsChanged();
  releaseCallback1->Run(gpu::SyncToken(), true /* lostResource */);
  EXPECT_EQ(live, true);

  m_drawingBuffer->beginDestruction();
  EXPECT_EQ(live, true);

  m_drawingBuffer->markContentsChanged();
  releaseCallback2->Run(gpu::SyncToken(), false /* lostResource */);
  EXPECT_EQ(live, true);

  DrawingBufferForTests* rawPtr = m_drawingBuffer.get();
  m_drawingBuffer.clear();
  EXPECT_EQ(live, true);

  rawPtr->markContentsChanged();
  releaseCallback3->Run(gpu::SyncToken(), true /* lostResource */);
  EXPECT_EQ(live, false);
}

TEST_F(DrawingBufferTest, verifyOnlyOneRecycledMailboxMustBeKept) {
  cc::TextureMailbox textureMailbox1;
  std::unique_ptr<cc::SingleReleaseCallback> releaseCallback1;
  cc::TextureMailbox textureMailbox2;
  std::unique_ptr<cc::SingleReleaseCallback> releaseCallback2;
  cc::TextureMailbox textureMailbox3;
  std::unique_ptr<cc::SingleReleaseCallback> releaseCallback3;

  // Produce mailboxes.
  m_drawingBuffer->markContentsChanged();
  EXPECT_TRUE(m_drawingBuffer->PrepareTextureMailbox(&textureMailbox1,
                                                     &releaseCallback1));
  m_drawingBuffer->markContentsChanged();
  EXPECT_TRUE(m_drawingBuffer->PrepareTextureMailbox(&textureMailbox2,
                                                     &releaseCallback2));
  m_drawingBuffer->markContentsChanged();
  EXPECT_TRUE(m_drawingBuffer->PrepareTextureMailbox(&textureMailbox3,
                                                     &releaseCallback3));

  // Release mailboxes by specific order; 1, 3, 2.
  m_drawingBuffer->markContentsChanged();
  releaseCallback1->Run(gpu::SyncToken(), false /* lostResource */);
  m_drawingBuffer->markContentsChanged();
  releaseCallback3->Run(gpu::SyncToken(), false /* lostResource */);
  m_drawingBuffer->markContentsChanged();
  releaseCallback2->Run(gpu::SyncToken(), false /* lostResource */);

  // The first recycled mailbox must be 2. 1 and 3 were deleted by FIFO order
  // because DrawingBuffer never keeps more than one mailbox.
  cc::TextureMailbox recycledTextureMailbox1;
  std::unique_ptr<cc::SingleReleaseCallback> recycledReleaseCallback1;
  m_drawingBuffer->markContentsChanged();
  EXPECT_TRUE(m_drawingBuffer->PrepareTextureMailbox(
      &recycledTextureMailbox1, &recycledReleaseCallback1));
  EXPECT_EQ(textureMailbox2.mailbox(), recycledTextureMailbox1.mailbox());

  // The second recycled mailbox must be a new mailbox.
  cc::TextureMailbox recycledTextureMailbox2;
  std::unique_ptr<cc::SingleReleaseCallback> recycledReleaseCallback2;
  m_drawingBuffer->markContentsChanged();
  EXPECT_TRUE(m_drawingBuffer->PrepareTextureMailbox(
      &recycledTextureMailbox2, &recycledReleaseCallback2));
  EXPECT_NE(textureMailbox1.mailbox(), recycledTextureMailbox2.mailbox());
  EXPECT_NE(textureMailbox2.mailbox(), recycledTextureMailbox2.mailbox());
  EXPECT_NE(textureMailbox3.mailbox(), recycledTextureMailbox2.mailbox());

  recycledReleaseCallback1->Run(gpu::SyncToken(), false /* lostResource */);
  recycledReleaseCallback2->Run(gpu::SyncToken(), false /* lostResource */);
  m_drawingBuffer->beginDestruction();
}

TEST_F(DrawingBufferTest, verifyInsertAndWaitSyncTokenCorrectly) {
  cc::TextureMailbox textureMailbox;
  std::unique_ptr<cc::SingleReleaseCallback> releaseCallback;

  // Produce mailboxes.
  m_drawingBuffer->markContentsChanged();
  EXPECT_EQ(gpu::SyncToken(), m_gl->mostRecentlyWaitedSyncToken());
  EXPECT_TRUE(m_drawingBuffer->PrepareTextureMailbox(&textureMailbox,
                                                     &releaseCallback));
  // PrepareTextureMailbox() does not wait for any sync point.
  EXPECT_EQ(gpu::SyncToken(), m_gl->mostRecentlyWaitedSyncToken());

  gpu::SyncToken waitSyncToken;
  m_gl->GenSyncTokenCHROMIUM(m_gl->InsertFenceSyncCHROMIUM(),
                             waitSyncToken.GetData());
  releaseCallback->Run(waitSyncToken, false /* lostResource */);
  // m_drawingBuffer will wait for the sync point when recycling.
  EXPECT_EQ(gpu::SyncToken(), m_gl->mostRecentlyWaitedSyncToken());

  m_drawingBuffer->markContentsChanged();
  EXPECT_TRUE(m_drawingBuffer->PrepareTextureMailbox(&textureMailbox,
                                                     &releaseCallback));
  // m_drawingBuffer waits for the sync point when recycling in
  // PrepareTextureMailbox().
  EXPECT_EQ(waitSyncToken, m_gl->mostRecentlyWaitedSyncToken());

  m_drawingBuffer->beginDestruction();
  m_gl->GenSyncTokenCHROMIUM(m_gl->InsertFenceSyncCHROMIUM(),
                             waitSyncToken.GetData());
  releaseCallback->Run(waitSyncToken, false /* lostResource */);
  // m_drawingBuffer waits for the sync point because the destruction is in
  // progress.
  EXPECT_EQ(waitSyncToken, m_gl->mostRecentlyWaitedSyncToken());
}

class DrawingBufferImageChromiumTest : public DrawingBufferTest {
 protected:
  void SetUp() override {
    IntSize initialSize(InitialWidth, InitialHeight);
    std::unique_ptr<GLES2InterfaceForTests> gl =
        wrapUnique(new GLES2InterfaceForTests);
    m_gl = gl.get();
    SetAndSaveRestoreState(true);
    std::unique_ptr<WebGraphicsContext3DProviderForTests> provider =
        wrapUnique(new WebGraphicsContext3DProviderForTests(std::move(gl)));
    RuntimeEnabledFeatures::setWebGLImageChromiumEnabled(true);
    m_imageId0 = m_gl->nextImageIdToBeCreated();
    EXPECT_CALL(*m_gl, BindTexImage2DMock(m_imageId0)).Times(1);
    m_drawingBuffer = DrawingBufferForTests::create(
        std::move(provider), m_gl, initialSize, DrawingBuffer::Preserve);
    CHECK(m_drawingBuffer);
    testing::Mock::VerifyAndClearExpectations(m_gl);
  }

  void TearDown() override {
    RuntimeEnabledFeatures::setWebGLImageChromiumEnabled(false);
  }

  GLuint m_imageId0;
};

TEST_F(DrawingBufferImageChromiumTest, verifyResizingReallocatesImages) {
  cc::TextureMailbox textureMailbox;
  std::unique_ptr<cc::SingleReleaseCallback> releaseCallback;

  IntSize initialSize(InitialWidth, InitialHeight);
  IntSize alternateSize(InitialWidth, AlternateHeight);

  GLuint m_imageId1 = m_gl->nextImageIdToBeCreated();
  EXPECT_CALL(*m_gl, BindTexImage2DMock(m_imageId1)).Times(1);
  // Produce one mailbox at size 100x100.
  m_drawingBuffer->markContentsChanged();
  EXPECT_TRUE(m_drawingBuffer->PrepareTextureMailbox(&textureMailbox,
                                                     &releaseCallback));
  EXPECT_EQ(initialSize, m_gl->mostRecentlyProducedSize());
  EXPECT_TRUE(textureMailbox.is_overlay_candidate());
  testing::Mock::VerifyAndClearExpectations(m_gl);
  VerifyStateWasRestored();

  GLuint m_imageId2 = m_gl->nextImageIdToBeCreated();
  EXPECT_CALL(*m_gl, BindTexImage2DMock(m_imageId2)).Times(1);
  EXPECT_CALL(*m_gl, DestroyImageMock(m_imageId0)).Times(1);
  EXPECT_CALL(*m_gl, ReleaseTexImage2DMock(m_imageId0)).Times(1);
  EXPECT_CALL(*m_gl, DestroyImageMock(m_imageId1)).Times(1);
  EXPECT_CALL(*m_gl, ReleaseTexImage2DMock(m_imageId1)).Times(1);
  // Resize to 100x50.
  m_drawingBuffer->resize(alternateSize);
  VerifyStateWasRestored();
  releaseCallback->Run(gpu::SyncToken(), false /* lostResource */);
  VerifyStateWasRestored();
  testing::Mock::VerifyAndClearExpectations(m_gl);

  GLuint m_imageId3 = m_gl->nextImageIdToBeCreated();
  EXPECT_CALL(*m_gl, BindTexImage2DMock(m_imageId3)).Times(1);
  // Produce a mailbox at this size.
  m_drawingBuffer->markContentsChanged();
  EXPECT_TRUE(m_drawingBuffer->PrepareTextureMailbox(&textureMailbox,
                                                     &releaseCallback));
  EXPECT_EQ(alternateSize, m_gl->mostRecentlyProducedSize());
  EXPECT_TRUE(textureMailbox.is_overlay_candidate());
  testing::Mock::VerifyAndClearExpectations(m_gl);

  GLuint m_imageId4 = m_gl->nextImageIdToBeCreated();
  EXPECT_CALL(*m_gl, BindTexImage2DMock(m_imageId4)).Times(1);
  EXPECT_CALL(*m_gl, DestroyImageMock(m_imageId2)).Times(1);
  EXPECT_CALL(*m_gl, ReleaseTexImage2DMock(m_imageId2)).Times(1);
  EXPECT_CALL(*m_gl, DestroyImageMock(m_imageId3)).Times(1);
  EXPECT_CALL(*m_gl, ReleaseTexImage2DMock(m_imageId3)).Times(1);
  // Reset to initial size.
  m_drawingBuffer->resize(initialSize);
  VerifyStateWasRestored();
  releaseCallback->Run(gpu::SyncToken(), false /* lostResource */);
  VerifyStateWasRestored();
  testing::Mock::VerifyAndClearExpectations(m_gl);

  GLuint m_imageId5 = m_gl->nextImageIdToBeCreated();
  EXPECT_CALL(*m_gl, BindTexImage2DMock(m_imageId5)).Times(1);
  // Prepare another mailbox and verify that it's the correct size.
  m_drawingBuffer->markContentsChanged();
  EXPECT_TRUE(m_drawingBuffer->PrepareTextureMailbox(&textureMailbox,
                                                     &releaseCallback));
  EXPECT_EQ(initialSize, m_gl->mostRecentlyProducedSize());
  EXPECT_TRUE(textureMailbox.is_overlay_candidate());
  testing::Mock::VerifyAndClearExpectations(m_gl);

  // Prepare one final mailbox and verify that it's the correct size.
  releaseCallback->Run(gpu::SyncToken(), false /* lostResource */);
  m_drawingBuffer->markContentsChanged();
  EXPECT_TRUE(m_drawingBuffer->PrepareTextureMailbox(&textureMailbox,
                                                     &releaseCallback));
  EXPECT_EQ(initialSize, m_gl->mostRecentlyProducedSize());
  EXPECT_TRUE(textureMailbox.is_overlay_candidate());
  releaseCallback->Run(gpu::SyncToken(), false /* lostResource */);

  EXPECT_CALL(*m_gl, DestroyImageMock(m_imageId5)).Times(1);
  EXPECT_CALL(*m_gl, ReleaseTexImage2DMock(m_imageId5)).Times(1);
  EXPECT_CALL(*m_gl, DestroyImageMock(m_imageId4)).Times(1);
  EXPECT_CALL(*m_gl, ReleaseTexImage2DMock(m_imageId4)).Times(1);
  m_drawingBuffer->beginDestruction();
  testing::Mock::VerifyAndClearExpectations(m_gl);
}

TEST_F(DrawingBufferImageChromiumTest, allocationFailure) {
  cc::TextureMailbox textureMailbox1;
  std::unique_ptr<cc::SingleReleaseCallback> releaseCallback1;
  cc::TextureMailbox textureMailbox2;
  std::unique_ptr<cc::SingleReleaseCallback> releaseCallback2;
  cc::TextureMailbox textureMailbox3;
  std::unique_ptr<cc::SingleReleaseCallback> releaseCallback3;

  // Request a mailbox. An image should already be created. Everything works
  // as expected.
  EXPECT_CALL(*m_gl, BindTexImage2DMock(_)).Times(1);
  IntSize initialSize(InitialWidth, InitialHeight);
  m_drawingBuffer->markContentsChanged();
  EXPECT_TRUE(m_drawingBuffer->PrepareTextureMailbox(&textureMailbox1,
                                                     &releaseCallback1));
  EXPECT_TRUE(textureMailbox1.is_overlay_candidate());
  testing::Mock::VerifyAndClearExpectations(m_gl);
  VerifyStateWasRestored();

  // Force image CHROMIUM creation failure. Request another mailbox. It should
  // still be provided, but this time with allowOverlay = false.
  m_gl->setCreateImageChromiumFail(true);
  m_drawingBuffer->markContentsChanged();
  EXPECT_TRUE(m_drawingBuffer->PrepareTextureMailbox(&textureMailbox2,
                                                     &releaseCallback2));
  EXPECT_FALSE(textureMailbox2.is_overlay_candidate());
  VerifyStateWasRestored();

  // Check that if image CHROMIUM starts working again, mailboxes are
  // correctly created with allowOverlay = true.
  EXPECT_CALL(*m_gl, BindTexImage2DMock(_)).Times(1);
  m_gl->setCreateImageChromiumFail(false);
  m_drawingBuffer->markContentsChanged();
  EXPECT_TRUE(m_drawingBuffer->PrepareTextureMailbox(&textureMailbox3,
                                                     &releaseCallback3));
  EXPECT_TRUE(textureMailbox3.is_overlay_candidate());
  testing::Mock::VerifyAndClearExpectations(m_gl);
  VerifyStateWasRestored();

  releaseCallback1->Run(gpu::SyncToken(), false /* lostResource */);
  releaseCallback2->Run(gpu::SyncToken(), false /* lostResource */);
  releaseCallback3->Run(gpu::SyncToken(), false /* lostResource */);

  EXPECT_CALL(*m_gl, DestroyImageMock(_)).Times(3);
  EXPECT_CALL(*m_gl, ReleaseTexImage2DMock(_)).Times(3);
  m_drawingBuffer->beginDestruction();
  testing::Mock::VerifyAndClearExpectations(m_gl);
}

class DepthStencilTrackingGLES2Interface
    : public gpu::gles2::GLES2InterfaceStub {
 public:
  void FramebufferRenderbuffer(GLenum target,
                               GLenum attachment,
                               GLenum renderbuffertarget,
                               GLuint renderbuffer) override {
    switch (attachment) {
      case GL_DEPTH_ATTACHMENT:
        m_depthAttachment = renderbuffer;
        break;
      case GL_STENCIL_ATTACHMENT:
        m_stencilAttachment = renderbuffer;
        break;
      case GL_DEPTH_STENCIL_ATTACHMENT:
        m_depthStencilAttachment = renderbuffer;
        break;
      default:
        ASSERT_NOT_REACHED();
        break;
    }
  }

  GLenum CheckFramebufferStatus(GLenum target) override {
    return GL_FRAMEBUFFER_COMPLETE;
  }

  void GetIntegerv(GLenum ptype, GLint* value) override {
    switch (ptype) {
      case GL_MAX_TEXTURE_SIZE:
        *value = 1024;
        return;
    }
  }

  const GLubyte* GetString(GLenum type) override {
    if (type == GL_EXTENSIONS)
      return reinterpret_cast<const GLubyte*>("GL_OES_packed_depth_stencil");
    return reinterpret_cast<const GLubyte*>("");
  }

  void GenRenderbuffers(GLsizei n, GLuint* renderbuffers) override {
    for (GLsizei i = 0; i < n; ++i)
      renderbuffers[i] = m_nextGenRenderbufferId++;
  }

  GLuint stencilAttachment() const { return m_stencilAttachment; }
  GLuint depthAttachment() const { return m_depthAttachment; }
  GLuint depthStencilAttachment() const { return m_depthStencilAttachment; }
  size_t numAllocatedRenderBuffer() const {
    return m_nextGenRenderbufferId - 1;
  }

 private:
  GLuint m_nextGenRenderbufferId = 1;
  GLuint m_depthAttachment = 0;
  GLuint m_stencilAttachment = 0;
  GLuint m_depthStencilAttachment = 0;
};

struct DepthStencilTestCase {
  DepthStencilTestCase(bool requestStencil,
                       bool requestDepth,
                       int expectedRenderBuffers,
                       const char* const testCaseName)
      : requestStencil(requestStencil),
        requestDepth(requestDepth),
        expectedRenderBuffers(expectedRenderBuffers),
        testCaseName(testCaseName) {}

  bool requestStencil;
  bool requestDepth;
  size_t expectedRenderBuffers;
  const char* const testCaseName;
};

// This tests that, when the packed depth+stencil extension is supported, and
// either depth or stencil is requested, DrawingBuffer always allocates a single
// packed renderbuffer and properly computes the actual context attributes as
// defined by WebGL. We always allocate a packed buffer in this case since many
// desktop OpenGL drivers that support this extension do not consider a
// framebuffer with only a depth or a stencil buffer attached to be complete.
TEST(DrawingBufferDepthStencilTest, packedDepthStencilSupported) {
  DepthStencilTestCase cases[] = {
      DepthStencilTestCase(false, false, 0, "neither"),
      DepthStencilTestCase(true, false, 1, "stencil only"),
      DepthStencilTestCase(false, true, 1, "depth only"),
      DepthStencilTestCase(true, true, 1, "both"),
  };

  for (size_t i = 0; i < WTF_ARRAY_LENGTH(cases); i++) {
    SCOPED_TRACE(cases[i].testCaseName);
    std::unique_ptr<DepthStencilTrackingGLES2Interface> gl =
        wrapUnique(new DepthStencilTrackingGLES2Interface);
    DepthStencilTrackingGLES2Interface* trackingGL = gl.get();
    std::unique_ptr<WebGraphicsContext3DProviderForTests> provider =
        wrapUnique(new WebGraphicsContext3DProviderForTests(std::move(gl)));
    DrawingBuffer::PreserveDrawingBuffer preserve = DrawingBuffer::Preserve;

    bool premultipliedAlpha = false;
    bool wantAlphaChannel = true;
    bool wantDepthBuffer = cases[i].requestDepth;
    bool wantStencilBuffer = cases[i].requestStencil;
    bool wantAntialiasing = false;
    RefPtr<DrawingBuffer> drawingBuffer = DrawingBuffer::create(
        std::move(provider), nullptr, IntSize(10, 10), premultipliedAlpha,
        wantAlphaChannel, wantDepthBuffer, wantStencilBuffer, wantAntialiasing,
        preserve, DrawingBuffer::WebGL1, DrawingBuffer::AllowChromiumImage);

    // When we request a depth or a stencil buffer, we will get both.
    EXPECT_EQ(cases[i].requestDepth || cases[i].requestStencil,
              drawingBuffer->hasDepthBuffer());
    EXPECT_EQ(cases[i].requestDepth || cases[i].requestStencil,
              drawingBuffer->hasStencilBuffer());
    EXPECT_EQ(cases[i].expectedRenderBuffers,
              trackingGL->numAllocatedRenderBuffer());
    if (cases[i].requestDepth || cases[i].requestStencil) {
      EXPECT_NE(0u, trackingGL->depthStencilAttachment());
      EXPECT_EQ(0u, trackingGL->depthAttachment());
      EXPECT_EQ(0u, trackingGL->stencilAttachment());
    } else {
      EXPECT_EQ(0u, trackingGL->depthStencilAttachment());
      EXPECT_EQ(0u, trackingGL->depthAttachment());
      EXPECT_EQ(0u, trackingGL->stencilAttachment());
    }

    drawingBuffer->resize(IntSize(10, 20));
    EXPECT_EQ(cases[i].requestDepth || cases[i].requestStencil,
              drawingBuffer->hasDepthBuffer());
    EXPECT_EQ(cases[i].requestDepth || cases[i].requestStencil,
              drawingBuffer->hasStencilBuffer());
    EXPECT_EQ(cases[i].expectedRenderBuffers,
              trackingGL->numAllocatedRenderBuffer());
    if (cases[i].requestDepth || cases[i].requestStencil) {
      EXPECT_NE(0u, trackingGL->depthStencilAttachment());
      EXPECT_EQ(0u, trackingGL->depthAttachment());
      EXPECT_EQ(0u, trackingGL->stencilAttachment());
    } else {
      EXPECT_EQ(0u, trackingGL->depthStencilAttachment());
      EXPECT_EQ(0u, trackingGL->depthAttachment());
      EXPECT_EQ(0u, trackingGL->stencilAttachment());
    }

    drawingBuffer->beginDestruction();
  }
}

TEST_F(DrawingBufferTest, verifySetIsHiddenProperlyAffectsMailboxes) {
  cc::TextureMailbox textureMailbox;
  std::unique_ptr<cc::SingleReleaseCallback> releaseCallback;

  // Produce mailboxes.
  m_drawingBuffer->markContentsChanged();
  EXPECT_TRUE(m_drawingBuffer->PrepareTextureMailbox(&textureMailbox,
                                                     &releaseCallback));

  gpu::SyncToken waitSyncToken;
  m_gl->GenSyncTokenCHROMIUM(m_gl->InsertFenceSyncCHROMIUM(),
                             waitSyncToken.GetData());
  m_drawingBuffer->setIsHidden(true);
  releaseCallback->Run(waitSyncToken, false /* lostResource */);
  // m_drawingBuffer deletes mailbox immediately when hidden.

  EXPECT_EQ(waitSyncToken, m_gl->mostRecentlyWaitedSyncToken());

  m_drawingBuffer->beginDestruction();
}

}  // namespace blink
