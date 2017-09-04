// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/browser/frame_host/interstitial_page_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/interstitial_page_delegate.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/layout.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/skia_util.h"

#define EXPECT_SIZE_EQ(expected, actual)               \
  do {                                                 \
    EXPECT_EQ((expected).width(), (actual).width());   \
    EXPECT_EQ((expected).height(), (actual).height()); \
  } while (false)

using testing::ElementsAre;

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

  void CancelDialogs(WebContents* web_contents,
                     bool suppress_callbacks,
                     bool reset_state) override {}

 private:
  DialogClosedCallback callback_;
  bool handle_;
  DISALLOW_COPY_AND_ASSIGN(TestJavaScriptDialogManager);
};

}

class DevToolsProtocolTest : public ContentBrowserTest,
                             public DevToolsAgentHostClient,
                             public WebContentsDelegate {
 public:
  DevToolsProtocolTest()
      : last_sent_id_(0),
        waiting_for_command_result_id_(0),
        in_dispatch_(false),
        last_shown_certificate_(nullptr),
        ok_cert_(nullptr),
        expired_cert_(nullptr) {}

  void SetUpOnMainThread() override {
    ok_cert_ =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
    expired_cert_ = net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                            "expired_cert.pem");
  }

 protected:
  // WebContentsDelegate method:
  bool DidAddMessageToConsole(WebContents* source,
                              int32_t level,
                              const base::string16& message,
                              int32_t line_no,
                              const base::string16& source_id) override {
    console_messages_.push_back(base::UTF16ToUTF8(message));
    return true;
  }

  void ShowCertificateViewerInDevTools(
      WebContents* web_contents,
      scoped_refptr<net::X509Certificate> certificate) override {
    last_shown_certificate_ = certificate;
  }

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
      base::RunLoop().Run();
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
    shell()->web_contents()->SetDelegate(this);
  }

  void TearDownOnMainThread() override {
    if (agent_host_) {
      agent_host_->DetachClient(this);
      agent_host_ = nullptr;
    }
  }

  std::unique_ptr<base::DictionaryValue> WaitForNotification(
      const std::string& notification) {
    return WaitForNotification(notification, false);
  }

  std::unique_ptr<base::DictionaryValue> WaitForNotification(
      const std::string& notification,
      bool allow_existing) {
    if (allow_existing) {
      for (size_t i = 0; i < notifications_.size(); i++) {
        if (notifications_[i] == notification) {
          std::unique_ptr<base::DictionaryValue> result =
              std::move(notification_params_[i]);
          notifications_.erase(notifications_.begin() + i);
          notification_params_.erase(notification_params_.begin() + i);
          return result;
        }
      }
    }

    waiting_for_notification_ = notification;
    RunMessageLoop();
    return std::move(waiting_for_notification_params_);
  }

  void ClearNotifications() {
    notifications_.clear();
    notification_params_.clear();
  }

  struct ExpectedNavigation {
    std::string url;
    bool is_in_main_frame;
    bool is_redirect;
    std::string navigation_response;
  };

  std::string RemovePort(const GURL& url) {
    GURL::Replacements remove_port;
    remove_port.ClearPort();
    return url.ReplaceComponents(remove_port).spec();
  }

  // Waits for the expected navigations to occur in any order. If an expected
  // navigation occurs, Page.processNavigation is called with the specified
  // navigation_response to either allow it to proceed or to cancel it.
  void ProcessNavigationsAnyOrder(
      std::vector<ExpectedNavigation> expected_navigations) {
    while (!expected_navigations.empty()) {
      std::unique_ptr<base::DictionaryValue> params =
          WaitForNotification("Page.navigationRequested");

      std::string url;
      ASSERT_TRUE(params->GetString("url", &url));

      // The url will typically have a random port which we want to remove.
      url = RemovePort(GURL(url));

      int navigation_id;
      ASSERT_TRUE(params->GetInteger("navigationId", &navigation_id));
      bool is_in_main_frame;
      ASSERT_TRUE(params->GetBoolean( "isInMainFrame", &is_in_main_frame));
      bool is_redirect;
      ASSERT_TRUE(params->GetBoolean("isRedirect", &is_redirect));

      bool navigation_was_expected;
      for (auto it = expected_navigations.begin();
           it != expected_navigations.end(); it++) {
        if (url != it->url || is_in_main_frame != it->is_in_main_frame ||
            is_redirect != it->is_redirect) {
          continue;
        }

        std::unique_ptr<base::DictionaryValue> process_params(
            new base::DictionaryValue());
        process_params->SetString("response", it->navigation_response);
        process_params->SetInteger("navigationId", navigation_id);
        SendCommand("Page.processNavigation", std::move(process_params), false);

        navigation_was_expected = true;
        expected_navigations.erase(it);
        break;
      }
      EXPECT_TRUE(navigation_was_expected)
          << "url = " << url << "is_in_main_frame = " << is_in_main_frame
          << "is_redirect = " << is_redirect;
    }
  }

  std::vector<std::string> GetAllFrameUrls() {
    std::vector<std::string> urls;
    for (RenderFrameHost* render_frame_host :
         shell()->web_contents()->GetAllFrames()) {
      urls.push_back(RemovePort(render_frame_host->GetLastCommittedURL()));
    }
    return urls;
  }

  const scoped_refptr<net::X509Certificate>& last_shown_certificate() {
    return last_shown_certificate_;
  }

  const scoped_refptr<net::X509Certificate>& ok_cert() { return ok_cert_; }

  const scoped_refptr<net::X509Certificate>& expired_cert() {
    return expired_cert_;
  }

  std::unique_ptr<base::DictionaryValue> result_;
  scoped_refptr<DevToolsAgentHost> agent_host_;
  int last_sent_id_;
  std::vector<int> result_ids_;
  std::vector<std::string> notifications_;
  std::vector<std::string> console_messages_;
  std::vector<std::unique_ptr<base::DictionaryValue>> notification_params_;

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
      base::DictionaryValue* params;
      if (root->GetDictionary("params", &params)) {
        notification_params_.push_back(params->CreateDeepCopy());
      } else {
        notification_params_.push_back(
            base::WrapUnique(new base::DictionaryValue()));
      }
      if (waiting_for_notification_ == notification) {
        waiting_for_notification_ = std::string();
        waiting_for_notification_params_ = base::WrapUnique(
            notification_params_[notification_params_.size() - 1]->DeepCopy());
        base::MessageLoop::current()->QuitNow();
      }
    }
  }

  void AgentHostClosed(DevToolsAgentHost* agent_host, bool replaced) override {
    DCHECK(false);
  }

  std::string waiting_for_notification_;
  std::unique_ptr<base::DictionaryValue> waiting_for_notification_params_;
  int waiting_for_command_result_id_;
  bool in_dispatch_;
  scoped_refptr<net::X509Certificate> last_shown_certificate_;
  scoped_refptr<net::X509Certificate> ok_cert_;
  scoped_refptr<net::X509Certificate> expired_cert_;
};

