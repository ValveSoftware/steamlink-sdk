// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/webview/webview.h"

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread.h"
#include "ui/base/ime/text_input_focus_manager.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gl/gl_surface.h"
#include "ui/views/test/webview_test_helper.h"
#include "ui/views/test/widget_test.h"

namespace {

class WebViewInteractiveUiTest : public views::test::WidgetTest {
 public:
  WebViewInteractiveUiTest()
      : ui_thread_(content::BrowserThread::UI, base::MessageLoop::current()) {}

  virtual void SetUp() OVERRIDE {
    gfx::GLSurface::InitializeOneOffForTests();
    WidgetTest::SetUp();
  }

 protected:
  content::BrowserContext* browser_context() { return &browser_context_; }

 private:
  content::TestBrowserContext browser_context_;
  views::WebViewTestHelper webview_test_helper_;
  content::TestBrowserThread ui_thread_;

  DISALLOW_COPY_AND_ASSIGN(WebViewInteractiveUiTest);
};

TEST_F(WebViewInteractiveUiTest, TextInputClientIsUpToDate) {
  // WebViewInteractiveUiTest.TextInputClientIsUpToDate needs
  // kEnableTextInputFocusManager flag to be enabled.
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  cmd_line->AppendSwitch(switches::kEnableTextInputFocusManager);

  // Create a top level widget and a webview as its content.
  views::Widget* widget = CreateTopLevelFramelessPlatformWidget();
  views::WebView* webview = new views::WebView(browser_context());
  widget->SetContentsView(webview);
  widget->Show();
  webview->RequestFocus();

  ui::TextInputFocusManager* text_input_focus_manager =
      ui::TextInputFocusManager::GetInstance();
  const ui::TextInputClient* null_text_input_client = NULL;
  EXPECT_EQ(null_text_input_client, webview->GetTextInputClient());
  EXPECT_EQ(text_input_focus_manager->GetFocusedTextInputClient(),
            webview->GetTextInputClient());

  // Case 1: Creates a new WebContents.
  webview->GetWebContents();
  webview->RequestFocus();
  ui::TextInputClient* client1 = webview->GetTextInputClient();
  EXPECT_NE(null_text_input_client, client1);
  EXPECT_EQ(text_input_focus_manager->GetFocusedTextInputClient(), client1);

  // Case 2: Replaces a WebContents by SetWebContents().
  content::WebContents::CreateParams params(browser_context());
  scoped_ptr<content::WebContents> web_contents(
      content::WebContents::Create(params));
  webview->SetWebContents(web_contents.get());
  ui::TextInputClient* client2 = webview->GetTextInputClient();
  EXPECT_NE(null_text_input_client, client2);
  EXPECT_EQ(text_input_focus_manager->GetFocusedTextInputClient(), client2);
  EXPECT_NE(client1, client2);

  widget->Close();
  RunPendingMessages();
}

}  // namespace
