// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RENDER_WIDGET_HOST_VIEW_MAC_BASE_H_
#define CONTENT_PUBLIC_BROWSER_RENDER_WIDGET_HOST_VIEW_MAC_BASE_H_

// The Mac RenderWidgetHostView implementation conforms to this protocol.
//
// While Chrome does not need any details of the actual
// implementation, there may be cases where it needs to alter behavior
// depending on whether an event receiver is the Mac implementation of
// RenderWidgetHostView.  For this purpose,
// conformsToProtocol:@protocol(RenderWidgetHostViewMacBase) can be used.
@protocol RenderWidgetHostViewMacBase
@end

#endif  // CONTENT_PUBLIC_BROWSER_RENDER_WIDGET_HOST_VIEW_MAC_BASE_H_
