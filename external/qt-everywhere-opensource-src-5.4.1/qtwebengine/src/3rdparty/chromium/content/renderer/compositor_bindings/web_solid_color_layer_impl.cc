// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/compositor_bindings/web_solid_color_layer_impl.h"

#include "cc/layers/solid_color_layer.h"
#include "content/renderer/compositor_bindings/web_layer_impl.h"

using cc::SolidColorLayer;

namespace content {

WebSolidColorLayerImpl::WebSolidColorLayerImpl()
    : layer_(new WebLayerImpl(SolidColorLayer::Create())) {
  layer_->layer()->SetIsDrawable(true);
}

WebSolidColorLayerImpl::~WebSolidColorLayerImpl() {
}

blink::WebLayer* WebSolidColorLayerImpl::layer() {
  return layer_.get();
}

void WebSolidColorLayerImpl::setBackgroundColor(blink::WebColor color) {
  layer_->setBackgroundColor(color);
}

}  // namespace content

