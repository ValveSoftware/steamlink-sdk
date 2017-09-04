// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BLIMP_LAYER_FACTORY_H_
#define CC_BLIMP_LAYER_FACTORY_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "cc/input/scrollbar.h"

namespace cc {
class ContentLayerClient;
class Layer;
class PictureLayer;
class SolidColorScrollbarLayer;

// Used to allow tests to inject the Layer created by the
// CompositorStateDeserializer on the client.
class LayerFactory {
 public:
  virtual ~LayerFactory() {}

  virtual scoped_refptr<Layer> CreateLayer(int engine_layer_id) = 0;

  virtual scoped_refptr<PictureLayer> CreatePictureLayer(
      int engine_layer_id,
      ContentLayerClient* content_layer_client) = 0;

  virtual scoped_refptr<SolidColorScrollbarLayer>
  CreateSolidColorScrollbarLayer(int engine_layer_id,
                                 ScrollbarOrientation orientation,
                                 int thumb_thickness,
                                 int track_start,
                                 bool is_left_side_vertical_scrollbar,
                                 int scroll_layer_id) = 0;

  // Create layers for testing.
  virtual scoped_refptr<PictureLayer> CreateFakePictureLayer(
      int engine_layer_id,
      ContentLayerClient* content_layer_client) = 0;

  virtual scoped_refptr<Layer> CreatePushPropertiesCountingLayer(
      int engine_layer_id) = 0;
};

}  // namespace cc

#endif  // CC_BLIMP_LAYER_FACTORY_H_
