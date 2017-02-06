// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/blink/web_compositor_support_impl.h"

#include <memory>

#include "cc/blink/web_content_layer_impl.h"
#include "cc/blink/web_display_item_list_impl.h"
#include "cc/blink/web_external_texture_layer_impl.h"
#include "cc/blink/web_image_layer_impl.h"
#include "cc/blink/web_layer_impl.h"
#include "cc/blink/web_scrollbar_layer_impl.h"
#include "cc/layers/layer.h"
#include "cc/output/output_surface.h"
#include "cc/output/software_output_device.h"

using blink::WebContentLayer;
using blink::WebContentLayerClient;
using blink::WebDisplayItemList;
using blink::WebExternalTextureLayer;
using blink::WebExternalTextureLayerClient;
using blink::WebImageLayer;
using blink::WebLayer;
using blink::WebScrollbar;
using blink::WebScrollbarLayer;
using blink::WebScrollbarThemeGeometry;
using blink::WebScrollbarThemePainter;

namespace cc_blink {

WebCompositorSupportImpl::WebCompositorSupportImpl() {
}

WebCompositorSupportImpl::~WebCompositorSupportImpl() {
}

WebLayer* WebCompositorSupportImpl::createLayer() {
  return new WebLayerImpl();
}

WebLayer* WebCompositorSupportImpl::createLayerFromCCLayer(cc::Layer* layer) {
  return new WebLayerImpl(layer);
}

WebContentLayer* WebCompositorSupportImpl::createContentLayer(
    WebContentLayerClient* client) {
  return new WebContentLayerImpl(client);
}

WebExternalTextureLayer* WebCompositorSupportImpl::createExternalTextureLayer(
    WebExternalTextureLayerClient* client) {
  return new WebExternalTextureLayerImpl(client);
}

blink::WebImageLayer* WebCompositorSupportImpl::createImageLayer() {
  return new WebImageLayerImpl();
}

WebScrollbarLayer* WebCompositorSupportImpl::createScrollbarLayer(
    WebScrollbar* scrollbar,
    WebScrollbarThemePainter painter,
    WebScrollbarThemeGeometry* geometry) {
  return new WebScrollbarLayerImpl(scrollbar, painter, geometry);
}

WebScrollbarLayer* WebCompositorSupportImpl::createSolidColorScrollbarLayer(
    WebScrollbar::Orientation orientation,
    int thumb_thickness,
    int track_start,
    bool is_left_side_vertical_scrollbar) {
  return new WebScrollbarLayerImpl(orientation,
                                   thumb_thickness,
                                   track_start,
                                   is_left_side_vertical_scrollbar);
}

}  // namespace cc_blink
