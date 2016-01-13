// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/gl_helper.h"

#include <queue>
#include <string>

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "content/common/gpu/client/gl_helper_readback_support.h"
#include "content/common/gpu/client/gl_helper_scaling.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

using gpu::gles2::GLES2Interface;

namespace {

class ScopedFlush {
 public:
  explicit ScopedFlush(gpu::gles2::GLES2Interface* gl) : gl_(gl) {}

  ~ScopedFlush() { gl_->Flush(); }

 private:
  gpu::gles2::GLES2Interface* gl_;

  DISALLOW_COPY_AND_ASSIGN(ScopedFlush);
};

// Helper class for allocating and holding an RGBA texture of a given
// size and an associated framebuffer.
class TextureFrameBufferPair {
 public:
  TextureFrameBufferPair(GLES2Interface* gl, gfx::Size size)
      : texture_(gl), framebuffer_(gl), size_(size) {
    content::ScopedTextureBinder<GL_TEXTURE_2D> texture_binder(gl, texture_);
    gl->TexImage2D(GL_TEXTURE_2D,
                   0,
                   GL_RGBA,
                   size.width(),
                   size.height(),
                   0,
                   GL_RGBA,
                   GL_UNSIGNED_BYTE,
                   NULL);
    content::ScopedFramebufferBinder<GL_FRAMEBUFFER> framebuffer_binder(
        gl, framebuffer_);
    gl->FramebufferTexture2D(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_, 0);
  }

  GLuint texture() const { return texture_.id(); }
  GLuint framebuffer() const { return framebuffer_.id(); }
  gfx::Size size() const { return size_; }

 private:
  content::ScopedTexture texture_;
  content::ScopedFramebuffer framebuffer_;
  gfx::Size size_;

  DISALLOW_COPY_AND_ASSIGN(TextureFrameBufferPair);
};

// Helper class for holding a scaler, a texture for the output of that
// scaler and an associated frame buffer. This is inteded to be used
// when the output of a scaler is to be sent to a readback.
class ScalerHolder {
 public:
  ScalerHolder(GLES2Interface* gl, content::GLHelper::ScalerInterface* scaler)
      : texture_and_framebuffer_(gl, scaler->DstSize()), scaler_(scaler) {}

  void Scale(GLuint src_texture) {
    scaler_->Scale(src_texture, texture_and_framebuffer_.texture());
  }

  content::GLHelper::ScalerInterface* scaler() const { return scaler_.get(); }
  TextureFrameBufferPair* texture_and_framebuffer() {
    return &texture_and_framebuffer_;
  }
  GLuint texture() const { return texture_and_framebuffer_.texture(); }

 private:
  TextureFrameBufferPair texture_and_framebuffer_;
  scoped_ptr<content::GLHelper::ScalerInterface> scaler_;

  DISALLOW_COPY_AND_ASSIGN(ScalerHolder);
};

}  // namespace