class TestInterstitialDelegate : public InterstitialPageDelegate {
 private:
  // InterstitialPageDelegate:
  std::string GetHTMLContents() override { return "<p>Interstitial</p>"; }
};

class SyntheticKeyEventTest : public DevToolsProtocolTest {
 protected:
  void SendKeyEvent(const std::string& type,
                    int modifier,
                    int windowsKeyCode,
                    int nativeKeyCode,
                    const std::string& key) {
    std::unique_ptr<base::DictionaryValue> params(new base::DictionaryValue());
    params->SetString("type", type);
    params->SetInteger("modifiers", modifier);
    params->SetInteger("windowsVirtualKeyCode", windowsKeyCode);
    params->SetInteger("nativeVirtualKeyCode", nativeKeyCode);
    params->SetString("key", key);
    SendCommand("Input.dispatchKeyEvent", std::move(params));
  }
};

IN_PROC_BROWSER_TEST_F(SyntheticKeyEventTest, KeyEventSynthesizeKey) {
  NavigateToURLBlockUntilNavigationsComplete(shell(), GURL("about:blank"), 1);
  Attach();
  ASSERT_TRUE(content::ExecuteScript(
      shell()->web_contents()->GetRenderViewHost(),
      "function handleKeyEvent(event) {"
        "domAutomationController.setAutomationId(0);"
        "domAutomationController.send(event.key);"
      "}"
      "document.body.addEventListener('keydown', handleKeyEvent);"
      "document.body.addEventListener('keyup', handleKeyEvent);"));

  DOMMessageQueue dom_message_queue;

  // Send enter (keycode 13).
  SendKeyEvent("rawKeyDown", 0, 13, 13, "Enter");
  SendKeyEvent("keyUp", 0, 13, 13, "Enter");

  std::string key;
  ASSERT_TRUE(dom_message_queue.WaitForMessage(&key));
  EXPECT_EQ("\"Enter\"", key);
  ASSERT_TRUE(dom_message_queue.WaitForMessage(&key));
  EXPECT_EQ("\"Enter\"", key);

  // Send escape (keycode 27).
  SendKeyEvent("rawKeyDown", 0, 27, 27, "Escape");
  SendKeyEvent("keyUp", 0, 27, 27, "Escape");

  ASSERT_TRUE(dom_message_queue.WaitForMessage(&key));
  EXPECT_EQ("\"Escape\"", key);
  ASSERT_TRUE(dom_message_queue.WaitForMessage(&key));
  EXPECT_EQ("\"Escape\"", key);
}

