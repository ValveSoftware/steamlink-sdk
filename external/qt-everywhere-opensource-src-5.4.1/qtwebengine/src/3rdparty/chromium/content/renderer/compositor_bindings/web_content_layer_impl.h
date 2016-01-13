// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_COMPOSITOR_BINDINGS_WEB_CONTENT_LAYER_IMPL_H_
#define CONTENT_RENDERER_COMPOSITOR_BINDINGS_WEB_CONTENT_LAYER_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "cc/layers/content_layer_client.h"
#include "content/common/content_export.h"
#include "content/renderer/compositor_bindings/web_layer_impl.h"
#include "third_party/WebKit/public/platform/WebContentLayer.h"

namespace cc {
class IntRect;
class FloatRect;
}

namespace blink {
class WebContentLayerClient;
}

namespace content {

class WebContentLayerImpl : public blink::WebContentLayer,
                            public cc::ContentLayerClient {
 public:
  CONTENT_EXPORT explicit WebContentLayerImpl(
      blink::WebContentLayerClient*);

  // WebContentLayer implementation.
  virtual blink::WebLayer* layer();
  virtual void setDoubleSided(bool double_sided);
  virtual void setDrawCheckerboardForMissingTiles(bool checkerboard);

 protected:
  virtual ~WebContentLayerImpl();

  // ContentLayerClient implementation.
  virtual void PaintContents(SkCanvas* canvas,
                             const gfx::Rect& clip,
                             gfx::RectF* opaque,
                             ContentLayerClient::GraphicsContextStatus
                                 graphics_context_status) OVERRIDE;
  virtual void DidChangeLayerCanUseLCDText() OVERRIDE;
  virtual bool FillsBoundsCompletely() const OVERRIDE;

  scoped_ptr<WebLayerImpl> layer_;
  blink::WebContentLayerClient* client_;
  bool draws_content_;

 private:
  bool can_use_lcd_text_;
  bool ignore_lcd_text_change_;

  DISALLOW_COPY_AND_ASSIGN(WebContentLayerImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_COMPOSITOR_BINDINGS_WEB_CONTENT_LAYER_IMPL_H_

