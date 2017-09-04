// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/interstitial_page_impl.h"

#include <tuple>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/clipboard_messages.h"
#include "content/common/frame_messages.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/interstitial_page_delegate.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "ipc/message_filter.h"

namespace content {

namespace {

class TestInterstitialPageDelegate : public InterstitialPageDelegate {
 private:
  // InterstitialPageDelegate:
  std::string GetHTMLContents() override {
    return "<html>"
           "<head>"
           "<script>"
           "function create_input_and_set_text(text) {"
           "  var input = document.createElement('input');"
           "  input.id = 'input';"
           "  document.body.appendChild(input);"
           "  document.getElementById('input').value = text;"
           "  input.addEventListener('input',"
           "      function() { document.title='TEXT_CHANGED'; });"
           "}"
           "function focus_select_input() {"
           "  document.getElementById('input').select();"
           "}"
           "function get_input_text() {"
           "  window.domAutomationController.send("
           "      document.getElementById('input').value);"
           "}"
           "function get_selection() {"
           "  window.domAutomationController.send("
           "      window.getSelection().toString());"
           "}"
           "function set_selection_change_listener() {"
           "  document.addEventListener('selectionchange',"
           "    function() { document.title='SELECTION_CHANGED'; })"
           "}"
           "</script>"
           "</head>"
           "<body>original body text</body>"
           "</html>";
  }
};

// A message filter that watches for WriteText and CommitWrite clipboard IPC
// messages to make sure cut/copy is working properly. It will mark these events
// as handled to prevent modification of the actual clipboard.
class ClipboardMessageWatcher : public IPC::MessageFilter {
 public:
  explicit ClipboardMessageWatcher(InterstitialPage* interstitial) {
    interstitial->GetMainFrame()->GetProcess()->GetChannel()->AddFilter(this);
  }

  void InitWait() {
    DCHECK(!run_loop_);
    run_loop_.reset(new base::RunLoop());
  }

  void WaitForWriteCommit() {
    DCHECK(run_loop_);
    run_loop_->Run();
    run_loop_.reset();
  }

  const std::string& last_text() const { return last_text_; }

 private:
  ~ClipboardMessageWatcher() override {}

  void OnWriteText(const std::string& text) { last_text_ = text; }

  void OnCommitWrite() {
    DCHECK(run_loop_);
    run_loop_->Quit();
  }

  // IPC::MessageFilter:
  bool OnMessageReceived(const IPC::Message& message) override {
    if (!run_loop_)
      return false;

    if (message.type() == ClipboardHostMsg_WriteText::ID) {
      ClipboardHostMsg_WriteText::Param params;
      if (ClipboardHostMsg_WriteText::Read(&message, &params)) {
        BrowserThread::PostTask(
            BrowserThread::UI, FROM_HERE,
            base::Bind(&ClipboardMessageWatcher::OnWriteText, this,
                       base::UTF16ToUTF8(std::get<1>(params))));
      }
      return true;
    }
    if (message.type() == ClipboardHostMsg_CommitWrite::ID) {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&ClipboardMessageWatcher::OnCommitWrite, this));
      return true;
    }
    return false;
  }

  std::unique_ptr<base::RunLoop> run_loop_;
  std::string last_text_;

  DISALLOW_COPY_AND_ASSIGN(ClipboardMessageWatcher);
};

}  // namespace

class InterstitialPageImplTest : public ContentBrowserTest {
 public:
  InterstitialPageImplTest() {}

  ~InterstitialPageImplTest() override {}

 protected:
  void SetUpInterstitialPage() {
    WebContentsImpl* web_contents =
        static_cast<WebContentsImpl*>(shell()->web_contents());

    // Create the interstitial page.
    TestInterstitialPageDelegate* interstitial_delegate =
        new TestInterstitialPageDelegate;
    GURL url("http://interstitial");
    interstitial_.reset(new InterstitialPageImpl(
        web_contents, static_cast<RenderWidgetHostDelegate*>(web_contents),
        true, url, interstitial_delegate));
    interstitial_->Show();
    WaitForInterstitialAttach(web_contents);

    // Focus the interstitial frame
    FrameTree* frame_tree = static_cast<RenderViewHostDelegate*>(
                                interstitial_.get())->GetFrameTree();
    frame_tree->SetFocusedFrame(frame_tree->root(),
                                frame_tree->GetMainFrame()->GetSiteInstance());

    clipboard_message_watcher_ =
        new ClipboardMessageWatcher(interstitial_.get());

    // Wait until page loads completely.
    ASSERT_TRUE(WaitForRenderFrameReady(interstitial_->GetMainFrame()));
  }

  void TearDownInterstitialPage() {
    // Close the interstitial.
    interstitial_->DontProceed();
    WaitForInterstitialDetach(shell()->web_contents());
    interstitial_.reset();
  }

  bool FocusInputAndSelectText() {
    return ExecuteScript(interstitial_->GetMainFrame(), "focus_select_input()");
  }

  bool GetInputText(std::string* input_text) {
    return ExecuteScriptAndExtractString(interstitial_->GetMainFrame(),
                                         "get_input_text()", input_text);
  }

