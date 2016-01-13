// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_COMPOSITOR_BINDINGS_WEB_SCROLLBAR_LAYER_IMPL_H_
#define CONTENT_RENDERER_COMPOSITOR_BINDINGS_WEB_SCROLLBAR_LAYER_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebScrollbar.h"
#include "third_party/WebKit/public/platform/WebScrollbarLayer.h"

namespace blink {
class WebScrollbarThemeGeometry;
class WebScrollbarThemePainter;
}

namespace content {

class WebLayerImpl;

class WebScrollbarLayerImpl : public blink::WebScrollbarLayer {
 public:
  CONTENT_EXPORT WebScrollbarLayerImpl(
      blink::WebScrollbar* scrollbar,
      blink::WebScrollbarThemePainter painter,
      blink::WebScrollbarThemeGeometry* geometry);
  CONTENT_EXPORT WebScrollbarLayerImpl(
      blink::WebScrollbar::Orientation orientation,
      int thumb_thickness,
      int track_start,
      bool is_left_side_vertical_scrollbar);
  virtual ~WebScrollbarLayerImpl();

  // blink::WebScrollbarLayer implementation.
  virtual blink::WebLayer* layer();
  virtual void setScrollLayer(blink::WebLayer* layer);
  virtual void setClipLayer(blink::WebLayer* layer);

 private:
  scoped_ptr<WebLayerImpl> layer_;

  DISALLOW_COPY_AND_ASSIGN(WebScrollbarLayerImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_COMPOSITOR_BINDINGS_WEB_SCROLLBAR_LAYER_IMPL_H_
