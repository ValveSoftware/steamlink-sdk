// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/blink/web_external_texture_layer_impl.h"

#include "cc/blink/web_layer_impl.h"
#include "cc/layers/texture_layer.h"

using cc::TextureLayer;

namespace cc_blink {

WebExternalTextureLayerImpl::WebExternalTextureLayerImpl(
    cc::TextureLayerClient* client) {
  scoped_refptr<TextureLayer> layer = TextureLayer::CreateForMailbox(client);
  layer->SetIsDrawable(true);
  layer_.reset(new WebLayerImpl(layer));
}

WebExternalTextureLayerImpl::~WebExternalTextureLayerImpl() {
  static_cast<TextureLayer*>(layer_->layer())->ClearClient();
}

blink::WebLayer* WebExternalTextureLayerImpl::layer() {
  return layer_.get();
}

void WebExternalTextureLayerImpl::clearTexture() {
  TextureLayer* layer = static_cast<TextureLayer*>(layer_->layer());
  layer->ClearTexture();
}

void WebExternalTextureLayerImpl::setOpaque(bool opaque) {
  static_cast<TextureLayer*>(layer_->layer())->SetContentsOpaque(opaque);
}

void WebExternalTextureLayerImpl::setPremultipliedAlpha(
    bool premultiplied_alpha) {
  static_cast<TextureLayer*>(layer_->layer())
      ->SetPremultipliedAlpha(premultiplied_alpha);
}

void WebExternalTextureLayerImpl::setBlendBackgroundColor(bool blend) {
  static_cast<TextureLayer*>(layer_->layer())->SetBlendBackgroundColor(blend);
}

void WebExternalTextureLayerImpl::setNearestNeighbor(bool nearest_neighbor) {
  static_cast<TextureLayer*>(layer_->layer())
      ->SetNearestNeighbor(nearest_neighbor);
}

}  // namespace cc_blink
