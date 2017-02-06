// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_DEFAULT_MOJO_MEDIA_CLIENT_H_
#define MEDIA_MOJO_SERVICES_DEFAULT_MOJO_MEDIA_CLIENT_H_

#include <memory>

#include "base/macros.h"
#include "media/mojo/services/mojo_media_client.h"

namespace media {

// Default MojoMediaClient for MojoMediaApplication.
class DefaultMojoMediaClient : public MojoMediaClient {
 public:
  DefaultMojoMediaClient();
  ~DefaultMojoMediaClient() final;

  // MojoMediaClient implementation.
  void Initialize() final;
  std::unique_ptr<CdmFactory> CreateCdmFactory(
      shell::mojom::InterfaceProvider* /* interface_provider */) final;

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultMojoMediaClient);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_DEFAULT_MOJO_MEDIA_CLIENT_H_
