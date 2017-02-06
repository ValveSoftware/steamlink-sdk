// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_WINDOW_ANDROID_COMPOSITOR_H_
#define UI_ANDROID_WINDOW_ANDROID_COMPOSITOR_H_

#include <memory>

#include "cc/output/copy_output_request.h"
#include "ui/android/ui_android_export.h"

namespace cc {
class Layer;
}

namespace ui {

class ResourceManager;

// Android interface for compositor-related tasks.
class UI_ANDROID_EXPORT WindowAndroidCompositor {
 public:
  virtual ~WindowAndroidCompositor() {}

  virtual void AttachLayerForReadback(scoped_refptr<cc::Layer> layer) = 0;
  virtual void RequestCopyOfOutputOnRootLayer(
      std::unique_ptr<cc::CopyOutputRequest> request) = 0;
  virtual void OnVSync(base::TimeTicks frame_time,
                       base::TimeDelta vsync_period) = 0;
  virtual void SetNeedsAnimate() = 0;
  virtual ResourceManager& GetResourceManager() = 0;
};

}  // namespace ui

#endif  // UI_ANDROID_WINDOW_ANDROID_COMPOSITOR_H_
