// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/display_compositor/gl_helper.h"

#include <stddef.h>
#include <stdint.h>

#include <queue>
#include <string>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "components/display_compositor/gl_helper_readback_support.h"
#include "components/display_compositor/gl_helper_scaling.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

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
    display_compositor::ScopedTextureBinder<GL_TEXTURE_2D> texture_binder(
        gl, texture_);
    gl->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.width(), size.height(), 0,
                   GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    display_compositor::ScopedFramebufferBinder<GL_FRAMEBUFFER>
        framebuffer_binder(gl, framebuffer_);
    gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             GL_TEXTURE_2D, texture_, 0);
  }

  GLuint texture() const { return texture_.id(); }
  GLuint framebuffer() const { return framebuffer_.id(); }
  gfx::Size size() const { return size_; }

 private:
  display_compositor::ScopedTexture texture_;
  display_compositor::ScopedFramebuffer framebuffer_;
  gfx::Size size_;

  DISALLOW_COPY_AND_ASSIGN(TextureFrameBufferPair);
};

// Helper class for holding a scaler, a texture for the output of that
// scaler and an associated frame buffer. This is inteded to be used
// when the output of a scaler is to be sent to a readback.
class ScalerHolder {
 public:
  ScalerHolder(GLES2Interface* gl,
               display_compositor::GLHelper::ScalerInterface* scaler)
      : texture_and_framebuffer_(gl, scaler->DstSize()), scaler_(scaler) {}

  void Scale(GLuint src_texture) {
    scaler_->Scale(src_texture, texture_and_framebuffer_.texture());
  }

  display_compositor::GLHelper::ScalerInterface* scaler() const {
    return scaler_.get();
  }
  TextureFrameBufferPair* texture_and_framebuffer() {
    return &texture_and_framebuffer_;
  }
  GLuint texture() const { return texture_and_framebuffer_.texture(); }

 private:
  TextureFrameBufferPair texture_and_framebuffer_;
  std::unique_ptr<display_compositor::GLHelper::ScalerInterface> scaler_;

  DISALLOW_COPY_AND_ASSIGN(ScalerHolder);
};

}  // namespace

