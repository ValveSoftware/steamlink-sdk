// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains an implementation of VaapiWrapper, used by
// VaapiVideoDecodeAccelerator and VaapiH264Decoder to interface
// with libva (VA-API library for hardware video decode).

#ifndef CONTENT_COMMON_GPU_MEDIA_VAAPI_WRAPPER_H_
#define CONTENT_COMMON_GPU_MEDIA_VAAPI_WRAPPER_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "content/common/content_export.h"
#include "content/common/gpu/media/va_surface.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "third_party/libva/va/va_x11.h"
#include "ui/gfx/size.h"

namespace content {

// This class handles VA-API calls and ensures proper locking of VA-API calls
// to libva, the userspace shim to the HW decoder driver. libva is not
// thread-safe, so we have to perform locking ourselves. This class is fully
// synchronous and its methods can be called from any thread and may wait on
// the va_lock_ while other, concurrent calls run.
//
// This class is responsible for managing VAAPI connection, contexts and state.
// It is also responsible for managing and freeing VABuffers (not VASurfaces),
// which are used to queue decode parameters and slice data to the HW decoder,
// as well as underlying memory for VASurfaces themselves.
class CONTENT_EXPORT VaapiWrapper {
 public:
  // |report_error_to_uma_cb| will be called independently from reporting
  // errors to clients via method return values.
  static scoped_ptr<VaapiWrapper> Create(
      media::VideoCodecProfile profile,
      Display* x_display,
      const base::Closure& report_error_to_uma_cb);

  ~VaapiWrapper();

  // Create |num_surfaces| backing surfaces in driver for VASurfaces, each
  // of size |size|. Returns true when successful, with the created IDs in
  // |va_surfaces| to be managed and later wrapped in VASurfaces.
  // The client must DestroySurfaces() each time before calling this method
  // again to free the allocated surfaces first, but is not required to do so
  // at destruction time, as this will be done automatically from
  // the destructor.
  bool CreateSurfaces(gfx::Size size,
                      size_t num_surfaces,
                      std::vector<VASurfaceID>* va_surfaces);

  // Free all memory allocated in CreateSurfaces.
  void DestroySurfaces();

  // Submit parameters or slice data of |va_buffer_type|, copying them from
  // |buffer| of size |size|, into HW decoder. The data in |buffer| is no
  // longer needed and can be freed after this method returns.
  // Data submitted via this method awaits in the HW decoder until
  // DecodeAndDestroyPendingBuffers is called to execute or
  // DestroyPendingBuffers is used to cancel a pending decode.
  bool SubmitBuffer(VABufferType va_buffer_type, size_t size, void* buffer);

  // Cancel and destroy all buffers queued to the HW decoder via SubmitBuffer.
  // Useful when a pending decode is to be cancelled (on reset or error).
  void DestroyPendingBuffers();

  // Execute decode in hardware into |va_surface_id} and destroy pending
  // buffers. Return false if SubmitDecode() fails.
  bool DecodeAndDestroyPendingBuffers(VASurfaceID va_surface_id);

  // Put data from |va_surface_id| into |x_pixmap| of size |size|,
  // converting/scaling to it.
  bool PutSurfaceIntoPixmap(VASurfaceID va_surface_id,
                            Pixmap x_pixmap,
                            gfx::Size dest_size);

  // Returns true if the VAAPI version is less than the specified version.
  bool VAAPIVersionLessThan(int major, int minor);

  // Get a VAImage from a VASurface and map it into memory. The VAImage should
  // be released using the ReturnVaImage function. Returns true when successful.
  // This is intended for testing only.
  bool GetVaImageForTesting(VASurfaceID va_surface_id,
                            VAImage* image,
                            void** mem);

  // Release the VAImage (and the associated memory mapping) obtained from
  // GetVaImage(). This is intended for testing only.
  void ReturnVaImageForTesting(VAImage* image);

 private:
  VaapiWrapper();

  bool Initialize(media::VideoCodecProfile profile,
                  Display* x_display,
                  const base::Closure& report_error__to_uma_cb);
  void Deinitialize();

  // Execute decode in hardware and destroy pending buffers. Return false if
  // vaapi driver refuses to accept parameter or slice buffers submitted
  // by client or if decode fails in hardware.
  bool SubmitDecode(VASurfaceID va_surface_id);

  // Attempt to set render mode to "render to texture.". Failure is non-fatal.
  void TryToSetVADisplayAttributeToLocalGPU();

  // Lazily initialize static data after sandbox is enabled.  Return false on
  // init failure.
  static bool PostSandboxInitialization();

  // Libva is not thread safe, so we have to do locking for it ourselves.
  // This lock is to be taken for the duration of all VA-API calls and for
  // the entire decode execution sequence in DecodeAndDestroyPendingBuffers().
  base::Lock va_lock_;

  // Allocated ids for VASurfaces.
  std::vector<VASurfaceID> va_surface_ids_;

  // The VAAPI version.
  int major_version_, minor_version_;

  // VA handles.
  // Both valid after successful Initialize() and until Deinitialize().
  VADisplay va_display_;
  VAConfigID va_config_id_;
  // Created for the current set of va_surface_ids_ in CreateSurfaces() and
  // valid until DestroySurfaces().
  VAContextID va_context_id_;

  // Data queued up for HW decoder, to be committed on next HW decode.
  std::vector<VABufferID> pending_slice_bufs_;
  std::vector<VABufferID> pending_va_bufs_;

  // Called to report decoding errors to UMA. Errors to clients are reported via
  // return values from public methods.
  base::Closure report_error_to_uma_cb_;

  DISALLOW_COPY_AND_ASSIGN(VaapiWrapper);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_VAAPI_WRAPPER_H_
