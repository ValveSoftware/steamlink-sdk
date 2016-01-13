// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_view_impl_params.h"

namespace content {

RenderViewImplParams::RenderViewImplParams(
    int32 opener_id,
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
    AccessibilityMode accessibility_mode)
    : opener_id(opener_id),
      window_was_created_with_opener(window_was_created_with_opener),
      renderer_prefs(renderer_prefs),
      webkit_prefs(webkit_prefs),
      routing_id(routing_id),
      main_frame_routing_id(main_frame_routing_id),
      surface_id(surface_id),
      session_storage_namespace_id(session_storage_namespace_id),
      frame_name(frame_name),
      is_renderer_created(is_renderer_created),
      swapped_out(swapped_out),
      proxy_routing_id(proxy_routing_id),
      hidden(hidden),
      never_visible(never_visible),
      next_page_id(next_page_id),
      screen_info(screen_info),
      accessibility_mode(accessibility_mode) {}

RenderViewImplParams::~RenderViewImplParams() {}

}  // namespace content