namespace display_compositor {
typedef GLHelperReadbackSupport::FormatSupport FormatSupport;

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
                                 const gpu::SyncToken& sync_token) {
    return helper_->ConsumeMailboxToTexture(mailbox, sync_token);
  }

  void CropScaleReadbackAndCleanTexture(
      GLuint src_texture,
      const gfx::Size& src_size,
      const gfx::Rect& src_subrect,
      const gfx::Size& dst_size,
      unsigned char* out,
      const SkColorType out_color_type,
      const base::Callback<void(bool)>& callback,
      GLHelper::ScalerQuality quality);

  void ReadbackTextureSync(GLuint texture,
                           const gfx::Rect& src_rect,
                           unsigned char* out,
                           SkColorType format);

  void ReadbackTextureAsync(GLuint texture,
                            const gfx::Size& dst_size,
                            unsigned char* out,
                            SkColorType color_type,
                            const base::Callback<void(bool)>& callback);

  // Reads back bytes from the currently bound frame buffer.
  // Note that dst_size is specified in bytes, not pixels.
  void ReadbackAsync(
      const gfx::Size& dst_size,
      int32_t bytes_per_row,     // generally dst_size.width() * 4
      int32_t row_stride_bytes,  // generally dst_size.width() * 4
      unsigned char* out,
      GLenum format,
      GLenum type,
      size_t bytes_per_pixel,
      const base::Callback<void(bool)>& callback);

  void ReadbackPlane(TextureFrameBufferPair* source,
                     int row_stride_bytes,
                     unsigned char* data,
                     int size_shift,
                     const gfx::Rect& paste_rect,
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
      bool flip_vertically,
      bool use_mrt);

  // Returns the maximum number of draw buffers available,
  // 0 if GL_EXT_draw_buffers is not available.
  GLint MaxDrawBuffers() const { return max_draw_buffers_; }

  FormatSupport GetReadbackConfig(SkColorType color_type,
                                  bool can_swizzle,
                                  GLenum* format,
                                  GLenum* type,
                                  size_t* bytes_per_pixel);

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
            int32_t bytes_per_row_,
            int32_t row_stride_bytes_,
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
    bool result;
    gfx::Size size;
    int bytes_per_row;
    int row_stride_bytes;
    unsigned char* pixels;
    base::Callback<void(bool)> callback;
    GLuint buffer;
    GLuint query;
  };

  // We must take care to call the callbacks last, as they may
  // end up destroying the gl_helper and make *this invalid.
  // We stick the finished requests in a stack object that calls
  // the callbacks when it goes out of scope.
  class FinishRequestHelper {
   public:
    FinishRequestHelper() {}
    ~FinishRequestHelper() {
      while (!requests_.empty()) {
        Request* request = requests_.front();
        requests_.pop();
        request->callback.Run(request->result);
        delete request;
      }
    }
    void Add(Request* r) { requests_.push(r); }

   private:
    std::queue<Request*> requests_;
    DISALLOW_COPY_AND_ASSIGN(FinishRequestHelper);
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
                    bool flip_vertically,
                    ReadbackSwizzle swizzle);

    void ReadbackYUV(const gpu::Mailbox& mailbox,
                     const gpu::SyncToken& sync_token,
                     const gfx::Rect& target_visible_rect,
                     int y_plane_row_stride_bytes,
                     unsigned char* y_plane_data,
                     int u_plane_row_stride_bytes,
                     unsigned char* u_plane_data,
                     int v_plane_row_stride_bytes,
                     unsigned char* v_plane_data,
                     const gfx::Point& paste_location,
                     const base::Callback<void(bool)>& callback) override;

    ScalerInterface* scaler() override { return scaler_.scaler(); }

   private:
    GLES2Interface* gl_;
    CopyTextureToImpl* copy_impl_;
    gfx::Size dst_size_;
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
                    bool flip_vertically,
                    ReadbackSwizzle swizzle);

    void ReadbackYUV(const gpu::Mailbox& mailbox,
                     const gpu::SyncToken& sync_token,
                     const gfx::Rect& target_visible_rect,
                     int y_plane_row_stride_bytes,
                     unsigned char* y_plane_data,
                     int u_plane_row_stride_bytes,
                     unsigned char* u_plane_data,
                     int v_plane_row_stride_bytes,
                     unsigned char* v_plane_data,
                     const gfx::Point& paste_location,
                     const base::Callback<void(bool)>& callback) override;

    ScalerInterface* scaler() override { return scaler_.scaler(); }

   private:
    GLES2Interface* gl_;
    CopyTextureToImpl* copy_impl_;
    gfx::Size dst_size_;
    GLHelper::ScalerQuality quality_;
    ReadbackSwizzle swizzle_;
    ScalerHolder scaler_;
    std::unique_ptr<display_compositor::GLHelperScaling::ShaderInterface>
        pass1_shader_;
    std::unique_ptr<display_compositor::GLHelperScaling::ShaderInterface>
        pass2_shader_;
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
                      SkColorType color_type,
                      GLHelper::ScalerQuality quality);

  // Converts each four consecutive pixels of the source texture into one pixel
  // in the result texture with each pixel channel representing the grayscale
  // color of one of the four original pixels:
  // R1G1B1A1 R2G2B2A2 R3G3B3A3 R4G4B4A4 -> X1X2X3X4
  // The resulting texture is still an RGBA texture (which is ~4 times narrower
  // than the original). If rendered directly, it wouldn't show anything useful,
  // but the data in it can be used to construct a grayscale image.
  // |encoded_texture_size| is the exact size of the resulting RGBA texture. It
  // is equal to src_size.width()/4 rounded upwards. Some channels in the last
  // pixel ((-src_size.width()) % 4) to be exact) are padding and don't contain
  // useful data.
  // If swizzle is set to true, the transformed pixels are reordered:
  // R1G1B1A1 R2G2B2A2 R3G3B3A3 R4G4B4A4 -> X3X2X1X4.
  GLuint EncodeTextureAsGrayscale(GLuint src_texture,
                                  const gfx::Size& src_size,
                                  gfx::Size* const encoded_texture_size,
                                  bool vertically_flip_texture,
                                  bool swizzle);

  static void nullcallback(bool success) {}
  void ReadbackDone(Request* request, int bytes_per_pixel);
  void FinishRequest(Request* request,
                     bool result,
                     FinishRequestHelper* helper);
  void CancelRequests();

  static const float kRGBtoYColorWeights[];
  static const float kRGBtoUColorWeights[];
  static const float kRGBtoVColorWeights[];
  static const float kRGBtoGrayscaleColorWeights[];

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
  return scaler_impl_->CreateScaler(quality, src_size, src_subrect, dst_size,
                                    vertically_flip_texture, swizzle);
}

GLuint GLHelper::CopyTextureToImpl::ScaleTexture(
    GLuint src_texture,
    const gfx::Size& src_size,
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    bool vertically_flip_texture,
    bool swizzle,
    SkColorType color_type,
    GLHelper::ScalerQuality quality) {
  GLuint dst_texture = 0u;
  gl_->GenTextures(1, &dst_texture);
  {
    GLenum format = GL_RGBA, type = GL_UNSIGNED_BYTE;
    ScopedTextureBinder<GL_TEXTURE_2D> texture_binder(gl_, dst_texture);

    // Use GL_RGBA for destination/temporary texture unless we're working with
    // 16-bit data
    if (color_type == kRGB_565_SkColorType) {
      format = GL_RGB;
      type = GL_UNSIGNED_SHORT_5_6_5;
    }

    gl_->TexImage2D(GL_TEXTURE_2D, 0, format, dst_size.width(),
                    dst_size.height(), 0, format, type, NULL);
  }
  std::unique_ptr<ScalerInterface> scaler(
      helper_->CreateScaler(quality, src_size, src_subrect, dst_size,
                            vertically_flip_texture, swizzle));
  scaler->Scale(src_texture, dst_texture);
  return dst_texture;
}

