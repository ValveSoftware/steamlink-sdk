// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebFrameOwnerProperties_h
#define WebFrameOwnerProperties_h

#include "public/platform/WebVector.h"
#include "public/platform/modules/permissions/WebPermissionType.h"
#include <algorithm>

namespace blink {

struct WebFrameOwnerProperties {
    enum class ScrollingMode {
        Auto,
        AlwaysOff,
        AlwaysOn,
        Last = AlwaysOn
    };

    ScrollingMode scrollingMode;
    int marginWidth;
    int marginHeight;
    bool allowFullscreen;
    WebVector<WebPermissionType> delegatedPermissions;

    WebFrameOwnerProperties()
        : scrollingMode(ScrollingMode::Auto)
        , marginWidth(-1)
        , marginHeight(-1)
        , allowFullscreen(false)
    {
    }

#if INSIDE_BLINK
    WebFrameOwnerProperties(ScrollbarMode scrollingMode, int marginWidth, int marginHeight, bool allowFullscreen, const WebVector<WebPermissionType>& delegatedPermissions)
        : scrollingMode(static_cast<ScrollingMode>(scrollingMode))
        , marginWidth(marginWidth)
        , marginHeight(marginHeight)
        , allowFullscreen(allowFullscreen)
        , delegatedPermissions(delegatedPermissions)
    {
    }
#endif

    bool operator==(const WebFrameOwnerProperties& other) const
    {
        return scrollingMode == other.scrollingMode
            && marginWidth == other.marginWidth
            && marginHeight == other.marginHeight
            && allowFullscreen == other.allowFullscreen
            && std::equal(delegatedPermissions.begin(),
                delegatedPermissions.end(),
                other.delegatedPermissions.begin());
    }

    bool operator!=(const WebFrameOwnerProperties& other) const
    {
        return !(*this == other);
    }
};

} // namespace blink

#endif
