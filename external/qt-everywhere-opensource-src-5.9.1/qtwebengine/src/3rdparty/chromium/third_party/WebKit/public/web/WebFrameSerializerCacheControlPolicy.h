// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebFrameSerializerCacheControlPolicy_h
#define WebFrameSerializerCacheControlPolicy_h

namespace blink {

// WebFrameSerializerCacheControlPolicy configures how WebFrameSerializer
// processes resources with different Cache-Control headers. By default, frame
// serialization encompasses all resources in a page.  If it is desirable to
// serialize only those resources that could be stored by a http cache, the
// other options may be used.
// TODO(dewittj): Add more policies for subframes and subresources.
enum class WebFrameSerializerCacheControlPolicy {
  None = 0,
  FailForNoStoreMainFrame,
  SkipAnyFrameOrResourceMarkedNoStore,
  Last = SkipAnyFrameOrResourceMarkedNoStore,
};

}  // namespace blink

#endif  // WebFrameSerializerCacheControlPolicy_h
