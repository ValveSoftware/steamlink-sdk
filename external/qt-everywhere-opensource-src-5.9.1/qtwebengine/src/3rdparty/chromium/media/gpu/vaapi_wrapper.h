// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains an implementation of VaapiWrapper, used by
// VaapiVideoDecodeAccelerator and VaapiH264Decoder for decode,
// and VaapiVideoEncodeAccelerator for encode, to interface
// with libva (VA-API library for hardware video codec).

#ifndef MEDIA_GPU_VAAPI_WRAPPER_H_
#define MEDIA_GPU_VAAPI_WRAPPER_H_

#include <stddef.h>
#include <stdint.h>

#include <set>
#include <vector>

#include "base/files/file.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/va_surface.h"
#include "media/video/jpeg_decode_accelerator.h"
#include "media/video/video_decode_accelerator.h"
#include "media/video/video_encode_accelerator.h"
#include "third_party/libva/va/va.h"
#include "third_party/libva/va/va_vpp.h"
#include "ui/gfx/geometry/size.h"

#if defined(USE_X11)
#include "third_party/libva/va/va_x11.h"
#endif  // USE_X11

#if defined(USE_OZONE)
namespace ui {
class NativePixmap;
}
#endif

namespace media {

// This class handles VA-API calls and ensures proper locking of VA-API calls
// to libva, the userspace shim to the HW codec driver. libva is not
// thread-safe, so we have to perform locking ourselves. This class is fully
// synchronous and its methods can be called from any thread and may wait on
// the va_lock_ while other, concurrent calls run.
//
// This class is responsible for managing VAAPI connection, contexts and state.
// It is also responsible for managing and freeing VABuffers (not VASurfaces),
// which are used to queue parameters and slice data to the HW codec,
// as well as underlying memory for VASurfaces themselves.
class MEDIA_GPU_EXPORT VaapiWrapper
    : public base::RefCountedThreadSafe<VaapiWrapper> {
 public:
  enum CodecMode {
    kDecode,
    kEncode,
    kCodecModeMax,
  };

  // Return an instance of VaapiWrapper initialized for |va_profile| and
  // |mode|. |report_error_to_uma_cb| will be called independently from
  // reporting errors to clients via method return values.
  static scoped_refptr<VaapiWrapper> Create(
      CodecMode mode,
      VAProfile va_profile,
      const base::Closure& report_error_to_uma_cb);

  // Create VaapiWrapper for VideoCodecProfile. It maps VideoCodecProfile
  // |profile| to VAProfile.
  // |report_error_to_uma_cb| will be called independently from reporting
  // errors to clients via method return values.
  static scoped_refptr<VaapiWrapper> CreateForVideoCodec(
      CodecMode mode,
      VideoCodecProfile profile,
      const base::Closure& report_error_to_uma_cb);

  // Return the supported video encode profiles.
  static VideoEncodeAccelerator::SupportedProfiles GetSupportedEncodeProfiles();

  // Return the supported video decode profiles.
  static VideoDecodeAccelerator::SupportedProfiles GetSupportedDecodeProfiles();

  // Return true when JPEG decode is supported.
  static bool IsJpegDecodeSupported();

  // Create |num_surfaces| backing surfaces in driver for VASurfaces of
  // |va_format|, each of size |size|. Returns true when successful, with the
  // created IDs in |va_surfaces| to be managed and later wrapped in
  // VASurfaces.
  // The client must DestroySurfaces() each time before calling this method
  // again to free the allocated surfaces first, but is not required to do so
  // at destruction time, as this will be done automatically from
  // the destructor.
  bool CreateSurfaces(unsigned int va_format,
                      const gfx::Size& size,
                      size_t num_surfaces,
                      std::vector<VASurfaceID>* va_surfaces);

  // Free all memory allocated in CreateSurfaces.
  void DestroySurfaces();

  // Create a VASurface of |va_format|, |size| and using |va_attribs|
  // attributes. The ownership of the surface is transferred to the
  // caller. It differs from surfaces created using CreateSurfaces(),
  // where VaapiWrapper is the owner of the surfaces.
  scoped_refptr<VASurface> CreateUnownedSurface(
      unsigned int va_format,
      const gfx::Size& size,
      const std::vector<VASurfaceAttrib>& va_attribs);

#if defined(USE_OZONE)
  // Create a VASurface for |pixmap|. The ownership of the surface is
  // transferred to the caller. It differs from surfaces created using
  // CreateSurfaces(), where VaapiWrapper is the owner of the surfaces.
  scoped_refptr<VASurface> CreateVASurfaceForPixmap(
      const scoped_refptr<ui::NativePixmap>& pixmap);

  // Use VPP to process |source_pixmap| to |target_pixmap| with scaling and
  // color space conversion.
  bool ProcessPixmap(const scoped_refptr<ui::NativePixmap>& source_pixmap,
                     scoped_refptr<ui::NativePixmap> target_pixmap);

#endif

  // Submit parameters or slice data of |va_buffer_type|, copying them from
  // |buffer| of size |size|, into HW codec. The data in |buffer| is no
  // longer needed and can be freed after this method returns.
  // Data submitted via this method awaits in the HW codec until
  // ExecuteAndDestroyPendingBuffers() is called to execute or
  // DestroyPendingBuffers() is used to cancel a pending job.
  bool SubmitBuffer(VABufferType va_buffer_type, size_t size, void* buffer);

  // Submit a VAEncMiscParameterBuffer of given |misc_param_type|, copying its
  // data from |buffer| of size |size|, into HW codec. The data in |buffer| is
  // no longer needed and can be freed after this method returns.
  // Data submitted via this method awaits in the HW codec until
  // ExecuteAndDestroyPendingBuffers() is called to execute or
  // DestroyPendingBuffers() is used to cancel a pending job.
  bool SubmitVAEncMiscParamBuffer(VAEncMiscParameterType misc_param_type,
                                  size_t size,
                                  void* buffer);

  // Cancel and destroy all buffers queued to the HW codec via SubmitBuffer().
  // Useful when a pending job is to be cancelled (on reset or error).
  void DestroyPendingBuffers();

  // Execute job in hardware on target |va_surface_id| and destroy pending
  // buffers. Return false if Execute() fails.
  bool ExecuteAndDestroyPendingBuffers(VASurfaceID va_surface_id);

#if defined(USE_X11)
  // Put data from |va_surface_id| into |x_pixmap| of size
  // |dest_size|, converting/scaling to it.
  bool PutSurfaceIntoPixmap(VASurfaceID va_surface_id,
                            Pixmap x_pixmap,
                            gfx::Size dest_size);
#endif  // USE_X11

  // Get a VAImage from a VASurface and map it into memory. The size and format
  // are derived from the surface. Use GetVaImage() instead if |format| or
  // |size| are different from surface internal representation. The VAImage
  // should be released using the ReturnVaImage function. Returns true when
  // successful.
  bool GetDerivedVaImage(VASurfaceID va_surface_id, VAImage* image, void** mem);

  // Get a VAImage from a VASurface |va_surface_id| and map it into memory with
  // given |format| and |size|. The output is |image| and the mapped memory is
  // |mem|. If |format| doesn't equal to the internal format, the underlying
  // implementation will do format conversion if supported. |size| should be
  // smaller than or equal to the surface. If |size| is smaller, the image will
  // be cropped. The VAImage should be released using the ReturnVaImage
  // function. Returns true when successful.
  bool GetVaImage(VASurfaceID va_surface_id,
                  VAImageFormat* format,
                  const gfx::Size& size,
                  VAImage* image,
                  void** mem);

  // Release the VAImage (and the associated memory mapping) obtained from
  // GetVaImage() or GetDerivedVaImage().
  void ReturnVaImage(VAImage* image);

  // Upload contents of |frame| into |va_surface_id| for encode.
  bool UploadVideoFrameToSurface(const scoped_refptr<VideoFrame>& frame,
                                 VASurfaceID va_surface_id);

  // Create a buffer of |size| bytes to be used as encode output.
  bool CreateCodedBuffer(size_t size, VABufferID* buffer_id);

  // Download the contents of the buffer with given |buffer_id| into a buffer of
  // size |target_size|, pointed to by |target_ptr|. The number of bytes
  // downloaded will be returned in |coded_data_size|. |sync_surface_id| will
  // be used as a sync point, i.e. it will have to become idle before starting
  // the download. |sync_surface_id| should be the source surface passed
  // to the encode job.
  bool DownloadAndDestroyCodedBuffer(VABufferID buffer_id,
                                     VASurfaceID sync_surface_id,
                                     uint8_t* target_ptr,
                                     size_t target_size,
                                     size_t* coded_data_size);

  // Destroy all previously-allocated (and not yet destroyed) coded buffers.
  void DestroyCodedBuffers();

  // Blits a VASurface |va_surface_src| into another VASurface
  // |va_surface_dest| applying pixel format conversion and scaling
  // if needed.
  bool BlitSurface(const scoped_refptr<VASurface>& va_surface_src,
                   const scoped_refptr<VASurface>& va_surface_dest);

  // Initialize static data before sandbox is enabled.
  static void PreSandboxInitialization();

  // Get the created surfaces format.
  unsigned int va_surface_format() const { return va_surface_format_; }

 private:
  friend class base::RefCountedThreadSafe<VaapiWrapper>;

  struct ProfileInfo {
    VAProfile va_profile;
    gfx::Size max_resolution;
  };

  class LazyProfileInfos {
   public:
    LazyProfileInfos();
    ~LazyProfileInfos();
    std::vector<ProfileInfo> GetSupportedProfileInfosForCodecMode(
        CodecMode mode);
    bool IsProfileSupported(CodecMode mode, VAProfile va_profile);

   private:
    std::vector<ProfileInfo> supported_profiles_[kCodecModeMax];
  };

  class VADisplayState {
   public:
    VADisplayState();
    ~VADisplayState();

    // |va_lock_| must be held on entry.
    bool Initialize();
    void Deinitialize(VAStatus* status);

    base::Lock* va_lock() { return &va_lock_; }
    VADisplay va_display() const { return va_display_; }

#if defined(USE_OZONE)
    void SetDrmFd(base::PlatformFile fd);
#endif  // USE_OZONE

   private:
    friend class base::LazyInstance<VADisplayState>;

    // Returns true if the VAAPI version is less than the specified version.
    bool VAAPIVersionLessThan(int major, int minor);

    // Protected by |va_lock_|.
    int refcount_;

    // Libva is not thread safe, so we have to do locking for it ourselves.
    // This lock is to be taken for the duration of all VA-API calls and for
    // the entire job submission sequence in ExecuteAndDestroyPendingBuffers().
    base::Lock va_lock_;

#if defined(USE_OZONE)
    // Drm fd used to obtain access to the driver interface by VA.
    base::ScopedFD drm_fd_;
#endif  // USE_OZONE

    // The VADisplay handle.
    VADisplay va_display_;

    // The VAAPI version.
    int major_version_, minor_version_;

    // True if vaInitialize has been called successfully.
    bool va_initialized_;
  };

  VaapiWrapper();
  ~VaapiWrapper();

  bool Initialize(CodecMode mode, VAProfile va_profile);
  void Deinitialize();
  bool VaInitialize(const base::Closure& report_error_to_uma_cb);
  bool GetSupportedVaProfiles(std::vector<VAProfile>* profiles);

  // Check if |va_profile| supports |entrypoint| or not. |va_lock_| must be
  // held on entry.
  bool IsEntrypointSupported_Locked(VAProfile va_profile,
                                    VAEntrypoint entrypoint);

  // Return true if |va_profile| for |entrypoint| with |required_attribs| is
  // supported. |va_lock_| must be held on entry.
  bool AreAttribsSupported_Locked(
      VAProfile va_profile,
      VAEntrypoint entrypoint,
      const std::vector<VAConfigAttrib>& required_attribs);

  // Get maximum resolution for |va_profile| and |entrypoint| with
  // |required_attribs|. If return value is true, |resolution| is the maximum
  // resolution. |va_lock_| must be held on entry.
  bool GetMaxResolution_Locked(VAProfile va_profile,
                               VAEntrypoint entrypoint,
                               std::vector<VAConfigAttrib>& required_attribs,
                               gfx::Size* resolution);

  // Destroys a |va_surface| created using CreateUnownedSurface.
  void DestroyUnownedSurface(VASurfaceID va_surface_id);

  // Initialize the video post processing context with the |size| of
  // the input pictures to be processed.
  bool InitializeVpp_Locked();

  // Deinitialize the video post processing context.
  void DeinitializeVpp();

  // Execute pending job in hardware and destroy pending buffers. Return false
  // if vaapi driver refuses to accept parameter or slice buffers submitted
  // by client, or if execution fails in hardware.
  bool Execute(VASurfaceID va_surface_id);

  // Attempt to set render mode to "render to texture.". Failure is non-fatal.
  void TryToSetVADisplayAttributeToLocalGPU();

  // Get supported profile infos for |mode|.
  std::vector<ProfileInfo> GetSupportedProfileInfosForCodecModeInternal(
      CodecMode mode);

  // Lazily initialize static data after sandbox is enabled.  Return false on
  // init failure.
  static bool PostSandboxInitialization();

  // Map VideoCodecProfile enum values to VaProfile values. This function
  // includes a workaround for crbug.com/345569. If va_profile is h264 baseline
  // and it is not supported, we try constrained baseline.
  static VAProfile ProfileToVAProfile(VideoCodecProfile profile,
                                      CodecMode mode);

  // Pointer to VADisplayState's member |va_lock_|. Guaranteed to be valid for
  // the lifetime of VaapiWrapper.
  base::Lock* va_lock_;

  // Allocated ids for VASurfaces.
  std::vector<VASurfaceID> va_surface_ids_;

  // VA format of surfaces with va_surface_ids_.
  unsigned int va_surface_format_;

  // Singleton instance of VADisplayState.
  static base::LazyInstance<VADisplayState> va_display_state_;

  // VA handles.
  // All valid after successful Initialize() and until Deinitialize().
  VADisplay va_display_;
  VAConfigID va_config_id_;
  // Created for the current set of va_surface_ids_ in CreateSurfaces() and
  // valid until DestroySurfaces().
  VAContextID va_context_id_;

  // Data queued up for HW codec, to be committed on next execution.
  std::vector<VABufferID> pending_slice_bufs_;
  std::vector<VABufferID> pending_va_bufs_;

  // Bitstream buffers for encode.
  std::set<VABufferID> coded_buffers_;

  // Called to report codec errors to UMA. Errors to clients are reported via
  // return values from public methods.
  base::Closure report_error_to_uma_cb_;

  // VPP (Video Post Processing) context, this is used to convert
  // pictures used by the decoder to RGBA pictures usable by GL or the
  // display hardware.
  VAConfigID va_vpp_config_id_;
  VAContextID va_vpp_context_id_;
  VABufferID va_vpp_buffer_id_;

  // Singleton variable to store supported profile information for encode and
  // decode.
  static base::LazyInstance<LazyProfileInfos> profile_infos_;

  DISALLOW_COPY_AND_ASSIGN(VaapiWrapper);
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_WRAPPER_H_
