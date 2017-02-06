// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <tuple>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "cc/trees/layer_tree_host.h"
#include "content/child/request_extra_data.h"
#include "content/child/service_worker/service_worker_network_provider.h"
#include "content/common/content_switches_internal.h"
#include "content/common/frame_messages.h"
#include "content/common/frame_replication_state.h"
#include "content/common/site_isolation_policy.h"
#include "content/common/ssl_status_serialization.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/web_ui_controller_factory.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/page_zoom.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/document_state.h"
#include "content/public/renderer/navigation_state.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/frame_load_waiter.h"
#include "content/public/test/render_view_test.h"
#include "content/public/test/test_utils.h"
#include "content/renderer/accessibility/render_accessibility_impl.h"
#include "content/renderer/devtools/devtools_agent.h"
#include "content/renderer/gpu/render_widget_compositor.h"
#include "content/renderer/history_controller.h"
#include "content/renderer/history_serialization.h"
#include "content/renderer/navigation_state_impl.h"
#include "content/renderer/render_frame_proxy.h"
#include "content/renderer/render_process.h"
#include "content/renderer/render_view_impl.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/test/mock_keyboard.h"
#include "content/test/test_render_frame.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebData.h"
#include "third_party/WebKit/public/platform/WebHTTPBody.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "third_party/WebKit/public/web/WebDeviceEmulationParams.h"
#include "third_party/WebKit/public/web/WebFrameContentDumper.h"
#include "third_party/WebKit/public/web/WebHistoryCommitType.h"
#include "third_party/WebKit/public/web/WebHistoryItem.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebPerformance.h"
#include "third_party/WebKit/public/web/WebRuntimeFeatures.h"
#include "third_party/WebKit/public/web/WebScriptSource.h"
#include "third_party/WebKit/public/web/WebSettings.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/WebKit/public/web/WebWindowFeatures.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/range/range.h"

#if defined(USE_AURA) && defined(USE_X11)
#include <X11/Xlib.h>
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/test/events_test_utils.h"
#include "ui/events/test/events_test_utils_x11.h"
#endif

#if defined(USE_OZONE)
#include "ui/events/keycodes/keyboard_code_conversion.h"
#endif

#include "url/url_constants.h"

using blink::WebFrame;
using blink::WebFrameContentDumper;
using blink::WebInputEvent;
using blink::WebLocalFrame;
using blink::WebMouseEvent;
using blink::WebRuntimeFeatures;
using blink::WebString;
using blink::WebTextDirection;
using blink::WebURLError;

namespace content {

namespace {

static const int kProxyRoutingId = 13;

#if (defined(USE_AURA) && defined(USE_X11)) || defined(USE_OZONE)
// Converts MockKeyboard::Modifiers to ui::EventFlags.
int ConvertMockKeyboardModifier(MockKeyboard::Modifiers modifiers) {
  static struct ModifierMap {
    MockKeyboard::Modifiers src;
    int dst;
  } kModifierMap[] = {
    { MockKeyboard::LEFT_SHIFT, ui::EF_SHIFT_DOWN },
    { MockKeyboard::RIGHT_SHIFT, ui::EF_SHIFT_DOWN },
    { MockKeyboard::LEFT_CONTROL, ui::EF_CONTROL_DOWN },
    { MockKeyboard::RIGHT_CONTROL, ui::EF_CONTROL_DOWN },
    { MockKeyboard::LEFT_ALT,  ui::EF_ALT_DOWN },
    { MockKeyboard::RIGHT_ALT, ui::EF_ALT_DOWN },
  };
  int flags = 0;
  for (size_t i = 0; i < arraysize(kModifierMap); ++i) {
    if (kModifierMap[i].src & modifiers) {
      flags |= kModifierMap[i].dst;
    }
  }
  return flags;
}
#endif

class WebUITestWebUIControllerFactory : public WebUIControllerFactory {
 public:
  WebUIController* CreateWebUIControllerForURL(WebUI* web_ui,
                                               const GURL& url) const override {
    return NULL;
  }
  WebUI::TypeID GetWebUIType(BrowserContext* browser_context,
                             const GURL& url) const override {
    return WebUI::kNoWebUI;
  }
  bool UseWebUIForURL(BrowserContext* browser_context,
                      const GURL& url) const override {
    return HasWebUIScheme(url);
  }
  bool UseWebUIBindingsForURL(BrowserContext* browser_context,
                              const GURL& url) const override {
    return HasWebUIScheme(url);
  }
};

// Timestamps logged close to each other under low resolution timers
// are more likely to record the same value. Allow for this by relaxing
// constraints on systems with these timers.
bool TimeTicksGT(const base::TimeTicks& x, const base::TimeTicks& y) {
  return base::TimeTicks::IsHighResolution() ? x > y : x >= y;
}

// FrameReplicationState is normally maintained in the browser process,
// but the function below provides a way for tests to construct a partial
// FrameReplicationState within the renderer process.  We say "partial",
// because some fields of FrameReplicationState cannot be filled out
// by content-layer, renderer code (still the constructed, partial
// FrameReplicationState is sufficiently complete to avoid trigerring
// asserts that a default/empty FrameReplicationState would).
FrameReplicationState ReconstructReplicationStateForTesting(
    TestRenderFrame* test_render_frame) {
  blink::WebLocalFrame* frame = test_render_frame->GetWebFrame();

  FrameReplicationState result;
  // can't recover result.scope - no way to get WebTreeScopeType via public
  // blink API...
  result.name = base::UTF16ToUTF8(base::StringPiece16(frame->assignedName()));
  result.unique_name =
      base::UTF16ToUTF8(base::StringPiece16(frame->uniqueName()));
  result.sandbox_flags = frame->effectiveSandboxFlags();
  // result.should_enforce_strict_mixed_content_checking is calculated in the
  // browser...
  result.origin = frame->getSecurityOrigin();

  return result;
}

}  // namespace

class RenderViewImplTest : public RenderViewTest {
 public:
  RenderViewImplTest() {
    // Attach a pseudo keyboard device to this object.
    mock_keyboard_.reset(new MockKeyboard());
  }

  ~RenderViewImplTest() override {}

  void SetUp() override {
    // Enable Blink's experimental and test only features so that test code
    // does not have to bother enabling each feature.
    WebRuntimeFeatures::enableExperimentalFeatures(true);
    WebRuntimeFeatures::enableTestOnlyFeatures(true);
    RenderViewTest::SetUp();
  }

  RenderViewImpl* view() {
    return static_cast<RenderViewImpl*>(view_);
  }

  int view_page_id() {
    return view()->page_id_;
  }

  TestRenderFrame* frame() {
    return static_cast<TestRenderFrame*>(view()->GetMainRenderFrame());
  }

  void GoToOffsetWithParams(int offset,
                            const PageState& state,
                            const CommonNavigationParams common_params,
                            const StartNavigationParams start_params,
                            RequestNavigationParams request_params) {
    EXPECT_TRUE(common_params.transition & ui::PAGE_TRANSITION_FORWARD_BACK);
    int pending_offset = offset + view()->history_list_offset_;

    request_params.page_state = state;
    request_params.page_id = view()->page_id_ + offset;
    request_params.nav_entry_id = pending_offset + 1;
    request_params.pending_history_list_offset = pending_offset;
    request_params.current_history_list_offset = view()->history_list_offset_;
    request_params.current_history_list_length = view()->history_list_length_;
    frame()->Navigate(common_params, start_params, request_params);

    // The load actually happens asynchronously, so we pump messages to process
    // the pending continuation.
    FrameLoadWaiter(frame()).Wait();
  }

  template<class T>
  typename T::Param ProcessAndReadIPC() {
    ProcessPendingMessages();
    const IPC::Message* message =
        render_thread_->sink().GetUniqueMessageMatching(T::ID);
    typename T::Param param;
    T::Read(message, &param);
    return param;
  }

  // Sends IPC messages that emulates a key-press event.
  int SendKeyEvent(MockKeyboard::Layout layout,
                   int key_code,
                   MockKeyboard::Modifiers modifiers,
                   base::string16* output) {
#if defined(OS_WIN)
    // Retrieve the Unicode character for the given tuple (keyboard-layout,
    // key-code, and modifiers).
    // Exit when a keyboard-layout driver cannot assign a Unicode character to
    // the tuple to prevent sending an invalid key code to the RenderView
    // object.
    CHECK(mock_keyboard_.get());
    CHECK(output);
    int length = mock_keyboard_->GetCharacters(layout, key_code, modifiers,
                                               output);
    if (length != 1)
      return -1;

    // Create IPC messages from Windows messages and send them to our
    // back-end.
    // A keyboard event of Windows consists of three Windows messages:
    // WM_KEYDOWN, WM_CHAR, and WM_KEYUP.
    // WM_KEYDOWN and WM_KEYUP sends virtual-key codes. On the other hand,
    // WM_CHAR sends a composed Unicode character.
    MSG msg1 = { NULL, WM_KEYDOWN, key_code, 0 };
    ui::KeyEvent evt1(msg1);
    NativeWebKeyboardEvent keydown_event(evt1);
    SendNativeKeyEvent(keydown_event);

    MSG msg2 = { NULL, WM_CHAR, (*output)[0], 0 };
    ui::KeyEvent evt2(msg2);
    NativeWebKeyboardEvent char_event(evt2);
    SendNativeKeyEvent(char_event);

    MSG msg3 = { NULL, WM_KEYUP, key_code, 0 };
    ui::KeyEvent evt3(msg3);
    NativeWebKeyboardEvent keyup_event(evt3);
    SendNativeKeyEvent(keyup_event);

    return length;
#elif defined(USE_AURA) && defined(USE_X11)
    // We ignore |layout|, which means we are only testing the layout of the
    // current locale. TODO(mazda): fix this to respect |layout|.
    CHECK(output);
    const int flags = ConvertMockKeyboardModifier(modifiers);

    ui::ScopedXI2Event xevent;
    xevent.InitKeyEvent(ui::ET_KEY_PRESSED,
                        static_cast<ui::KeyboardCode>(key_code),
                        flags);
    ui::KeyEvent event1(xevent);
    NativeWebKeyboardEvent keydown_event(event1);
    SendNativeKeyEvent(keydown_event);

    // X11 doesn't actually have native character events, but give the test
    // what it wants.
    xevent.InitKeyEvent(ui::ET_KEY_PRESSED,
                        static_cast<ui::KeyboardCode>(key_code),
                        flags);
    ui::KeyEvent event2(xevent);
    event2.set_character(
        DomCodeToUsLayoutCharacter(event2.code(), event2.flags()));
    ui::KeyEventTestApi test_event2(&event2);
    test_event2.set_is_char(true);
    NativeWebKeyboardEvent char_event(event2);
    SendNativeKeyEvent(char_event);

    xevent.InitKeyEvent(ui::ET_KEY_RELEASED,
                        static_cast<ui::KeyboardCode>(key_code),
                        flags);
    ui::KeyEvent event3(xevent);
    NativeWebKeyboardEvent keyup_event(event3);
    SendNativeKeyEvent(keyup_event);

    long c = DomCodeToUsLayoutCharacter(
        UsLayoutKeyboardCodeToDomCode(static_cast<ui::KeyboardCode>(key_code)),
        flags);
    output->assign(1, static_cast<base::char16>(c));
    return 1;
#elif defined(USE_OZONE)
    const int flags = ConvertMockKeyboardModifier(modifiers);

    ui::KeyEvent keydown_event(ui::ET_KEY_PRESSED,
                               static_cast<ui::KeyboardCode>(key_code),
                               flags);
    NativeWebKeyboardEvent keydown_web_event(keydown_event);
    SendNativeKeyEvent(keydown_web_event);

    ui::KeyEvent char_event(keydown_event.GetCharacter(),
                            static_cast<ui::KeyboardCode>(key_code),
                            flags);
    NativeWebKeyboardEvent char_web_event(char_event);
    SendNativeKeyEvent(char_web_event);

    ui::KeyEvent keyup_event(ui::ET_KEY_RELEASED,
                             static_cast<ui::KeyboardCode>(key_code),
                             flags);
    NativeWebKeyboardEvent keyup_web_event(keyup_event);
    SendNativeKeyEvent(keyup_web_event);

    long c = DomCodeToUsLayoutCharacter(
        UsLayoutKeyboardCodeToDomCode(static_cast<ui::KeyboardCode>(key_code)),
        flags);
    output->assign(1, static_cast<base::char16>(c));
    return 1;
#else
    NOTIMPLEMENTED();
    return L'\0';
#endif
  }

  void EnablePreferredSizeMode() {
    view()->OnEnablePreferredSizeChangedMode();
  }

  const gfx::Size& GetPreferredSize() {
    view()->CheckPreferredSize();
    return view()->preferred_size_;
  }

  void SetZoomLevel(double level) {
    view()->OnSetZoomLevel(
        PageMsg_SetZoomLevel_Command::USE_CURRENT_TEMPORARY_MODE, level);
  }

 private:
  std::unique_ptr<MockKeyboard> mock_keyboard_;
};

class DevToolsAgentTest : public RenderViewImplTest {
 public:
  void Attach() {
    notifications_ = std::vector<std::string>();
    expecting_pause_ = false;
    std::string host_id = "host_id";
    agent()->OnAttach(host_id, 17);
    agent()->send_protocol_message_callback_for_test_ = base::Bind(
       &DevToolsAgentTest::OnDevToolsMessage, base::Unretained(this));
  }