namespace {
bool DecodePNG(std::string base64_data, SkBitmap* bitmap) {
  std::string png_data;
  if (!base::Base64Decode(base64_data, &png_data))
    return false;
  return gfx::PNGCodec::Decode(
      reinterpret_cast<unsigned const char*>(png_data.data()), png_data.size(),
      bitmap);
}

// Adapted from cc::ExactPixelComparator.
bool MatchesBitmap(const SkBitmap& expected_bmp,
                   const SkBitmap& actual_bmp,
                   const gfx::Rect& matching_mask) {
  // Number of pixels with an error
  int error_pixels_count = 0;

  gfx::Rect error_bounding_rect = gfx::Rect();

  // Check that bitmaps have identical dimensions.
  EXPECT_EQ(expected_bmp.width(), actual_bmp.width());
  EXPECT_EQ(expected_bmp.height(), actual_bmp.height());
  if (expected_bmp.width() != actual_bmp.width() ||
      expected_bmp.height() != actual_bmp.height()) {
    return false;
  }

  SkAutoLockPixels lock_actual_bmp(actual_bmp);
  SkAutoLockPixels lock_expected_bmp(expected_bmp);

  DCHECK(gfx::SkIRectToRect(actual_bmp.bounds()).Contains(matching_mask));

  for (int x = matching_mask.x(); x < matching_mask.width(); ++x) {
    for (int y = matching_mask.y(); y < matching_mask.height(); ++y) {
      SkColor actual_color = actual_bmp.getColor(x, y);
      SkColor expected_color = expected_bmp.getColor(x, y);
      if (actual_color != expected_color) {
        if (error_pixels_count < 10) {
          LOG(ERROR) << "Pixel (" << x << "," << y << "): expected "
                     << expected_color << " actual " << actual_color;
        }
        error_pixels_count++;
        error_bounding_rect.Union(gfx::Rect(x, y, 1, 1));
      }
    }
  }

  if (error_pixels_count != 0) {
    LOG(ERROR) << "Number of pixel with an error: " << error_pixels_count;
    LOG(ERROR) << "Error Bounding Box : " << error_bounding_rect.ToString();
    return false;
  }

  return true;
}
}  // namespace

class CaptureScreenshotTest : public DevToolsProtocolTest {
 protected:
  void CaptureScreenshotAndCompareTo(const SkBitmap& expected_bitmap) {
    SendCommand("Page.captureScreenshot", nullptr);
    std::string base64;
    EXPECT_TRUE(result_->GetString("data", &base64));
    SkBitmap result_bitmap;
    EXPECT_TRUE(DecodePNG(base64, &result_bitmap));
    gfx::Rect matching_mask(gfx::SkIRectToRect(expected_bitmap.bounds()));
#if defined(OS_MACOSX)
    // Mask out the corners, which may be drawn differently on Mac because of
    // rounded corners.
    matching_mask.Inset(4, 4, 4, 4);
#endif
    EXPECT_TRUE(MatchesBitmap(expected_bitmap, result_bitmap, matching_mask));
  }

