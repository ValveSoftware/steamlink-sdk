// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ANDROID_COMPOSITOR_H_
#define CONTENT_PUBLIC_BROWSER_ANDROID_COMPOSITOR_H_

#include "base/callback.h"
#include "cc/resources/ui_resource_bitmap.h"
#include "content/common/content_export.h"
#include "content/public/browser/android/ui_resource_provider.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

class SkBitmap;

namespace cc {
class Layer;
}

namespace gfx {
class JavaBitmap;
}

namespace content {
class CompositorClient;
class UIResourceProvider;

// An interface to the browser-side compositor.
class CONTENT_EXPORT Compositor {
 public:
  virtual ~Compositor() {}

  // Performs the global initialization needed before any compositor
  // instance can be used. This should be called only once.
  static void Initialize();

  // Creates and returns a compositor instance.  |root_window| needs to outlive
  // the compositor as it manages callbacks on the compositor.
  static Compositor* Create(CompositorClient* client,
                            gfx::NativeWindow root_window);

  // Attaches the layer tree.
  virtual void SetRootLayer(scoped_refptr<cc::Layer> root) = 0;

  // Set the scale factor from DIP to pixel.
  virtual void setDeviceScaleFactor(float factor) = 0;

  // Set the output surface bounds.
  virtual void SetWindowBounds(const gfx::Size& size) = 0;

  // Sets the window visibility. When becoming invisible, resources will get
  // freed and other calls into the compositor are not allowed until after
  // having been made visible again.
  virtual void SetVisible(bool visible) = 0;

  // Set the output surface handle which the compositor renders into.
  // DEPRECATED: Use SetSurface() which takes a Java Surface object.
  virtual void SetWindowSurface(ANativeWindow* window) = 0;

  // Set the output surface which the compositor renders into.
  virtual void SetSurface(jobject surface) = 0;

  // Tells the view tree to assume a transparent background when rendering.
  virtual void SetHasTransparentBackground(bool flag) = 0;

  // Request layout and draw. You only need to call this if you need to trigger
  // Composite *without* having modified the layer tree.
  virtual void SetNeedsComposite() = 0;

  // Returns the UI resource provider associated with the compositor.
  virtual UIResourceProvider& GetUIResourceProvider() = 0;

 protected:
  Compositor() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ANDROID_COMPOSITOR_H_