GLuint GLHelper::CopyTextureToImpl::EncodeTextureAsGrayscale(
    GLuint src_texture,
    const gfx::Size& src_size,
    gfx::Size* const encoded_texture_size,
    bool vertically_flip_texture,
    bool swizzle) {
  GLuint dst_texture = 0u;
  gl_->GenTextures(1, &dst_texture);
  // The size of the encoded texture.
  *encoded_texture_size =
      gfx::Size((src_size.width() + 3) / 4, src_size.height());
  {
    ScopedTextureBinder<GL_TEXTURE_2D> texture_binder(gl_, dst_texture);
    gl_->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, encoded_texture_size->width(),
                    encoded_texture_size->height(), 0, GL_RGBA,
                    GL_UNSIGNED_BYTE, NULL);
  }

  helper_->InitScalerImpl();
  std::unique_ptr<ScalerInterface> grayscale_scaler(
      helper_->scaler_impl_.get()->CreatePlanarScaler(
          src_size,
          gfx::Rect(0, 0, (src_size.width() + 3) & ~3, src_size.height()),
          *encoded_texture_size, vertically_flip_texture, swizzle,
          kRGBtoGrayscaleColorWeights));
  grayscale_scaler->Scale(src_texture, dst_texture);
  return dst_texture;
}

void GLHelper::CopyTextureToImpl::ReadbackAsync(
    const gfx::Size& dst_size,
    int32_t bytes_per_row,
    int32_t row_stride_bytes,
    unsigned char* out,
    GLenum format,
    GLenum type,
    size_t bytes_per_pixel,
    const base::Callback<void(bool)>& callback) {
  TRACE_EVENT0("gpu.capture", "GLHelper::CopyTextureToImpl::ReadbackAsync");
  Request* request =
      new Request(dst_size, bytes_per_row, row_stride_bytes, out, callback);
  request_queue_.push(request);
  request->buffer = 0u;

  gl_->GenBuffers(1, &request->buffer);
  gl_->BindBuffer(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM, request->buffer);
  gl_->BufferData(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM,
                  bytes_per_pixel * dst_size.GetArea(), NULL, GL_STREAM_READ);

  request->query = 0u;
  gl_->GenQueriesEXT(1, &request->query);
  gl_->BeginQueryEXT(GL_ASYNC_PIXEL_PACK_COMPLETED_CHROMIUM, request->query);
  gl_->ReadPixels(0, 0, dst_size.width(), dst_size.height(), format, type,
                  NULL);
  gl_->EndQueryEXT(GL_ASYNC_PIXEL_PACK_COMPLETED_CHROMIUM);
  gl_->BindBuffer(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM, 0);
  context_support_->SignalQuery(
      request->query, base::Bind(&CopyTextureToImpl::ReadbackDone, AsWeakPtr(),
                                 request, bytes_per_pixel));
}

