// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/media/cast_mojo_media_client.h"

#include "base/memory/ptr_util.h"
#include "chromecast/browser/media/cast_renderer.h"
#include "media/base/cdm_factory.h"
#include "media/base/media_log.h"
#include "media/base/renderer_factory.h"

namespace {

class CastRendererFactory : public media::RendererFactory {
 public:
  CastRendererFactory(
      const chromecast::media::CreateMediaPipelineBackendCB& create_backend_cb,
      const scoped_refptr<media::MediaLog>& media_log)
      : create_backend_cb_(create_backend_cb), media_log_(media_log) {}
  ~CastRendererFactory() final {}

  std::unique_ptr<media::Renderer> CreateRenderer(
      const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
      const scoped_refptr<base::TaskRunner>& worker_task_runner,
      media::AudioRendererSink* audio_renderer_sink,
      media::VideoRendererSink* video_renderer_sink,
      const media::RequestSurfaceCB& request_surface_cb) final {
    DCHECK(!audio_renderer_sink && !video_renderer_sink);
    return base::WrapUnique(new chromecast::media::CastRenderer(
        create_backend_cb_, media_task_runner));
  }

 private:
  const chromecast::media::CreateMediaPipelineBackendCB create_backend_cb_;
  scoped_refptr<media::MediaLog> media_log_;
  DISALLOW_COPY_AND_ASSIGN(CastRendererFactory);
};

}  // namespace

namespace chromecast {
namespace media {

CastMojoMediaClient::CastMojoMediaClient(
    const CreateMediaPipelineBackendCB& create_backend_cb,
    const CreateCdmFactoryCB& create_cdm_factory_cb)
    : create_backend_cb_(create_backend_cb),
      create_cdm_factory_cb_(create_cdm_factory_cb) {}

CastMojoMediaClient::~CastMojoMediaClient() {}

std::unique_ptr<::media::RendererFactory>
CastMojoMediaClient::CreateRendererFactory(
    const scoped_refptr<::media::MediaLog>& media_log) {
  return base::WrapUnique(
      new CastRendererFactory(create_backend_cb_, media_log));
}

std::unique_ptr<::media::CdmFactory> CastMojoMediaClient::CreateCdmFactory(
    ::shell::mojom::InterfaceProvider* interface_provider) {
  return create_cdm_factory_cb_.Run();
}

}  // namespace media
}  // namespace chromecast
