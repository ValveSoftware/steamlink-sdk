// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/fetchers/manifest_fetcher.h"

#include "base/bind.h"
#include "base/logging.h"
#include "content/public/renderer/associated_resource_fetcher.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/web/WebAssociatedURLLoaderOptions.h"
#include "third_party/WebKit/public/web/WebFrame.h"

namespace content {

ManifestFetcher::ManifestFetcher(const GURL& url)
    : completed_(false) {
  fetcher_.reset(AssociatedResourceFetcher::Create(url));
}

ManifestFetcher::~ManifestFetcher() {
  if (!completed_)
    Cancel();
}

void ManifestFetcher::Start(blink::WebFrame* frame,
                            bool use_credentials,
                            const Callback& callback) {
  callback_ = callback;

  blink::WebAssociatedURLLoaderOptions options;
  options.allowCredentials = use_credentials;
  options.crossOriginRequestPolicy = blink::WebAssociatedURLLoaderOptions::
      CrossOriginRequestPolicyUseAccessControl;
  fetcher_->SetLoaderOptions(options);

  fetcher_->Start(frame,
                  blink::WebURLRequest::RequestContextManifest,
                  blink::WebURLRequest::FrameTypeNone,
                  base::Bind(&ManifestFetcher::OnLoadComplete,
                             base::Unretained(this)));
}

void ManifestFetcher::Cancel() {
  DCHECK(!completed_);
  fetcher_->Cancel();
}

void ManifestFetcher::OnLoadComplete(const blink::WebURLResponse& response,
                                     const std::string& data) {
  DCHECK(!completed_);
  completed_ = true;

  Callback callback = callback_;
  callback.Run(response, data);
}

} // namespace content