  bool GetSelection(std::string* input_text) {
    return ExecuteScriptAndExtractString(interstitial_->GetMainFrame(),
                                         "get_selection()", input_text);
  }

  bool CreateInputAndSetText(const std::string& text) {
    return ExecuteScript(interstitial_->GetMainFrame(),
                         "create_input_and_set_text('" + text + "')");
  }

  bool SetSelectionChangeListener() {
    return ExecuteScript(interstitial_->GetMainFrame(),
                         "set_selection_change_listener()");
  }

  std::string PerformCut() {
    clipboard_message_watcher_->InitWait();
    const base::string16 expected_title = base::UTF8ToUTF16("TEXT_CHANGED");
    content::TitleWatcher title_watcher(shell()->web_contents(),
                                        expected_title);
    RenderFrameHostImpl* rfh =
        static_cast<RenderFrameHostImpl*>(interstitial_->GetMainFrame());
    rfh->GetRenderWidgetHost()->delegate()->Cut();
    clipboard_message_watcher_->WaitForWriteCommit();
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
    return clipboard_message_watcher_->last_text();
  }

  std::string PerformCopy() {
    clipboard_message_watcher_->InitWait();
    RenderFrameHostImpl* rfh =
        static_cast<RenderFrameHostImpl*>(interstitial_->GetMainFrame());
    rfh->GetRenderWidgetHost()->delegate()->Copy();
    clipboard_message_watcher_->WaitForWriteCommit();
    return clipboard_message_watcher_->last_text();
  }

  void PerformPaste() {
    const base::string16 expected_title = base::UTF8ToUTF16("TEXT_CHANGED");
    content::TitleWatcher title_watcher(shell()->web_contents(),
                                        expected_title);
    RenderFrameHostImpl* rfh =
        static_cast<RenderFrameHostImpl*>(interstitial_->GetMainFrame());
    rfh->GetRenderWidgetHost()->delegate()->Paste();
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  }

  void PerformSelectAll() {
    const base::string16 expected_title =
        base::UTF8ToUTF16("SELECTION_CHANGED");
    content::TitleWatcher title_watcher(shell()->web_contents(),
                                        expected_title);
    RenderFrameHostImpl* rfh =
        static_cast<RenderFrameHostImpl*>(interstitial_->GetMainFrame());
    rfh->GetRenderWidgetHost()->delegate()->SelectAll();
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  }

 private:
  std::unique_ptr<InterstitialPageImpl> interstitial_;
  scoped_refptr<ClipboardMessageWatcher> clipboard_message_watcher_;

  DISALLOW_COPY_AND_ASSIGN(InterstitialPageImplTest);
};

// Has errors on TSan. See https://crbug.com/631322.
#if defined(THREAD_SANITIZER)
#define MAYBE_Cut DISABLED_Cut
#else
#define MAYBE_Cut Cut
#endif
IN_PROC_BROWSER_TEST_F(InterstitialPageImplTest, MAYBE_Cut) {
  SetUpInterstitialPage();

  ASSERT_TRUE(CreateInputAndSetText("text-to-cut"));
  ASSERT_TRUE(FocusInputAndSelectText());

  std::string clipboard_text = PerformCut();
  EXPECT_EQ("text-to-cut", clipboard_text);

  std::string input_text;
  ASSERT_TRUE(GetInputText(&input_text));
  EXPECT_EQ(std::string(), input_text);

  TearDownInterstitialPage();
}

IN_PROC_BROWSER_TEST_F(InterstitialPageImplTest, Copy) {
  SetUpInterstitialPage();

  ASSERT_TRUE(CreateInputAndSetText("text-to-copy"));
  ASSERT_TRUE(FocusInputAndSelectText());

  std::string clipboard_text = PerformCopy();
  EXPECT_EQ("text-to-copy", clipboard_text);

  std::string input_text;
  ASSERT_TRUE(GetInputText(&input_text));
  EXPECT_EQ("text-to-copy", input_text);

  TearDownInterstitialPage();
}

IN_PROC_BROWSER_TEST_F(InterstitialPageImplTest, Paste) {
  BrowserTestClipboardScope clipboard;
  SetUpInterstitialPage();

  clipboard.SetText("text-to-paste");

  ASSERT_TRUE(CreateInputAndSetText(std::string()));
  ASSERT_TRUE(FocusInputAndSelectText());

  PerformPaste();

  std::string input_text;
  ASSERT_TRUE(GetInputText(&input_text));
  EXPECT_EQ("text-to-paste", input_text);

  TearDownInterstitialPage();
}

IN_PROC_BROWSER_TEST_F(InterstitialPageImplTest, SelectAll) {
  SetUpInterstitialPage();
  ASSERT_TRUE(SetSelectionChangeListener());

  std::string input_text;
  ASSERT_TRUE(GetSelection(&input_text));
  EXPECT_EQ(std::string(), input_text);

  PerformSelectAll();

  ASSERT_TRUE(GetSelection(&input_text));
  EXPECT_EQ("original body text", input_text);

  TearDownInterstitialPage();
}

}  // namespace content
