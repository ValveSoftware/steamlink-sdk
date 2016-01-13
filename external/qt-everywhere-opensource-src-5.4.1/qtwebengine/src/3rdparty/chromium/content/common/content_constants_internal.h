// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_CONTENT_CONSTANTS_INTERNAL_H_
#define CONTENT_COMMON_CONTENT_CONSTANTS_INTERNAL_H_

#include "base/basictypes.h"
#include "content/common/content_export.h"

namespace content {

// How long to wait before we consider a renderer hung.
CONTENT_EXPORT extern const int kHungRendererDelayMs;

// The maximum plugin width and height.
extern const uint16 kMaxPluginSideLength;
// The maximum plugin size, defined as the number of pixels occupied by the
// plugin.
extern const uint32 kMaxPluginSize;

// Constants used to organize content processes in about:tracing.
CONTENT_EXPORT extern const int kTraceEventBrowserProcessSortIndex;
CONTENT_EXPORT extern const int kTraceEventRendererProcessSortIndex;
CONTENT_EXPORT extern const int kTraceEventPluginProcessSortIndex;
CONTENT_EXPORT extern const int kTraceEventPpapiProcessSortIndex;
CONTENT_EXPORT extern const int kTraceEventPpapiBrokerProcessSortIndex;
CONTENT_EXPORT extern const int kTraceEventGpuProcessSortIndex;

// Constants used to organize content threads in about:tracing.
CONTENT_EXPORT extern const int kTraceEventRendererMainThreadSortIndex;

} // namespace content

#endif  // CONTENT_COMMON_CONTENT_CONSTANTS_INTERNAL_H_
