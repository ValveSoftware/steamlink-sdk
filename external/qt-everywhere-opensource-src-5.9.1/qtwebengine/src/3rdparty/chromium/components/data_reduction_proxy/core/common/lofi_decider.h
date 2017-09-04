// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_LOFI_DECIDER_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_LOFI_DECIDER_H_

#include "base/macros.h"

namespace net {
class HttpRequestHeaders;
class URLRequest;
}

namespace data_reduction_proxy {

// Interface to determine if a request should be made for a low fidelity version
// of the resource.
class LoFiDecider {
 public:
  virtual ~LoFiDecider() {}

  // Returns true when Lo-Fi mode is on for the given |request|. This means the
  // Lo-Fi header should be added to the given request.
  virtual bool IsUsingLoFiMode(const net::URLRequest& request) const = 0;

  // Adds a previews-specific directive to the Chrome-Proxy-Accept-Transform
  // header if needed. If a slow page preview is triggered, adds "lite-page" or
  // "empty-image", depending on whether the request is for the main frame
  // and lite pages are enabled, or for a subresource and Lo-Fi mode is enabled,
  // respectively. If a slow page preview is not triggered, "lite-page;if-heavy"
  // and "empty-image;if-heavy" are added in the respective aforementioned cases
  // to request that the server transform the page if it determines it to be
  // heavy. Previews-related transformation headers are never added if
  // |is_previews_disabled| is true.
  virtual void MaybeSetAcceptTransformHeader(
      const net::URLRequest& request,
      bool is_previews_disabled,
      net::HttpRequestHeaders* headers) const = 0;

  // Returns true if |headers| contains the Chrome-Proxy-Accept-Transform
  // header and a slow page previews directive ("lite-page" or "empty-image")
  // is present and not conditioned on "if-heavy".
  virtual bool IsSlowPagePreviewRequested(
      const net::HttpRequestHeaders& headers) const = 0;

  // Returns true if |headers| contains the Chrome-Proxy-Accept-Transform
  // header with the "lite-page" directive.
  virtual bool IsLitePagePreviewRequested(
      const net::HttpRequestHeaders& headers) const = 0;

  // Unconditionally removes the Chrome-Proxy-Accept-Transform header from
  // |headers.|
  virtual void RemoveAcceptTransformHeader(
      net::HttpRequestHeaders* headers) const = 0;

  // Adds a directive to tell the server to ignore blacklists when a Lite Page
  // preview is being requested due to command line flags being set.
  virtual void MaybeSetIgnorePreviewsBlacklistDirective(
      net::HttpRequestHeaders* headers) const = 0;

  // Returns true if the Lo-Fi specific UMA should be recorded. It is set to
  // true if Lo-Fi is enabled for |request|, Chrome session is in Lo-Fi
  // Enabled or Control field trial, and the network quality was slow.
  virtual bool ShouldRecordLoFiUMA(const net::URLRequest& request) const = 0;
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_LOFI_DECIDER_H_
