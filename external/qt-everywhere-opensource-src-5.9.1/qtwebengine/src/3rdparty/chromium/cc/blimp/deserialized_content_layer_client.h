// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BLIMP_DESERIALIZED_CONTENT_LAYER_CLIENT_H_
#define CC_BLIMP_DESERIALIZED_CONTENT_LAYER_CLIENT_H_

#include "base/macros.h"
#include "cc/layers/content_layer_client.h"

namespace cc {

// The ContentLayerClient used by the PictureLayers created for the remote
// client. This holds the deserialized DisplayItemList received from the
// corresponding PictureLayer's client on the engine.
class DeserializedContentLayerClient : public ContentLayerClient {
 public:
  DeserializedContentLayerClient();
  ~DeserializedContentLayerClient() override;

  // Updates from the display list update received from the engine.
  void UpdateDisplayListAndRecordedViewport(
      scoped_refptr<DisplayItemList> display_list,
      gfx::Rect recorded_viewport);

  // ContentLayerClient implementation.
  gfx::Rect PaintableRegion() override;
  scoped_refptr<DisplayItemList> PaintContentsToDisplayList(
      PaintingControlSetting painting_status) override;
  bool FillsBoundsCompletely() const override;
  size_t GetApproximateUnsharedMemoryUsage() const override;

 private:
  scoped_refptr<DisplayItemList> display_list_;
  gfx::Rect recorded_viewport_;

  DISALLOW_COPY_AND_ASSIGN(DeserializedContentLayerClient);
};

}  // namespace cc

#endif  // CC_BLIMP_DESERIALIZED_CONTENT_LAYER_CLIENT_H_
