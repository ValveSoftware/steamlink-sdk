// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RENDERER_UNRESPONSIVE_TYPE_H_
#define CONTENT_PUBLIC_BROWSER_RENDERER_UNRESPONSIVE_TYPE_H_

#include "content/common/content_export.h"

namespace content {

// Used in histograms to differentiate between the different types of
// renderer hang. Only add values at the end, do not delete values.
enum RendererUnresponsiveType {
  RENDERER_UNRESPONSIVE_UNKNOWN = 0,
  RENDERER_UNRESPONSIVE_IN_FLIGHT_EVENTS = 1,
  RENDERER_UNRESPONSIVE_DIALOG_CLOSED = 2,
  RENDERER_UNRESPONSIVE_DIALOG_SUPPRESSED = 3,
  RENDERER_UNRESPONSIVE_BEFORE_UNLOAD = 4,
  RENDERER_UNRESPONSIVE_UNLOAD = 5,
  RENDERER_UNRESPONSIVE_CLOSE_PAGE = 6,
  RENDERER_UNRESPONSIVE_MAX = RENDERER_UNRESPONSIVE_CLOSE_PAGE,
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RENDERER_UNRESPONSIVE_TYPE_H_