  // Takes a screenshot of a colored box that is positioned inside the frame.
  void PlaceAndCaptureBox(const gfx::Size& frame_size,
                          const gfx::Size& box_size,
                          float screenshot_scale) {
    static const int kBoxOffsetHeight = 100;
    const gfx::Size scaled_box_size =
        ScaleToFlooredSize(box_size, screenshot_scale);
    std::unique_ptr<base::DictionaryValue> params;

    VLOG(1) << "Testing screenshot of box with size " << box_size.width() << "x"
            << box_size.height() << "px at scale " << screenshot_scale
            << " ...";

    // Draw a blue box of provided size in the horizontal center of the page.
    EXPECT_TRUE(content::ExecuteScript(
        shell()->web_contents()->GetRenderViewHost(),
        base::StringPrintf(
            "var style = document.body.style;                             "
            "style.overflow = 'hidden';                                   "
            "style.minHeight = '%dpx';                                    "
            "style.backgroundImage = 'linear-gradient(#0000ff, #0000ff)'; "
            "style.backgroundSize = '%dpx %dpx';                          "
            "style.backgroundPosition = '50%% %dpx';                      "
            "style.backgroundRepeat = 'no-repeat';                        ",
            box_size.height() + kBoxOffsetHeight, box_size.width(),
            box_size.height(), kBoxOffsetHeight)));

    // Force frame size: The offset of the blue box within the frame shouldn't
    // change during screenshotting. This verifies that the page doesn't observe
    // a change in frame size as a side effect of screenshotting.
    params.reset(new base::DictionaryValue());
    params->SetInteger("width", frame_size.width());
    params->SetInteger("height", frame_size.height());
    params->SetDouble("deviceScaleFactor", 0);
    params->SetBoolean("mobile", false);
    params->SetBoolean("fitWindow", false);
    SendCommand("Emulation.setDeviceMetricsOverride", std::move(params));

    // Resize frame to scaled blue box size.
    params.reset(new base::DictionaryValue());
    params->SetInteger("width", scaled_box_size.width());
    params->SetInteger("height", scaled_box_size.height());
    SendCommand("Emulation.setVisibleSize", std::move(params));

    // Force viewport to match scaled blue box.
    params.reset(new base::DictionaryValue());
    params->SetDouble("x", (frame_size.width() - box_size.width()) / 2.);
    params->SetDouble("y", kBoxOffsetHeight);
    params->SetDouble("scale", screenshot_scale);
    SendCommand("Emulation.forceViewport", std::move(params));

    // Capture screenshot and verify that it is indeed blue.
    SkBitmap expected_bitmap;
    expected_bitmap.allocN32Pixels(scaled_box_size.width(),
                                   scaled_box_size.height());
    expected_bitmap.eraseColor(SkColorSetRGB(0x00, 0x00, 0xff));
    CaptureScreenshotAndCompareTo(expected_bitmap);

    // Reset for next screenshot.
    SendCommand("Emulation.resetViewport", nullptr);
    SendCommand("Emulation.clearDeviceMetricsOverride", nullptr);
  }

 private:
#if !defined(OS_ANDROID)
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kEnablePixelOutputInTests);
  }
#endif
};

IN_PROC_BROWSER_TEST_F(CaptureScreenshotTest, CaptureScreenshot) {
  // This test fails consistently on low-end Android devices.
  // See crbug.com/653637.
  if (base::SysInfo::IsLowEndDevice()) return;

  shell()->LoadURL(GURL("about:blank"));
  Attach();
  EXPECT_TRUE(
      content::ExecuteScript(shell()->web_contents()->GetRenderViewHost(),
                             "document.body.style.background = '#123456'"));
  SkBitmap expected_bitmap;
  // We compare against the actual physical backing size rather than the
  // view size, because the view size is stored adjusted for DPI and only in
  // integer precision.
  gfx::Size view_size = static_cast<RenderWidgetHostViewBase*>(
                            shell()->web_contents()->GetRenderWidgetHostView())
                            ->GetPhysicalBackingSize();
  expected_bitmap.allocN32Pixels(view_size.width(), view_size.height());
  expected_bitmap.eraseColor(SkColorSetRGB(0x12, 0x34, 0x56));
  CaptureScreenshotAndCompareTo(expected_bitmap);
}

// Setting frame size (through RWHV) is not supported on Android.
#if defined(OS_ANDROID)
#define MAYBE_CaptureScreenshotArea DISABLED_CaptureScreenshotArea
#else
#define MAYBE_CaptureScreenshotArea CaptureScreenshotArea
#endif
IN_PROC_BROWSER_TEST_F(CaptureScreenshotTest,
                       MAYBE_CaptureScreenshotArea) {
  static const gfx::Size kFrameSize(800, 600);

  shell()->LoadURL(GURL("about:blank"));
  Attach();

  // Test capturing a subarea inside the emulated frame at different scales.
  PlaceAndCaptureBox(kFrameSize, gfx::Size(100, 200), 1.0);
  PlaceAndCaptureBox(kFrameSize, gfx::Size(100, 200), 2.0);
  PlaceAndCaptureBox(kFrameSize, gfx::Size(100, 200), 0.5);

  // Ensure that content outside the emulated frame is painted, too.
  PlaceAndCaptureBox(kFrameSize, gfx::Size(10, 10000), 1.0);
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
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

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
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

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
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

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
  EXPECT_FALSE(result_->HasKey("exceptionDetails"));
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
  EXPECT_FALSE(result_->HasKey("exceptionDetails"));
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

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, BrowserCreateAndCloseTarget) {
  NavigateToURLBlockUntilNavigationsComplete(shell(), GURL("about:blank"), 1);
  Attach();
  EXPECT_EQ(1u, shell()->windows().size());
  std::unique_ptr<base::DictionaryValue> params(new base::DictionaryValue());
  params->SetString("url", "about:blank");
  SendCommand("Target.createTarget", std::move(params), true);
  std::string target_id;
  EXPECT_TRUE(result_->GetString("targetId", &target_id));
  EXPECT_EQ(2u, shell()->windows().size());

  // TODO(eseckler): Since the RenderView is closed asynchronously, we currently
  // don't verify that the command actually closes the shell.
  bool success;
  params.reset(new base::DictionaryValue());
  params->SetString("targetId", target_id);
  SendCommand("Target.closeTarget", std::move(params), true);
  EXPECT_TRUE(result_->GetBoolean("success", &success));
  EXPECT_TRUE(success);
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, BrowserGetTargets) {
  NavigateToURLBlockUntilNavigationsComplete(shell(), GURL("about:blank"), 1);
  Attach();
  SendCommand("Target.getTargets", nullptr, true);
  base::ListValue* target_infos;
  EXPECT_TRUE(result_->GetList("targetInfos", &target_infos));
  EXPECT_EQ(1u, target_infos->GetSize());
  base::DictionaryValue* target_info;
  EXPECT_TRUE(target_infos->GetDictionary(0u, &target_info));
  std::string target_id, type, title, url;
  EXPECT_TRUE(target_info->GetString("targetId", &target_id));
  EXPECT_TRUE(target_info->GetString("type", &type));
  EXPECT_TRUE(target_info->GetString("title", &title));
  EXPECT_TRUE(target_info->GetString("url", &url));
  EXPECT_EQ("page", type);
  EXPECT_EQ("about:blank", title);
  EXPECT_EQ("about:blank", url);
}

