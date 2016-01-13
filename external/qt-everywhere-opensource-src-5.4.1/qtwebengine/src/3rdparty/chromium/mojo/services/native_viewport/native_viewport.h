// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_NATIVE_VIEWPORT_NATIVE_VIEWPORT_H_
#define MOJO_SERVICES_NATIVE_VIEWPORT_NATIVE_VIEWPORT_H_

#include "base/memory/scoped_ptr.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"

namespace gfx {
class Rect;
}

namespace ui {
class Event;
}

namespace mojo {
namespace shell {
class Context;
}

namespace services {

class NativeViewportDelegate {
 public:
  virtual ~NativeViewportDelegate() {}

  virtual void OnBoundsChanged(const gfx::Rect& size) = 0;
  virtual void OnAcceleratedWidgetAvailable(gfx::AcceleratedWidget widget) = 0;
  virtual bool OnEvent(ui::Event* ui_event) = 0;
  virtual void OnDestroyed() = 0;
};

// Encapsulation of platform-specific Viewport.
// TODO(abarth): Rename this class so that it doesn't conflict with the name of
// the service.
class NativeViewport {
 public:
  virtual ~NativeViewport() {}

  virtual void Init(const gfx::Rect& bounds) = 0;
  virtual void Show() = 0;
  virtual void Hide() = 0;
  virtual void Close() = 0;
  virtual gfx::Size GetSize() = 0;
  virtual void SetBounds(const gfx::Rect& bounds) = 0;

  virtual void SetCapture() = 0;
  virtual void ReleaseCapture() = 0;

  // |context| is NULL when loaded into separate process.
  static scoped_ptr<NativeViewport> Create(shell::Context* context,
                                           NativeViewportDelegate* delegate);
};

}  // namespace services
}  // namespace mojo

#endif  // MOJO_SERVICES_NATIVE_VIEWPORT_NATIVE_VIEWPORT_H_
