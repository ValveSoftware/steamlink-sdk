// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/notification_types.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/WebKit/public/web/WebFindOptions.h"

namespace content {

namespace {

const int kInvalidId = -1;

// The results of a find request.
struct FindResults {
  FindResults(int request_id, int number_of_matches, int active_match_ordinal)
      : request_id(request_id),
        number_of_matches(number_of_matches),
        active_match_ordinal(active_match_ordinal) {}
  FindResults() : FindResults(kInvalidId, 0, 0) {}

  int request_id;
  int number_of_matches;
  int active_match_ordinal;
};

}  // namespace

class TestWebContentsDelegate : public WebContentsDelegate {
 public:
  TestWebContentsDelegate()
      : last_request_id_(kInvalidId),
        last_finished_request_id_(kInvalidId),
        next_reply_received_(false),
        record_replies_(false),
        waiting_for_(NOTHING) {}
  ~TestWebContentsDelegate() override {}

  // Returns the current find results.
  const FindResults& GetFindResults() const {
    return current_results_;
  }

  // Waits for all pending replies to be received.
  void WaitForFinalReply() {
    if (last_finished_request_id_ >= last_request_id_)
      return;

    WaitFor(FINAL_REPLY);
  }

  // Waits for the next find reply. This is useful for waiting for a single
  // match to be activated, or for a new frame to be searched.
  void WaitForNextReply() {
    if (next_reply_received_)
      return;

    WaitFor(NEXT_REPLY);
  }

  // Indicates that the next find reply from this point will be the one to wait
  // for when WaitForNextReply() is called. It may be the case that the reply
  // comes before the call to WaitForNextReply(), in which case it will return
  // immediately.
  void MarkNextReply() {
    next_reply_received_ = false;
  }

  // Called when a new find request is issued, so the delegate knows the last
  // request ID.
  void UpdateLastRequest(int request_id) {
    last_request_id_ = request_id;
  }

  // From when this function is called, all replies coming in via FindReply()
  // will be recorded. These replies can be retrieved via GetReplyRecord().
  void StartReplyRecord() {
    reply_record_.clear();
    record_replies_ = true;
  }

  // Retreives the results from the find replies recorded since the last call to
  // StartReplyRecord(). Calling this function also stops the recording new find
  // replies.
  const std::vector<FindResults>& GetReplyRecord() {
    record_replies_ = false;
    return reply_record_;
  }

#if defined(OS_ANDROID)
  // Waits for all of the find match rects to be received.
  void WaitForMatchRects() {
    WaitFor(MATCH_RECTS);
  }

  const std::vector<gfx::RectF>& find_match_rects() const {
    return find_match_rects_;
  }

  const gfx::RectF& active_match_rect() const {
    return active_match_rect_;
  }
#endif

 private:
  enum WaitingFor {
    NOTHING,
    FINAL_REPLY,
    NEXT_REPLY,
#if defined(OS_ANDROID)
    MATCH_RECTS
#endif
  };

  // WebContentsDelegate override.
  void FindReply(WebContents* web_contents,
                 int request_id,
                 int number_of_matches,
                 const gfx::Rect& selection_rect,
                 int active_match_ordinal,
                 bool final_update) override {
    if (record_replies_) {
      reply_record_.emplace_back(
          request_id, number_of_matches, active_match_ordinal);
    }

    // Update the current results.
    if (request_id > current_results_.request_id)
      current_results_.request_id = request_id;
    if (number_of_matches != -1)
      current_results_.number_of_matches = number_of_matches;
    if (active_match_ordinal != -1)
      current_results_.active_match_ordinal = active_match_ordinal;

    if (!final_update)
      return;

    if (request_id > last_finished_request_id_)
      last_finished_request_id_ = request_id;
    next_reply_received_ = true;

    // If we are waiting for this find reply, stop waiting.
    if (waiting_for_ == NEXT_REPLY ||
        (waiting_for_ == FINAL_REPLY &&
         last_finished_request_id_ >= last_request_id_)) {
      StopWaiting();
    }
  }

