// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebCachePolicy_h
#define WebCachePolicy_h

namespace blink {

enum class WebCachePolicy {
    UseProtocolCachePolicy, // normal load
    ValidatingCacheData, // reload
    BypassingCache, // end-to-end reload
    ReturnCacheDataElseLoad, // back/forward or encoding change - allow stale data
    ReturnCacheDataDontLoad, // results of a post - allow stale data and only use cache
};

} // namespace blink

#endif // WebCachePolicy_h
