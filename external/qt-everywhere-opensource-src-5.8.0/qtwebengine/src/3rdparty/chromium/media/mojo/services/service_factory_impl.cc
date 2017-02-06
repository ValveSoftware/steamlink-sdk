// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/service_factory_impl.h"

#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/media_log.h"
#include "media/mojo/services/mojo_media_client.h"
#include "services/shell/public/interfaces/interface_provider.mojom.h"

#if defined(ENABLE_MOJO_AUDIO_DECODER)
#include "media/mojo/services/mojo_audio_decoder_service.h"
#endif  // defined(ENABLE_MOJO_AUDIO_DECODER)

#if defined(ENABLE_MOJO_VIDEO_DECODER)
#include "media/mojo/services/mojo_video_decoder_service.h"
#endif  // defined(ENABLE_MOJO_VIDEO_DECODER)

#if defined(ENABLE_MOJO_RENDERER)
#include "media/base/renderer_factory.h"
#include "media/mojo/services/mojo_renderer_service.h"
#endif  // defined(ENABLE_MOJO_RENDERER)

#if defined(ENABLE_MOJO_CDM)
#include "media/base/cdm_factory.h"
#include "media/mojo/services/mojo_cdm_service.h"
#endif  // defined(ENABLE_MOJO_CDM)

namespace media {

ServiceFactoryImpl::ServiceFactoryImpl(
    mojo::InterfaceRequest<mojom::ServiceFactory> request,
    shell::mojom::InterfaceProvider* interfaces,
    scoped_refptr<MediaLog> media_log,
    std::unique_ptr<shell::ShellConnectionRef> connection_ref,
    MojoMediaClient* mojo_media_client)
    : binding_(this, std::move(request)),
#if defined(ENABLE_MOJO_CDM)
      interfaces_(interfaces),
#endif
      media_log_(media_log),
      connection_ref_(std::move(connection_ref)),
      mojo_media_client_(mojo_media_client) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(mojo_media_client_);
}

ServiceFactoryImpl::~ServiceFactoryImpl() {
  DVLOG(1) << __FUNCTION__;
}

// mojom::ServiceFactory implementation.

void ServiceFactoryImpl::CreateAudioDecoder(
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

  new MojoAudioDecoderService(cdm_service_context_.GetWeakPtr(),
                              std::move(audio_decoder), std::move(request));
#endif  // defined(ENABLE_MOJO_AUDIO_DECODER)
}

void ServiceFactoryImpl::CreateVideoDecoder(
    mojom::VideoDecoderRequest request) {
#if defined(ENABLE_MOJO_VIDEO_DECODER)
  new MojoVideoDecoderService(std::move(request), mojo_media_client_);
#endif  // defined(ENABLE_MOJO_VIDEO_DECODER)
}

void ServiceFactoryImpl::CreateRenderer(
    mojo::InterfaceRequest<mojom::Renderer> request) {
#if defined(ENABLE_MOJO_RENDERER)
  // The created object is owned by the pipe.
  // The audio and video sinks are owned by the client.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner(
      base::ThreadTaskRunnerHandle::Get());
  AudioRendererSink* audio_renderer_sink =
      mojo_media_client_->CreateAudioRendererSink();
  VideoRendererSink* video_renderer_sink =
      mojo_media_client_->CreateVideoRendererSink(task_runner);

  RendererFactory* renderer_factory = GetRendererFactory();
  if (!renderer_factory)
    return;

  std::unique_ptr<Renderer> renderer = renderer_factory->CreateRenderer(
      task_runner, task_runner, audio_renderer_sink, video_renderer_sink,
      RequestSurfaceCB());
  if (!renderer) {
    LOG(ERROR) << "Renderer creation failed.";
    return;
  }

  new MojoRendererService(cdm_service_context_.GetWeakPtr(),
                          std::move(renderer), std::move(request));
#endif  // defined(ENABLE_MOJO_RENDERER)
}

void ServiceFactoryImpl::CreateCdm(
    mojo::InterfaceRequest<mojom::ContentDecryptionModule> request) {
#if defined(ENABLE_MOJO_CDM)
  CdmFactory* cdm_factory = GetCdmFactory();
  if (!cdm_factory)
    return;

  // The created object is owned by the pipe.
  new MojoCdmService(cdm_service_context_.GetWeakPtr(), cdm_factory,
                     std::move(request));
#endif  // defined(ENABLE_MOJO_CDM)
}

#if defined(ENABLE_MOJO_RENDERER)
RendererFactory* ServiceFactoryImpl::GetRendererFactory() {
  if (!renderer_factory_) {
    renderer_factory_ = mojo_media_client_->CreateRendererFactory(media_log_);
    LOG_IF(ERROR, !renderer_factory_) << "RendererFactory not available.";
  }
  return renderer_factory_.get();
}
#endif  // defined(ENABLE_MOJO_RENDERER)

#if defined(ENABLE_MOJO_CDM)
CdmFactory* ServiceFactoryImpl::GetCdmFactory() {
  if (!cdm_factory_) {
    cdm_factory_ = mojo_media_client_->CreateCdmFactory(interfaces_);
    LOG_IF(ERROR, !cdm_factory_) << "CdmFactory not available.";
  }
  return cdm_factory_.get();
}
#endif  // defined(ENABLE_MOJO_CDM)

}  // namespace media