void GLHelper::CopyTextureToImpl::CropScaleReadbackAndCleanTexture(
    GLuint src_texture,
    const gfx::Size& src_size,
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    unsigned char* out,
    const SkColorType out_color_type,
    const base::Callback<void(bool)>& callback,
    GLHelper::ScalerQuality quality) {
  GLenum format, type;
  size_t bytes_per_pixel;
  SkColorType readback_color_type = out_color_type;
  // Single-component textures are not supported by all GPUs, so  we implement
  // kAlpha_8_SkColorType support here via a special encoding (see below) using
  // a 32-bit texture to represent an 8-bit image.
  // Thus we use generic 32-bit readback in this case.
  if (out_color_type == kAlpha_8_SkColorType) {
    readback_color_type = kRGBA_8888_SkColorType;
  }

  FormatSupport supported = GetReadbackConfig(readback_color_type, true,
                                              &format, &type, &bytes_per_pixel);

  if (supported == GLHelperReadbackSupport::NOT_SUPPORTED) {
    callback.Run(false);
    return;
  }

  GLuint texture = src_texture;

  // Scale texture if needed
  // Optimization: SCALER_QUALITY_FAST is just a single bilinear pass, which we
  // can do just as well in EncodeTextureAsGrayscale, which we will do if
  // out_color_type is kAlpha_8_SkColorType, so let's skip the scaling step
  // in that case.
  bool scale_texture = out_color_type != kAlpha_8_SkColorType ||
                       quality != GLHelper::SCALER_QUALITY_FAST;
  if (scale_texture) {
    // Don't swizzle during the scale step for kAlpha_8_SkColorType.
    // We will swizzle in the encode step below if needed.
    bool scale_swizzle = out_color_type == kAlpha_8_SkColorType
                             ? false
                             : supported == GLHelperReadbackSupport::SWIZZLE;
    texture = ScaleTexture(src_texture, src_size, src_subrect, dst_size, true,
                           scale_swizzle, out_color_type == kAlpha_8_SkColorType
                                              ? kN32_SkColorType
                                              : out_color_type,
                           quality);
    DCHECK(texture);
  }

  gfx::Size readback_texture_size = dst_size;
  // Encode texture to grayscale if needed.
  if (out_color_type == kAlpha_8_SkColorType) {
    // Do the vertical flip here if we haven't already done it when we scaled
    // the texture.
    bool encode_as_grayscale_vertical_flip = !scale_texture;
    // EncodeTextureAsGrayscale by default creates a texture which should be
    // read back as RGBA, so need to swizzle if the readback format is BGRA.
    bool encode_as_grayscale_swizzle = format == GL_BGRA_EXT;
    GLuint tmp_texture = EncodeTextureAsGrayscale(
        texture, dst_size, &readback_texture_size,
        encode_as_grayscale_vertical_flip, encode_as_grayscale_swizzle);
    // If the scaled texture was created - delete it
    if (scale_texture)
      gl_->DeleteTextures(1, &texture);
    texture = tmp_texture;
    DCHECK(texture);
  }

  // Readback the pixels of the resulting texture
  ScopedFramebuffer dst_framebuffer(gl_);
  ScopedFramebufferBinder<GL_FRAMEBUFFER> framebuffer_binder(gl_,
                                                             dst_framebuffer);
  ScopedTextureBinder<GL_TEXTURE_2D> texture_binder(gl_, texture);
  gl_->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                            texture, 0);

  int32_t bytes_per_row = out_color_type == kAlpha_8_SkColorType
                              ? dst_size.width()
                              : dst_size.width() * bytes_per_pixel;

  ReadbackAsync(readback_texture_size, bytes_per_row, bytes_per_row, out,
                format, type, bytes_per_pixel, callback);
  gl_->DeleteTextures(1, &texture);
}

void GLHelper::CopyTextureToImpl::ReadbackTextureSync(GLuint texture,
                                                      const gfx::Rect& src_rect,
                                                      unsigned char* out,
                                                      SkColorType color_type) {
  GLenum format, type;
  size_t bytes_per_pixel;
  FormatSupport supported =
      GetReadbackConfig(color_type, false, &format, &type, &bytes_per_pixel);
  if (supported == GLHelperReadbackSupport::NOT_SUPPORTED) {
    return;
  }

  ScopedFramebuffer dst_framebuffer(gl_);
  ScopedFramebufferBinder<GL_FRAMEBUFFER> framebuffer_binder(gl_,
                                                             dst_framebuffer);
  ScopedTextureBinder<GL_TEXTURE_2D> texture_binder(gl_, texture);
  gl_->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                            texture, 0);
  gl_->ReadPixels(src_rect.x(), src_rect.y(), src_rect.width(),
                  src_rect.height(), format, type, out);
}

void GLHelper::CopyTextureToImpl::ReadbackTextureAsync(
    GLuint texture,
    const gfx::Size& dst_size,
    unsigned char* out,
    SkColorType color_type,
    const base::Callback<void(bool)>& callback) {
  GLenum format, type;
  size_t bytes_per_pixel;
  FormatSupport supported =
      GetReadbackConfig(color_type, false, &format, &type, &bytes_per_pixel);
  if (supported == GLHelperReadbackSupport::NOT_SUPPORTED) {
    callback.Run(false);
    return;
  }

  ScopedFramebuffer dst_framebuffer(gl_);
  ScopedFramebufferBinder<GL_FRAMEBUFFER> framebuffer_binder(gl_,
                                                             dst_framebuffer);
  ScopedTextureBinder<GL_TEXTURE_2D> texture_binder(gl_, texture);
  gl_->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                            texture, 0);
  ReadbackAsync(dst_size, dst_size.width() * bytes_per_pixel,
                dst_size.width() * bytes_per_pixel, out, format, type,
                bytes_per_pixel, callback);
}

GLuint GLHelper::CopyTextureToImpl::CopyAndScaleTexture(
    GLuint src_texture,
    const gfx::Size& src_size,
    const gfx::Size& dst_size,
    bool vertically_flip_texture,
    GLHelper::ScalerQuality quality) {
  return ScaleTexture(src_texture, src_size, gfx::Rect(src_size), dst_size,
                      vertically_flip_texture, false,
                      kRGBA_8888_SkColorType,  // GL_RGBA
                      quality);
}