  // Uses |message_loop_runner_| to wait for various things.
  void WaitFor(WaitingFor wait_for) {
    ASSERT_EQ(NOTHING, waiting_for_);
    ASSERT_NE(NOTHING, wait_for);

    // Wait for |wait_for|.
    waiting_for_ = wait_for;
    message_loop_runner_ = new content::MessageLoopRunner;
    message_loop_runner_->Run();

    // Done waiting.
    waiting_for_ = NOTHING;
    message_loop_runner_ = nullptr;
  }

  // Stop waiting for |waiting_for_|.
  void StopWaiting() {
    if (!message_loop_runner_.get())
      return;

    ASSERT_NE(NOTHING, waiting_for_);
    message_loop_runner_->Quit();
  }

#if defined(OS_ANDROID)
  // WebContentsDelegate override.
  void FindMatchRectsReply(WebContents* web_contents,
                           int version,
                           const std::vector<gfx::RectF>& rects,
                           const gfx::RectF& active_rect) override {
    // Update the current rects.
    find_match_rects_ = rects;
    active_match_rect_ = active_rect;

    // If we are waiting for match rects, stop waiting.
    if (waiting_for_ == MATCH_RECTS)
      StopWaiting();
  }

  std::vector<gfx::RectF> find_match_rects_;

  gfx::RectF active_match_rect_;
#endif

  // The latest known results from the current find request.
  FindResults current_results_;

  // The ID of the last find request issued.
  int last_request_id_;

  // The ID of the last find request to finish (all replies received).
  int last_finished_request_id_;

  // Indicates whether the next reply after MarkNextReply() has been received.
  bool next_reply_received_;

  // Indicates whether the find results from incoming find replies are currently
  // being recorded.
  bool record_replies_;

  // A record of all find replies that have come in via FindReply() since
  // StartReplyRecor() was last called.
  std::vector<FindResults> reply_record_;

  // Indicates what |message_loop_runner_| is waiting for, if anything.
  WaitingFor waiting_for_;

  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(TestWebContentsDelegate);
};

class FindRequestManagerTest : public ContentBrowserTest,
                               public testing::WithParamInterface<bool> {
 public:
  FindRequestManagerTest()
      : normal_delegate_(nullptr),
        last_request_id_(0) {}
  ~FindRequestManagerTest() override {}

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());

    // Swap the WebContents's delegate for our test delegate.
    normal_delegate_ = contents()->GetDelegate();
    contents()->SetDelegate(&test_delegate_);
  }

  void TearDownOnMainThread() override {
    // Swap the WebContents's delegate back to its usual delegate.
    contents()->SetDelegate(normal_delegate_);
  }

#if !defined(OS_ANDROID)
  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolateAllSitesForTesting(command_line);
  }
