// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BLINK_WEB_EXTERNAL_TEXTURE_LAYER_IMPL_H_
#define CC_BLINK_WEB_EXTERNAL_TEXTURE_LAYER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "cc/blink/cc_blink_export.h"
#include "third_party/WebKit/public/platform/WebExternalTextureLayer.h"

namespace cc {
class TextureLayerClient;
}

namespace cc_blink {
class WebLayerImpl;

class WebExternalTextureLayerImpl : public blink::WebExternalTextureLayer {
 public:
  CC_BLINK_EXPORT explicit WebExternalTextureLayerImpl(cc::TextureLayerClient*);
  ~WebExternalTextureLayerImpl() override;

  // blink::WebExternalTextureLayer implementation.
  blink::WebLayer* layer() override;
  void clearTexture() override;
  void setOpaque(bool opaque) override;
  void setPremultipliedAlpha(bool premultiplied) override;
  void setBlendBackgroundColor(bool blend) override;
  void setNearestNeighbor(bool nearest_neighbor) override;

 private:
  std::unique_ptr<WebLayerImpl> layer_;

  DISALLOW_COPY_AND_ASSIGN(WebExternalTextureLayerImpl);
};

}  // namespace cc_blink

#endif  // CC_BLINK_WEB_EXTERNAL_TEXTURE_LAYER_IMPL_H_
