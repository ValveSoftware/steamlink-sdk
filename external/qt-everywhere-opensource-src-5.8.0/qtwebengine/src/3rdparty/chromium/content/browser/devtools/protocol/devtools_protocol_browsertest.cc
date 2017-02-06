// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <utility>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/gfx/codec/png_codec.h"

namespace content {

namespace {

const char kIdParam[] = "id";
const char kMethodParam[] = "method";
const char kParamsParam[] = "params";

class TestJavaScriptDialogManager : public JavaScriptDialogManager,
                                    public WebContentsDelegate {
 public:
  TestJavaScriptDialogManager() : handle_(false) {}
  ~TestJavaScriptDialogManager() override {}

  void Handle()
  {
    if (!callback_.is_null()) {
      callback_.Run(true, base::string16());
      callback_.Reset();
    } else {
      handle_ = true;
    }
  }

  // WebContentsDelegate
  JavaScriptDialogManager* GetJavaScriptDialogManager(
      WebContents* source) override {
    return this;
  }

  // JavaScriptDialogManager
  void RunJavaScriptDialog(WebContents* web_contents,
                           const GURL& origin_url,
                           JavaScriptMessageType javascript_message_type,
                           const base::string16& message_text,
                           const base::string16& default_prompt_text,
                           const DialogClosedCallback& callback,
                           bool* did_suppress_message) override {
    if (handle_) {
      handle_ = false;
      callback.Run(true, base::string16());
    } else {
      callback_ = callback;
    }
  };

  void RunBeforeUnloadDialog(WebContents* web_contents,
                             bool is_reload,
                             const DialogClosedCallback& callback) override {}

  bool HandleJavaScriptDialog(WebContents* web_contents,
                              bool accept,
                              const base::string16* prompt_override) override {
    return true;
  }

  void CancelActiveAndPendingDialogs(WebContents* web_contents) override {}

  void ResetDialogState(WebContents* web_contents) override {}

 private:
  DialogClosedCallback callback_;
  bool handle_;
  DISALLOW_COPY_AND_ASSIGN(TestJavaScriptDialogManager);
};

}

class DevToolsProtocolTest : public ContentBrowserTest,
                             public DevToolsAgentHostClient {
 public:
  DevToolsProtocolTest()
      : last_sent_id_(0),
        waiting_for_command_result_id_(0),
        in_dispatch_(false) {
  }

 protected:
  void SendCommand(const std::string& method,
                   std::unique_ptr<base::DictionaryValue> params) {
    SendCommand(method, std::move(params), true);
  }

  void SendCommand(const std::string& method,
                   std::unique_ptr<base::DictionaryValue> params,
                   bool wait) {
    in_dispatch_ = true;
    base::DictionaryValue command;
    command.SetInteger(kIdParam, ++last_sent_id_);
    command.SetString(kMethodParam, method);
    if (params)
      command.Set(kParamsParam, params.release());

    std::string json_command;
    base::JSONWriter::Write(command, &json_command);
    agent_host_->DispatchProtocolMessage(this, json_command);
    // Some messages are dispatched synchronously.
    // Only run loop if we are not finished yet.
    if (in_dispatch_ && wait) {
      waiting_for_command_result_id_ = last_sent_id_;
      base::MessageLoop::current()->Run();
    }
    in_dispatch_ = false;
  }

  bool HasValue(const std::string& path) {
    base::Value* value = 0;
    return result_->Get(path, &value);
  }

  bool HasListItem(const std::string& path_to_list,
                   const std::string& name,
                   const std::string& value) {
    base::ListValue* list;
    if (!result_->GetList(path_to_list, &list))
      return false;

    for (size_t i = 0; i != list->GetSize(); i++) {
      base::DictionaryValue* item;
      if (!list->GetDictionary(i, &item))
        return false;
      std::string id;
      if (!item->GetString(name, &id))
        return false;
      if (id == value)
        return true;
    }
    return false;
  }

  void Attach() {
    agent_host_ = DevToolsAgentHost::GetOrCreateFor(shell()->web_contents());
    agent_host_->AttachClient(this);
  }

  void TearDownOnMainThread() override {
    if (agent_host_) {
      agent_host_->DetachClient(this);
      agent_host_ = nullptr;
    }
  }

  void WaitForNotification(const std::string& notification) {
    waiting_for_notification_ = notification;
    RunMessageLoop();
  }

  std::unique_ptr<base::DictionaryValue> result_;
  scoped_refptr<DevToolsAgentHost> agent_host_;
  int last_sent_id_;
  std::vector<int> result_ids_;
  std::vector<std::string> notifications_;

 private:
  void DispatchProtocolMessage(DevToolsAgentHost* agent_host,
                               const std::string& message) override {
    std::unique_ptr<base::DictionaryValue> root(
        static_cast<base::DictionaryValue*>(
            base::JSONReader::Read(message).release()));
    int id;
    if (root->GetInteger("id", &id)) {
      result_ids_.push_back(id);
      base::DictionaryValue* result;
      ASSERT_TRUE(root->GetDictionary("result", &result));
      result_.reset(result->DeepCopy());
      in_dispatch_ = false;
      if (id && id == waiting_for_command_result_id_) {
        waiting_for_command_result_id_ = 0;
        base::MessageLoop::current()->QuitNow();
      }
    } else {
      std::string notification;
      EXPECT_TRUE(root->GetString("method", &notification));
      notifications_.push_back(notification);
      if (waiting_for_notification_ == notification) {
        waiting_for_notification_ = std::string();
        base::MessageLoop::current()->QuitNow();
      }
    }
  }

  void AgentHostClosed(DevToolsAgentHost* agent_host, bool replaced) override {
    DCHECK(false);
  }

  std::string waiting_for_notification_;
  int waiting_for_command_result_id_;
  bool in_dispatch_;
};

class SyntheticKeyEventTest : public DevToolsProtocolTest {
 protected:
  void SendKeyEvent(const std::string& type,
                    int modifier,
                    int windowsKeyCode,
                    int nativeKeyCode) {
    std::unique_ptr<base::DictionaryValue> params(new base::DictionaryValue());
    params->SetString("type", type);
    params->SetInteger("modifiers", modifier);
    params->SetInteger("windowsVirtualKeyCode", windowsKeyCode);
    params->SetInteger("nativeVirtualKeyCode", nativeKeyCode);
    SendCommand("Input.dispatchKeyEvent", std::move(params));
  }
};

IN_PROC_BROWSER_TEST_F(SyntheticKeyEventTest, KeyEventSynthesizeKeyIdentifier) {
  NavigateToURLBlockUntilNavigationsComplete(shell(), GURL("about:blank"), 1);
  Attach();
  ASSERT_TRUE(content::ExecuteScript(
      shell()->web_contents()->GetRenderViewHost(),
      "function handleKeyEvent(event) {"
        "domAutomationController.setAutomationId(0);"
        "domAutomationController.send(event.keyIdentifier);"
      "}"
      "document.body.addEventListener('keydown', handleKeyEvent);"
      "document.body.addEventListener('keyup', handleKeyEvent);"));

  DOMMessageQueue dom_message_queue;

  // Send enter (keycode 13).
  SendKeyEvent("rawKeyDown", 0, 13, 13);
  SendKeyEvent("keyUp", 0, 13, 13);

  std::string key_identifier;
  ASSERT_TRUE(dom_message_queue.WaitForMessage(&key_identifier));
  EXPECT_EQ("\"Enter\"", key_identifier);
  ASSERT_TRUE(dom_message_queue.WaitForMessage(&key_identifier));
  EXPECT_EQ("\"Enter\"", key_identifier);

  // Send escape (keycode 27).
  SendKeyEvent("rawKeyDown", 0, 27, 27);
  SendKeyEvent("keyUp", 0, 27, 27);

  ASSERT_TRUE(dom_message_queue.WaitForMessage(&key_identifier));
  EXPECT_EQ("\"U+001B\"", key_identifier);
  ASSERT_TRUE(dom_message_queue.WaitForMessage(&key_identifier));
  EXPECT_EQ("\"U+001B\"", key_identifier);
}

class CaptureScreenshotTest : public DevToolsProtocolTest {
 private:
#if !defined(OS_ANDROID)
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kEnablePixelOutputInTests);
  }
