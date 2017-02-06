// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/render_widget_host_view_guest.h"

#include <stdint.h>
#include <utility>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_manager.h"
#include "cc/surfaces/surface_sequence.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/compositor/test/no_transport_image_transport_factory.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_plugin_guest_delegate.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {
class MockRenderWidgetHostDelegate : public RenderWidgetHostDelegate {
 public:
  MockRenderWidgetHostDelegate() {}
  ~MockRenderWidgetHostDelegate() override {}

 private:
  // RenderWidgetHostDelegate:
  void Cut() override {}
  void Copy() override {}
  void Paste() override {}
  void SelectAll() override {}
};

class RenderWidgetHostViewGuestTest : public testing::Test {
 public:
  RenderWidgetHostViewGuestTest() {}

  void SetUp() override {
#if !defined(OS_ANDROID)
    ImageTransportFactory::InitializeForUnitTests(
        std::unique_ptr<ImageTransportFactory>(
            new NoTransportImageTransportFactory));
#endif
    browser_context_.reset(new TestBrowserContext);
    MockRenderProcessHost* process_host =
        new MockRenderProcessHost(browser_context_.get());
    int32_t routing_id = process_host->GetNextRoutingID();
    widget_host_ =
        new RenderWidgetHostImpl(&delegate_, process_host, routing_id, false);
    view_ = new RenderWidgetHostViewGuest(
        widget_host_, NULL,
        (new TestRenderWidgetHostView(widget_host_))->GetWeakPtr());
  }

  void TearDown() override {
    if (view_)
      view_->Destroy();
    delete widget_host_;

    browser_context_.reset();

    message_loop_.DeleteSoon(FROM_HERE, browser_context_.release());
    message_loop_.RunUntilIdle();
#if !defined(OS_ANDROID)
    ImageTransportFactory::Terminate();
#endif
  }

 protected:
  base::MessageLoopForUI message_loop_;
  std::unique_ptr<BrowserContext> browser_context_;
  MockRenderWidgetHostDelegate delegate_;

  // Tests should set these to NULL if they've already triggered their
  // destruction.
  RenderWidgetHostImpl* widget_host_;
  RenderWidgetHostViewGuest* view_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewGuestTest);
};

}  // namespace

TEST_F(RenderWidgetHostViewGuestTest, VisibilityTest) {
  view_->Show();
  ASSERT_TRUE(view_->IsShowing());

  view_->Hide();
  ASSERT_FALSE(view_->IsShowing());
}

class TestBrowserPluginGuest : public BrowserPluginGuest {
 public:
  TestBrowserPluginGuest(WebContentsImpl* web_contents,
                         BrowserPluginGuestDelegate* delegate)
      : BrowserPluginGuest(web_contents->HasOpener(), web_contents, delegate),
        last_scale_factor_received_(0.f) {}
  ~TestBrowserPluginGuest() override {}

  void ResetTestData() {
    last_surface_id_received_ = cc::SurfaceId();
    last_frame_size_received_ = gfx::Size();
    last_scale_factor_received_ = 0.f;
  }

  void set_has_attached_since_surface_set(bool has_attached_since_surface_set) {
    BrowserPluginGuest::set_has_attached_since_surface_set_for_test(
        has_attached_since_surface_set);
  }

  void set_attached(bool attached) {
    BrowserPluginGuest::set_attached_for_test(attached);
  }

  void SetChildFrameSurface(const cc::SurfaceId& surface_id,
                            const gfx::Size& frame_size,
                            float scale_factor,
                            const cc::SurfaceSequence& sequence) override {
    last_surface_id_received_ = surface_id;
    last_frame_size_received_ = frame_size;
    last_scale_factor_received_ = scale_factor;
  }

  cc::SurfaceId last_surface_id_received_;
  gfx::Size last_frame_size_received_;
  float last_scale_factor_received_;
};