void GLHelper::CopyTextureToImpl::ReadbackDone(Request* finished_request,
                                               int bytes_per_pixel) {
  TRACE_EVENT0("gpu.capture",
               "GLHelper::CopyTextureToImpl::CheckReadbackFramebufferComplete");
  finished_request->done = true;

  FinishRequestHelper finish_request_helper;

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
    FinishRequest(request, result, &finish_request_helper);
  }
}

void GLHelper::CopyTextureToImpl::FinishRequest(
    Request* request,
    bool result,
    FinishRequestHelper* finish_request_helper) {
  TRACE_EVENT0("gpu.capture", "GLHelper::CopyTextureToImpl::FinishRequest");
  DCHECK(request_queue_.front() == request);
  request_queue_.pop();
  request->result = result;
  ScopedFlush flush(gl_);
  if (request->query != 0) {
    gl_->DeleteQueriesEXT(1, &request->query);
    request->query = 0;
  }
  if (request->buffer != 0) {
    gl_->DeleteBuffers(1, &request->buffer);
    request->buffer = 0;
  }
  finish_request_helper->Add(request);
}

void GLHelper::CopyTextureToImpl::CancelRequests() {
  FinishRequestHelper finish_request_helper;
  while (!request_queue_.empty()) {
    Request* request = request_queue_.front();
    FinishRequest(request, false, &finish_request_helper);
  }
}

FormatSupport GLHelper::CopyTextureToImpl::GetReadbackConfig(
    SkColorType color_type,
    bool can_swizzle,
    GLenum* format,
    GLenum* type,
    size_t* bytes_per_pixel) {
  return helper_->readback_support_->GetReadbackConfig(
      color_type, can_swizzle, format, type, bytes_per_pixel);
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
    const SkColorType out_color_type,
    const base::Callback<void(bool)>& callback,
    GLHelper::ScalerQuality quality) {
  InitCopyTextToImpl();
  copy_texture_to_impl_->CropScaleReadbackAndCleanTexture(
      src_texture, src_size, src_subrect, dst_size, out, out_color_type,
      callback, quality);
}

void GLHelper::CropScaleReadbackAndCleanMailbox(
    const gpu::Mailbox& src_mailbox,
    const gpu::SyncToken& sync_token,
    const gfx::Size& src_size,
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    unsigned char* out,
    const SkColorType out_color_type,
    const base::Callback<void(bool)>& callback,
    GLHelper::ScalerQuality quality) {
  GLuint mailbox_texture = ConsumeMailboxToTexture(src_mailbox, sync_token);
  CropScaleReadbackAndCleanTexture(mailbox_texture, src_size, src_subrect,
                                   dst_size, out, out_color_type, callback,
                                   quality);
  gl_->DeleteTextures(1, &mailbox_texture);
}

void GLHelper::ReadbackTextureSync(GLuint texture,
                                   const gfx::Rect& src_rect,
                                   unsigned char* out,
                                   SkColorType format) {
  InitCopyTextToImpl();
  copy_texture_to_impl_->ReadbackTextureSync(texture, src_rect, out, format);
}

void GLHelper::ReadbackTextureAsync(
    GLuint texture,
    const gfx::Size& dst_size,
    unsigned char* out,
    SkColorType color_type,
    const base::Callback<void(bool)>& callback) {
  InitCopyTextToImpl();
  copy_texture_to_impl_->ReadbackTextureAsync(texture, dst_size, out,
                                              color_type, callback);
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
      std::unique_ptr<GLchar[]> log(new GLchar[log_length]);
      GLsizei returned_log_length = 0;
      gl_->GetShaderInfoLog(shader, log_length, &returned_log_length,
                            log.get());
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

void GLHelper::CopySubBufferDamage(GLenum target,
                                   GLuint texture,
                                   GLuint previous_texture,
                                   const SkRegion& new_damage,
                                   const SkRegion& old_damage) {
  SkRegion region(old_damage);
  if (region.op(new_damage, SkRegion::kDifference_Op)) {
    ScopedFramebuffer dst_framebuffer(gl_);
    ScopedFramebufferBinder<GL_FRAMEBUFFER> framebuffer_binder(gl_,
                                                               dst_framebuffer);
    gl_->BindTexture(target, texture);
    gl_->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, target,
                              previous_texture, 0);
    for (SkRegion::Iterator it(region); !it.done(); it.next()) {
      const SkIRect& rect = it.rect();
      gl_->CopyTexSubImage2D(target, 0, rect.x(), rect.y(), rect.x(), rect.y(),
                             rect.width(), rect.height());
    }
    gl_->BindTexture(target, 0);
    gl_->Flush();
  }
}

