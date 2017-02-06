// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_INTERFACE_PROVIDER_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_INTERFACE_PROVIDER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "media/mojo/interfaces/service_factory.mojom.h"
#include "services/shell/public/interfaces/interface_provider.mojom.h"
#include "url/gurl.h"

namespace content {

// MediaInterfaceProvider is an implementation of mojo InterfaceProvider that
// provides media related services and handles app disconnection automatically.
// This class is single threaded.
class CONTENT_EXPORT MediaInterfaceProvider
    : public shell::mojom::InterfaceProvider {
 public:
  // Callback used to connect to a mojo application.
  using ConnectToApplicationCB =
      base::Callback<shell::mojom::InterfaceProviderPtr(const GURL& url)>;

  explicit MediaInterfaceProvider(
      const ConnectToApplicationCB& connect_to_app_cb);
  ~MediaInterfaceProvider() final;

  // InterfaceProvider implementation.
  void GetInterface(const mojo::String& interface_name,
                    mojo::ScopedMessagePipeHandle pipe) final;

 private:
  media::mojom::ServiceFactory* GetMediaServiceFactory();
  void OnConnectionError();

  base::ThreadChecker thread_checker_;
  ConnectToApplicationCB connect_to_app_cb_;
  media::mojom::ServiceFactoryPtr media_service_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaInterfaceProvider);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_INTERFACE_PROVIDER_H_
