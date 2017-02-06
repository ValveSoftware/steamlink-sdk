// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_EMPTY_CONTENT_LAYER_CLIENT_H_
#define CC_LAYERS_EMPTY_CONTENT_LAYER_CLIENT_H_

#include <memory>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "cc/base/cc_export.h"
#include "cc/layers/content_layer_client.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {

class DisplayItemList;

// This is used by deserialized PictureLayers which have no local
// ContentLayerClient but still have valid content from the original serialized
// PictureLayer.  The PictureLayer class requires a valid ContentLayerClient to
// determine if it can draw.  This is a dummy client to keep the logic fairly
// straightforward.  This is also used by unit tests for creating dummy
// PictureLayers.
class CC_EXPORT EmptyContentLayerClient : public ContentLayerClient {
 public:
  static ContentLayerClient* GetInstance();

  // ContentLayerClient implementation.
  gfx::Rect PaintableRegion() override;
  scoped_refptr<DisplayItemList> PaintContentsToDisplayList(
      PaintingControlSetting painting_status) override;
  bool FillsBoundsCompletely() const override;
  size_t GetApproximateUnsharedMemoryUsage() const override;

 private:
  friend struct base::DefaultLazyInstanceTraits<EmptyContentLayerClient>;

  EmptyContentLayerClient();
  ~EmptyContentLayerClient() override;

  DISALLOW_COPY_AND_ASSIGN(EmptyContentLayerClient);
};

}  // namespace cc

#endif  // CC_LAYERS_EMPTY_CONTENT_LAYER_CLIENT_H_