  void Detach() {
    agent()->send_protocol_message_callback_for_test_.Reset();
    agent()->OnDetach();
  }

  bool IsPaused() {
    return agent()->paused_;
  }

  void DispatchDevToolsMessage(const std::string& method,
                               const std::string& message) {
    agent()->OnDispatchOnInspectorBackend(17, 1, method, message);
  }

  void CloseWhilePaused() {
    EXPECT_TRUE(IsPaused());
    view()->NotifyOnClose();
  }

  void OnDevToolsMessage(
      int, int, const std::string& message, const std::string&) {
    last_message_ = base::WrapUnique(static_cast<base::DictionaryValue*>(
        base::JSONReader::Read(message).release()));
    int id;
    if (!last_message_->GetInteger("id", &id)) {
      std::string notification;
      EXPECT_TRUE(last_message_->GetString("method", &notification));
      notifications_.push_back(notification);

      if (notification == "Debugger.paused" && expecting_pause_) {
        base::ListValue* call_frames;
        EXPECT_TRUE(last_message_->GetList("params.callFrames", &call_frames));
        if (call_frames) {
          EXPECT_EQ(call_frames_count_,
                    static_cast<int>(call_frames->GetSize()));
        }
        expecting_pause_ = false;
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE,
            base::Bind(&DevToolsAgentTest::DispatchDevToolsMessage,
                       base::Unretained(this),
                       "Debugger.resume",
                       "{\"id\":100,\"method\":\"Debugger.resume\"}"));
      }
    }
  }

  int CountNotifications(const std::string& notification) {
    int result = 0;
    for (const std::string& s : notifications_) {
      if (s == notification)
        ++result;
    }
    return result;
  }

  base::DictionaryValue* LastReceivedMessage() {
    return last_message_.get();
  }

  void ExpectPauseAndResume(int call_frames_count) {
    expecting_pause_ = true;
    call_frames_count_ = call_frames_count;
  }

 private:
  DevToolsAgent* agent() {
    return frame()->devtools_agent();
  }

  std::vector<std::string> notifications_;
  std::unique_ptr<base::DictionaryValue> last_message_;
  int call_frames_count_;
  bool expecting_pause_;
};

class RenderViewImplBlinkSettingsTest : public RenderViewImplTest {
 public:
  virtual void DoSetUp() {
    RenderViewImplTest::SetUp();
  }

  blink::WebSettings* settings() {
    return view()->webview()->settings();
  }

 protected:
  // Blink settings may be specified on the command line, which must
  // be configured before RenderViewImplTest::SetUp runs. Thus we make
  // SetUp() a no-op, and expose RenderViewImplTest::SetUp() via
  // DoSetUp(), to allow tests to perform command line modifications
  // before RenderViewImplTest::SetUp is run. Each test must invoke
  // DoSetUp manually once pre-SetUp configuration is complete.
  void SetUp() override {}
};

class RenderViewImplScaleFactorTest : public RenderViewImplBlinkSettingsTest {
 protected:
  void SetDeviceScaleFactor(float dsf) {
    ResizeParams params;
    params.screen_info.deviceScaleFactor = dsf;
    params.new_size = gfx::Size(100, 100);
    params.physical_backing_size = gfx::Size(200, 200);
    params.visible_viewport_size = params.new_size;
    params.needs_resize_ack = false;
    view()->OnResize(params);
    ASSERT_EQ(dsf, view()->device_scale_factor_);
  }

  void TestEmulatedSizeDprDsf(int width, int height, float dpr,
                              float compositor_dsf) {
    static base::string16 get_width =
        base::ASCIIToUTF16("Number(window.innerWidth)");
    static base::string16 get_height =
        base::ASCIIToUTF16("Number(window.innerHeight)");
    static base::string16 get_dpr =
        base::ASCIIToUTF16("Number(window.devicePixelRatio * 10)");

    int emulated_width, emulated_height;
    int emulated_dpr;
    blink::WebDeviceEmulationParams params;
    params.viewSize.width = width;
    params.viewSize.height = height;
    params.deviceScaleFactor = dpr;
    view()->OnEnableDeviceEmulation(params);
    EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(get_width, &emulated_width));
    EXPECT_EQ(width, emulated_width);
    EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(get_height,
                                                   &emulated_height));
    EXPECT_EQ(height, emulated_height);
    EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(get_dpr, &emulated_dpr));
    EXPECT_EQ(static_cast<int>(dpr * 10), emulated_dpr);
    EXPECT_EQ(compositor_dsf,
              view()->compositor()->layer_tree_host()->device_scale_factor());
  }
};

// Ensure that the main RenderFrame is deleted and cleared from the RenderView
// after closing it.
TEST_F(RenderViewImplTest, RenderFrameClearedAfterClose) {
  // Create a new main frame RenderFrame so that we don't interfere with the
  // shutdown of frame() in RenderViewTest.TearDown.
  blink::WebURLRequest popup_request(GURL("http://foo.com"));
  blink::WebView* new_web_view = view()->createView(
      GetMainFrame(), popup_request, blink::WebWindowFeatures(), "foo",
      blink::WebNavigationPolicyNewForegroundTab, false);
  RenderViewImpl* new_view = RenderViewImpl::FromWebView(new_web_view);

  // Close the view, causing the main RenderFrame to be detached and deleted.
  new_view->Close();
  EXPECT_FALSE(new_view->GetMainRenderFrame());

  // Clean up after the new view so we don't leak it.
  new_view->Release();
}

// Test that we get form state change notifications when input fields change.
TEST_F(RenderViewImplTest, OnNavStateChanged) {
  view()->set_send_content_state_immediately(true);
  LoadHTML("<input type=\"text\" id=\"elt_text\"></input>");

  // We should NOT have gotten a form state change notification yet.
  EXPECT_FALSE(render_thread_->sink().GetFirstMessageMatching(
      FrameHostMsg_UpdateState::ID));
  EXPECT_FALSE(render_thread_->sink().GetFirstMessageMatching(
      ViewHostMsg_UpdateState::ID));
  render_thread_->sink().ClearMessages();

  // Change the value of the input. We should have gotten an update state
  // notification. We need to spin the message loop to catch this update.
  ExecuteJavaScriptForTests(
      "document.getElementById('elt_text').value = 'foo';");
  ProcessPendingMessages();

  if (SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    EXPECT_TRUE(render_thread_->sink().GetUniqueMessageMatching(
        FrameHostMsg_UpdateState::ID));
  } else {
    EXPECT_TRUE(render_thread_->sink().GetUniqueMessageMatching(
        ViewHostMsg_UpdateState::ID));
  }
}

TEST_F(RenderViewImplTest, OnNavigationHttpPost) {
  // An http url will trigger a resource load so cannot be used here.
  CommonNavigationParams common_params;
  StartNavigationParams start_params;
  RequestNavigationParams request_params;
  common_params.url = GURL("data:text/html,<div>Page</div>");
  common_params.navigation_type = FrameMsg_Navigate_Type::NORMAL;
  common_params.transition = ui::PAGE_TRANSITION_TYPED;
  common_params.method = "POST";
  request_params.page_id = -1;

  // Set up post data.
  const char raw_data[] = "post \0\ndata";
  const size_t length = arraysize(raw_data);
  scoped_refptr<ResourceRequestBodyImpl> post_data(new ResourceRequestBodyImpl);
  post_data->AppendBytes(raw_data, length);
  common_params.post_data = post_data;

  frame()->Navigate(common_params, start_params, request_params);
  ProcessPendingMessages();

  const IPC::Message* frame_navigate_msg =
      render_thread_->sink().GetUniqueMessageMatching(
          FrameHostMsg_DidCommitProvisionalLoad::ID);
  EXPECT_TRUE(frame_navigate_msg);

  FrameHostMsg_DidCommitProvisionalLoad::Param host_nav_params;
  FrameHostMsg_DidCommitProvisionalLoad::Read(frame_navigate_msg,
                                              &host_nav_params);
  EXPECT_EQ("POST", std::get<0>(host_nav_params).method);

  // Check post data sent to browser matches
  EXPECT_TRUE(std::get<0>(host_nav_params).page_state.IsValid());
  std::unique_ptr<HistoryEntry> entry =
      PageStateToHistoryEntry(std::get<0>(host_nav_params).page_state);
  blink::WebHTTPBody body = entry->root().httpBody();
  blink::WebHTTPBody::Element element;
  bool successful = body.elementAt(0, element);
  EXPECT_TRUE(successful);
  EXPECT_EQ(blink::WebHTTPBody::Element::TypeData, element.type);
  EXPECT_EQ(length, element.data.size());
  EXPECT_EQ(0, memcmp(raw_data, element.data.data(), length));
}

// Check that page ID will be initialized in case of navigation
// that replaces current entry.
TEST_F(RenderViewImplTest, OnBrowserNavigationUpdatePageID) {
  // An http url will trigger a resource load so cannot be used here.
  CommonNavigationParams common_params;
  StartNavigationParams start_params;
  RequestNavigationParams request_params;
  common_params.url = GURL("data:text/html,<div>Page</div>");
  common_params.navigation_type = FrameMsg_Navigate_Type::NORMAL;
  common_params.transition = ui::PAGE_TRANSITION_TYPED;

  // Set up params to emulate a browser side navigation
  // that should replace current entry.
  common_params.should_replace_current_entry = true;
  request_params.page_id = -1;
  request_params.nav_entry_id = 1;
  request_params.current_history_list_length = 1;

  frame()->Navigate(common_params, start_params, request_params);
  ProcessPendingMessages();

  // Page ID should be initialized.
  EXPECT_NE(view_page_id(), -1);

  const IPC::Message* frame_navigate_msg =
      render_thread_->sink().GetUniqueMessageMatching(
          FrameHostMsg_DidCommitProvisionalLoad::ID);
  EXPECT_TRUE(frame_navigate_msg);

  FrameHostMsg_DidCommitProvisionalLoad::Param host_nav_params;
  FrameHostMsg_DidCommitProvisionalLoad::Read(frame_navigate_msg,
                                              &host_nav_params);
  EXPECT_TRUE(std::get<0>(host_nav_params).page_state.IsValid());

  const IPC::Message* frame_page_id_msg =
      render_thread_->sink().GetUniqueMessageMatching(
          FrameHostMsg_DidAssignPageId::ID);
  EXPECT_TRUE(frame_page_id_msg);

  FrameHostMsg_DidAssignPageId::Param host_page_id_params;
  FrameHostMsg_DidAssignPageId::Read(frame_page_id_msg, &host_page_id_params);

  EXPECT_EQ(std::get<0>(host_page_id_params), view_page_id());
}

#if defined(OS_ANDROID)
TEST_F(RenderViewImplTest, OnNavigationLoadDataWithBaseURL) {
  CommonNavigationParams common_params;
  common_params.url = GURL("data:text/html,");
  common_params.navigation_type = FrameMsg_Navigate_Type::NORMAL;
  common_params.transition = ui::PAGE_TRANSITION_TYPED;
  common_params.base_url_for_data_url = GURL("about:blank");
  common_params.history_url_for_data_url = GURL("about:blank");
  RequestNavigationParams request_params;
  request_params.data_url_as_string =
      "data:text/html,<html><head><title>Data page</title></head></html>";

  frame()->Navigate(common_params, StartNavigationParams(),
                    request_params);
  const IPC::Message* frame_title_msg = nullptr;
  do {
    ProcessPendingMessages();
    frame_title_msg = render_thread_->sink().GetUniqueMessageMatching(
        FrameHostMsg_UpdateTitle::ID);
  } while (!frame_title_msg);

  // Check post data sent to browser matches.
  FrameHostMsg_UpdateTitle::Param title_params;
  EXPECT_TRUE(FrameHostMsg_UpdateTitle::Read(frame_title_msg, &title_params));
  EXPECT_EQ(base::ASCIIToUTF16("Data page"), std::get<0>(title_params));
}
#endif

TEST_F(RenderViewImplTest, DecideNavigationPolicy) {
  WebUITestWebUIControllerFactory factory;
  WebUIControllerFactory::RegisterFactory(&factory);

  DocumentState state;
  state.set_navigation_state(NavigationStateImpl::CreateContentInitiated());

  // Navigations to normal HTTP URLs can be handled locally.
  blink::WebURLRequest request(GURL("http://foo.com"));
  request.setFetchRequestMode(blink::WebURLRequest::FetchRequestModeNavigate);
  request.setFetchCredentialsMode(
      blink::WebURLRequest::FetchCredentialsModeInclude);
  request.setFetchRedirectMode(blink::WebURLRequest::FetchRedirectModeManual);
  request.setFrameType(blink::WebURLRequest::FrameTypeTopLevel);
  blink::WebFrameClient::NavigationPolicyInfo policy_info(request);
  policy_info.navigationType = blink::WebNavigationTypeLinkClicked;
  policy_info.defaultPolicy = blink::WebNavigationPolicyCurrentTab;
  blink::WebNavigationPolicy policy = frame()->decidePolicyForNavigation(
          policy_info);
  if (!IsBrowserSideNavigationEnabled()) {
    EXPECT_EQ(blink::WebNavigationPolicyCurrentTab, policy);
  } else {
    // If this is a renderer-initiated navigation that just begun, it should
    // stop and be sent to the browser.
    EXPECT_EQ(blink::WebNavigationPolicyHandledByClient, policy);

    // If this a navigation that is ready to commit, it should be handled
    // locally.
    request.setCheckForBrowserSideNavigation(false);
    policy = frame()->decidePolicyForNavigation(policy_info);
    EXPECT_EQ(blink::WebNavigationPolicyCurrentTab, policy);
  }

  // Verify that form posts to WebUI URLs will be sent to the browser process.
  blink::WebURLRequest form_request(GURL("chrome://foo"));
  blink::WebFrameClient::NavigationPolicyInfo form_policy_info(form_request);
  form_policy_info.navigationType = blink::WebNavigationTypeFormSubmitted;
  form_policy_info.defaultPolicy = blink::WebNavigationPolicyCurrentTab;
  form_request.setHTTPMethod("POST");
  policy = frame()->decidePolicyForNavigation(form_policy_info);
  EXPECT_EQ(blink::WebNavigationPolicyIgnore, policy);

  // Verify that popup links to WebUI URLs also are sent to browser.
  blink::WebURLRequest popup_request(GURL("chrome://foo"));
  blink::WebFrameClient::NavigationPolicyInfo popup_policy_info(popup_request);
  popup_policy_info.navigationType = blink::WebNavigationTypeLinkClicked;
  popup_policy_info.defaultPolicy = blink::WebNavigationPolicyNewForegroundTab;
  policy = frame()->decidePolicyForNavigation(popup_policy_info);
  EXPECT_EQ(blink::WebNavigationPolicyIgnore, policy);
}