namespace content {

// Implements GLHelper::CropScaleReadbackAndCleanTexture and encapsulates
// the data needed for it.
class GLHelper::CopyTextureToImpl
    : public base::SupportsWeakPtr<GLHelper::CopyTextureToImpl> {
 public:
  CopyTextureToImpl(GLES2Interface* gl,
                    gpu::ContextSupport* context_support,
                    GLHelper* helper)
      : gl_(gl),
        context_support_(context_support),
        helper_(helper),
        flush_(gl),
        max_draw_buffers_(0) {
    const GLubyte* extensions = gl_->GetString(GL_EXTENSIONS);
    if (!extensions)
      return;
    std::string extensions_string =
        " " + std::string(reinterpret_cast<const char*>(extensions)) + " ";
    if (extensions_string.find(" GL_EXT_draw_buffers ") != std::string::npos) {
      gl_->GetIntegerv(GL_MAX_DRAW_BUFFERS_EXT, &max_draw_buffers_);
    }
  }
  ~CopyTextureToImpl() { CancelRequests(); }

  GLuint ConsumeMailboxToTexture(const gpu::Mailbox& mailbox,
                                 uint32 sync_point) {
    return helper_->ConsumeMailboxToTexture(mailbox, sync_point);
  }

  void CropScaleReadbackAndCleanTexture(
      GLuint src_texture,
      const gfx::Size& src_size,
      const gfx::Rect& src_subrect,
      const gfx::Size& dst_size,
      unsigned char* out,
      const SkBitmap::Config config,
      const base::Callback<void(bool)>& callback,
      GLHelper::ScalerQuality quality);

  void ReadbackTextureSync(GLuint texture,
                           const gfx::Rect& src_rect,
                           unsigned char* out,
                           SkBitmap::Config format);

  void ReadbackTextureAsync(GLuint texture,
                            const gfx::Size& dst_size,
                            unsigned char* out,
                            SkBitmap::Config config,
                            const base::Callback<void(bool)>& callback);

  // Reads back bytes from the currently bound frame buffer.
  // Note that dst_size is specified in bytes, not pixels.
  void ReadbackAsync(const gfx::Size& dst_size,
                     int32 bytes_per_row,     // generally dst_size.width() * 4
                     int32 row_stride_bytes,  // generally dst_size.width() * 4
                     unsigned char* out,
                     const SkBitmap::Config config,
                     ReadbackSwizzle swizzle,
                     const base::Callback<void(bool)>& callback);

  void ReadbackPlane(TextureFrameBufferPair* source,
                     const scoped_refptr<media::VideoFrame>& target,
                     int plane,
                     int size_shift,
                     const gfx::Rect& dst_subrect,
                     ReadbackSwizzle swizzle,
                     const base::Callback<void(bool)>& callback);

  GLuint CopyAndScaleTexture(GLuint texture,
                             const gfx::Size& src_size,
                             const gfx::Size& dst_size,
                             bool vertically_flip_texture,
                             GLHelper::ScalerQuality quality);

  ReadbackYUVInterface* CreateReadbackPipelineYUV(
      GLHelper::ScalerQuality quality,
      const gfx::Size& src_size,
      const gfx::Rect& src_subrect,
      const gfx::Size& dst_size,
      const gfx::Rect& dst_subrect,
      bool flip_vertically,
      bool use_mrt);

  // Returns the maximum number of draw buffers available,
  // 0 if GL_EXT_draw_buffers is not available.
  GLint MaxDrawBuffers() const { return max_draw_buffers_; }

  bool IsReadbackConfigSupported(SkBitmap::Config bitmap_config);

 private:
  // A single request to CropScaleReadbackAndCleanTexture.
  // The main thread can cancel the request, before it's handled by the helper
  // thread, by resetting the texture and pixels fields. Alternatively, the
  // thread marks that it handles the request by resetting the pixels field
  // (meaning it guarantees that the callback with be called).
  // In either case, the callback must be called exactly once, and the texture
  // must be deleted by the main thread gl.
  struct Request {
    Request(const gfx::Size& size_,
            int32 bytes_per_row_,
            int32 row_stride_bytes_,
            unsigned char* pixels_,
            const base::Callback<void(bool)>& callback_)
        : done(false),
          size(size_),
          bytes_per_row(bytes_per_row_),
          row_stride_bytes(row_stride_bytes_),
          pixels(pixels_),
          callback(callback_),
          buffer(0),
          query(0) {}

    bool done;
    gfx::Size size;
    int bytes_per_row;
    int row_stride_bytes;
    unsigned char* pixels;
    base::Callback<void(bool)> callback;
    GLuint buffer;
    GLuint query;
  };

  // A readback pipeline that also converts the data to YUV before
  // reading it back.
  class ReadbackYUVImpl : public ReadbackYUVInterface {
   public:
    ReadbackYUVImpl(GLES2Interface* gl,
                    CopyTextureToImpl* copy_impl,
                    GLHelperScaling* scaler_impl,
                    GLHelper::ScalerQuality quality,
                    const gfx::Size& src_size,
                    const gfx::Rect& src_subrect,
                    const gfx::Size& dst_size,
                    const gfx::Rect& dst_subrect,
                    bool flip_vertically,
                    ReadbackSwizzle swizzle);

    virtual void ReadbackYUV(const gpu::Mailbox& mailbox,
                             uint32 sync_point,
                             const scoped_refptr<media::VideoFrame>& target,
                             const base::Callback<void(bool)>& callback)
        OVERRIDE;

    virtual ScalerInterface* scaler() OVERRIDE { return scaler_.scaler(); }

   private:
    GLES2Interface* gl_;
    CopyTextureToImpl* copy_impl_;
    gfx::Size dst_size_;
    gfx::Rect dst_subrect_;
    ReadbackSwizzle swizzle_;
    ScalerHolder scaler_;
    ScalerHolder y_;
    ScalerHolder u_;
    ScalerHolder v_;

    DISALLOW_COPY_AND_ASSIGN(ReadbackYUVImpl);
  };

  // A readback pipeline that also converts the data to YUV before
  // reading it back. This one uses Multiple Render Targets, which
  // may not be supported on all platforms.
  class ReadbackYUV_MRT : public ReadbackYUVInterface {
   public:
    ReadbackYUV_MRT(GLES2Interface* gl,
                    CopyTextureToImpl* copy_impl,
                    GLHelperScaling* scaler_impl,
                    GLHelper::ScalerQuality quality,
                    const gfx::Size& src_size,
                    const gfx::Rect& src_subrect,
                    const gfx::Size& dst_size,
                    const gfx::Rect& dst_subrect,
                    bool flip_vertically,
                    ReadbackSwizzle swizzle);

    virtual void ReadbackYUV(const gpu::Mailbox& mailbox,
                             uint32 sync_point,
                             const scoped_refptr<media::VideoFrame>& target,
                             const base::Callback<void(bool)>& callback)
        OVERRIDE;

    virtual ScalerInterface* scaler() OVERRIDE { return scaler_.scaler(); }

   private:
    GLES2Interface* gl_;
    CopyTextureToImpl* copy_impl_;
    gfx::Size dst_size_;
    gfx::Rect dst_subrect_;
    GLHelper::ScalerQuality quality_;
    ReadbackSwizzle swizzle_;
    ScalerHolder scaler_;
    scoped_ptr<content::GLHelperScaling::ShaderInterface> pass1_shader_;
    scoped_ptr<content::GLHelperScaling::ShaderInterface> pass2_shader_;
    TextureFrameBufferPair y_;
    ScopedTexture uv_;
    TextureFrameBufferPair u_;
    TextureFrameBufferPair v_;

    DISALLOW_COPY_AND_ASSIGN(ReadbackYUV_MRT);
  };

  // Copies the block of pixels specified with |src_subrect| from |src_texture|,
  // scales it to |dst_size|, writes it into a texture, and returns its ID.
  // |src_size| is the size of |src_texture|.
  GLuint ScaleTexture(GLuint src_texture,
                      const gfx::Size& src_size,
                      const gfx::Rect& src_subrect,
                      const gfx::Size& dst_size,
                      bool vertically_flip_texture,
                      bool swizzle,
                      SkBitmap::Config config,
                      GLHelper::ScalerQuality quality);

  static void nullcallback(bool success) {}
  void ReadbackDone(Request *request, int bytes_per_pixel);
  void FinishRequest(Request* request, bool result);
  void CancelRequests();

  static const float kRGBtoYColorWeights[];
  static const float kRGBtoUColorWeights[];
  static const float kRGBtoVColorWeights[];

  GLES2Interface* gl_;
  gpu::ContextSupport* context_support_;
  GLHelper* helper_;

  // A scoped flush that will ensure all resource deletions are flushed when
  // this object is destroyed. Must be declared before other Scoped* fields.
  ScopedFlush flush_;

  std::queue<Request*> request_queue_;
  GLint max_draw_buffers_;
};

GLHelper::ScalerInterface* GLHelper::CreateScaler(ScalerQuality quality,
                                                  const gfx::Size& src_size,
                                                  const gfx::Rect& src_subrect,
                                                  const gfx::Size& dst_size,
                                                  bool vertically_flip_texture,
                                                  bool swizzle) {
  InitScalerImpl();
  return scaler_impl_->CreateScaler(quality,
                                    src_size,
                                    src_subrect,
                                    dst_size,
                                    vertically_flip_texture,
                                    swizzle);
}

GLuint GLHelper::CopyTextureToImpl::ScaleTexture(
    GLuint src_texture,
    const gfx::Size& src_size,
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    bool vertically_flip_texture,
    bool swizzle,
    SkBitmap::Config bitmap_config,
    GLHelper::ScalerQuality quality) {
  if (!IsReadbackConfigSupported(bitmap_config))
    return 0;

  scoped_ptr<ScalerInterface> scaler(
      helper_->CreateScaler(quality,
                            src_size,
                            src_subrect,
                            dst_size,
                            vertically_flip_texture,
                            swizzle));
  GLuint dst_texture = 0u;
  // Start with ARGB8888 params as any other format which is not
  // supported is already asserted above.
  GLenum format = GL_RGBA , type = GL_UNSIGNED_BYTE;
  gl_->GenTextures(1, &dst_texture);
  {
    ScopedTextureBinder<GL_TEXTURE_2D> texture_binder(gl_, dst_texture);
    switch (bitmap_config) {
      case SkBitmap::kARGB_8888_Config:
        // Do nothing params already set.
        break;
      case SkBitmap::kRGB_565_Config:
        format = GL_RGB;
        type = GL_UNSIGNED_SHORT_5_6_5;
        break;
      default:
        NOTREACHED();
        break;
    }
    gl_->TexImage2D(GL_TEXTURE_2D,
                    0,
                    format,
                    dst_size.width(),
                    dst_size.height(),
                    0,
                    format,
                    type,
                    NULL);
  }
  scaler->Scale(src_texture, dst_texture);
  return dst_texture;
}

void GLHelper::CopyTextureToImpl::ReadbackAsync(
    const gfx::Size& dst_size,
    int32 bytes_per_row,
    int32 row_stride_bytes,
    unsigned char* out,
    const SkBitmap::Config bitmap_config,
    ReadbackSwizzle swizzle,
    const base::Callback<void(bool)>& callback) {
  if (!IsReadbackConfigSupported(bitmap_config)) {
    callback.Run(false);
    return;
  }
  Request* request =
      new Request(dst_size, bytes_per_row, row_stride_bytes, out, callback);
  request_queue_.push(request);
  request->buffer = 0u;
  // Start with ARGB8888 params as any other format which is not
  // supported is already asserted above.
  GLenum format = GL_RGBA, type = GL_UNSIGNED_BYTE;
  int bytes_per_pixel = 4;

  switch (bitmap_config) {
    case SkBitmap::kARGB_8888_Config:
      if (swizzle == kSwizzleBGRA)
        format = GL_BGRA_EXT;
      break;
    case SkBitmap::kRGB_565_Config:
      format = GL_RGB;
      type = GL_UNSIGNED_SHORT_5_6_5;
      bytes_per_pixel = 2;
      break;
    default:
      NOTREACHED();
      break;
  }
  gl_->GenBuffers(1, &request->buffer);
  gl_->BindBuffer(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM, request->buffer);
  gl_->BufferData(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM,
                  bytes_per_pixel * dst_size.GetArea(),
                  NULL,
                  GL_STREAM_READ);

  request->query = 0u;
  gl_->GenQueriesEXT(1, &request->query);
  gl_->BeginQueryEXT(GL_ASYNC_PIXEL_PACK_COMPLETED_CHROMIUM, request->query);
  gl_->ReadPixels(0,
                  0,
                  dst_size.width(),
                  dst_size.height(),
                  format,
                  type,
                  NULL);
  gl_->EndQueryEXT(GL_ASYNC_PIXEL_PACK_COMPLETED_CHROMIUM);
  gl_->BindBuffer(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM, 0);
  context_support_->SignalQuery(
      request->query,
      base::Bind(&CopyTextureToImpl::ReadbackDone, AsWeakPtr(),
                 request, bytes_per_pixel));
}
void GLHelper::CopyTextureToImpl::CropScaleReadbackAndCleanTexture(
    GLuint src_texture,
    const gfx::Size& src_size,
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    unsigned char* out,
    const SkBitmap::Config bitmap_config,
    const base::Callback<void(bool)>& callback,
    GLHelper::ScalerQuality quality) {
  if (!IsReadbackConfigSupported(bitmap_config)) {
    callback.Run(false);
    return;
  }
  GLuint texture = ScaleTexture(src_texture,
                                src_size,
                                src_subrect,
                                dst_size,
                                true,
#if (SK_R32_SHIFT == 16) && !SK_B32_SHIFT
                                true,
#else
                                false,
#endif
                                bitmap_config,
                                quality);
  DCHECK(texture);
  ScopedFramebuffer dst_framebuffer(gl_);
  ScopedFramebufferBinder<GL_FRAMEBUFFER> framebuffer_binder(gl_,
                                                             dst_framebuffer);
  ScopedTextureBinder<GL_TEXTURE_2D> texture_binder(gl_, texture);
  gl_->FramebufferTexture2D(GL_FRAMEBUFFER,
                            GL_COLOR_ATTACHMENT0,
                            GL_TEXTURE_2D,
                            texture,
                            0);
  int bytes_per_pixel = 4;
  switch (bitmap_config) {
    case SkBitmap::kARGB_8888_Config:
      // Do nothing params already set.
      break;
    case SkBitmap::kRGB_565_Config:
      bytes_per_pixel = 2;
      break;
    default:
      NOTREACHED();
      break;
  }
  ReadbackAsync(dst_size,
                dst_size.width() * bytes_per_pixel,
                dst_size.width() * bytes_per_pixel,
                out,
                bitmap_config,
                kSwizzleNone,
                callback);
  gl_->DeleteTextures(1, &texture);
}

void GLHelper::CopyTextureToImpl::ReadbackTextureSync(
    GLuint texture,
    const gfx::Rect& src_rect,
    unsigned char* out,
    SkBitmap::Config bitmap_config) {
  if (!IsReadbackConfigSupported(bitmap_config))
    return;

  ScopedFramebuffer dst_framebuffer(gl_);
  ScopedFramebufferBinder<GL_FRAMEBUFFER> framebuffer_binder(gl_,
                                                             dst_framebuffer);
  ScopedTextureBinder<GL_TEXTURE_2D> texture_binder(gl_, texture);
  gl_->FramebufferTexture2D(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
  GLenum format =
      (bitmap_config == SkBitmap::kRGB_565_Config) ? GL_RGB : GL_RGBA;
  GLenum type = (bitmap_config == SkBitmap::kRGB_565_Config)
                    ? GL_UNSIGNED_SHORT_5_6_5
                    : GL_UNSIGNED_BYTE;
  gl_->ReadPixels(src_rect.x(),
                  src_rect.y(),
                  src_rect.width(),
                  src_rect.height(),
                  format,
                  type,
                  out);
}

void GLHelper::CopyTextureToImpl::ReadbackTextureAsync(
    GLuint texture,
    const gfx::Size& dst_size,
    unsigned char* out,
    SkBitmap::Config bitmap_config,
    const base::Callback<void(bool)>& callback) {
  if (!IsReadbackConfigSupported(bitmap_config))
    return;

  ScopedFramebuffer dst_framebuffer(gl_);
  ScopedFramebufferBinder<GL_FRAMEBUFFER> framebuffer_binder(gl_,
                                                             dst_framebuffer);
  ScopedTextureBinder<GL_TEXTURE_2D> texture_binder(gl_, texture);
  gl_->FramebufferTexture2D(GL_FRAMEBUFFER,
                            GL_COLOR_ATTACHMENT0,
                            GL_TEXTURE_2D,
                            texture,
                            0);
  int bytes_per_pixel = (bitmap_config == SkBitmap::kRGB_565_Config) ? 2 : 4;
  ReadbackAsync(dst_size,
                dst_size.width() * bytes_per_pixel,
                dst_size.width() * bytes_per_pixel,
                out,
                bitmap_config,
                kSwizzleNone,
                callback);
}

GLuint GLHelper::CopyTextureToImpl::CopyAndScaleTexture(
    GLuint src_texture,
    const gfx::Size& src_size,
    const gfx::Size& dst_size,
    bool vertically_flip_texture,
    GLHelper::ScalerQuality quality) {
  return ScaleTexture(src_texture,
                      src_size,
                      gfx::Rect(src_size),
                      dst_size,
                      vertically_flip_texture,
                      false,
                      SkBitmap::kARGB_8888_Config,
                      quality);
}

bool GLHelper::CopyTextureToImpl::IsReadbackConfigSupported(
    SkBitmap::Config bitmap_config) {
  if (!helper_) {
    DCHECK(helper_);
    return false;
  }
  return helper_->IsReadbackConfigSupported(bitmap_config);
}

void GLHelper::CopyTextureToImpl::ReadbackDone(Request* finished_request,
                                               int bytes_per_pixel) {
  TRACE_EVENT0("mirror",
               "GLHelper::CopyTextureToImpl::CheckReadbackFramebufferComplete");
  finished_request->done = true;

  // We process transfer requests in the order they were received, regardless
  // of the order we get the callbacks in.
  while (!request_queue_.empty()) {
    Request* request = request_queue_.front();
    if (!request->done) {
      break;
    }

    bool result = false;
    if (request->buffer != 0) {
      gl_->BindBuffer(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM, request->buffer);
      unsigned char* data = static_cast<unsigned char*>(gl_->MapBufferCHROMIUM(
          GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM, GL_READ_ONLY));
      if (data) {
        result = true;
        if (request->bytes_per_row == request->size.width() * bytes_per_pixel &&
            request->bytes_per_row == request->row_stride_bytes) {
          memcpy(request->pixels, data,
                 request->size.GetArea() * bytes_per_pixel);
        } else {
          unsigned char* out = request->pixels;
          for (int y = 0; y < request->size.height(); y++) {
            memcpy(out, data, request->bytes_per_row);
            out += request->row_stride_bytes;
            data += request->size.width() * bytes_per_pixel;
          }
        }
        gl_->UnmapBufferCHROMIUM(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM);
      }
      gl_->BindBuffer(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM, 0);
    }
    FinishRequest(request, result);
  }
}

void GLHelper::CopyTextureToImpl::FinishRequest(Request* request, bool result) {
  TRACE_EVENT0("mirror", "GLHelper::CopyTextureToImpl::FinishRequest");
  DCHECK(request_queue_.front() == request);
  request_queue_.pop();
  request->callback.Run(result);
  ScopedFlush flush(gl_);
  if (request->query != 0) {
    gl_->DeleteQueriesEXT(1, &request->query);
    request->query = 0;
  }
  if (request->buffer != 0) {
    gl_->DeleteBuffers(1, &request->buffer);
    request->buffer = 0;
  }
  delete request;
}

void GLHelper::CopyTextureToImpl::CancelRequests() {
  while (!request_queue_.empty()) {
    Request* request = request_queue_.front();
    FinishRequest(request, false);
  }
}

GLHelper::GLHelper(GLES2Interface* gl, gpu::ContextSupport* context_support)
    : gl_(gl),
      context_support_(context_support),
      readback_support_(new GLHelperReadbackSupport(gl)) {}

GLHelper::~GLHelper() {}

void GLHelper::CropScaleReadbackAndCleanTexture(
    GLuint src_texture,
    const gfx::Size& src_size,
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    unsigned char* out,
    const SkBitmap::Config config,
    const base::Callback<void(bool)>& callback,
    GLHelper::ScalerQuality quality) {
  InitCopyTextToImpl();
  copy_texture_to_impl_->CropScaleReadbackAndCleanTexture(
      src_texture,
      src_size,
      src_subrect,
      dst_size,
      out,
      config,
      callback,
      quality);
}

void GLHelper::CropScaleReadbackAndCleanMailbox(
    const gpu::Mailbox& src_mailbox,
    uint32 sync_point,
    const gfx::Size& src_size,
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    unsigned char* out,
    const SkBitmap::Config bitmap_config,
    const base::Callback<void(bool)>& callback,
    GLHelper::ScalerQuality quality) {
  GLuint mailbox_texture = ConsumeMailboxToTexture(src_mailbox, sync_point);
  CropScaleReadbackAndCleanTexture(
      mailbox_texture, src_size, src_subrect, dst_size, out,
      bitmap_config,
      callback,
      quality);
  gl_->DeleteTextures(1, &mailbox_texture);
}

void GLHelper::ReadbackTextureSync(GLuint texture,
                                   const gfx::Rect& src_rect,
                                   unsigned char* out,
                                   SkBitmap::Config format) {
  InitCopyTextToImpl();
  copy_texture_to_impl_->ReadbackTextureSync(texture, src_rect, out, format);
}

void GLHelper::ReadbackTextureAsync(
    GLuint texture,
    const gfx::Size& dst_size,
    unsigned char* out,
    SkBitmap::Config config,
    const base::Callback<void(bool)>& callback) {
  InitCopyTextToImpl();
  copy_texture_to_impl_->ReadbackTextureAsync(texture,
                                              dst_size,
                                              out,
                                              config,
                                              callback);
}

GLuint GLHelper::CopyTexture(GLuint texture, const gfx::Size& size) {
  InitCopyTextToImpl();
  return copy_texture_to_impl_->CopyAndScaleTexture(
      texture, size, size, false, GLHelper::SCALER_QUALITY_FAST);
}

GLuint GLHelper::CopyAndScaleTexture(GLuint texture,
                                     const gfx::Size& src_size,
                                     const gfx::Size& dst_size,
                                     bool vertically_flip_texture,
                                     ScalerQuality quality) {
  InitCopyTextToImpl();
  return copy_texture_to_impl_->CopyAndScaleTexture(
      texture, src_size, dst_size, vertically_flip_texture, quality);
}

GLuint GLHelper::CompileShaderFromSource(const GLchar* source, GLenum type) {
  GLuint shader = gl_->CreateShader(type);
  GLint length = strlen(source);
  gl_->ShaderSource(shader, 1, &source, &length);
  gl_->CompileShader(shader);
  GLint compile_status = 0;
  gl_->GetShaderiv(shader, GL_COMPILE_STATUS, &compile_status);
  if (!compile_status) {
    GLint log_length = 0;
    gl_->GetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
    if (log_length) {
      scoped_ptr<GLchar[]> log(new GLchar[log_length]);
      GLsizei returned_log_length = 0;
      gl_->GetShaderInfoLog(
          shader, log_length, &returned_log_length, log.get());
      LOG(ERROR) << std::string(log.get(), returned_log_length);
    }
    gl_->DeleteShader(shader);
    return 0;
  }
  return shader;
}

void GLHelper::InitCopyTextToImpl() {
  // Lazily initialize |copy_texture_to_impl_|
  if (!copy_texture_to_impl_)
    copy_texture_to_impl_.reset(
        new CopyTextureToImpl(gl_, context_support_, this));
}

void GLHelper::InitScalerImpl() {
  // Lazily initialize |scaler_impl_|
  if (!scaler_impl_)
    scaler_impl_.reset(new GLHelperScaling(gl_, this));
}

GLint GLHelper::MaxDrawBuffers() {
  InitCopyTextToImpl();
  return copy_texture_to_impl_->MaxDrawBuffers();
}

void GLHelper::CopySubBufferDamage(GLuint texture,
                                   GLuint previous_texture,
                                   const SkRegion& new_damage,
                                   const SkRegion& old_damage) {
  SkRegion region(old_damage);
  if (region.op(new_damage, SkRegion::kDifference_Op)) {
    ScopedFramebuffer dst_framebuffer(gl_);
    ScopedFramebufferBinder<GL_FRAMEBUFFER> framebuffer_binder(gl_,
                                                               dst_framebuffer);
    ScopedTextureBinder<GL_TEXTURE_2D> texture_binder(gl_, texture);
    gl_->FramebufferTexture2D(GL_FRAMEBUFFER,
                              GL_COLOR_ATTACHMENT0,
                              GL_TEXTURE_2D,
                              previous_texture,
                              0);
    for (SkRegion::Iterator it(region); !it.done(); it.next()) {
      const SkIRect& rect = it.rect();
      gl_->CopyTexSubImage2D(GL_TEXTURE_2D,
                             0,
                             rect.x(),
                             rect.y(),
                             rect.x(),
                             rect.y(),
                             rect.width(),
                             rect.height());
    }
    gl_->Flush();
  }
}

GLuint GLHelper::CreateTexture() {
  GLuint texture = 0u;
  gl_->GenTextures(1, &texture);
  content::ScopedTextureBinder<GL_TEXTURE_2D> texture_binder(gl_, texture);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  return texture;
}

void GLHelper::DeleteTexture(GLuint texture_id) {
  gl_->DeleteTextures(1, &texture_id);
}

uint32 GLHelper::InsertSyncPoint() { return gl_->InsertSyncPointCHROMIUM(); }

void GLHelper::WaitSyncPoint(uint32 sync_point) {
  gl_->WaitSyncPointCHROMIUM(sync_point);
}

gpu::MailboxHolder GLHelper::ProduceMailboxHolderFromTexture(
    GLuint texture_id) {
  gpu::Mailbox mailbox;
  gl_->GenMailboxCHROMIUM(mailbox.name);
  content::ScopedTextureBinder<GL_TEXTURE_2D> texture_binder(gl_, texture_id);
  gl_->ProduceTextureCHROMIUM(GL_TEXTURE_2D, mailbox.name);
  return gpu::MailboxHolder(mailbox, GL_TEXTURE_2D, InsertSyncPoint());
}

GLuint GLHelper::ConsumeMailboxToTexture(const gpu::Mailbox& mailbox,
                                         uint32 sync_point) {
  if (mailbox.IsZero())
    return 0;
  if (sync_point)
    WaitSyncPoint(sync_point);
  GLuint texture = CreateTexture();
  content::ScopedTextureBinder<GL_TEXTURE_2D> texture_binder(gl_, texture);
  gl_->ConsumeTextureCHROMIUM(GL_TEXTURE_2D, mailbox.name);
  return texture;
}

void GLHelper::ResizeTexture(GLuint texture, const gfx::Size& size) {
  content::ScopedTextureBinder<GL_TEXTURE_2D> texture_binder(gl_, texture);
  gl_->TexImage2D(GL_TEXTURE_2D,
                  0,
                  GL_RGB,
                  size.width(),
                  size.height(),
                  0,
                  GL_RGB,
                  GL_UNSIGNED_BYTE,
                  NULL);
}

void GLHelper::CopyTextureSubImage(GLuint texture, const gfx::Rect& rect) {
  content::ScopedTextureBinder<GL_TEXTURE_2D> texture_binder(gl_, texture);
  gl_->CopyTexSubImage2D(GL_TEXTURE_2D,
                         0,
                         rect.x(),
                         rect.y(),
                         rect.x(),
                         rect.y(),
                         rect.width(),
                         rect.height());
}

void GLHelper::CopyTextureFullImage(GLuint texture, const gfx::Size& size) {
  content::ScopedTextureBinder<GL_TEXTURE_2D> texture_binder(gl_, texture);
  gl_->CopyTexImage2D(
      GL_TEXTURE_2D, 0, GL_RGB, 0, 0, size.width(), size.height(), 0);
}

void GLHelper::Flush() {
  gl_->Flush();
}

void GLHelper::CopyTextureToImpl::ReadbackPlane(
    TextureFrameBufferPair* source,
    const scoped_refptr<media::VideoFrame>& target,
    int plane,
    int size_shift,
    const gfx::Rect& dst_subrect,
    ReadbackSwizzle swizzle,
    const base::Callback<void(bool)>& callback) {
  gl_->BindFramebuffer(GL_FRAMEBUFFER, source->framebuffer());
  size_t offset = target->stride(plane) * (dst_subrect.y() >> size_shift) +
      (dst_subrect.x() >> size_shift);
  ReadbackAsync(source->size(),
                dst_subrect.width() >> size_shift,
                target->stride(plane),
                target->data(plane) + offset,
                SkBitmap::kARGB_8888_Config,
                swizzle,
                callback);
}

const float GLHelper::CopyTextureToImpl::kRGBtoYColorWeights[] = {
    0.257f, 0.504f, 0.098f, 0.0625f};
const float GLHelper::CopyTextureToImpl::kRGBtoUColorWeights[] = {
    -0.148f, -0.291f, 0.439f, 0.5f};
const float GLHelper::CopyTextureToImpl::kRGBtoVColorWeights[] = {
    0.439f, -0.368f, -0.071f, 0.5f};

// YUV readback constructors. Initiates the main scaler pipeline and
// one planar scaler for each of the Y, U and V planes.
GLHelper::CopyTextureToImpl::ReadbackYUVImpl::ReadbackYUVImpl(
    GLES2Interface* gl,
    CopyTextureToImpl* copy_impl,
    GLHelperScaling* scaler_impl,
    GLHelper::ScalerQuality quality,
    const gfx::Size& src_size,
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    const gfx::Rect& dst_subrect,
    bool flip_vertically,
    ReadbackSwizzle swizzle)
    : gl_(gl),
      copy_impl_(copy_impl),
      dst_size_(dst_size),
      dst_subrect_(dst_subrect),
      swizzle_(swizzle),
      scaler_(gl,
              scaler_impl->CreateScaler(quality,
                                        src_size,
                                        src_subrect,
                                        dst_subrect.size(),
                                        flip_vertically,
                                        false)),
      y_(gl,
         scaler_impl->CreatePlanarScaler(
             dst_subrect.size(),
             gfx::Rect(0,
                       0,
                       (dst_subrect.width() + 3) & ~3,
                       dst_subrect.height()),
             gfx::Size((dst_subrect.width() + 3) / 4, dst_subrect.height()),
             false,
             (swizzle == kSwizzleBGRA),
             kRGBtoYColorWeights)),
      u_(gl,
         scaler_impl->CreatePlanarScaler(
             dst_subrect.size(),
             gfx::Rect(0,
                       0,
                       (dst_subrect.width() + 7) & ~7,
                       (dst_subrect.height() + 1) & ~1),
             gfx::Size((dst_subrect.width() + 7) / 8,
                       (dst_subrect.height() + 1) / 2),
             false,
             (swizzle == kSwizzleBGRA),
             kRGBtoUColorWeights)),
      v_(gl,
         scaler_impl->CreatePlanarScaler(
             dst_subrect.size(),
             gfx::Rect(0,
                       0,
                       (dst_subrect.width() + 7) & ~7,
                       (dst_subrect.height() + 1) & ~1),
             gfx::Size((dst_subrect.width() + 7) / 8,
                       (dst_subrect.height() + 1) / 2),
             false,
             (swizzle == kSwizzleBGRA),
             kRGBtoVColorWeights)) {
  DCHECK(!(dst_size.width() & 1));
  DCHECK(!(dst_size.height() & 1));
  DCHECK(!(dst_subrect.width() & 1));
  DCHECK(!(dst_subrect.height() & 1));
  DCHECK(!(dst_subrect.x() & 1));
  DCHECK(!(dst_subrect.y() & 1));
}

static void CallbackKeepingVideoFrameAlive(
    scoped_refptr<media::VideoFrame> video_frame,
    const base::Callback<void(bool)>& callback,
    bool success) {
  callback.Run(success);
}

void GLHelper::CopyTextureToImpl::ReadbackYUVImpl::ReadbackYUV(
    const gpu::Mailbox& mailbox,
    uint32 sync_point,
    const scoped_refptr<media::VideoFrame>& target,
    const base::Callback<void(bool)>& callback) {
  GLuint mailbox_texture =
      copy_impl_->ConsumeMailboxToTexture(mailbox, sync_point);

  // Scale texture to right size.
  scaler_.Scale(mailbox_texture);
  gl_->DeleteTextures(1, &mailbox_texture);

  // Convert the scaled texture in to Y, U and V planes.
  y_.Scale(scaler_.texture());
  u_.Scale(scaler_.texture());
  v_.Scale(scaler_.texture());

  if (target->coded_size() != dst_size_) {
    DCHECK(target->coded_size() == dst_size_);
    LOG(ERROR) << "ReadbackYUV size error!";
    callback.Run(false);
    return;
  }

  // Read back planes, one at a time. Keep the video frame alive while doing the
  // readback.
  copy_impl_->ReadbackPlane(y_.texture_and_framebuffer(),
                            target,
                            media::VideoFrame::kYPlane,
                            0,
                            dst_subrect_,
                            swizzle_,
                            base::Bind(&nullcallback));
  copy_impl_->ReadbackPlane(u_.texture_and_framebuffer(),
                            target,
                            media::VideoFrame::kUPlane,
                            1,
                            dst_subrect_,
                            swizzle_,
                            base::Bind(&nullcallback));
  copy_impl_->ReadbackPlane(
      v_.texture_and_framebuffer(),
      target,
      media::VideoFrame::kVPlane,
      1,
      dst_subrect_,
      swizzle_,
      base::Bind(&CallbackKeepingVideoFrameAlive, target, callback));
  gl_->BindFramebuffer(GL_FRAMEBUFFER, 0);
  media::LetterboxYUV(target, dst_subrect_);
}

// YUV readback constructors. Initiates the main scaler pipeline and
// one planar scaler for each of the Y, U and V planes.
GLHelper::CopyTextureToImpl::ReadbackYUV_MRT::ReadbackYUV_MRT(
    GLES2Interface* gl,
    CopyTextureToImpl* copy_impl,
    GLHelperScaling* scaler_impl,
    GLHelper::ScalerQuality quality,
    const gfx::Size& src_size,
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    const gfx::Rect& dst_subrect,
    bool flip_vertically,
    ReadbackSwizzle swizzle)
    : gl_(gl),
      copy_impl_(copy_impl),
      dst_size_(dst_size),
      dst_subrect_(dst_subrect),
      quality_(quality),
      swizzle_(swizzle),
      scaler_(gl,
              scaler_impl->CreateScaler(quality,
                                        src_size,
                                        src_subrect,
                                        dst_subrect.size(),
                                        false,
                                        false)),
      pass1_shader_(scaler_impl->CreateYuvMrtShader(
          dst_subrect.size(),
          gfx::Rect(0, 0, (dst_subrect.width() + 3) & ~3, dst_subrect.height()),
          gfx::Size((dst_subrect.width() + 3) / 4, dst_subrect.height()),
          flip_vertically,
          (swizzle == kSwizzleBGRA),
          GLHelperScaling::SHADER_YUV_MRT_PASS1)),
      pass2_shader_(scaler_impl->CreateYuvMrtShader(
          gfx::Size((dst_subrect.width() + 3) / 4, dst_subrect.height()),
          gfx::Rect(0,
                    0,
                    (dst_subrect.width() + 7) / 8 * 2,
                    dst_subrect.height()),
          gfx::Size((dst_subrect.width() + 7) / 8,
                    (dst_subrect.height() + 1) / 2),
          false,
          (swizzle == kSwizzleBGRA),
          GLHelperScaling::SHADER_YUV_MRT_PASS2)),
      y_(gl, gfx::Size((dst_subrect.width() + 3) / 4, dst_subrect.height())),
      uv_(gl),
      u_(gl,
         gfx::Size((dst_subrect.width() + 7) / 8,
                   (dst_subrect.height() + 1) / 2)),
      v_(gl,
         gfx::Size((dst_subrect.width() + 7) / 8,
                   (dst_subrect.height() + 1) / 2)) {

  content::ScopedTextureBinder<GL_TEXTURE_2D> texture_binder(gl, uv_);
  gl->TexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGBA,
                 (dst_subrect.width() + 3) / 4,
                 dst_subrect.height(),
                 0,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 NULL);

  DCHECK(!(dst_size.width() & 1));
  DCHECK(!(dst_size.height() & 1));
  DCHECK(!(dst_subrect.width() & 1));
  DCHECK(!(dst_subrect.height() & 1));
  DCHECK(!(dst_subrect.x() & 1));
  DCHECK(!(dst_subrect.y() & 1));
}

void GLHelper::CopyTextureToImpl::ReadbackYUV_MRT::ReadbackYUV(
    const gpu::Mailbox& mailbox,
    uint32 sync_point,
    const scoped_refptr<media::VideoFrame>& target,
    const base::Callback<void(bool)>& callback) {
  GLuint mailbox_texture =
      copy_impl_->ConsumeMailboxToTexture(mailbox, sync_point);

  GLuint texture;
  if (quality_ == GLHelper::SCALER_QUALITY_FAST) {
    // Optimization: SCALER_QUALITY_FAST is just a single bilinear
    // pass, which pass1_shader_ can do just as well, so let's skip
    // the actual scaling in that case.
    texture = mailbox_texture;
  } else {
    // Scale texture to right size.
    scaler_.Scale(mailbox_texture);
    texture = scaler_.texture();
  }

  std::vector<GLuint> outputs(2);
  // Convert the scaled texture in to Y, U and V planes.
  outputs[0] = y_.texture();
  outputs[1] = uv_;
  pass1_shader_->Execute(texture, outputs);

  gl_->DeleteTextures(1, &mailbox_texture);

  outputs[0] = u_.texture();
  outputs[1] = v_.texture();
  pass2_shader_->Execute(uv_, outputs);

  if (target->coded_size() != dst_size_) {
    DCHECK(target->coded_size() == dst_size_);
    LOG(ERROR) << "ReadbackYUV size error!";
    callback.Run(false);
    return;
  }

  // Read back planes, one at a time.
  copy_impl_->ReadbackPlane(&y_,
                            target,
                            media::VideoFrame::kYPlane,
                            0,
                            dst_subrect_,
                            swizzle_,
                            base::Bind(&nullcallback));
  copy_impl_->ReadbackPlane(&u_,
                            target,
                            media::VideoFrame::kUPlane,
                            1,
                            dst_subrect_,
                            swizzle_,
                            base::Bind(&nullcallback));
  copy_impl_->ReadbackPlane(
      &v_,
      target,
      media::VideoFrame::kVPlane,
      1,
      dst_subrect_,
      swizzle_,
      base::Bind(&CallbackKeepingVideoFrameAlive, target, callback));
  gl_->BindFramebuffer(GL_FRAMEBUFFER, 0);
  media::LetterboxYUV(target, dst_subrect_);
}

bool GLHelper::IsReadbackConfigSupported(SkBitmap::Config texture_format) {
  DCHECK(readback_support_.get());
  return readback_support_.get()->IsReadbackConfigSupported(texture_format);
}

ReadbackYUVInterface* GLHelper::CopyTextureToImpl::CreateReadbackPipelineYUV(
    GLHelper::ScalerQuality quality,
    const gfx::Size& src_size,
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    const gfx::Rect& dst_subrect,
    bool flip_vertically,
    bool use_mrt) {
  helper_->InitScalerImpl();
  // Query preferred format for glReadPixels, if is is GL_BGRA then use that
  // and trigger the appropriate swizzle in the YUV shaders.
  GLint format = 0, type = 0;
  ReadbackSwizzle swizzle = kSwizzleNone;
  helper_->readback_support_.get()->GetAdditionalFormat(GL_RGBA,
                                                        GL_UNSIGNED_BYTE,
                                                        &format, &type);
  if (format == GL_BGRA_EXT && type == GL_UNSIGNED_BYTE)
    swizzle = kSwizzleBGRA;
  if (max_draw_buffers_ >= 2 && use_mrt) {
    return new ReadbackYUV_MRT(gl_,
                               this,
                               helper_->scaler_impl_.get(),
                               quality,
                               src_size,
                               src_subrect,
                               dst_size,
                               dst_subrect,
                               flip_vertically,
                               swizzle);
  }
  return new ReadbackYUVImpl(gl_,
                             this,
                             helper_->scaler_impl_.get(),
                             quality,
                             src_size,
                             src_subrect,
                             dst_size,
                             dst_subrect,
                             flip_vertically,
                             swizzle);
}

ReadbackYUVInterface* GLHelper::CreateReadbackPipelineYUV(
    ScalerQuality quality,
    const gfx::Size& src_size,
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    const gfx::Rect& dst_subrect,
    bool flip_vertically,
    bool use_mrt) {
  InitCopyTextToImpl();
  return copy_texture_to_impl_->CreateReadbackPipelineYUV(quality,
                                                          src_size,
                                                          src_subrect,
                                                          dst_size,
                                                          dst_subrect,
                                                          flip_vertically,
                                                          use_mrt);
}

}  // namespace content
