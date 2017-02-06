// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/dxva_picture_buffer_win.h"

#include "media/gpu/dxva_video_decode_accelerator_win.h"
#include "third_party/angle/include/EGL/egl.h"
#include "third_party/angle/include/EGL/eglext.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_fence.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/scoped_binders.h"

namespace media {

#define RETURN_ON_FAILURE(result, log, ret) \
  do {                                      \
    if (!(result)) {                        \
      DLOG(ERROR) << log;                   \
      return ret;                           \
    }                                       \
  } while (0)

#define RETURN_ON_HR_FAILURE(result, log, ret) \
  RETURN_ON_FAILURE(SUCCEEDED(result),         \
                    log << ", HRESULT: 0x" << std::hex << result, ret);

enum {
  // The keyed mutex should always be released before the other thread
  // attempts to acquire it, so AcquireSync should always return immediately.
  kAcquireSyncWaitMs = 0,
};

// static
linked_ptr<DXVAPictureBuffer> DXVAPictureBuffer::Create(
    const DXVAVideoDecodeAccelerator& decoder,
    const PictureBuffer& buffer,
    EGLConfig egl_config) {
  if (decoder.share_nv12_textures_) {
    linked_ptr<EGLStreamPictureBuffer> picture_buffer(
        new EGLStreamPictureBuffer(buffer));
    if (!picture_buffer->Initialize())
      return linked_ptr<DXVAPictureBuffer>(nullptr);

    return picture_buffer;
  }
  if (decoder.copy_nv12_textures_) {
    linked_ptr<EGLStreamCopyPictureBuffer> picture_buffer(
        new EGLStreamCopyPictureBuffer(buffer));
    if (!picture_buffer->Initialize(decoder))
      return linked_ptr<DXVAPictureBuffer>(nullptr);

    return picture_buffer;
  }
  linked_ptr<PbufferPictureBuffer> picture_buffer(
      new PbufferPictureBuffer(buffer));

  if (!picture_buffer->Initialize(decoder, egl_config))
    return linked_ptr<DXVAPictureBuffer>(nullptr);

  return picture_buffer;
}

DXVAPictureBuffer::~DXVAPictureBuffer() {}

void DXVAPictureBuffer::ResetReuseFence() {
  NOTREACHED();
}

bool DXVAPictureBuffer::CopyOutputSampleDataToPictureBuffer(
    DXVAVideoDecodeAccelerator* decoder,
    IDirect3DSurface9* dest_surface,
    ID3D11Texture2D* dx11_texture,
    int input_buffer_id) {
  NOTREACHED();
  return false;
}

bool DXVAPictureBuffer::waiting_to_reuse() const {
  return false;
}

gl::GLFence* DXVAPictureBuffer::reuse_fence() {
  return nullptr;
}

bool DXVAPictureBuffer::CopySurfaceComplete(IDirect3DSurface9* src_surface,
                                            IDirect3DSurface9* dest_surface) {
  NOTREACHED();
  return false;
}

DXVAPictureBuffer::DXVAPictureBuffer(const PictureBuffer& buffer)
    : available_(true), picture_buffer_(buffer) {}

bool DXVAPictureBuffer::BindSampleToTexture(
    base::win::ScopedComPtr<IMFSample> sample) {
  NOTREACHED();
  return false;
}

bool PbufferPictureBuffer::Initialize(const DXVAVideoDecodeAccelerator& decoder,
                                      EGLConfig egl_config) {
  EGLDisplay egl_display = gl::GLSurfaceEGL::GetHardwareDisplay();
  EGLint use_rgb = 1;
  eglGetConfigAttrib(egl_display, egl_config, EGL_BIND_TO_TEXTURE_RGB,
                     &use_rgb);

  if (!InitializeTexture(decoder, !!use_rgb))
    return false;

  EGLint attrib_list[] = {EGL_WIDTH,
                          size().width(),
                          EGL_HEIGHT,
                          size().height(),
                          EGL_TEXTURE_FORMAT,
                          use_rgb ? EGL_TEXTURE_RGB : EGL_TEXTURE_RGBA,
                          EGL_TEXTURE_TARGET,
                          EGL_TEXTURE_2D,
                          EGL_NONE};

  decoding_surface_ = eglCreatePbufferFromClientBuffer(
      egl_display, EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE, texture_share_handle_,
      egl_config, attrib_list);
  RETURN_ON_FAILURE(decoding_surface_, "Failed to create surface", false);
  if (decoder.d3d11_device_ && decoder.use_keyed_mutex_) {
    void* keyed_mutex = nullptr;
    EGLBoolean ret =
        eglQuerySurfacePointerANGLE(egl_display, decoding_surface_,
                                    EGL_DXGI_KEYED_MUTEX_ANGLE, &keyed_mutex);
    RETURN_ON_FAILURE(keyed_mutex && ret == EGL_TRUE,
                      "Failed to query ANGLE keyed mutex", false);
    egl_keyed_mutex_ = base::win::ScopedComPtr<IDXGIKeyedMutex>(
        static_cast<IDXGIKeyedMutex*>(keyed_mutex));
  }
  use_rgb_ = !!use_rgb;
  return true;
}

bool PbufferPictureBuffer::InitializeTexture(
    const DXVAVideoDecodeAccelerator& decoder,
    bool use_rgb) {
  DCHECK(!texture_share_handle_);
  if (decoder.d3d11_device_) {
    D3D11_TEXTURE2D_DESC desc;
    desc.Width = picture_buffer_.size().width();
    desc.Height = picture_buffer_.size().height();
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = decoder.use_keyed_mutex_
                         ? D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX
                         : D3D11_RESOURCE_MISC_SHARED;

    HRESULT hr = decoder.d3d11_device_->CreateTexture2D(
        &desc, nullptr, dx11_decoding_texture_.Receive());
    RETURN_ON_HR_FAILURE(hr, "Failed to create texture", false);
    if (decoder.use_keyed_mutex_) {
      hr = dx11_keyed_mutex_.QueryFrom(dx11_decoding_texture_.get());
      RETURN_ON_HR_FAILURE(hr, "Failed to get keyed mutex", false);
    }

    base::win::ScopedComPtr<IDXGIResource> resource;
    hr = resource.QueryFrom(dx11_decoding_texture_.get());
    DCHECK(SUCCEEDED(hr));
    hr = resource->GetSharedHandle(&texture_share_handle_);
    RETURN_ON_FAILURE(SUCCEEDED(hr) && texture_share_handle_,
                      "Failed to query shared handle", false);

  } else {
    HRESULT hr = E_FAIL;
    hr = decoder.d3d9_device_ex_->CreateTexture(
        picture_buffer_.size().width(), picture_buffer_.size().height(), 1,
        D3DUSAGE_RENDERTARGET, use_rgb ? D3DFMT_X8R8G8B8 : D3DFMT_A8R8G8B8,
        D3DPOOL_DEFAULT, decoding_texture_.Receive(), &texture_share_handle_);
    RETURN_ON_HR_FAILURE(hr, "Failed to create texture", false);
    RETURN_ON_FAILURE(texture_share_handle_, "Failed to query shared handle",
                      false);
  }
  return true;
}

void PbufferPictureBuffer::ResetReuseFence() {
  if (!reuse_fence_ || !reuse_fence_->ResetSupported())
    reuse_fence_.reset(gl::GLFence::Create());
  else
    reuse_fence_->ResetState();
  waiting_to_reuse_ = true;
}

bool PbufferPictureBuffer::CopyOutputSampleDataToPictureBuffer(
    DXVAVideoDecodeAccelerator* decoder,
    IDirect3DSurface9* dest_surface,
    ID3D11Texture2D* dx11_texture,
    int input_buffer_id) {
  DCHECK(dest_surface || dx11_texture);
  if (dx11_texture) {
    // Grab a reference on the decoder texture. This reference will be released
    // when we receive a notification that the copy was completed or when the
    // DXVAPictureBuffer instance is destroyed.
    decoder_dx11_texture_ = dx11_texture;
    decoder->CopyTexture(dx11_texture, dx11_decoding_texture_.get(),
                         dx11_keyed_mutex_, keyed_mutex_value_, id(),
                         input_buffer_id);
    return true;
  }
  D3DSURFACE_DESC surface_desc;
  HRESULT hr = dest_surface->GetDesc(&surface_desc);
  RETURN_ON_HR_FAILURE(hr, "Failed to get surface description", false);

  D3DSURFACE_DESC texture_desc;
  decoding_texture_->GetLevelDesc(0, &texture_desc);

  if (texture_desc.Width != surface_desc.Width ||
      texture_desc.Height != surface_desc.Height) {
    NOTREACHED() << "Decode surface of different dimension than texture";
    return false;
  }

  hr = decoder->d3d9_->CheckDeviceFormatConversion(
      D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, surface_desc.Format,
      use_rgb_ ? D3DFMT_X8R8G8B8 : D3DFMT_A8R8G8B8);
  RETURN_ON_HR_FAILURE(hr, "Device does not support format converision", false);

  // The same picture buffer can be reused for a different frame. Release the
  // target surface and the decoder references here.
  target_surface_.Release();
  decoder_surface_.Release();

  // Grab a reference on the decoder surface and the target surface. These
  // references will be released when we receive a notification that the
  // copy was completed or when the DXVAPictureBuffer instance is destroyed.
  // We hold references here as it is easier to manage their lifetimes.
  hr = decoding_texture_->GetSurfaceLevel(0, target_surface_.Receive());
  RETURN_ON_HR_FAILURE(hr, "Failed to get surface from texture", false);

  decoder_surface_ = dest_surface;

  decoder->CopySurface(decoder_surface_.get(), target_surface_.get(), id(),
                       input_buffer_id);
  return true;
}

bool PbufferPictureBuffer::waiting_to_reuse() const {
  return waiting_to_reuse_;
}

gl::GLFence* PbufferPictureBuffer::reuse_fence() {
  return reuse_fence_.get();
}

bool PbufferPictureBuffer::CopySurfaceComplete(
    IDirect3DSurface9* src_surface,
    IDirect3DSurface9* dest_surface) {
  DCHECK(!available());

  GLint current_texture = 0;
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &current_texture);

