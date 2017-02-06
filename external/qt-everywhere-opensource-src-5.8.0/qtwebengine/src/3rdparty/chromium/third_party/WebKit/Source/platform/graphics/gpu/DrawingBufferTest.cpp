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

#include "gpu/command_buffer/client/gles2_interface_stub.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/UnacceleratedImageBufferSurface.h"
#include "platform/graphics/gpu/Extensions3DUtil.h"
#include "public/platform/Platform.h"
#include "public/platform/WebExternalTextureMailbox.h"
#include "public/platform/WebGraphicsContext3DProvider.h"
#include "public/platform/functional/WebFunction.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/PtrUtil.h"
#include "wtf/RefPtr.h"
#include <memory>

using testing::Test;
using testing::_;

namespace blink {

namespace {

// The target to use when binding a texture to a Chromium image.
GLenum imageCHROMIUMTextureTarget()
{
#if OS(MACOSX)
    return GC3D_TEXTURE_RECTANGLE_ARB;
#else
    return GL_TEXTURE_2D;
#endif
}

// The target to use when preparing a mailbox texture.
GLenum drawingBufferTextureTarget()
{
    if (RuntimeEnabledFeatures::webGLImageChromiumEnabled())
        return imageCHROMIUMTextureTarget();
    return GL_TEXTURE_2D;
}

} // namespace

class GLES2InterfaceForTests : public gpu::gles2::GLES2InterfaceStub {
public:
    void BindTexture(GLenum target, GLuint texture) override
    {
        if (target != m_boundTextureTarget && texture == 0)
            return;

        // For simplicity, only allow one target to ever be bound.
        ASSERT_TRUE(m_boundTextureTarget == 0 || target == m_boundTextureTarget);
        m_boundTextureTarget = target;
        m_boundTexture = texture;
    }

    GLuint64 InsertFenceSyncCHROMIUM() override
    {
        static GLuint64 syncPointGenerator = 0;
        return ++syncPointGenerator;
    }

    void WaitSyncTokenCHROMIUM(const GLbyte* syncToken) override
    {
        memcpy(&m_mostRecentlyWaitedSyncToken, syncToken, sizeof(m_mostRecentlyWaitedSyncToken));
    }

    GLenum CheckFramebufferStatus(GLenum target) override
    {
        return GL_FRAMEBUFFER_COMPLETE;
    }

    void GetIntegerv(GLenum pname, GLint* value) override
    {
        if (pname == GL_MAX_TEXTURE_SIZE)
            *value = 1024;
    }

    void GetImageivCHROMIUM(GLuint imageId, GLenum pname, GLint* data) override
    {
        if (pname == GC3D_GPU_MEMORY_BUFFER_ID)
            *data = 1;
        else
            *data = -1;
    }

    void GenMailboxCHROMIUM(GLbyte* mailbox) override
    {
        ++m_currentMailboxByte;
        WebExternalTextureMailbox temp;
        memset(mailbox, m_currentMailboxByte, sizeof(temp.name));
    }

    void ProduceTextureDirectCHROMIUM(GLuint texture, GLenum target, const GLbyte* mailbox) override
    {
        ASSERT_EQ(target, drawingBufferTextureTarget());

        if (!m_createImageChromiumFail) {
            ASSERT_TRUE(m_textureSizes.contains(texture));
            m_mostRecentlyProducedSize = m_textureSizes.get(texture);
        }
    }

    void TexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels) override
    {
        if (target == GL_TEXTURE_2D && !level) {
            m_textureSizes.set(m_boundTexture, IntSize(width, height));
        }
    }

    GLuint CreateGpuMemoryBufferImageCHROMIUM(GLsizei width, GLsizei height, GLenum internalformat, GLenum usage) override
    {
        if (m_createImageChromiumFail)
            return 0;
        m_imageSizes.set(m_currentImageId, IntSize(width, height));
        return m_currentImageId++;
    }

    MOCK_METHOD1(DestroyImageMock, void(GLuint imageId));
    void DestroyImageCHROMIUM(GLuint imageId)
    {
        m_imageSizes.remove(imageId);
        // No textures should be bound to this.
        ASSERT(m_imageToTextureMap.find(imageId) == m_imageToTextureMap.end());
        m_imageSizes.remove(imageId);
        DestroyImageMock(imageId);
    }

