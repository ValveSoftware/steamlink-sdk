// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/empty_content_layer_client.h"

#include "cc/playback/display_item_list.h"
#include "cc/playback/display_item_list_settings.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {

namespace {
base::LazyInstance<EmptyContentLayerClient> g_empty_content_layer_client =
    LAZY_INSTANCE_INITIALIZER;
}

// static
ContentLayerClient* EmptyContentLayerClient::GetInstance() {
  return g_empty_content_layer_client.Pointer();
}

EmptyContentLayerClient::EmptyContentLayerClient() {}

EmptyContentLayerClient::~EmptyContentLayerClient() {}

gfx::Rect EmptyContentLayerClient::PaintableRegion() {
  return gfx::Rect();
}

scoped_refptr<DisplayItemList>
EmptyContentLayerClient::PaintContentsToDisplayList(
    PaintingControlSetting painting_status) {
  return DisplayItemList::Create(gfx::Rect(), DisplayItemListSettings());
}

bool EmptyContentLayerClient::FillsBoundsCompletely() const {
  return false;
}

size_t EmptyContentLayerClient::GetApproximateUnsharedMemoryUsage() const {
  return 0u;
}

}  // namespace cc
