// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/media_service.h"

#include <utility>

#include "media/base/media_log.h"
#include "media/mojo/services/interface_factory_impl.h"
#include "media/mojo/services/mojo_media_client.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/service_manager/public/cpp/connection.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/interface_registry.h"

namespace media {

// TODO(xhwang): Hook up MediaLog when possible.
MediaService::MediaService(std::unique_ptr<MojoMediaClient> mojo_media_client)
    : mojo_media_client_(std::move(mojo_media_client)),
      media_log_(new MediaLog()) {
  DCHECK(mojo_media_client_);
}

MediaService::~MediaService() {}

void MediaService::OnStart() {
  ref_factory_.reset(new service_manager::ServiceContextRefFactory(
      base::Bind(&service_manager::ServiceContext::RequestQuit,
                 base::Unretained(context()))));
  mojo_media_client_->Initialize();
}

bool MediaService::OnConnect(const service_manager::ServiceInfo& remote_info,
                             service_manager::InterfaceRegistry* registry) {
  registry->AddInterface<mojom::MediaService>(this);
  return true;
}

bool MediaService::OnStop() {
  mojo_media_client_.reset();
  return true;
}

void MediaService::Create(const service_manager::Identity& remote_identity,
                          mojom::MediaServiceRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void MediaService::CreateInterfaceFactory(
    mojom::InterfaceFactoryRequest request,
    service_manager::mojom::InterfaceProviderPtr remote_interfaces) {
  // Ignore request if service has already stopped.
  if (!mojo_media_client_)
    return;

  mojo::MakeStrongBinding(
      base::MakeUnique<InterfaceFactoryImpl>(
          std::move(remote_interfaces), media_log_, ref_factory_->CreateRef(),
          mojo_media_client_.get()),
      std::move(request));
}

}  // namespace media