#endif

 protected:
  // Navigates to |url| and waits for it to finish loading.
  void LoadAndWait(const std::string& url) {
    TestNavigationObserver navigation_observer(contents());
    NavigateToURL(shell(), embedded_test_server()->GetURL("a.com", url));
    EXPECT_TRUE(navigation_observer.last_navigation_succeeded());
  }

  // Loads a multi-frame page. The page will have a full binary frame tree of
  // height |height|. If |cross_process| is true, child frames will be loaded
  // cross-process.
  void LoadMultiFramePage(int height, bool cross_process) {
    LoadAndWait("/find_in_page_multi_frame.html");
    FrameTreeNode* root = contents()->GetFrameTree()->root();
    LoadMultiFramePageChildFrames(height, cross_process, root);
  }

  // Reloads the child frame cross-process.
  void MakeChildFrameCrossProcess() {
    FrameTreeNode* root = contents()->GetFrameTree()->root();
    FrameTreeNode* child = root->child_at(0);
    GURL url(embedded_test_server()->GetURL(
        "b.com", child->current_url().path()));

    TestNavigationObserver observer(shell()->web_contents());
    NavigateFrameToURL(child, url);
    EXPECT_EQ(url, observer.last_navigation_url());
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }

  void Find(const std::string& search_text,
            const blink::WebFindOptions& options) {
    delegate()->UpdateLastRequest(++last_request_id_);
    contents()->Find(last_request_id_,
                     base::UTF8ToUTF16(search_text),
                     options);
  }

  WebContentsImpl* contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  TestWebContentsDelegate* delegate() const {
    return static_cast<TestWebContentsDelegate*>(contents()->GetDelegate());
  }

  int last_request_id() const {
    return last_request_id_;
  }

 private:
  // Helper function for LoadMultiFramePage. Loads child frames until the frame
  // tree rooted at |root| is a full binary tree of height |height|.
  void LoadMultiFramePageChildFrames(int height,
                                     bool cross_process,
                                     FrameTreeNode* root) {
    if (height == 0)
      return;

    std::string hostname = root->current_origin().host();
    if (cross_process)
      hostname.insert(0, 1, 'a');
    GURL url(embedded_test_server()->GetURL(hostname,
                                            "/find_in_page_multi_frame.html"));

    TestNavigationObserver observer(shell()->web_contents());

    FrameTreeNode* child = root->child_at(0);
    NavigateFrameToURL(child, url);
    EXPECT_TRUE(observer.last_navigation_succeeded());
    LoadMultiFramePageChildFrames(height - 1, cross_process, child);

    child = root->child_at(1);
    NavigateFrameToURL(child, url);
    EXPECT_TRUE(observer.last_navigation_succeeded());
    LoadMultiFramePageChildFrames(height - 1, cross_process, child);
  }

  TestWebContentsDelegate test_delegate_;
  WebContentsDelegate* normal_delegate_;

  // The ID of the last find request requested.
  int last_request_id_;

  DISALLOW_COPY_AND_ASSIGN(FindRequestManagerTest);
};

// Frames are made cross-process when the test param is set to
// true. Cross-process frames are not used on android.
#if defined(OS_ANDROID)
INSTANTIATE_TEST_CASE_P(
    FindRequestManagerTests, FindRequestManagerTest, testing::Values(false));
#else
INSTANTIATE_TEST_CASE_P(
    FindRequestManagerTests, FindRequestManagerTest, testing::Bool());
#endif

// TODO(crbug.com/615291): These tests frequently fail on Android.
#if defined(OS_ANDROID)
#define MAYBE(x) DISABLED_##x
#else
#define MAYBE(x) x
#endif


// Tests basic find-in-page functionality (such as searching forward and
// backward) and check for correct results at each step.
IN_PROC_BROWSER_TEST_P(FindRequestManagerTest, MAYBE(Basic)) {
  LoadAndWait("/find_in_page.html");
  if (GetParam())
    MakeChildFrameCrossProcess();

  blink::WebFindOptions options;
  Find("result", options);
  delegate()->WaitForFinalReply();

  FindResults results = delegate()->GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  EXPECT_EQ(19, results.number_of_matches);
  EXPECT_EQ(1, results.active_match_ordinal);

  options.findNext = true;
  for (int i = 2; i <= 10; ++i) {
    Find("result", options);
    delegate()->WaitForFinalReply();

    results = delegate()->GetFindResults();
    EXPECT_EQ(last_request_id(), results.request_id);
    EXPECT_EQ(19, results.number_of_matches);
    EXPECT_EQ(i, results.active_match_ordinal);
  }

  options.forward = false;
  for (int i = 9; i >= 5; --i) {
    Find("result", options);
    delegate()->WaitForFinalReply();

    results = delegate()->GetFindResults();
    EXPECT_EQ(last_request_id(), results.request_id);
    EXPECT_EQ(19, results.number_of_matches);
    EXPECT_EQ(i, results.active_match_ordinal);
  }
}