namespace {
class NavigationFinishedObserver : public content::WebContentsObserver {
 public:
  explicit NavigationFinishedObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents),
        num_finished_(0),
        num_to_wait_for_(0) {}

  ~NavigationFinishedObserver() override {}

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (navigation_handle->WasServerRedirect())
      return;

    num_finished_++;
    if (num_finished_ >= num_to_wait_for_ && num_to_wait_for_ != 0) {
      base::MessageLoop::current()->QuitNow();
    }
  }

  void WaitForNavigationsToFinish(int num_to_wait_for) {
    if (num_finished_ < num_to_wait_for) {
      num_to_wait_for_ = num_to_wait_for;
      RunMessageLoop();
    }
    num_to_wait_for_ = 0;
  }

 private:
  int num_finished_;
  int num_to_wait_for_;
};
}  // namespace

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, ControlNavigationsMainFrame) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate to about:blank first so we can make sure there is a target page we
  // can attach to, and have Page.setControlNavigations complete before we start
  // the navigations we're interested in.
  NavigateToURLBlockUntilNavigationsComplete(shell(), GURL("about:blank"), 1);
  Attach();

  std::unique_ptr<base::DictionaryValue> params(new base::DictionaryValue());
  params->SetBoolean("enabled", true);
  SendCommand("Page.setControlNavigations", std::move(params), true);

  NavigationFinishedObserver navigation_finished_observer(
      shell()->web_contents());

  GURL test_url = embedded_test_server()->GetURL(
      "/devtools/control_navigations/meta_tag.html");
  shell()->LoadURL(test_url);

  std::vector<ExpectedNavigation> expected_navigations = {
      {"http://127.0.0.1/devtools/control_navigations/meta_tag.html",
       true /* expected_is_in_main_frame */, false /* expected_is_redirect */,
       "Proceed"},
      {"http://127.0.0.1/devtools/navigation.html",
       true /* expected_is_in_main_frame */, false /* expected_is_redirect */,
       "Cancel"}};

  ProcessNavigationsAnyOrder(std::move(expected_navigations));

  // Wait for the initial navigation and the cancelled meta refresh navigation
  // to finish.
  navigation_finished_observer.WaitForNavigationsToFinish(2);

  // Check main frame has the expected url.
  EXPECT_EQ(
      "http://127.0.0.1/devtools/control_navigations/meta_tag.html",
      RemovePort(
          shell()->web_contents()->GetMainFrame()->GetLastCommittedURL()));
}

class IsolatedDevToolsProtocolTest : public DevToolsProtocolTest {
 public:
  ~IsolatedDevToolsProtocolTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolateAllSitesForTesting(command_line);
  }
};