    MOCK_METHOD1(BindTexImage2DMock, void(GLint imageId));
    void BindTexImage2DCHROMIUM(GLenum target, GLint imageId)
    {
        if (target == imageCHROMIUMTextureTarget()) {
            m_textureSizes.set(m_boundTexture, m_imageSizes.find(imageId)->value);
            m_imageToTextureMap.set(imageId, m_boundTexture);
            BindTexImage2DMock(imageId);
        }
    }

    MOCK_METHOD1(ReleaseTexImage2DMock, void(GLint imageId));
    void ReleaseTexImage2DCHROMIUM(GLenum target, GLint imageId)
    {
        if (target == imageCHROMIUMTextureTarget()) {
            m_imageSizes.set(m_currentImageId, IntSize());
            m_imageToTextureMap.remove(imageId);
            ReleaseTexImage2DMock(imageId);
        }
    }

    void GenSyncTokenCHROMIUM(GLuint64 fenceSync, GLbyte* syncToken) override
    {
        memcpy(syncToken, &fenceSync, sizeof(fenceSync));
    }

    void GenTextures(GLsizei n, GLuint* textures) override
    {
        static GLuint id = 1;
        for (GLsizei i = 0; i < n; ++i)
            textures[i] = id++;
    }

    GLuint boundTexture() const { return m_boundTexture; }
    GLuint boundTextureTarget() const { return m_boundTextureTarget; }
    GLuint mostRecentlyWaitedSyncToken() const { return m_mostRecentlyWaitedSyncToken; }
    GLuint nextImageIdToBeCreated() const { return m_currentImageId; }
    IntSize mostRecentlyProducedSize() const { return m_mostRecentlyProducedSize; }

    void setCreateImageChromiumFail(bool fail) { m_createImageChromiumFail = fail; }

private:
    GLuint m_boundTexture = 0;
    GLuint m_boundTextureTarget = 0;
    GLuint m_mostRecentlyWaitedSyncToken = 0;
    GLbyte m_currentMailboxByte = 0;
    IntSize m_mostRecentlyProducedSize;
    bool m_createImageChromiumFail = false;
    GLuint m_currentImageId = 1;
    HashMap<GLuint, IntSize> m_textureSizes;
    HashMap<GLuint, IntSize> m_imageSizes;
    HashMap<GLuint, GLuint> m_imageToTextureMap;
};

static const int initialWidth = 100;
static const int initialHeight = 100;
static const int alternateHeight = 50;

class DrawingBufferForTests : public DrawingBuffer {
public:
    static PassRefPtr<DrawingBufferForTests> create(std::unique_ptr<WebGraphicsContext3DProvider> contextProvider, const IntSize& size, PreserveDrawingBuffer preserve)
    {
        std::unique_ptr<Extensions3DUtil> extensionsUtil = Extensions3DUtil::create(contextProvider->contextGL());
        RefPtr<DrawingBufferForTests> drawingBuffer = adoptRef(new DrawingBufferForTests(std::move(contextProvider), std::move(extensionsUtil), preserve));
        bool multisampleExtensionSupported = false;
        if (!drawingBuffer->initialize(size, multisampleExtensionSupported)) {
            drawingBuffer->beginDestruction();
            return nullptr;
        }
        return drawingBuffer.release();
    }

    DrawingBufferForTests(std::unique_ptr<WebGraphicsContext3DProvider> contextProvider, std::unique_ptr<Extensions3DUtil> extensionsUtil, PreserveDrawingBuffer preserve)
        : DrawingBuffer(std::move(contextProvider), std::move(extensionsUtil), false /* discardFramebufferSupported */, true /* wantAlphaChannel */, false /* premultipliedAlpha */, preserve, false /* wantDepth */, false /* wantStencil */)
        , m_live(0)
    { }

    ~DrawingBufferForTests() override
    {
        if (m_live)
            *m_live = false;
    }

    bool* m_live;
};

class WebGraphicsContext3DProviderForTests : public WebGraphicsContext3DProvider {
public:
    WebGraphicsContext3DProviderForTests(std::unique_ptr<gpu::gles2::GLES2Interface> gl)
        : m_gl(std::move(gl))
    {
    }