  glBindTexture(GL_TEXTURE_2D, picture_buffer_.texture_ids()[0]);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

  if (src_surface && dest_surface) {
    DCHECK_EQ(src_surface, decoder_surface_.get());
    DCHECK_EQ(dest_surface, target_surface_.get());
    decoder_surface_.Release();
    target_surface_.Release();
  } else {
    DCHECK(decoder_dx11_texture_.get());
    decoder_dx11_texture_.Release();
  }
  if (egl_keyed_mutex_) {
    keyed_mutex_value_++;
    HRESULT result =
        egl_keyed_mutex_->AcquireSync(keyed_mutex_value_, kAcquireSyncWaitMs);
    RETURN_ON_FAILURE(result == S_OK, "Could not acquire sync mutex", false);
  }

  EGLDisplay egl_display = gl::GLSurfaceEGL::GetHardwareDisplay();
  eglBindTexImage(egl_display, decoding_surface_, EGL_BACK_BUFFER);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glBindTexture(GL_TEXTURE_2D, current_texture);
  return true;
}

PbufferPictureBuffer::PbufferPictureBuffer(const PictureBuffer& buffer)
    : DXVAPictureBuffer(buffer),
      waiting_to_reuse_(false),
      decoding_surface_(NULL),
      texture_share_handle_(nullptr),
      keyed_mutex_value_(0),
      use_rgb_(true) {}

