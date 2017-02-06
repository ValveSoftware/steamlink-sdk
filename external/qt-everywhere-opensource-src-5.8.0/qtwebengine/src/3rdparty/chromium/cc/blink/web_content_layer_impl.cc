// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/blink/web_content_layer_impl.h"

#include <stddef.h>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "cc/base/switches.h"
#include "cc/blink/web_display_item_list_impl.h"
#include "cc/layers/picture_layer.h"
#include "cc/playback/display_item_list_settings.h"
#include "third_party/WebKit/public/platform/WebContentLayerClient.h"
#include "third_party/WebKit/public/platform/WebFloatPoint.h"
#include "third_party/WebKit/public/platform/WebFloatRect.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/skia/include/core/SkMatrix44.h"

using cc::PictureLayer;

namespace cc_blink {

static bool UseCachedPictureRaster() {
  static bool use = !base::CommandLine::ForCurrentProcess()->HasSwitch(
      cc::switches::kDisableCachedPictureRaster);
  return use;
}

static blink::WebContentLayerClient::PaintingControlSetting
PaintingControlToWeb(
    cc::ContentLayerClient::PaintingControlSetting painting_control) {
  switch (painting_control) {
    case cc::ContentLayerClient::PAINTING_BEHAVIOR_NORMAL:
      return blink::WebContentLayerClient::PaintDefaultBehavior;
    case cc::ContentLayerClient::PAINTING_BEHAVIOR_NORMAL_FOR_TEST:
      return blink::WebContentLayerClient::PaintDefaultBehaviorForTest;
    case cc::ContentLayerClient::DISPLAY_LIST_CONSTRUCTION_DISABLED:
      return blink::WebContentLayerClient::DisplayListConstructionDisabled;
    case cc::ContentLayerClient::DISPLAY_LIST_CACHING_DISABLED:
      return blink::WebContentLayerClient::DisplayListCachingDisabled;
    case cc::ContentLayerClient::DISPLAY_LIST_PAINTING_DISABLED:
      return blink::WebContentLayerClient::DisplayListPaintingDisabled;
    case cc::ContentLayerClient::SUBSEQUENCE_CACHING_DISABLED:
      return blink::WebContentLayerClient::SubsequenceCachingDisabled;
  }
  NOTREACHED();
  return blink::WebContentLayerClient::PaintDefaultBehavior;
}

WebContentLayerImpl::WebContentLayerImpl(blink::WebContentLayerClient* client)
    : client_(client) {
  layer_ = base::WrapUnique(new WebLayerImpl(PictureLayer::Create(this)));
  layer_->layer()->SetIsDrawable(true);
}

WebContentLayerImpl::~WebContentLayerImpl() {
  static_cast<PictureLayer*>(layer_->layer())->ClearClient();
}

blink::WebLayer* WebContentLayerImpl::layer() {
  return layer_.get();
}

gfx::Rect WebContentLayerImpl::PaintableRegion() {
  return client_->paintableRegion();
}

scoped_refptr<cc::DisplayItemList>
WebContentLayerImpl::PaintContentsToDisplayList(
    cc::ContentLayerClient::PaintingControlSetting painting_control) {
  cc::DisplayItemListSettings settings;
  settings.use_cached_picture = UseCachedPictureRaster();

  scoped_refptr<cc::DisplayItemList> display_list =
      cc::DisplayItemList::Create(PaintableRegion(), settings);
  if (client_) {
    WebDisplayItemListImpl list(display_list.get());
    client_->paintContents(&list, PaintingControlToWeb(painting_control));
  }
  display_list->Finalize();
  return display_list;
}

bool WebContentLayerImpl::FillsBoundsCompletely() const {
  return false;
}

size_t WebContentLayerImpl::GetApproximateUnsharedMemoryUsage() const {
  return client_->approximateUnsharedMemoryUsage();
}

}  // namespace cc_blink
