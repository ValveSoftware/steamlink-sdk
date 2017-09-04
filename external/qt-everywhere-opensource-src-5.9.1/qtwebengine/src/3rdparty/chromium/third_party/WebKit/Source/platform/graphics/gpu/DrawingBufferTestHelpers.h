// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/common/capabilities.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/gpu/DrawingBuffer.h"
#include "platform/graphics/gpu/Extensions3DUtil.h"
#include "public/platform/WebGraphicsContext3DProvider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

enum {
  InitialWidth = 100,
  InitialHeight = 100,
  AlternateHeight = 50,
};

class DrawingBufferForTests : public DrawingBuffer {
 public:
  static PassRefPtr<DrawingBufferForTests> create(
      std::unique_ptr<WebGraphicsContext3DProvider> contextProvider,
      DrawingBuffer::Client* client,
      const IntSize& size,
      PreserveDrawingBuffer preserve) {
    std::unique_ptr<Extensions3DUtil> extensionsUtil =
        Extensions3DUtil::create(contextProvider->contextGL());
    RefPtr<DrawingBufferForTests> drawingBuffer = adoptRef(
        new DrawingBufferForTests(std::move(contextProvider),
                                  std::move(extensionsUtil), client, preserve));
    bool multisampleExtensionSupported = false;
    if (!drawingBuffer->initialize(size, multisampleExtensionSupported)) {
      drawingBuffer->beginDestruction();
      return nullptr;
    }
    return drawingBuffer.release();
  }

  DrawingBufferForTests(
      std::unique_ptr<WebGraphicsContext3DProvider> contextProvider,
      std::unique_ptr<Extensions3DUtil> extensionsUtil,
      DrawingBuffer::Client* client,
      PreserveDrawingBuffer preserve)
      : DrawingBuffer(
            std::move(contextProvider),
            std::move(extensionsUtil),
            client,
            false /* discardFramebufferSupported */,
            true /* wantAlphaChannel */,
            false /* premultipliedAlpha */,
            preserve,
            WebGL1,
            false /* wantDepth */,
            false /* wantStencil */,
            DrawingBuffer::AllowChromiumImage /* ChromiumImageUsage */),
        m_live(0) {}

  ~DrawingBufferForTests() override {
    if (m_live)
      *m_live = false;
  }

  bool* m_live;

  int recycledBitmapCount() { return m_recycledBitmaps.size(); }
};

