// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/browser_plugin/browser_plugin_browsertest.h"

#include "base/debug/leak_annotations.h"
#include "base/files/file_path.h"
#include "base/memory/singleton.h"
#include "base/path_service.h"
#include "base/pickle.h"
#include "content/public/common/content_constants.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/browser_plugin/browser_plugin.h"
#include "content/renderer/browser_plugin/browser_plugin_manager_factory.h"
#include "content/renderer/browser_plugin/mock_browser_plugin.h"
#include "content/renderer/browser_plugin/mock_browser_plugin_manager.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/renderer_webkitplatformsupport_impl.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/public/platform/WebCursorInfo.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebScriptSource.h"

namespace content {

namespace {
const char kHTMLForBrowserPluginObject[] =
    "<object id='browserplugin' width='640px' height='480px'"
    " src='foo' type='%s'></object>"
    "<script>document.querySelector('object').nonExistentAttribute;</script>";

const char kHTMLForBrowserPluginWithAllAttributes[] =
    "<object id='browserplugin' width='640' height='480' type='%s'"
    " autosize maxheight='600' maxwidth='800' minheight='240'"
    " minwidth='320' name='Jim' partition='someid' src='foo'>";

const char kHTMLForSourcelessPluginObject[] =
    "<object id='browserplugin' width='640px' height='480px' type='%s'>";

std::string GetHTMLForBrowserPluginObject() {
  return base::StringPrintf(kHTMLForBrowserPluginObject,
                            kBrowserPluginMimeType);
}

}  // namespace

// Test factory for creating test instances of BrowserPluginManager.
class TestBrowserPluginManagerFactory : public BrowserPluginManagerFactory {
 public:
  virtual MockBrowserPluginManager* CreateBrowserPluginManager(
      RenderViewImpl* render_view) OVERRIDE {
    return new MockBrowserPluginManager(render_view);
  }

  // Singleton getter.
  static TestBrowserPluginManagerFactory* GetInstance() {
    return Singleton<TestBrowserPluginManagerFactory>::get();
  }

 protected:
  TestBrowserPluginManagerFactory() {}
  virtual ~TestBrowserPluginManagerFactory() {}

 private:
  // For Singleton.
  friend struct DefaultSingletonTraits<TestBrowserPluginManagerFactory>;

