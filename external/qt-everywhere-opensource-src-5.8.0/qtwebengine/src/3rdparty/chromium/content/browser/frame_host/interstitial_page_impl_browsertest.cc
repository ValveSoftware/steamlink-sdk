// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/interstitial_page_impl.h"

#include <tuple>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "build/build_config.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/clipboard_messages.h"
#include "content/common/frame_messages.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/interstitial_page_delegate.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "ipc/message_filter.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/test/test_clipboard.h"

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

// A title watcher for interstitial pages. The existing TitleWatcher does not
// work for interstitial pages. Note that this title watcher waits for the
// title update IPC message not the actual title update. So, the new title is
// probably not propagated completely, yet.
class InterstitialTitleUpdateWatcher : public BrowserMessageFilter {
 public:
  explicit InterstitialTitleUpdateWatcher(InterstitialPage* interstitial)
      : BrowserMessageFilter(FrameMsgStart) {
    interstitial->GetMainFrame()->GetProcess()->AddFilter(this);
  }

  void InitWait(const std::string& expected_title) {
    DCHECK(!run_loop_);
    expected_title_ = base::UTF8ToUTF16(expected_title);
    run_loop_.reset(new base::RunLoop());
  }

  void Wait() {
    DCHECK(run_loop_);
    run_loop_->Run();
    run_loop_.reset();
  }

 private:
  ~InterstitialTitleUpdateWatcher() override {}

  void OnTitleUpdateReceived(const base::string16& title) {
    DCHECK(run_loop_);
    if (title == expected_title_)
      run_loop_->Quit();
  }

  // BrowserMessageFilter:
  bool OnMessageReceived(const IPC::Message& message) override {
    if (!run_loop_)
      return false;

    if (message.type() == FrameHostMsg_UpdateTitle::ID) {
      FrameHostMsg_UpdateTitle::Param params;
      if (FrameHostMsg_UpdateTitle::Read(&message, &params)) {
        BrowserThread::PostTask(
            BrowserThread::UI, FROM_HERE,
            base::Bind(&InterstitialTitleUpdateWatcher::OnTitleUpdateReceived,
                       this, std::get<0>(params)));
      }
    }
    return false;
  }

  base::string16 expected_title_;
  std::unique_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(InterstitialTitleUpdateWatcher);
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
  void SetUpTestClipboard() {
#if defined(OS_WIN)
    // On Windows, clipboard reads are handled on the IO thread. So, the test
    // clipboard should be created for the IO thread.
    if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
      RunTaskOnIOThreadAndWait(
          base::Bind(&InterstitialPageImplTest::SetUpTestClipboard, this));
      return;
    }
#endif
    ui::TestClipboard::CreateForCurrentThread();
  }

  void TearDownTestClipboard() {
#if defined(OS_WIN)
    // On Windows, test clipboard is created for the IO thread. So, destroy it
    // for the IO thread, too.
    if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
      RunTaskOnIOThreadAndWait(
          base::Bind(&InterstitialPageImplTest::TearDownTestClipboard, this));
      return;
    }
#endif
    ui::Clipboard::DestroyClipboardForCurrentThread();
  }

  void SetClipboardText(const std::string& text) {
#if defined(OS_WIN)
    // On Windows, clipboard reads are handled on the IO thread. So, set the
    // text for the IO thread clipboard.
    if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
      RunTaskOnIOThreadAndWait(
          base::Bind(&InterstitialPageImplTest::SetClipboardText, this, text));
      return;
    }
#endif
    ui::ScopedClipboardWriter clipboard_writer(ui::CLIPBOARD_TYPE_COPY_PASTE);
    clipboard_writer.WriteText(base::ASCIIToUTF16(text));
  }

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
    title_update_watcher_ =
        new InterstitialTitleUpdateWatcher(interstitial_.get());

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
    title_update_watcher_->InitWait("TEXT_CHANGED");
    RenderFrameHostImpl* rfh =
        static_cast<RenderFrameHostImpl*>(interstitial_->GetMainFrame());
    rfh->GetRenderWidgetHost()->delegate()->Cut();
    clipboard_message_watcher_->WaitForWriteCommit();
    title_update_watcher_->Wait();
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
    title_update_watcher_->InitWait("TEXT_CHANGED");
    RenderFrameHostImpl* rfh =
        static_cast<RenderFrameHostImpl*>(interstitial_->GetMainFrame());
    rfh->GetRenderWidgetHost()->delegate()->Paste();
    title_update_watcher_->Wait();
  }

  void PerformSelectAll() {
    title_update_watcher_->InitWait("SELECTION_CHANGED");
    RenderFrameHostImpl* rfh =
        static_cast<RenderFrameHostImpl*>(interstitial_->GetMainFrame());
    rfh->GetRenderWidgetHost()->delegate()->SelectAll();
    title_update_watcher_->Wait();
  }

 private:
  void RunTaskOnIOThreadAndWait(const base::Closure& task) {
    base::WaitableEvent completion(
        base::WaitableEvent::ResetPolicy::AUTOMATIC,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                            base::Bind(&InterstitialPageImplTest::RunTask, this,
                                       task, &completion));
    completion.Wait();
  }

  void RunTask(const base::Closure& task, base::WaitableEvent* completion) {
    task.Run();
    completion->Signal();
  }

  std::unique_ptr<InterstitialPageImpl> interstitial_;
  scoped_refptr<ClipboardMessageWatcher> clipboard_message_watcher_;
  scoped_refptr<InterstitialTitleUpdateWatcher> title_update_watcher_;

  DISALLOW_COPY_AND_ASSIGN(InterstitialPageImplTest);
};

IN_PROC_BROWSER_TEST_F(InterstitialPageImplTest, Cut) {
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
  SetUpTestClipboard();
  SetUpInterstitialPage();

  SetClipboardText("text-to-paste");

  ASSERT_TRUE(CreateInputAndSetText(std::string()));
  ASSERT_TRUE(FocusInputAndSelectText());

  PerformPaste();

  std::string input_text;
  ASSERT_TRUE(GetInputText(&input_text));
  EXPECT_EQ("text-to-paste", input_text);

  TearDownInterstitialPage();
  TearDownTestClipboard();
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
