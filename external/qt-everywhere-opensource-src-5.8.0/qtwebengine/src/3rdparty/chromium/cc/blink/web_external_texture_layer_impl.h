// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BLINK_WEB_EXTERNAL_TEXTURE_LAYER_IMPL_H_
#define CC_BLINK_WEB_EXTERNAL_TEXTURE_LAYER_IMPL_H_

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "cc/blink/cc_blink_export.h"
#include "cc/layers/texture_layer_client.h"
#include "third_party/WebKit/public/platform/WebExternalTextureLayer.h"

namespace blink {
class WebExternalTextureLayerClient;
struct WebFloatRect;
struct WebExternalTextureMailbox;
}

namespace cc {
class SingleReleaseCallback;
class TextureMailbox;
}

namespace cc_blink {

class WebLayerImpl;
class WebExternalBitmapImpl;

class WebExternalTextureLayerImpl
    : public blink::WebExternalTextureLayer,
      public cc::TextureLayerClient,
      public base::SupportsWeakPtr<WebExternalTextureLayerImpl> {
 public:
  CC_BLINK_EXPORT explicit WebExternalTextureLayerImpl(
      blink::WebExternalTextureLayerClient*);
  ~WebExternalTextureLayerImpl() override;

  // blink::WebExternalTextureLayer implementation.
  blink::WebLayer* layer() override;
  void clearTexture() override;
  void setOpaque(bool opaque) override;
  void setPremultipliedAlpha(bool premultiplied) override;
  void setBlendBackgroundColor(bool blend) override;
  void setNearestNeighbor(bool nearest_neighbor) override;

  // TextureLayerClient implementation.
  bool PrepareTextureMailbox(
      cc::TextureMailbox* mailbox,
      std::unique_ptr<cc::SingleReleaseCallback>* release_callback,
      bool use_shared_memory) override;

 private:
  static void DidReleaseMailbox(
      base::WeakPtr<WebExternalTextureLayerImpl> layer,
      const blink::WebExternalTextureMailbox& mailbox,
      WebExternalBitmapImpl* bitmap,
      const gpu::SyncToken& sync_token,
      bool lost_resource);

  WebExternalBitmapImpl* AllocateBitmap();

  blink::WebExternalTextureLayerClient* client_;
  std::unique_ptr<WebLayerImpl> layer_;
  std::vector<std::unique_ptr<WebExternalBitmapImpl>> free_bitmaps_;

  DISALLOW_COPY_AND_ASSIGN(WebExternalTextureLayerImpl);
};

}  // namespace cc_blink

#endif  // CC_BLINK_WEB_EXTERNAL_TEXTURE_LAYER_IMPL_H_
