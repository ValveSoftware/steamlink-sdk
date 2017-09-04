// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/interface_factory_impl.h"

#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/media_log.h"
#include "media/mojo/services/mojo_media_client.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/service_manager/public/interfaces/interface_provider.mojom.h"

#if defined(ENABLE_MOJO_AUDIO_DECODER)
#include "media/mojo/services/mojo_audio_decoder_service.h"
#endif  // defined(ENABLE_MOJO_AUDIO_DECODER)

#if defined(ENABLE_MOJO_VIDEO_DECODER)
#include "media/mojo/services/mojo_video_decoder_service.h"
#endif  // defined(ENABLE_MOJO_VIDEO_DECODER)

#if defined(ENABLE_MOJO_RENDERER)
#include "media/base/audio_renderer_sink.h"
#include "media/base/renderer_factory.h"
#include "media/base/video_renderer_sink.h"
#include "media/mojo/services/mojo_renderer_service.h"
#endif  // defined(ENABLE_MOJO_RENDERER)

#if defined(ENABLE_MOJO_CDM)
#include "media/base/cdm_factory.h"
#include "media/mojo/services/mojo_cdm_service.h"
#endif  // defined(ENABLE_MOJO_CDM)

namespace media {

InterfaceFactoryImpl::InterfaceFactoryImpl(
    service_manager::mojom::InterfaceProviderPtr interfaces,
    scoped_refptr<MediaLog> media_log,
    std::unique_ptr<service_manager::ServiceContextRef> connection_ref,
    MojoMediaClient* mojo_media_client)
    :
#if defined(ENABLE_MOJO_CDM)
      interfaces_(std::move(interfaces)),
#endif
      media_log_(media_log),
      connection_ref_(std::move(connection_ref)),
      mojo_media_client_(mojo_media_client) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(mojo_media_client_);
}

InterfaceFactoryImpl::~InterfaceFactoryImpl() {
  DVLOG(1) << __FUNCTION__;
}

// mojom::InterfaceFactory implementation.

void InterfaceFactoryImpl::CreateAudioDecoder(
    mojo::InterfaceRequest<mojom::AudioDecoder> request) {
#if defined(ENABLE_MOJO_AUDIO_DECODER)
  scoped_refptr<base::SingleThreadTaskRunner> task_runner(
      base::ThreadTaskRunnerHandle::Get());

  std::unique_ptr<AudioDecoder> audio_decoder =
      mojo_media_client_->CreateAudioDecoder(task_runner);
  if (!audio_decoder) {
    LOG(ERROR) << "AudioDecoder creation failed.";
    return;
  }

  mojo::MakeStrongBinding(
      base::MakeUnique<MojoAudioDecoderService>(
          cdm_service_context_.GetWeakPtr(), std::move(audio_decoder)),
      std::move(request));
#endif  // defined(ENABLE_MOJO_AUDIO_DECODER)
}

void InterfaceFactoryImpl::CreateVideoDecoder(
    mojom::VideoDecoderRequest request) {
#if defined(ENABLE_MOJO_VIDEO_DECODER)
  mojo::MakeStrongBinding(
      base::MakeUnique<MojoVideoDecoderService>(mojo_media_client_),
      std::move(request));
#endif  // defined(ENABLE_MOJO_VIDEO_DECODER)
}

void InterfaceFactoryImpl::CreateRenderer(
    const std::string& audio_device_id,
    mojo::InterfaceRequest<mojom::Renderer> request) {
#if defined(ENABLE_MOJO_RENDERER)
  RendererFactory* renderer_factory = GetRendererFactory();
  if (!renderer_factory)
    return;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner(
      base::ThreadTaskRunnerHandle::Get());
  auto audio_sink =
      mojo_media_client_->CreateAudioRendererSink(audio_device_id);
  auto video_sink = mojo_media_client_->CreateVideoRendererSink(task_runner);
  auto renderer = renderer_factory->CreateRenderer(
      task_runner, task_runner, audio_sink.get(), video_sink.get(),
      RequestSurfaceCB());
  if (!renderer) {
    LOG(ERROR) << "Renderer creation failed.";
    return;
  }

  MojoRendererService::Create(
      cdm_service_context_.GetWeakPtr(), std::move(audio_sink),
      std::move(video_sink), std::move(renderer),
      MojoRendererService::InitiateSurfaceRequestCB(), std::move(request));
#endif  // defined(ENABLE_MOJO_RENDERER)
}

void InterfaceFactoryImpl::CreateCdm(
    mojo::InterfaceRequest<mojom::ContentDecryptionModule> request) {
#if defined(ENABLE_MOJO_CDM)
  CdmFactory* cdm_factory = GetCdmFactory();
  if (!cdm_factory)
    return;

  mojo::MakeStrongBinding(base::MakeUnique<MojoCdmService>(
                              cdm_service_context_.GetWeakPtr(), cdm_factory),
                          std::move(request));
#endif  // defined(ENABLE_MOJO_CDM)
}

#if defined(ENABLE_MOJO_RENDERER)
RendererFactory* InterfaceFactoryImpl::GetRendererFactory() {
  if (!renderer_factory_) {
    renderer_factory_ = mojo_media_client_->CreateRendererFactory(media_log_);
    LOG_IF(ERROR, !renderer_factory_) << "RendererFactory not available.";
  }
  return renderer_factory_.get();
}
#endif  // defined(ENABLE_MOJO_RENDERER)

#if defined(ENABLE_MOJO_CDM)
CdmFactory* InterfaceFactoryImpl::GetCdmFactory() {
  if (!cdm_factory_) {
    cdm_factory_ = mojo_media_client_->CreateCdmFactory(interfaces_.get());
    LOG_IF(ERROR, !cdm_factory_) << "CdmFactory not available.";
  }
  return cdm_factory_.get();
}
#endif  // defined(ENABLE_MOJO_CDM)

}  // namespace media