GLuint GLHelper::CreateTexture() {
  GLuint texture = 0u;
  gl_->GenTextures(1, &texture);
  display_compositor::ScopedTextureBinder<GL_TEXTURE_2D> texture_binder(
      gl_, texture);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  return texture;
}

void GLHelper::DeleteTexture(GLuint texture_id) {
  gl_->DeleteTextures(1, &texture_id);
}

void GLHelper::GenerateSyncToken(gpu::SyncToken* sync_token) {
  const uint64_t fence_sync = gl_->InsertFenceSyncCHROMIUM();
  gl_->ShallowFlushCHROMIUM();
  gl_->GenSyncTokenCHROMIUM(fence_sync, sync_token->GetData());
}

void GLHelper::WaitSyncToken(const gpu::SyncToken& sync_token) {
  gl_->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
}

gpu::MailboxHolder GLHelper::ProduceMailboxHolderFromTexture(
    GLuint texture_id) {
  gpu::Mailbox mailbox;
  gl_->GenMailboxCHROMIUM(mailbox.name);
  gl_->ProduceTextureDirectCHROMIUM(texture_id, GL_TEXTURE_2D, mailbox.name);

  gpu::SyncToken sync_token;
  GenerateSyncToken(&sync_token);

  return gpu::MailboxHolder(mailbox, sync_token, GL_TEXTURE_2D);
}

GLuint GLHelper::ConsumeMailboxToTexture(const gpu::Mailbox& mailbox,
                                         const gpu::SyncToken& sync_token) {
  if (mailbox.IsZero())
    return 0;
  if (sync_token.HasData())
    WaitSyncToken(sync_token);
  GLuint texture =
      gl_->CreateAndConsumeTextureCHROMIUM(GL_TEXTURE_2D, mailbox.name);
  return texture;
}

void GLHelper::ResizeTexture(GLuint texture, const gfx::Size& size) {
  display_compositor::ScopedTextureBinder<GL_TEXTURE_2D> texture_binder(
      gl_, texture);
  gl_->TexImage2D(GL_TEXTURE_2D, 0, GL_RGB, size.width(), size.height(), 0,
                  GL_RGB, GL_UNSIGNED_BYTE, NULL);
}

void GLHelper::CopyTextureSubImage(GLuint texture, const gfx::Rect& rect) {
  display_compositor::ScopedTextureBinder<GL_TEXTURE_2D> texture_binder(
      gl_, texture);
  gl_->CopyTexSubImage2D(GL_TEXTURE_2D, 0, rect.x(), rect.y(), rect.x(),
                         rect.y(), rect.width(), rect.height());
}

void GLHelper::CopyTextureFullImage(GLuint texture, const gfx::Size& size) {
  display_compositor::ScopedTextureBinder<GL_TEXTURE_2D> texture_binder(
      gl_, texture);
  gl_->CopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, size.width(),
                      size.height(), 0);
}

void GLHelper::Flush() {
  gl_->Flush();
}

void GLHelper::InsertOrderingBarrier() {
  gl_->OrderingBarrierCHROMIUM();
}

void GLHelper::CopyTextureToImpl::ReadbackPlane(
    TextureFrameBufferPair* source,
    int row_stride_bytes,
    unsigned char* data,
    int size_shift,
    const gfx::Rect& paste_rect,
    ReadbackSwizzle swizzle,
    const base::Callback<void(bool)>& callback) {
  gl_->BindFramebuffer(GL_FRAMEBUFFER, source->framebuffer());
  const size_t offset = row_stride_bytes * (paste_rect.y() >> size_shift) +
                        (paste_rect.x() >> size_shift);
  ReadbackAsync(source->size(), paste_rect.width() >> size_shift,
                row_stride_bytes, data + offset,
                (swizzle == kSwizzleBGRA) ? GL_BGRA_EXT : GL_RGBA,
                GL_UNSIGNED_BYTE, 4, callback);
}

const float GLHelper::CopyTextureToImpl::kRGBtoYColorWeights[] = {
    0.257f, 0.504f, 0.098f, 0.0625f};
const float GLHelper::CopyTextureToImpl::kRGBtoUColorWeights[] = {
    -0.148f, -0.291f, 0.439f, 0.5f};
const float GLHelper::CopyTextureToImpl::kRGBtoVColorWeights[] = {
    0.439f, -0.368f, -0.071f, 0.5f};
