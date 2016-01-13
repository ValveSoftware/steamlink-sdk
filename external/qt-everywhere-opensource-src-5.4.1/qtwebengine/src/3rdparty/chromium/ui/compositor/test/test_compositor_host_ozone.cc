// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/test/test_compositor_host.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/rect.h"

namespace ui {

class TestCompositorHostOzone : public TestCompositorHost {
 public:
  TestCompositorHostOzone(const gfx::Rect& bounds,
                          ui::ContextFactory* context_factory);
  virtual ~TestCompositorHostOzone();

 private:
  // Overridden from TestCompositorHost:
  virtual void Show() OVERRIDE;
  virtual ui::Compositor* GetCompositor() OVERRIDE;

  void Draw();

  gfx::Rect bounds_;

  ui::ContextFactory* context_factory_;

  scoped_ptr<ui::Compositor> compositor_;

  DISALLOW_COPY_AND_ASSIGN(TestCompositorHostOzone);
};

TestCompositorHostOzone::TestCompositorHostOzone(
    const gfx::Rect& bounds,
    ui::ContextFactory* context_factory)
    : bounds_(bounds),
      context_factory_(context_factory) {}

TestCompositorHostOzone::~TestCompositorHostOzone() {}

void TestCompositorHostOzone::Show() {
  // Ozone should rightly have a backing native framebuffer
  // An in-memory array draw into by OSMesa is a reasonble
  // fascimile of a dumb framebuffer at present.
  // GLSurface will allocate the array so long as it is provided
  // with a non-0 widget.
  // TODO(rjkroege): Use a "real" ozone widget when it is
  // available: http://crbug.com/255128
  compositor_.reset(new ui::Compositor(1, context_factory_));
  compositor_->SetScaleAndSize(1.0f, bounds_.size());
}

ui::Compositor* TestCompositorHostOzone::GetCompositor() {
  return compositor_.get();
}

void TestCompositorHostOzone::Draw() {
  if (compositor_.get())
    compositor_->Draw();
}

// static
TestCompositorHost* TestCompositorHost::Create(
    const gfx::Rect& bounds,
    ui::ContextFactory* context_factory) {
  return new TestCompositorHostOzone(bounds, context_factory);
}

}  // namespace ui