TEST_F(RenderViewImplTest, DecideNavigationPolicyHandlesAllTopLevel) {
  DocumentState state;
  state.set_navigation_state(NavigationStateImpl::CreateContentInitiated());

  RendererPreferences prefs = view()->renderer_preferences();
  prefs.browser_handles_all_top_level_requests = true;
  view()->OnSetRendererPrefs(prefs);

  const blink::WebNavigationType kNavTypes[] = {
    blink::WebNavigationTypeLinkClicked,
    blink::WebNavigationTypeFormSubmitted,
    blink::WebNavigationTypeBackForward,
    blink::WebNavigationTypeReload,
    blink::WebNavigationTypeFormResubmitted,
    blink::WebNavigationTypeOther,
  };

  blink::WebURLRequest request(GURL("http://foo.com"));
  blink::WebFrameClient::NavigationPolicyInfo policy_info(request);
  policy_info.defaultPolicy = blink::WebNavigationPolicyCurrentTab;

  for (size_t i = 0; i < arraysize(kNavTypes); ++i) {
    policy_info.navigationType = kNavTypes[i];

    blink::WebNavigationPolicy policy = frame()->decidePolicyForNavigation(
        policy_info);
    EXPECT_EQ(blink::WebNavigationPolicyIgnore, policy);
  }
}

TEST_F(RenderViewImplTest, DecideNavigationPolicyForWebUI) {
  // Enable bindings to simulate a WebUI view.
  view()->OnAllowBindings(BINDINGS_POLICY_WEB_UI);

  DocumentState state;
  state.set_navigation_state(NavigationStateImpl::CreateContentInitiated());

  // Navigations to normal HTTP URLs will be sent to browser process.
  blink::WebURLRequest request(GURL("http://foo.com"));
  blink::WebFrameClient::NavigationPolicyInfo policy_info(request);
  policy_info.navigationType = blink::WebNavigationTypeLinkClicked;
  policy_info.defaultPolicy = blink::WebNavigationPolicyCurrentTab;

  blink::WebNavigationPolicy policy = frame()->decidePolicyForNavigation(
      policy_info);
  EXPECT_EQ(blink::WebNavigationPolicyIgnore, policy);

  // Navigations to WebUI URLs will also be sent to browser process.
  blink::WebURLRequest webui_request(GURL("chrome://foo"));
  blink::WebFrameClient::NavigationPolicyInfo webui_policy_info(webui_request);
  webui_policy_info.navigationType = blink::WebNavigationTypeLinkClicked;
  webui_policy_info.defaultPolicy = blink::WebNavigationPolicyCurrentTab;
  policy = frame()->decidePolicyForNavigation(webui_policy_info);
  EXPECT_EQ(blink::WebNavigationPolicyIgnore, policy);

  // Verify that form posts to data URLs will be sent to the browser process.
  blink::WebURLRequest data_request(GURL("data:text/html,foo"));
  blink::WebFrameClient::NavigationPolicyInfo data_policy_info(data_request);
  data_policy_info.navigationType = blink::WebNavigationTypeFormSubmitted;
  data_policy_info.defaultPolicy = blink::WebNavigationPolicyCurrentTab;
  data_request.setHTTPMethod("POST");
  policy = frame()->decidePolicyForNavigation(data_policy_info);
  EXPECT_EQ(blink::WebNavigationPolicyIgnore, policy);

  // Verify that a popup that creates a view first and then navigates to a
  // normal HTTP URL will be sent to the browser process, even though the
  // new view does not have any enabled_bindings_.
  blink::WebURLRequest popup_request(GURL("http://foo.com"));
  blink::WebView* new_web_view = view()->createView(
      GetMainFrame(), popup_request, blink::WebWindowFeatures(), "foo",
      blink::WebNavigationPolicyNewForegroundTab, false);
  RenderViewImpl* new_view = RenderViewImpl::FromWebView(new_web_view);
  blink::WebFrameClient::NavigationPolicyInfo popup_policy_info(popup_request);
  popup_policy_info.navigationType = blink::WebNavigationTypeLinkClicked;
  popup_policy_info.defaultPolicy = blink::WebNavigationPolicyNewForegroundTab;
  policy = static_cast<RenderFrameImpl*>(new_view->GetMainRenderFrame())->
      decidePolicyForNavigation(popup_policy_info);
  EXPECT_EQ(blink::WebNavigationPolicyIgnore, policy);

  // Clean up after the new view so we don't leak it.
  new_view->Close();
  new_view->Release();
}

// Verify that security origins are replicated properly to RenderFrameProxies
// when swapping out.
TEST_F(RenderViewImplTest, OriginReplicationForSwapOut) {
  // This test should only run with --site-per-process, since origin
  // replication only happens in that mode.
  if (!AreAllSitesIsolatedForTesting())
    return;

  LoadHTML(
      "Hello <iframe src='data:text/html,frame 1'></iframe>"
      "<iframe src='data:text/html,frame 2'></iframe>");
  WebFrame* web_frame = frame()->GetWebFrame();
  TestRenderFrame* child_frame = static_cast<TestRenderFrame*>(
      RenderFrame::FromWebFrame(web_frame->firstChild()));

  // Swap the child frame out and pass a replicated origin to be set for
  // WebRemoteFrame.
  content::FrameReplicationState replication_state =
      ReconstructReplicationStateForTesting(child_frame);
  replication_state.origin = url::Origin(GURL("http://foo.com"));
  child_frame->SwapOut(kProxyRoutingId, true, replication_state);

  // The child frame should now be a WebRemoteFrame.
  EXPECT_TRUE(web_frame->firstChild()->isWebRemoteFrame());

  // Expect the origin to be updated properly.
  blink::WebSecurityOrigin origin =
      web_frame->firstChild()->getSecurityOrigin();
  EXPECT_EQ(origin.toString(),
            WebString::fromUTF8(replication_state.origin.Serialize()));

  // Now, swap out the second frame using a unique origin and verify that it is
  // replicated correctly.
  replication_state.origin = url::Origin();
  TestRenderFrame* child_frame2 = static_cast<TestRenderFrame*>(
      RenderFrame::FromWebFrame(web_frame->lastChild()));
  child_frame2->SwapOut(kProxyRoutingId + 1, true, replication_state);
  EXPECT_TRUE(web_frame->lastChild()->isWebRemoteFrame());
  EXPECT_TRUE(web_frame->lastChild()->getSecurityOrigin().isUnique());
}

// Test for https://crbug.com/568676, where a parent detaches a remote child
// while the browser navigates it to the parent's site in parallel, with the
// detach happening after the provisional RenderFrame is created but before
// FrameMsg_Navigate is received.  This is a variant of
// https://crbug.com/526304.
TEST_F(RenderViewImplTest, NavigateProxyAndDetachBeforeOnNavigate) {
  // This test should only run with --site-per-process.
  if (!AreAllSitesIsolatedForTesting())
    return;

  LoadHTML("Hello <iframe src='data:text/html,frame 1'></iframe>");
  WebFrame* web_frame = frame()->GetWebFrame();
  TestRenderFrame* child_frame = static_cast<TestRenderFrame*>(
      RenderFrame::FromWebFrame(web_frame->firstChild()));

  // Swap the child frame out.
  FrameReplicationState replication_state =
      ReconstructReplicationStateForTesting(child_frame);
  child_frame->SwapOut(kProxyRoutingId, true, replication_state);
  EXPECT_TRUE(web_frame->firstChild()->isWebRemoteFrame());

  // Do the first step of a remote-to-local transition for the child proxy,
  // which is to create a provisional local frame.
  int routing_id = kProxyRoutingId + 1;
  FrameMsg_NewFrame_WidgetParams widget_params;
  widget_params.routing_id = MSG_ROUTING_NONE;
  widget_params.hidden = false;
  RenderFrameImpl::CreateFrame(routing_id, kProxyRoutingId, MSG_ROUTING_NONE,
                               frame()->GetRoutingID(), MSG_ROUTING_NONE,
                               replication_state, nullptr, widget_params,
                               blink::WebFrameOwnerProperties());
  TestRenderFrame* provisional_frame =
      static_cast<TestRenderFrame*>(RenderFrameImpl::FromRoutingID(routing_id));
  EXPECT_TRUE(provisional_frame);

  // Detach the child frame (currently remote) in the main frame.
  ExecuteJavaScriptForTests(
      "document.body.removeChild(document.querySelector('iframe'));");
  RenderFrameProxy* child_proxy =
      RenderFrameProxy::FromRoutingID(kProxyRoutingId);
  EXPECT_FALSE(child_proxy);

  // Attempt to start a navigation on the RenderFrame that was created to
  // replace the now-detached RenderFrameProxy.   This shouldn't crash and
  // should abort the navigation, since the frame no longer exists.
  CommonNavigationParams common_params;
  common_params.navigation_type = FrameMsg_Navigate_Type::NORMAL;
  common_params.url = GURL(url::kAboutBlankURL);
  provisional_frame->Navigate(common_params, StartNavigationParams(),
                              RequestNavigationParams());
  ProcessPendingMessages();

  // Check that there was no DidCommitProvisionalLoad.
  const IPC::Message* frame_navigate_msg =
      render_thread_->sink().GetUniqueMessageMatching(
          FrameHostMsg_DidCommitProvisionalLoad::ID);
  EXPECT_FALSE(frame_navigate_msg);

  // Detach the provisional frame to clean it up.  Normally, the browser
  // process would trigger this via FrameMsg_Delete.
  provisional_frame->GetWebFrame()->detach();
}

// Verify that DidFlushPaint doesn't crash if called after a RenderView is
// swapped out. See https://crbug.com/513552.
TEST_F(RenderViewImplTest, PaintAfterSwapOut) {
  // Create a new main frame RenderFrame so that we don't interfere with the
  // shutdown of frame() in RenderViewTest.TearDown.
  blink::WebURLRequest popup_request(GURL("http://foo.com"));
  blink::WebView* new_web_view = view()->createView(
      GetMainFrame(), popup_request, blink::WebWindowFeatures(), "foo",
      blink::WebNavigationPolicyNewForegroundTab, false);
  RenderViewImpl* new_view = RenderViewImpl::FromWebView(new_web_view);

  // Respond to a swap out request.
  TestRenderFrame* new_main_frame =
      static_cast<TestRenderFrame*>(new_view->GetMainRenderFrame());
  new_main_frame->SwapOut(
      kProxyRoutingId, true,
      ReconstructReplicationStateForTesting(new_main_frame));

  // Simulate getting painted after swapping out.
  new_view->DidFlushPaint();

  new_view->Close();
  new_view->Release();
}

// Verify that the renderer process doesn't crash when device scale factor
// changes after a cross-process navigation has commited.
// See https://crbug.com/571603.
TEST_F(RenderViewImplTest, SetZoomLevelAfterCrossProcessNavigation) {
  // This test should only run with out-of-process iframes enabled.
  if (!SiteIsolationPolicy::AreCrossProcessFramesPossible())
    return;

  // The bug reproduces if zoom is used for devices scale factor.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableUseZoomForDSF);

  LoadHTML("Hello world!");

  // Swap the main frame out after which it should become a WebRemoteFrame.
  TestRenderFrame* main_frame =
      static_cast<TestRenderFrame*>(view()->GetMainRenderFrame());
  main_frame->SwapOut(kProxyRoutingId, true,
                      ReconstructReplicationStateForTesting(main_frame));
  EXPECT_TRUE(view()->webview()->mainFrame()->isWebRemoteFrame());

  // This should not cause a crash.
  view()->OnDeviceScaleFactorChanged();
}

