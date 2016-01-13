// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/output_surface.h"

#include "base/test/test_simple_task_runner.h"
#include "cc/output/managed_memory_policy.h"
#include "cc/output/output_surface_client.h"
#include "cc/output/software_output_device.h"
#include "cc/test/begin_frame_args_test.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/scheduler_test_common.h"
#include "cc/test/test_context_provider.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/frame_time.h"

namespace cc {
namespace {

class TestOutputSurface : public OutputSurface {
 public:
  explicit TestOutputSurface(scoped_refptr<ContextProvider> context_provider)
      : OutputSurface(context_provider) {}

  explicit TestOutputSurface(scoped_ptr<SoftwareOutputDevice> software_device)
      : OutputSurface(software_device.Pass()) {}

  TestOutputSurface(scoped_refptr<ContextProvider> context_provider,
                    scoped_ptr<SoftwareOutputDevice> software_device)
      : OutputSurface(context_provider, software_device.Pass()) {}

  bool InitializeNewContext3d(
      scoped_refptr<ContextProvider> new_context_provider) {
    return InitializeAndSetContext3d(new_context_provider);
  }

  using OutputSurface::ReleaseGL;

  void CommitVSyncParametersForTesting(base::TimeTicks timebase,
                                       base::TimeDelta interval) {
    CommitVSyncParameters(timebase, interval);
  }

  void BeginFrameForTesting() {
    client_->BeginFrame(CreateExpiredBeginFrameArgsForTesting());
  }

  void DidSwapBuffersForTesting() { client_->DidSwapBuffers(); }

  void OnSwapBuffersCompleteForTesting() { client_->DidSwapBuffersComplete(); }

 protected:
};

class TestSoftwareOutputDevice : public SoftwareOutputDevice {
 public:
  TestSoftwareOutputDevice();
  virtual ~TestSoftwareOutputDevice();

  // Overriden from cc:SoftwareOutputDevice
  virtual void DiscardBackbuffer() OVERRIDE;
  virtual void EnsureBackbuffer() OVERRIDE;

  int discard_backbuffer_count() { return discard_backbuffer_count_; }
  int ensure_backbuffer_count() { return ensure_backbuffer_count_; }

 private:
  int discard_backbuffer_count_;
  int ensure_backbuffer_count_;
};

TestSoftwareOutputDevice::TestSoftwareOutputDevice()
    : discard_backbuffer_count_(0), ensure_backbuffer_count_(0) {}

TestSoftwareOutputDevice::~TestSoftwareOutputDevice() {}

void TestSoftwareOutputDevice::DiscardBackbuffer() {
  SoftwareOutputDevice::DiscardBackbuffer();
  discard_backbuffer_count_++;
}

void TestSoftwareOutputDevice::EnsureBackbuffer() {
  SoftwareOutputDevice::EnsureBackbuffer();
  ensure_backbuffer_count_++;
}

TEST(OutputSurfaceTest, ClientPointerIndicatesBindToClientSuccess) {
  scoped_refptr<TestContextProvider> provider = TestContextProvider::Create();
  TestOutputSurface output_surface(provider);
  EXPECT_FALSE(output_surface.HasClient());

  FakeOutputSurfaceClient client;
  EXPECT_TRUE(output_surface.BindToClient(&client));
  EXPECT_TRUE(output_surface.HasClient());
  EXPECT_FALSE(client.deferred_initialize_called());

  // Verify DidLoseOutputSurface callback is hooked up correctly.
  EXPECT_FALSE(client.did_lose_output_surface_called());
  output_surface.context_provider()->ContextGL()->LoseContextCHROMIUM(
      GL_GUILTY_CONTEXT_RESET_ARB, GL_INNOCENT_CONTEXT_RESET_ARB);
  output_surface.context_provider()->ContextGL()->Flush();
  EXPECT_TRUE(client.did_lose_output_surface_called());
}

TEST(OutputSurfaceTest, ClientPointerIndicatesBindToClientFailure) {
  scoped_refptr<TestContextProvider> context_provider =
      TestContextProvider::Create();

  // Lose the context so BindToClient fails.
  context_provider->UnboundTestContext3d()->set_context_lost(true);

  TestOutputSurface output_surface(context_provider);
  EXPECT_FALSE(output_surface.HasClient());

  FakeOutputSurfaceClient client;
  EXPECT_FALSE(output_surface.BindToClient(&client));
  EXPECT_FALSE(output_surface.HasClient());
}

class OutputSurfaceTestInitializeNewContext3d : public ::testing::Test {
 public:
  OutputSurfaceTestInitializeNewContext3d()
      : context_provider_(TestContextProvider::Create()),
        output_surface_(
            scoped_ptr<SoftwareOutputDevice>(new SoftwareOutputDevice)),
        client_(&output_surface_) {}

