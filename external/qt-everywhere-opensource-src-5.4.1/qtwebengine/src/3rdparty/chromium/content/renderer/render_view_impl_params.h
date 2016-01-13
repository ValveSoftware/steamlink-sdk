// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_VIEW_IMPL_PARAMS_H_
#define CONTENT_RENDERER_RENDER_VIEW_IMPL_PARAMS_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "content/common/content_export.h"
#include "content/common/view_message_enums.h"

struct WebPreferences;

namespace blink {
struct WebScreenInfo;
}

namespace content {

struct RendererPreferences;

// Container for all parameters passed to RenderViewImpl's constructor.
struct CONTENT_EXPORT RenderViewImplParams {
  RenderViewImplParams(int32 opener_id,
                       bool window_was_created_with_opener,
                       const RendererPreferences& renderer_prefs,
                       const WebPreferences& webkit_prefs,
                       int32 routing_id,
                       int32 main_frame_routing_id,
                       int32 surface_id,
                       int64 session_storage_namespace_id,
                       const base::string16& frame_name,
                       bool is_renderer_created,
                       bool swapped_out,
                       int32 proxy_routing_id,
                       bool hidden,
                       bool never_visible,
                       int32 next_page_id,
                       const blink::WebScreenInfo& screen_info,
                       AccessibilityMode accessibility_mode);
  ~RenderViewImplParams();

  int32 opener_id;
  bool window_was_created_with_opener;
  const RendererPreferences& renderer_prefs;
  const WebPreferences& webkit_prefs;
  int32 routing_id;
  int32 main_frame_routing_id;
  int32 surface_id;
  int64 session_storage_namespace_id;
  const base::string16& frame_name;
  bool is_renderer_created;
  bool swapped_out;
  int32 proxy_routing_id;
  bool hidden;
  bool never_visible;
  int32 next_page_id;
  const blink::WebScreenInfo& screen_info;
  AccessibilityMode accessibility_mode;
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_VIEW_IMPL_PARAMS_H_
