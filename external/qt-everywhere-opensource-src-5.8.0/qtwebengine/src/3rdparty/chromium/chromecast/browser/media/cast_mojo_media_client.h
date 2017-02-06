// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_MEDIA_CAST_MOJO_MEDIA_CLIENT_H_
#define CHROMECAST_BROWSER_MEDIA_CAST_MOJO_MEDIA_CLIENT_H_

#include "chromecast/browser/media/media_pipeline_backend_factory.h"
#include "media/mojo/services/mojo_media_client.h"

namespace chromecast {
namespace media {

class CastMojoMediaClient : public ::media::MojoMediaClient {
 public:
  using CreateCdmFactoryCB =
      base::Callback<std::unique_ptr<::media::CdmFactory>()>;

  CastMojoMediaClient(const CreateMediaPipelineBackendCB& create_backend_cb,
                      const CreateCdmFactoryCB& create_cdm_factory_cb);
  ~CastMojoMediaClient() override;

  // MojoMediaClient overrides.
  std::unique_ptr<::media::RendererFactory> CreateRendererFactory(
      const scoped_refptr<::media::MediaLog>& media_log) override;
  std::unique_ptr<::media::CdmFactory> CreateCdmFactory(
      ::shell::mojom::InterfaceProvider* interface_provider) override;

 private:
  const CreateMediaPipelineBackendCB create_backend_cb_;
  const CreateCdmFactoryCB create_cdm_factory_cb_;

  DISALLOW_COPY_AND_ASSIGN(CastMojoMediaClient);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_MEDIA_CAST_MOJO_MEDIA_CLIENT_H_
