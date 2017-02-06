/*
 * Copyright (c) 2010, Google Inc. All rights reserved.
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

#ifndef DrawingBuffer_h
#define DrawingBuffer_h

#include "platform/PlatformExport.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/GraphicsTypes3D.h"
#include "platform/graphics/gpu/WebGLImageConversion.h"
#include "public/platform/WebExternalTextureLayerClient.h"
#include "public/platform/WebExternalTextureMailbox.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "wtf/Deque.h"
#include "wtf/Noncopyable.h"
#include "wtf/RefCounted.h"
#include <memory>

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}

namespace WTF {
class ArrayBufferContents;
}

namespace blink {

class Extensions3DUtil;
class ImageBuffer;
class WebExternalBitmap;
class WebExternalTextureLayer;
class WebGraphicsContext3DProvider;
class WebLayer;

// Manages a rendering target (framebuffer + attachment) for a canvas.  Can publish its rendering
// results to a WebLayer for compositing.
class PLATFORM_EXPORT DrawingBuffer : public RefCounted<DrawingBuffer>, public WebExternalTextureLayerClient  {
    WTF_MAKE_NONCOPYABLE(DrawingBuffer);
public:
    enum PreserveDrawingBuffer {
        Preserve,
        Discard
    };

    static PassRefPtr<DrawingBuffer> create(
        std::unique_ptr<WebGraphicsContext3DProvider>,
        const IntSize&,
        bool premultipliedAlpha,
        bool wantAlphaChannel,
        bool wantDepthBuffer,
        bool wantStencilBuffer,
        bool wantAntialiasing,
        PreserveDrawingBuffer);
    static void forceNextDrawingBufferCreationToFail();

    ~DrawingBuffer() override;

    // Destruction will be completed after all mailboxes are released.
    void beginDestruction();

    // Issues a glClear() on all framebuffers associated with this DrawingBuffer. The caller is responsible for
    // making the context current and setting the clear values and masks. Modifies the framebuffer binding.
    void clearFramebuffers(GLbitfield clearMask);

    // Indicates whether the DrawingBuffer internally allocated a packed depth-stencil renderbuffer
    // in the situation where the end user only asked for a depth buffer. In this case, we need to
    // upgrade clears of the depth buffer to clears of the depth and stencil buffers in order to
    // avoid performance problems on some GPUs.
    bool hasImplicitStencilBuffer() const { return m_hasImplicitStencilBuffer; }
    bool hasDepthBuffer() const { return !!m_depthStencilBuffer; }
    bool hasStencilBuffer() const { return !!m_depthStencilBuffer; }

    // Given the desired buffer size, provides the largest dimensions that will fit in the pixel budget.
    static IntSize adjustSize(const IntSize& desiredSize, const IntSize& curSize, int maxTextureSize);

    // Resizes (or allocates if necessary) all buffers attached to the default
    // framebuffer. Returns whether the operation was successful. Leaves GL
    // bindings dirtied.
    bool reset(const IntSize&);

    // Bind the default framebuffer to |target|. |target| must be
    // GL_FRAMEBUFFER, GL_READ_FRAMEBUFFER, or GL_DRAW_FRAMEBUFFER.
    void bind(GLenum target);
    IntSize size() const { return m_size; }

    // Copies the multisample color buffer to the normal color buffer and leaves m_fbo bound.
    void commit();

    // commit should copy the full multisample buffer, and not respect the
    // current scissor bounds. Track the state of the scissor test so that it
    // can be disabled during calls to commit.
    void setScissorEnabled(bool scissorEnabled) { m_scissorEnabled = scissorEnabled; }

    // The DrawingBuffer needs to track the texture bound to texture unit 0.
    // The bound texture is tracked to avoid costly queries during rendering.
    void setTexture2DBinding(GLuint texture) { m_texture2DBinding = texture; }

    // The DrawingBuffer needs to track the currently bound framebuffer so it
    // restore the binding when needed.
    void setFramebufferBinding(GLenum target, GLuint fbo)
    {
        switch (target) {
        case GL_FRAMEBUFFER:
            m_drawFramebufferBinding = fbo;
            m_readFramebufferBinding = fbo;
            break;
        case GL_DRAW_FRAMEBUFFER:
            m_drawFramebufferBinding = fbo;
            break;
        case GL_READ_FRAMEBUFFER:
            m_readFramebufferBinding = fbo;
            break;
        default:
            ASSERT(0);
        }
    }

    // The DrawingBuffer needs to track the color mask and clear color so that
    // it can restore it when needed.
    void setClearColor(GLfloat* clearColor)
    {
        memcpy(m_clearColor, clearColor, 4 * sizeof(GLfloat));
    }

    void setColorMask(GLboolean* colorMask)
    {
        memcpy(m_colorMask, colorMask, 4 * sizeof(GLboolean));
    }

    // The DrawingBuffer needs to track the currently bound renderbuffer so it
    // restore the binding when needed.
    void setRenderbufferBinding(GLuint renderbuffer)
    {
        m_renderbufferBinding = renderbuffer;
    }

    // Track the currently active texture unit. Texture unit 0 is used as host for a scratch
    // texture.
    void setActiveTextureUnit(GLint textureUnit) { m_activeTextureUnit = textureUnit; }

    bool multisample() const;

    GLuint framebuffer() const;

    bool discardFramebufferSupported() const { return m_discardFramebufferSupported; }

    void markContentsChanged();
    void setBufferClearNeeded(bool);
    bool bufferClearNeeded() const;
    void setIsHidden(bool);
    void setFilterQuality(SkFilterQuality);

    // Whether the target for draw operations has format GL_RGBA, but is
    // emulating format GL_RGB. When the target's storage is first
    // allocated, its alpha channel must be cleared to 1. All future drawing
    // operations must use a color mask with alpha=GL_FALSE.
    bool requiresAlphaChannelToBePreserved();

    // Similar to requiresAlphaChannelToBePreserved(), but always targets the
    // default framebuffer.
    bool defaultBufferRequiresAlphaChannelToBePreserved();

    WebLayer* platformLayer();

    gpu::gles2::GLES2Interface* contextGL();
    WebGraphicsContext3DProvider* contextProvider();

    // WebExternalTextureLayerClient implementation.
    bool prepareMailbox(WebExternalTextureMailbox*, WebExternalBitmap*) override;
    void mailboxReleased(const WebExternalTextureMailbox&, bool lostResource = false) override;

    // Destroys the TEXTURE_2D binding for the owned context
    bool copyToPlatformTexture(gpu::gles2::GLES2Interface*, GLuint texture, GLenum internalFormat,
        GLenum destType, GLint level, bool premultiplyAlpha, bool flipY, SourceDrawingBuffer);

    void setPackAlignment(GLint param);

    bool paintRenderingResultsToImageData(int&, int&, SourceDrawingBuffer, WTF::ArrayBufferContents&);

    int sampleCount() const { return m_sampleCount; }
    bool explicitResolveOfMultisampleData() const { return m_antiAliasingMode == MSAAExplicitResolve; }

    // Bind to m_drawFramebufferBinding or m_readFramebufferBinding if it's not 0.
    // Otherwise, bind to the default FBO.
    void restoreFramebufferBindings();

    void restoreTextureBindings();

    void addNewMailboxCallback(std::unique_ptr<WTF::Closure> closure) { m_newMailboxCallback = std::move(closure); }

protected: // For unittests
    DrawingBuffer(
        std::unique_ptr<WebGraphicsContext3DProvider>,
        std::unique_ptr<Extensions3DUtil>,
        bool discardFramebufferSupported,
        bool wantAlphaChannel,
        bool premultipliedAlpha,
        PreserveDrawingBuffer,
        bool wantsDepth,
        bool wantsStencil);

    bool initialize(const IntSize&, bool useMultisampling);

private:
    // All parameters necessary to generate the texture that will be passed to
    // prepareMailbox.
    struct TextureParameters {
        DISALLOW_NEW();
        GLenum target = 0;
        GLenum internalColorFormat = 0;

        // The internal color format used when allocating storage for the
        // texture. This may be different from internalColorFormat if RGB
        // emulation is required.
        GLenum creationInternalColorFormat = 0;
        GLenum colorFormat = 0;
    };

    // If we used CHROMIUM_image as the backing storage for our buffers,
    // we need to know the mapping from texture id to image.
    struct TextureInfo {
        DISALLOW_NEW();
        GLuint textureId = 0;
        GLuint imageId = 0;

        // A GpuMemoryBuffer is a concept that the compositor understands. and
        // is able to operate on. The id is scoped to renderer process.
        GLint gpuMemoryBufferId = -1;

        TextureParameters parameters;
    };

    struct MailboxInfo : public RefCounted<MailboxInfo> {
        WTF_MAKE_NONCOPYABLE(MailboxInfo);

    public:
        MailboxInfo() {}

        WebExternalTextureMailbox mailbox;
        TextureInfo textureInfo;
        IntSize size;
        // This keeps the parent drawing buffer alive as long as the compositor is
        // referring to one of the mailboxes DrawingBuffer produced. The parent drawing buffer is
        // cleared when the compositor returns the mailbox. See mailboxReleased().
        RefPtr<DrawingBuffer> m_parentDrawingBuffer;
    };

    // The texture parameters to use for a texture that will be backed by a
    // CHROMIUM_image.
    TextureParameters chromiumImageTextureParameters();

    // The texture parameters to use for a default texture.
    TextureParameters defaultTextureParameters();

    void mailboxReleasedWithoutRecycling(const WebExternalTextureMailbox&);

    // Creates and binds a texture with the given parameters. Returns 0 on
    // failure, or the newly created texture id on success. The caller takes
    // ownership of the newly created texture.
    GLuint createColorTexture(const TextureParameters&);

    // Create the depth/stencil and multisample buffers, if needed.
    bool resizeMultisampleFramebuffer(const IntSize&);
    void resizeDepthStencil(const IntSize&);

    // Attempts to allocator storage for, or resize all buffers. Returns whether
    // the operation was successful.
    bool resizeDefaultFramebuffer(const IntSize&);

    void clearPlatformLayer();

    PassRefPtr<MailboxInfo> recycledMailbox();
    PassRefPtr<MailboxInfo> createNewMailbox(const TextureInfo&);
    void deleteMailbox(const WebExternalTextureMailbox&);
    void freeRecycledMailboxes();

    // Updates the current size of the buffer, ensuring that s_currentResourceUsePixels is updated.
    void setSize(const IntSize& size);

    // This is the order of bytes to use when doing a readback.
    enum ReadbackOrder {
        ReadbackRGBA,
        ReadbackSkia
    };

    // Helper function which does a readback from the currently-bound
    // framebuffer into a buffer of a certain size with 4-byte pixels.
    void readBackFramebuffer(unsigned char* pixels, int width, int height, ReadbackOrder, WebGLImageConversion::AlphaOp);

    // Helper function to flip a bitmap vertically.
    void flipVertically(uint8_t* data, int width, int height);

    // Helper to texImage2D with pixel==0 case: pixels are initialized to 0.
    // By default, alignment is 4, the OpenGL default setting.
    void texImage2DResourceSafe(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, GLint alignment = 4);
    // Allocate buffer storage to be sent to compositor using either texImage2D or CHROMIUM_image based on available support.
    void deleteChromiumImageForTexture(TextureInfo*);

    // If RGB emulation is required, then the CHROMIUM image's alpha channel
    // must be immediately cleared after it is bound to a texture. Nothing
    // should be allowed to change the alpha channel after this.
    void clearChromiumImageAlpha(const TextureInfo&);

    // Tries to create a CHROMIUM_image backed texture if
    // RuntimeEnabledFeatures::webGLImageChromiumEnabled() is true. On failure,
    // or if the flag is false, creates a default texture.
    TextureInfo createTextureAndAllocateMemory(const IntSize&);

    // Creates and allocates space for a default texture.
    TextureInfo createDefaultTextureAndAllocateMemory(const IntSize&);

    void resizeTextureMemory(TextureInfo*, const IntSize&);

    // Attaches |m_colorBuffer| to |m_fbo|, which is always the source for read
    // operations.
    void attachColorBufferToReadFramebuffer();

    // Whether the WebGL client desires an explicit resolve. This is
    // implemented by forwarding all draw operations to a multisample
    // renderbuffer, which is resolved before any read operations or swaps.
    bool wantExplicitResolve();

    // Whether the WebGL client wants a depth or stencil buffer.
    bool wantDepthOrStencil();

    // The format to use when creating a multisampled renderbuffer.
    GLenum getMultisampledRenderbufferFormat();

    const PreserveDrawingBuffer m_preserveDrawingBuffer;
    bool m_scissorEnabled = false;
    GLuint m_texture2DBinding = 0;
    GLuint m_drawFramebufferBinding = 0;
    GLuint m_readFramebufferBinding = 0;
    GLuint m_renderbufferBinding = 0;
    GLenum m_activeTextureUnit = GL_TEXTURE0;
    GLfloat m_clearColor[4];
    GLboolean m_colorMask[4];

    std::unique_ptr<WebGraphicsContext3DProvider> m_contextProvider;
    // Lifetime is tied to the m_contextProvider.
    gpu::gles2::GLES2Interface* m_gl;
    std::unique_ptr<Extensions3DUtil> m_extensionsUtil;
    IntSize m_size = { -1, -1 };
    const bool m_discardFramebufferSupported;
    const bool m_wantAlphaChannel;
    const bool m_premultipliedAlpha;
    bool m_hasImplicitStencilBuffer = false;
    struct FrontBufferInfo {
        TextureInfo texInfo;
        WebExternalTextureMailbox mailbox;
    };
    FrontBufferInfo m_frontColorBuffer;

    std::unique_ptr<WTF::Closure> m_newMailboxCallback;

    // This is used when the user requests either a depth or stencil buffer.
    GLuint m_depthStencilBuffer = 0;

    // When wantExplicitResolve() returns true, the target of all draw
    // operations.
    GLuint m_multisampleFBO = 0;

    // The id of the renderbuffer storage for |m_multisampleFBO|.
    GLuint m_multisampleRenderbuffer = 0;

    // When wantExplicitResolve() returns false, the target of all draw and
    // read operations. When wantExplicitResolve() returns true, the target of
    // all read operations. A swap is performed by exchanging |m_colorBuffer|
    // with |m_frontColorBuffer|.
    GLuint m_fbo = 0;

    // All information about the texture storage for |m_fbo|.
    TextureInfo m_colorBuffer;

    // True if our contents have been modified since the last presentation of this buffer.
    bool m_contentsChanged = true;

    // True if commit() has been called since the last time markContentsChanged() had been called.
    bool m_contentsChangeCommitted = false;
    bool m_bufferClearNeeded = false;

    // Whether the client wants a depth or stencil buffer.
    const bool m_wantDepth;
    const bool m_wantStencil;

    enum AntialiasingMode {
        None,
        MSAAImplicitResolve,
        MSAAExplicitResolve,
        ScreenSpaceAntialiasing,
    };

    AntialiasingMode m_antiAliasingMode = None;

    int m_maxTextureSize = 0;
    int m_sampleCount = 0;
    int m_packAlignment = 4;
    bool m_destructionInProgress = false;
    bool m_isHidden = false;
    SkFilterQuality m_filterQuality = kLow_SkFilterQuality;

    std::unique_ptr<WebExternalTextureLayer> m_layer;

    // All of the mailboxes that this DrawingBuffer has ever created.
    Vector<RefPtr<MailboxInfo>> m_textureMailboxes;
    // Mailboxes that were released by the compositor can be used again by this DrawingBuffer.
    Deque<WebExternalTextureMailbox> m_recycledMailboxQueue;

    // If the width and height of the Canvas's backing store don't
    // match those that we were given in the most recent call to
    // reshape(), then we need an intermediate bitmap to read back the
    // frame buffer into. This seems to happen when CSS styles are
    // used to resize the Canvas.
    SkBitmap m_resizingBitmap;

    // Used to flip a bitmap vertically.
    Vector<uint8_t> m_scanline;
};

} // namespace blink

#endif // DrawingBuffer_h