PbufferPictureBuffer::~PbufferPictureBuffer() {
  if (decoding_surface_) {
    EGLDisplay egl_display = gl::GLSurfaceEGL::GetHardwareDisplay();

    eglReleaseTexImage(egl_display, decoding_surface_, EGL_BACK_BUFFER);

    eglDestroySurface(egl_display, decoding_surface_);
    decoding_surface_ = NULL;
  }
}

bool PbufferPictureBuffer::ReusePictureBuffer() {
  DCHECK(decoding_surface_);
  EGLDisplay egl_display = gl::GLSurfaceEGL::GetHardwareDisplay();
  eglReleaseTexImage(egl_display, decoding_surface_, EGL_BACK_BUFFER);

  decoder_surface_.Release();
  target_surface_.Release();
  decoder_dx11_texture_.Release();
  waiting_to_reuse_ = false;
  set_available(true);
  if (egl_keyed_mutex_) {
    HRESULT hr = egl_keyed_mutex_->ReleaseSync(++keyed_mutex_value_);
    RETURN_ON_FAILURE(hr == S_OK, "Could not release sync mutex", false);
  }
  return true;
}

EGLStreamPictureBuffer::EGLStreamPictureBuffer(const PictureBuffer& buffer)
    : DXVAPictureBuffer(buffer), stream_(nullptr) {}

