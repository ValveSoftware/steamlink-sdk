// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/output_surface.h"

#include "base/memory/ptr_util.h"
#include "base/test/test_simple_task_runner.h"
#include "cc/output/managed_memory_policy.h"
#include "cc/output/output_surface_client.h"
#include "cc/output/software_output_device.h"
#include "cc/test/begin_frame_args_test.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/test_context_provider.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class TestOutputSurface : public OutputSurface {
 public:
  explicit TestOutputSurface(
      scoped_refptr<TestContextProvider> context_provider)
      : OutputSurface(std::move(context_provider), nullptr, nullptr) {}

  TestOutputSurface(scoped_refptr<TestContextProvider> context_provider,
                    scoped_refptr<TestContextProvider> worker_context_provider)
      : OutputSurface(std::move(context_provider),
                      std::move(worker_context_provider),
                      nullptr) {}

  explicit TestOutputSurface(
      std::unique_ptr<SoftwareOutputDevice> software_device)
      : OutputSurface(nullptr, nullptr, std::move(software_device)) {}

  TestOutputSurface(scoped_refptr<TestContextProvider> context_provider,
                    std::unique_ptr<SoftwareOutputDevice> software_device)
      : OutputSurface(std::move(context_provider),
                      nullptr,
                      std::move(software_device)) {}

  void SwapBuffers(CompositorFrame frame) override {
    client_->DidSwapBuffers();
    client_->DidSwapBuffersComplete();
  }
  uint32_t GetFramebufferCopyTextureFormat() override {
    // TestContextProvider has no real framebuffer, just use RGB.
    return GL_RGB;
  }

  void DidSwapBuffersForTesting() { client_->DidSwapBuffers(); }

  void OnSwapBuffersCompleteForTesting() { client_->DidSwapBuffersComplete(); }

 protected:
};

class TestSoftwareOutputDevice : public SoftwareOutputDevice {
 public:
  TestSoftwareOutputDevice();
  ~TestSoftwareOutputDevice() override;

  // Overriden from cc:SoftwareOutputDevice
  void DiscardBackbuffer() override;
  void EnsureBackbuffer() override;

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

  // Verify DidLoseOutputSurface callback is hooked up correctly.
  EXPECT_FALSE(client.did_lose_output_surface_called());
  output_surface.context_provider()->ContextGL()->LoseContextCHROMIUM(
      GL_GUILTY_CONTEXT_RESET_ARB, GL_INNOCENT_CONTEXT_RESET_ARB);
  output_surface.context_provider()->ContextGL()->Flush();
  EXPECT_TRUE(client.did_lose_output_surface_called());
}

TEST(OutputSurfaceTest, ClientPointerIndicatesWorkerBindToClientSuccess) {
  scoped_refptr<TestContextProvider> provider = TestContextProvider::Create();
  scoped_refptr<TestContextProvider> worker_provider =
      TestContextProvider::Create();
  TestOutputSurface output_surface(provider, worker_provider);
  EXPECT_FALSE(output_surface.HasClient());

  FakeOutputSurfaceClient client;
  EXPECT_TRUE(output_surface.BindToClient(&client));
  EXPECT_TRUE(output_surface.HasClient());

  // Verify DidLoseOutputSurface callback is hooked up correctly.
  EXPECT_FALSE(client.did_lose_output_surface_called());
  output_surface.context_provider()->ContextGL()->LoseContextCHROMIUM(
      GL_GUILTY_CONTEXT_RESET_ARB, GL_INNOCENT_CONTEXT_RESET_ARB);
  output_surface.context_provider()->ContextGL()->Flush();
  EXPECT_TRUE(client.did_lose_output_surface_called());
}

// TODO(danakj): Add a test for worker context failure as well when
// OutputSurface creates/binds it.
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

TEST(OutputSurfaceTest, SoftwareOutputDeviceBackbufferManagement) {
  TestSoftwareOutputDevice* software_output_device =
      new TestSoftwareOutputDevice();

  // TestOutputSurface now owns software_output_device and has responsibility to
  // free it.
  TestOutputSurface output_surface(base::WrapUnique(software_output_device));

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