// Test that we get the correct UpdateState message when we go back twice
// quickly without committing.  Regression test for http://crbug.com/58082.
// Disabled: http://crbug.com/157357 .
TEST_F(RenderViewImplTest,  DISABLED_LastCommittedUpdateState) {
  // Load page A.
  LoadHTML("<div>Page A</div>");

  // Load page B, which will trigger an UpdateState message for page A.
  LoadHTML("<div>Page B</div>");

  // Check for a valid UpdateState message for page A.
  ProcessPendingMessages();
  const IPC::Message* msg_A = render_thread_->sink().GetUniqueMessageMatching(
      ViewHostMsg_UpdateState::ID);
  ASSERT_TRUE(msg_A);
  ViewHostMsg_UpdateState::Param param;
  ViewHostMsg_UpdateState::Read(msg_A, &param);
  int page_id_A = std::get<0>(param);
  PageState state_A = std::get<1>(param);
  EXPECT_EQ(1, page_id_A);
  render_thread_->sink().ClearMessages();

  // Load page C, which will trigger an UpdateState message for page B.
  LoadHTML("<div>Page C</div>");

  // Check for a valid UpdateState for page B.
  ProcessPendingMessages();
  const IPC::Message* msg_B = render_thread_->sink().GetUniqueMessageMatching(
      ViewHostMsg_UpdateState::ID);
  ASSERT_TRUE(msg_B);
  ViewHostMsg_UpdateState::Read(msg_B, &param);
  int page_id_B = std::get<0>(param);
  PageState state_B = std::get<1>(param);
  EXPECT_EQ(2, page_id_B);
  EXPECT_NE(state_A, state_B);
  render_thread_->sink().ClearMessages();

  // Load page D, which will trigger an UpdateState message for page C.
  LoadHTML("<div>Page D</div>");

  // Check for a valid UpdateState for page C.
  ProcessPendingMessages();
  const IPC::Message* msg_C = render_thread_->sink().GetUniqueMessageMatching(
      ViewHostMsg_UpdateState::ID);
  ASSERT_TRUE(msg_C);
  ViewHostMsg_UpdateState::Read(msg_C, &param);
  int page_id_C = std::get<0>(param);
  PageState state_C = std::get<1>(param);
  EXPECT_EQ(3, page_id_C);
  EXPECT_NE(state_B, state_C);
  render_thread_->sink().ClearMessages();

  // Go back to C and commit, preparing for our real test.
  CommonNavigationParams common_params_C;
  RequestNavigationParams request_params_C;
  common_params_C.navigation_type = FrameMsg_Navigate_Type::NORMAL;
  common_params_C.transition = ui::PAGE_TRANSITION_FORWARD_BACK;
  request_params_C.current_history_list_length = 4;
  request_params_C.current_history_list_offset = 3;
  request_params_C.pending_history_list_offset = 2;
  request_params_C.page_id = 3;
  request_params_C.page_state = state_C;
  frame()->Navigate(common_params_C, StartNavigationParams(), request_params_C);
  ProcessPendingMessages();
  render_thread_->sink().ClearMessages();

  // Go back twice quickly, such that page B does not have a chance to commit.
  // This leads to two changes to the back/forward list but only one change to
  // the RenderView's page ID.

  // Back to page B (page_id 2), without committing.
  CommonNavigationParams common_params_B;
  RequestNavigationParams request_params_B;
  common_params_B.navigation_type = FrameMsg_Navigate_Type::NORMAL;
  common_params_B.transition = ui::PAGE_TRANSITION_FORWARD_BACK;
  request_params_B.current_history_list_length = 4;
  request_params_B.current_history_list_offset = 2;
  request_params_B.pending_history_list_offset = 1;
  request_params_B.page_id = 2;
  request_params_B.page_state = state_B;
  frame()->Navigate(common_params_B, StartNavigationParams(), request_params_B);

  // Back to page A (page_id 1) and commit.
  CommonNavigationParams common_params;
  RequestNavigationParams request_params;
  common_params.navigation_type = FrameMsg_Navigate_Type::NORMAL;
  common_params.transition = ui::PAGE_TRANSITION_FORWARD_BACK;
  request_params.current_history_list_length = 4;
  request_params.current_history_list_offset = 2;
  request_params.pending_history_list_offset = 0;
  request_params.page_id = 1;
  request_params.page_state = state_A;
  frame()->Navigate(common_params, StartNavigationParams(), request_params);
  ProcessPendingMessages();

  // Now ensure that the UpdateState message we receive is consistent
  // and represents page C in both page_id and state.
  const IPC::Message* msg = render_thread_->sink().GetUniqueMessageMatching(
      ViewHostMsg_UpdateState::ID);
  ASSERT_TRUE(msg);
  ViewHostMsg_UpdateState::Read(msg, &param);
  int page_id = std::get<0>(param);
  PageState state = std::get<1>(param);
  EXPECT_EQ(page_id_C, page_id);
  EXPECT_NE(state_A, state);
  EXPECT_NE(state_B, state);
  EXPECT_EQ(state_C, state);
}

// Test that our IME backend sends a notification message when the input focus
// changes.
TEST_F(RenderViewImplTest, OnImeTypeChanged) {
  // Load an HTML page consisting of two input fields.
  LoadHTML("<html>"
           "<head>"
           "</head>"
           "<body>"
           "<input id=\"test1\" type=\"text\" value=\"some text\"></input>"
           "<input id=\"test2\" type=\"password\"></input>"
           "<input id=\"test3\" type=\"text\" inputmode=\"verbatim\"></input>"
           "<input id=\"test4\" type=\"text\" inputmode=\"latin\"></input>"
           "<input id=\"test5\" type=\"text\" inputmode=\"latin-name\"></input>"
           "<input id=\"test6\" type=\"text\" inputmode=\"latin-prose\">"
               "</input>"
           "<input id=\"test7\" type=\"text\" inputmode=\"full-width-latin\">"
               "</input>"
           "<input id=\"test8\" type=\"text\" inputmode=\"kana\"></input>"
           "<input id=\"test9\" type=\"text\" inputmode=\"katakana\"></input>"
           "<input id=\"test10\" type=\"text\" inputmode=\"numeric\"></input>"
           "<input id=\"test11\" type=\"text\" inputmode=\"tel\"></input>"
           "<input id=\"test12\" type=\"text\" inputmode=\"email\"></input>"
           "<input id=\"test13\" type=\"text\" inputmode=\"url\"></input>"
           "<input id=\"test14\" type=\"text\" inputmode=\"unknown\"></input>"
           "<input id=\"test15\" type=\"text\" inputmode=\"verbatim\"></input>"
           "</body>"
           "</html>");
  render_thread_->sink().ClearMessages();

  struct InputModeTestCase {
    const char* input_id;
    ui::TextInputMode expected_mode;
  };
  static const InputModeTestCase kInputModeTestCases[] = {
     {"test1", ui::TEXT_INPUT_MODE_DEFAULT},
     {"test3", ui::TEXT_INPUT_MODE_VERBATIM},
     {"test4", ui::TEXT_INPUT_MODE_LATIN},
     {"test5", ui::TEXT_INPUT_MODE_LATIN_NAME},
     {"test6", ui::TEXT_INPUT_MODE_LATIN_PROSE},
     {"test7", ui::TEXT_INPUT_MODE_FULL_WIDTH_LATIN},
     {"test8", ui::TEXT_INPUT_MODE_KANA},
     {"test9", ui::TEXT_INPUT_MODE_KATAKANA},
     {"test10", ui::TEXT_INPUT_MODE_NUMERIC},
     {"test11", ui::TEXT_INPUT_MODE_TEL},
     {"test12", ui::TEXT_INPUT_MODE_EMAIL},
     {"test13", ui::TEXT_INPUT_MODE_URL},
     {"test14", ui::TEXT_INPUT_MODE_DEFAULT},
     {"test15", ui::TEXT_INPUT_MODE_VERBATIM},
  };

  const int kRepeatCount = 10;
  for (int i = 0; i < kRepeatCount; i++) {
    // Move the input focus to the first <input> element, where we should
    // activate IMEs.
    ExecuteJavaScriptForTests("document.getElementById('test1').focus();");
    ProcessPendingMessages();
    render_thread_->sink().ClearMessages();

    // Update the IME status and verify if our IME backend sends an IPC message
    // to activate IMEs.
    view()->UpdateTextInputState(ShowIme::HIDE_IME, ChangeSource::FROM_NON_IME);
    const IPC::Message* msg = render_thread_->sink().GetMessageAt(0);
    EXPECT_TRUE(msg != NULL);
    EXPECT_EQ(ViewHostMsg_TextInputStateChanged::ID, msg->type());
    ViewHostMsg_TextInputStateChanged::Param params;
    ViewHostMsg_TextInputStateChanged::Read(msg, &params);
    TextInputState p = std::get<0>(params);
    ui::TextInputType type = p.type;
    ui::TextInputMode input_mode = p.mode;
    bool can_compose_inline = p.can_compose_inline;
    EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT, type);
    EXPECT_EQ(true, can_compose_inline);

    // Move the input focus to the second <input> element, where we should
    // de-activate IMEs.
    ExecuteJavaScriptForTests("document.getElementById('test2').focus();");
    ProcessPendingMessages();
    render_thread_->sink().ClearMessages();

    // Update the IME status and verify if our IME backend sends an IPC message
    // to de-activate IMEs.
    view()->UpdateTextInputState(ShowIme::HIDE_IME, ChangeSource::FROM_NON_IME);
    msg = render_thread_->sink().GetMessageAt(0);
    EXPECT_TRUE(msg != NULL);
    EXPECT_EQ(ViewHostMsg_TextInputStateChanged::ID, msg->type());
    ViewHostMsg_TextInputStateChanged::Read(msg, &params);
    p = std::get<0>(params);
    type = p.type;
    input_mode = p.mode;
    EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD, type);

    for (size_t i = 0; i < arraysize(kInputModeTestCases); i++) {
      const InputModeTestCase* test_case = &kInputModeTestCases[i];
      std::string javascript =
          base::StringPrintf("document.getElementById('%s').focus();",
                             test_case->input_id);
      // Move the input focus to the target <input> element, where we should
      // activate IMEs.
      ExecuteJavaScriptAndReturnIntValue(base::ASCIIToUTF16(javascript), NULL);
      ProcessPendingMessages();
      render_thread_->sink().ClearMessages();

      // Update the IME status and verify if our IME backend sends an IPC
      // message to activate IMEs.
      view()->UpdateTextInputState(ShowIme::HIDE_IME,
                                   ChangeSource::FROM_NON_IME);
      ProcessPendingMessages();
      const IPC::Message* msg = render_thread_->sink().GetMessageAt(0);
      EXPECT_TRUE(msg != NULL);
      EXPECT_EQ(ViewHostMsg_TextInputStateChanged::ID, msg->type());
      ViewHostMsg_TextInputStateChanged::Read(msg, &params);
      p = std::get<0>(params);
      type = p.type;
      input_mode = p.mode;
      EXPECT_EQ(test_case->expected_mode, input_mode);
    }
  }
}