EGLStreamPictureBuffer::~EGLStreamPictureBuffer() {
  if (stream_) {
    EGLDisplay egl_display = gl::GLSurfaceEGL::GetHardwareDisplay();
    eglDestroyStreamKHR(egl_display, stream_);
    stream_ = nullptr;
  }
}

bool EGLStreamPictureBuffer::Initialize() {
  EGLDisplay egl_display = gl::GLSurfaceEGL::GetHardwareDisplay();
  const EGLint stream_attributes[] = {
      EGL_CONSUMER_LATENCY_USEC_KHR,
      0,
      EGL_CONSUMER_ACQUIRE_TIMEOUT_USEC_KHR,
      0,
      EGL_NONE,
  };
  stream_ = eglCreateStreamKHR(egl_display, stream_attributes);
  RETURN_ON_FAILURE(!!stream_, "Could not create stream", false);
  gl::ScopedActiveTexture texture0(GL_TEXTURE0);
  gl::ScopedTextureBinder texture0_binder(GL_TEXTURE_EXTERNAL_OES,
                                          picture_buffer_.texture_ids()[0]);
  gl::ScopedActiveTexture texture1(GL_TEXTURE1);
  gl::ScopedTextureBinder texture1_binder(GL_TEXTURE_EXTERNAL_OES,
                                          picture_buffer_.texture_ids()[1]);

  EGLAttrib consumer_attributes[] = {
      EGL_COLOR_BUFFER_TYPE,
      EGL_YUV_BUFFER_EXT,
      EGL_YUV_NUMBER_OF_PLANES_EXT,
      2,
      EGL_YUV_PLANE0_TEXTURE_UNIT_NV,
      0,
      EGL_YUV_PLANE1_TEXTURE_UNIT_NV,
      1,
      EGL_NONE,
  };
  EGLBoolean result = eglStreamConsumerGLTextureExternalAttribsNV(
      egl_display, stream_, consumer_attributes);
  RETURN_ON_FAILURE(result, "Could not set stream consumer", false);

  EGLAttrib producer_attributes[] = {
      EGL_NONE,
  };

  result = eglCreateStreamProducerD3DTextureNV12ANGLE(egl_display, stream_,
                                                      producer_attributes);
  RETURN_ON_FAILURE(result, "Could not create stream producer", false);
  return true;
}

bool EGLStreamPictureBuffer::ReusePictureBuffer() {
  EGLDisplay egl_display = gl::GLSurfaceEGL::GetHardwareDisplay();

  if (stream_) {
    EGLBoolean result = eglStreamConsumerReleaseKHR(egl_display, stream_);
    RETURN_ON_FAILURE(result, "Could not release stream", false);
  }
  if (current_d3d_sample_) {
    dx11_decoding_texture_.Release();
    current_d3d_sample_.Release();
  }
  set_available(true);
  return true;
}

