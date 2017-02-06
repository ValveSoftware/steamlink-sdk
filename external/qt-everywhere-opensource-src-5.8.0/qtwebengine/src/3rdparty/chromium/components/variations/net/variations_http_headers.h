// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_NET_VARIATIONS_HTTP_HEADERS_H_
#define COMPONENTS_VARIATIONS_NET_VARIATIONS_HTTP_HEADERS_H_

#include <set>
#include <string>

namespace net {
class HttpRequestHeaders;
}

class GURL;

namespace variations {

// Adds Chrome experiment and metrics state as custom headers to |headers|.
// Some headers may not be set given the |incognito| mode or whether
// the user has |uma_enabled|.  Also, we never transmit headers to non-Google
// sites, which is checked based on the destination |url|.
void AppendVariationHeaders(const GURL& url,
                            bool incognito,
                            bool uma_enabled,
                            net::HttpRequestHeaders* headers);

// Returns the HTTP header names which are added by AppendVariationHeaders().
std::set<std::string> GetVariationHeaderNames();

namespace internal {

// Checks whether variation headers should be appended to requests to the
// specified |url|. Returns true for google.<TLD> and youtube.<TLD> URLs.
bool ShouldAppendVariationHeaders(const GURL& url);

}  // namespace internal

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_NET_VARIATIONS_HTTP_HEADERS_H_
