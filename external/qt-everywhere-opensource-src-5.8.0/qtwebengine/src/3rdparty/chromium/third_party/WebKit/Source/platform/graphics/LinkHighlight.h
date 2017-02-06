// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LinkHighlight_h
#define LinkHighlight_h

#include "platform/PlatformExport.h"

namespace blink {

class WebLayer;

class PLATFORM_EXPORT LinkHighlight {
public:
    virtual void invalidate() = 0;
    virtual void clearCurrentGraphicsLayer() = 0;
    virtual WebLayer* layer() = 0;

protected:
    virtual ~LinkHighlight() { }
};

} // namespace blink

#endif // LinkHighlight_h
