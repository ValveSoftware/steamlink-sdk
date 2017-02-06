// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/render_widget_host_view_child_frame.h"

#include <stdint.h>

#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "build/build_config.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_manager.h"
#include "cc/surfaces/surface_sequence.h"
#include "content/browser/compositor/test/no_transport_image_transport_factory.h"
#include "content/browser/frame_host/cross_process_frame_connector.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/view_messages.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/test_render_view_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {
class MockRenderWidgetHostDelegate : public RenderWidgetHostDelegate {
 public:
  MockRenderWidgetHostDelegate() {}
  ~MockRenderWidgetHostDelegate() override {}
 private:
  void Cut() override {}
  void Copy() override {}
  void Paste() override {}
  void SelectAll() override {}
};

class MockCrossProcessFrameConnector : public CrossProcessFrameConnector {
 public:
  MockCrossProcessFrameConnector()
      : CrossProcessFrameConnector(nullptr), last_scale_factor_received_(0.f) {}
  ~MockCrossProcessFrameConnector() override {}

  void SetChildFrameSurface(const cc::SurfaceId& surface_id,
                            const gfx::Size& frame_size,
                            float scale_factor,
                            const cc::SurfaceSequence& sequence) override {
    last_surface_id_received_ = surface_id;
    last_frame_size_received_ = frame_size;
    last_scale_factor_received_ = scale_factor;
  }

  RenderWidgetHostViewBase* GetParentRenderWidgetHostView() override {
    return nullptr;
  }

  cc::SurfaceId last_surface_id_received_;
  gfx::Size last_frame_size_received_;
  float last_scale_factor_received_;
};

}  // namespace

class RenderWidgetHostViewChildFrameTest : public testing::Test {
 public:
  RenderWidgetHostViewChildFrameTest() {}

  void SetUp() override {
    browser_context_.reset(new TestBrowserContext);

// ImageTransportFactory doesn't exist on Android.
#if !defined(OS_ANDROID)
    ImageTransportFactory::InitializeForUnitTests(
        base::WrapUnique(new NoTransportImageTransportFactory));
#endif

    MockRenderProcessHost* process_host =
        new MockRenderProcessHost(browser_context_.get());
    int32_t routing_id = process_host->GetNextRoutingID();
    widget_host_ =
        new RenderWidgetHostImpl(&delegate_, process_host, routing_id, false);
    view_ = new RenderWidgetHostViewChildFrame(widget_host_);

    test_frame_connector_ = new MockCrossProcessFrameConnector();
    view_->SetCrossProcessFrameConnector(test_frame_connector_);
  }

  void TearDown() override {
    if (view_)
      view_->Destroy();
    delete widget_host_;
    delete test_frame_connector_;

    browser_context_.reset();

    message_loop_.DeleteSoon(FROM_HERE, browser_context_.release());
    message_loop_.RunUntilIdle();
#if !defined(OS_ANDROID)
    ImageTransportFactory::Terminate();
#endif
  }

  cc::SurfaceId surface_id() { return view_->surface_id_; }

 protected:
  base::MessageLoopForUI message_loop_;
  std::unique_ptr<BrowserContext> browser_context_;
  MockRenderWidgetHostDelegate delegate_;

  // Tests should set these to NULL if they've already triggered their
  // destruction.
  RenderWidgetHostImpl* widget_host_;
  RenderWidgetHostViewChildFrame* view_;
  MockCrossProcessFrameConnector* test_frame_connector_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewChildFrameTest);
};

cc::CompositorFrame CreateDelegatedFrame(float scale_factor,
                                         gfx::Size size,
                                         const gfx::Rect& damage) {
  cc::CompositorFrame frame;
  frame.metadata.device_scale_factor = scale_factor;
  frame.delegated_frame_data.reset(new cc::DelegatedFrameData);

  std::unique_ptr<cc::RenderPass> pass = cc::RenderPass::Create();
  pass->SetNew(cc::RenderPassId(1, 1), gfx::Rect(size), damage,
               gfx::Transform());
  frame.delegated_frame_data->render_pass_list.push_back(std::move(pass));
  return frame;
}

TEST_F(RenderWidgetHostViewChildFrameTest, VisibilityTest) {
  view_->Show();
  ASSERT_TRUE(view_->IsShowing());

  view_->Hide();
  ASSERT_FALSE(view_->IsShowing());
}

// Verify that OnSwapCompositorFrame behavior is correct when a delegated
// frame is received from a renderer process.
TEST_F(RenderWidgetHostViewChildFrameTest, SwapCompositorFrame) {
  gfx::Size view_size(100, 100);
  gfx::Rect view_rect(view_size);
  float scale_factor = 1.f;

  view_->SetSize(view_size);
  view_->Show();

  view_->OnSwapCompositorFrame(
      0, CreateDelegatedFrame(scale_factor, view_size, view_rect));

  cc::SurfaceId id = surface_id();
  if (!id.is_null()) {
#if !defined(OS_ANDROID)
    ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
    cc::SurfaceManager* manager = factory->GetSurfaceManager();
    cc::Surface* surface = manager->GetSurfaceForId(id);
    EXPECT_TRUE(surface);
    // There should be a SurfaceSequence created by the RWHVChildFrame.
    EXPECT_EQ(1u, surface->GetDestructionDependencyCount());
#endif

    // Surface ID should have been passed to CrossProcessFrameConnector to
    // be sent to the embedding renderer.
    EXPECT_EQ(id, test_frame_connector_->last_surface_id_received_);
    EXPECT_EQ(view_size, test_frame_connector_->last_frame_size_received_);
    EXPECT_EQ(scale_factor, test_frame_connector_->last_scale_factor_received_);
  }
}

}  // namespace content