    gpu::gles2::GLES2Interface* contextGL() override { return m_gl.get(); }
    // Not used by WebGL code.
    GrContext* grContext() override { return nullptr; }
    bool bindToCurrentThread() override { return false; }
    gpu::Capabilities getCapabilities()
    {
        return gpu::Capabilities();
    }
    void setLostContextCallback(WebClosure) {}
    void setErrorMessageCallback(WebFunction<void(const char*, int32_t id)>) {}

private:
    std::unique_ptr<gpu::gles2::GLES2Interface> m_gl;
};

class DrawingBufferTest : public Test {
protected:
    void SetUp() override
    {
        std::unique_ptr<GLES2InterfaceForTests> gl = wrapUnique(new GLES2InterfaceForTests);
        m_gl = gl.get();
        std::unique_ptr<WebGraphicsContext3DProviderForTests> provider = wrapUnique(new WebGraphicsContext3DProviderForTests(std::move(gl)));
        m_drawingBuffer = DrawingBufferForTests::create(std::move(provider), IntSize(initialWidth, initialHeight), DrawingBuffer::Preserve);
        CHECK(m_drawingBuffer);
    }

    GLES2InterfaceForTests* m_gl;
    RefPtr<DrawingBufferForTests> m_drawingBuffer;
};

TEST_F(DrawingBufferTest, verifyResizingProperlyAffectsMailboxes)
{
    WebExternalTextureMailbox mailbox;

    IntSize initialSize(initialWidth, initialHeight);
    IntSize alternateSize(initialWidth, alternateHeight);

    // Produce one mailbox at size 100x100.
    m_drawingBuffer->markContentsChanged();
    EXPECT_TRUE(m_drawingBuffer->prepareMailbox(&mailbox, 0));
    EXPECT_EQ(initialSize, m_gl->mostRecentlyProducedSize());

    // Resize to 100x50.
    m_drawingBuffer->reset(IntSize(initialWidth, alternateHeight));
    m_drawingBuffer->mailboxReleased(mailbox, false);

    // Produce a mailbox at this size.
    m_drawingBuffer->markContentsChanged();
    EXPECT_TRUE(m_drawingBuffer->prepareMailbox(&mailbox, 0));
    EXPECT_EQ(alternateSize, m_gl->mostRecentlyProducedSize());

    // Reset to initial size.
    m_drawingBuffer->reset(IntSize(initialWidth, initialHeight));
    m_drawingBuffer->mailboxReleased(mailbox, false);

    // Prepare another mailbox and verify that it's the correct size.
    m_drawingBuffer->markContentsChanged();
    EXPECT_TRUE(m_drawingBuffer->prepareMailbox(&mailbox, 0));
    EXPECT_EQ(initialSize, m_gl->mostRecentlyProducedSize());

    // Prepare one final mailbox and verify that it's the correct size.
    m_drawingBuffer->mailboxReleased(mailbox, false);
    m_drawingBuffer->markContentsChanged();
    EXPECT_TRUE(m_drawingBuffer->prepareMailbox(&mailbox, 0));
    EXPECT_EQ(initialSize, m_gl->mostRecentlyProducedSize());
    m_drawingBuffer->mailboxReleased(mailbox, false);
    m_drawingBuffer->beginDestruction();
}

TEST_F(DrawingBufferTest, verifyDestructionCompleteAfterAllMailboxesReleased)
{
    bool live = true;
    m_drawingBuffer->m_live = &live;

    WebExternalTextureMailbox mailbox1;
    WebExternalTextureMailbox mailbox2;
    WebExternalTextureMailbox mailbox3;

    IntSize initialSize(initialWidth, initialHeight);

    // Produce mailboxes.
    m_drawingBuffer->markContentsChanged();
    EXPECT_TRUE(m_drawingBuffer->prepareMailbox(&mailbox1, 0));
    m_drawingBuffer->markContentsChanged();
    EXPECT_TRUE(m_drawingBuffer->prepareMailbox(&mailbox2, 0));
    m_drawingBuffer->markContentsChanged();
    EXPECT_TRUE(m_drawingBuffer->prepareMailbox(&mailbox3, 0));

    m_drawingBuffer->markContentsChanged();
    m_drawingBuffer->mailboxReleased(mailbox1, false);

    m_drawingBuffer->beginDestruction();
    EXPECT_EQ(live, true);

    DrawingBufferForTests* weakPointer = m_drawingBuffer.get();
    m_drawingBuffer.clear();
    EXPECT_EQ(live, true);

    weakPointer->markContentsChanged();
    weakPointer->mailboxReleased(mailbox2, false);
    EXPECT_EQ(live, true);

    weakPointer->markContentsChanged();
    weakPointer->mailboxReleased(mailbox3, false);
    EXPECT_EQ(live, false);
}

