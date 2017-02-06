// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_ANDROID_PROVISION_FETCHER_IMPL_H_
#define CONTENT_BROWSER_MEDIA_ANDROID_PROVISION_FETCHER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/android/provision_fetcher_factory.h"
#include "media/base/android/provision_fetcher.h"
#include "media/mojo/interfaces/provision_fetcher.mojom.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace content {

class RenderFrameHost;

// A media::mojom::ProvisionFetcher implementation based on
// media::ProvisionFetcher.
class ProvisionFetcherImpl : public media::mojom::ProvisionFetcher {
 public:
  static void Create(
      RenderFrameHost* render_frame_host,
      mojo::InterfaceRequest<media::mojom::ProvisionFetcher> request);

  ProvisionFetcherImpl(
      std::unique_ptr<media::ProvisionFetcher> provision_fetcher,
      mojo::InterfaceRequest<ProvisionFetcher> request);
  ~ProvisionFetcherImpl() override;

  // media::mojom::ProvisionFetcher implementation.
  void Retrieve(const mojo::String& default_url,
                const mojo::String& request_data,
                const RetrieveCallback& callback) final;

 private:
  // Callback for media::ProvisionFetcher::Retrieve().
  void OnResponse(const RetrieveCallback& callback,
                  bool success,
                  const std::string& response);

  mojo::StrongBinding<media::mojom::ProvisionFetcher> binding_;
  std::unique_ptr<media::ProvisionFetcher> provision_fetcher_;

  base::WeakPtrFactory<ProvisionFetcherImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProvisionFetcherImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_ANDROID_PROVISION_FETCHER_IMPL_H_
