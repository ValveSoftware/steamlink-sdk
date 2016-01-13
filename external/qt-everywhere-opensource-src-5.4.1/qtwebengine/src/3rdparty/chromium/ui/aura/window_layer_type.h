// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_WINDOW_LAYER_TYPE_H_
#define UI_AURA_WINDOW_LAYER_TYPE_H_

namespace aura {

// These constants mirror that of ui::LayerType with the addition of
// WINDOW_LAYER_NONE. See ui::LayerType for description of ones in common.
enum WindowLayerType {
  // Note that Windows with WINDOW_LAYER_NONE impose limitations on the
  // Window: transforms and animations aren't supported.
  WINDOW_LAYER_NONE,

  WINDOW_LAYER_NOT_DRAWN,
  WINDOW_LAYER_TEXTURED,
  WINDOW_LAYER_SOLID_COLOR,
};

}  // namespace aura

#endif  // UI_AURA_WINDOW_LAYER_TYPE_H_
