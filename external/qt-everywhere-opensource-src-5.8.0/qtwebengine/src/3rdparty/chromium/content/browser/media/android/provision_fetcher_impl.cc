// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/android/provision_fetcher_impl.h"

#include "content/public/browser/android/provision_fetcher_factory.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "net/url_request/url_request_context_getter.h"

namespace content {

// static
void ProvisionFetcherImpl::Create(
    RenderFrameHost* render_frame_host,
    mojo::InterfaceRequest<media::mojom::ProvisionFetcher> request) {
  net::URLRequestContextGetter* context_getter =
      BrowserContext::GetDefaultStoragePartition(
          render_frame_host->GetProcess()->GetBrowserContext())->
              GetURLRequestContext();
  DCHECK(context_getter);

  // The created object is strongly bound to (and owned by) the pipe.
  new ProvisionFetcherImpl(CreateProvisionFetcher(context_getter),
                           std::move(request));
}

ProvisionFetcherImpl::ProvisionFetcherImpl(
    std::unique_ptr<media::ProvisionFetcher> provision_fetcher,
    mojo::InterfaceRequest<ProvisionFetcher> request)
    : binding_(this, std::move(request)),
      provision_fetcher_(std::move(provision_fetcher)),
      weak_factory_(this) {
  DVLOG(1) << __FUNCTION__;
}

ProvisionFetcherImpl::~ProvisionFetcherImpl() {}

void ProvisionFetcherImpl::Retrieve(const mojo::String& default_url,
                                    const mojo::String& request_data,
                                    const RetrieveCallback& callback) {
  DVLOG(1) << __FUNCTION__ << ": " << default_url;
  provision_fetcher_->Retrieve(
      default_url, request_data,
      base::Bind(&ProvisionFetcherImpl::OnResponse, weak_factory_.GetWeakPtr(),
                 callback));
}

void ProvisionFetcherImpl::OnResponse(const RetrieveCallback& callback,
                                      bool success,
                                      const std::string& response) {
  DVLOG(1) << __FUNCTION__ << ": " << success;
  callback.Run(success, response);
}

}  // namespace content