// Tests searching for a word character-by-character, as would typically be done
// by a user typing into the find bar.
IN_PROC_BROWSER_TEST_P(FindRequestManagerTest, MAYBE(CharacterByCharacter)) {
  LoadAndWait("/find_in_page.html");
  if (GetParam())
    MakeChildFrameCrossProcess();

  blink::WebFindOptions default_options;
  Find("r", default_options);
  Find("re", default_options);
  Find("res", default_options);
  Find("resu", default_options);
  Find("resul", default_options);
  Find("result", default_options);
  delegate()->WaitForFinalReply();

  FindResults results = delegate()->GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  EXPECT_EQ(19, results.number_of_matches);
  EXPECT_EQ(1, results.active_match_ordinal);
}

// Tests sending a large number of find requests subsequently.
IN_PROC_BROWSER_TEST_P(FindRequestManagerTest, MAYBE(RapidFire)) {
  LoadAndWait("/find_in_page.html");
  if (GetParam())
    MakeChildFrameCrossProcess();

  blink::WebFindOptions options;
  Find("result", options);

  options.findNext = true;
  for (int i = 2; i <= 1000; ++i)
    Find("result", options);
  delegate()->WaitForFinalReply();

  FindResults results = delegate()->GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  EXPECT_EQ(19, results.number_of_matches);
  EXPECT_EQ(last_request_id() % results.number_of_matches,
            results.active_match_ordinal);
}

// Tests removing a frame during a find session.
IN_PROC_BROWSER_TEST_P(FindRequestManagerTest, MAYBE(RemoveFrame)) {
  LoadMultiFramePage(2 /* height */, GetParam() /* cross_process */);

  blink::WebFindOptions options;
  Find("result", options);
  delegate()->WaitForFinalReply();
  options.findNext = true;
  options.forward = false;
  Find("result", options);
  Find("result", options);
  Find("result", options);
  Find("result", options);
  Find("result", options);
  delegate()->WaitForFinalReply();

  FindResults results = delegate()->GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  EXPECT_EQ(21, results.number_of_matches);
  EXPECT_EQ(17, results.active_match_ordinal);

  // Remove a frame.
  FrameTreeNode* root = contents()->GetFrameTree()->root();
  root->RemoveChild(root->child_at(0));

  // The number of matches and active match ordinal should update automatically
  // to exclude the matches from the removed frame.
  results = delegate()->GetFindResults();
  EXPECT_EQ(12, results.number_of_matches);
  EXPECT_EQ(8, results.active_match_ordinal);
}

// Tests adding a frame during a find session.
IN_PROC_BROWSER_TEST_P(FindRequestManagerTest, MAYBE(AddFrame)) {
  LoadMultiFramePage(2 /* height */, GetParam() /* cross_process */);

  blink::WebFindOptions options;
  Find("result", options);
  options.findNext = true;
  Find("result", options);
  Find("result", options);
  Find("result", options);
  Find("result", options);
  delegate()->WaitForFinalReply();

  FindResults results = delegate()->GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  EXPECT_EQ(21, results.number_of_matches);
  EXPECT_EQ(5, results.active_match_ordinal);

  // Add a frame. It contains 5 new matches.
  std::string url = embedded_test_server()->GetURL(
      GetParam() ? "b.com" : "a.com", "/find_in_simple_page.html").spec();
  std::string script = std::string() +
      "var frame = document.createElement('iframe');" +
      "frame.src = '" + url + "';" +
      "document.body.appendChild(frame);";
  delegate()->MarkNextReply();
  ASSERT_TRUE(ExecuteScript(shell(), script));
  delegate()->WaitForNextReply();

  // The number of matches should update automatically to include the matches
  // from the newly added frame.
  results = delegate()->GetFindResults();
  EXPECT_EQ(26, results.number_of_matches);
  EXPECT_EQ(5, results.active_match_ordinal);
}