TEST_F(DrawingBufferTest, verifyDrawingBufferStaysAliveIfResourcesAreLost)
{
    bool live = true;
    m_drawingBuffer->m_live = &live;
    WebExternalTextureMailbox mailbox1;
    WebExternalTextureMailbox mailbox2;
    WebExternalTextureMailbox mailbox3;

    m_drawingBuffer->markContentsChanged();
    EXPECT_TRUE(m_drawingBuffer->prepareMailbox(&mailbox1, 0));
    m_drawingBuffer->markContentsChanged();
    EXPECT_TRUE(m_drawingBuffer->prepareMailbox(&mailbox2, 0));
    m_drawingBuffer->markContentsChanged();
    EXPECT_TRUE(m_drawingBuffer->prepareMailbox(&mailbox3, 0));

    m_drawingBuffer->markContentsChanged();
    m_drawingBuffer->mailboxReleased(mailbox1, true);
    EXPECT_EQ(live, true);

    m_drawingBuffer->beginDestruction();
    EXPECT_EQ(live, true);

    m_drawingBuffer->markContentsChanged();
    m_drawingBuffer->mailboxReleased(mailbox2, false);
    EXPECT_EQ(live, true);

    DrawingBufferForTests* weakPtr = m_drawingBuffer.get();
    m_drawingBuffer.clear();
    EXPECT_EQ(live, true);

    weakPtr->markContentsChanged();
    weakPtr->mailboxReleased(mailbox3, true);
    EXPECT_EQ(live, false);
}

class TextureMailboxWrapper {
public:
    explicit TextureMailboxWrapper(const WebExternalTextureMailbox& mailbox)
        : m_mailbox(mailbox)
    { }

    bool operator==(const TextureMailboxWrapper& other) const
    {
        return !memcmp(m_mailbox.name, other.m_mailbox.name, sizeof(m_mailbox.name));
    }

    bool operator!=(const TextureMailboxWrapper& other) const
    {
        return !(*this == other);
    }

private:
    WebExternalTextureMailbox m_mailbox;
};

TEST_F(DrawingBufferTest, verifyOnlyOneRecycledMailboxMustBeKept)
{
    WebExternalTextureMailbox mailbox1;
    WebExternalTextureMailbox mailbox2;
    WebExternalTextureMailbox mailbox3;

    // Produce mailboxes.
    m_drawingBuffer->markContentsChanged();
    EXPECT_TRUE(m_drawingBuffer->prepareMailbox(&mailbox1, 0));
    m_drawingBuffer->markContentsChanged();
    EXPECT_TRUE(m_drawingBuffer->prepareMailbox(&mailbox2, 0));
    m_drawingBuffer->markContentsChanged();
    EXPECT_TRUE(m_drawingBuffer->prepareMailbox(&mailbox3, 0));

    // Release mailboxes by specific order; 1, 3, 2.
    m_drawingBuffer->markContentsChanged();
    m_drawingBuffer->mailboxReleased(mailbox1, false);
    m_drawingBuffer->markContentsChanged();
    m_drawingBuffer->mailboxReleased(mailbox3, false);
    m_drawingBuffer->markContentsChanged();
    m_drawingBuffer->mailboxReleased(mailbox2, false);

    // The first recycled mailbox must be 2. 1 and 3 were deleted by FIFO order because
    // DrawingBuffer never keeps more than one mailbox.
    WebExternalTextureMailbox recycledMailbox1;
    m_drawingBuffer->markContentsChanged();
    EXPECT_TRUE(m_drawingBuffer->prepareMailbox(&recycledMailbox1, 0));
    EXPECT_EQ(TextureMailboxWrapper(mailbox2), TextureMailboxWrapper(recycledMailbox1));

    // The second recycled mailbox must be a new mailbox.
    WebExternalTextureMailbox recycledMailbox2;
    m_drawingBuffer->markContentsChanged();
    EXPECT_TRUE(m_drawingBuffer->prepareMailbox(&recycledMailbox2, 0));
    EXPECT_NE(TextureMailboxWrapper(mailbox1), TextureMailboxWrapper(recycledMailbox2));
    EXPECT_NE(TextureMailboxWrapper(mailbox2), TextureMailboxWrapper(recycledMailbox2));
    EXPECT_NE(TextureMailboxWrapper(mailbox3), TextureMailboxWrapper(recycledMailbox2));

    m_drawingBuffer->mailboxReleased(recycledMailbox1, false);
    m_drawingBuffer->mailboxReleased(recycledMailbox2, false);
    m_drawingBuffer->beginDestruction();
}

