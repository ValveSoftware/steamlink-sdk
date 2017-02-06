// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/gpu_video_decode_accelerator_factory_impl.h"

#include "base/memory/ptr_util.h"
#include "gpu/command_buffer/service/gpu_preferences.h"
#include "media/gpu/gpu_video_accelerator_util.h"
#include "media/gpu/media_gpu_export.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "media/gpu/dxva_video_decode_accelerator_win.h"
#elif defined(OS_MACOSX)
#include "media/gpu/vt_video_decode_accelerator_mac.h"
#elif defined(OS_CHROMEOS)
#if defined(USE_V4L2_CODEC)
#include "media/gpu/v4l2_device.h"
#include "media/gpu/v4l2_slice_video_decode_accelerator.h"
#include "media/gpu/v4l2_video_decode_accelerator.h"
#include "ui/gl/gl_surface_egl.h"
#endif
#if defined(ARCH_CPU_X86_FAMILY)
#include "media/gpu/vaapi_video_decode_accelerator.h"
#include "ui/gl/gl_implementation.h"
#endif
#elif defined(OS_ANDROID)
#include "media/gpu/android_video_decode_accelerator.h"
#endif

namespace media {

// static
MEDIA_GPU_EXPORT std::unique_ptr<GpuVideoDecodeAcceleratorFactoryImpl>
GpuVideoDecodeAcceleratorFactoryImpl::Create(
    const GetGLContextCallback& get_gl_context_cb,
    const MakeGLContextCurrentCallback& make_context_current_cb,
    const BindGLImageCallback& bind_image_cb) {
  return base::WrapUnique(new GpuVideoDecodeAcceleratorFactoryImpl(
      get_gl_context_cb, make_context_current_cb, bind_image_cb,
      GetGLES2DecoderCallback()));
}

// static
MEDIA_GPU_EXPORT std::unique_ptr<GpuVideoDecodeAcceleratorFactoryImpl>
GpuVideoDecodeAcceleratorFactoryImpl::CreateWithGLES2Decoder(
    const GetGLContextCallback& get_gl_context_cb,
    const MakeGLContextCurrentCallback& make_context_current_cb,
    const BindGLImageCallback& bind_image_cb,
    const GetGLES2DecoderCallback& get_gles2_decoder_cb) {
  return base::WrapUnique(new GpuVideoDecodeAcceleratorFactoryImpl(
      get_gl_context_cb, make_context_current_cb, bind_image_cb,
      get_gles2_decoder_cb));
}

// static
MEDIA_GPU_EXPORT std::unique_ptr<GpuVideoDecodeAcceleratorFactoryImpl>
GpuVideoDecodeAcceleratorFactoryImpl::CreateWithNoGL() {
  return Create(GetGLContextCallback(), MakeGLContextCurrentCallback(),
                BindGLImageCallback());
}

// static
MEDIA_GPU_EXPORT gpu::VideoDecodeAcceleratorCapabilities
GpuVideoDecodeAcceleratorFactoryImpl::GetDecoderCapabilities(
    const gpu::GpuPreferences& gpu_preferences) {
  VideoDecodeAccelerator::Capabilities capabilities;
  if (gpu_preferences.disable_accelerated_video_decode)
    return gpu::VideoDecodeAcceleratorCapabilities();

  // Query VDAs for their capabilities and construct a set of supported
  // profiles for current platform. This must be done in the same order as in
  // CreateVDA(), as we currently preserve additional capabilities (such as
  // resolutions supported) only for the first VDA supporting the given codec
  // profile (instead of calculating a superset).
  // TODO(posciak,henryhsu): improve this so that we choose a superset of
  // resolutions and other supported profile parameters.
#if defined(OS_WIN)
  capabilities.supported_profiles =
      DXVAVideoDecodeAccelerator::GetSupportedProfiles();
#elif defined(OS_CHROMEOS)
  VideoDecodeAccelerator::SupportedProfiles vda_profiles;
#if defined(USE_V4L2_CODEC)
  vda_profiles = V4L2VideoDecodeAccelerator::GetSupportedProfiles();
  GpuVideoAcceleratorUtil::InsertUniqueDecodeProfiles(
      vda_profiles, &capabilities.supported_profiles);
  vda_profiles = V4L2SliceVideoDecodeAccelerator::GetSupportedProfiles();
  GpuVideoAcceleratorUtil::InsertUniqueDecodeProfiles(
      vda_profiles, &capabilities.supported_profiles);
#endif
#if defined(ARCH_CPU_X86_FAMILY)
  vda_profiles = VaapiVideoDecodeAccelerator::GetSupportedProfiles();
  GpuVideoAcceleratorUtil::InsertUniqueDecodeProfiles(
      vda_profiles, &capabilities.supported_profiles);
#endif
#elif defined(OS_MACOSX)
  capabilities.supported_profiles =
      VTVideoDecodeAccelerator::GetSupportedProfiles();
#elif defined(OS_ANDROID)
  capabilities =
      AndroidVideoDecodeAccelerator::GetCapabilities(gpu_preferences);
#endif
  return GpuVideoAcceleratorUtil::ConvertMediaToGpuDecodeCapabilities(
      capabilities);
}

MEDIA_GPU_EXPORT std::unique_ptr<VideoDecodeAccelerator>
GpuVideoDecodeAcceleratorFactoryImpl::CreateVDA(
    VideoDecodeAccelerator::Client* client,
    const VideoDecodeAccelerator::Config& config,
    const gpu::GpuDriverBugWorkarounds& workarounds,
    const gpu::GpuPreferences& gpu_preferences) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (gpu_preferences.disable_accelerated_video_decode)
    return nullptr;