// Tests adding a frame during a find session where there were previously no
// matches.
IN_PROC_BROWSER_TEST_F(FindRequestManagerTest, MAYBE(AddFrameAfterNoMatches)) {
  TestNavigationObserver navigation_observer(contents());
  NavigateToURL(shell(), GURL("about:blank"));
  EXPECT_TRUE(navigation_observer.last_navigation_succeeded());

  blink::WebFindOptions default_options;
  Find("result", default_options);
  delegate()->WaitForFinalReply();

  // Initially, there are no matches on the page.
  FindResults results = delegate()->GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  EXPECT_EQ(0, results.number_of_matches);
  EXPECT_EQ(0, results.active_match_ordinal);

  // Add a frame. It contains 5 new matches.
  std::string url =
      embedded_test_server()->GetURL("/find_in_simple_page.html").spec();
  std::string script = std::string() +
      "var frame = document.createElement('iframe');" +
      "frame.src = '" + url + "';" +
      "document.body.appendChild(frame);";
  delegate()->MarkNextReply();
  ASSERT_TRUE(ExecuteScript(shell(), script));
  delegate()->WaitForNextReply();

  // The matches from the new frame should be found automatically, and the first
  // match in the frame should be activated.
  results = delegate()->GetFindResults();
  EXPECT_EQ(5, results.number_of_matches);
  EXPECT_EQ(1, results.active_match_ordinal);
}

// Tests a frame navigating to a different page during a find session.
IN_PROC_BROWSER_TEST_P(FindRequestManagerTest, MAYBE(NavigateFrame)) {
  LoadMultiFramePage(2 /* height */, GetParam() /* cross_process */);

  blink::WebFindOptions options;
  Find("result", options);
  options.findNext = true;
  options.forward = false;
  Find("result", options);
  Find("result", options);
  Find("result", options);
  delegate()->WaitForFinalReply();

  FindResults results = delegate()->GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  EXPECT_EQ(21, results.number_of_matches);
  EXPECT_EQ(19, results.active_match_ordinal);

  // Navigate one of the empty frames to a page with 5 matches.
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell()->web_contents())->
      GetFrameTree()->root();
  GURL url(embedded_test_server()->GetURL(
      GetParam() ? "b.com" : "a.com", "/find_in_simple_page.html"));
  delegate()->MarkNextReply();
  TestNavigationObserver navigation_observer(contents());
  NavigateFrameToURL(root->child_at(0)->child_at(1)->child_at(0), url);
  EXPECT_TRUE(navigation_observer.last_navigation_succeeded());
  delegate()->WaitForNextReply();

  // The navigation results in an extra reply before the one we care about. This
  // extra reply happens because the RenderFrameHost changes before it navigates
  // (because the navigation is cross-origin). The first reply will not change
  // the number of matches because the frame that is navigating was empty
  // before.
  if (delegate()->GetFindResults().number_of_matches == 21) {
    delegate()->MarkNextReply();
    delegate()->WaitForNextReply();
  }

  // The number of matches and the active match ordinal should update
  // automatically to include the new matches.
  results = delegate()->GetFindResults();
  EXPECT_EQ(26, results.number_of_matches);
  EXPECT_EQ(24, results.active_match_ordinal);
}

// Tests Searching in a hidden frame. Matches in the hidden frame should be
// ignored.
IN_PROC_BROWSER_TEST_F(FindRequestManagerTest, MAYBE(HiddenFrame)) {
  LoadAndWait("/find_in_hidden_frame.html");

  blink::WebFindOptions default_options;
  Find("hello", default_options);
  delegate()->WaitForFinalReply();
  FindResults results = delegate()->GetFindResults();

  EXPECT_EQ(last_request_id(), results.request_id);
  EXPECT_EQ(1, results.number_of_matches);
  EXPECT_EQ(1, results.active_match_ordinal);
}