TEST_F(DrawingBufferTest, verifyInsertAndWaitSyncTokenCorrectly)
{
    WebExternalTextureMailbox mailbox;

    // Produce mailboxes.
    m_drawingBuffer->markContentsChanged();
    EXPECT_EQ(0u, m_gl->mostRecentlyWaitedSyncToken());
    EXPECT_TRUE(m_drawingBuffer->prepareMailbox(&mailbox, 0));
    // prepareMailbox() does not wait for any sync point.
    EXPECT_EQ(0u, m_gl->mostRecentlyWaitedSyncToken());

    GLuint64 waitSyncToken = 0;
    m_gl->GenSyncTokenCHROMIUM(m_gl->InsertFenceSyncCHROMIUM(), reinterpret_cast<GLbyte*>(&waitSyncToken));
    memcpy(mailbox.syncToken, &waitSyncToken, sizeof(waitSyncToken));
    mailbox.validSyncToken = true;
    m_drawingBuffer->mailboxReleased(mailbox, false);
    // m_drawingBuffer will wait for the sync point when recycling.
    EXPECT_EQ(0u, m_gl->mostRecentlyWaitedSyncToken());

    m_drawingBuffer->markContentsChanged();
    EXPECT_TRUE(m_drawingBuffer->prepareMailbox(&mailbox, 0));
    // m_drawingBuffer waits for the sync point when recycling in prepareMailbox().
    EXPECT_EQ(waitSyncToken, m_gl->mostRecentlyWaitedSyncToken());

    m_drawingBuffer->beginDestruction();
    m_gl->GenSyncTokenCHROMIUM(m_gl->InsertFenceSyncCHROMIUM(), reinterpret_cast<GLbyte*>(&waitSyncToken));
    memcpy(mailbox.syncToken, &waitSyncToken, sizeof(waitSyncToken));
    mailbox.validSyncToken = true;
    m_drawingBuffer->mailboxReleased(mailbox, false);
    // m_drawingBuffer waits for the sync point because the destruction is in progress.
    EXPECT_EQ(waitSyncToken, m_gl->mostRecentlyWaitedSyncToken());
}

class DrawingBufferImageChromiumTest : public DrawingBufferTest {
protected:
    void SetUp() override
    {
        std::unique_ptr<GLES2InterfaceForTests> gl = wrapUnique(new GLES2InterfaceForTests);
        m_gl = gl.get();
        std::unique_ptr<WebGraphicsContext3DProviderForTests> provider = wrapUnique(new WebGraphicsContext3DProviderForTests(std::move(gl)));
        RuntimeEnabledFeatures::setWebGLImageChromiumEnabled(true);
        m_imageId0 = m_gl->nextImageIdToBeCreated();
        EXPECT_CALL(*m_gl, BindTexImage2DMock(m_imageId0)).Times(1);
        m_drawingBuffer = DrawingBufferForTests::create(std::move(provider),
            IntSize(initialWidth, initialHeight), DrawingBuffer::Preserve);
        CHECK(m_drawingBuffer);
        testing::Mock::VerifyAndClearExpectations(m_gl);
    }

    void TearDown() override
    {
        RuntimeEnabledFeatures::setWebGLImageChromiumEnabled(false);
    }

    GLuint m_imageId0;
};

