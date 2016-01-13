// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/common/shell_switches.h"
#include "content/test/net/url_request_mock_http_job.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/url_request/url_request.h"
#include "ui/gfx/rect.h"

#if defined(OS_WIN)
#include "base/win/registry.h"
#endif

// TODO(jschuh): Finish plugins on Win64. crbug.com/180861
#if defined(OS_WIN) && defined(ARCH_CPU_X86_64)
#define MAYBE(x) DISABLED_##x
#else
#define MAYBE(x) x
#endif

using base::ASCIIToUTF16;

namespace content {
namespace {

void SetUrlRequestMock(const base::FilePath& path) {
  URLRequestMockHTTPJob::AddUrlHandler(path);
}

}

class PluginTest : public ContentBrowserTest {
 protected:
  PluginTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    // Some NPAPI tests schedule garbage collection to force object tear-down.
    command_line->AppendSwitchASCII(switches::kJavaScriptFlags, "--expose_gc");

#if defined(OS_WIN)
    const testing::TestInfo* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    if (strcmp(test_info->name(), "MediaPlayerNew") == 0) {
      // The installer adds our process names to the registry key below.  Since
      // the installer might not have run on this machine, add it manually.
      base::win::RegKey regkey;
      if (regkey.Open(HKEY_LOCAL_MACHINE,
                      L"Software\\Microsoft\\MediaPlayer\\ShimInclusionList",
                      KEY_WRITE) == ERROR_SUCCESS) {
        regkey.CreateKey(L"BROWSER_TESTS.EXE", KEY_READ);
      }
    }
#elif defined(OS_MACOSX)
    base::FilePath plugin_dir;
    PathService::Get(base::DIR_MODULE, &plugin_dir);
    plugin_dir = plugin_dir.AppendASCII("plugins");
    // The plugins directory isn't read by default on the Mac, so it needs to be
    // explicitly registered.
    command_line->AppendSwitchPath(switches::kExtraPluginDir, plugin_dir);
#endif
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    base::FilePath path = GetTestFilePath("", "");
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE, base::Bind(&SetUrlRequestMock, path));
  }

  static void LoadAndWaitInWindow(Shell* window, const GURL& url) {
    base::string16 expected_title(ASCIIToUTF16("OK"));
    TitleWatcher title_watcher(window->web_contents(), expected_title);
    title_watcher.AlsoWaitForTitle(ASCIIToUTF16("FAIL"));
    title_watcher.AlsoWaitForTitle(ASCIIToUTF16("plugin_not_found"));
    NavigateToURL(window, url);
    base::string16 title = title_watcher.WaitAndGetTitle();
    if (title == ASCIIToUTF16("plugin_not_found")) {
      const testing::TestInfo* const test_info =
          testing::UnitTest::GetInstance()->current_test_info();
      VLOG(0) << "PluginTest." << test_info->name()
              << " not running because plugin not installed.";
    } else {
      EXPECT_EQ(expected_title, title);
    }
  }

  void LoadAndWait(const GURL& url) {
    LoadAndWaitInWindow(shell(), url);
  }

  GURL GetURL(const char* filename) {
    return GetTestUrl("npapi", filename);
  }

  void NavigateAway() {
    GURL url = GetTestUrl("", "simple_page.html");
    LoadAndWait(url);
  }

  void TestPlugin(const char* filename) {
    base::FilePath path = GetTestFilePath("plugin", filename);
    if (!base::PathExists(path)) {
      const testing::TestInfo* const test_info =
          testing::UnitTest::GetInstance()->current_test_info();
      VLOG(0) << "PluginTest." << test_info->name()
              << " not running because test data wasn't found.";
      return;
    }

    GURL url = GetTestUrl("plugin", filename);
    LoadAndWait(url);
  }
};

// Make sure that navigating away from a plugin referenced by JS doesn't
// crash.
IN_PROC_BROWSER_TEST_F(PluginTest, UnloadNoCrash) {
  LoadAndWait(GetURL("layout_test_plugin.html"));
  NavigateAway();
}

// Tests if a plugin executing a self deleting script using NPN_GetURL
// works without crashing or hanging
// Flaky: http://crbug.com/59327
IN_PROC_BROWSER_TEST_F(PluginTest, MAYBE(SelfDeletePluginGetUrl)) {
  LoadAndWait(GetURL("self_delete_plugin_geturl.html"));
}