// Tests that new matches can be found in dynamically added text.
IN_PROC_BROWSER_TEST_P(FindRequestManagerTest, MAYBE(FindNewMatches)) {
  LoadAndWait("/find_in_dynamic_page.html");

  blink::WebFindOptions options;
  Find("result", options);
  options.findNext = true;
  Find("result", options);
  Find("result", options);
  delegate()->WaitForFinalReply();

  FindResults results = delegate()->GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  EXPECT_EQ(3, results.number_of_matches);
  EXPECT_EQ(3, results.active_match_ordinal);

  // Dynamically add new text to the page. This text contains 5 new matches for
  // "result".
  ASSERT_TRUE(ExecuteScript(contents()->GetMainFrame(), "addNewText()"));

  Find("result", options);
  delegate()->WaitForFinalReply();

  results = delegate()->GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  EXPECT_EQ(8, results.number_of_matches);
  EXPECT_EQ(4, results.active_match_ordinal);
}

IN_PROC_BROWSER_TEST_F(FindRequestManagerTest, MAYBE(FindInPage_Issue627799)) {
  LoadAndWait("/find_in_long_page.html");

  blink::WebFindOptions options;
  Find("42", options);
  delegate()->WaitForFinalReply();

  FindResults results = delegate()->GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  EXPECT_EQ(970, results.number_of_matches);
  EXPECT_EQ(1, results.active_match_ordinal);

  delegate()->StartReplyRecord();
  options.findNext = true;
  options.forward = false;
  Find("42", options);
  delegate()->WaitForFinalReply();

  // This is the crux of the issue that this test guards against. Searching
  // across the frame boundary should not cause the frame to be re-scoped. If
  // the re-scope occurs, then we will see the number of matches change in one
  // of the recorded find replies.
  for (auto& reply : delegate()->GetReplyRecord()) {
    EXPECT_EQ(last_request_id(), reply.request_id);
    EXPECT_TRUE(reply.number_of_matches == kInvalidId ||
                reply.number_of_matches == results.number_of_matches);
  }
}

IN_PROC_BROWSER_TEST_F(FindRequestManagerTest, MAYBE(FindInPage_Issue644448)) {
  TestNavigationObserver navigation_observer(contents());
  NavigateToURL(shell(), GURL("about:blank"));
  EXPECT_TRUE(navigation_observer.last_navigation_succeeded());

  blink::WebFindOptions default_options;
  Find("result", default_options);
  delegate()->WaitForFinalReply();

  // Initially, there are no matches on the page.
  FindResults results = delegate()->GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  EXPECT_EQ(0, results.number_of_matches);
  EXPECT_EQ(0, results.active_match_ordinal);

  // Load a page with matches.
  LoadAndWait("/find_in_simple_page.html");

  Find("result", default_options);
  delegate()->WaitForFinalReply();

  // There should now be matches found. When the bug was present, there were
  // still no matches found.
  results = delegate()->GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  EXPECT_EQ(5, results.number_of_matches);
}