TEST_F(DrawingBufferImageChromiumTest, verifyResizingReallocatesImages)
{
    WebExternalTextureMailbox mailbox;

    IntSize initialSize(initialWidth, initialHeight);
    IntSize alternateSize(initialWidth, alternateHeight);

    GLuint m_imageId1 = m_gl->nextImageIdToBeCreated();
    EXPECT_CALL(*m_gl, BindTexImage2DMock(m_imageId1)).Times(1);
    // Produce one mailbox at size 100x100.
    m_drawingBuffer->markContentsChanged();
    EXPECT_TRUE(m_drawingBuffer->prepareMailbox(&mailbox, 0));
    EXPECT_EQ(initialSize, m_gl->mostRecentlyProducedSize());
    EXPECT_TRUE(mailbox.allowOverlay);
    testing::Mock::VerifyAndClearExpectations(m_gl);

    GLuint m_imageId2 = m_gl->nextImageIdToBeCreated();
    EXPECT_CALL(*m_gl, BindTexImage2DMock(m_imageId2)).Times(1);
    EXPECT_CALL(*m_gl, DestroyImageMock(m_imageId0)).Times(1);
    EXPECT_CALL(*m_gl, ReleaseTexImage2DMock(m_imageId0)).Times(1);
    // Resize to 100x50.
    m_drawingBuffer->reset(IntSize(initialWidth, alternateHeight));
    m_drawingBuffer->mailboxReleased(mailbox, false);
    testing::Mock::VerifyAndClearExpectations(m_gl);

    GLuint m_imageId3 = m_gl->nextImageIdToBeCreated();
    EXPECT_CALL(*m_gl, BindTexImage2DMock(m_imageId3)).Times(1);
    EXPECT_CALL(*m_gl, DestroyImageMock(m_imageId1)).Times(1);
    EXPECT_CALL(*m_gl, ReleaseTexImage2DMock(m_imageId1)).Times(1);
    // Produce a mailbox at this size.
    m_drawingBuffer->markContentsChanged();
    EXPECT_TRUE(m_drawingBuffer->prepareMailbox(&mailbox, 0));
    EXPECT_EQ(alternateSize, m_gl->mostRecentlyProducedSize());
    EXPECT_TRUE(mailbox.allowOverlay);
    testing::Mock::VerifyAndClearExpectations(m_gl);

    GLuint m_imageId4 = m_gl->nextImageIdToBeCreated();
    EXPECT_CALL(*m_gl, BindTexImage2DMock(m_imageId4)).Times(1);
    EXPECT_CALL(*m_gl, DestroyImageMock(m_imageId2)).Times(1);
    EXPECT_CALL(*m_gl, ReleaseTexImage2DMock(m_imageId2)).Times(1);
    // Reset to initial size.
    m_drawingBuffer->reset(IntSize(initialWidth, initialHeight));
    m_drawingBuffer->mailboxReleased(mailbox, false);
    testing::Mock::VerifyAndClearExpectations(m_gl);

    GLuint m_imageId5 = m_gl->nextImageIdToBeCreated();
    EXPECT_CALL(*m_gl, BindTexImage2DMock(m_imageId5)).Times(1);
    EXPECT_CALL(*m_gl, DestroyImageMock(m_imageId3)).Times(1);
    EXPECT_CALL(*m_gl, ReleaseTexImage2DMock(m_imageId3)).Times(1);
    // Prepare another mailbox and verify that it's the correct size.
    m_drawingBuffer->markContentsChanged();
    EXPECT_TRUE(m_drawingBuffer->prepareMailbox(&mailbox, 0));
    EXPECT_EQ(initialSize, m_gl->mostRecentlyProducedSize());
    EXPECT_TRUE(mailbox.allowOverlay);
    testing::Mock::VerifyAndClearExpectations(m_gl);

    // Prepare one final mailbox and verify that it's the correct size.
    m_drawingBuffer->mailboxReleased(mailbox, false);
    m_drawingBuffer->markContentsChanged();
    EXPECT_TRUE(m_drawingBuffer->prepareMailbox(&mailbox, 0));
    EXPECT_EQ(initialSize, m_gl->mostRecentlyProducedSize());
    EXPECT_TRUE(mailbox.allowOverlay);
    m_drawingBuffer->mailboxReleased(mailbox, false);

    EXPECT_CALL(*m_gl, DestroyImageMock(m_imageId5)).Times(1);
    EXPECT_CALL(*m_gl, ReleaseTexImage2DMock(m_imageId5)).Times(1);
    EXPECT_CALL(*m_gl, DestroyImageMock(m_imageId4)).Times(1);
    EXPECT_CALL(*m_gl, ReleaseTexImage2DMock(m_imageId4)).Times(1);
    m_drawingBuffer->beginDestruction();
    testing::Mock::VerifyAndClearExpectations(m_gl);
}