const float GLHelper::CopyTextureToImpl::kRGBtoGrayscaleColorWeights[] = {
    0.213f, 0.715f, 0.072f, 0.0f};

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
    bool flip_vertically,
    ReadbackSwizzle swizzle)
    : gl_(gl),
      copy_impl_(copy_impl),
      dst_size_(dst_size),
      swizzle_(swizzle),
      scaler_(gl,
              scaler_impl->CreateScaler(quality,
                                        src_size,
                                        src_subrect,
                                        dst_size,
                                        flip_vertically,
                                        false)),
      y_(gl,
         scaler_impl->CreatePlanarScaler(
             dst_size,
             gfx::Rect(0, 0, (dst_size.width() + 3) & ~3, dst_size.height()),
             gfx::Size((dst_size.width() + 3) / 4, dst_size.height()),
             false,
             (swizzle == kSwizzleBGRA),
             kRGBtoYColorWeights)),
      u_(gl,
         scaler_impl->CreatePlanarScaler(
             dst_size,
             gfx::Rect(0,
                       0,
                       (dst_size.width() + 7) & ~7,
                       (dst_size.height() + 1) & ~1),
             gfx::Size((dst_size.width() + 7) / 8, (dst_size.height() + 1) / 2),
             false,
             (swizzle == kSwizzleBGRA),
             kRGBtoUColorWeights)),
      v_(gl,
         scaler_impl->CreatePlanarScaler(
             dst_size,
             gfx::Rect(0,
                       0,
                       (dst_size.width() + 7) & ~7,
                       (dst_size.height() + 1) & ~1),
             gfx::Size((dst_size.width() + 7) / 8, (dst_size.height() + 1) / 2),
             false,
             (swizzle == kSwizzleBGRA),
             kRGBtoVColorWeights)) {
  DCHECK(!(dst_size.width() & 1));
  DCHECK(!(dst_size.height() & 1));
}

void GLHelper::CopyTextureToImpl::ReadbackYUVImpl::ReadbackYUV(
    const gpu::Mailbox& mailbox,
    const gpu::SyncToken& sync_token,
    const gfx::Rect& target_visible_rect,
    int y_plane_row_stride_bytes,
    unsigned char* y_plane_data,
    int u_plane_row_stride_bytes,
    unsigned char* u_plane_data,
    int v_plane_row_stride_bytes,
    unsigned char* v_plane_data,
    const gfx::Point& paste_location,
    const base::Callback<void(bool)>& callback) {
  DCHECK(!(paste_location.x() & 1));
  DCHECK(!(paste_location.y() & 1));

  GLuint mailbox_texture =
      copy_impl_->ConsumeMailboxToTexture(mailbox, sync_token);

  // Scale texture to right size.
  scaler_.Scale(mailbox_texture);
  gl_->DeleteTextures(1, &mailbox_texture);

  // Convert the scaled texture in to Y, U and V planes.
  y_.Scale(scaler_.texture());
  u_.Scale(scaler_.texture());
  v_.Scale(scaler_.texture());

  const gfx::Rect paste_rect(paste_location, dst_size_);
  if (!target_visible_rect.Contains(paste_rect)) {
    LOG(DFATAL) << "Paste rect not inside VideoFrame's visible rect!";
    callback.Run(false);
    return;
  }

  // Read back planes, one at a time. Keep the video frame alive while doing the
  // readback.
  copy_impl_->ReadbackPlane(y_.texture_and_framebuffer(),
                            y_plane_row_stride_bytes, y_plane_data, 0,
                            paste_rect, swizzle_, base::Bind(&nullcallback));
  copy_impl_->ReadbackPlane(u_.texture_and_framebuffer(),
                            u_plane_row_stride_bytes, u_plane_data, 1,
                            paste_rect, swizzle_, base::Bind(&nullcallback));
  copy_impl_->ReadbackPlane(v_.texture_and_framebuffer(),
                            v_plane_row_stride_bytes, v_plane_data, 1,
                            paste_rect, swizzle_, callback);
  gl_->BindFramebuffer(GL_FRAMEBUFFER, 0);
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
    bool flip_vertically,
    ReadbackSwizzle swizzle)
    : gl_(gl),
      copy_impl_(copy_impl),
      dst_size_(dst_size),
      quality_(quality),
      swizzle_(swizzle),
      scaler_(gl,
              scaler_impl->CreateScaler(quality,
                                        src_size,
                                        src_subrect,
                                        dst_size,
                                        false,
                                        false)),
      pass1_shader_(scaler_impl->CreateYuvMrtShader(
          dst_size,
          gfx::Rect(0, 0, (dst_size.width() + 3) & ~3, dst_size.height()),
          gfx::Size((dst_size.width() + 3) / 4, dst_size.height()),
          flip_vertically,
          (swizzle == kSwizzleBGRA),
          GLHelperScaling::SHADER_YUV_MRT_PASS1)),
      pass2_shader_(scaler_impl->CreateYuvMrtShader(
          gfx::Size((dst_size.width() + 3) / 4, dst_size.height()),
          gfx::Rect(0, 0, (dst_size.width() + 7) / 8 * 2, dst_size.height()),
          gfx::Size((dst_size.width() + 7) / 8, (dst_size.height() + 1) / 2),
          false,
          (swizzle == kSwizzleBGRA),
          GLHelperScaling::SHADER_YUV_MRT_PASS2)),
      y_(gl, gfx::Size((dst_size.width() + 3) / 4, dst_size.height())),
      uv_(gl),
      u_(gl,
         gfx::Size((dst_size.width() + 7) / 8, (dst_size.height() + 1) / 2)),
      v_(gl,
         gfx::Size((dst_size.width() + 7) / 8, (dst_size.height() + 1) / 2)) {
  DCHECK(!(dst_size.width() & 1));
  DCHECK(!(dst_size.height() & 1));

  display_compositor::ScopedTextureBinder<GL_TEXTURE_2D> texture_binder(gl,
                                                                        uv_);
  gl->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (dst_size.width() + 3) / 4,
                 dst_size.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
}