#endif
};

// Does not link on Android
#if defined(OS_ANDROID)
#define MAYBE_CaptureScreenshot DISABLED_CaptureScreenshot
#elif defined(OS_MACOSX)  // Fails on 10.9. http://crbug.com/430620
#define MAYBE_CaptureScreenshot DISABLED_CaptureScreenshot
#elif defined(MEMORY_SANITIZER)
// Also fails under MSAN. http://crbug.com/423583
#define MAYBE_CaptureScreenshot DISABLED_CaptureScreenshot
#else
#define MAYBE_CaptureScreenshot CaptureScreenshot
#endif
IN_PROC_BROWSER_TEST_F(CaptureScreenshotTest, MAYBE_CaptureScreenshot) {
  shell()->LoadURL(GURL("about:blank"));
  Attach();
  EXPECT_TRUE(content::ExecuteScript(
      shell()->web_contents()->GetRenderViewHost(),
      "document.body.style.background = '#123456'"));
  SendCommand("Page.captureScreenshot", nullptr);
  std::string base64;
  EXPECT_TRUE(result_->GetString("data", &base64));
  std::string png;
  EXPECT_TRUE(base::Base64Decode(base64, &png));
  SkBitmap bitmap;
  gfx::PNGCodec::Decode(reinterpret_cast<const unsigned char*>(png.data()),
                        png.size(), &bitmap);
  SkColor color(bitmap.getColor(0, 0));
  EXPECT_TRUE(std::abs(0x12-(int)SkColorGetR(color)) <= 1);
  EXPECT_TRUE(std::abs(0x34-(int)SkColorGetG(color)) <= 1);
  EXPECT_TRUE(std::abs(0x56-(int)SkColorGetB(color)) <= 1);
}

