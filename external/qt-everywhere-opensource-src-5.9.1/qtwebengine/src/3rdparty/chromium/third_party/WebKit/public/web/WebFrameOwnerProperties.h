// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebFrameOwnerProperties_h
#define WebFrameOwnerProperties_h

#include "public/platform/WebString.h"
#include "public/platform/WebVector.h"
#include "public/platform/modules/permissions/WebPermissionType.h"
#include <algorithm>

namespace blink {

struct WebFrameOwnerProperties {
  enum class ScrollingMode { Auto, AlwaysOff, AlwaysOn, Last = AlwaysOn };

  ScrollingMode scrollingMode;
  int marginWidth;
  int marginHeight;
  bool allowFullscreen;
  bool allowPaymentRequest;
  WebString requiredCsp;
  WebVector<WebPermissionType> delegatedPermissions;

  WebFrameOwnerProperties()
      : scrollingMode(ScrollingMode::Auto),
        marginWidth(-1),
        marginHeight(-1),
        allowFullscreen(false),
        allowPaymentRequest(false) {}

#if INSIDE_BLINK
  WebFrameOwnerProperties(
      ScrollbarMode scrollingMode,
      int marginWidth,
      int marginHeight,
      bool allowFullscreen,
      bool allowPaymentRequest,
      const WebString& requiredCsp,
      const WebVector<WebPermissionType>& delegatedPermissions)
      : scrollingMode(static_cast<ScrollingMode>(scrollingMode)),
        marginWidth(marginWidth),
        marginHeight(marginHeight),
        allowFullscreen(allowFullscreen),
        allowPaymentRequest(allowPaymentRequest),
        requiredCsp(requiredCsp),
        delegatedPermissions(delegatedPermissions) {}
#endif
};

}  // namespace blink

#endif
