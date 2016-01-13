// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_COMPOSITOR_BINDINGS_WEB_EXTERNAL_TEXTURE_LAYER_IMPL_H_
#define CONTENT_RENDERER_COMPOSITOR_BINDINGS_WEB_EXTERNAL_TEXTURE_LAYER_IMPL_H_

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "cc/layers/texture_layer_client.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebExternalTextureLayer.h"

namespace cc {
class SingleReleaseCallback;
class TextureMailbox;
}

namespace blink {
struct WebFloatRect;
struct WebExternalTextureMailbox;
}

namespace content {

class WebLayerImpl;
class WebExternalBitmapImpl;

class WebExternalTextureLayerImpl
    : public blink::WebExternalTextureLayer,
      public cc::TextureLayerClient,
      public base::SupportsWeakPtr<WebExternalTextureLayerImpl> {
 public:
  CONTENT_EXPORT explicit WebExternalTextureLayerImpl(
      blink::WebExternalTextureLayerClient*);
  virtual ~WebExternalTextureLayerImpl();

  // blink::WebExternalTextureLayer implementation.
  virtual blink::WebLayer* layer();
  virtual void clearTexture();
  virtual void setOpaque(bool opaque);
  virtual void setPremultipliedAlpha(bool premultiplied);
  virtual void setBlendBackgroundColor(bool blend);
  virtual void setRateLimitContext(bool rate_limit);

  // TextureLayerClient implementation.
  virtual bool PrepareTextureMailbox(
      cc::TextureMailbox* mailbox,
      scoped_ptr<cc::SingleReleaseCallback>* release_callback,
      bool use_shared_memory) OVERRIDE;

 private:
  static void DidReleaseMailbox(
      base::WeakPtr<WebExternalTextureLayerImpl> layer,
      const blink::WebExternalTextureMailbox& mailbox,
      WebExternalBitmapImpl* bitmap,
      unsigned sync_point,
      bool lost_resource);

  WebExternalBitmapImpl* AllocateBitmap();

  blink::WebExternalTextureLayerClient* client_;
  scoped_ptr<WebLayerImpl> layer_;
  ScopedVector<WebExternalBitmapImpl> free_bitmaps_;

  DISALLOW_COPY_AND_ASSIGN(WebExternalTextureLayerImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_COMPOSITOR_BINDINGS_WEB_EXTERNAL_TEXTURE_LAYER_IMPL_H_