 protected:
  void BindOutputSurface() {
    EXPECT_TRUE(output_surface_.BindToClient(&client_));
    EXPECT_TRUE(output_surface_.HasClient());
  }

  void InitializeNewContextExpectFail() {
    EXPECT_FALSE(output_surface_.InitializeNewContext3d(context_provider_));
    EXPECT_TRUE(output_surface_.HasClient());

    EXPECT_FALSE(output_surface_.context_provider());
    EXPECT_TRUE(output_surface_.software_device());
  }

  scoped_refptr<TestContextProvider> context_provider_;
  TestOutputSurface output_surface_;
  FakeOutputSurfaceClient client_;
};

TEST_F(OutputSurfaceTestInitializeNewContext3d, Success) {
  BindOutputSurface();
  EXPECT_FALSE(client_.deferred_initialize_called());

  EXPECT_TRUE(output_surface_.InitializeNewContext3d(context_provider_));
  EXPECT_TRUE(client_.deferred_initialize_called());
  EXPECT_EQ(context_provider_, output_surface_.context_provider());

  EXPECT_FALSE(client_.did_lose_output_surface_called());
  context_provider_->ContextGL()->LoseContextCHROMIUM(
      GL_GUILTY_CONTEXT_RESET_ARB, GL_INNOCENT_CONTEXT_RESET_ARB);
  context_provider_->ContextGL()->Flush();
  EXPECT_TRUE(client_.did_lose_output_surface_called());

  output_surface_.ReleaseGL();
  EXPECT_FALSE(output_surface_.context_provider());
}

TEST_F(OutputSurfaceTestInitializeNewContext3d, Context3dMakeCurrentFails) {
  BindOutputSurface();

  context_provider_->UnboundTestContext3d()->set_context_lost(true);
  InitializeNewContextExpectFail();
}

TEST(OutputSurfaceTest, MemoryAllocation) {
  scoped_refptr<TestContextProvider> context_provider =
      TestContextProvider::Create();

  TestOutputSurface output_surface(context_provider);

  FakeOutputSurfaceClient client;
  EXPECT_TRUE(output_surface.BindToClient(&client));

  ManagedMemoryPolicy policy(0);
  policy.bytes_limit_when_visible = 1234;
  policy.priority_cutoff_when_visible =
      gpu::MemoryAllocation::CUTOFF_ALLOW_REQUIRED_ONLY;

  context_provider->SetMemoryAllocation(policy);
  EXPECT_EQ(1234u, client.memory_policy().bytes_limit_when_visible);
  EXPECT_EQ(gpu::MemoryAllocation::CUTOFF_ALLOW_REQUIRED_ONLY,
            client.memory_policy().priority_cutoff_when_visible);

  policy.priority_cutoff_when_visible =
      gpu::MemoryAllocation::CUTOFF_ALLOW_EVERYTHING;
  context_provider->SetMemoryAllocation(policy);
  EXPECT_EQ(gpu::MemoryAllocation::CUTOFF_ALLOW_EVERYTHING,
            client.memory_policy().priority_cutoff_when_visible);

  // 0 bytes limit should be ignored.
  policy.bytes_limit_when_visible = 0;
  context_provider->SetMemoryAllocation(policy);
  EXPECT_EQ(1234u, client.memory_policy().bytes_limit_when_visible);
}

TEST(OutputSurfaceTest, SoftwareOutputDeviceBackbufferManagement) {
  TestSoftwareOutputDevice* software_output_device =
      new TestSoftwareOutputDevice();

  // TestOutputSurface now owns software_output_device and has responsibility to
  // free it.
  scoped_ptr<TestSoftwareOutputDevice> p(software_output_device);
  TestOutputSurface output_surface(p.PassAs<SoftwareOutputDevice>());

  EXPECT_EQ(0, software_output_device->ensure_backbuffer_count());
  EXPECT_EQ(0, software_output_device->discard_backbuffer_count());

  output_surface.EnsureBackbuffer();
  EXPECT_EQ(1, software_output_device->ensure_backbuffer_count());
  EXPECT_EQ(0, software_output_device->discard_backbuffer_count());
  output_surface.DiscardBackbuffer();

  EXPECT_EQ(1, software_output_device->ensure_backbuffer_count());
  EXPECT_EQ(1, software_output_device->discard_backbuffer_count());
}

}  // namespace
}  // namespace cc
