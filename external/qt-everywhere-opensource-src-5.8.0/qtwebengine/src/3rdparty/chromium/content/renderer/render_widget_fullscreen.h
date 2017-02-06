// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_WIDGET_FULLSCREEN_H_
#define CONTENT_RENDERER_RENDER_WIDGET_FULLSCREEN_H_

#include "content/renderer/render_widget.h"

#include <stdint.h>

#include "third_party/WebKit/public/web/WebWidget.h"

namespace content {
class CompositorDependencies;

// TODO(boliu): Override non-supported methods with no-op? eg setWindowRect().
class RenderWidgetFullscreen : public RenderWidget {
 public:
  void show(blink::WebNavigationPolicy) override;

 protected:
  RenderWidgetFullscreen(CompositorDependencies* compositor_deps,
                         const blink::WebScreenInfo& screen_info);
  ~RenderWidgetFullscreen() override;

  virtual blink::WebWidget* CreateWebWidget();

  bool Init(int32_t opener_id);
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_WIDGET_FULLSCREEN_H_
