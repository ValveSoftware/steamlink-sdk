// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_APP_ANDROID_BLIMP_COMPOSITOR_MANAGER_ANDROID_H_
#define BLIMP_CLIENT_APP_ANDROID_BLIMP_COMPOSITOR_MANAGER_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/macros.h"
#include "blimp/client/feature/compositor/blimp_compositor_manager.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"

namespace base {
class SingleThreadTaskRunner;
class Thread;
}

namespace cc {
class LayerTreeHost;
}

namespace blimp {
namespace client {

class RenderWidgetFeature;

// An Android specific version of the BlimpCompositor.  This class builds a
// gfx::AcceleratedWidget out of an Android SurfaceView's surface.
class BlimpCompositorManagerAndroid : public BlimpCompositorManager {
 public:
  // |real_size| is the total display area including system decorations (see
  // android.view.Display.getRealSize()).  |size| is the total display
  // area not including system decorations (see android.view.Display.getSize()).
  static std::unique_ptr<BlimpCompositorManagerAndroid> Create(
      const gfx::Size& real_size,
      const gfx::Size& size,
      RenderWidgetFeature* render_widget_feature,
      BlimpCompositorManagerClient* client);

  ~BlimpCompositorManagerAndroid() override;

 protected:
  // |size| is the size of the display.  |real_size_supported| determines
  // whether or not this size is the real display size or the display size
  // not including the system decorations.  |dp_to_px| is the scale factor that
  // is required to convert from dp (device pixels) to px.
  BlimpCompositorManagerAndroid(const gfx::Size& size,
                                bool real_size_supported,
                                RenderWidgetFeature* render_widget_feature,
                                BlimpCompositorManagerClient* client);

  // BlimpCompositor implementation.
  void GenerateLayerTreeSettings(cc::LayerTreeSettings* settings) override;

 private:
  // Used to determine tile size for the compositor's rastered tiles. For a
  // device of width X height |portrait_width_| will be min(width, height) and
  // |landscape_width_| will be max(width, height).
  int portrait_width_;
  int landscape_width_;

  // True if the |portrait_width_| and |landscape_width_| represent the device's
  // physical dimensions, including any area occupied by system decorations.
  bool real_size_supported_;

  DISALLOW_COPY_AND_ASSIGN(BlimpCompositorManagerAndroid);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_APP_ANDROID_BLIMP_COMPOSITOR_MANAGER_ANDROID_H_