// Tests if a plugin executing a self deleting script using Invoke
// works without crashing or hanging
// Flaky. See http://crbug.com/30702
IN_PROC_BROWSER_TEST_F(PluginTest, MAYBE(SelfDeletePluginInvoke)) {
  LoadAndWait(GetURL("self_delete_plugin_invoke.html"));
}

IN_PROC_BROWSER_TEST_F(PluginTest, MAYBE(NPObjectReleasedOnDestruction)) {
  NavigateToURL(shell(), GetURL("npobject_released_on_destruction.html"));
  NavigateAway();
}

// Test that a dialog is properly created when a plugin throws an
// exception.  Should be run for in and out of process plugins, but
// the more interesting case is out of process, where we must route
// the exception to the correct renderer.
IN_PROC_BROWSER_TEST_F(PluginTest, MAYBE(NPObjectSetException)) {
  LoadAndWait(GetURL("npobject_set_exception.html"));
}

#if defined(OS_WIN)
// Tests if a plugin executing a self deleting script in the context of
// a synchronous mouseup works correctly.
// This was never ported to Mac. The only thing remaining is to make
// SimulateMouseClick get to Mac plugins, currently it doesn't work.
IN_PROC_BROWSER_TEST_F(PluginTest,
                       MAYBE(SelfDeletePluginInvokeInSynchronousMouseUp)) {
  NavigateToURL(shell(), GetURL("execute_script_delete_in_mouse_up.html"));

  base::string16 expected_title(ASCIIToUTF16("OK"));
  TitleWatcher title_watcher(shell()->web_contents(), expected_title);
  title_watcher.AlsoWaitForTitle(ASCIIToUTF16("FAIL"));
  SimulateMouseClick(shell()->web_contents(), 0,
      blink::WebMouseEvent::ButtonLeft);
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}
#endif

// Flaky, http://crbug.com/302274.
#if defined(OS_MACOSX)
#define MAYBE_GetURLRequest404Response DISABLED_GetURLRequest404Response
#else
#define MAYBE_GetURLRequest404Response MAYBE(GetURLRequest404Response)
#endif

IN_PROC_BROWSER_TEST_F(PluginTest, MAYBE_GetURLRequest404Response) {
  GURL url(URLRequestMockHTTPJob::GetMockUrl(
      base::FilePath().AppendASCII("npapi").
                       AppendASCII("plugin_url_request_404.html")));
  LoadAndWait(url);
}