  // Array of Create..VDA() function pointers, potentially usable on current
  // platform. This list is ordered by priority, from most to least preferred,
  // if applicable. This list must be in the same order as the querying order
  // in GetDecoderCapabilities() above.
  using CreateVDAFp = std::unique_ptr<VideoDecodeAccelerator> (
      GpuVideoDecodeAcceleratorFactoryImpl::*)(
      const gpu::GpuDriverBugWorkarounds&, const gpu::GpuPreferences&) const;
  const CreateVDAFp create_vda_fps[] = {
#if defined(OS_WIN)
    &GpuVideoDecodeAcceleratorFactoryImpl::CreateDXVAVDA,
#endif
#if defined(OS_CHROMEOS) && defined(USE_V4L2_CODEC)
    &GpuVideoDecodeAcceleratorFactoryImpl::CreateV4L2VDA,
    &GpuVideoDecodeAcceleratorFactoryImpl::CreateV4L2SVDA,
#endif
#if defined(OS_CHROMEOS) && defined(ARCH_CPU_X86_FAMILY)
    &GpuVideoDecodeAcceleratorFactoryImpl::CreateVaapiVDA,
#endif
#if defined(OS_MACOSX)
    &GpuVideoDecodeAcceleratorFactoryImpl::CreateVTVDA,
#endif
#if defined(OS_ANDROID)
    &GpuVideoDecodeAcceleratorFactoryImpl::CreateAndroidVDA,
#endif
  };

  std::unique_ptr<VideoDecodeAccelerator> vda;

  for (const auto& create_vda_function : create_vda_fps) {
    vda = (this->*create_vda_function)(workarounds, gpu_preferences);
    if (vda && vda->Initialize(config, client))
      return vda;
  }

  return nullptr;
}

#if defined(OS_WIN)
std::unique_ptr<VideoDecodeAccelerator>
GpuVideoDecodeAcceleratorFactoryImpl::CreateDXVAVDA(
    const gpu::GpuDriverBugWorkarounds& workarounds,
    const gpu::GpuPreferences& gpu_preferences) const {
  std::unique_ptr<VideoDecodeAccelerator> decoder;
  if (base::win::GetVersion() >= base::win::VERSION_WIN7) {
    DVLOG(0) << "Initializing DXVA HW decoder for windows.";
    decoder.reset(new DXVAVideoDecodeAccelerator(get_gl_context_cb_,
                                                 make_context_current_cb_,
                                                 workarounds, gpu_preferences));
  }
  return decoder;
}
#endif

