// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebIconSizesParser_h
#define WebIconSizesParser_h

#include "WebCommon.h"
#include "WebVector.h"

namespace blink {

class WebString;
struct WebSize;

// Helper class for parsing icon sizes. The spec is:
// https://html.spec.whatwg.org/multipage/semantics.html#attr-link-sizes
// TODO(zqzhang): merge with WebIconURL, and rename it "WebIcon"?
class WebIconSizesParser {
 public:
  BLINK_PLATFORM_EXPORT static WebVector<WebSize> parseIconSizes(
      const WebString& sizesString);
};

}  // namespace blink

#endif
