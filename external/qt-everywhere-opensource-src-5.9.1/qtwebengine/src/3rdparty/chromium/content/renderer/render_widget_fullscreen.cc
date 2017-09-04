// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_widget_fullscreen.h"

#include "content/common/view_messages.h"
#include "content/renderer/render_thread_impl.h"
#include "third_party/WebKit/public/web/WebWidget.h"

using blink::WebWidget;

namespace content {

void RenderWidgetFullscreen::show(blink::WebNavigationPolicy) {
  DCHECK(!did_show_) << "received extraneous Show call";
  DCHECK_NE(MSG_ROUTING_NONE, routing_id());
  DCHECK_NE(MSG_ROUTING_NONE, opener_id_);

  if (!did_show_) {
    did_show_ = true;
    Send(new ViewHostMsg_ShowFullscreenWidget(opener_id_, routing_id()));
    SetPendingWindowRect(initial_rect_);
  }
}

RenderWidgetFullscreen::RenderWidgetFullscreen(
    int32_t widget_routing_id,
    CompositorDependencies* compositor_deps,
    const ScreenInfo& screen_info)
    : RenderWidget(widget_routing_id,
                   compositor_deps,
                   blink::WebPopupTypeNone,
                   screen_info,
                   false,
                   false,
                   false) {}

RenderWidgetFullscreen::~RenderWidgetFullscreen() {}

WebWidget* RenderWidgetFullscreen::CreateWebWidget() {
  // TODO(boliu): Handle full screen render widgets here.
  return RenderWidget::CreateWebWidget(this);
}

bool RenderWidgetFullscreen::Init(int32_t opener_id) {
  DCHECK(!GetWebWidget());

  RenderWidget::Init(opener_id, CreateWebWidget());
  return true;
}

}  // namespace content