bool EGLStreamPictureBuffer::BindSampleToTexture(
    base::win::ScopedComPtr<IMFSample> sample) {
  DCHECK(!available());

  current_d3d_sample_ = sample;
  EGLDisplay egl_display = gl::GLSurfaceEGL::GetHardwareDisplay();

  base::win::ScopedComPtr<IMFMediaBuffer> output_buffer;
  HRESULT hr =
      current_d3d_sample_->GetBufferByIndex(0, output_buffer.Receive());
  RETURN_ON_HR_FAILURE(hr, "Failed to get buffer from output sample", false);

  base::win::ScopedComPtr<IMFDXGIBuffer> dxgi_buffer;
  hr = dxgi_buffer.QueryFrom(output_buffer.get());
  RETURN_ON_HR_FAILURE(hr, "Failed to get DXGIBuffer from output sample",
                       false);
  hr = dxgi_buffer->GetResource(IID_PPV_ARGS(dx11_decoding_texture_.Receive()));
  RETURN_ON_HR_FAILURE(hr, "Failed to get texture from output sample", false);
  UINT subresource;
  dxgi_buffer->GetSubresourceIndex(&subresource);

  EGLAttrib frame_attributes[] = {
      EGL_D3D_TEXTURE_SUBRESOURCE_ID_ANGLE, subresource, EGL_NONE,
  };

  EGLBoolean result = eglStreamPostD3DTextureNV12ANGLE(
      egl_display, stream_, static_cast<void*>(dx11_decoding_texture_.get()),
      frame_attributes);
  RETURN_ON_FAILURE(result, "Could not post texture", false);
  result = eglStreamConsumerAcquireKHR(egl_display, stream_);
  RETURN_ON_FAILURE(result, "Could not post acquire stream", false);
  return true;
}

EGLStreamCopyPictureBuffer::EGLStreamCopyPictureBuffer(
    const PictureBuffer& buffer)
    : DXVAPictureBuffer(buffer), stream_(nullptr) {}

EGLStreamCopyPictureBuffer::~EGLStreamCopyPictureBuffer() {
  if (stream_) {
    EGLDisplay egl_display = gl::GLSurfaceEGL::GetHardwareDisplay();
    eglDestroyStreamKHR(egl_display, stream_);
    stream_ = nullptr;
  }
}

bool EGLStreamCopyPictureBuffer::Initialize(
    const DXVAVideoDecodeAccelerator& decoder) {
  EGLDisplay egl_display = gl::GLSurfaceEGL::GetHardwareDisplay();
  const EGLint stream_attributes[] = {
      EGL_CONSUMER_LATENCY_USEC_KHR,
      0,
      EGL_CONSUMER_ACQUIRE_TIMEOUT_USEC_KHR,
      0,
      EGL_NONE,
  };
  stream_ = eglCreateStreamKHR(egl_display, stream_attributes);
  RETURN_ON_FAILURE(!!stream_, "Could not create stream", false);
  gl::ScopedActiveTexture texture0(GL_TEXTURE0);
  gl::ScopedTextureBinder texture0_binder(GL_TEXTURE_EXTERNAL_OES,
                                          picture_buffer_.texture_ids()[0]);
  gl::ScopedActiveTexture texture1(GL_TEXTURE1);
  gl::ScopedTextureBinder texture1_binder(GL_TEXTURE_EXTERNAL_OES,
                                          picture_buffer_.texture_ids()[1]);

  EGLAttrib consumer_attributes[] = {
      EGL_COLOR_BUFFER_TYPE,
      EGL_YUV_BUFFER_EXT,
      EGL_YUV_NUMBER_OF_PLANES_EXT,
      2,
      EGL_YUV_PLANE0_TEXTURE_UNIT_NV,
      0,
      EGL_YUV_PLANE1_TEXTURE_UNIT_NV,
      1,
      EGL_NONE,
  };
  EGLBoolean result = eglStreamConsumerGLTextureExternalAttribsNV(
      egl_display, stream_, consumer_attributes);
  RETURN_ON_FAILURE(result, "Could not set stream consumer", false);

  EGLAttrib producer_attributes[] = {
      EGL_NONE,
  };

  result = eglCreateStreamProducerD3DTextureNV12ANGLE(egl_display, stream_,
                                                      producer_attributes);
  RETURN_ON_FAILURE(result, "Could not create stream producer", false);

  DCHECK(decoder.use_keyed_mutex_);
  D3D11_TEXTURE2D_DESC desc;
  desc.Width = picture_buffer_.size().width();
  desc.Height = picture_buffer_.size().height();
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = DXGI_FORMAT_NV12;
  desc.SampleDesc.Count = 1;
  desc.SampleDesc.Quality = 0;
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
  desc.CPUAccessFlags = 0;
  desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

  HRESULT hr = decoder.d3d11_device_->CreateTexture2D(
      &desc, nullptr, decoder_copy_texture_.Receive());
  RETURN_ON_HR_FAILURE(hr, "Failed to create texture", false);
  DCHECK(decoder.use_keyed_mutex_);
  hr = dx11_keyed_mutex_.QueryFrom(decoder_copy_texture_.get());
  RETURN_ON_HR_FAILURE(hr, "Failed to get keyed mutex", false);

  base::win::ScopedComPtr<IDXGIResource> resource;
  hr = resource.QueryFrom(decoder_copy_texture_.get());
  DCHECK(SUCCEEDED(hr));
  hr = resource->GetSharedHandle(&texture_share_handle_);
  RETURN_ON_FAILURE(SUCCEEDED(hr) && texture_share_handle_,
                    "Failed to query shared handle", false);

  hr = decoder.angle_device_->OpenSharedResource(
      texture_share_handle_, IID_PPV_ARGS(angle_copy_texture_.Receive()));
  RETURN_ON_HR_FAILURE(hr, "Failed to open shared resource", false);
  hr = egl_keyed_mutex_.QueryFrom(angle_copy_texture_.get());
  RETURN_ON_HR_FAILURE(hr, "Failed to get ANGLE mutex", false);
  return true;
}

