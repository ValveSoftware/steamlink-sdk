// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/notification_types.h"
#include "content/public/common/content_switches.h"
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
  int request_id = kInvalidId;
  int number_of_matches = 0;
  int active_match_ordinal = 0;
};

}  // namespace

class TestWebContentsDelegate : public WebContentsDelegate {
 public:
  TestWebContentsDelegate()
      : last_finished_request_id_(kInvalidId),
        waiting_request_id_(kInvalidId) {}
  ~TestWebContentsDelegate() override {}

  // Waits for the final reply to the find request with ID |request_id|.
  void WaitForFinalReply(int request_id) {
    if (last_finished_request_id_ >= request_id)
      return;

    waiting_request_id_ = request_id;
    find_message_loop_runner_ = new content::MessageLoopRunner;
    find_message_loop_runner_->Run();
  }

  // Returns the current find results.
  FindResults GetFindResults() {
    return current_results_;
  }

#if defined(OS_ANDROID)
  // Waits for the next find reply. This is useful for waiting for a single
  // match to be activated, which results in a single find reply (without a
  // unique request ID).
  void WaitForNextReply() {
    waiting_request_id_ = 0;
    find_message_loop_runner_ = new content::MessageLoopRunner;
    find_message_loop_runner_->Run();
  }

  // Waits for all of the find match rects to be received.
  void WaitForMatchRects() {
    match_rects_message_loop_runner_ = new content::MessageLoopRunner;
    match_rects_message_loop_runner_->Run();
  }

  const std::vector<gfx::RectF>& find_match_rects() const {
    return find_match_rects_;
  }

  const gfx::RectF& active_match_rect() const {
    return active_match_rect_;
  }
#endif

 private:
  // WebContentsDelegate override.
  void FindReply(WebContents* web_contents,
                 int request_id,
                 int number_of_matches,
                 const gfx::Rect& selection_rect,
                 int active_match_ordinal,
                 bool final_update) override {
    // Update the current results.
    if (request_id > current_results_.request_id)
      current_results_.request_id = request_id;
    if (number_of_matches != -1)
      current_results_.number_of_matches = number_of_matches;
    if (active_match_ordinal != -1)
      current_results_.active_match_ordinal = active_match_ordinal;

    if (final_update)
      last_finished_request_id_ = request_id;

    // If we are waiting for a final reply and this is it, stop waiting.
    if (find_message_loop_runner_.get() &&
        last_finished_request_id_ >= waiting_request_id_) {
      find_message_loop_runner_->Quit();
    }
  }

#if defined(OS_ANDROID)
  void FindMatchRectsReply(WebContents* web_contents,
                           int version,
                           const std::vector<gfx::RectF>& rects,
                           const gfx::RectF& active_rect) override {
    // Update the current rects.
    find_match_rects_ = rects;
    active_match_rect_ = active_rect;

    // If we are waiting for match rects, stop waiting.
    if (match_rects_message_loop_runner_.get())
      match_rects_message_loop_runner_->Quit();
  }

  std::vector<gfx::RectF> find_match_rects_;

  gfx::RectF active_match_rect_;
#endif

  // The latest known results from the current find request.
  FindResults current_results_;

  // The ID of the last find request to finish (all replies received).
  int last_finished_request_id_;

  // If waiting using |find_message_loop_runner_|, this is the ID of the find
  // request being waited for.
  int waiting_request_id_;

  scoped_refptr<content::MessageLoopRunner> find_message_loop_runner_;
  scoped_refptr<content::MessageLoopRunner> match_rects_message_loop_runner_;

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

    FrameTreeNode* root =
        static_cast<WebContentsImpl*>(shell()->web_contents())->
        GetFrameTree()->root();