#if defined(OS_ANDROID)
// Tests requesting find match rects.
IN_PROC_BROWSER_TEST_F(FindRequestManagerTest, MAYBE(FindMatchRects)) {
  LoadAndWait("/find_in_page.html");

  blink::WebFindOptions default_options;
  Find("result", default_options);
  delegate()->WaitForFinalReply();
  EXPECT_EQ(19, delegate()->GetFindResults().number_of_matches);

  // Request the find match rects.
  contents()->RequestFindMatchRects(-1);
  delegate()->WaitForMatchRects();
  const std::vector<gfx::RectF>& rects = delegate()->find_match_rects();

  // The first match should be active.
  EXPECT_EQ(rects[0], delegate()->active_match_rect());

  // All results after the first two should be between them in find-in-page
  // coordinates. This is because results 2 to 19 are inside an iframe located
  // between results 0 and 1. This applies to the fixed div too.
  EXPECT_LT(rects[0].y(), rects[1].y());
  for (int i = 2; i < 19; ++i) {
    EXPECT_LT(rects[0].y(), rects[i].y());
    EXPECT_GT(rects[1].y(), rects[i].y());
  }

  // Result 3 should be below results 2 and 4. This is caused by the CSS
  // transform in the containing div. If the transform doesn't work then result
  // 3 will be between results 2 and 4.
  EXPECT_GT(rects[3].y(), rects[2].y());
  EXPECT_GT(rects[3].y(), rects[4].y());

  // Results 6, 7, 8 and 9 should be one below the other in that same order. If
  // overflow:scroll is not properly handled then result 8 would be below result
  // 9 or result 7 above result 6 depending on the scroll.
  EXPECT_LT(rects[6].y(), rects[7].y());
  EXPECT_LT(rects[7].y(), rects[8].y());
  EXPECT_LT(rects[8].y(), rects[9].y());

  // Results 11, 12, 13 and 14 should be between results 10 and 15, as they are
  // inside the table.
  EXPECT_GT(rects[11].y(), rects[10].y());
  EXPECT_GT(rects[12].y(), rects[10].y());
  EXPECT_GT(rects[13].y(), rects[10].y());
  EXPECT_GT(rects[14].y(), rects[10].y());
  EXPECT_LT(rects[11].y(), rects[15].y());
  EXPECT_LT(rects[12].y(), rects[15].y());
  EXPECT_LT(rects[13].y(), rects[15].y());
  EXPECT_LT(rects[14].y(), rects[15].y());

  // Result 11 should be above results 12, 13 and 14 as it's in the table
  // header.
  EXPECT_LT(rects[11].y(), rects[12].y());
  EXPECT_LT(rects[11].y(), rects[13].y());
  EXPECT_LT(rects[11].y(), rects[14].y());

  // Result 11 should also be right of results 12, 13 and 14 because of the
  // colspan.
  EXPECT_GT(rects[11].x(), rects[12].x());
  EXPECT_GT(rects[11].x(), rects[13].x());
  EXPECT_GT(rects[11].x(), rects[14].x());

  // Result 12 should be left of results 11, 13 and 14 in the table layout.
  EXPECT_LT(rects[12].x(), rects[11].x());
  EXPECT_LT(rects[12].x(), rects[13].x());
  EXPECT_LT(rects[12].x(), rects[14].x());

  // Results 13, 12 and 14 should be one above the other in that order because
  // of the rowspan and vertical-align: middle by default.
  EXPECT_LT(rects[13].y(), rects[12].y());
  EXPECT_LT(rects[12].y(), rects[14].y());

  // Result 16 should be below result 15.
  EXPECT_GT(rects[15].y(), rects[14].y());

  // Result 18 should be normalized with respect to the position:relative div,
  // and not it's immediate containing div. Consequently, result 18 should be
  // above result 17.
  EXPECT_GT(rects[17].y(), rects[18].y());
}

// Tests activating the find match nearest to a given point.
IN_PROC_BROWSER_TEST_F(FindRequestManagerTest,
                       MAYBE(ActivateNearestFindMatch)) {
  LoadAndWait("/find_in_page.html");

  blink::WebFindOptions default_options;
  Find("result", default_options);
  delegate()->WaitForFinalReply();
  EXPECT_EQ(19, delegate()->GetFindResults().number_of_matches);

  // Get the find match rects.
  contents()->RequestFindMatchRects(-1);
  delegate()->WaitForMatchRects();
  const std::vector<gfx::RectF>& rects = delegate()->find_match_rects();

  // Activate matches via points inside each of the find match rects, in an
  // arbitrary order. Check that the correct match becomes active after each
  // activation.
  int order[19] =
      {11, 13, 2, 0, 16, 5, 7, 10, 6, 1, 15, 14, 9, 17, 18, 3, 8, 12, 4};
  for (int i = 0; i < 19; ++i) {
    delegate()->MarkNextReply();
    contents()->ActivateNearestFindResult(
        rects[order[i]].CenterPoint().x(), rects[order[i]].CenterPoint().y());
    delegate()->WaitForNextReply();
    EXPECT_EQ(order[i] + 1, delegate()->GetFindResults().active_match_ordinal);
  }
}
#endif  // defined(OS_ANDROID)

}  // namespace content