class WebGraphicsContext3DProviderForTests
    : public WebGraphicsContext3DProvider {
 public:
  WebGraphicsContext3DProviderForTests(
      std::unique_ptr<gpu::gles2::GLES2Interface> gl)
      : m_gl(std::move(gl)) {}

  gpu::gles2::GLES2Interface* contextGL() override { return m_gl.get(); }
  bool isSoftwareRendering() const override { return false; }

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

// The target to use when binding a texture to a Chromium image.
GLenum imageCHROMIUMTextureTarget() {
#if OS(MACOSX)
  return GC3D_TEXTURE_RECTANGLE_ARB;
#else
  return GL_TEXTURE_2D;
#endif
}

// The target to use when preparing a mailbox texture.
GLenum drawingBufferTextureTarget() {
  if (RuntimeEnabledFeatures::webGLImageChromiumEnabled())
    return imageCHROMIUMTextureTarget();
  return GL_TEXTURE_2D;
}

class GLES2InterfaceForTests : public gpu::gles2::GLES2InterfaceStub,
                               public DrawingBuffer::Client {
 public:
  // GLES2InterfaceStub implementation:
  void BindTexture(GLenum target, GLuint texture) override {
    if (target == GL_TEXTURE_2D)
      m_state.activeTexture2DBinding = texture;
    m_boundTextures[target] = texture;
  }

  void BindFramebuffer(GLenum target, GLuint framebuffer) override {
    switch (target) {
      case GL_FRAMEBUFFER:
        m_state.drawFramebufferBinding = framebuffer;
        m_state.readFramebufferBinding = framebuffer;
        break;
      case GL_DRAW_FRAMEBUFFER:
        m_state.drawFramebufferBinding = framebuffer;
        break;
      case GL_READ_FRAMEBUFFER:
        m_state.readFramebufferBinding = framebuffer;
        break;
      default:
        break;
    }
  }

  void BindRenderbuffer(GLenum target, GLuint renderbuffer) override {
    m_state.renderbufferBinding = renderbuffer;
  }

  void Enable(GLenum cap) {
    if (cap == GL_SCISSOR_TEST)
      m_state.scissorEnabled = true;
  }

  void Disable(GLenum cap) {
    if (cap == GL_SCISSOR_TEST)
      m_state.scissorEnabled = false;
  }

  void ClearColor(GLfloat red,
                  GLfloat green,
                  GLfloat blue,
                  GLfloat alpha) override {
    m_state.clearColor[0] = red;
    m_state.clearColor[1] = green;
    m_state.clearColor[2] = blue;
    m_state.clearColor[3] = alpha;
  }

  void ClearDepthf(GLfloat depth) override { m_state.clearDepth = depth; }

  void ClearStencil(GLint s) override { m_state.clearStencil = s; }

  void ColorMask(GLboolean red,
                 GLboolean green,
                 GLboolean blue,
                 GLboolean alpha) override {
    m_state.colorMask[0] = red;
    m_state.colorMask[1] = green;
    m_state.colorMask[2] = blue;
    m_state.colorMask[3] = alpha;
  }

  void DepthMask(GLboolean flag) override { m_state.depthMask = flag; }

  void StencilMask(GLuint mask) override { m_state.stencilMask = mask; }

  void StencilMaskSeparate(GLenum face, GLuint mask) override {
    if (face == GL_FRONT)
      m_state.stencilMask = mask;
  }

  void PixelStorei(GLenum pname, GLint param) override {
    if (pname == GL_PACK_ALIGNMENT)
      m_state.packAlignment = param;
  }

  void BindBuffer(GLenum target, GLuint buffer) override {
    if (target == GL_PIXEL_UNPACK_BUFFER)
      m_state.pixelUnpackBufferBinding = buffer;
  }

  GLuint64 InsertFenceSyncCHROMIUM() override {
    static GLuint64 syncPointGenerator = 0;
    return ++syncPointGenerator;
  }

  void WaitSyncTokenCHROMIUM(const GLbyte* syncToken) override {
    memcpy(&m_mostRecentlyWaitedSyncToken, syncToken,
           sizeof(m_mostRecentlyWaitedSyncToken));
  }

  GLenum CheckFramebufferStatus(GLenum target) override {
    return GL_FRAMEBUFFER_COMPLETE;
  }

  void GetIntegerv(GLenum pname, GLint* value) override {
    if (pname == GL_MAX_TEXTURE_SIZE)
      *value = 1024;
  }

  void GenMailboxCHROMIUM(GLbyte* mailbox) override {
    ++m_currentMailboxByte;
    memset(mailbox, m_currentMailboxByte, GL_MAILBOX_SIZE_CHROMIUM);
  }

  void ProduceTextureDirectCHROMIUM(GLuint texture,
                                    GLenum target,
                                    const GLbyte* mailbox) override {
    ASSERT_EQ(target, drawingBufferTextureTarget());

    if (!m_createImageChromiumFail) {
      ASSERT_TRUE(m_textureSizes.contains(texture));
      m_mostRecentlyProducedSize = m_textureSizes.get(texture);
    }
  }

  void TexImage2D(GLenum target,
                  GLint level,
                  GLint internalformat,
                  GLsizei width,
                  GLsizei height,
                  GLint border,
                  GLenum format,
                  GLenum type,
                  const void* pixels) override {
    if (target == GL_TEXTURE_2D && !level) {
      m_textureSizes.set(m_boundTextures[target], IntSize(width, height));
    }
  }

  GLuint CreateGpuMemoryBufferImageCHROMIUM(GLsizei width,
                                            GLsizei height,
                                            GLenum internalformat,
                                            GLenum usage) override {
    if (m_createImageChromiumFail)
      return 0;
    m_imageSizes.set(m_currentImageId, IntSize(width, height));
    return m_currentImageId++;
  }

  MOCK_METHOD1(DestroyImageMock, void(GLuint imageId));
  void DestroyImageCHROMIUM(GLuint imageId) {
    m_imageSizes.remove(imageId);
    // No textures should be bound to this.
    CHECK(m_imageToTextureMap.find(imageId) == m_imageToTextureMap.end());
    m_imageSizes.remove(imageId);
    DestroyImageMock(imageId);
  }

  MOCK_METHOD1(BindTexImage2DMock, void(GLint imageId));
  void BindTexImage2DCHROMIUM(GLenum target, GLint imageId) {
    if (target == imageCHROMIUMTextureTarget()) {
      m_textureSizes.set(m_boundTextures[target],
                         m_imageSizes.find(imageId)->value);
      m_imageToTextureMap.set(imageId, m_boundTextures[target]);
      BindTexImage2DMock(imageId);
    }
  }

  MOCK_METHOD1(ReleaseTexImage2DMock, void(GLint imageId));
  void ReleaseTexImage2DCHROMIUM(GLenum target, GLint imageId) {
    if (target == imageCHROMIUMTextureTarget()) {
      m_imageSizes.set(m_currentImageId, IntSize());
      m_imageToTextureMap.remove(imageId);
      ReleaseTexImage2DMock(imageId);
    }
  }

  void GenSyncTokenCHROMIUM(GLuint64 fenceSync, GLbyte* syncToken) override {
    static uint64_t uniqueId = 1;
    gpu::SyncToken source(gpu::GPU_IO, 1,
                          gpu::CommandBufferId::FromUnsafeValue(uniqueId++), 2);
    memcpy(syncToken, &source, sizeof(source));
  }

  void GenTextures(GLsizei n, GLuint* textures) override {
    static GLuint id = 1;
    for (GLsizei i = 0; i < n; ++i)
      textures[i] = id++;
  }

  // DrawingBuffer::Client implementation.
  bool DrawingBufferClientIsBoundForDraw() override {
    return !m_state.drawFramebufferBinding;
  }
  void DrawingBufferClientRestoreScissorTest() override {
    m_state.scissorEnabled = m_savedState.scissorEnabled;
  }
  void DrawingBufferClientRestoreMaskAndClearValues() override {
    memcpy(m_state.colorMask, m_savedState.colorMask,
           sizeof(m_state.colorMask));
    m_state.clearDepth = m_savedState.clearDepth;
    m_state.clearStencil = m_savedState.clearStencil;

    memcpy(m_state.clearColor, m_savedState.clearColor,
           sizeof(m_state.clearColor));
    m_state.depthMask = m_savedState.depthMask;
    m_state.stencilMask = m_savedState.stencilMask;
  }
  void DrawingBufferClientRestorePixelPackAlignment() override {
    m_state.packAlignment = m_savedState.packAlignment;
  }
  void DrawingBufferClientRestoreTexture2DBinding() override {
    m_state.activeTexture2DBinding = m_savedState.activeTexture2DBinding;
  }
  void DrawingBufferClientRestoreRenderbufferBinding() override {
    m_state.renderbufferBinding = m_savedState.renderbufferBinding;
  }
  void DrawingBufferClientRestoreFramebufferBinding() override {
    m_state.drawFramebufferBinding = m_savedState.drawFramebufferBinding;
    m_state.readFramebufferBinding = m_savedState.readFramebufferBinding;
  }
  void DrawingBufferClientRestorePixelUnpackBufferBinding() override {
    m_state.pixelUnpackBufferBinding = m_savedState.pixelUnpackBufferBinding;
  }

  // Testing methods.
  gpu::SyncToken mostRecentlyWaitedSyncToken() const {
    return m_mostRecentlyWaitedSyncToken;
  }
  GLuint nextImageIdToBeCreated() const { return m_currentImageId; }
  IntSize mostRecentlyProducedSize() const {
    return m_mostRecentlyProducedSize;
  }

  void setCreateImageChromiumFail(bool fail) {
    m_createImageChromiumFail = fail;
  }

  // Saves current GL state for later verification.
  void SaveState() { m_savedState = m_state; }
  void VerifyStateHasNotChangedSinceSave() const {
    for (size_t i = 0; i < 4; ++i) {
      EXPECT_EQ(m_state.clearColor[0], m_savedState.clearColor[0]);
      EXPECT_EQ(m_state.colorMask[0], m_savedState.colorMask[0]);
    }
    EXPECT_EQ(m_state.clearDepth, m_savedState.clearDepth);
    EXPECT_EQ(m_state.clearStencil, m_savedState.clearStencil);
    EXPECT_EQ(m_state.depthMask, m_savedState.depthMask);
    EXPECT_EQ(m_state.stencilMask, m_savedState.stencilMask);
    EXPECT_EQ(m_state.packAlignment, m_savedState.packAlignment);
    EXPECT_EQ(m_state.activeTexture2DBinding,
              m_savedState.activeTexture2DBinding);
    EXPECT_EQ(m_state.renderbufferBinding, m_savedState.renderbufferBinding);
    EXPECT_EQ(m_state.drawFramebufferBinding,
              m_savedState.drawFramebufferBinding);
    EXPECT_EQ(m_state.readFramebufferBinding,
              m_savedState.readFramebufferBinding);
    EXPECT_EQ(m_state.pixelUnpackBufferBinding,
              m_savedState.pixelUnpackBufferBinding);
  }

 private:
  std::map<GLenum, GLuint> m_boundTextures;

  // State tracked to verify that it is restored correctly.
  struct State {
    bool scissorEnabled = false;

    GLfloat clearColor[4] = {0, 0, 0, 0};
    GLfloat clearDepth = 0;
    GLint clearStencil = 0;

    GLboolean colorMask[4] = {0, 0, 0, 0};
    GLboolean depthMask = 0;
    GLuint stencilMask = 0;

    GLint packAlignment = 4;

    // The bound 2D texture for the active texture unit.
    GLuint activeTexture2DBinding = 0;
    GLuint renderbufferBinding = 0;
    GLuint drawFramebufferBinding = 0;
    GLuint readFramebufferBinding = 0;
    GLuint pixelUnpackBufferBinding = 0;
  };
  State m_state;
  State m_savedState;

  gpu::SyncToken m_mostRecentlyWaitedSyncToken;
  GLbyte m_currentMailboxByte = 0;
  IntSize m_mostRecentlyProducedSize;
  bool m_createImageChromiumFail = false;
  GLuint m_currentImageId = 1;
  HashMap<GLuint, IntSize> m_textureSizes;
  HashMap<GLuint, IntSize> m_imageSizes;
  HashMap<GLuint, GLuint> m_imageToTextureMap;
};

}  // blink