#if defined(OS_CHROMEOS) && defined(USE_V4L2_CODEC)
std::unique_ptr<VideoDecodeAccelerator>
GpuVideoDecodeAcceleratorFactoryImpl::CreateV4L2VDA(
    const gpu::GpuDriverBugWorkarounds& workarounds,
    const gpu::GpuPreferences& gpu_preferences) const {
  std::unique_ptr<VideoDecodeAccelerator> decoder;
  scoped_refptr<V4L2Device> device = V4L2Device::Create(V4L2Device::kDecoder);
  if (device.get()) {
    decoder.reset(new V4L2VideoDecodeAccelerator(
        gl::GLSurfaceEGL::GetHardwareDisplay(), get_gl_context_cb_,
        make_context_current_cb_, device));
  }
  return decoder;
}

std::unique_ptr<VideoDecodeAccelerator>
GpuVideoDecodeAcceleratorFactoryImpl::CreateV4L2SVDA(
    const gpu::GpuDriverBugWorkarounds& workarounds,
    const gpu::GpuPreferences& gpu_preferences) const {
  std::unique_ptr<VideoDecodeAccelerator> decoder;
  scoped_refptr<V4L2Device> device = V4L2Device::Create(V4L2Device::kDecoder);
  if (device.get()) {
    decoder.reset(new V4L2SliceVideoDecodeAccelerator(
        device, gl::GLSurfaceEGL::GetHardwareDisplay(), get_gl_context_cb_,
        make_context_current_cb_));
  }
  return decoder;
}
#endif

#if defined(OS_CHROMEOS) && defined(ARCH_CPU_X86_FAMILY)
std::unique_ptr<VideoDecodeAccelerator>
GpuVideoDecodeAcceleratorFactoryImpl::CreateVaapiVDA(
    const gpu::GpuDriverBugWorkarounds& workarounds,
    const gpu::GpuPreferences& gpu_preferences) const {
  std::unique_ptr<VideoDecodeAccelerator> decoder;
  decoder.reset(new VaapiVideoDecodeAccelerator(make_context_current_cb_,
                                                bind_image_cb_));
  return decoder;
}
#endif

#if defined(OS_MACOSX)
std::unique_ptr<VideoDecodeAccelerator>
GpuVideoDecodeAcceleratorFactoryImpl::CreateVTVDA(
    const gpu::GpuDriverBugWorkarounds& workarounds,
    const gpu::GpuPreferences& gpu_preferences) const {
  std::unique_ptr<VideoDecodeAccelerator> decoder;
  decoder.reset(
      new VTVideoDecodeAccelerator(make_context_current_cb_, bind_image_cb_));
  return decoder;
}
#endif

#if defined(OS_ANDROID)
std::unique_ptr<VideoDecodeAccelerator>
GpuVideoDecodeAcceleratorFactoryImpl::CreateAndroidVDA(
    const gpu::GpuDriverBugWorkarounds& workarounds,
    const gpu::GpuPreferences& gpu_preferences) const {
  std::unique_ptr<VideoDecodeAccelerator> decoder;
  decoder.reset(new AndroidVideoDecodeAccelerator(make_context_current_cb_,
                                                  get_gles2_decoder_cb_));
  return decoder;
}
#endif

GpuVideoDecodeAcceleratorFactoryImpl::GpuVideoDecodeAcceleratorFactoryImpl(
    const GetGLContextCallback& get_gl_context_cb,
    const MakeGLContextCurrentCallback& make_context_current_cb,
    const BindGLImageCallback& bind_image_cb,
    const GetGLES2DecoderCallback& get_gles2_decoder_cb)
    : get_gl_context_cb_(get_gl_context_cb),
      make_context_current_cb_(make_context_current_cb),
      bind_image_cb_(bind_image_cb),
      get_gles2_decoder_cb_(get_gles2_decoder_cb) {}

GpuVideoDecodeAcceleratorFactoryImpl::~GpuVideoDecodeAcceleratorFactoryImpl() {}

}  // namespace media
