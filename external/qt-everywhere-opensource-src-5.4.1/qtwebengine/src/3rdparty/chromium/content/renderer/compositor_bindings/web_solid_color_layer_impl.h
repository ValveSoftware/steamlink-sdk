// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_COMPOSITOR_BINDINGS_WEB_SOLID_COLOR_LAYER_IMPL_H_
#define CONTENT_RENDERER_COMPOSITOR_BINDINGS_WEB_SOLID_COLOR_LAYER_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebColor.h"
#include "third_party/WebKit/public/platform/WebSolidColorLayer.h"

namespace content {
class WebLayerImpl;

class WebSolidColorLayerImpl : public blink::WebSolidColorLayer {
 public:
  CONTENT_EXPORT WebSolidColorLayerImpl();
  virtual ~WebSolidColorLayerImpl();

  // blink::WebSolidColorLayer implementation.
  virtual blink::WebLayer* layer();
  virtual void setBackgroundColor(blink::WebColor);

 private:
  scoped_ptr<WebLayerImpl> layer_;

  DISALLOW_COPY_AND_ASSIGN(WebSolidColorLayerImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_COMPOSITOR_BINDINGS_WEB_SOLID_COLOR_LAYER_IMPL_H_