TEST_F(DrawingBufferImageChromiumTest, allocationFailure)
{
    WebExternalTextureMailbox mailboxes[3];

    // Request a mailbox. An image should already be created. Everything works
    // as expected.
    EXPECT_CALL(*m_gl, BindTexImage2DMock(_)).Times(1);
    IntSize initialSize(initialWidth, initialHeight);
    m_drawingBuffer->markContentsChanged();
    EXPECT_TRUE(m_drawingBuffer->prepareMailbox(&mailboxes[0], 0));
    EXPECT_TRUE(mailboxes[0].allowOverlay);
    testing::Mock::VerifyAndClearExpectations(m_gl);

    // Force image CHROMIUM creation failure. Request another mailbox. It should
    // still be provided, but this time with allowOverlay = false.
    m_gl->setCreateImageChromiumFail(true);
    m_drawingBuffer->markContentsChanged();
    EXPECT_TRUE(m_drawingBuffer->prepareMailbox(&mailboxes[1], 0));
    EXPECT_FALSE(mailboxes[1].allowOverlay);

    // Check that if image CHROMIUM starts working again, mailboxes are
    // correctly created with allowOverlay = true.
    EXPECT_CALL(*m_gl, BindTexImage2DMock(_)).Times(1);
    m_gl->setCreateImageChromiumFail(false);
    m_drawingBuffer->markContentsChanged();
    EXPECT_TRUE(m_drawingBuffer->prepareMailbox(&mailboxes[2], 0));
    EXPECT_TRUE(mailboxes[2].allowOverlay);
    testing::Mock::VerifyAndClearExpectations(m_gl);

    for (int i = 0; i < 3; ++i)
        m_drawingBuffer->mailboxReleased(mailboxes[i], false);

    EXPECT_CALL(*m_gl, DestroyImageMock(_)).Times(3);
    EXPECT_CALL(*m_gl, ReleaseTexImage2DMock(_)).Times(3);
    m_drawingBuffer->beginDestruction();
    testing::Mock::VerifyAndClearExpectations(m_gl);
}

class DepthStencilTrackingGLES2Interface : public gpu::gles2::GLES2InterfaceStub {
public:
    void FramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer) override
    {
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

    GLenum CheckFramebufferStatus(GLenum target) override
    {
        return GL_FRAMEBUFFER_COMPLETE;
    }

    void GetIntegerv(GLenum ptype, GLint* value) override
    {
        switch (ptype) {
        case GL_MAX_TEXTURE_SIZE:
            *value = 1024;
            return;
        }
    }

    const GLubyte* GetString(GLenum type) override
    {
        if (type == GL_EXTENSIONS)
            return reinterpret_cast<const GLubyte*>("GL_OES_packed_depth_stencil");
        return reinterpret_cast<const GLubyte*>("");
    }

    void GenRenderbuffers(GLsizei n, GLuint* renderbuffers) override
    {
        for (GLsizei i = 0; i < n; ++i)
            renderbuffers[i] = m_nextGenRenderbufferId++;
    }

    GLuint stencilAttachment() const { return m_stencilAttachment; }
    GLuint depthAttachment() const { return m_depthAttachment; }
    GLuint depthStencilAttachment() const { return m_depthStencilAttachment; }
    size_t numAllocatedRenderBuffer() const { return m_nextGenRenderbufferId - 1; }

private:
    GLuint m_nextGenRenderbufferId = 1;
    GLuint m_depthAttachment = 0;
    GLuint m_stencilAttachment = 0;
    GLuint m_depthStencilAttachment = 0;
};

struct DepthStencilTestCase {
    DepthStencilTestCase(bool requestStencil, bool requestDepth, int expectedRenderBuffers, const char* const testCaseName)
        : requestStencil(requestStencil)
        , requestDepth(requestDepth)
        , expectedRenderBuffers(expectedRenderBuffers)
        , testCaseName(testCaseName) { }

    bool requestStencil;
    bool requestDepth;
    size_t expectedRenderBuffers;
    const char* const testCaseName;
};