IN_PROC_BROWSER_TEST_F(IsolatedDevToolsProtocolTest,
                       ControlNavigationsChildFrames) {
  host_resolver()->AddRule("*", "127.0.0.1");
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate to about:blank first so we can make sure there is a target page we
  // can attach to, and have Page.setControlNavigations complete before we start
  // the navigations we're interested in.
  NavigateToURLBlockUntilNavigationsComplete(shell(), GURL("about:blank"), 1);
  Attach();

  std::unique_ptr<base::DictionaryValue> params(new base::DictionaryValue());
  params->SetBoolean("enabled", true);
  SendCommand("Page.setControlNavigations", std::move(params), true);

  NavigationFinishedObserver navigation_finished_observer(
      shell()->web_contents());

  GURL test_url = embedded_test_server()->GetURL(
      "/devtools/control_navigations/iframe_navigation.html");
  shell()->LoadURL(test_url);

  // Allow main frame navigation, and all iframe navigations to http://a.com
  // Allow initial iframe navigation to http://b.com but dissallow it to
  // navigate to /devtools/navigation.html.
  std::vector<ExpectedNavigation> expected_navigations = {
      {"http://127.0.0.1/devtools/control_navigations/"
       "iframe_navigation.html",
       /* expected_is_in_main_frame */ true,
       /* expected_is_redirect */ false, "Proceed"},
      {"http://127.0.0.1/cross-site/a.com/devtools/control_navigations/"
       "meta_tag.html",
       /* expected_is_in_main_frame */ false,
       /* expected_is_redirect */ false, "Proceed"},
      {"http://127.0.0.1/cross-site/b.com/devtools/control_navigations/"
       "meta_tag.html",
       /* expected_is_in_main_frame */ false,
       /* expected_is_redirect */ false, "Proceed"},
      {"http://a.com/devtools/control_navigations/meta_tag.html",
       /* expected_is_in_main_frame */ false,
       /* expected_is_redirect */ true, "Proceed"},
      {"http://b.com/devtools/control_navigations/meta_tag.html",
       /* expected_is_in_main_frame */ false,
       /* expected_is_redirect */ true, "Proceed"},
      {"http://a.com/devtools/navigation.html",
       /* expected_is_in_main_frame */ false,
       /* expected_is_redirect */ false, "Proceed"},
      {"http://b.com/devtools/navigation.html",
       /* expected_is_in_main_frame */ false,
       /* expected_is_redirect */ false, "Cancel"}};

  ProcessNavigationsAnyOrder(std::move(expected_navigations));

  // Wait for each frame's navigation to finish, ignoring redirects.
  navigation_finished_observer.WaitForNavigationsToFinish(3);

  // Make sure each frame has the expected url.
  EXPECT_THAT(
      GetAllFrameUrls(),
      ElementsAre("http://127.0.0.1/devtools/control_navigations/"
                  "iframe_navigation.html",
                  "http://a.com/devtools/navigation.html",
                  "http://b.com/devtools/control_navigations/meta_tag.html"));
}

// Setting RWHV size is not supported on Android.
#if defined(OS_ANDROID)
#define MAYBE_EmulationSetVisibleSize DISABLED_EmulationSetVisibleSize
#else
#define MAYBE_EmulationSetVisibleSize EmulationSetVisibleSize
#endif
IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest,
                       MAYBE_EmulationSetVisibleSize) {
  NavigateToURLBlockUntilNavigationsComplete(shell(), GURL("about:blank"), 1);
  Attach();
  gfx::Size new_size(200, 400);
  std::unique_ptr<base::DictionaryValue> params(new base::DictionaryValue());
  params->SetInteger("width", new_size.width());
  params->SetInteger("height", new_size.height());
  SendCommand("Emulation.setVisibleSize", std::move(params), true);
  EXPECT_SIZE_EQ(new_size, (shell()->web_contents())
                               ->GetRenderWidgetHostView()
                               ->GetViewBounds()
                               .size());
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, VirtualTimeTest) {
  NavigateToURLBlockUntilNavigationsComplete(shell(), GURL("about:blank"), 1);
  Attach();

  std::unique_ptr<base::DictionaryValue> params(new base::DictionaryValue());
  params->SetString("policy", "pause");
  SendCommand("Emulation.setVirtualTimePolicy", std::move(params), true);

  params.reset(new base::DictionaryValue());
  params->SetString("expression",
                    "setTimeout(function(){console.log('before')}, 1000);"
                    "setTimeout(function(){console.log('after')}, 1001);");
  SendCommand("Runtime.evaluate", std::move(params), true);

  // Let virtual time advance for one second.
  params.reset(new base::DictionaryValue());
  params->SetString("policy", "advance");
  params->SetInteger("budget", 1000);
  SendCommand("Emulation.setVirtualTimePolicy", std::move(params), true);

  WaitForNotification("Emulation.virtualTimeBudgetExpired");

  params.reset(new base::DictionaryValue());
  params->SetString("expression", "console.log('done')");
  SendCommand("Runtime.evaluate", std::move(params), true);

  // The second timer shold not fire.
  EXPECT_THAT(console_messages_, ElementsAre("before", "done"));

  // Let virtual time advance for another second, which should make the second
  // timer fire.
  params.reset(new base::DictionaryValue());
  params->SetString("policy", "advance");
  params->SetInteger("budget", 1000);
  SendCommand("Emulation.setVirtualTimePolicy", std::move(params), true);

  WaitForNotification("Emulation.virtualTimeBudgetExpired");

  EXPECT_THAT(console_messages_, ElementsAre("before", "done", "after"));
}

