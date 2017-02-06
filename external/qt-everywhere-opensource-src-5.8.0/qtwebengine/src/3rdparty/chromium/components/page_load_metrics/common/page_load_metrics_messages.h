// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for page load metrics.
// Multiply-included message file, hence no include guard.

#include "base/time/time.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "ipc/ipc_message_macros.h"

#define IPC_MESSAGE_START PageLoadMetricsMsgStart

// See comments in page_load_timing.h for details on each field.
IPC_STRUCT_TRAITS_BEGIN(page_load_metrics::PageLoadTiming)
  IPC_STRUCT_TRAITS_MEMBER(navigation_start)
  IPC_STRUCT_TRAITS_MEMBER(response_start)
  IPC_STRUCT_TRAITS_MEMBER(dom_loading)
  IPC_STRUCT_TRAITS_MEMBER(dom_content_loaded_event_start)
  IPC_STRUCT_TRAITS_MEMBER(load_event_start)
  IPC_STRUCT_TRAITS_MEMBER(first_layout)
  IPC_STRUCT_TRAITS_MEMBER(first_paint)
  IPC_STRUCT_TRAITS_MEMBER(first_text_paint)
  IPC_STRUCT_TRAITS_MEMBER(first_image_paint)
  IPC_STRUCT_TRAITS_MEMBER(first_contentful_paint)
  IPC_STRUCT_TRAITS_MEMBER(parse_start)
  IPC_STRUCT_TRAITS_MEMBER(parse_stop)
  IPC_STRUCT_TRAITS_MEMBER(parse_blocked_on_script_load_duration)
  IPC_STRUCT_TRAITS_MEMBER(
      parse_blocked_on_script_load_from_document_write_duration)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(page_load_metrics::PageLoadMetadata)
  IPC_STRUCT_TRAITS_MEMBER(behavior_flags)
IPC_STRUCT_TRAITS_END()

// Sent from renderer to browser process when the PageLoadTiming for the
// associated frame changed.
IPC_MESSAGE_ROUTED2(PageLoadMetricsMsg_TimingUpdated,
                    page_load_metrics::PageLoadTiming,
                    page_load_metrics::PageLoadMetadata)
