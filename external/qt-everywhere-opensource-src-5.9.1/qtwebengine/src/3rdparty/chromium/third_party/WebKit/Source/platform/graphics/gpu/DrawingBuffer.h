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

#include "cc/layers/texture_layer_client.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "platform/PlatformExport.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/GraphicsTypes3D.h"
#include "platform/graphics/gpu/WebGLImageConversion.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "wtf/Deque.h"
#include "wtf/Noncopyable.h"
#include "wtf/RefCounted.h"
#include "wtf/Vector.h"
#include <memory>

namespace cc {
class SharedBitmap;
}

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
class StaticBitmapImage;
class WebExternalTextureLayer;
class WebGraphicsContext3DProvider;
class WebGraphicsContext3DProviderWrapper;
class WebLayer;

// Manages a rendering target (framebuffer + attachment) for a canvas.  Can
// publish its rendering results to a WebLayer for compositing.
class PLATFORM_EXPORT DrawingBuffer
    : public NON_EXPORTED_BASE(cc::TextureLayerClient),
      public RefCounted<DrawingBuffer> {
  WTF_MAKE_NONCOPYABLE(DrawingBuffer);

 public:
  class Client {
   public:
    // Returns true if the DrawingBuffer is currently bound for draw.
    virtual bool DrawingBufferClientIsBoundForDraw() = 0;
    virtual void DrawingBufferClientRestoreScissorTest() = 0;
    // Restores the mask and clear value for color, depth, and stencil buffers.
    virtual void DrawingBufferClientRestoreMaskAndClearValues() = 0;
    virtual void DrawingBufferClientRestorePixelPackAlignment() = 0;
    // Restores the GL_TEXTURE_2D binding for the active texture unit only.
    virtual void DrawingBufferClientRestoreTexture2DBinding() = 0;
    virtual void DrawingBufferClientRestoreRenderbufferBinding() = 0;
    virtual void DrawingBufferClientRestoreFramebufferBinding() = 0;
    virtual void DrawingBufferClientRestorePixelUnpackBufferBinding() = 0;
  };

  enum PreserveDrawingBuffer {
    Preserve,
    Discard,
  };
  enum WebGLVersion {
    WebGL1,
    WebGL2,
  };

  enum ChromiumImageUsage {
    AllowChromiumImage,
    DisallowChromiumImage,
  };

  static PassRefPtr<DrawingBuffer> create(
      std::unique_ptr<WebGraphicsContext3DProvider>,
      Client*,
      const IntSize&,
      bool premultipliedAlpha,
      bool wantAlphaChannel,
      bool wantDepthBuffer,
      bool wantStencilBuffer,
      bool wantAntialiasing,
      PreserveDrawingBuffer,
      WebGLVersion,
      ChromiumImageUsage);
  static void forceNextDrawingBufferCreationToFail();

  ~DrawingBuffer() override;

  // Destruction will be completed after all mailboxes are released.
  void beginDestruction();

  // Issues a glClear() on all framebuffers associated with this DrawingBuffer.
  void clearFramebuffers(GLbitfield clearMask);

  // Indicates whether the DrawingBuffer internally allocated a packed
  // depth-stencil renderbuffer in the situation where the end user only asked
  // for a depth buffer. In this case, we need to upgrade clears of the depth
  // buffer to clears of the depth and stencil buffers in order to avoid
  // performance problems on some GPUs.
  bool hasImplicitStencilBuffer() const { return m_hasImplicitStencilBuffer; }
  bool hasDepthBuffer() const { return !!m_depthStencilBuffer; }
  bool hasStencilBuffer() const { return !!m_depthStencilBuffer; }

  // Given the desired buffer size, provides the largest dimensions that will
  // fit in the pixel budget.
  static IntSize adjustSize(const IntSize& desiredSize,
                            const IntSize& curSize,
                            int maxTextureSize);

  // Resizes (or allocates if necessary) all buffers attached to the default
  // framebuffer. Returns whether the operation was successful.
  bool resize(const IntSize&);

  // Bind the default framebuffer to |target|. |target| must be
  // GL_FRAMEBUFFER, GL_READ_FRAMEBUFFER, or GL_DRAW_FRAMEBUFFER.
  void bind(GLenum target);
  IntSize size() const { return m_size; }

  // Resolves the multisample color buffer to the normal color buffer and leaves
  // the resolved color buffer bound to GL_READ_FRAMEBUFFER and
  // GL_DRAW_FRAMEBUFFER.
  void resolveAndBindForReadAndDraw();

  bool multisample() const;

  bool discardFramebufferSupported() const {
    return m_discardFramebufferSupported;
  }

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

  // cc::TextureLayerClient implementation.
  bool PrepareTextureMailbox(
      cc::TextureMailbox* outMailbox,
      std::unique_ptr<cc::SingleReleaseCallback>* outReleaseCallback) override;

  // Returns a StaticBitmapImage backed by a texture containing the current
  // contents of the front buffer. This is done without any pixel copies. The
  // texture in the ImageBitmap is from the active ContextProvider on the
  // DrawingBuffer.
  PassRefPtr<StaticBitmapImage> transferToStaticBitmapImage();

  bool copyToPlatformTexture(gpu::gles2::GLES2Interface*,
                             GLuint texture,
                             GLenum internalFormat,
                             GLenum destType,
                             GLint level,
                             bool premultiplyAlpha,
                             bool flipY,
                             const IntPoint& destTextureOffset,
                             const IntRect& sourceSubRectangle,
                             SourceDrawingBuffer);

  bool paintRenderingResultsToImageData(int&,
                                        int&,
                                        SourceDrawingBuffer,
                                        WTF::ArrayBufferContents&);

  int sampleCount() const { return m_sampleCount; }
  bool explicitResolveOfMultisampleData() const {
    return m_antiAliasingMode == MSAAExplicitResolve;
  }

  // Rebind the read and draw framebuffers that WebGL is expecting.
  void restoreFramebufferBindings();

  // Restore all state that may have been dirtied by any call.
  void restoreAllState();

  void addNewMailboxCallback(std::unique_ptr<WTF::Closure> closure) {
    m_newMailboxCallback = std::move(closure);
  }

 protected:  // For unittests
  DrawingBuffer(std::unique_ptr<WebGraphicsContext3DProvider>,
                std::unique_ptr<Extensions3DUtil>,
                Client*,
                bool discardFramebufferSupported,
                bool wantAlphaChannel,
                bool premultipliedAlpha,
                PreserveDrawingBuffer,
                WebGLVersion,
                bool wantsDepth,
                bool wantsStencil,
                ChromiumImageUsage);

  bool initialize(const IntSize&, bool useMultisampling);

  // Shared memory bitmaps that were released by the compositor and can be used
  // again by this DrawingBuffer.
  struct RecycledBitmap {
    std::unique_ptr<cc::SharedBitmap> bitmap;
    IntSize size;
  };
  Vector<RecycledBitmap> m_recycledBitmaps;

 private:
  friend class ScopedStateRestorer;
  friend class ColorBuffer;

  // This structure should wrap all public entrypoints that may modify GL state.
  // It will restore all state when it drops out of scope.
  class ScopedStateRestorer {
   public:
    ScopedStateRestorer(DrawingBuffer*);
    ~ScopedStateRestorer();

    // Mark parts of the state that are dirty and need to be restored.
    void setClearStateDirty() { m_clearStateDirty = true; }
    void setPixelPackAlignmentDirty() { m_pixelPackAlignmentDirty = true; }
    void setTextureBindingDirty() { m_textureBindingDirty = true; }
    void setRenderbufferBindingDirty() { m_renderbufferBindingDirty = true; }
    void setFramebufferBindingDirty() { m_framebufferBindingDirty = true; }
    void setPixelUnpackBufferBindingDirty() {
      m_pixelUnpackBufferBindingDirty = true;
    }

   private:
    RefPtr<DrawingBuffer> m_drawingBuffer;
    // The previous state restorer, in case restorers are nested.
    ScopedStateRestorer* m_previousStateRestorer = nullptr;
    bool m_clearStateDirty = false;
    bool m_pixelPackAlignmentDirty = false;
    bool m_textureBindingDirty = false;
    bool m_renderbufferBindingDirty = false;
    bool m_framebufferBindingDirty = false;
    bool m_pixelUnpackBufferBindingDirty = false;
  };

  // All parameters necessary to generate the texture for the ColorBuffer.
  struct ColorBufferParameters {
    DISALLOW_NEW();
    GLenum target = 0;
    GLenum internalColorFormat = 0;

    // The internal color format used when allocating storage for the
    // texture. This may be different from internalColorFormat if RGB
    // emulation is required.
    GLenum creationInternalColorFormat = 0;
    GLenum colorFormat = 0;
  };

  struct ColorBuffer : public RefCounted<ColorBuffer> {
    ColorBuffer(DrawingBuffer*,
                const ColorBufferParameters&,
                const IntSize&,
                GLuint textureId,
                GLuint imageId);
    ~ColorBuffer();

    // The owning DrawingBuffer. Note that DrawingBuffer is explicitly destroyed
    // by the beginDestruction method, which will eventually drain all of its
    // ColorBuffers.
    RefPtr<DrawingBuffer> drawingBuffer;

    const ColorBufferParameters parameters;
    const IntSize size;

    const GLuint textureId = 0;
    const GLuint imageId = 0;

    // The mailbox used to send this buffer to the compositor.
    gpu::Mailbox mailbox;

    // The sync token for when this buffer was sent to the compositor.
    gpu::SyncToken produceSyncToken;

    // The sync token for when this buffer was received back from the
    // compositor.
    gpu::SyncToken receiveSyncToken;

   private:
    WTF_MAKE_NONCOPYABLE(ColorBuffer);
  };

  // The same as clearFramebuffers(), but leaves GL state dirty.
  void clearFramebuffersInternal(GLbitfield clearMask);

  // The same as reset(), but leaves GL state dirty.
  bool resizeFramebufferInternal(const IntSize&);

  // The same as commit(), but leaves GL state dirty.
  void resolveMultisampleFramebufferInternal();

  bool prepareTextureMailboxInternal(
      cc::TextureMailbox* outMailbox,
      std::unique_ptr<cc::SingleReleaseCallback>* outReleaseCallback,
      bool forceGpuResult);

  // Helper functions to be called only by prepareTextureMailboxInternal.
  bool finishPrepareTextureMailboxGpu(
      cc::TextureMailbox* outMailbox,
      std::unique_ptr<cc::SingleReleaseCallback>* outReleaseCallback);
  bool finishPrepareTextureMailboxSoftware(
      cc::TextureMailbox* outMailbox,
      std::unique_ptr<cc::SingleReleaseCallback>* outReleaseCallback);

  // Callbacks for mailboxes given to the compositor from
  // finishPrepareTextureMailboxGpu and finishPrepareTextureMailboxSoftware.
  void mailboxReleasedGpu(RefPtr<ColorBuffer>,
                          const gpu::SyncToken&,
                          bool lostResource);
  void mailboxReleasedSoftware(std::unique_ptr<cc::SharedBitmap>,
                               const IntSize&,
                               const gpu::SyncToken&,
                               bool lostResource);

  // The texture parameters to use for a texture that will be backed by a
  // CHROMIUM_image, backed by a GpuMemoryBuffer.
  ColorBufferParameters gpuMemoryBufferColorBufferParameters();

  // The texture parameters to use for an ordinary GL texture.
  ColorBufferParameters textureColorBufferParameters();

  // Attempts to allocator storage for, or resize all buffers. Returns whether
  // the operation was successful.
  bool resizeDefaultFramebuffer(const IntSize&);

  void clearPlatformLayer();

  std::unique_ptr<cc::SharedBitmap> createOrRecycleBitmap();

  // Updates the current size of the buffer, ensuring that
  // s_currentResourceUsePixels is updated.
  void setSize(const IntSize& size);

  // This is the order of bytes to use when doing a readback.
  enum ReadbackOrder { ReadbackRGBA, ReadbackSkia };

  // Helper function which does a readback from the currently-bound
  // framebuffer into a buffer of a certain size with 4-byte pixels.
  void readBackFramebuffer(unsigned char* pixels,
                           int width,
                           int height,
                           ReadbackOrder,
                           WebGLImageConversion::AlphaOp);

  // Helper function to flip a bitmap vertically.
  void flipVertically(uint8_t* data, int width, int height);

  // If RGB emulation is required, then the CHROMIUM image's alpha channel
  // must be immediately cleared after it is bound to a texture. Nothing
  // should be allowed to change the alpha channel after this.
  void clearChromiumImageAlpha(const ColorBuffer&);

  // Tries to create a CHROMIUM_image backed texture if
  // RuntimeEnabledFeatures::webGLImageChromiumEnabled() is true. On failure,
  // or if the flag is false, creates a default texture. Always returns a valid
  // ColorBuffer.
  RefPtr<ColorBuffer> createColorBuffer(const IntSize&);

  // Creates or recycles a ColorBuffer of size |m_size|.
  PassRefPtr<ColorBuffer> createOrRecycleColorBuffer();

  // Attaches |m_backColorBuffer| to |m_fbo|, which is always the source for
  // read operations.
  void attachColorBufferToReadFramebuffer();

  // Whether the WebGL client desires an explicit resolve. This is
  // implemented by forwarding all draw operations to a multisample
  // renderbuffer, which is resolved before any read operations or swaps.
  bool wantExplicitResolve();

  // Whether the WebGL client wants a depth or stencil buffer.
  bool wantDepthOrStencil();

  // The format to use when creating a multisampled renderbuffer.
  GLenum getMultisampledRenderbufferFormat();

  // Weak, reset by beginDestruction.
  Client* m_client = nullptr;

  const PreserveDrawingBuffer m_preserveDrawingBuffer;
  const WebGLVersion m_webGLVersion;

  std::unique_ptr<WebGraphicsContext3DProviderWrapper> m_contextProvider;
  // Lifetime is tied to the m_contextProvider.
  gpu::gles2::GLES2Interface* m_gl;
  std::unique_ptr<Extensions3DUtil> m_extensionsUtil;
  IntSize m_size = {-1, -1};
  const bool m_discardFramebufferSupported;
  const bool m_wantAlphaChannel;
  const bool m_premultipliedAlpha;
  const bool m_softwareRendering;
  bool m_hasImplicitStencilBuffer = false;
  bool m_storageTextureSupported = false;

  std::unique_ptr<WTF::Closure> m_newMailboxCallback;

  // The current state restorer, which is used to track state dirtying. It is in
  // error to dirty state shared with WebGL while there is no existing state
  // restorer. It is also in error to instantiate two state restorers at once.
  ScopedStateRestorer* m_stateRestorer = nullptr;

  // This is used when the user requests either a depth or stencil buffer.
  GLuint m_depthStencilBuffer = 0;

  // When wantExplicitResolve() returns true, the target of all draw
  // operations.
  GLuint m_multisampleFBO = 0;

  // The id of the renderbuffer storage for |m_multisampleFBO|.
  GLuint m_multisampleRenderbuffer = 0;

  // When wantExplicitResolve() returns false, the target of all draw and
  // read operations. When wantExplicitResolve() returns true, the target of
  // all read operations.
  GLuint m_fbo = 0;

  // The ColorBuffer that backs |m_fbo|.
  RefPtr<ColorBuffer> m_backColorBuffer;

  // The ColorBuffer that was most recently presented to the compositor by
  // prepareTextureMailboxInternal.
  RefPtr<ColorBuffer> m_frontColorBuffer;

  // True if our contents have been modified since the last presentation of this
  // buffer.
  bool m_contentsChanged = true;

  // True if commit() has been called since the last time markContentsChanged()
  // had been called.
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
  bool m_destructionInProgress = false;
  bool m_isHidden = false;
  SkFilterQuality m_filterQuality = kLow_SkFilterQuality;

  std::unique_ptr<WebExternalTextureLayer> m_layer;

  // Mailboxes that were released by the compositor can be used again by this
  // DrawingBuffer.
  Deque<RefPtr<ColorBuffer>> m_recycledColorBufferQueue;

  // If the width and height of the Canvas's backing store don't
  // match those that we were given in the most recent call to
  // reshape(), then we need an intermediate bitmap to read back the
  // frame buffer into. This seems to happen when CSS styles are
  // used to resize the Canvas.
  SkBitmap m_resizingBitmap;

  // In the case of OffscreenCanvas, we do not want to enable the
  // WebGLImageChromium flag, so we replace all the
  // RuntimeEnabledFeatures::webGLImageChromiumEnabled() call with
  // shouldUseChromiumImage() calls, and set m_chromiumImageUsage to
  // DisallowChromiumImage in the case of OffscreenCanvas.
  ChromiumImageUsage m_chromiumImageUsage;
  bool shouldUseChromiumImage();
};

}  // namespace blink

#endif  // DrawingBuffer_h
