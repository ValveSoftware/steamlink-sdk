// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "cc/output/compositor_frame.h"
#include "content/browser/compositor/browser_compositor_output_surface_proxy.h"
#include "content/browser/compositor/software_browser_compositor_output_surface.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/test/context_factories_for_test.h"
#include "ui/gfx/vsync_provider.h"

namespace {

class FakeVSyncProvider : public gfx::VSyncProvider {
 public:
  FakeVSyncProvider() : call_count_(0) {}
  virtual ~FakeVSyncProvider() {}

  virtual void GetVSyncParameters(const UpdateVSyncCallback& callback)
      OVERRIDE {
    callback.Run(timebase_, interval_);
    call_count_++;
  }

  int call_count() const { return call_count_; }

  void set_timebase(base::TimeTicks timebase) { timebase_ = timebase; }
  void set_interval(base::TimeDelta interval) { interval_ = interval; }

 private:
  base::TimeTicks timebase_;
  base::TimeDelta interval_;

  int call_count_;

  DISALLOW_COPY_AND_ASSIGN(FakeVSyncProvider);
};

class FakeSoftwareOutputDevice : public cc::SoftwareOutputDevice {
 public:
  FakeSoftwareOutputDevice() : vsync_provider_(new FakeVSyncProvider()) {}
  virtual ~FakeSoftwareOutputDevice() {}

  virtual gfx::VSyncProvider* GetVSyncProvider() OVERRIDE {
    return vsync_provider_.get();
  }

 private:
  scoped_ptr<gfx::VSyncProvider> vsync_provider_;

  DISALLOW_COPY_AND_ASSIGN(FakeSoftwareOutputDevice);
};

}  // namespace

class SoftwareBrowserCompositorOutputSurfaceTest : public testing::Test {
 public:
  SoftwareBrowserCompositorOutputSurfaceTest();
  virtual ~SoftwareBrowserCompositorOutputSurfaceTest();

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  scoped_ptr<content::BrowserCompositorOutputSurface> CreateSurface(
      scoped_ptr<cc::SoftwareOutputDevice> device);

 protected:
  scoped_ptr<content::BrowserCompositorOutputSurface> output_surface_;

  scoped_ptr<base::MessageLoop> message_loop_;
  scoped_ptr<ui::Compositor> compositor_;

  IDMap<content::BrowserCompositorOutputSurface> surface_map_;
  scoped_refptr<content::BrowserCompositorOutputSurfaceProxy> surface_proxy_;

  DISALLOW_COPY_AND_ASSIGN(SoftwareBrowserCompositorOutputSurfaceTest);
};

SoftwareBrowserCompositorOutputSurfaceTest::
    SoftwareBrowserCompositorOutputSurfaceTest() {
  // |message_loop_| is not used, but the main thread still has to exist for the
  // compositor to use.
  message_loop_.reset(new base::MessageLoopForUI);
}

SoftwareBrowserCompositorOutputSurfaceTest::
    ~SoftwareBrowserCompositorOutputSurfaceTest() {}

void SoftwareBrowserCompositorOutputSurfaceTest::SetUp() {
  bool enable_pixel_output = false;
  ui::ContextFactory* context_factory =
      ui::InitializeContextFactoryForTests(enable_pixel_output);

  compositor_.reset(new ui::Compositor(gfx::kNullAcceleratedWidget,
                                       context_factory));
  surface_proxy_ =
      new content::BrowserCompositorOutputSurfaceProxy(&surface_map_);
}

void SoftwareBrowserCompositorOutputSurfaceTest::TearDown() {
  output_surface_.reset();
  compositor_.reset();

  EXPECT_TRUE(surface_map_.IsEmpty());

  surface_map_.Clear();
  ui::TerminateContextFactoryForTests();
}

scoped_ptr<content::BrowserCompositorOutputSurface>
SoftwareBrowserCompositorOutputSurfaceTest::CreateSurface(
    scoped_ptr<cc::SoftwareOutputDevice> device) {
  return scoped_ptr<content::BrowserCompositorOutputSurface>(
      new content::SoftwareBrowserCompositorOutputSurface(
          surface_proxy_,
          device.Pass(),
          1,
          &surface_map_,
          compositor_->vsync_manager()));
}

TEST_F(SoftwareBrowserCompositorOutputSurfaceTest, NoVSyncProvider) {
  scoped_ptr<cc::SoftwareOutputDevice> software_device(
      new cc::SoftwareOutputDevice());
  output_surface_ = CreateSurface(software_device.Pass());

  cc::CompositorFrame frame;
  output_surface_->SwapBuffers(&frame);

  EXPECT_EQ(NULL, output_surface_->software_device()->GetVSyncProvider());
}

TEST_F(SoftwareBrowserCompositorOutputSurfaceTest, VSyncProviderUpdates) {
  scoped_ptr<cc::SoftwareOutputDevice> software_device(
      new FakeSoftwareOutputDevice());
  output_surface_ = CreateSurface(software_device.Pass());

  FakeVSyncProvider* vsync_provider = static_cast<FakeVSyncProvider*>(
      output_surface_->software_device()->GetVSyncProvider());
  EXPECT_EQ(0, vsync_provider->call_count());

  cc::CompositorFrame frame;
  output_surface_->SwapBuffers(&frame);

  EXPECT_EQ(1, vsync_provider->call_count());
}
