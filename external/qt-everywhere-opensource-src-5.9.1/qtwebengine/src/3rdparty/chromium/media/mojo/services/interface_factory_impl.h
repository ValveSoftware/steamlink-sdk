// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_INTERFACE_FACTORY_IMPL_H_
#define MEDIA_MOJO_SERVICES_INTERFACE_FACTORY_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "media/mojo/interfaces/interface_factory.mojom.h"
#include "media/mojo/services/mojo_cdm_service_context.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service_context_ref.h"

namespace service_manager {
namespace mojom {
class InterfaceProvider;
}
}

namespace media {

class CdmFactory;
class MediaLog;
class MojoMediaClient;
class RendererFactory;

class InterfaceFactoryImpl : public mojom::InterfaceFactory {
 public:
  InterfaceFactoryImpl(
      service_manager::mojom::InterfaceProviderPtr interfaces,
      scoped_refptr<MediaLog> media_log,
      std::unique_ptr<service_manager::ServiceContextRef> connection_ref,
      MojoMediaClient* mojo_media_client);
  ~InterfaceFactoryImpl() final;

  // mojom::InterfaceFactory implementation.
  void CreateAudioDecoder(mojom::AudioDecoderRequest request) final;
  void CreateVideoDecoder(mojom::VideoDecoderRequest request) final;
  void CreateRenderer(const std::string& audio_device_id,
                      mojom::RendererRequest request) final;
  void CreateCdm(mojom::ContentDecryptionModuleRequest request) final;

 private:
#if defined(ENABLE_MOJO_RENDERER)
  RendererFactory* GetRendererFactory();
#endif  // defined(ENABLE_MOJO_RENDERER)

#if defined(ENABLE_MOJO_CDM)
  CdmFactory* GetCdmFactory();
#endif  // defined(ENABLE_MOJO_CDM)

  MojoCdmServiceContext cdm_service_context_;

#if defined(ENABLE_MOJO_RENDERER)
  std::unique_ptr<RendererFactory> renderer_factory_;
#endif  // defined(ENABLE_MOJO_RENDERER)

#if defined(ENABLE_MOJO_CDM)
  std::unique_ptr<CdmFactory> cdm_factory_;
  service_manager::mojom::InterfaceProviderPtr interfaces_;
#endif  // defined(ENABLE_MOJO_CDM)

  scoped_refptr<MediaLog> media_log_;
  std::unique_ptr<service_manager::ServiceContextRef> connection_ref_;
  MojoMediaClient* mojo_media_client_;

  DISALLOW_COPY_AND_ASSIGN(InterfaceFactoryImpl);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_INTERFACE_FACTORY_IMPL_H_
