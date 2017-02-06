// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_DXVA_PICTURE_BUFFER_WIN_H_
#define MEDIA_GPU_DXVA_PICTURE_BUFFER_WIN_H_

#include <d3d11.h>
#include <d3d9.h>
#include <mfidl.h>

#include <memory>

#include "base/memory/linked_ptr.h"
#include "base/win/scoped_comptr.h"
#include "media/video/picture.h"
#include "third_party/angle/include/EGL/egl.h"
#include "third_party/angle/include/EGL/eglext.h"
#include "ui/gl/gl_fence.h"

interface IMFSample;

namespace media {
class DXVAVideoDecodeAccelerator;

// Maintains information about a DXVA picture buffer, i.e. whether it is
// available for rendering, the texture information, etc.
class DXVAPictureBuffer {
 public:
  static linked_ptr<DXVAPictureBuffer> Create(
      const DXVAVideoDecodeAccelerator& decoder,
      const PictureBuffer& buffer,
      EGLConfig egl_config);
  virtual ~DXVAPictureBuffer();

  virtual bool ReusePictureBuffer() = 0;
  virtual void ResetReuseFence();
  // Copies the output sample data to the picture buffer provided by the
  // client.
  // The dest_surface parameter contains the decoded bits.
  virtual bool CopyOutputSampleDataToPictureBuffer(
      DXVAVideoDecodeAccelerator* decoder,
      IDirect3DSurface9* dest_surface,
      ID3D11Texture2D* dx11_texture,
      int input_buffer_id);

  bool available() const { return available_; }

  void set_available(bool available) { available_ = available; }

  int id() const { return picture_buffer_.id(); }

  gfx::Size size() const { return picture_buffer_.size(); }

  virtual bool waiting_to_reuse() const;
  virtual gl::GLFence* reuse_fence();

  // Called when the source surface |src_surface| is copied to the destination
  // |dest_surface|
  virtual bool CopySurfaceComplete(IDirect3DSurface9* src_surface,
                                   IDirect3DSurface9* dest_surface);
  virtual bool BindSampleToTexture(base::win::ScopedComPtr<IMFSample> sample);

 protected:
  explicit DXVAPictureBuffer(const PictureBuffer& buffer);

  bool available_;
  PictureBuffer picture_buffer_;

  DISALLOW_COPY_AND_ASSIGN(DXVAPictureBuffer);
};

// Copies the video result into an RGBA EGL pbuffer.
class PbufferPictureBuffer : public DXVAPictureBuffer {
 public:
  explicit PbufferPictureBuffer(const PictureBuffer& buffer);
  ~PbufferPictureBuffer() override;

  bool Initialize(const DXVAVideoDecodeAccelerator& decoder,
                  EGLConfig egl_config);
  bool InitializeTexture(const DXVAVideoDecodeAccelerator& decoder,
                         bool use_rgb);

  bool ReusePictureBuffer() override;
  void ResetReuseFence() override;
  bool CopyOutputSampleDataToPictureBuffer(DXVAVideoDecodeAccelerator* decoder,
                                           IDirect3DSurface9* dest_surface,
                                           ID3D11Texture2D* dx11_texture,
                                           int input_buffer_id) override;
  bool waiting_to_reuse() const override;
  gl::GLFence* reuse_fence() override;
  bool CopySurfaceComplete(IDirect3DSurface9* src_surface,
                           IDirect3DSurface9* dest_surface) override;

 protected:
  // This is true if the decoder is currently waiting on the fence before
  // reusing the buffer.
  bool waiting_to_reuse_;
  EGLSurface decoding_surface_;

  std::unique_ptr<gl::GLFence> reuse_fence_;

  HANDLE texture_share_handle_;
  base::win::ScopedComPtr<IDirect3DTexture9> decoding_texture_;
  base::win::ScopedComPtr<ID3D11Texture2D> dx11_decoding_texture_;

  base::win::ScopedComPtr<IDXGIKeyedMutex> egl_keyed_mutex_;
  base::win::ScopedComPtr<IDXGIKeyedMutex> dx11_keyed_mutex_;

  // This is the last value that was used to release the keyed mutex.
  uint64_t keyed_mutex_value_;

  // The following |IDirect3DSurface9| interface pointers are used to hold
  // references on the surfaces during the course of a StretchRect operation
  // to copy the source surface to the target. The references are released
  // when the StretchRect operation i.e. the copy completes.
  base::win::ScopedComPtr<IDirect3DSurface9> decoder_surface_;
  base::win::ScopedComPtr<IDirect3DSurface9> target_surface_;

  // This ID3D11Texture2D interface pointer is used to hold a reference to the
  // decoder texture during the course of a copy operation. This reference is
  // released when the copy completes.
  base::win::ScopedComPtr<ID3D11Texture2D> decoder_dx11_texture_;

  // Set to true if RGB is supported by the texture.
  // Defaults to true.
  bool use_rgb_;
};

// Shares the decoded texture with ANGLE without copying by using an EGL stream.
class EGLStreamPictureBuffer : public DXVAPictureBuffer {
 public:
  explicit EGLStreamPictureBuffer(const PictureBuffer& buffer);
  ~EGLStreamPictureBuffer() override;

  bool Initialize();
  bool ReusePictureBuffer() override;
  bool BindSampleToTexture(base::win::ScopedComPtr<IMFSample> sample) override;

 private:
  EGLStreamKHR stream_;

  base::win::ScopedComPtr<IMFSample> current_d3d_sample_;
  base::win::ScopedComPtr<ID3D11Texture2D> dx11_decoding_texture_;
};

// Creates an NV12 texture and copies to it, then shares that with ANGLE.
class EGLStreamCopyPictureBuffer : public DXVAPictureBuffer {
 public:
  explicit EGLStreamCopyPictureBuffer(const PictureBuffer& buffer);
  ~EGLStreamCopyPictureBuffer() override;

  bool Initialize(const DXVAVideoDecodeAccelerator& decoder);
  bool ReusePictureBuffer() override;

  bool CopyOutputSampleDataToPictureBuffer(DXVAVideoDecodeAccelerator* decoder,
                                           IDirect3DSurface9* dest_surface,
                                           ID3D11Texture2D* dx11_texture,
                                           int input_buffer_id) override;
  bool CopySurfaceComplete(IDirect3DSurface9* src_surface,
                           IDirect3DSurface9* dest_surface) override;

 private:
  EGLStreamKHR stream_;

  // True if the copy has been completed and it has not been set for reuse
  // yet.
  bool frame_in_consumer_ = false;

  // This ID3D11Texture2D interface pointer is used to hold a reference to the
  // MFT decoder texture during the course of a copy operation. This reference
  // is released when the copy completes.
  base::win::ScopedComPtr<ID3D11Texture2D> dx11_decoding_texture_;

  base::win::ScopedComPtr<IDXGIKeyedMutex> egl_keyed_mutex_;
  base::win::ScopedComPtr<IDXGIKeyedMutex> dx11_keyed_mutex_;

  HANDLE texture_share_handle_;
  // This is the texture (created on ANGLE's device) that will be put in the
  // EGLStream.
  base::win::ScopedComPtr<ID3D11Texture2D> angle_copy_texture_;
  // This is another copy of that shared resource that will be copied to from
  // the decoder.
  base::win::ScopedComPtr<ID3D11Texture2D> decoder_copy_texture_;

  // This is the last value that was used to release the keyed mutex.
  uint64_t keyed_mutex_value_ = 0;
};

}  // namespace media

#endif  // MEDIA_GPU_DXVA_PICTURE_BUFFER_WIN_H_