// Tests if a plugin executing a self deleting script using Invoke with
// a modal dialog showing works without crashing or hanging
// Disabled, flakily exceeds timeout, http://crbug.com/46257.
IN_PROC_BROWSER_TEST_F(PluginTest, MAYBE(SelfDeletePluginInvokeAlert)) {
  // Navigate asynchronously because if we waitd until it completes, there's a
  // race condition where the alert can come up before we start watching for it.
  shell()->LoadURL(GetURL("self_delete_plugin_invoke_alert.html"));

  base::string16 expected_title(ASCIIToUTF16("OK"));
  TitleWatcher title_watcher(shell()->web_contents(), expected_title);
  title_watcher.AlsoWaitForTitle(ASCIIToUTF16("FAIL"));

  WaitForAppModalDialog(shell());

  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

// Test passing arguments to a plugin.
// crbug.com/306318
#if !defined(OS_LINUX)
IN_PROC_BROWSER_TEST_F(PluginTest, MAYBE(Arguments)) {
  LoadAndWait(GetURL("arguments.html"));
}
#endif

// Test invoking many plugins within a single page.
IN_PROC_BROWSER_TEST_F(PluginTest, MAYBE(ManyPlugins)) {
  LoadAndWait(GetURL("many_plugins.html"));
}

// Test various calls to GetURL from a plugin.
IN_PROC_BROWSER_TEST_F(PluginTest, MAYBE(GetURL)) {
  LoadAndWait(GetURL("geturl.html"));
}

// Test various calls to GetURL for javascript URLs with
// non NULL targets from a plugin.
IN_PROC_BROWSER_TEST_F(PluginTest, MAYBE(GetJavaScriptURL)) {
  LoadAndWait(GetURL("get_javascript_url.html"));
}

// Test that calling GetURL with a javascript URL and target=_self
// works properly when the plugin is embedded in a subframe.
IN_PROC_BROWSER_TEST_F(PluginTest, MAYBE(GetJavaScriptURL2)) {
  LoadAndWait(GetURL("get_javascript_url2.html"));
}

// Test is flaky on linux/cros/win builders.  http://crbug.com/71904
IN_PROC_BROWSER_TEST_F(PluginTest, GetURLRedirectNotification) {
  LoadAndWait(GetURL("geturl_redirect_notify.html"));
}

// Tests that identity is preserved for NPObjects passed from a plugin
// into JavaScript.
IN_PROC_BROWSER_TEST_F(PluginTest, MAYBE(NPObjectIdentity)) {
  LoadAndWait(GetURL("npobject_identity.html"));
}

// Tests that if an NPObject is proxies back to its original process, the
// original pointer is returned and not a proxy.  If this fails the plugin
// will crash.
IN_PROC_BROWSER_TEST_F(PluginTest, MAYBE(NPObjectProxy)) {
  LoadAndWait(GetURL("npobject_proxy.html"));
}

#if defined(OS_WIN) || defined(OS_MACOSX)
// Tests if a plugin executing a self deleting script in the context of
// a synchronous paint event works correctly
// http://crbug.com/44960
IN_PROC_BROWSER_TEST_F(PluginTest,
                       MAYBE(SelfDeletePluginInvokeInSynchronousPaint)) {
  LoadAndWait(GetURL("execute_script_delete_in_paint.html"));
}
#endif

// Tests that if a plugin executes a self resizing script in the context of a
// synchronous paint, the plugin doesn't use deallocated memory.
// http://crbug.com/139462
IN_PROC_BROWSER_TEST_F(PluginTest, MAYBE(ResizeDuringPaint)) {
  LoadAndWait(GetURL("resize_during_paint.html"));
}

IN_PROC_BROWSER_TEST_F(PluginTest, MAYBE(SelfDeletePluginInNewStream)) {
  LoadAndWait(GetURL("self_delete_plugin_stream.html"));
}

// On Mac this test asserts in plugin_host: http://crbug.com/95558
// On all platforms it flakes in ~URLRequestContext: http://crbug.com/310336
#if !defined(NDEBUG)
IN_PROC_BROWSER_TEST_F(PluginTest, DISABLED_DeletePluginInDeallocate) {
  LoadAndWait(GetURL("plugin_delete_in_deallocate.html"));
}
#endif

#if defined(OS_WIN)

IN_PROC_BROWSER_TEST_F(PluginTest, MAYBE(VerifyPluginWindowRect)) {
  LoadAndWait(GetURL("verify_plugin_window_rect.html"));
}

// Tests that creating a new instance of a plugin while another one is handling
// a paint message doesn't cause deadlock.
IN_PROC_BROWSER_TEST_F(PluginTest, MAYBE(CreateInstanceInPaint)) {
  LoadAndWait(GetURL("create_instance_in_paint.html"));
}

// Tests that putting up an alert in response to a paint doesn't deadlock.
IN_PROC_BROWSER_TEST_F(PluginTest, DISABLED_AlertInWindowMessage) {
  NavigateToURL(shell(), GetURL("alert_in_window_message.html"));

  WaitForAppModalDialog(shell());
  WaitForAppModalDialog(shell());
}

IN_PROC_BROWSER_TEST_F(PluginTest, MAYBE(VerifyNPObjectLifetimeTest)) {
  LoadAndWait(GetURL("npobject_lifetime_test.html"));
}

// Tests that we don't crash or assert if NPP_New fails
IN_PROC_BROWSER_TEST_F(PluginTest, MAYBE(NewFails)) {
  LoadAndWait(GetURL("new_fails.html"));
}

IN_PROC_BROWSER_TEST_F(PluginTest, MAYBE(SelfDeletePluginInNPNEvaluate)) {
  LoadAndWait(GetURL("execute_script_delete_in_npn_evaluate.html"));
}

IN_PROC_BROWSER_TEST_F(PluginTest,
                       MAYBE(SelfDeleteCreatePluginInNPNEvaluate)) {
  LoadAndWait(GetURL("npn_plugin_delete_create_in_evaluate.html"));
}

#endif  // OS_WIN

// If this flakes, reopen http://crbug.com/17645
// As of 6 July 2011, this test is flaky on Windows (perhaps due to timing out).
#if !defined(OS_MACOSX) && !defined(OS_LINUX)
// Disabled on Mac because the plugin side isn't implemented yet, see
// "TODO(port)" in plugin_javascript_open_popup.cc.
// Disabled on Linux because we don't support NPAPI any more.
IN_PROC_BROWSER_TEST_F(PluginTest, MAYBE(OpenPopupWindowWithPlugin)) {
  LoadAndWait(GetURL("get_javascript_open_popup_with_plugin.html"));
}
#endif

// Test checking the privacy mode is off.
IN_PROC_BROWSER_TEST_F(PluginTest, MAYBE(PrivateDisabled)) {
  LoadAndWait(GetURL("private.html"));
}

IN_PROC_BROWSER_TEST_F(PluginTest, MAYBE(ScheduleTimer)) {
  LoadAndWait(GetURL("schedule_timer.html"));
}

IN_PROC_BROWSER_TEST_F(PluginTest, MAYBE(PluginThreadAsyncCall)) {
  LoadAndWait(GetURL("plugin_thread_async_call.html"));
}

IN_PROC_BROWSER_TEST_F(PluginTest, PluginSingleRangeRequest) {
  LoadAndWait(GetURL("plugin_single_range_request.html"));
}

// Test checking the privacy mode is on.
// If this flakes on Linux, use http://crbug.com/104380
IN_PROC_BROWSER_TEST_F(PluginTest, MAYBE(PrivateEnabled)) {
  GURL url = GetURL("private.html");
  url = GURL(url.spec() + "?private");
  LoadAndWaitInWindow(CreateOffTheRecordBrowser(), url);
}

#if defined(OS_WIN) || defined(OS_MACOSX)
// Test a browser hang due to special case of multiple
// plugin instances indulged in sync calls across renderer.
IN_PROC_BROWSER_TEST_F(PluginTest, MAYBE(MultipleInstancesSyncCalls)) {
  LoadAndWait(GetURL("multiple_instances_sync_calls.html"));
}
#endif

IN_PROC_BROWSER_TEST_F(PluginTest, MAYBE(GetURLRequestFailWrite)) {
  GURL url(URLRequestMockHTTPJob::GetMockUrl(
      base::FilePath().AppendASCII("npapi").
                       AppendASCII("plugin_url_request_fail_write.html")));
  LoadAndWait(url);
}

#if defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(PluginTest, MAYBE(EnsureScriptingWorksInDestroy)) {
  LoadAndWait(GetURL("ensure_scripting_works_in_destroy.html"));
}