// Test that our IME backend can compose CJK words.
// Our IME front-end sends many platform-independent messages to the IME backend
// while it composes CJK words. This test sends the minimal messages captured
// on my local environment directly to the IME backend to verify if the backend
// can compose CJK words without any problems.
// This test uses an array of command sets because an IME composotion does not
// only depends on IME events, but also depends on window events, e.g. moving
// the window focus while composing a CJK text. To handle such complicated
// cases, this test should not only call IME-related functions in the
// RenderWidget class, but also call some RenderWidget members, e.g.
// ExecuteJavaScriptForTests(), RenderWidget::OnSetFocus(), etc.
TEST_F(RenderViewImplTest, ImeComposition) {
  enum ImeCommand {
    IME_INITIALIZE,
    IME_SETINPUTMODE,
    IME_SETFOCUS,
    IME_SETCOMPOSITION,
    IME_CONFIRMCOMPOSITION,
    IME_CANCELCOMPOSITION
  };
  struct ImeMessage {
    ImeCommand command;
    bool enable;
    int selection_start;
    int selection_end;
    const wchar_t* ime_string;
    const wchar_t* result;
  };
  static const ImeMessage kImeMessages[] = {
    // Scenario 1: input a Chinese word with Microsoft IME (on Vista).
    {IME_INITIALIZE, true, 0, 0, NULL, NULL},
    {IME_SETINPUTMODE, true, 0, 0, NULL, NULL},
    {IME_SETFOCUS, true, 0, 0, NULL, NULL},
    {IME_SETCOMPOSITION, false, 1, 1, L"n", L"n"},
    {IME_SETCOMPOSITION, false, 2, 2, L"ni", L"ni"},
    {IME_SETCOMPOSITION, false, 3, 3, L"nih", L"nih"},
    {IME_SETCOMPOSITION, false, 4, 4, L"niha", L"niha"},
    {IME_SETCOMPOSITION, false, 5, 5, L"nihao", L"nihao"},
    {IME_CONFIRMCOMPOSITION, false, -1, -1, L"\x4F60\x597D", L"\x4F60\x597D"},
    // Scenario 2: input a Japanese word with Microsoft IME (on Vista).
    {IME_INITIALIZE, true, 0, 0, NULL, NULL},
    {IME_SETINPUTMODE, true, 0, 0, NULL, NULL},
    {IME_SETFOCUS, true, 0, 0, NULL, NULL},
    {IME_SETCOMPOSITION, false, 0, 1, L"\xFF4B", L"\xFF4B"},
    {IME_SETCOMPOSITION, false, 0, 1, L"\x304B", L"\x304B"},
    {IME_SETCOMPOSITION, false, 0, 2, L"\x304B\xFF4E", L"\x304B\xFF4E"},
    {IME_SETCOMPOSITION, false, 0, 3, L"\x304B\x3093\xFF4A",
     L"\x304B\x3093\xFF4A"},
    {IME_SETCOMPOSITION, false, 0, 3, L"\x304B\x3093\x3058",
     L"\x304B\x3093\x3058"},
    {IME_SETCOMPOSITION, false, 0, 2, L"\x611F\x3058", L"\x611F\x3058"},
    {IME_SETCOMPOSITION, false, 0, 2, L"\x6F22\x5B57", L"\x6F22\x5B57"},
    {IME_CONFIRMCOMPOSITION, false, -1, -1, L"", L"\x6F22\x5B57"},
    {IME_CANCELCOMPOSITION, false, -1, -1, L"", L"\x6F22\x5B57"},
    // Scenario 3: input a Korean word with Microsot IME (on Vista).
    {IME_INITIALIZE, true, 0, 0, NULL, NULL},
    {IME_SETINPUTMODE, true, 0, 0, NULL, NULL},
    {IME_SETFOCUS, true, 0, 0, NULL, NULL},
    {IME_SETCOMPOSITION, false, 0, 1, L"\x3147", L"\x3147"},
    {IME_SETCOMPOSITION, false, 0, 1, L"\xC544", L"\xC544"},
    {IME_SETCOMPOSITION, false, 0, 1, L"\xC548", L"\xC548"},
    {IME_CONFIRMCOMPOSITION, false, -1, -1, L"", L"\xC548"},
    {IME_SETCOMPOSITION, false, 0, 1, L"\x3134", L"\xC548\x3134"},
    {IME_SETCOMPOSITION, false, 0, 1, L"\xB140", L"\xC548\xB140"},
    {IME_SETCOMPOSITION, false, 0, 1, L"\xB155", L"\xC548\xB155"},
    {IME_CANCELCOMPOSITION, false, -1, -1, L"", L"\xC548"},
    {IME_SETCOMPOSITION, false, 0, 1, L"\xB155", L"\xC548\xB155"},
    {IME_CONFIRMCOMPOSITION, false, -1, -1, L"", L"\xC548\xB155"},
  };

  for (size_t i = 0; i < arraysize(kImeMessages); i++) {
    const ImeMessage* ime_message = &kImeMessages[i];
    switch (ime_message->command) {
      case IME_INITIALIZE:
        // Load an HTML page consisting of a content-editable <div> element,
        // and move the input focus to the <div> element, where we can use
        // IMEs.
        LoadHTML("<html>"
                "<head>"
                "</head>"
                "<body>"
                "<div id=\"test1\" contenteditable=\"true\"></div>"
                "</body>"
                "</html>");
        ExecuteJavaScriptForTests("document.getElementById('test1').focus();");
        break;

      case IME_SETINPUTMODE:
        break;

      case IME_SETFOCUS:
        // Update the window focus.
        view()->OnSetFocus(ime_message->enable);
        break;

      case IME_SETCOMPOSITION:
        view()->OnImeSetComposition(
            base::WideToUTF16(ime_message->ime_string),
            std::vector<blink::WebCompositionUnderline>(),
            gfx::Range::InvalidRange(),
            ime_message->selection_start,
            ime_message->selection_end);
        break;

      case IME_CONFIRMCOMPOSITION:
        view()->OnImeConfirmComposition(
            base::WideToUTF16(ime_message->ime_string),
            gfx::Range::InvalidRange(),
            false);
        break;

      case IME_CANCELCOMPOSITION:
        view()->OnImeSetComposition(
            base::string16(),
            std::vector<blink::WebCompositionUnderline>(),
            gfx::Range::InvalidRange(),
            0, 0);
        break;
    }

    // Update the status of our IME back-end.
    // TODO(hbono): we should verify messages to be sent from the back-end.
    view()->UpdateTextInputState(ShowIme::HIDE_IME, ChangeSource::FROM_NON_IME);
    ProcessPendingMessages();
    render_thread_->sink().ClearMessages();

    if (ime_message->result) {
      // Retrieve the content of this page and compare it with the expected
      // result.
      const int kMaxOutputCharacters = 128;
      base::string16 output = WebFrameContentDumper::dumpWebViewAsText(
          view()->GetWebView(), kMaxOutputCharacters);
      EXPECT_EQ(base::WideToUTF16(ime_message->result), output);
    }
  }
}

// Test that the RenderView::OnSetTextDirection() function can change the text
// direction of the selected input element.
TEST_F(RenderViewImplTest, OnSetTextDirection) {
  // Load an HTML page consisting of a <textarea> element and a <div> element.
  // This test changes the text direction of the <textarea> element, and
  // writes the values of its 'dir' attribute and its 'direction' property to
  // verify that the text direction is changed.
  LoadHTML("<html>"
           "<head>"
           "</head>"
           "<body>"
           "<textarea id=\"test\"></textarea>"
           "<div id=\"result\" contenteditable=\"true\"></div>"
           "</body>"
           "</html>");
  render_thread_->sink().ClearMessages();

  static const struct {
    WebTextDirection direction;
    const wchar_t* expected_result;
  } kTextDirection[] = {
    { blink::WebTextDirectionRightToLeft, L"\x000A" L"rtl,rtl" },
    { blink::WebTextDirectionLeftToRight, L"\x000A" L"ltr,ltr" },
  };
  for (size_t i = 0; i < arraysize(kTextDirection); ++i) {
    // Set the text direction of the <textarea> element.
    ExecuteJavaScriptForTests("document.getElementById('test').focus();");
    view()->OnSetTextDirection(kTextDirection[i].direction);

    // Write the values of its DOM 'dir' attribute and its CSS 'direction'
    // property to the <div> element.
    ExecuteJavaScriptForTests(
        "var result = document.getElementById('result');"
        "var node = document.getElementById('test');"
        "var style = getComputedStyle(node, null);"
        "result.innerText ="
        "    node.getAttribute('dir') + ',' +"
        "    style.getPropertyValue('direction');");

    // Copy the document content to std::wstring and compare with the
    // expected result.
    const int kMaxOutputCharacters = 16;
    base::string16 output = WebFrameContentDumper::dumpWebViewAsText(
        view()->GetWebView(), kMaxOutputCharacters);
    EXPECT_EQ(base::WideToUTF16(kTextDirection[i].expected_result), output);
  }
}

// Crashy, http://crbug.com/53247.
TEST_F(RenderViewImplTest, DISABLED_DidFailProvisionalLoadWithErrorForError) {
  GetMainFrame()->enableViewSourceMode(true);
  WebURLError error;
  error.domain = WebString::fromUTF8(net::kErrorDomain);
  error.reason = net::ERR_FILE_NOT_FOUND;
  error.unreachableURL = GURL("http://foo");
  WebLocalFrame* web_frame = GetMainFrame();

  // Start a load that will reach provisional state synchronously,
  // but won't complete synchronously.
  CommonNavigationParams common_params;
  common_params.navigation_type = FrameMsg_Navigate_Type::NORMAL;
  common_params.url = GURL("data:text/html,test data");
  frame()->Navigate(common_params, StartNavigationParams(),
                    RequestNavigationParams());

  // An error occurred.
  view()->GetMainRenderFrame()->didFailProvisionalLoad(
      web_frame, error, blink::WebStandardCommit);
  // Frame should exit view-source mode.
  EXPECT_FALSE(web_frame->isViewSourceModeEnabled());
}

TEST_F(RenderViewImplTest, DidFailProvisionalLoadWithErrorForCancellation) {
  GetMainFrame()->enableViewSourceMode(true);
  WebURLError error;
  error.domain = WebString::fromUTF8(net::kErrorDomain);
  error.reason = net::ERR_ABORTED;
  error.unreachableURL = GURL("http://foo");
  WebLocalFrame* web_frame = GetMainFrame();

  // Start a load that will reach provisional state synchronously,
  // but won't complete synchronously.
  CommonNavigationParams common_params;
  common_params.navigation_type = FrameMsg_Navigate_Type::NORMAL;
  common_params.url = GURL("data:text/html,test data");
  frame()->Navigate(common_params, StartNavigationParams(),
                    RequestNavigationParams());

  // A cancellation occurred.
  view()->GetMainRenderFrame()->didFailProvisionalLoad(
      web_frame, error, blink::WebStandardCommit);
  // Frame should stay in view-source mode.
  EXPECT_TRUE(web_frame->isViewSourceModeEnabled());
}

// Regression test for http://crbug.com/41562
TEST_F(RenderViewImplTest, UpdateTargetURLWithInvalidURL) {
  const GURL invalid_gurl("http://");
  view()->setMouseOverURL(blink::WebURL(invalid_gurl));
  EXPECT_EQ(invalid_gurl, view()->target_url_);
}

TEST_F(RenderViewImplTest, SetHistoryLengthAndOffset) {
  // No history to merge; one committed page.
  view()->OnSetHistoryOffsetAndLength(0, 1);
  EXPECT_EQ(1, view()->history_list_length_);
  EXPECT_EQ(0, view()->history_list_offset_);

  // History of length 1 to merge; one committed page.
  view()->OnSetHistoryOffsetAndLength(1, 2);
  EXPECT_EQ(2, view()->history_list_length_);
  EXPECT_EQ(1, view()->history_list_offset_);
}

TEST_F(RenderViewImplTest, ContextMenu) {
  LoadHTML("<div>Page A</div>");

  // Create a right click in the center of the iframe. (I'm hoping this will
  // make this a bit more robust in case of some other formatting or other bug.)
  WebMouseEvent mouse_event;
  mouse_event.type = WebInputEvent::MouseDown;
  mouse_event.button = WebMouseEvent::ButtonRight;
  mouse_event.x = 250;
  mouse_event.y = 250;
  mouse_event.globalX = 250;
  mouse_event.globalY = 250;

  SendWebMouseEvent(mouse_event);

  // Now simulate the corresponding up event which should display the menu
  mouse_event.type = WebInputEvent::MouseUp;
  SendWebMouseEvent(mouse_event);

  EXPECT_TRUE(render_thread_->sink().GetUniqueMessageMatching(
      FrameHostMsg_ContextMenu::ID));
}

TEST_F(RenderViewImplTest, TestBackForward) {
  LoadHTML("<div id=pagename>Page A</div>");
  PageState page_a_state = GetCurrentPageState();
  int was_page_a = -1;
  base::string16 check_page_a =
      base::ASCIIToUTF16(
          "Number(document.getElementById('pagename').innerHTML == 'Page A')");
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(check_page_a, &was_page_a));
  EXPECT_EQ(1, was_page_a);

  LoadHTML("<div id=pagename>Page B</div>");
  int was_page_b = -1;
  base::string16 check_page_b =
      base::ASCIIToUTF16(
          "Number(document.getElementById('pagename').innerHTML == 'Page B')");
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(check_page_b, &was_page_b));
  EXPECT_EQ(1, was_page_b);

  PageState back_state = GetCurrentPageState();

  LoadHTML("<div id=pagename>Page C</div>");
  int was_page_c = -1;
  base::string16 check_page_c =
      base::ASCIIToUTF16(
          "Number(document.getElementById('pagename').innerHTML == 'Page C')");
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(check_page_c, &was_page_c));
  EXPECT_EQ(1, was_page_c);

  PageState forward_state = GetCurrentPageState();

  // Go back.
  GoBack(GURL("data:text/html;charset=utf-8,<div id=pagename>Page B</div>"),
         back_state);

  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(check_page_b, &was_page_b));
  EXPECT_EQ(1, was_page_b);
  PageState back_state2 = GetCurrentPageState();

  // Go forward.
  GoForward(GURL("data:text/html;charset=utf-8,<div id=pagename>Page C</div>"),
            forward_state);
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(check_page_c, &was_page_c));
  EXPECT_EQ(1, was_page_c);

  // Go back.
  GoBack(GURL("data:text/html;charset=utf-8,<div id=pagename>Page B</div>"),
         back_state2);
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(check_page_b, &was_page_b));
  EXPECT_EQ(1, was_page_b);

  forward_state = GetCurrentPageState();

  // Go back.
  GoBack(GURL("data:text/html;charset=utf-8,<div id=pagename>Page A</div>"),
         page_a_state);
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(check_page_a, &was_page_a));
  EXPECT_EQ(1, was_page_a);

  // Go forward.
  GoForward(GURL("data:text/html;charset=utf-8,<div id=pagename>Page B</div>"),
            forward_state);
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(check_page_b, &was_page_b));
  EXPECT_EQ(1, was_page_b);
}

