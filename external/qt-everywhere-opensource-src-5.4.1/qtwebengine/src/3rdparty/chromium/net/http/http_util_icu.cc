// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The rules for parsing content-types were borrowed from Firefox:
// http://lxr.mozilla.org/mozilla/source/netwerk/base/src/nsURLHelper.cpp#834

#include "net/http/http_util.h"

#include "base/logging.h"
#include "net/base/net_util.h"

namespace net {

// static
std::string HttpUtil::PathForRequest(const GURL& url) {
  DCHECK(url.is_valid() && (url.SchemeIsHTTPOrHTTPS() ||
                            url.SchemeIsWSOrWSS()));
  if (url.has_query())
    return url.path() + "?" + url.query();
  return url.path();
}

// static
std::string HttpUtil::SpecForRequest(const GURL& url) {
  // We may get ftp scheme when fetching ftp resources through proxy.
  DCHECK(url.is_valid() && (url.SchemeIsHTTPOrHTTPS() || url.SchemeIs("ftp") ||
                            url.SchemeIsWSOrWSS()));
  return SimplifyUrlForRequest(url).spec();
}

}  // namespace net
