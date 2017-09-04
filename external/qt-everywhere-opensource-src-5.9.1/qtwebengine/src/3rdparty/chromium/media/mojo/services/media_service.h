// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MEDIA_SERVICE_H_
#define MEDIA_MOJO_SERVICES_MEDIA_SERVICE_H_

#include <stdint.h>

#include <memory>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "media/mojo/interfaces/interface_factory.mojom.h"
#include "media/mojo/interfaces/media_service.mojom.h"
#include "media/mojo/services/media_mojo_export.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/interface_factory.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/cpp/service_context_ref.h"
#include "url/gurl.h"

namespace media {

class MediaLog;
class MojoMediaClient;

class MEDIA_MOJO_EXPORT MediaService
    : public NON_EXPORTED_BASE(service_manager::Service),
      public NON_EXPORTED_BASE(
          service_manager::InterfaceFactory<mojom::MediaService>),
      public NON_EXPORTED_BASE(mojom::MediaService) {
 public:
  explicit MediaService(std::unique_ptr<MojoMediaClient> mojo_media_client);
  ~MediaService() final;

 private:
  // service_manager::Service implementation.
  void OnStart() final;
  bool OnConnect(const service_manager::ServiceInfo& remote_info,
                 service_manager::InterfaceRegistry* registry) final;
  bool OnStop() final;

  // service_manager::InterfaceFactory<mojom::MediaService> implementation.
  void Create(const service_manager::Identity& remote_identity,
              mojom::MediaServiceRequest request) final;

  // mojom::MediaService implementation.
  void CreateInterfaceFactory(
      mojom::InterfaceFactoryRequest request,
      service_manager::mojom::InterfaceProviderPtr remote_interfaces) final;

  // Note: Since each instance runs on a different thread, do not share a common
  // MojoMediaClient with other instances to avoid threading issues. Hence using
  // a unique_ptr here.
  std::unique_ptr<MojoMediaClient> mojo_media_client_;

  scoped_refptr<MediaLog> media_log_;
  std::unique_ptr<service_manager::ServiceContextRefFactory> ref_factory_;

  mojo::BindingSet<mojom::MediaService> bindings_;
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MEDIA_SERVICE_H_
