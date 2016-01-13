// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ANDROID_LAYER_TREE_BUILD_HELPER_H_
#define CONTENT_PUBLIC_BROWSER_ANDROID_LAYER_TREE_BUILD_HELPER_H_

#include "base/memory/ref_counted.h"

namespace cc {
class Layer;
}

namespace content {

// A Helper class to build a layer tree to be composited
// given a content root layer.
class LayerTreeBuildHelper {
 public:
  LayerTreeBuildHelper() {};
  virtual scoped_refptr<cc::Layer> GetLayerTree(
      scoped_refptr<cc::Layer> content_root_layer) = 0;
  virtual ~LayerTreeBuildHelper() {};

 private:
  DISALLOW_COPY_AND_ASSIGN(LayerTreeBuildHelper);
};

}

#endif  // CONTENT_PUBLIC_BROWSER_ANDROID_LAYER_TREE_BUILD_HELPER_H_