// TODO(wjmaclean): we should restructure RenderWidgetHostViewChildFrameTest to
// look more like this one, and then this one could be derived from it. Also,
// include CreateDelegatedFrame as part of the test class so we don't have to
// repeat it here.
class RenderWidgetHostViewGuestSurfaceTest
    : public testing::Test {
 public:
  RenderWidgetHostViewGuestSurfaceTest()
      : widget_host_(nullptr), view_(nullptr) {}

  void SetUp() override {
#if !defined(OS_ANDROID)
    ImageTransportFactory::InitializeForUnitTests(
        std::unique_ptr<ImageTransportFactory>(
            new NoTransportImageTransportFactory));
#endif
    browser_context_.reset(new TestBrowserContext);
    MockRenderProcessHost* process_host =
        new MockRenderProcessHost(browser_context_.get());
    web_contents_.reset(
        TestWebContents::Create(browser_context_.get(), nullptr));
    // We don't own the BPG, the WebContents does.
    browser_plugin_guest_ = new TestBrowserPluginGuest(
        web_contents_.get(), &browser_plugin_guest_delegate_);

    int32_t routing_id = process_host->GetNextRoutingID();
    widget_host_ =
        new RenderWidgetHostImpl(&delegate_, process_host, routing_id, false);
    view_ = new RenderWidgetHostViewGuest(
        widget_host_, browser_plugin_guest_,
        (new TestRenderWidgetHostView(widget_host_))->GetWeakPtr());
  }

  void TearDown() override {
    if (view_)
      view_->Destroy();
    delete widget_host_;

    // It's important to make sure that the view finishes destructing before
    // we hit the destructor for the TestBrowserThreadBundle, so run the message
    // loop here.
    base::RunLoop().RunUntilIdle();
#if !defined(OS_ANDROID)
    ImageTransportFactory::Terminate();
#endif
  }

  cc::SurfaceId surface_id() {
    DCHECK(view_);
    return static_cast<RenderWidgetHostViewChildFrame*>(view_)->surface_id_;
  }

 protected:
  TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<BrowserContext> browser_context_;
  MockRenderWidgetHostDelegate delegate_;
  BrowserPluginGuestDelegate browser_plugin_guest_delegate_;
  std::unique_ptr<TestWebContents> web_contents_;
  TestBrowserPluginGuest* browser_plugin_guest_;

  // Tests should set these to NULL if they've already triggered their
  // destruction.
  RenderWidgetHostImpl* widget_host_;
  RenderWidgetHostViewGuest* view_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewGuestSurfaceTest);
};

namespace {
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
}  // anonymous namespace

TEST_F(RenderWidgetHostViewGuestSurfaceTest, TestGuestSurface) {
  gfx::Size view_size(100, 100);
  gfx::Rect view_rect(view_size);
  float scale_factor = 1.f;

  ASSERT_TRUE(browser_plugin_guest_);

  view_->SetSize(view_size);
  view_->Show();

  browser_plugin_guest_->set_attached(true);
  view_->OnSwapCompositorFrame(
      0, CreateDelegatedFrame(scale_factor, view_size, view_rect));

  cc::SurfaceId id = surface_id();
  if (!id.is_null()) {
#if !defined(OS_ANDROID)
    ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
    cc::SurfaceManager* manager = factory->GetSurfaceManager();
    cc::Surface* surface = manager->GetSurfaceForId(id);
    EXPECT_TRUE(surface);
    // There should be a SurfaceSequence created by the RWHVGuest.
    EXPECT_EQ(1u, surface->GetDestructionDependencyCount());
#endif
    // Surface ID should have been passed to BrowserPluginGuest to
    // be sent to the embedding renderer.
    EXPECT_EQ(id, browser_plugin_guest_->last_surface_id_received_);
    EXPECT_EQ(view_size, browser_plugin_guest_->last_frame_size_received_);
    EXPECT_EQ(scale_factor, browser_plugin_guest_->last_scale_factor_received_);
  }

  browser_plugin_guest_->ResetTestData();
  browser_plugin_guest_->set_has_attached_since_surface_set(true);

  view_->OnSwapCompositorFrame(
      0, CreateDelegatedFrame(scale_factor, view_size, view_rect));

  id = surface_id();
  if (!id.is_null()) {
#if !defined(OS_ANDROID)
    ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
    cc::SurfaceManager* manager = factory->GetSurfaceManager();
    cc::Surface* surface = manager->GetSurfaceForId(id);
    EXPECT_TRUE(surface);
    // There should be a SurfaceSequence created by the RWHVGuest.
    EXPECT_EQ(1u, surface->GetDestructionDependencyCount());
#endif
    // Surface ID should have been passed to BrowserPluginGuest to
    // be sent to the embedding renderer.
    EXPECT_EQ(id, browser_plugin_guest_->last_surface_id_received_);
    EXPECT_EQ(view_size, browser_plugin_guest_->last_frame_size_received_);
    EXPECT_EQ(scale_factor,
              browser_plugin_guest_->last_scale_factor_received_);
  }

  browser_plugin_guest_->set_attached(false);
  browser_plugin_guest_->ResetTestData();

  view_->OnSwapCompositorFrame(
      0, CreateDelegatedFrame(scale_factor, view_size, view_rect));
  EXPECT_TRUE(surface_id().is_null());
}

}  // namespace content
