// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/gpu/gpu_video_decode_accelerator_factory.h"

#include "base/memory/ptr_util.h"
#include "content/gpu/gpu_child_thread.h"
#include "media/gpu/gpu_video_decode_accelerator_factory_impl.h"

namespace content {

GpuVideoDecodeAcceleratorFactory::~GpuVideoDecodeAcceleratorFactory() {}

// static
std::unique_ptr<GpuVideoDecodeAcceleratorFactory>
GpuVideoDecodeAcceleratorFactory::Create(
    const GetGLContextCallback& get_gl_context_cb,
    const MakeGLContextCurrentCallback& make_context_current_cb,
    const BindGLImageCallback& bind_image_cb) {
  auto gvdafactory_impl = media::GpuVideoDecodeAcceleratorFactoryImpl::Create(
      get_gl_context_cb, make_context_current_cb, bind_image_cb);
  if (!gvdafactory_impl)
    return nullptr;

  return base::WrapUnique(
      new GpuVideoDecodeAcceleratorFactory(std::move(gvdafactory_impl)));
}

// static
std::unique_ptr<GpuVideoDecodeAcceleratorFactory>
GpuVideoDecodeAcceleratorFactory::CreateWithGLES2Decoder(
    const GetGLContextCallback& get_gl_context_cb,
    const MakeGLContextCurrentCallback& make_context_current_cb,
    const BindGLImageCallback& bind_image_cb,
    const GetGLES2DecoderCallback& get_gles2_decoder_cb) {
  auto gvdafactory_impl =
      media::GpuVideoDecodeAcceleratorFactoryImpl::CreateWithGLES2Decoder(
          get_gl_context_cb, make_context_current_cb, bind_image_cb,
          get_gles2_decoder_cb);
  if (!gvdafactory_impl)
    return nullptr;

  return base::WrapUnique(
      new GpuVideoDecodeAcceleratorFactory(std::move(gvdafactory_impl)));
}

// static
std::unique_ptr<GpuVideoDecodeAcceleratorFactory>
GpuVideoDecodeAcceleratorFactory::CreateWithNoGL() {
  auto gvdafactory_impl =
      media::GpuVideoDecodeAcceleratorFactoryImpl::CreateWithNoGL();
  if (!gvdafactory_impl)
    return nullptr;

  return base::WrapUnique(
      new GpuVideoDecodeAcceleratorFactory(std::move(gvdafactory_impl)));
}

// static
gpu::VideoDecodeAcceleratorCapabilities
GpuVideoDecodeAcceleratorFactory::GetDecoderCapabilities() {
  const gpu::GpuPreferences gpu_preferences =
      GpuChildThread::current()->gpu_preferences();
  return media::GpuVideoDecodeAcceleratorFactoryImpl::GetDecoderCapabilities(
      gpu_preferences);
}

std::unique_ptr<media::VideoDecodeAccelerator>
GpuVideoDecodeAcceleratorFactory::CreateVDA(
    media::VideoDecodeAccelerator::Client* client,
    const media::VideoDecodeAccelerator::Config& config) {
  if (!gvdafactory_impl_)
    return nullptr;

  const gpu::GpuPreferences gpu_preferences =
      GpuChildThread::current()->gpu_preferences();
  return gvdafactory_impl_->CreateVDA(
      client, config, gpu::GpuDriverBugWorkarounds(), gpu_preferences);
}

GpuVideoDecodeAcceleratorFactory::GpuVideoDecodeAcceleratorFactory(
    std::unique_ptr<media::GpuVideoDecodeAcceleratorFactoryImpl>
    gvdafactory_impl) : gvdafactory_impl_(std::move(gvdafactory_impl)) {}

}  // namespace content