    LoadMultiFramePageChildFrames(height, cross_process, root);
  }

  // Reloads the child frame cross-process.
  void MakeChildFrameCrossProcess() {
    FrameTreeNode* root =
        static_cast<WebContentsImpl*>(shell()->web_contents())->
        GetFrameTree()->root();

    TestNavigationObserver observer(shell()->web_contents());

    FrameTreeNode* child = root->child_at(0);
    GURL url(embedded_test_server()->GetURL("b.com",
                                            child->current_url().path()));
    NavigateFrameToURL(child, url);
    EXPECT_EQ(url, observer.last_navigation_url());
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }

  void Find(const std::string& search_text,
            const blink::WebFindOptions& options) {
    contents()->Find(++last_request_id_,
                     base::UTF8ToUTF16(search_text),
                     options);
  }

  void WaitForFinalReply() const {
    delegate()->WaitForFinalReply(last_request_id_);
  }

  WebContents* contents() const {
    return shell()->web_contents();
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

// TODO(crbug.com/615291): These tests sometimes fail on the
// linux_android_rel_ng trybot.
#if defined(OS_ANDROID) && defined(NDEBUG)
#define MAYBE_Basic DISABLED_Basic
#define MAYBE_CharacterByCharacter DISABLED_CharacterByCharacter
#define MAYBE_RapidFire DISABLED_RapidFire
#define MAYBE_RemoveFrame DISABLED_RemoveFrame
#define MAYBE_HiddenFrame DISABLED_HiddenFrame
#define MAYBE_FindMatchRects DISABLED_FindMatchRects
#define MAYBE_ActivateNearestFindMatch DISABLED_ActivateNearestFindMatch
#else
#define MAYBE_Basic Basic
#define MAYBE_CharacterByCharacter CharacterByCharacter
#define MAYBE_RapidFire RapidFire
#define MAYBE_RemoveFrame RemoveFrame
#define MAYBE_HiddenFrame HiddenFrame
#define MAYBE_FindMatchRects FindMatchRects
#define MAYBE_ActivateNearestFindMatch ActivateNearestFindMatch
#endif


// Tests basic find-in-page functionality (such as searching forward and
// backward) and check for correct results at each step.
IN_PROC_BROWSER_TEST_P(FindRequestManagerTest, MAYBE_Basic) {
  LoadAndWait("/find_in_page.html");
  if (GetParam())
    MakeChildFrameCrossProcess();

  blink::WebFindOptions options;
  Find("result", options);
  WaitForFinalReply();

  FindResults results = delegate()->GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  EXPECT_EQ(19, results.number_of_matches);
  EXPECT_EQ(1, results.active_match_ordinal);

  options.findNext = true;
  for (int i = 2; i <= 10; ++i) {
    Find("result", options);
    WaitForFinalReply();

    results = delegate()->GetFindResults();
    EXPECT_EQ(last_request_id(), results.request_id);
    EXPECT_EQ(19, results.number_of_matches);
    EXPECT_EQ(i, results.active_match_ordinal);
  }

  options.forward = false;
  for (int i = 9; i >= 5; --i) {
    Find("result", options);
    WaitForFinalReply();

    results = delegate()->GetFindResults();
    EXPECT_EQ(last_request_id(), results.request_id);
    EXPECT_EQ(19, results.number_of_matches);
    EXPECT_EQ(i, results.active_match_ordinal);
  }
}

// Tests searching for a word character-by-character, as would typically be done
// by a user typing into the find bar.
IN_PROC_BROWSER_TEST_P(FindRequestManagerTest, MAYBE_CharacterByCharacter) {
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
  WaitForFinalReply();

  FindResults results = delegate()->GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  EXPECT_EQ(19, results.number_of_matches);
  EXPECT_EQ(1, results.active_match_ordinal);
}

// Tests sending a large number of find requests subsequently.
IN_PROC_BROWSER_TEST_P(FindRequestManagerTest, MAYBE_RapidFire) {
  LoadAndWait("/find_in_page.html");
  if (GetParam())
    MakeChildFrameCrossProcess();

  blink::WebFindOptions options;
  Find("result", options);

  options.findNext = true;
  for (int i = 2; i <= 1000; ++i)
    Find("result", options);
  WaitForFinalReply();

  FindResults results = delegate()->GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  EXPECT_EQ(19, results.number_of_matches);
  EXPECT_EQ(last_request_id() % results.number_of_matches,
            results.active_match_ordinal);
}

// Tests removing a frame during a find session.
IN_PROC_BROWSER_TEST_P(FindRequestManagerTest, MAYBE_RemoveFrame) {
  LoadMultiFramePage(2 /* height */, GetParam() /* cross_process */);

  blink::WebFindOptions options;
  Find("result", options);
  options.findNext = true;
  options.forward = false;
  Find("result", options);
  Find("result", options);
  Find("result", options);
  Find("result", options);
  Find("result", options);
  WaitForFinalReply();

  FindResults results = delegate()->GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  EXPECT_EQ(21, results.number_of_matches);
  EXPECT_EQ(17, results.active_match_ordinal);

  // Remove a frame.
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell()->web_contents())->
      GetFrameTree()->root();
  root->RemoveChild(root->child_at(0));

  // The number of matches and active match ordinal should update automatically
  // to exclude the matches from the removed frame.
  results = delegate()->GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  EXPECT_EQ(12, results.number_of_matches);
  EXPECT_EQ(8, results.active_match_ordinal);

  // TODO(paulemeyer): Once adding frames mid-session is handled, test that too.
}

// Tests Searching in a hidden frame. Matches in the hidden frame should be
// ignored.
IN_PROC_BROWSER_TEST_F(FindRequestManagerTest, MAYBE_HiddenFrame) {
  LoadAndWait("/find_in_hidden_frame.html");

  blink::WebFindOptions default_options;
  Find("hello", default_options);
  WaitForFinalReply();
  FindResults results = delegate()->GetFindResults();

  EXPECT_EQ(last_request_id(), results.request_id);
  EXPECT_EQ(1, results.number_of_matches);
  EXPECT_EQ(1, results.active_match_ordinal);
}

#if defined(OS_ANDROID)
// Tests requesting find match rects.
IN_PROC_BROWSER_TEST_F(FindRequestManagerTest, MAYBE_FindMatchRects) {
  LoadAndWait("/find_in_page.html");

  blink::WebFindOptions default_options;
  Find("result", default_options);
  WaitForFinalReply();
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
IN_PROC_BROWSER_TEST_F(FindRequestManagerTest, MAYBE_ActivateNearestFindMatch) {
  LoadAndWait("/find_in_page.html");

  blink::WebFindOptions default_options;
  Find("result", default_options);
  WaitForFinalReply();
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
    contents()->ActivateNearestFindResult(
        rects[order[i]].CenterPoint().x(), rects[order[i]].CenterPoint().y());
    delegate()->WaitForNextReply();
    EXPECT_EQ(order[i] + 1, delegate()->GetFindResults().active_match_ordinal);
  }
}
#endif  // defined(OS_ANDROID)

}  // namespace content