#if defined(OS_MACOSX) || defined(USE_AURA)
TEST_F(RenderViewImplTest, GetCompositionCharacterBoundsTest) {
#if defined(OS_WIN)
  // http://crbug.com/304193
  if (base::win::GetVersion() < base::win::VERSION_VISTA)
    return;
#endif

  LoadHTML("<textarea id=\"test\" cols=\"100\"></textarea>");
  ExecuteJavaScriptForTests("document.getElementById('test').focus();");

  const base::string16 empty_string;
  const std::vector<blink::WebCompositionUnderline> empty_underline;
  std::vector<gfx::Rect> bounds;
  view()->OnSetFocus(true);

  // ASCII composition
  const base::string16 ascii_composition = base::UTF8ToUTF16("aiueo");
  view()->OnImeSetComposition(ascii_composition, empty_underline,
                              gfx::Range::InvalidRange(), 0, 0);
  view()->GetCompositionCharacterBounds(&bounds);
  ASSERT_EQ(ascii_composition.size(), bounds.size());

  for (size_t i = 0; i < bounds.size(); ++i)
    EXPECT_LT(0, bounds[i].width());
  view()->OnImeConfirmComposition(
      empty_string, gfx::Range::InvalidRange(), false);

  // Non surrogate pair unicode character.
  const base::string16 unicode_composition = base::UTF8ToUTF16(
      "\xE3\x81\x82\xE3\x81\x84\xE3\x81\x86\xE3\x81\x88\xE3\x81\x8A");
  view()->OnImeSetComposition(unicode_composition, empty_underline,
                              gfx::Range::InvalidRange(), 0, 0);
  view()->GetCompositionCharacterBounds(&bounds);
  ASSERT_EQ(unicode_composition.size(), bounds.size());
  for (size_t i = 0; i < bounds.size(); ++i)
    EXPECT_LT(0, bounds[i].width());
  view()->OnImeConfirmComposition(
      empty_string, gfx::Range::InvalidRange(), false);

  // Surrogate pair character.
  const base::string16 surrogate_pair_char =
      base::UTF8ToUTF16("\xF0\xA0\xAE\x9F");
  view()->OnImeSetComposition(surrogate_pair_char,
                              empty_underline,
                              gfx::Range::InvalidRange(),
                              0,
                              0);
  view()->GetCompositionCharacterBounds(&bounds);
  ASSERT_EQ(surrogate_pair_char.size(), bounds.size());
  EXPECT_LT(0, bounds[0].width());
  EXPECT_EQ(0, bounds[1].width());
  view()->OnImeConfirmComposition(
      empty_string, gfx::Range::InvalidRange(), false);

  // Mixed string.
  const base::string16 surrogate_pair_mixed_composition =
      surrogate_pair_char + base::UTF8ToUTF16("\xE3\x81\x82") +
      surrogate_pair_char + base::UTF8ToUTF16("b") + surrogate_pair_char;
  const size_t utf16_length = 8UL;
  const bool is_surrogate_pair_empty_rect[8] = {
    false, true, false, false, true, false, false, true };
  view()->OnImeSetComposition(surrogate_pair_mixed_composition,
                              empty_underline,
                              gfx::Range::InvalidRange(),
                              0,
                              0);
  view()->GetCompositionCharacterBounds(&bounds);
  ASSERT_EQ(utf16_length, bounds.size());
  for (size_t i = 0; i < utf16_length; ++i) {
    if (is_surrogate_pair_empty_rect[i]) {
      EXPECT_EQ(0, bounds[i].width());
    } else {
      EXPECT_LT(0, bounds[i].width());
    }
  }
  view()->OnImeConfirmComposition(
      empty_string, gfx::Range::InvalidRange(), false);
}
#endif

TEST_F(RenderViewImplTest, ZoomLimit) {
  const double kMinZoomLevel = ZoomFactorToZoomLevel(kMinimumZoomFactor);
  const double kMaxZoomLevel = ZoomFactorToZoomLevel(kMaximumZoomFactor);

  // Verifies navigation to a URL with preset zoom level indeed sets the level.
  // Regression test for http://crbug.com/139559, where the level was not
  // properly set when it is out of the default zoom limits of WebView.
  CommonNavigationParams common_params;
  common_params.url = GURL("data:text/html,min_zoomlimit_test");
  view()->OnSetZoomLevelForLoadingURL(common_params.url, kMinZoomLevel);
  frame()->Navigate(common_params, StartNavigationParams(),
                    RequestNavigationParams());
  ProcessPendingMessages();
  EXPECT_DOUBLE_EQ(kMinZoomLevel, view()->GetWebView()->zoomLevel());

  // It should work even when the zoom limit is temporarily changed in the page.
  view()->GetWebView()->zoomLimitsChanged(ZoomFactorToZoomLevel(1.0),
                                          ZoomFactorToZoomLevel(1.0));
  common_params.url = GURL("data:text/html,max_zoomlimit_test");
  view()->OnSetZoomLevelForLoadingURL(common_params.url, kMaxZoomLevel);
  frame()->Navigate(common_params, StartNavigationParams(),
                    RequestNavigationParams());
  ProcessPendingMessages();
  EXPECT_DOUBLE_EQ(kMaxZoomLevel, view()->GetWebView()->zoomLevel());
}

TEST_F(RenderViewImplTest, SetEditableSelectionAndComposition) {
  // Load an HTML page consisting of an input field.
  LoadHTML("<html>"
           "<head>"
           "</head>"
           "<body>"
           "<input id=\"test1\" value=\"some test text hello\"></input>"
           "</body>"
           "</html>");
  ExecuteJavaScriptForTests("document.getElementById('test1').focus();");
  frame()->SetEditableSelectionOffsets(4, 8);
  const std::vector<blink::WebCompositionUnderline> empty_underline;
  frame()->SetCompositionFromExistingText(7, 10, empty_underline);
  blink::WebTextInputInfo info = view()->webview()->textInputInfo();
  EXPECT_EQ(4, info.selectionStart);
  EXPECT_EQ(8, info.selectionEnd);
  EXPECT_EQ(7, info.compositionStart);
  EXPECT_EQ(10, info.compositionEnd);
  frame()->Unselect();
  info = view()->webview()->textInputInfo();
  EXPECT_EQ(0, info.selectionStart);
  EXPECT_EQ(0, info.selectionEnd);
}


TEST_F(RenderViewImplTest, OnExtendSelectionAndDelete) {
  // Load an HTML page consisting of an input field.
  LoadHTML("<html>"
           "<head>"
           "</head>"
           "<body>"
           "<input id=\"test1\" value=\"abcdefghijklmnopqrstuvwxyz\"></input>"
           "</body>"
           "</html>");
  ExecuteJavaScriptForTests("document.getElementById('test1').focus();");
  frame()->SetEditableSelectionOffsets(10, 10);
  frame()->ExtendSelectionAndDelete(3, 4);
  blink::WebTextInputInfo info = view()->webview()->textInputInfo();
  EXPECT_EQ("abcdefgopqrstuvwxyz", info.value);
  EXPECT_EQ(7, info.selectionStart);
  EXPECT_EQ(7, info.selectionEnd);
  frame()->SetEditableSelectionOffsets(4, 8);
  frame()->ExtendSelectionAndDelete(2, 5);
  info = view()->webview()->textInputInfo();
  EXPECT_EQ("abuvwxyz", info.value);
  EXPECT_EQ(2, info.selectionStart);
  EXPECT_EQ(2, info.selectionEnd);
}

// Test that the navigating specific frames works correctly.
TEST_F(RenderViewImplTest, NavigateSubframe) {
  // Load page A.
  LoadHTML("hello <iframe srcdoc='fail' name='frame'></iframe>");

  // Navigate the frame only.
  CommonNavigationParams common_params;
  RequestNavigationParams request_params;
  common_params.url = GURL("data:text/html,world");
  common_params.navigation_type = FrameMsg_Navigate_Type::NORMAL;
  common_params.transition = ui::PAGE_TRANSITION_TYPED;
  common_params.navigation_start = base::TimeTicks::FromInternalValue(1);
  request_params.current_history_list_length = 1;
  request_params.current_history_list_offset = 0;
  request_params.pending_history_list_offset = 1;
  request_params.page_id = -1;

  TestRenderFrame* subframe =
      static_cast<TestRenderFrame*>(RenderFrameImpl::FromWebFrame(
          view()->webview()->findFrameByName("frame")));
  subframe->Navigate(common_params, StartNavigationParams(), request_params);
  FrameLoadWaiter(subframe).Wait();

  // Copy the document content to std::wstring and compare with the
  // expected result.
  const int kMaxOutputCharacters = 256;
  std::string output = base::UTF16ToUTF8(
      base::StringPiece16(WebFrameContentDumper::dumpWebViewAsText(
          view()->GetWebView(), kMaxOutputCharacters)));
  EXPECT_EQ(output, "hello \n\nworld");
}

// This test ensures that a RenderFrame object is created for the top level
// frame in the RenderView.
TEST_F(RenderViewImplTest, BasicRenderFrame) {
  EXPECT_TRUE(view()->main_render_frame_);
}

TEST_F(RenderViewImplTest, GetSSLStatusOfFrame) {
  LoadHTML("<!DOCTYPE html><html><body></body></html>");

  WebLocalFrame* frame = GetMainFrame();
  SSLStatus ssl_status = view()->GetSSLStatusOfFrame(frame);
  EXPECT_FALSE(net::IsCertStatusError(ssl_status.cert_status));

  SSLStatus status;
  status.cert_status = net::CERT_STATUS_ALL_ERRORS;
  const_cast<blink::WebURLResponse&>(frame->dataSource()->response())
      .setSecurityInfo(SerializeSecurityInfo(status));
  ssl_status = view()->GetSSLStatusOfFrame(frame);
  EXPECT_TRUE(net::IsCertStatusError(ssl_status.cert_status));
}

TEST_F(RenderViewImplTest, MessageOrderInDidChangeSelection) {
  LoadHTML("<textarea id=\"test\"></textarea>");

  view()->SetHandlingInputEventForTesting(true);
  ExecuteJavaScriptForTests("document.getElementById('test').focus();");

  bool is_input_type_called = false;
  bool is_selection_called = false;
  size_t last_input_type = 0;
  size_t last_selection = 0;

  for (size_t i = 0; i < render_thread_->sink().message_count(); ++i) {
    const uint32_t type = render_thread_->sink().GetMessageAt(i)->type();
    if (type == ViewHostMsg_TextInputStateChanged::ID) {
      is_input_type_called = true;
      last_input_type = i;
    } else if (type == ViewHostMsg_SelectionChanged::ID) {
      is_selection_called = true;
      last_selection = i;
    }
  }

  EXPECT_TRUE(is_input_type_called);
  EXPECT_TRUE(is_selection_called);

  // InputTypeChange shold be called earlier than SelectionChanged.
  EXPECT_LT(last_input_type, last_selection);
}

class RendererErrorPageTest : public RenderViewImplTest {
 public:
  ContentRendererClient* CreateContentRendererClient() override {
    return new TestContentRendererClient;
  }

  RenderViewImpl* view() {
    return static_cast<RenderViewImpl*>(view_);
  }

  RenderFrameImpl* frame() {
    return static_cast<RenderFrameImpl*>(view()->GetMainRenderFrame());
  }

 private:
  class TestContentRendererClient : public ContentRendererClient {
   public:
    bool ShouldSuppressErrorPage(RenderFrame* render_frame,
                                 const GURL& url) override {
      return url == GURL("http://example.com/suppress");
    }

    void GetNavigationErrorStrings(content::RenderFrame* render_frame,
                                   const blink::WebURLRequest& failed_request,
                                   const blink::WebURLError& error,
                                   std::string* error_html,
                                   base::string16* error_description) override {
      if (error_html)
        *error_html = "A suffusion of yellow.";
    }

    bool HasErrorPage(int http_status_code,
                      std::string* error_domain) override {
      return true;
    }
  };
};

#if defined(OS_ANDROID)
// Crashing on Android: http://crbug.com/311341
#define MAYBE_Suppresses DISABLED_Suppresses
#else
#define MAYBE_Suppresses Suppresses
#endif

TEST_F(RendererErrorPageTest, MAYBE_Suppresses) {
  WebURLError error;
  error.domain = WebString::fromUTF8(net::kErrorDomain);
  error.reason = net::ERR_FILE_NOT_FOUND;
  error.unreachableURL = GURL("http://example.com/suppress");
  WebLocalFrame* web_frame = GetMainFrame();

  // Start a load that will reach provisional state synchronously,
  // but won't complete synchronously.
  CommonNavigationParams common_params;
  common_params.navigation_type = FrameMsg_Navigate_Type::NORMAL;
  common_params.url = GURL("data:text/html,test data");
  TestRenderFrame* main_frame = static_cast<TestRenderFrame*>(frame());
  main_frame->Navigate(common_params, StartNavigationParams(),
                       RequestNavigationParams());

  // An error occurred.
  main_frame->didFailProvisionalLoad(web_frame, error,
                                     blink::WebStandardCommit);
  const int kMaxOutputCharacters = 22;
  EXPECT_EQ("", base::UTF16ToASCII(base::StringPiece16(
                    WebFrameContentDumper::dumpWebViewAsText(
                        view()->GetWebView(), kMaxOutputCharacters))));
}

#if defined(OS_ANDROID)
// Crashing on Android: http://crbug.com/311341
#define MAYBE_DoesNotSuppress DISABLED_DoesNotSuppress
#else
#define MAYBE_DoesNotSuppress DoesNotSuppress
#endif

TEST_F(RendererErrorPageTest, MAYBE_DoesNotSuppress) {
  WebURLError error;
  error.domain = WebString::fromUTF8(net::kErrorDomain);
  error.reason = net::ERR_FILE_NOT_FOUND;
  error.unreachableURL = GURL("http://example.com/dont-suppress");
  WebLocalFrame* web_frame = GetMainFrame();

  // Start a load that will reach provisional state synchronously,
  // but won't complete synchronously.
  CommonNavigationParams common_params;
  common_params.navigation_type = FrameMsg_Navigate_Type::NORMAL;
  common_params.url = GURL("data:text/html,test data");
  TestRenderFrame* main_frame = static_cast<TestRenderFrame*>(frame());
  main_frame->Navigate(common_params, StartNavigationParams(),
                       RequestNavigationParams());

  // An error occurred.
  main_frame->didFailProvisionalLoad(web_frame, error,
                                     blink::WebStandardCommit);

  // The error page itself is loaded asynchronously.
  FrameLoadWaiter(main_frame).Wait();
  const int kMaxOutputCharacters = 22;
  EXPECT_EQ("A suffusion of yellow.",
            base::UTF16ToASCII(
                base::StringPiece16(WebFrameContentDumper::dumpWebViewAsText(
                    view()->GetWebView(), kMaxOutputCharacters))));
}