#if defined(OS_ANDROID)
// Disabled, see http://crbug.com/469947.
IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, DISABLED_SynthesizePinchGesture) {
  GURL test_url = GetTestUrl("devtools", "synthetic_gesture_tests.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 1);
  Attach();

  int old_width;
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
      shell(), "domAutomationController.send(window.innerWidth)", &old_width));

  int old_height;
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
      shell(), "domAutomationController.send(window.innerHeight)",
      &old_height));

  std::unique_ptr<base::DictionaryValue> params(new base::DictionaryValue());
  params->SetInteger("x", old_width / 2);
  params->SetInteger("y", old_height / 2);
  params->SetDouble("scaleFactor", 2.0);
  SendCommand("Input.synthesizePinchGesture", std::move(params));

  int new_width;
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
      shell(), "domAutomationController.send(window.innerWidth)", &new_width));
  ASSERT_DOUBLE_EQ(2.0, static_cast<double>(old_width) / new_width);

  int new_height;
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
      shell(), "domAutomationController.send(window.innerHeight)",
      &new_height));
  ASSERT_DOUBLE_EQ(2.0, static_cast<double>(old_height) / new_height);
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, DISABLED_SynthesizeScrollGesture) {
  GURL test_url = GetTestUrl("devtools", "synthetic_gesture_tests.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 1);
  Attach();

  int scroll_top;
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
      shell(), "domAutomationController.send(document.body.scrollTop)",
      &scroll_top));
  ASSERT_EQ(0, scroll_top);

  std::unique_ptr<base::DictionaryValue> params(new base::DictionaryValue());
  params->SetInteger("x", 0);
  params->SetInteger("y", 0);
  params->SetInteger("xDistance", 0);
  params->SetInteger("yDistance", -100);
  SendCommand("Input.synthesizeScrollGesture", std::move(params));

  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
      shell(), "domAutomationController.send(document.body.scrollTop)",
      &scroll_top));
  ASSERT_EQ(100, scroll_top);
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, DISABLED_SynthesizeTapGesture) {
  GURL test_url = GetTestUrl("devtools", "synthetic_gesture_tests.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 1);
  Attach();

  int scroll_top;
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
      shell(), "domAutomationController.send(document.body.scrollTop)",
      &scroll_top));
  ASSERT_EQ(0, scroll_top);

  std::unique_ptr<base::DictionaryValue> params(new base::DictionaryValue());
  params->SetInteger("x", 16);
  params->SetInteger("y", 16);
  params->SetString("gestureSourceType", "touch");
  SendCommand("Input.synthesizeTapGesture", std::move(params));

  // The link that we just tapped should take us to the bottom of the page. The
  // new value of |document.body.scrollTop| will depend on the screen dimensions
  // of the device that we're testing on, but in any case it should be greater
  // than 0.
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
      shell(), "domAutomationController.send(document.body.scrollTop)",
      &scroll_top));
  ASSERT_GT(scroll_top, 0);
}
#endif  // defined(OS_ANDROID)

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, NavigationPreservesMessages) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_url = embedded_test_server()->GetURL("/devtools/navigation.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 1);
  Attach();
  SendCommand("Page.enable", nullptr, false);

  std::unique_ptr<base::DictionaryValue> params(new base::DictionaryValue());
  test_url = GetTestUrl("devtools", "navigation.html");
  params->SetString("url", test_url.spec());
  TestNavigationObserver navigation_observer(shell()->web_contents());
  SendCommand("Page.navigate", std::move(params), true);
  navigation_observer.Wait();

  bool enough_results = result_ids_.size() >= 2u;
  EXPECT_TRUE(enough_results);
  if (enough_results) {
    EXPECT_EQ(1, result_ids_[0]);  // Page.enable
    EXPECT_EQ(2, result_ids_[1]);  // Page.navigate
  }

  enough_results = notifications_.size() >= 1u;
  EXPECT_TRUE(enough_results);
  bool found_frame_notification = false;
  for (const std::string& notification : notifications_) {
    if (notification == "Page.frameStartedLoading")
      found_frame_notification = true;
  }
  EXPECT_TRUE(found_frame_notification);
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, CrossSiteNoDetach) {
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(embedded_test_server()->Start());
  content::SetupCrossSiteRedirector(embedded_test_server());

  GURL test_url1 = embedded_test_server()->GetURL(
      "A.com", "/devtools/navigation.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url1, 1);
  Attach();

  GURL test_url2 = embedded_test_server()->GetURL(
      "B.com", "/devtools/navigation.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url2, 1);

  EXPECT_EQ(0u, notifications_.size());
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, ReconnectPreservesState) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_url = embedded_test_server()->GetURL("/devtools/navigation.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 1);

  Shell* second = CreateBrowser();
  NavigateToURLBlockUntilNavigationsComplete(second, test_url, 1);

  Attach();
  SendCommand("Runtime.enable", nullptr);

  agent_host_->DisconnectWebContents();
  agent_host_->ConnectWebContents(second->web_contents());
  WaitForNotification("Runtime.executionContextsCleared");
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, CrossSitePauseInBeforeUnload) {
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(embedded_test_server()->Start());
  content::SetupCrossSiteRedirector(embedded_test_server());

  NavigateToURLBlockUntilNavigationsComplete(shell(),
      embedded_test_server()->GetURL("A.com", "/devtools/navigation.html"), 1);
  Attach();
  SendCommand("Debugger.enable", nullptr);

  ASSERT_TRUE(content::ExecuteScript(
      shell(),
      "window.onbeforeunload = function() { debugger; return null; }"));

  shell()->LoadURL(
      embedded_test_server()->GetURL("B.com", "/devtools/navigation.html"));
  WaitForNotification("Debugger.paused");
  TestNavigationObserver observer(shell()->web_contents(), 1);
  SendCommand("Debugger.resume", nullptr);
  observer.Wait();
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, InspectDuringFrameSwap) {
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(embedded_test_server()->Start());
  content::SetupCrossSiteRedirector(embedded_test_server());

  GURL test_url1 =
      embedded_test_server()->GetURL("A.com", "/devtools/navigation.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url1, 1);

  ShellAddedObserver new_shell_observer;
  EXPECT_TRUE(ExecuteScript(shell(), "window.open('about:blank','foo');"));
  Shell* new_shell = new_shell_observer.GetShell();
  EXPECT_TRUE(new_shell->web_contents()->HasOpener());

  agent_host_ = DevToolsAgentHost::GetOrCreateFor(new_shell->web_contents());
  agent_host_->AttachClient(this);

  GURL test_url2 =
      embedded_test_server()->GetURL("B.com", "/devtools/navigation.html");

  // After this navigation, if the bug exists, the process will crash.
  NavigateToURLBlockUntilNavigationsComplete(new_shell, test_url2, 1);

  // Ensure that the A.com process is still alive by executing a script in the
  // original tab.
  //
  // TODO(alexmos, nasko):  A better way to do this is to navigate the original
  // tab to another site, watch for process exit, and check whether there was a
  // crash. However, currently there's no way to wait for process exit
  // regardless of whether it's a crash or not.  RenderProcessHostWatcher
  // should be fixed to support waiting on both WATCH_FOR_PROCESS_EXIT and
  // WATCH_FOR_HOST_DESTRUCTION, and then used here.
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(shell(),
                                          "window.domAutomationController.send("
                                          "    !!window.open('', 'foo'));",
                                          &success));
  EXPECT_TRUE(success);

  GURL test_url3 =
      embedded_test_server()->GetURL("A.com", "/devtools/navigation.html");

  // After this navigation, if the bug exists, the process will crash.
  NavigateToURLBlockUntilNavigationsComplete(new_shell, test_url3, 1);

  // Ensure that the A.com process is still alive by executing a script in the
  // original tab.
  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(shell(),
                                          "window.domAutomationController.send("
                                          "    !!window.open('', 'foo'));",
                                          &success));
  EXPECT_TRUE(success);
}

// CrashTab() works differently on Windows, leading to RFH removal before
// RenderProcessGone is called. TODO(dgozman): figure out the problem.
#if defined(OS_WIN)
#define MAYBE_DoubleCrash DISABLED_DoubleCrash
#else
#define MAYBE_DoubleCrash DoubleCrash
#endif
IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, MAYBE_DoubleCrash) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_url = embedded_test_server()->GetURL("/devtools/navigation.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), GURL("about:blank"), 1);
  Attach();
  SendCommand("ServiceWorker.enable", nullptr);
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 1);
  CrashTab(shell()->web_contents());
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 1);
  CrashTab(shell()->web_contents());
  NavigateToURLBlockUntilNavigationsComplete(shell(), GURL("about:blank"), 1);
  // Should not crash at this point.
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, ReloadBlankPage) {
  Shell* window =  Shell::CreateNewWindow(
      shell()->web_contents()->GetBrowserContext(),
      GURL("javascript:x=1"),
      nullptr,
      gfx::Size());
  WaitForLoadStop(window->web_contents());
  Attach();
  SendCommand("Page.reload", nullptr, false);
  // Should not crash at this point.
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, EvaluateInBlankPage) {
  NavigateToURLBlockUntilNavigationsComplete(shell(), GURL("about:blank"), 1);
  Attach();
  std::unique_ptr<base::DictionaryValue> params(new base::DictionaryValue());
  params->SetString("expression", "window");
  SendCommand("Runtime.evaluate", std::move(params), true);
  bool wasThrown = true;
  EXPECT_TRUE(result_->GetBoolean("wasThrown", &wasThrown));
  EXPECT_FALSE(wasThrown);
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest,
    EvaluateInBlankPageAfterNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_url = embedded_test_server()->GetURL("/devtools/navigation.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 1);
  Attach();
  NavigateToURLBlockUntilNavigationsComplete(shell(), GURL("about:blank"), 1);
  std::unique_ptr<base::DictionaryValue> params(new base::DictionaryValue());
  params->SetString("expression", "window");
  SendCommand("Runtime.evaluate", std::move(params), true);
  bool wasThrown = true;
  EXPECT_TRUE(result_->GetBoolean("wasThrown", &wasThrown));
  EXPECT_FALSE(wasThrown);
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, JavaScriptDialogNotifications) {
  NavigateToURLBlockUntilNavigationsComplete(shell(), GURL("about:blank"), 1);
  Attach();
  TestJavaScriptDialogManager dialog_manager;
  shell()->web_contents()->SetDelegate(&dialog_manager);
  SendCommand("Page.enable", nullptr, true);
  std::unique_ptr<base::DictionaryValue> params(new base::DictionaryValue());
  params->SetString("expression", "alert('alert')");
  SendCommand("Runtime.evaluate", std::move(params), false);
  WaitForNotification("Page.javascriptDialogOpening");
  dialog_manager.Handle();
}

}  // namespace content