// Tests that the Security.showCertificateViewer command shows the
// certificate corresponding to the visible navigation entry, even when
// an interstitial is showing. Regression test for
// https://crbug.com/647759.
IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, ShowCertificateViewer) {
  // First test that the correct certificate is shown for a normal
  // (non-interstitial) page.
  NavigateToURLBlockUntilNavigationsComplete(shell(), GURL("about:blank"), 1);
  Attach();

  // Set a dummy certificate on the NavigationEntry.
  shell()
      ->web_contents()
      ->GetController()
      .GetVisibleEntry()
      ->GetSSL()
      .certificate = ok_cert();

  std::unique_ptr<base::DictionaryValue> params1(new base::DictionaryValue());
  SendCommand("Security.showCertificateViewer", std::move(params1), true);

  scoped_refptr<net::X509Certificate> normal_page_cert = shell()
                                                             ->web_contents()
                                                             ->GetController()
                                                             .GetVisibleEntry()
                                                             ->GetSSL()
                                                             .certificate;
  ASSERT_TRUE(normal_page_cert);
  EXPECT_EQ(normal_page_cert, last_shown_certificate());

  // Now test that the correct certificate is shown on an interstitial.
  TestInterstitialDelegate* delegate = new TestInterstitialDelegate;
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  GURL interstitial_url("https://example.test");
  InterstitialPageImpl* interstitial = new InterstitialPageImpl(
      web_contents, static_cast<RenderWidgetHostDelegate*>(web_contents), true,
      interstitial_url, delegate);
  interstitial->Show();
  WaitForInterstitialAttach(web_contents);

  // Set the transient navigation entry certificate.
  NavigationEntry* transient_entry =
      web_contents->GetController().GetTransientEntry();
  ASSERT_TRUE(transient_entry);
  transient_entry->GetSSL().certificate = expired_cert();
  ASSERT_TRUE(transient_entry->GetSSL().certificate);

  std::unique_ptr<base::DictionaryValue> params2(new base::DictionaryValue());
  SendCommand("Security.showCertificateViewer", std::move(params2), true);
  EXPECT_EQ(transient_entry->GetSSL().certificate, last_shown_certificate());
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, TargetDiscovery) {
  std::string temp;
  std::set<std::string> ids;
  std::unique_ptr<base::DictionaryValue> command_params;
  std::unique_ptr<base::DictionaryValue> params;

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL first_url = embedded_test_server()->GetURL("/devtools/navigation.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), first_url, 1);

  GURL second_url = embedded_test_server()->GetURL("/devtools/navigation.html");
  Shell* second = CreateBrowser();
  NavigateToURLBlockUntilNavigationsComplete(second, second_url, 1);

  Attach();
  command_params.reset(new base::DictionaryValue());
  command_params->SetBoolean("discover", true);
  SendCommand("Target.setDiscoverTargets", std::move(command_params), true);
  params = WaitForNotification("Target.targetCreated", true);
  EXPECT_TRUE(params->GetString("targetInfo.type", &temp));
  EXPECT_EQ("page", temp);
  EXPECT_TRUE(params->GetString("targetInfo.targetId", &temp));
  EXPECT_TRUE(ids.find(temp) == ids.end());
  ids.insert(temp);
  params = WaitForNotification("Target.targetCreated", true);
  EXPECT_TRUE(params->GetString("targetInfo.type", &temp));
  EXPECT_EQ("page", temp);
  EXPECT_TRUE(params->GetString("targetInfo.targetId", &temp));
  EXPECT_TRUE(ids.find(temp) == ids.end());
  ids.insert(temp);
  EXPECT_TRUE(notifications_.empty());

  GURL third_url = embedded_test_server()->GetURL("/devtools/navigation.html");
  Shell* third = CreateBrowser();
  NavigateToURLBlockUntilNavigationsComplete(third, third_url, 1);
  params = WaitForNotification("Target.targetCreated", true);
  EXPECT_TRUE(params->GetString("targetInfo.type", &temp));
  EXPECT_EQ("page", temp);
  EXPECT_TRUE(params->GetString("targetInfo.targetId", &temp));
  EXPECT_TRUE(ids.find(temp) == ids.end());
  std::string attached_id = temp;
  ids.insert(temp);
  EXPECT_TRUE(notifications_.empty());

  second->Close();
  second = nullptr;
  params = WaitForNotification("Target.targetDestroyed", true);
  EXPECT_TRUE(params->GetString("targetId", &temp));
  EXPECT_TRUE(ids.find(temp) != ids.end());
  ids.erase(temp);
  EXPECT_TRUE(notifications_.empty());

  command_params.reset(new base::DictionaryValue());
  command_params->SetString("targetId", attached_id);
  SendCommand("Target.attachToTarget", std::move(command_params), true);
  params = WaitForNotification("Target.attachedToTarget", true);
  EXPECT_TRUE(params->GetString("targetInfo.targetId", &temp));
  EXPECT_EQ(attached_id, temp);
  EXPECT_TRUE(notifications_.empty());

  command_params.reset(new base::DictionaryValue());
  command_params->SetBoolean("discover", false);
  SendCommand("Target.setDiscoverTargets", std::move(command_params), true);
  params = WaitForNotification("Target.targetDestroyed", true);
  EXPECT_TRUE(params->GetString("targetId", &temp));
  EXPECT_TRUE(ids.find(temp) != ids.end());
  ids.erase(temp);
  params = WaitForNotification("Target.targetDestroyed", true);
  EXPECT_TRUE(params->GetString("targetId", &temp));
  EXPECT_TRUE(ids.find(temp) != ids.end());
  ids.erase(temp);
  EXPECT_TRUE(notifications_.empty());

  command_params.reset(new base::DictionaryValue());
  command_params->SetString("targetId", attached_id);
  SendCommand("Target.detachFromTarget", std::move(command_params), true);
  params = WaitForNotification("Target.detachedFromTarget", true);
  EXPECT_TRUE(params->GetString("targetId", &temp));
  EXPECT_EQ(attached_id, temp);
  EXPECT_TRUE(notifications_.empty());
}

class SitePerProcessDevToolsProtocolTest : public DevToolsProtocolTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    DevToolsProtocolTest::SetUpCommandLine(command_line);
    IsolateAllSitesForTesting(command_line);
  };

  void SetUpOnMainThread() override {
    DevToolsProtocolTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

IN_PROC_BROWSER_TEST_F(SitePerProcessDevToolsProtocolTest, TargetNoDiscovery) {
  std::string temp;
  std::string target_id;
  std::unique_ptr<base::DictionaryValue> command_params;
  std::unique_ptr<base::DictionaryValue> params;

  GURL main_url(embedded_test_server()->GetURL("/site_per_process_main.html"));
  NavigateToURLBlockUntilNavigationsComplete(shell(), main_url, 1);
  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell()->web_contents())->
          GetFrameTree()->root();

  // Load cross-site page into iframe.
  GURL::Replacements replace_host;
  GURL cross_site_url(embedded_test_server()->GetURL("/title1.html"));
  replace_host.SetHostStr("foo.com");
  cross_site_url = cross_site_url.ReplaceComponents(replace_host);
  NavigateFrameToURL(root->child_at(0), cross_site_url);

  // Enable auto-attach.
  Attach();
  command_params.reset(new base::DictionaryValue());
  command_params->SetBoolean("autoAttach", true);
  command_params->SetBoolean("waitForDebuggerOnStart", true);
  SendCommand("Target.setAutoAttach", std::move(command_params), true);
  EXPECT_TRUE(notifications_.empty());
  command_params.reset(new base::DictionaryValue());
  command_params->SetBoolean("value", true);
  SendCommand("Target.setAttachToFrames", std::move(command_params), false);
  params = WaitForNotification("Target.attachedToTarget", true);
  EXPECT_TRUE(params->GetString("targetInfo.targetId", &target_id));
  EXPECT_TRUE(params->GetString("targetInfo.type", &temp));
  EXPECT_EQ("iframe", temp);

  // Load same-site page into iframe.
  FrameTreeNode* child = root->child_at(0);
  GURL http_url(embedded_test_server()->GetURL("/title1.html"));
  NavigateFrameToURL(child, http_url);
  params = WaitForNotification("Target.detachedFromTarget", true);
  EXPECT_TRUE(params->GetString("targetId", &temp));
  EXPECT_EQ(target_id, temp);

  // Navigate back to cross-site iframe.
  NavigateFrameToURL(root->child_at(0), cross_site_url);
  params = WaitForNotification("Target.attachedToTarget", true);
  EXPECT_TRUE(params->GetString("targetInfo.targetId", &target_id));
  EXPECT_TRUE(params->GetString("targetInfo.type", &temp));
  EXPECT_EQ("iframe", temp);

  // Disable auto-attach.
  command_params.reset(new base::DictionaryValue());
  command_params->SetBoolean("autoAttach", false);
  command_params->SetBoolean("waitForDebuggerOnStart", false);
  SendCommand("Target.setAutoAttach", std::move(command_params), false);
  params = WaitForNotification("Target.detachedFromTarget", true);
  EXPECT_TRUE(params->GetString("targetId", &temp));
  EXPECT_EQ(target_id, temp);
}

}  // namespace content