// This test uses a Windows Event to signal to the plugin that it should crash
// on NP_Initialize.
IN_PROC_BROWSER_TEST_F(PluginTest, MAYBE(NoHangIfInitCrashes)) {
  HANDLE crash_event = CreateEvent(NULL, TRUE, FALSE, L"TestPluginCrashOnInit");
  SetEvent(crash_event);
  LoadAndWait(GetURL("no_hang_if_init_crashes.html"));
  CloseHandle(crash_event);
}
#endif

// If this flakes on Mac, use http://crbug.com/111508
IN_PROC_BROWSER_TEST_F(PluginTest, MAYBE(PluginReferrerTest)) {
  GURL url(URLRequestMockHTTPJob::GetMockUrl(
      base::FilePath().AppendASCII("npapi").
                       AppendASCII("plugin_url_request_referrer_test.html")));
  LoadAndWait(url);
}

#if defined(OS_MACOSX)
// Test is flaky, see http://crbug.com/134515.
IN_PROC_BROWSER_TEST_F(PluginTest, DISABLED_PluginConvertPointTest) {
  gfx::Rect bounds(50, 50, 400, 400);
  SetWindowBounds(shell()->window(), bounds);

  NavigateToURL(shell(), GetURL("convert_point.html"));

  base::string16 expected_title(ASCIIToUTF16("OK"));
  TitleWatcher title_watcher(shell()->web_contents(), expected_title);
  title_watcher.AlsoWaitForTitle(ASCIIToUTF16("FAIL"));
  // TODO(stuartmorgan): When the automation system supports sending clicks,
  // change the test to trigger on mouse-down rather than window focus.

  // TODO: is this code still needed? It was here when it used to run in
  // browser_tests.
  //static_cast<WebContentsDelegate*>(shell())->
  //    ActivateContents(shell()->web_contents());
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}
#endif

IN_PROC_BROWSER_TEST_F(PluginTest, MAYBE(Flash)) {
  TestPlugin("flash.html");
}

#if defined(OS_WIN)
// Windows only test
IN_PROC_BROWSER_TEST_F(PluginTest, DISABLED_FlashSecurity) {
  TestPlugin("flash.html");
}
#endif  // defined(OS_WIN)

