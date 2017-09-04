// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "blimp/client/public/blimp_client_context.h"
#include "blimp/client/public/contents/blimp_contents.h"
#include "blimp/client/public/contents/blimp_contents_observer.h"
#include "blimp/client/public/contents/blimp_contents_view.h"
#include "blimp/client/public/contents/blimp_navigation_controller.h"
#include "blimp/client/support/compositor/compositor_dependencies_impl.h"
#include "blimp/client/test/compositor/test_blimp_embedder_compositor.h"
#include "blimp/client/test/test_blimp_client_context_delegate.h"
#include "blimp/engine/browser_tests/blimp_browser_test.h"
#include "blimp/engine/browser_tests/blimp_contents_view_readback_helper.h"
#include "blimp/engine/browser_tests/waitable_content_pump.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkGraphics.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/init/gl_factory.h"

namespace blimp {
namespace {

class PageLoadWaitBlimpContentsObserver : public client::BlimpContentsObserver {
 public:
  explicit PageLoadWaitBlimpContentsObserver(client::BlimpContents* contents)
      : client::BlimpContentsObserver(contents), waiter_(nullptr) {}
  ~PageLoadWaitBlimpContentsObserver() override = default;

  // BlimpContentsObserver implementation.
  void OnPageLoadingStateChanged(bool loading) override {
    if (!loading && waiter_)
      waiter_->Signal();
  }

  void WaitForPageLoadToFinish(base::WaitableEvent* waiter) {
    waiter_ = waiter;
  }

 private:
  base::WaitableEvent* waiter_;
};

class BlimpIntegrationTest : public BlimpBrowserTest {
 public:
  BlimpIntegrationTest() {}

 protected:
  void SetUpOnMainThread() override {
    BlimpBrowserTest::SetUpOnMainThread();

    AllowUIWaits();

    {
      base::ThreadRestrictions::ScopedAllowIO allow_io;
      ASSERT_TRUE(gl::init::InitializeGLOneOff());
    }
    SkGraphics::Init();

    // After GL is initialized, build a gl::DisableNullDrawGLBindings to make
    // sure GL actually draws for this test.
    scoped_enable_gl_bindings_ =
        base::MakeUnique<gl::DisableNullDrawGLBindings>();

    std::unique_ptr<client::CompositorDependencies> compositor_dependencies =
        base::MakeUnique<client::CompositorDependenciesImpl>();

    compositor_ = base::MakeUnique<client::TestBlimpEmbedderCompositor>(
        compositor_dependencies.get());

    delegate_ = base::MakeUnique<client::TestBlimpClientContextDelegate>();

    client::BlimpClientContext::RegisterPrefs(prefs_.registry());

    context_ = base::WrapUnique<client::BlimpClientContext>(
        client::BlimpClientContext::Create(
            content::BrowserThread::GetTaskRunnerForThread(
                content::BrowserThread::IO),
            content::BrowserThread::GetTaskRunnerForThread(
                content::BrowserThread::FILE),
            std::move(compositor_dependencies), &prefs_));

    context_->SetDelegate(delegate_.get());
    context_->ConnectWithAssignment(GetAssignment());
    contents_ = context_->CreateBlimpContents(nullptr);

    compositor_->SetContentLayer(contents_->GetView()->GetLayer());
  }

  void TearDownOnMainThread() override {
    // Tear down the client objects.  Ordering is important.
    contents_.reset();
    compositor_.reset();
    context_.reset();
    delegate_.reset();
    BlimpBrowserTest::TearDownOnMainThread();
  }

  std::unique_ptr<client::BlimpClientContextDelegate> delegate_;
  std::unique_ptr<client::BlimpClientContext> context_;
  std::unique_ptr<client::TestBlimpEmbedderCompositor> compositor_;
  std::unique_ptr<client::BlimpContents> contents_;

 private:
  TestingPrefServiceSimple prefs_;
  std::unique_ptr<gl::DisableNullDrawGLBindings> scoped_enable_gl_bindings_;

  DISALLOW_COPY_AND_ASSIGN(BlimpIntegrationTest);
};

IN_PROC_BROWSER_TEST_F(BlimpIntegrationTest, TestFullRenderPath) {
  WaitableContentPump waiter;

  PageLoadWaitBlimpContentsObserver observer(contents_.get());

  // Set up the compositor viewport to be a 1x1 screen.  We only need a single
  // pixel for validation.
  compositor_->SetSize(gfx::Size(1, 1));
  contents_->GetView()->SetSizeAndScale(gfx::Size(1, 1), 1.f);

  // Show the BlimpContents and trigger a load of a URL that sets a red
  // background.
  contents_->Show();
  contents_->GetNavigationController().LoadURL(
      GURL("data:text/html;charset=utf-8,<html><body "
           "style=background-color:#FF0000></body></html>"));

  // Block the test execution until loading finishes.
  observer.WaitForPageLoadToFinish(waiter.event());
  waiter.Wait();

  // Grab a readback of the page content.  Block the test execution until the
  // readback completes.
  BlimpContentsViewReadbackHelper readback_helper;
  contents_->GetView()->CopyFromCompositingSurface(
      readback_helper.GetCallback());
  WaitableContentPump::WaitFor(readback_helper.event());

  // Validate the readback bitmap basic characteristics.
  SkBitmap* bitmap = readback_helper.GetBitmap();
  EXPECT_NE(nullptr, bitmap);
  EXPECT_EQ(1, bitmap->width());
  EXPECT_EQ(1, bitmap->height());

  // Validate the readback bitmap contents.
  SkAutoLockPixels bitmap_lock(*bitmap);
  EXPECT_EQ(SK_ColorRED, bitmap->getColor(0, 0));
}

}  // namespace
}  // namespace blimp
