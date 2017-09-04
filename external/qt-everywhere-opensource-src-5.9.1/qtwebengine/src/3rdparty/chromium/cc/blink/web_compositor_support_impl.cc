// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/blink/web_compositor_support_impl.h"

#include <utility>

#include "base/memory/ptr_util.h"
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

std::unique_ptr<WebLayer> WebCompositorSupportImpl::createLayer() {
  return base::MakeUnique<WebLayerImpl>();
}

std::unique_ptr<WebLayer> WebCompositorSupportImpl::createLayerFromCCLayer(
    cc::Layer* layer) {
  return base::MakeUnique<WebLayerImpl>(layer);
}

std::unique_ptr<WebContentLayer> WebCompositorSupportImpl::createContentLayer(
    WebContentLayerClient* client) {
  return base::MakeUnique<WebContentLayerImpl>(client);
}

std::unique_ptr<WebExternalTextureLayer>
WebCompositorSupportImpl::createExternalTextureLayer(
    cc::TextureLayerClient* client) {
  return base::MakeUnique<WebExternalTextureLayerImpl>(client);
}

std::unique_ptr<blink::WebImageLayer>
WebCompositorSupportImpl::createImageLayer() {
  return base::MakeUnique<WebImageLayerImpl>();
}

std::unique_ptr<WebScrollbarLayer>
WebCompositorSupportImpl::createScrollbarLayer(
    std::unique_ptr<WebScrollbar> scrollbar,
    WebScrollbarThemePainter painter,
    std::unique_ptr<WebScrollbarThemeGeometry> geometry) {
  return base::MakeUnique<WebScrollbarLayerImpl>(std::move(scrollbar), painter,
                                                 std::move(geometry));
}

std::unique_ptr<WebScrollbarLayer>
WebCompositorSupportImpl::createSolidColorScrollbarLayer(
    WebScrollbar::Orientation orientation,
    int thumb_thickness,
    int track_start,
    bool is_left_side_vertical_scrollbar) {
  return base::MakeUnique<WebScrollbarLayerImpl>(
      orientation, thumb_thickness, track_start,
      is_left_side_vertical_scrollbar);
}

}  // namespace cc_blink