#if defined(OS_ANDROID)
// Crashing on Android: http://crbug.com/311341
#define MAYBE_HttpStatusCodeErrorWithEmptyBody \
  DISABLED_HttpStatusCodeErrorWithEmptyBody
#else
#define MAYBE_HttpStatusCodeErrorWithEmptyBody HttpStatusCodeErrorWithEmptyBody
#endif
TEST_F(RendererErrorPageTest, MAYBE_HttpStatusCodeErrorWithEmptyBody) {
  blink::WebURLResponse response;
  response.initialize();
  response.setHTTPStatusCode(503);
  WebLocalFrame* web_frame = GetMainFrame();

  // Start a load that will reach provisional state synchronously,
  // but won't complete synchronously.
  CommonNavigationParams common_params;
  common_params.navigation_type = FrameMsg_Navigate_Type::NORMAL;
  common_params.url = GURL("data:text/html,test data");
  TestRenderFrame* main_frame = static_cast<TestRenderFrame*>(frame());
  main_frame->Navigate(common_params, StartNavigationParams(),
                       RequestNavigationParams());

  // Emulate a 4xx/5xx main resource response with an empty body.
  main_frame->didReceiveResponse(1, response);
  main_frame->didFinishDocumentLoad(web_frame);
  main_frame->runScriptsAtDocumentReady(web_frame, true);

  // The error page itself is loaded asynchronously.
  FrameLoadWaiter(main_frame).Wait();
  const int kMaxOutputCharacters = 22;
  EXPECT_EQ("A suffusion of yellow.",
            base::UTF16ToASCII(
                base::StringPiece16(WebFrameContentDumper::dumpWebViewAsText(
                    view()->GetWebView(), kMaxOutputCharacters))));
}

// Ensure the render view sends favicon url update events correctly.
TEST_F(RenderViewImplTest, SendFaviconURLUpdateEvent) {
  // An event should be sent when a favicon url exists.
  LoadHTML("<html>"
           "<head>"
           "<link rel='icon' href='http://www.google.com/favicon.ico'>"
           "</head>"
           "</html>");
  EXPECT_TRUE(render_thread_->sink().GetFirstMessageMatching(
      ViewHostMsg_UpdateFaviconURL::ID));
  render_thread_->sink().ClearMessages();

  // An event should not be sent if no favicon url exists. This is an assumption
  // made by some of Chrome's favicon handling.
  LoadHTML("<html>"
           "<head>"
           "</head>"
           "</html>");
  EXPECT_FALSE(render_thread_->sink().GetFirstMessageMatching(
      ViewHostMsg_UpdateFaviconURL::ID));
}

TEST_F(RenderViewImplTest, FocusElementCallsFocusedNodeChanged) {
  LoadHTML("<input id='test1' value='hello1'></input>"
           "<input id='test2' value='hello2'></input>");

  ExecuteJavaScriptForTests("document.getElementById('test1').focus();");
  const IPC::Message* msg1 = render_thread_->sink().GetFirstMessageMatching(
      ViewHostMsg_FocusedNodeChanged::ID);
  EXPECT_TRUE(msg1);

  ViewHostMsg_FocusedNodeChanged::Param params;
  ViewHostMsg_FocusedNodeChanged::Read(msg1, &params);
  EXPECT_TRUE(std::get<0>(params));
  render_thread_->sink().ClearMessages();

  ExecuteJavaScriptForTests("document.getElementById('test2').focus();");
  const IPC::Message* msg2 = render_thread_->sink().GetFirstMessageMatching(
        ViewHostMsg_FocusedNodeChanged::ID);
  EXPECT_TRUE(msg2);
  ViewHostMsg_FocusedNodeChanged::Read(msg2, &params);
  EXPECT_TRUE(std::get<0>(params));
  render_thread_->sink().ClearMessages();

  view()->webview()->clearFocusedElement();
  const IPC::Message* msg3 = render_thread_->sink().GetFirstMessageMatching(
        ViewHostMsg_FocusedNodeChanged::ID);
  EXPECT_TRUE(msg3);
  ViewHostMsg_FocusedNodeChanged::Read(msg3, &params);
  EXPECT_FALSE(std::get<0>(params));
  render_thread_->sink().ClearMessages();
}

TEST_F(RenderViewImplTest, ServiceWorkerNetworkProviderSetup) {
  ServiceWorkerNetworkProvider* provider = NULL;
  RequestExtraData* extra_data = NULL;

  // Make sure each new document has a new provider and
  // that the main request is tagged with the provider's id.
  LoadHTML("<b>A Document</b>");
  ASSERT_TRUE(GetMainFrame()->dataSource());
  provider = ServiceWorkerNetworkProvider::FromDocumentState(
      DocumentState::FromDataSource(GetMainFrame()->dataSource()));
  ASSERT_TRUE(provider);
  extra_data = static_cast<RequestExtraData*>(
      GetMainFrame()->dataSource()->request().getExtraData());
  ASSERT_TRUE(extra_data);
  EXPECT_EQ(extra_data->service_worker_provider_id(),
            provider->provider_id());
  int provider1_id = provider->provider_id();

  LoadHTML("<b>New Document B Goes Here</b>");
  ASSERT_TRUE(GetMainFrame()->dataSource());
  provider = ServiceWorkerNetworkProvider::FromDocumentState(
      DocumentState::FromDataSource(GetMainFrame()->dataSource()));
  ASSERT_TRUE(provider);
  EXPECT_NE(provider1_id, provider->provider_id());
  extra_data = static_cast<RequestExtraData*>(
      GetMainFrame()->dataSource()->request().getExtraData());
  ASSERT_TRUE(extra_data);
  EXPECT_EQ(extra_data->service_worker_provider_id(),
            provider->provider_id());

  // See that subresource requests are also tagged with the provider's id.
  EXPECT_EQ(frame(), RenderFrameImpl::FromWebFrame(GetMainFrame()));
  blink::WebURLRequest request(GURL("http://foo.com"));
  request.setRequestContext(blink::WebURLRequest::RequestContextSubresource);
  blink::WebURLResponse redirect_response;
  frame()->willSendRequest(GetMainFrame(), 0, request, redirect_response);
  extra_data = static_cast<RequestExtraData*>(request.getExtraData());
  ASSERT_TRUE(extra_data);
  EXPECT_EQ(extra_data->service_worker_provider_id(),
            provider->provider_id());
}

TEST_F(RenderViewImplTest, OnSetAccessibilityMode) {
  ASSERT_EQ(AccessibilityModeOff, frame()->accessibility_mode());
  ASSERT_FALSE(frame()->render_accessibility());

  frame()->SetAccessibilityMode(AccessibilityModeTreeOnly);
  ASSERT_EQ(AccessibilityModeTreeOnly, frame()->accessibility_mode());
  ASSERT_TRUE(frame()->render_accessibility());

  frame()->SetAccessibilityMode(AccessibilityModeOff);
  ASSERT_EQ(AccessibilityModeOff, frame()->accessibility_mode());
  ASSERT_FALSE(frame()->render_accessibility());

  frame()->SetAccessibilityMode(AccessibilityModeComplete);
  ASSERT_EQ(AccessibilityModeComplete, frame()->accessibility_mode());
  ASSERT_TRUE(frame()->render_accessibility());
}

// Sanity check for the Navigation Timing API |navigationStart| override. We
// are asserting only most basic constraints, as TimeTicks (passed as the
// override) are not comparable with the wall time (returned by the Blink API).
TEST_F(RenderViewImplTest, NavigationStartOverride) {
  // Verify that a navigation that claims to have started in the future - 42
  // days from now is *not* reported as one that starts in the future; as we
  // sanitize the override allowing a maximum of ::Now().
  CommonNavigationParams late_common_params;
  late_common_params.url = GURL("data:text/html,<div>Another page</div>");
  late_common_params.navigation_type = FrameMsg_Navigate_Type::NORMAL;
  late_common_params.transition = ui::PAGE_TRANSITION_TYPED;
  late_common_params.navigation_start =
      base::TimeTicks::Now() + base::TimeDelta::FromDays(42);
  late_common_params.method = "POST";

  frame()->Navigate(late_common_params, StartNavigationParams(),
                    RequestNavigationParams());
  ProcessPendingMessages();
  base::Time after_navigation =
      base::Time::Now() + base::TimeDelta::FromDays(1);

  base::Time late_nav_reported_start =
      base::Time::FromDoubleT(GetMainFrame()->performance().navigationStart());
  EXPECT_LE(late_nav_reported_start, after_navigation);
}

TEST_F(RenderViewImplTest, RendererNavigationStartTransmittedToBrowser) {
  base::TimeTicks lower_bound_navigation_start;
  frame()->GetWebFrame()->loadHTMLString(
      "hello world", blink::WebURL(GURL("data:text/html,")));
  ProcessPendingMessages();
  const IPC::Message* frame_navigate_msg =
      render_thread_->sink().GetUniqueMessageMatching(
          FrameHostMsg_DidStartProvisionalLoad::ID);
  FrameHostMsg_DidStartProvisionalLoad::Param host_nav_params;
  FrameHostMsg_DidStartProvisionalLoad::Read(frame_navigate_msg,
                                                     &host_nav_params);
  base::TimeTicks transmitted_start = std::get<1>(host_nav_params);
  EXPECT_FALSE(transmitted_start.is_null());
  EXPECT_LT(lower_bound_navigation_start, transmitted_start);
}

TEST_F(RenderViewImplTest, BrowserNavigationStartNotUsedForReload) {
  const char url_string[] = "data:text/html,<div>Page</div>";
  // Navigate once, then reload.
  LoadHTML(url_string);
  ProcessPendingMessages();
  render_thread_->sink().ClearMessages();

  CommonNavigationParams common_params;
  common_params.url = GURL(url_string);
  common_params.navigation_type =
      FrameMsg_Navigate_Type::RELOAD_ORIGINAL_REQUEST_URL;
  common_params.transition = ui::PAGE_TRANSITION_RELOAD;

  frame()->Navigate(common_params, StartNavigationParams(),
                    RequestNavigationParams());

  FrameHostMsg_DidStartProvisionalLoad::Param host_nav_params =
      ProcessAndReadIPC<FrameHostMsg_DidStartProvisionalLoad>();
  // The true timestamp is later than the browser initiated one.
  EXPECT_PRED2(TimeTicksGT, std::get<1>(host_nav_params),
               common_params.navigation_start);
}

TEST_F(RenderViewImplTest, BrowserNavigationStartNotUsedForHistoryNavigation) {
  LoadHTML("<div id=pagename>Page A</div>");
  LoadHTML("<div id=pagename>Page B</div>");
  PageState back_state = GetCurrentPageState();
  LoadHTML("<div id=pagename>Page C</div>");
  PageState forward_state = GetCurrentPageState();
  ProcessPendingMessages();
  render_thread_->sink().ClearMessages();

  // Go back.
  CommonNavigationParams common_params_back;
  common_params_back.url =
      GURL("data:text/html;charset=utf-8,<div id=pagename>Page B</div>");
  common_params_back.transition = ui::PAGE_TRANSITION_FORWARD_BACK;
  GoToOffsetWithParams(-1, back_state, common_params_back,
                       StartNavigationParams(), RequestNavigationParams());
  FrameHostMsg_DidStartProvisionalLoad::Param host_nav_params =
      ProcessAndReadIPC<FrameHostMsg_DidStartProvisionalLoad>();
  EXPECT_PRED2(TimeTicksGT, std::get<1>(host_nav_params),
               common_params_back.navigation_start);
  render_thread_->sink().ClearMessages();

  // Go forward.
  CommonNavigationParams common_params_forward;
  common_params_forward.url =
      GURL("data:text/html;charset=utf-8,<div id=pagename>Page C</div>");
  common_params_forward.transition = ui::PAGE_TRANSITION_FORWARD_BACK;
  GoToOffsetWithParams(1, forward_state, common_params_forward,
                       StartNavigationParams(), RequestNavigationParams());
  FrameHostMsg_DidStartProvisionalLoad::Param host_nav_params2 =
      ProcessAndReadIPC<FrameHostMsg_DidStartProvisionalLoad>();
  EXPECT_PRED2(TimeTicksGT, std::get<1>(host_nav_params2),
               common_params_forward.navigation_start);
}

TEST_F(RenderViewImplTest, BrowserNavigationStartSuccessfullyTransmitted) {
  CommonNavigationParams common_params;
  common_params.url = GURL("data:text/html,<div>Page</div>");
  common_params.navigation_type = FrameMsg_Navigate_Type::NORMAL;
  common_params.transition = ui::PAGE_TRANSITION_TYPED;

  frame()->Navigate(common_params, StartNavigationParams(),
                    RequestNavigationParams());

  FrameHostMsg_DidStartProvisionalLoad::Param host_nav_params =
      ProcessAndReadIPC<FrameHostMsg_DidStartProvisionalLoad>();
  EXPECT_EQ(std::get<1>(host_nav_params), common_params.navigation_start);
}

TEST_F(RenderViewImplTest, PreferredSizeZoomed) {
  LoadHTML("<body style='margin:0;'><div style='display:inline-block; "
           "width:400px; height:400px;'/></body>");
  view()->webview()->mainFrame()->setCanHaveScrollbars(false);
  EnablePreferredSizeMode();

  gfx::Size size = GetPreferredSize();
  EXPECT_EQ(gfx::Size(400, 400), size);

  SetZoomLevel(ZoomFactorToZoomLevel(2.0));
  size = GetPreferredSize();
  EXPECT_EQ(gfx::Size(800, 800), size);
}

