// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/blimp/deserialized_content_layer_client.h"

namespace cc {

DeserializedContentLayerClient::DeserializedContentLayerClient() = default;

DeserializedContentLayerClient::~DeserializedContentLayerClient() = default;

void DeserializedContentLayerClient::UpdateDisplayListAndRecordedViewport(
    scoped_refptr<DisplayItemList> display_list,
    gfx::Rect recorded_viewport) {
  display_list_ = std::move(display_list);
  recorded_viewport_ = recorded_viewport;
}

gfx::Rect DeserializedContentLayerClient::PaintableRegion() {
  return recorded_viewport_;
}

scoped_refptr<DisplayItemList>
DeserializedContentLayerClient::PaintContentsToDisplayList(
    PaintingControlSetting painting_status) {
  return display_list_;
}

bool DeserializedContentLayerClient::FillsBoundsCompletely() const {
  return false;
}

size_t DeserializedContentLayerClient::GetApproximateUnsharedMemoryUsage()
    const {
  return 0;
}

}  // namespace cc
