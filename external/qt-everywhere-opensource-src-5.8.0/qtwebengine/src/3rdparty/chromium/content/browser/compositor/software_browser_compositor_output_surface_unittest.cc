// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/software_browser_compositor_output_surface.h"

#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/test/test_message_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/output/compositor_frame.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/scheduler/delay_based_time_source.h"
#include "cc/test/fake_output_surface_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/test/context_factories_for_test.h"
#include "ui/gfx/vsync_provider.h"

namespace {

class FakeVSyncProvider : public gfx::VSyncProvider {
 public:
  FakeVSyncProvider() : call_count_(0) {}
  ~FakeVSyncProvider() override {}

  void GetVSyncParameters(const UpdateVSyncCallback& callback) override {
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
  ~FakeSoftwareOutputDevice() override {}

  gfx::VSyncProvider* GetVSyncProvider() override {
    return vsync_provider_.get();
  }

 private:
  std::unique_ptr<gfx::VSyncProvider> vsync_provider_;

  DISALLOW_COPY_AND_ASSIGN(FakeSoftwareOutputDevice);
};

}  // namespace

class SoftwareBrowserCompositorOutputSurfaceTest : public testing::Test {
 public:
  SoftwareBrowserCompositorOutputSurfaceTest()
      : begin_frame_source_(base::MakeUnique<cc::DelayBasedTimeSource>(
            message_loop_.task_runner().get())) {}
  ~SoftwareBrowserCompositorOutputSurfaceTest() override = default;

  void SetUp() override;
  void TearDown() override;

  std::unique_ptr<content::BrowserCompositorOutputSurface> CreateSurface(
      std::unique_ptr<cc::SoftwareOutputDevice> device);

 protected:
  std::unique_ptr<content::BrowserCompositorOutputSurface> output_surface_;

  // TODO(crbug.com/616973): We shouldn't be using ThreadTaskRunnerHandle::Get()
  // inside the OutputSurface, so we shouldn't need a MessageLoop. The
  // OutputSurface should be using the TaskRunner given to the compositor.
  base::TestMessageLoop message_loop_;
  cc::DelayBasedBeginFrameSource begin_frame_source_;
  std::unique_ptr<ui::Compositor> compositor_;

  DISALLOW_COPY_AND_ASSIGN(SoftwareBrowserCompositorOutputSurfaceTest);
};

void SoftwareBrowserCompositorOutputSurfaceTest::SetUp() {
  bool enable_pixel_output = false;
  ui::ContextFactory* context_factory =
      ui::InitializeContextFactoryForTests(enable_pixel_output);

  compositor_.reset(
      new ui::Compositor(context_factory, message_loop_.task_runner().get()));
  compositor_->SetAcceleratedWidget(gfx::kNullAcceleratedWidget);
}

void SoftwareBrowserCompositorOutputSurfaceTest::TearDown() {
  output_surface_.reset();
  compositor_.reset();
  ui::TerminateContextFactoryForTests();
}

std::unique_ptr<content::BrowserCompositorOutputSurface>
SoftwareBrowserCompositorOutputSurfaceTest::CreateSurface(
    std::unique_ptr<cc::SoftwareOutputDevice> device) {
  return base::MakeUnique<content::SoftwareBrowserCompositorOutputSurface>(
      std::move(device), compositor_->vsync_manager(), &begin_frame_source_);
}

TEST_F(SoftwareBrowserCompositorOutputSurfaceTest, NoVSyncProvider) {
  cc::FakeOutputSurfaceClient output_surface_client;
  std::unique_ptr<cc::SoftwareOutputDevice> software_device(
      new cc::SoftwareOutputDevice());
  output_surface_ = CreateSurface(std::move(software_device));
  CHECK(output_surface_->BindToClient(&output_surface_client));

  cc::CompositorFrame frame;
  output_surface_->SwapBuffers(std::move(frame));

  EXPECT_EQ(1, output_surface_client.swap_count());
  EXPECT_EQ(NULL, output_surface_->software_device()->GetVSyncProvider());
}

TEST_F(SoftwareBrowserCompositorOutputSurfaceTest, VSyncProviderUpdates) {
  cc::FakeOutputSurfaceClient output_surface_client;
  std::unique_ptr<cc::SoftwareOutputDevice> software_device(
      new FakeSoftwareOutputDevice());
  output_surface_ = CreateSurface(std::move(software_device));
  CHECK(output_surface_->BindToClient(&output_surface_client));

  FakeVSyncProvider* vsync_provider = static_cast<FakeVSyncProvider*>(
      output_surface_->software_device()->GetVSyncProvider());
  EXPECT_EQ(0, vsync_provider->call_count());

  cc::CompositorFrame frame;
  output_surface_->SwapBuffers(std::move(frame));

  EXPECT_EQ(1, output_surface_client.swap_count());
  EXPECT_EQ(1, vsync_provider->call_count());
}