void GLHelper::CopyTextureToImpl::ReadbackYUV_MRT::ReadbackYUV(
    const gpu::Mailbox& mailbox,
    const gpu::SyncToken& sync_token,
    const gfx::Rect& target_visible_rect,
    int y_plane_row_stride_bytes,
    unsigned char* y_plane_data,
    int u_plane_row_stride_bytes,
    unsigned char* u_plane_data,
    int v_plane_row_stride_bytes,
    unsigned char* v_plane_data,
    const gfx::Point& paste_location,
    const base::Callback<void(bool)>& callback) {
  DCHECK(!(paste_location.x() & 1));
  DCHECK(!(paste_location.y() & 1));

  GLuint mailbox_texture =
      copy_impl_->ConsumeMailboxToTexture(mailbox, sync_token);

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

  const gfx::Rect paste_rect(paste_location, dst_size_);
  if (!target_visible_rect.Contains(paste_rect)) {
    LOG(DFATAL) << "Paste rect not inside VideoFrame's visible rect!";
    callback.Run(false);
    return;
  }

  // Read back planes, one at a time.
  copy_impl_->ReadbackPlane(&y_, y_plane_row_stride_bytes, y_plane_data, 0,
                            paste_rect, swizzle_, base::Bind(&nullcallback));
  copy_impl_->ReadbackPlane(&u_, u_plane_row_stride_bytes, u_plane_data, 1,
                            paste_rect, swizzle_, base::Bind(&nullcallback));
  copy_impl_->ReadbackPlane(&v_, v_plane_row_stride_bytes, v_plane_data, 1,
                            paste_rect, swizzle_, callback);
  gl_->BindFramebuffer(GL_FRAMEBUFFER, 0);
}

bool GLHelper::IsReadbackConfigSupported(SkColorType color_type) {
  DCHECK(readback_support_.get());
  GLenum format, type;
  size_t bytes_per_pixel;
  FormatSupport support = readback_support_->GetReadbackConfig(
      color_type, false, &format, &type, &bytes_per_pixel);

  return (support == GLHelperReadbackSupport::SUPPORTED);
}

ReadbackYUVInterface* GLHelper::CopyTextureToImpl::CreateReadbackPipelineYUV(
    GLHelper::ScalerQuality quality,
    const gfx::Size& src_size,
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    bool flip_vertically,
    bool use_mrt) {
  helper_->InitScalerImpl();
  // Just query if the best readback configuration needs a swizzle In
  // ReadbackPlane() we will choose GL_RGBA/GL_BGRA_EXT based on swizzle
  GLenum format, type;
  size_t bytes_per_pixel;
  FormatSupport supported = GetReadbackConfig(kRGBA_8888_SkColorType, true,
                                              &format, &type, &bytes_per_pixel);
  DCHECK((format == GL_RGBA || format == GL_BGRA_EXT) &&
         type == GL_UNSIGNED_BYTE);

  ReadbackSwizzle swizzle = kSwizzleNone;
  if (supported == GLHelperReadbackSupport::SWIZZLE)
    swizzle = kSwizzleBGRA;

  if (max_draw_buffers_ >= 2 && use_mrt) {
    return new ReadbackYUV_MRT(gl_, this, helper_->scaler_impl_.get(), quality,
                               src_size, src_subrect, dst_size, flip_vertically,
                               swizzle);
  }
  return new ReadbackYUVImpl(gl_, this, helper_->scaler_impl_.get(), quality,
                             src_size, src_subrect, dst_size, flip_vertically,
                             swizzle);
}

ReadbackYUVInterface* GLHelper::CreateReadbackPipelineYUV(
    ScalerQuality quality,
    const gfx::Size& src_size,
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    bool flip_vertically,
    bool use_mrt) {
  InitCopyTextToImpl();
  return copy_texture_to_impl_->CreateReadbackPipelineYUV(
      quality, src_size, src_subrect, dst_size, flip_vertically, use_mrt);
}

}  // namespace display_compositor
