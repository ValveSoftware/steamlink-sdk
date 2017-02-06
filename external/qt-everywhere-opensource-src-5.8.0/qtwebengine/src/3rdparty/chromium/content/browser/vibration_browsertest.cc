// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "device/vibration/vibration_manager.mojom.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/shell/public/cpp/interface_registry.h"

// These tests run against a dummy implementation of the VibrationManager
// service. That is, they verify that the service implementation is correctly
// exposed to the renderer, whatever the implementation is.

namespace content {

namespace {

// Global, record milliseconds when FakeVibrationManager::Vibrate got called.
int64_t g_vibrate_milliseconds = -1;

// Global, record whether FakeVibrationManager::Cancel got called.
bool g_cancelled = false;

// Global, wait for end of execution for FakeVibrationManager::Vibrate.
scoped_refptr<MessageLoopRunner> g_wait_vibrate_runner;

// Global, wait for end of execution for FakeVibrationManager::Cancel.
scoped_refptr<MessageLoopRunner> g_wait_cancel_runner;

void ResetGlobalValues() {
  g_vibrate_milliseconds = -1;
  g_cancelled = false;

  g_wait_vibrate_runner = new MessageLoopRunner();
  g_wait_cancel_runner = new MessageLoopRunner();
}

class FakeVibrationManager : public device::VibrationManager {
 public:
  static void Create(mojo::InterfaceRequest<VibrationManager> request) {
    new FakeVibrationManager(std::move(request));
  }

 private:
  FakeVibrationManager(mojo::InterfaceRequest<VibrationManager> request)
      : binding_(this, std::move(request)) {}

  ~FakeVibrationManager() override {}

  void Vibrate(int64_t milliseconds, const VibrateCallback& callback) override {
    g_vibrate_milliseconds = milliseconds;
    callback.Run();
    g_wait_vibrate_runner->Quit();
  }

  void Cancel(const CancelCallback& callback) override {
    g_cancelled = true;
    callback.Run();
    g_wait_cancel_runner->Quit();
  }

  mojo::StrongBinding<VibrationManager> binding_;
};

class VibrationTest : public ContentBrowserTest {
 public:
  VibrationTest() {}

  void SetUpOnMainThread() override {
    ResetGlobalValues();
    GetMainFrame()->GetInterfaceRegistry()->AddInterface(
        base::Bind(&FakeVibrationManager::Create));
  }

  bool Vibrate(int duration) { return Vibrate(duration, GetMainFrame()); }

  bool Vibrate(int duration, RenderFrameHost* frame) {
    bool result;
    std::string script = "domAutomationController.send(navigator.vibrate(" +
                         base::IntToString(duration) + "))";
    EXPECT_TRUE(ExecuteScriptAndExtractBool(frame, script, &result));
    return result;
  }

  bool Cancel() {
    bool result;
    std::string script = "domAutomationController.send(navigator.vibrate(0))";
    EXPECT_TRUE(ExecuteScriptAndExtractBool(GetMainFrame(), script, &result));
    return result;
  }

  bool DestroyIframe() {
    std::string script =
        "document.getElementById('test_iframe').parentNode.innerHTML = ''";
    return ExecuteScript(GetMainFrame(), script);
  }

  RenderFrameHost* GetMainFrame() {
    return shell()->web_contents()->GetMainFrame();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(VibrationTest);
};

IN_PROC_BROWSER_TEST_F(VibrationTest, Vibrate) {
  ASSERT_EQ(-1, g_vibrate_milliseconds);

  ASSERT_TRUE(NavigateToURL(shell(), GetTestUrl(".", "simple_page.html")));
  ASSERT_TRUE(Vibrate(1234));
  g_wait_vibrate_runner->Run();

  ASSERT_EQ(1234, g_vibrate_milliseconds);
}

IN_PROC_BROWSER_TEST_F(VibrationTest, Cancel) {
  ASSERT_FALSE(g_cancelled);

  ASSERT_TRUE(NavigateToURL(shell(), GetTestUrl(".", "simple_page.html")));
  ASSERT_TRUE(Vibrate(1234));
  g_wait_vibrate_runner->Run();
  ASSERT_TRUE(Cancel());
  g_wait_cancel_runner->Run();

  ASSERT_TRUE(g_cancelled);
}

IN_PROC_BROWSER_TEST_F(VibrationTest,
                       CancelVibrationFromMainFrameWhenMainFrameIsReloaded) {
  ASSERT_FALSE(g_cancelled);

  ASSERT_TRUE(NavigateToURL(shell(), GetTestUrl(".", "simple_page.html")));
  ASSERT_TRUE(Vibrate(1234));
  g_wait_vibrate_runner->Run();
  ASSERT_TRUE(NavigateToURL(shell(), GetTestUrl(".", "simple_page.html")));
  g_wait_cancel_runner->Run();

  ASSERT_TRUE(g_cancelled);
}

IN_PROC_BROWSER_TEST_F(VibrationTest,
                       CancelVibrationFromSubFrameWhenSubFrameIsReloaded) {
  ASSERT_FALSE(g_cancelled);

  ASSERT_TRUE(NavigateToURL(shell(), GetTestUrl(".", "page_with_iframe.html")));
  RenderFrameHost* iframe = ChildFrameAt(GetMainFrame(), 0);
  iframe->GetInterfaceRegistry()->AddInterface(
      base::Bind(&FakeVibrationManager::Create));
  ASSERT_TRUE(Vibrate(1234, iframe));
  g_wait_vibrate_runner->Run();
  ASSERT_TRUE(NavigateIframeToURL(shell()->web_contents(), "test_iframe",
                                  GetTestUrl(".", "title1.html")));
  g_wait_cancel_runner->Run();

  ASSERT_TRUE(g_cancelled);
}

IN_PROC_BROWSER_TEST_F(VibrationTest,
                       CancelVibrationFromSubFrameWhenMainFrameIsReloaded) {
  ASSERT_FALSE(g_cancelled);

  ASSERT_TRUE(NavigateToURL(shell(), GetTestUrl(".", "page_with_iframe.html")));
  RenderFrameHost* iframe = ChildFrameAt(GetMainFrame(), 0);
  iframe->GetInterfaceRegistry()->AddInterface(
      base::Bind(&FakeVibrationManager::Create));
  ASSERT_TRUE(Vibrate(1234, iframe));
  g_wait_vibrate_runner->Run();
  ASSERT_TRUE(NavigateToURL(shell(), GetTestUrl(".", "page_with_iframe.html")));
  g_wait_cancel_runner->Run();

  ASSERT_TRUE(g_cancelled);
}

IN_PROC_BROWSER_TEST_F(VibrationTest,
                       CancelVibrationFromSubFrameWhenSubFrameIsDestroyed) {
  ASSERT_FALSE(g_cancelled);

  ASSERT_TRUE(NavigateToURL(shell(), GetTestUrl(".", "page_with_iframe.html")));
  RenderFrameHost* iframe = ChildFrameAt(GetMainFrame(), 0);
  iframe->GetInterfaceRegistry()->AddInterface(
      base::Bind(&FakeVibrationManager::Create));
  ASSERT_TRUE(Vibrate(1234, iframe));
  g_wait_vibrate_runner->Run();
  ASSERT_TRUE(DestroyIframe());
  g_wait_cancel_runner->Run();

  ASSERT_TRUE(g_cancelled);
}

}  //  namespace

}  //  namespace content
