// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/default_mojo_media_client.h"

#include "media/base/cdm_factory.h"

namespace media {

DefaultMojoMediaClient::DefaultMojoMediaClient() {}

DefaultMojoMediaClient::~DefaultMojoMediaClient() {}

void DefaultMojoMediaClient::Initialize() {
  // TODO(jrummell): Do one-time initialization work here.
}

std::unique_ptr<CdmFactory> DefaultMojoMediaClient::CreateCdmFactory(
    shell::mojom::InterfaceProvider* /* interface_provider */) {
  DVLOG(1) << __FUNCTION__;
  // TODO(jrummell): Return a CdmFactory that can create CdmAdapter here.
  return nullptr;
}

}  // namespace media