#if defined(OS_WIN)
// TODO(port) Port the following tests to platforms that have the required
// plugins.
// Flaky: http://crbug.com/55915
IN_PROC_BROWSER_TEST_F(PluginTest, MAYBE(Quicktime)) {
  TestPlugin("quicktime.html");
}

// Disabled - http://crbug.com/44662
IN_PROC_BROWSER_TEST_F(PluginTest, MAYBE(MediaPlayerNew)) {
  TestPlugin("wmp_new.html");
}

// Disabled - http://crbug.com/44673
IN_PROC_BROWSER_TEST_F(PluginTest, MAYBE(Real)) {
  TestPlugin("real.html");
}

// http://crbug.com/320041
#if (defined(OS_WIN) && defined(ARCH_CPU_X86_64)) || \
    (defined(GOOGLE_CHROME_BUILD) && defined(OS_WIN))
#define MAYBE_FlashOctetStream DISABLED_FlashOctetStream
#else
#define MAYBE_FlashOctetStream FlashOctetStream
#endif
IN_PROC_BROWSER_TEST_F(PluginTest, MAYBE_FlashOctetStream) {
  TestPlugin("flash-octet-stream.html");
}

#if defined(OS_WIN)
// http://crbug.com/53926
IN_PROC_BROWSER_TEST_F(PluginTest, DISABLED_FlashLayoutWhilePainting) {
#else
IN_PROC_BROWSER_TEST_F(PluginTest, FlashLayoutWhilePainting) {
#endif
  TestPlugin("flash-layout-while-painting.html");
}

// http://crbug.com/8690
IN_PROC_BROWSER_TEST_F(PluginTest, DISABLED_Java) {
  TestPlugin("Java.html");
}

IN_PROC_BROWSER_TEST_F(PluginTest, MAYBE(Silverlight)) {
  TestPlugin("silverlight.html");
}
#endif  // defined(OS_WIN)

class TestResourceDispatcherHostDelegate
    : public ResourceDispatcherHostDelegate {
 public:
  TestResourceDispatcherHostDelegate() : found_cookie_(false) {}

  bool found_cookie() { return found_cookie_; }

  void WaitForPluginRequest() {
    if (found_cookie_)
      return;

    runner_ = new MessageLoopRunner;
    runner_->Run();
  }

 private:
  // ResourceDispatcherHostDelegate implementation:
  virtual void OnResponseStarted(
      net::URLRequest* request,
      ResourceContext* resource_context,
      ResourceResponse* response,
      IPC::Sender* sender) OVERRIDE {
    // The URL below comes from plugin_geturl_test.cc.
    if (!EndsWith(request->url().spec(),
                 "npapi/plugin_ref_target_page.html",
                 true)) {
      return;
    }
    net::HttpRequestHeaders headers;
    bool found_cookie = false;
    if (request->GetFullRequestHeaders(&headers) &&
        headers.ToString().find("Cookie: blah") != std::string::npos) {
      found_cookie = true;
    }
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&TestResourceDispatcherHostDelegate::GotCookie,
                   base::Unretained(this), found_cookie));
  }

  void GotCookie(bool found_cookie) {
    found_cookie_ = found_cookie;
    if (runner_)
      runner_->QuitClosure().Run();
  }

  scoped_refptr<MessageLoopRunner> runner_;
  bool found_cookie_;

  DISALLOW_COPY_AND_ASSIGN(TestResourceDispatcherHostDelegate);
};

// Ensure that cookies get sent with plugin requests.
IN_PROC_BROWSER_TEST_F(PluginTest, MAYBE(Cookies)) {
  // Create a new browser just to ensure that the plugin process' child_id is
  // not equal to its type (PROCESS_TYPE_PLUGIN), as that was the error which
  // caused this bug.
  NavigateToURL(CreateBrowser(), GURL("about:blank"));

  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  GURL url(embedded_test_server()->GetURL("/npapi/cookies.html"));

  TestResourceDispatcherHostDelegate test_delegate;
  ResourceDispatcherHostDelegate* old_delegate =
      ResourceDispatcherHostImpl::Get()->delegate();
  ResourceDispatcherHostImpl::Get()->SetDelegate(&test_delegate);
  LoadAndWait(url);
  test_delegate.WaitForPluginRequest();
  ASSERT_TRUE(test_delegate.found_cookie());
  ResourceDispatcherHostImpl::Get()->SetDelegate(old_delegate);
}

}  // namespace content
