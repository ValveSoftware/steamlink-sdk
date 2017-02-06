// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_URL_FETCHER_H_
#define CONTENT_PUBLIC_COMMON_URL_FETCHER_H_

#include "content/common/content_export.h"

class GURL;

namespace net {
class URLFetcher;
}  // namespace

namespace content {

// Mark URLRequests started by the URLFetcher to stem from the given render
// frame.
CONTENT_EXPORT void AssociateURLFetcherWithRenderFrame(
    net::URLFetcher* url_fetcher,
    const GURL& first_party_for_cookies,
    int render_process_id,
    int render_frame_id);

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_URL_FETCHER_H_
