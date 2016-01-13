// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_COMPOSITOR_BINDINGS_WEB_IMAGE_LAYER_IMPL_H_
#define CONTENT_RENDERER_COMPOSITOR_BINDINGS_WEB_IMAGE_LAYER_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebImageLayer.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace content {

class WebLayerImpl;

class WebImageLayerImpl : public blink::WebImageLayer {
 public:
  CONTENT_EXPORT WebImageLayerImpl();
  virtual ~WebImageLayerImpl();

  // blink::WebImageLayer implementation.
  virtual blink::WebLayer* layer();
  virtual void setBitmap(SkBitmap);

 private:
  scoped_ptr<WebLayerImpl> layer_;

  DISALLOW_COPY_AND_ASSIGN(WebImageLayerImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_COMPOSITOR_BINDINGS_WEB_IMAGE_LAYER_IMPL_H_

