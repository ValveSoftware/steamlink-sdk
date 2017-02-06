// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef URLConversion_h
#define URLConversion_h

#include "WebCommon.h"

class GURL;

namespace blink {

class WebString;

BLINK_COMMON_EXPORT GURL WebStringToGURL(const WebString&);

} // namespace blink

#endif
