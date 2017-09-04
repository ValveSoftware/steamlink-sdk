// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_INTERFACE_PROVIDER_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_INTERFACE_PROVIDER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "media/mojo/interfaces/interface_factory.mojom.h"
#include "services/service_manager/public/interfaces/interface_provider.mojom.h"
#include "url/gurl.h"

namespace service_manager {
class InterfaceProvider;
}

namespace content {

// MediaInterfaceProvider is an implementation of mojo InterfaceProvider that
// provides media related services and handles disconnection automatically.
// This class is single threaded.
class CONTENT_EXPORT MediaInterfaceProvider
    : public service_manager::mojom::InterfaceProvider {
 public:
  explicit MediaInterfaceProvider(
      service_manager::InterfaceProvider* remote_interfaces);
  ~MediaInterfaceProvider() final;

  // InterfaceProvider implementation.
  void GetInterface(const std::string& interface_name,
                    mojo::ScopedMessagePipeHandle pipe) final;

 private:
  media::mojom::InterfaceFactory* GetMediaInterfaceFactory();
  void OnConnectionError();

  base::ThreadChecker thread_checker_;
  service_manager::InterfaceProvider* remote_interfaces_;
  media::mojom::InterfaceFactoryPtr media_interface_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaInterfaceProvider);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_INTERFACE_PROVIDER_H_
