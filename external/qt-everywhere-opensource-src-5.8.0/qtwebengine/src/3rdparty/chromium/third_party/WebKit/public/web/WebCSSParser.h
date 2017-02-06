// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebCSSParser_h
#define WebCSSParser_h

#include "public/platform/WebColor.h"
#include "public/platform/WebCommon.h"

namespace blink {

class WebString;

class WebCSSParser {
public:
    BLINK_EXPORT static bool parseColor(WebColor*, const WebString&);
};

} // namespace blink

#endif
