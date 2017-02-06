// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/test/test_compositor_host.h"

#include <memory>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {

class TestCompositorHostOzone : public TestCompositorHost {
 public:
  TestCompositorHostOzone(const gfx::Rect& bounds,
                          ui::ContextFactory* context_factory);
  ~TestCompositorHostOzone() override;

 private:
  // Overridden from TestCompositorHost:
  void Show() override;
  ui::Compositor* GetCompositor() override;

  gfx::Rect bounds_;

  ui::Compositor compositor_;

  DISALLOW_COPY_AND_ASSIGN(TestCompositorHostOzone);
};

TestCompositorHostOzone::TestCompositorHostOzone(
    const gfx::Rect& bounds,
    ui::ContextFactory* context_factory)
    : bounds_(bounds),
      compositor_(context_factory, base::ThreadTaskRunnerHandle::Get()) {}

TestCompositorHostOzone::~TestCompositorHostOzone() {}

void TestCompositorHostOzone::Show() {
  // Ozone should rightly have a backing native framebuffer
  // An in-memory array draw into by OSMesa is a reasonble
  // fascimile of a dumb framebuffer at present.
  // GLSurface will allocate the array so long as it is provided
  // with a non-0 widget.
  // TODO(rjkroege): Use a "real" ozone widget when it is
  // available: http://crbug.com/255128
  compositor_.SetAcceleratedWidget(1);
  compositor_.SetScaleAndSize(1.0f, bounds_.size());
}

ui::Compositor* TestCompositorHostOzone::GetCompositor() {
  return &compositor_;
}

// static
TestCompositorHost* TestCompositorHost::Create(
    const gfx::Rect& bounds,
    ui::ContextFactory* context_factory) {
  return new TestCompositorHostOzone(bounds, context_factory);
}

}  // namespace ui