bool EGLStreamCopyPictureBuffer::CopyOutputSampleDataToPictureBuffer(
    DXVAVideoDecodeAccelerator* decoder,
    IDirect3DSurface9* dest_surface,
    ID3D11Texture2D* dx11_texture,
    int input_buffer_id) {
  DCHECK(dx11_texture);
  // Grab a reference on the decoder texture. This reference will be released
  // when we receive a notification that the copy was completed or when the
  // DXVAPictureBuffer instance is destroyed.
  dx11_decoding_texture_ = dx11_texture;
  decoder->CopyTexture(dx11_texture, decoder_copy_texture_.get(),
                       dx11_keyed_mutex_, keyed_mutex_value_, id(),
                       input_buffer_id);
  // The texture copy will acquire the current keyed mutex value and release
  // with the value + 1.
  keyed_mutex_value_++;
  return true;
}

bool EGLStreamCopyPictureBuffer::CopySurfaceComplete(
    IDirect3DSurface9* src_surface,
    IDirect3DSurface9* dest_surface) {
  DCHECK(!available());
  DCHECK(!src_surface);
  DCHECK(!dest_surface);

  dx11_decoding_texture_.Release();

  HRESULT hr =
      egl_keyed_mutex_->AcquireSync(keyed_mutex_value_, kAcquireSyncWaitMs);
  RETURN_ON_FAILURE(hr == S_OK, "Could not acquire sync mutex", false);

  EGLAttrib frame_attributes[] = {
      EGL_D3D_TEXTURE_SUBRESOURCE_ID_ANGLE, 0, EGL_NONE,
  };

  EGLDisplay egl_display = gl::GLSurfaceEGL::GetHardwareDisplay();

  EGLBoolean result = eglStreamPostD3DTextureNV12ANGLE(
      egl_display, stream_, static_cast<void*>(angle_copy_texture_.get()),
      frame_attributes);
  RETURN_ON_FAILURE(result, "Could not post stream", false);
  result = eglStreamConsumerAcquireKHR(egl_display, stream_);
  RETURN_ON_FAILURE(result, "Could not post acquire stream", false);
  frame_in_consumer_ = true;

  return true;
}

bool EGLStreamCopyPictureBuffer::ReusePictureBuffer() {
  EGLDisplay egl_display = gl::GLSurfaceEGL::GetHardwareDisplay();

  if (frame_in_consumer_) {
    HRESULT hr = egl_keyed_mutex_->ReleaseSync(++keyed_mutex_value_);
    RETURN_ON_FAILURE(hr == S_OK, "Could not release sync mutex", false);
  }
  frame_in_consumer_ = false;

  if (stream_) {
    EGLBoolean result = eglStreamConsumerReleaseKHR(egl_display, stream_);
    RETURN_ON_FAILURE(result, "Could not release stream", false);
  }
  set_available(true);
  return true;
}

}  // namespace media