// This tests that when the packed depth+stencil extension is supported DrawingBuffer always allocates
// a single packed renderbuffer if either is requested and properly computes the actual context attributes
// as defined by WebGL. We always allocate a packed buffer in this case since many desktop OpenGL drivers
// that support this extension do not consider a framebuffer with only a depth or a stencil buffer attached
// to be complete.
TEST(DrawingBufferDepthStencilTest, packedDepthStencilSupported)
{
    DepthStencilTestCase cases[] = {
        DepthStencilTestCase(false, false, 0, "neither"),
        DepthStencilTestCase(true, false, 1, "stencil only"),
        DepthStencilTestCase(false, true, 1, "depth only"),
        DepthStencilTestCase(true, true, 1, "both"),
    };

    for (size_t i = 0; i < WTF_ARRAY_LENGTH(cases); i++) {
        SCOPED_TRACE(cases[i].testCaseName);
        std::unique_ptr<DepthStencilTrackingGLES2Interface> gl = wrapUnique(new DepthStencilTrackingGLES2Interface);
        DepthStencilTrackingGLES2Interface* trackingGL = gl.get();
        std::unique_ptr<WebGraphicsContext3DProviderForTests> provider = wrapUnique(new WebGraphicsContext3DProviderForTests(std::move(gl)));
        DrawingBuffer::PreserveDrawingBuffer preserve = DrawingBuffer::Preserve;

        bool premultipliedAlpha = false;
        bool wantAlphaChannel = true;
        bool wantDepthBuffer = cases[i].requestDepth;
        bool wantStencilBuffer = cases[i].requestStencil;
        bool wantAntialiasing = false;
        RefPtr<DrawingBuffer> drawingBuffer = DrawingBuffer::create(
            std::move(provider),
            IntSize(10, 10),
            premultipliedAlpha,
            wantAlphaChannel,
            wantDepthBuffer,
            wantStencilBuffer,
            wantAntialiasing,
            preserve);

        // When we request a depth or a stencil buffer, we will get both.
        EXPECT_EQ(cases[i].requestDepth || cases[i].requestStencil, drawingBuffer->hasDepthBuffer());
        EXPECT_EQ(cases[i].requestDepth || cases[i].requestStencil, drawingBuffer->hasStencilBuffer());
        EXPECT_EQ(cases[i].expectedRenderBuffers, trackingGL->numAllocatedRenderBuffer());
        if (cases[i].requestDepth || cases[i].requestStencil) {
            EXPECT_NE(0u, trackingGL->depthStencilAttachment());
            EXPECT_EQ(0u, trackingGL->depthAttachment());
            EXPECT_EQ(0u, trackingGL->stencilAttachment());
        } else {
            EXPECT_EQ(0u, trackingGL->depthStencilAttachment());
            EXPECT_EQ(0u, trackingGL->depthAttachment());
            EXPECT_EQ(0u, trackingGL->stencilAttachment());
        }

        drawingBuffer->reset(IntSize(10, 20));
        EXPECT_EQ(cases[i].requestDepth || cases[i].requestStencil, drawingBuffer->hasDepthBuffer());
        EXPECT_EQ(cases[i].requestDepth || cases[i].requestStencil, drawingBuffer->hasStencilBuffer());
        EXPECT_EQ(cases[i].expectedRenderBuffers, trackingGL->numAllocatedRenderBuffer());
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

TEST_F(DrawingBufferTest, verifySetIsHiddenProperlyAffectsMailboxes)
{
    blink::WebExternalTextureMailbox mailbox;

    // Produce mailboxes.
    m_drawingBuffer->markContentsChanged();
    EXPECT_TRUE(m_drawingBuffer->prepareMailbox(&mailbox, 0));

    m_gl->GenSyncTokenCHROMIUM(m_gl->InsertFenceSyncCHROMIUM(), mailbox.syncToken);
    mailbox.validSyncToken = true;
    m_drawingBuffer->setIsHidden(true);
    m_drawingBuffer->mailboxReleased(mailbox);
    // m_drawingBuffer deletes mailbox immediately when hidden.

    GLuint waitSyncToken = 0;
    memcpy(&waitSyncToken, mailbox.syncToken, sizeof(waitSyncToken));
    EXPECT_EQ(waitSyncToken, m_gl->mostRecentlyWaitedSyncToken());

    m_drawingBuffer->beginDestruction();
}

} // namespace blink
