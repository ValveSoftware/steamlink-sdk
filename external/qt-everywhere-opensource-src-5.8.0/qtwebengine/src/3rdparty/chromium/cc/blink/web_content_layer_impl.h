// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BLINK_WEB_CONTENT_LAYER_IMPL_H_
#define CC_BLINK_WEB_CONTENT_LAYER_IMPL_H_

#include <stddef.h>

#include <memory>

#include "base/macros.h"
#include "cc/blink/cc_blink_export.h"
#include "cc/blink/web_layer_impl.h"
#include "cc/layers/content_layer_client.h"
#include "third_party/WebKit/public/platform/WebContentLayer.h"

namespace cc {
class IntRect;
class FloatRect;
}

namespace blink {
class WebContentLayerClient;
}

namespace cc_blink {

class WebContentLayerImpl : public blink::WebContentLayer,
                            public cc::ContentLayerClient {
 public:
  CC_BLINK_EXPORT explicit WebContentLayerImpl(blink::WebContentLayerClient*);

  // WebContentLayer implementation.
  blink::WebLayer* layer() override;

 protected:
  ~WebContentLayerImpl() override;

  // ContentLayerClient implementation.
  gfx::Rect PaintableRegion() override;
  scoped_refptr<cc::DisplayItemList> PaintContentsToDisplayList(
      PaintingControlSetting painting_control) override;
  bool FillsBoundsCompletely() const override;
  size_t GetApproximateUnsharedMemoryUsage() const override;

  std::unique_ptr<WebLayerImpl> layer_;
  blink::WebContentLayerClient* client_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebContentLayerImpl);
};

}  // namespace cc_blink

#endif  // CC_BLINK_WEB_CONTENT_LAYER_IMPL_H_