// Ensure the RenderViewImpl history list is properly updated when starting a
// new browser-initiated navigation.
TEST_F(RenderViewImplTest, HistoryIsProperlyUpdatedOnNavigation) {
  EXPECT_EQ(0, view()->historyBackListCount());
  EXPECT_EQ(0, view()->historyBackListCount() +
      view()->historyForwardListCount() + 1);

  // Receive a Navigate message with history parameters.
  RequestNavigationParams request_params;
  request_params.current_history_list_length = 2;
  request_params.current_history_list_offset = 1;
  request_params.pending_history_list_offset = 2;
  request_params.page_id = -1;
  frame()->Navigate(CommonNavigationParams(), StartNavigationParams(),
                    request_params);

  // The history list in RenderView should have been updated.
  EXPECT_EQ(1, view()->historyBackListCount());
  EXPECT_EQ(2, view()->historyBackListCount() +
      view()->historyForwardListCount() + 1);
}

TEST_F(RenderViewImplBlinkSettingsTest, Default) {
  DoSetUp();
  EXPECT_FALSE(settings()->viewportEnabled());
}

TEST_F(RenderViewImplBlinkSettingsTest, CommandLine) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kBlinkSettings,
      "multiTargetTapNotificationEnabled=true,viewportEnabled=true");
  DoSetUp();
  EXPECT_TRUE(settings()->multiTargetTapNotificationEnabled());
  EXPECT_TRUE(settings()->viewportEnabled());
}

TEST_F(RenderViewImplBlinkSettingsTest, Negative) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kBlinkSettings,
      "multiTargetTapNotificationEnabled=false,viewportEnabled=true");
  DoSetUp();
  EXPECT_FALSE(settings()->multiTargetTapNotificationEnabled());
  EXPECT_TRUE(settings()->viewportEnabled());
}

TEST_F(RenderViewImplScaleFactorTest, ConverViewportToWindowWithoutZoomForDSF) {
  DoSetUp();
  if (IsUseZoomForDSFEnabled())
    return;
  SetDeviceScaleFactor(2.f);
  blink::WebRect rect(20, 10, 200, 100);
  view()->convertViewportToWindow(&rect);
  EXPECT_EQ(20, rect.x);
  EXPECT_EQ(10, rect.y);
  EXPECT_EQ(200, rect.width);
  EXPECT_EQ(100, rect.height);
}

TEST_F(RenderViewImplScaleFactorTest, ScreenMetricsEmulationWithOriginalDSF1) {
  DoSetUp();
  SetDeviceScaleFactor(1.f);

  LoadHTML("<body style='min-height:1000px;'></body>");
  {
    SCOPED_TRACE("327x415 1dpr");
    TestEmulatedSizeDprDsf(327, 415, 1.f, 1.f);
  }
  {
    SCOPED_TRACE("327x415 1.5dpr");
    TestEmulatedSizeDprDsf(327, 415, 1.5f, 1.f);
  }
  {
    SCOPED_TRACE("1005x1102 2dpr");
    TestEmulatedSizeDprDsf(1005, 1102, 2.f, 1.f);
  }
  {
    SCOPED_TRACE("1005x1102 3dpr");
    TestEmulatedSizeDprDsf(1005, 1102, 3.f, 1.f);
  }

  view()->OnDisableDeviceEmulation();

  blink::WebDeviceEmulationParams params;
  view()->OnEnableDeviceEmulation(params);
  // Don't disable here to test that emulation is being shutdown properly.
}

TEST_F(RenderViewImplScaleFactorTest, ScreenMetricsEmulationWithOriginalDSF2) {
  DoSetUp();
  SetDeviceScaleFactor(2.f);
  float compositor_dsf =
      IsUseZoomForDSFEnabled() ? 1.f : 2.f;

  LoadHTML("<body style='min-height:1000px;'></body>");
  {
    SCOPED_TRACE("327x415 1dpr");
    TestEmulatedSizeDprDsf(327, 415, 1.f, compositor_dsf);
  }
  {
    SCOPED_TRACE("327x415 1.5dpr");
    TestEmulatedSizeDprDsf(327, 415, 1.5f, compositor_dsf);
  }
  {
    SCOPED_TRACE("1005x1102 2dpr");
    TestEmulatedSizeDprDsf(1005, 1102, 2.f, compositor_dsf);
  }
  {
    SCOPED_TRACE("1005x1102 3dpr");
    TestEmulatedSizeDprDsf(1005, 1102, 3.f, compositor_dsf);
  }

  view()->OnDisableDeviceEmulation();

  blink::WebDeviceEmulationParams params;
  view()->OnEnableDeviceEmulation(params);
  // Don't disable here to test that emulation is being shutdown properly.
}

TEST_F(RenderViewImplScaleFactorTest, ConverViewportToWindowWithZoomForDSF) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableUseZoomForDSF);
  DoSetUp();
  SetDeviceScaleFactor(1.f);
  {
    blink::WebRect rect(20, 10, 200, 100);
    view()->convertViewportToWindow(&rect);
    EXPECT_EQ(20, rect.x);
    EXPECT_EQ(10, rect.y);
    EXPECT_EQ(200, rect.width);
    EXPECT_EQ(100, rect.height);
  }

  SetDeviceScaleFactor(2.f);
  {
    blink::WebRect rect(20, 10, 200, 100);
    view()->convertViewportToWindow(&rect);
    EXPECT_EQ(10, rect.x);
    EXPECT_EQ(5, rect.y);
    EXPECT_EQ(100, rect.width);
    EXPECT_EQ(50, rect.height);
  }
}

#if defined(OS_MACOSX) || defined(USE_AURA)
TEST_F(RenderViewImplScaleFactorTest,
       DISABLED_GetCompositionCharacterBoundsTest) {  // http://crbug.com/582016
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableUseZoomForDSF);
  DoSetUp();
  SetDeviceScaleFactor(1.f);
#if defined(OS_WIN)
  // http://crbug.com/508747
  if (base::win::GetVersion() >= base::win::VERSION_WIN10)
    return;
#endif

  LoadHTML("<textarea id=\"test\"></textarea>");
  ExecuteJavaScriptForTests("document.getElementById('test').focus();");

  const base::string16 empty_string;
  const std::vector<blink::WebCompositionUnderline> empty_underline;
  std::vector<gfx::Rect> bounds_at_1x;
  view()->OnSetFocus(true);

  // ASCII composition
  const base::string16 ascii_composition = base::UTF8ToUTF16("aiueo");
  view()->OnImeSetComposition(ascii_composition, empty_underline,
                              gfx::Range::InvalidRange(), 0, 0);
  view()->GetCompositionCharacterBounds(&bounds_at_1x);
  ASSERT_EQ(ascii_composition.size(), bounds_at_1x.size());

  SetDeviceScaleFactor(2.f);
  std::vector<gfx::Rect> bounds_at_2x;
  view()->GetCompositionCharacterBounds(&bounds_at_2x);
  ASSERT_EQ(bounds_at_1x.size(), bounds_at_2x.size());
  for (size_t i = 0; i < bounds_at_1x.size(); i++) {
    const gfx::Rect& b1 = bounds_at_1x[i];
    const gfx::Rect& b2 = bounds_at_2x[i];
    gfx::Vector2d origin_diff = b1.origin() - b2.origin();

    // The bounds may not be exactly same because the font metrics are different
    // at 1x and 2x. Just make sure that the difference is small.
    EXPECT_LT(origin_diff.x(), 2);
    EXPECT_LT(origin_diff.y(), 2);
    EXPECT_LT(std::abs(b1.width() - b2.width()), 3);
    EXPECT_LT(std::abs(b1.height() - b2.height()), 2);
  }
}
#endif

#if !defined(OS_ANDROID)
// No extensions/autoresize on Android.
namespace {

// Don't use text as it text will change the size in DIP at different
// scale factor.
const char kAutoResizeTestPage[] =
    "<div style='width=20px; height=20px'></div>";

}  // namespace

TEST_F(RenderViewImplScaleFactorTest, AutoResizeWithZoomForDSF) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableUseZoomForDSF);
  DoSetUp();
  view()->EnableAutoResizeForTesting(gfx::Size(5, 5), gfx::Size(1000, 1000));
  LoadHTML(kAutoResizeTestPage);
  gfx::Size size_at_1x = view()->GetWidget()->size();
  ASSERT_FALSE(size_at_1x.IsEmpty());

  SetDeviceScaleFactor(2.f);
  LoadHTML(kAutoResizeTestPage);
  gfx::Size size_at_2x = view()->GetWidget()->size();
  EXPECT_EQ(size_at_1x, size_at_2x);
}

TEST_F(RenderViewImplScaleFactorTest, AutoResizeWithoutZoomForDSF) {
  DoSetUp();
  view()->EnableAutoResizeForTesting(gfx::Size(5, 5), gfx::Size(1000, 1000));
  LoadHTML(kAutoResizeTestPage);
  gfx::Size size_at_1x = view()->GetWidget()->size();
  ASSERT_FALSE(size_at_1x.IsEmpty());

  SetDeviceScaleFactor(2.f);
  LoadHTML(kAutoResizeTestPage);
  gfx::Size size_at_2x = view()->GetWidget()->size();
  EXPECT_EQ(size_at_1x, size_at_2x);
}

#endif

TEST_F(DevToolsAgentTest, DevToolsResumeOnClose) {
  Attach();
  EXPECT_FALSE(IsPaused());
  DispatchDevToolsMessage("Debugger.enable",
                          "{\"id\":1,\"method\":\"Debugger.enable\"}");

  // Executing javascript will pause the thread and create nested message loop.
  // Posting task simulates message coming from browser.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&DevToolsAgentTest::CloseWhilePaused, base::Unretained(this)));
  ExecuteJavaScriptForTests("debugger;");

  // CloseWhilePaused should resume execution and continue here.
  EXPECT_FALSE(IsPaused());
  Detach();
}

TEST_F(DevToolsAgentTest, RuntimeEnableForcesContexts) {
  LoadHTML("<body>page<iframe></iframe></body>");
  Attach();
  DispatchDevToolsMessage("Runtime.enable",
                          "{\"id\":1,\"method\":\"Runtime.enable\"}");
  EXPECT_EQ(2, CountNotifications("Runtime.executionContextCreated"));
}

TEST_F(DevToolsAgentTest, RuntimeEnableForcesContextsAfterNavigation) {
  Attach();
  DispatchDevToolsMessage("Runtime.enable",
                          "{\"id\":1,\"method\":\"Runtime.enable\"}");
  EXPECT_EQ(0, CountNotifications("Runtime.executionContextCreated"));
  LoadHTML("<body>page<iframe></iframe></body>");
  EXPECT_EQ(2, CountNotifications("Runtime.executionContextCreated"));
}

TEST_F(DevToolsAgentTest, RuntimeEvaluateRunMicrotasks) {
  LoadHTML("<body>page</body>");
  Attach();
  DispatchDevToolsMessage("Console.enable",
                          "{\"id\":1,\"method\":\"Console.enable\"}");
  DispatchDevToolsMessage("Runtime.evaluate",
                          "{\"id\":2,"
                          "\"method\":\"Runtime.evaluate\","
                          "\"params\":{"
                          "\"expression\":\"Promise.resolve().then("
                          "() => console.log(42));\""
                          "}"
                          "}");
  EXPECT_EQ(1, CountNotifications("Console.messageAdded"));
}

TEST_F(DevToolsAgentTest, RuntimeCallFunctionOnRunMicrotasks) {
  LoadHTML("<body>page</body>");
  Attach();
  DispatchDevToolsMessage("Console.enable",
                          "{\"id\":1,\"method\":\"Console.enable\"}");
  DispatchDevToolsMessage("Runtime.evaluate",
                          "{\"id\":2,"
                          "\"method\":\"Runtime.evaluate\","
                          "\"params\":{"
                          "\"expression\":\"window\""
                          "}"
                          "}");

  base::DictionaryValue* root = LastReceivedMessage();
  const base::Value* object_id;
  ASSERT_TRUE(root->Get("result.result.objectId", &object_id));
  std::string object_id_str;
  EXPECT_TRUE(base::JSONWriter::Write(*object_id, &object_id_str));

  DispatchDevToolsMessage("Runtime.callFunctionOn",
                          "{\"id\":3,"
                          "\"method\":\"Runtime.callFunctionOn\","
                          "\"params\":{"
                          "\"objectId\":" +
                              object_id_str +
                              ","
                              "\"functionDeclaration\":\"function foo(){ "
                              "Promise.resolve().then(() => "
                              "console.log(239))}\""
                              "}"
                              "}");
  EXPECT_EQ(1, CountNotifications("Console.messageAdded"));
}

TEST_F(DevToolsAgentTest, CallFramesInIsolatedWorld) {
  LoadHTML("<body>page</body>");
  blink::WebScriptSource source1(
      WebString::fromUTF8("function func1() { debugger; }"));
  frame()->GetWebFrame()->executeScriptInIsolatedWorld(17, &source1, 1, 1);

  Attach();
  DispatchDevToolsMessage("Debugger.enable",
                          "{\"id\":1,\"method\":\"Debugger.enable\"}");

  ExpectPauseAndResume(3);
  blink::WebScriptSource source2(
      WebString::fromUTF8("function func2() { func1(); }; func2();"));
  frame()->GetWebFrame()->executeScriptInIsolatedWorld(17, &source2, 1, 1);

  EXPECT_FALSE(IsPaused());
  Detach();
}

}  // namespace content
