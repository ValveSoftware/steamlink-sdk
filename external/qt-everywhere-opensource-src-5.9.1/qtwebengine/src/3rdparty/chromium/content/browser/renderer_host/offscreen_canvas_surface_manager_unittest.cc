// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/test/no_transport_image_transport_factory.h"
#include "content/browser/renderer_host/offscreen_canvas_surface_impl.h"
#include "content/browser/renderer_host/offscreen_canvas_surface_manager.h"
#include "content/public/test/test_browser_thread.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "content/browser/renderer_host/context_provider_factory_impl_android.h"
#include "content/test/mock_gpu_channel_establish_factory.h"
#else
#include "content/browser/compositor/image_transport_factory.h"
#endif

namespace content {

class OffscreenCanvasSurfaceManagerTest : public testing::Test {
 public:
  int getNumSurfaceImplInstances() {
    return OffscreenCanvasSurfaceManager::GetInstance()
        ->registered_surface_instances_.size();
  }
  const cc::SurfaceId& getCurrentSurfaceId() const {
    return current_surface_id_;
  }
  void setSurfaceId(const cc::SurfaceId& surface_id) {
    current_surface_id_ = surface_id;
  }

 protected:
  void SetUp() override;
  void TearDown() override;

 private:
  cc::SurfaceId current_surface_id_;
  std::unique_ptr<TestBrowserThread> ui_thread_;
  base::MessageLoopForUI message_loop_;
#if defined(OS_ANDROID)
  MockGpuChannelEstablishFactory gpu_channel_factory_;
#endif
};

void OffscreenCanvasSurfaceManagerTest::SetUp() {
#if defined(OS_ANDROID)
  ContextProviderFactoryImpl::Initialize(&gpu_channel_factory_);
  ui::ContextProviderFactory::SetInstance(
      ContextProviderFactoryImpl::GetInstance());
#else
  ImageTransportFactory::InitializeForUnitTests(
      std::unique_ptr<ImageTransportFactory>(
          new NoTransportImageTransportFactory));
#endif
  ui_thread_.reset(new TestBrowserThread(BrowserThread::UI, &message_loop_));
}

void OffscreenCanvasSurfaceManagerTest::TearDown() {
#if defined(OS_ANDROID)
  ui::ContextProviderFactory::SetInstance(nullptr);
  ContextProviderFactoryImpl::Terminate();
#else
  ImageTransportFactory::Terminate();
#endif
}

// This test mimics the workflow of OffscreenCanvas.commit() on renderer
// process.
TEST_F(OffscreenCanvasSurfaceManagerTest,
       SingleHTMLCanvasElementTransferToOffscreen) {
  // Assume that HTMLCanvasElement.transferControlToOffscreen() is triggered and
  // it will invoke GetSurfaceId function on OffscreenCanvasSurfaceImpl to
  // obtain a unique SurfaceId from browser.
  auto surface_impl = base::WrapUnique(new OffscreenCanvasSurfaceImpl());
  surface_impl->GetSurfaceId(
      base::Bind(&OffscreenCanvasSurfaceManagerTest::setSurfaceId,
                 base::Unretained(this)));

  EXPECT_TRUE(this->getCurrentSurfaceId().is_valid());
  EXPECT_EQ(1, this->getNumSurfaceImplInstances());
  cc::FrameSinkId frame_sink_id = surface_impl.get()->frame_sink_id();
  EXPECT_EQ(frame_sink_id, this->getCurrentSurfaceId().frame_sink_id());
  EXPECT_EQ(surface_impl.get(),
            OffscreenCanvasSurfaceManager::GetInstance()->GetSurfaceInstance(
                frame_sink_id));

  surface_impl = nullptr;
  EXPECT_EQ(0, this->getNumSurfaceImplInstances());
}

TEST_F(OffscreenCanvasSurfaceManagerTest,
       MultiHTMLCanvasElementTransferToOffscreen) {
  // Same scenario as above test except that now we have two HTMLCanvasElement
  // transferControlToOffscreen at the same time.
  auto surface_impl_a = base::WrapUnique(new OffscreenCanvasSurfaceImpl());
  surface_impl_a->GetSurfaceId(
      base::Bind(&OffscreenCanvasSurfaceManagerTest::setSurfaceId,
                 base::Unretained(this)));
  cc::SurfaceId surface_id_a = this->getCurrentSurfaceId();

  EXPECT_TRUE(surface_id_a.is_valid());

  auto surface_impl_b = base::WrapUnique(new OffscreenCanvasSurfaceImpl());
  surface_impl_b->GetSurfaceId(
      base::Bind(&OffscreenCanvasSurfaceManagerTest::setSurfaceId,
                 base::Unretained(this)));
  cc::SurfaceId surface_id_b = this->getCurrentSurfaceId();

  EXPECT_TRUE(surface_id_b.is_valid());
  EXPECT_NE(surface_id_a, surface_id_b);

  EXPECT_EQ(2, this->getNumSurfaceImplInstances());
  EXPECT_EQ(surface_impl_a.get(),
            OffscreenCanvasSurfaceManager::GetInstance()->GetSurfaceInstance(
                surface_id_a.frame_sink_id()));
  EXPECT_EQ(surface_impl_b.get(),
            OffscreenCanvasSurfaceManager::GetInstance()->GetSurfaceInstance(
                surface_id_b.frame_sink_id()));

  surface_impl_a = nullptr;
  EXPECT_EQ(1, this->getNumSurfaceImplInstances());
  surface_impl_b = nullptr;
  EXPECT_EQ(0, this->getNumSurfaceImplInstances());
}

}  // namespace content