  DISALLOW_COPY_AND_ASSIGN(TestBrowserPluginManagerFactory);
};

BrowserPluginTest::BrowserPluginTest() {}

BrowserPluginTest::~BrowserPluginTest() {}

void BrowserPluginTest::SetUp() {
  BrowserPluginManager::set_factory_for_testing(
      TestBrowserPluginManagerFactory::GetInstance());
  content::RenderViewTest::SetUp();
}

void BrowserPluginTest::TearDown() {
  BrowserPluginManager::set_factory_for_testing(
      TestBrowserPluginManagerFactory::GetInstance());
#if defined(LEAK_SANITIZER)
  // Do this before shutting down V8 in RenderViewTest::TearDown().
  // http://crbug.com/328552
  __lsan_do_leak_check();
#endif
  RenderViewTest::TearDown();
}

std::string BrowserPluginTest::ExecuteScriptAndReturnString(
    const std::string& script) {
  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
  v8::Handle<v8::Value> value = GetMainFrame()->executeScriptAndReturnValue(
      blink::WebScriptSource(blink::WebString::fromUTF8(script.c_str())));
  if (value.IsEmpty() || !value->IsString())
    return std::string();

  v8::Local<v8::String> v8_str = value->ToString();
  int length = v8_str->Utf8Length() + 1;
  scoped_ptr<char[]> str(new char[length]);
  v8_str->WriteUtf8(str.get(), length);
  return str.get();
}

int BrowserPluginTest::ExecuteScriptAndReturnInt(
    const std::string& script) {
  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
  v8::Handle<v8::Value> value = GetMainFrame()->executeScriptAndReturnValue(
      blink::WebScriptSource(blink::WebString::fromUTF8(script.c_str())));
  if (value.IsEmpty() || !value->IsInt32())
    return 0;

  return value->Int32Value();
}

// A return value of false means that a value was not present. The return value
// of the script is stored in |result|
bool BrowserPluginTest::ExecuteScriptAndReturnBool(
    const std::string& script, bool* result) {
  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
  v8::Handle<v8::Value> value = GetMainFrame()->executeScriptAndReturnValue(
      blink::WebScriptSource(blink::WebString::fromUTF8(script.c_str())));
  if (value.IsEmpty() || !value->IsBoolean())
    return false;

  *result = value->BooleanValue();
  return true;
}

MockBrowserPlugin* BrowserPluginTest::GetCurrentPlugin() {
  BrowserPluginHostMsg_Attach_Params params;
  return GetCurrentPluginWithAttachParams(&params);
}

MockBrowserPlugin* BrowserPluginTest::GetCurrentPluginWithAttachParams(
    BrowserPluginHostMsg_Attach_Params* params) {
  MockBrowserPlugin* browser_plugin = static_cast<MockBrowserPluginManager*>(
      browser_plugin_manager())->last_plugin();
  if (!browser_plugin)
    return NULL;
  browser_plugin_manager()->AllocateInstanceID(browser_plugin);

  int instance_id = 0;
  const IPC::Message* msg =
      browser_plugin_manager()->sink().GetUniqueMessageMatching(
          BrowserPluginHostMsg_Attach::ID);
  if (!msg)
    return NULL;

  PickleIterator iter(*msg);
  if (!iter.ReadInt(&instance_id))
    return NULL;

  if (!IPC::ParamTraits<BrowserPluginHostMsg_Attach_Params>::Read(
      msg, &iter, params)) {
    return NULL;
  }

  browser_plugin->OnAttachACK(instance_id);
  return browser_plugin;
}

// This test verifies that an initial resize occurs when we instantiate the
// browser plugin.
TEST_F(BrowserPluginTest, InitialResize) {
  LoadHTML(GetHTMLForBrowserPluginObject().c_str());
  // Verify that the information in Attach is correct.
  BrowserPluginHostMsg_Attach_Params params;
  MockBrowserPlugin* browser_plugin = GetCurrentPluginWithAttachParams(&params);

  EXPECT_EQ(640, params.resize_guest_params.view_size.width());
  EXPECT_EQ(480, params.resize_guest_params.view_size.height());
  ASSERT_TRUE(browser_plugin);
}

// This test verifies that all attributes (present at the time of writing) are
// parsed on initialization. However, this test does minimal checking of
// correct behavior.
TEST_F(BrowserPluginTest, ParseAllAttributes) {
  std::string html = base::StringPrintf(kHTMLForBrowserPluginWithAllAttributes,
                                        kBrowserPluginMimeType);
  LoadHTML(html.c_str());
  bool result;
  bool has_value = ExecuteScriptAndReturnBool(
      "document.getElementById('browserplugin').autosize", &result);
  EXPECT_TRUE(has_value);
  EXPECT_TRUE(result);
  int maxHeight = ExecuteScriptAndReturnInt(
      "document.getElementById('browserplugin').maxheight");
  EXPECT_EQ(600, maxHeight);
  int maxWidth = ExecuteScriptAndReturnInt(
      "document.getElementById('browserplugin').maxwidth");
  EXPECT_EQ(800, maxWidth);
  int minHeight = ExecuteScriptAndReturnInt(
      "document.getElementById('browserplugin').minheight");
  EXPECT_EQ(240, minHeight);
  int minWidth = ExecuteScriptAndReturnInt(
      "document.getElementById('browserplugin').minwidth");
  EXPECT_EQ(320, minWidth);
}

TEST_F(BrowserPluginTest, ResizeFlowControl) {
  LoadHTML(GetHTMLForBrowserPluginObject().c_str());
  MockBrowserPlugin* browser_plugin = GetCurrentPlugin();
  ASSERT_TRUE(browser_plugin);
  int instance_id = browser_plugin->guest_instance_id();
  // Send an UpdateRect to the BrowserPlugin to make sure the browser sees a
  // resize related (SetAutoSize) message.
  {
    // We send a stale UpdateRect to the BrowserPlugin.
    BrowserPluginMsg_UpdateRect_Params update_rect_params;
    update_rect_params.view_size = gfx::Size(640, 480);
    update_rect_params.scale_factor = 1.0f;
    update_rect_params.is_resize_ack = true;
    BrowserPluginMsg_UpdateRect msg(instance_id, update_rect_params);
    browser_plugin->OnMessageReceived(msg);
  }

  browser_plugin_manager()->sink().ClearMessages();

  // Resize the browser plugin three times.

  ExecuteJavaScript("document.getElementById('browserplugin').width = '641px'");
  GetMainFrame()->view()->layout();
  ProcessPendingMessages();

  ExecuteJavaScript("document.getElementById('browserplugin').width = '642px'");
  GetMainFrame()->view()->layout();
  ProcessPendingMessages();

  ExecuteJavaScript("document.getElementById('browserplugin').width = '643px'");
  GetMainFrame()->view()->layout();
  ProcessPendingMessages();

  // Expect to see one resize messsage in the sink. BrowserPlugin will not issue
  // subsequent resize requests until the first request is satisfied by the
  // guest. The rest of the messages could be
  // BrowserPluginHostMsg_UpdateGeometry msgs.
  EXPECT_LE(1u, browser_plugin_manager()->sink().message_count());
  for (size_t i = 0; i < browser_plugin_manager()->sink().message_count();
       ++i) {
    const IPC::Message* msg = browser_plugin_manager()->sink().GetMessageAt(i);
    if (msg->type() != BrowserPluginHostMsg_ResizeGuest::ID)
      EXPECT_EQ(msg->type(), BrowserPluginHostMsg_UpdateGeometry::ID);
  }
  const IPC::Message* msg =
      browser_plugin_manager()->sink().GetUniqueMessageMatching(
          BrowserPluginHostMsg_ResizeGuest::ID);
  ASSERT_TRUE(msg);
  BrowserPluginHostMsg_ResizeGuest::Param param;
  BrowserPluginHostMsg_ResizeGuest::Read(msg, &param);
  instance_id = param.a;
  BrowserPluginHostMsg_ResizeGuest_Params params = param.b;
  EXPECT_EQ(641, params.view_size.width());
  EXPECT_EQ(480, params.view_size.height());

  {
    // We send a stale UpdateRect to the BrowserPlugin.
    BrowserPluginMsg_UpdateRect_Params update_rect_params;
    update_rect_params.view_size = gfx::Size(641, 480);
    update_rect_params.scale_factor = 1.0f;
    update_rect_params.is_resize_ack = true;
    BrowserPluginMsg_UpdateRect msg(instance_id, update_rect_params);
    browser_plugin->OnMessageReceived(msg);
  }
  // Send the BrowserPlugin another UpdateRect, but this time with a size
  // that matches the size of the container.
  {
    BrowserPluginMsg_UpdateRect_Params update_rect_params;
    update_rect_params.view_size = gfx::Size(643, 480);
    update_rect_params.scale_factor = 1.0f;
    update_rect_params.is_resize_ack = true;
    BrowserPluginMsg_UpdateRect msg(instance_id, update_rect_params);
    browser_plugin->OnMessageReceived(msg);
  }
}

TEST_F(BrowserPluginTest, RemovePlugin) {
  LoadHTML(GetHTMLForBrowserPluginObject().c_str());
  MockBrowserPlugin* browser_plugin = GetCurrentPlugin();
  ASSERT_TRUE(browser_plugin);

  EXPECT_FALSE(browser_plugin_manager()->sink().GetUniqueMessageMatching(
      BrowserPluginHostMsg_PluginDestroyed::ID));
  ExecuteJavaScript("x = document.getElementById('browserplugin'); "
                    "x.parentNode.removeChild(x);");
  ProcessPendingMessages();
  EXPECT_TRUE(browser_plugin_manager()->sink().GetUniqueMessageMatching(
      BrowserPluginHostMsg_PluginDestroyed::ID));
}

// This test verifies that PluginDestroyed messages do not get sent from a
// BrowserPlugin that has never navigated.
TEST_F(BrowserPluginTest, RemovePluginBeforeNavigation) {
  std::string html = base::StringPrintf(kHTMLForSourcelessPluginObject,
                                        kBrowserPluginMimeType);
  LoadHTML(html.c_str());
  EXPECT_FALSE(browser_plugin_manager()->sink().GetUniqueMessageMatching(
      BrowserPluginHostMsg_PluginDestroyed::ID));
  ExecuteJavaScript("x = document.getElementById('browserplugin'); "
                    "x.parentNode.removeChild(x);");
  ProcessPendingMessages();
  EXPECT_FALSE(browser_plugin_manager()->sink().GetUniqueMessageMatching(
      BrowserPluginHostMsg_PluginDestroyed::ID));
}

// Verify that the 'partition' attribute on the browser plugin is parsed
// correctly.
TEST_F(BrowserPluginTest, AutoSizeAttributes) {
  std::string html = base::StringPrintf(kHTMLForSourcelessPluginObject,
                                        kBrowserPluginMimeType);
  LoadHTML(html.c_str());
  const char* kSetAutoSizeParametersAndNavigate =
    "var browserplugin = document.getElementById('browserplugin');"
    "browserplugin.autosize = true;"
    "browserplugin.minwidth = 42;"
    "browserplugin.minheight = 43;"
    "browserplugin.maxwidth = 1337;"
    "browserplugin.maxheight = 1338;"
    "browserplugin.src = 'foobar';";
  const char* kDisableAutoSize =
    "document.getElementById('browserplugin').removeAttribute('autosize');";

  int instance_id = 0;
  // Set some autosize parameters before navigating then navigate.
  // Verify that the BrowserPluginHostMsg_Attach message contains
  // the correct autosize parameters.
  ExecuteJavaScript(kSetAutoSizeParametersAndNavigate);
  ProcessPendingMessages();

  BrowserPluginHostMsg_Attach_Params params;
  MockBrowserPlugin* browser_plugin = GetCurrentPluginWithAttachParams(&params);
  ASSERT_TRUE(browser_plugin);

  EXPECT_TRUE(params.auto_size_params.enable);
  EXPECT_EQ(42, params.auto_size_params.min_size.width());
  EXPECT_EQ(43, params.auto_size_params.min_size.height());
  EXPECT_EQ(1337, params.auto_size_params.max_size.width());
  EXPECT_EQ(1338, params.auto_size_params.max_size.height());

  // Disable autosize. AutoSize state will not be sent to the guest until
  // the guest has responded to the last resize request.
  ExecuteJavaScript(kDisableAutoSize);
  ProcessPendingMessages();

  const IPC::Message* auto_size_msg =
  browser_plugin_manager()->sink().GetUniqueMessageMatching(
      BrowserPluginHostMsg_SetAutoSize::ID);
  EXPECT_FALSE(auto_size_msg);

  // Send the BrowserPlugin an UpdateRect equal to its |max_size|.
  BrowserPluginMsg_UpdateRect_Params update_rect_params;
  update_rect_params.view_size = gfx::Size(1337, 1338);
  update_rect_params.scale_factor = 1.0f;
  update_rect_params.is_resize_ack = true;
  BrowserPluginMsg_UpdateRect msg(instance_id, update_rect_params);
  browser_plugin->OnMessageReceived(msg);

  // Verify that the autosize state has been updated.
  {
    const IPC::Message* auto_size_msg =
        browser_plugin_manager()->sink().GetUniqueMessageMatching(
            BrowserPluginHostMsg_SetAutoSize::ID);
    ASSERT_TRUE(auto_size_msg);

    BrowserPluginHostMsg_SetAutoSize::Param param;
    BrowserPluginHostMsg_SetAutoSize::Read(auto_size_msg, &param);
    BrowserPluginHostMsg_AutoSize_Params auto_size_params = param.b;
    BrowserPluginHostMsg_ResizeGuest_Params resize_params = param.c;
    EXPECT_FALSE(auto_size_params.enable);
    // These value are not populated (as an optimization) if autosize is
    // disabled.
    EXPECT_EQ(0, auto_size_params.min_size.width());
    EXPECT_EQ(0, auto_size_params.min_size.height());
    EXPECT_EQ(0, auto_size_params.max_size.width());
    EXPECT_EQ(0, auto_size_params.max_size.height());
  }
}

}  // namespace content
