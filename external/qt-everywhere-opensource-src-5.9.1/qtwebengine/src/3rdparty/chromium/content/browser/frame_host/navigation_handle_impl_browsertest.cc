// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/navigation_handle_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/request_context_type.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/page_transition_types.h"
#include "url/url_constants.h"

namespace content {

namespace {

// Gathers data from the NavigationHandle assigned to navigations that start
// with the expected URL.
class NavigationHandleObserver : public WebContentsObserver {
 public:
  NavigationHandleObserver(WebContents* web_contents,
                           const GURL& expected_start_url)
      : WebContentsObserver(web_contents),
        handle_(nullptr),
        has_committed_(false),
        is_error_(false),
        is_main_frame_(false),
        is_parent_main_frame_(false),
        is_renderer_initiated_(true),
        is_same_page_(false),
        is_srcdoc_(false),
        was_redirected_(false),
        frame_tree_node_id_(-1),
        page_transition_(ui::PAGE_TRANSITION_LINK),
        expected_start_url_(expected_start_url) {}

  void DidStartNavigation(NavigationHandle* navigation_handle) override {
    if (handle_ || navigation_handle->GetURL() != expected_start_url_)
      return;

    handle_ = navigation_handle;
    has_committed_ = false;
    is_error_ = false;
    page_transition_ = ui::PAGE_TRANSITION_LINK;
    last_committed_url_ = GURL();

    is_main_frame_ = navigation_handle->IsInMainFrame();
    is_parent_main_frame_ = navigation_handle->IsParentMainFrame();
    is_renderer_initiated_ = navigation_handle->IsRendererInitiated();
    is_same_page_ = navigation_handle->IsSamePage();
    is_srcdoc_ = navigation_handle->IsSrcdoc();
    was_redirected_ = navigation_handle->WasServerRedirect();
    frame_tree_node_id_ = navigation_handle->GetFrameTreeNodeId();
  }

  void DidFinishNavigation(NavigationHandle* navigation_handle) override {
    if (navigation_handle != handle_)
      return;

    DCHECK_EQ(is_main_frame_, navigation_handle->IsInMainFrame());
    DCHECK_EQ(is_parent_main_frame_, navigation_handle->IsParentMainFrame());
    DCHECK_EQ(is_same_page_, navigation_handle->IsSamePage());
    DCHECK_EQ(is_renderer_initiated_, navigation_handle->IsRendererInitiated());
    DCHECK_EQ(is_srcdoc_, navigation_handle->IsSrcdoc());
    DCHECK_EQ(frame_tree_node_id_, navigation_handle->GetFrameTreeNodeId());

    was_redirected_ = navigation_handle->WasServerRedirect();

    if (navigation_handle->HasCommitted()) {
      has_committed_ = true;
      if (!navigation_handle->IsErrorPage()) {
        page_transition_ = navigation_handle->GetPageTransition();
        last_committed_url_ = navigation_handle->GetURL();
      } else {
        is_error_ = true;
      }
    } else {
      has_committed_ = false;
      is_error_ = true;
    }

    handle_ = nullptr;
  }

  bool has_committed() { return has_committed_; }
  bool is_error() { return is_error_; }
  bool is_main_frame() { return is_main_frame_; }
  bool is_parent_main_frame() { return is_parent_main_frame_; }
  bool is_renderer_initiated() { return is_renderer_initiated_; }
  bool is_same_page() { return is_same_page_; }
  bool is_srcdoc() { return is_srcdoc_; }
  bool was_redirected() { return was_redirected_; }
  int frame_tree_node_id() { return frame_tree_node_id_; }

  const GURL& last_committed_url() { return last_committed_url_; }

  ui::PageTransition page_transition() { return page_transition_; }

 private:
  // A reference to the NavigationHandle so this class will track only
  // one navigation at a time. It is set at DidStartNavigation and cleared
  // at DidFinishNavigation before the NavigationHandle is destroyed.
  NavigationHandle* handle_;
  bool has_committed_;
  bool is_error_;
  bool is_main_frame_;
  bool is_parent_main_frame_;
  bool is_renderer_initiated_;
  bool is_same_page_;
  bool is_srcdoc_;
  bool was_redirected_;
  int frame_tree_node_id_;
  ui::PageTransition page_transition_;
  GURL expected_start_url_;
  GURL last_committed_url_;
};

// A test NavigationThrottle that will return pre-determined checks and run
// callbacks when the various NavigationThrottle methods are called. It is
// not instantiated directly but through a TestNavigationThrottleInstaller.
class TestNavigationThrottle : public NavigationThrottle {
 public:
  TestNavigationThrottle(
      NavigationHandle* handle,
      NavigationThrottle::ThrottleCheckResult will_start_result,
      NavigationThrottle::ThrottleCheckResult will_redirect_result,
      NavigationThrottle::ThrottleCheckResult will_process_result,
      base::Closure did_call_will_start,
      base::Closure did_call_will_redirect,
      base::Closure did_call_will_process)
      : NavigationThrottle(handle),
        will_start_result_(will_start_result),
        will_redirect_result_(will_redirect_result),
        will_process_result_(will_process_result),
        did_call_will_start_(did_call_will_start),
        did_call_will_redirect_(did_call_will_redirect),
        did_call_will_process_(did_call_will_process) {}
  ~TestNavigationThrottle() override {}

  void Resume() { navigation_handle()->Resume(); }

  RequestContextType request_context_type() { return request_context_type_; }

 private:
  // NavigationThrottle implementation.
  NavigationThrottle::ThrottleCheckResult WillStartRequest() override {
    NavigationHandleImpl* navigation_handle_impl =
        static_cast<NavigationHandleImpl*>(navigation_handle());
    CHECK_NE(REQUEST_CONTEXT_TYPE_UNSPECIFIED,
             navigation_handle_impl->GetRequestContextType());
    request_context_type_ = navigation_handle_impl->GetRequestContextType();

    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, did_call_will_start_);
    return will_start_result_;
  }

  NavigationThrottle::ThrottleCheckResult WillRedirectRequest() override {
    NavigationHandleImpl* navigation_handle_impl =
        static_cast<NavigationHandleImpl*>(navigation_handle());
    CHECK_EQ(request_context_type_,
             navigation_handle_impl->GetRequestContextType());

    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            did_call_will_redirect_);
    return will_redirect_result_;
  }

  NavigationThrottle::ThrottleCheckResult WillProcessResponse() override {
    NavigationHandleImpl* navigation_handle_impl =
        static_cast<NavigationHandleImpl*>(navigation_handle());
    CHECK_EQ(request_context_type_,
             navigation_handle_impl->GetRequestContextType());

    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            did_call_will_process_);
    return will_process_result_;
  }

  NavigationThrottle::ThrottleCheckResult will_start_result_;
  NavigationThrottle::ThrottleCheckResult will_redirect_result_;
  NavigationThrottle::ThrottleCheckResult will_process_result_;
  base::Closure did_call_will_start_;
  base::Closure did_call_will_redirect_;
  base::Closure did_call_will_process_;
  RequestContextType request_context_type_;
};

// Install a TestNavigationThrottle on all following requests and allows waiting
// for various NavigationThrottle related events. Waiting works only for the
// immediately next navigation. New instances are needed to wait for further
// navigations.
class TestNavigationThrottleInstaller : public WebContentsObserver {
 public:
  TestNavigationThrottleInstaller(
      WebContents* web_contents,
      NavigationThrottle::ThrottleCheckResult will_start_result,
      NavigationThrottle::ThrottleCheckResult will_redirect_result,
      NavigationThrottle::ThrottleCheckResult will_process_result)
      : WebContentsObserver(web_contents),
        will_start_result_(will_start_result),
        will_redirect_result_(will_redirect_result),
        will_process_result_(will_process_result),
        will_start_called_(0),
        will_redirect_called_(0),
        will_process_called_(0),
        navigation_throttle_(nullptr) {}
  ~TestNavigationThrottleInstaller() override{};

  TestNavigationThrottle* navigation_throttle() { return navigation_throttle_; }

  void WaitForThrottleWillStart() {
    if (will_start_called_)
      return;
    will_start_loop_runner_ = new MessageLoopRunner();
    will_start_loop_runner_->Run();
    will_start_loop_runner_ = nullptr;
  }

  void WaitForThrottleWillRedirect() {
    if (will_redirect_called_)
      return;
    will_redirect_loop_runner_ = new MessageLoopRunner();
    will_redirect_loop_runner_->Run();
    will_redirect_loop_runner_ = nullptr;
  }

  void WaitForThrottleWillProcess() {
    if (will_process_called_)
      return;
    will_process_loop_runner_ = new MessageLoopRunner();
    will_process_loop_runner_->Run();
    will_process_loop_runner_ = nullptr;
  }

  int will_start_called() { return will_start_called_; }
  int will_redirect_called() { return will_redirect_called_; }
  int will_process_called() { return will_process_called_; }

 private:
  void DidStartNavigation(NavigationHandle* handle) override {
    std::unique_ptr<NavigationThrottle> throttle(new TestNavigationThrottle(
        handle, will_start_result_, will_redirect_result_, will_process_result_,
        base::Bind(&TestNavigationThrottleInstaller::DidCallWillStartRequest,
                   base::Unretained(this)),
        base::Bind(&TestNavigationThrottleInstaller::DidCallWillRedirectRequest,
                   base::Unretained(this)),
        base::Bind(&TestNavigationThrottleInstaller::DidCallWillProcessResponse,
                   base::Unretained(this))));
    navigation_throttle_ = static_cast<TestNavigationThrottle*>(throttle.get());
    handle->RegisterThrottleForTesting(std::move(throttle));
  }

  void DidFinishNavigation(NavigationHandle* handle) override {
    if (!navigation_throttle_)
      return;

    if (handle == navigation_throttle_->navigation_handle())
      navigation_throttle_ = nullptr;
  }

  void DidCallWillStartRequest() {
    will_start_called_++;
    if (will_start_loop_runner_)
      will_start_loop_runner_->Quit();
  }

  void DidCallWillRedirectRequest() {
    will_redirect_called_++;
    if (will_redirect_loop_runner_)
      will_redirect_loop_runner_->Quit();
  }

  void DidCallWillProcessResponse() {
    will_process_called_++;
    if (will_process_loop_runner_)
      will_process_loop_runner_->Quit();
  }

  NavigationThrottle::ThrottleCheckResult will_start_result_;
  NavigationThrottle::ThrottleCheckResult will_redirect_result_;
  NavigationThrottle::ThrottleCheckResult will_process_result_;
  int will_start_called_;
  int will_redirect_called_;
  int will_process_called_;
  TestNavigationThrottle* navigation_throttle_;
  scoped_refptr<MessageLoopRunner> will_start_loop_runner_;
  scoped_refptr<MessageLoopRunner> will_redirect_loop_runner_;
  scoped_refptr<MessageLoopRunner> will_process_loop_runner_;
};

// Records all navigation start URLs from the WebContents.
class NavigationStartUrlRecorder : public WebContentsObserver {
 public:
  NavigationStartUrlRecorder(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  void DidStartNavigation(NavigationHandle* navigation_handle) override {
    urls_.push_back(navigation_handle->GetURL());
  }

  const std::vector<GURL>& urls() const { return urls_; }

 private:
  std::vector<GURL> urls_;
};

}  // namespace

class NavigationHandleImplBrowserTest : public ContentBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

// Ensure that PageTransition is properly set on the NavigationHandle.
IN_PROC_BROWSER_TEST_F(NavigationHandleImplBrowserTest, VerifyPageTransition) {
  {
    // Test browser initiated navigation, which should have a PageTransition as
    // if it came from the omnibox.
    GURL url(embedded_test_server()->GetURL("/title1.html"));
    NavigationHandleObserver observer(shell()->web_contents(), url);
    ui::PageTransition expected_transition = ui::PageTransitionFromInt(
        ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);

    EXPECT_TRUE(NavigateToURL(shell(), url));

    EXPECT_TRUE(observer.has_committed());
    EXPECT_FALSE(observer.is_error());
    EXPECT_EQ(url, observer.last_committed_url());
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        observer.page_transition(), expected_transition));
  }

  {
    // Test navigating to a page with subframe. The subframe will have
    // PageTransition of type AUTO_SUBFRAME.
    GURL url(
        embedded_test_server()->GetURL("/frame_tree/page_with_one_frame.html"));
    NavigationHandleObserver observer(
        shell()->web_contents(),
        embedded_test_server()->GetURL("/cross-site/baz.com/title1.html"));
    ui::PageTransition expected_transition =
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_AUTO_SUBFRAME);

    EXPECT_TRUE(NavigateToURL(shell(), url));

    EXPECT_TRUE(observer.has_committed());
    EXPECT_FALSE(observer.is_error());
    EXPECT_EQ(embedded_test_server()->GetURL("baz.com", "/title1.html"),
              observer.last_committed_url());
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        observer.page_transition(), expected_transition));
    EXPECT_FALSE(observer.is_main_frame());
  }
}

// Ensure that the following methods on NavigationHandle behave correctly:
// * IsInMainFrame
// * IsParentMainFrame
IN_PROC_BROWSER_TEST_F(NavigationHandleImplBrowserTest, VerifyFrameTree) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c))"));
  GURL b_url(embedded_test_server()->GetURL(
      "b.com", "/cross_site_iframe_factory.html?b(c())"));
  GURL c_url(embedded_test_server()->GetURL(
      "c.com", "/cross_site_iframe_factory.html?c()"));

  NavigationHandleObserver main_observer(shell()->web_contents(), main_url);
  NavigationHandleObserver b_observer(shell()->web_contents(), b_url);
  NavigationHandleObserver c_observer(shell()->web_contents(), c_url);

  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  // Verify the main frame.
  EXPECT_TRUE(main_observer.has_committed());
  EXPECT_FALSE(main_observer.is_error());
  EXPECT_EQ(main_url, main_observer.last_committed_url());
  EXPECT_TRUE(main_observer.is_main_frame());
  EXPECT_EQ(root->frame_tree_node_id(), main_observer.frame_tree_node_id());

  // Verify the b.com frame.
  EXPECT_TRUE(b_observer.has_committed());
  EXPECT_FALSE(b_observer.is_error());
  EXPECT_EQ(b_url, b_observer.last_committed_url());
  EXPECT_FALSE(b_observer.is_main_frame());
  EXPECT_TRUE(b_observer.is_parent_main_frame());
  EXPECT_EQ(root->child_at(0)->frame_tree_node_id(),
            b_observer.frame_tree_node_id());

  // Verify the c.com frame.
  EXPECT_TRUE(c_observer.has_committed());
  EXPECT_FALSE(c_observer.is_error());
  EXPECT_EQ(c_url, c_observer.last_committed_url());
  EXPECT_FALSE(c_observer.is_main_frame());
  EXPECT_FALSE(c_observer.is_parent_main_frame());
  EXPECT_EQ(root->child_at(0)->child_at(0)->frame_tree_node_id(),
            c_observer.frame_tree_node_id());
}

// Ensure that the WasRedirected() method on NavigationHandle behaves correctly.
IN_PROC_BROWSER_TEST_F(NavigationHandleImplBrowserTest, VerifyRedirect) {
  {
    GURL url(embedded_test_server()->GetURL("/title1.html"));
    NavigationHandleObserver observer(shell()->web_contents(), url);

    EXPECT_TRUE(NavigateToURL(shell(), url));

    EXPECT_TRUE(observer.has_committed());
    EXPECT_FALSE(observer.is_error());
    EXPECT_FALSE(observer.was_redirected());
  }

  {
    GURL url(embedded_test_server()->GetURL("/cross-site/baz.com/title1.html"));
    NavigationHandleObserver observer(shell()->web_contents(), url);

    NavigateToURL(shell(), url);

    EXPECT_TRUE(observer.has_committed());
    EXPECT_FALSE(observer.is_error());
    EXPECT_TRUE(observer.was_redirected());
  }
}

// Ensure that the IsRendererInitiated() method on NavigationHandle behaves
// correctly.
IN_PROC_BROWSER_TEST_F(NavigationHandleImplBrowserTest,
                       VerifyRendererInitiated) {
  {
    // Test browser initiated navigation.
    GURL url(embedded_test_server()->GetURL("/title1.html"));
    NavigationHandleObserver observer(shell()->web_contents(), url);

    EXPECT_TRUE(NavigateToURL(shell(), url));

    EXPECT_TRUE(observer.has_committed());
    EXPECT_FALSE(observer.is_error());
    EXPECT_FALSE(observer.is_renderer_initiated());
  }

  {
    // Test a main frame + subframes navigation.
    GURL main_url(embedded_test_server()->GetURL(
        "a.com", "/cross_site_iframe_factory.html?a(b(c))"));
    GURL b_url(embedded_test_server()->GetURL(
        "b.com", "/cross_site_iframe_factory.html?b(c())"));
    GURL c_url(embedded_test_server()->GetURL(
        "c.com", "/cross_site_iframe_factory.html?c()"));

    NavigationHandleObserver main_observer(shell()->web_contents(), main_url);
    NavigationHandleObserver b_observer(shell()->web_contents(), b_url);
    NavigationHandleObserver c_observer(shell()->web_contents(), c_url);

    EXPECT_TRUE(NavigateToURL(shell(), main_url));

    // Verify that the main frame navigation is not renderer initiated.
    EXPECT_TRUE(main_observer.has_committed());
    EXPECT_FALSE(main_observer.is_error());
    EXPECT_FALSE(main_observer.is_renderer_initiated());

    // Verify that the subframe navigations are renderer initiated.
    EXPECT_TRUE(b_observer.has_committed());
    EXPECT_FALSE(b_observer.is_error());
    EXPECT_TRUE(b_observer.is_renderer_initiated());
    EXPECT_TRUE(c_observer.has_committed());
    EXPECT_FALSE(c_observer.is_error());
    EXPECT_TRUE(c_observer.is_renderer_initiated());
  }

  {
    // Test a pushState navigation.
    GURL url(embedded_test_server()->GetURL(
        "a.com", "/cross_site_iframe_factory.html?a(a())"));
    EXPECT_TRUE(NavigateToURL(shell(), url));

    FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                              ->GetFrameTree()
                              ->root();

    NavigationHandleObserver observer(
        shell()->web_contents(),
        embedded_test_server()->GetURL("a.com", "/bar"));
    EXPECT_TRUE(ExecuteScript(root->child_at(0),
                              "window.history.pushState({}, '', 'bar');"));

    EXPECT_TRUE(observer.has_committed());
    EXPECT_FALSE(observer.is_error());
    EXPECT_TRUE(observer.is_renderer_initiated());
  }
}

// Ensure that the IsSrcdoc() method on NavigationHandle behaves correctly.
IN_PROC_BROWSER_TEST_F(NavigationHandleImplBrowserTest, VerifySrcdoc) {
  GURL url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_srcdoc_frame.html"));
  NavigationHandleObserver observer(shell()->web_contents(),
                                    GURL(url::kAboutBlankURL));

  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_TRUE(observer.has_committed());
  EXPECT_FALSE(observer.is_error());
  EXPECT_TRUE(observer.is_srcdoc());
}

// Ensure that the IsSamePage() method on NavigationHandle behaves correctly.
IN_PROC_BROWSER_TEST_F(NavigationHandleImplBrowserTest, VerifySamePage) {
  GURL url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a())"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  {
    NavigationHandleObserver observer(
        shell()->web_contents(),
        embedded_test_server()->GetURL("a.com", "/foo"));
    EXPECT_TRUE(ExecuteScript(root->child_at(0),
                              "window.history.pushState({}, '', 'foo');"));

    EXPECT_TRUE(observer.has_committed());
    EXPECT_FALSE(observer.is_error());
    EXPECT_TRUE(observer.is_same_page());
  }
  {
    NavigationHandleObserver observer(
        shell()->web_contents(),
        embedded_test_server()->GetURL("a.com", "/bar"));
    EXPECT_TRUE(ExecuteScript(root->child_at(0),
                              "window.history.replaceState({}, '', 'bar');"));

    EXPECT_TRUE(observer.has_committed());
    EXPECT_FALSE(observer.is_error());
    EXPECT_TRUE(observer.is_same_page());
  }
  {
    NavigationHandleObserver observer(
        shell()->web_contents(),
        embedded_test_server()->GetURL("a.com", "/bar#frag"));
    EXPECT_TRUE(
        ExecuteScript(root->child_at(0), "window.location.replace('#frag');"));

    EXPECT_TRUE(observer.has_committed());
    EXPECT_FALSE(observer.is_error());
    EXPECT_TRUE(observer.is_same_page());
  }

  GURL about_blank_url(url::kAboutBlankURL);
  {
    NavigationHandleObserver observer(shell()->web_contents(), about_blank_url);
    EXPECT_TRUE(ExecuteScript(
        root, "document.body.appendChild(document.createElement('iframe'));"));

    EXPECT_TRUE(observer.has_committed());
    EXPECT_FALSE(observer.is_error());
    EXPECT_FALSE(observer.is_same_page());
    EXPECT_EQ(about_blank_url, observer.last_committed_url());
  }
  {
    NavigationHandleObserver observer(shell()->web_contents(), about_blank_url);
    NavigateFrameToURL(root->child_at(0), about_blank_url);

    EXPECT_TRUE(observer.has_committed());
    EXPECT_FALSE(observer.is_error());
    EXPECT_FALSE(observer.is_same_page());
    EXPECT_EQ(about_blank_url, observer.last_committed_url());
  }
}

// Ensure that a NavigationThrottle can cancel the navigation at navigation
// start.
IN_PROC_BROWSER_TEST_F(NavigationHandleImplBrowserTest, ThrottleCancelStart) {
  GURL start_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  GURL redirect_url(
      embedded_test_server()->GetURL("/cross-site/bar.com/title2.html"));
  NavigationHandleObserver observer(shell()->web_contents(), redirect_url);
  TestNavigationThrottleInstaller installer(
      shell()->web_contents(), NavigationThrottle::CANCEL,
      NavigationThrottle::PROCEED, NavigationThrottle::PROCEED);

  EXPECT_FALSE(NavigateToURL(shell(), redirect_url));

  EXPECT_FALSE(observer.has_committed());
  EXPECT_TRUE(observer.is_error());

  // The navigation should have been canceled before being redirected.
  EXPECT_FALSE(observer.was_redirected());
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), start_url);
}

// Ensure that a NavigationThrottle can cancel the navigation when a navigation
// is redirected.
IN_PROC_BROWSER_TEST_F(NavigationHandleImplBrowserTest,
                       ThrottleCancelRedirect) {
  GURL start_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  // A navigation with a redirect should be canceled.
  {
    GURL redirect_url(
        embedded_test_server()->GetURL("/cross-site/bar.com/title2.html"));
    NavigationHandleObserver observer(shell()->web_contents(), redirect_url);
    TestNavigationThrottleInstaller installer(
        shell()->web_contents(), NavigationThrottle::PROCEED,
        NavigationThrottle::CANCEL, NavigationThrottle::PROCEED);

    EXPECT_FALSE(NavigateToURL(shell(), redirect_url));

    EXPECT_FALSE(observer.has_committed());
    EXPECT_TRUE(observer.is_error());
    EXPECT_TRUE(observer.was_redirected());
    EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), start_url);
  }

  // A navigation without redirects should be successful.
  {
    GURL no_redirect_url(embedded_test_server()->GetURL("/title2.html"));
    NavigationHandleObserver observer(shell()->web_contents(), no_redirect_url);
    TestNavigationThrottleInstaller installer(
        shell()->web_contents(), NavigationThrottle::PROCEED,
        NavigationThrottle::CANCEL, NavigationThrottle::PROCEED);

    EXPECT_TRUE(NavigateToURL(shell(), no_redirect_url));

    EXPECT_TRUE(observer.has_committed());
    EXPECT_FALSE(observer.is_error());
    EXPECT_FALSE(observer.was_redirected());
    EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), no_redirect_url);
  }
}

// Ensure that a NavigationThrottle can cancel the navigation when the response
// is received.
IN_PROC_BROWSER_TEST_F(NavigationHandleImplBrowserTest,
                       ThrottleCancelResponse) {
  GURL start_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  GURL redirect_url(
      embedded_test_server()->GetURL("/cross-site/bar.com/title2.html"));
  NavigationHandleObserver observer(shell()->web_contents(), redirect_url);
  TestNavigationThrottleInstaller installer(
      shell()->web_contents(), NavigationThrottle::PROCEED,
      NavigationThrottle::PROCEED, NavigationThrottle::CANCEL);

  EXPECT_FALSE(NavigateToURL(shell(), redirect_url));

  EXPECT_FALSE(observer.has_committed());
  EXPECT_TRUE(observer.is_error());
  // The navigation should have been redirected first, and then canceled when
  // the response arrived.
  EXPECT_TRUE(observer.was_redirected());
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), start_url);
}

// Ensure that a NavigationThrottle can defer and resume the navigation at
// navigation start, navigation redirect and response received.
IN_PROC_BROWSER_TEST_F(NavigationHandleImplBrowserTest, ThrottleDefer) {
  GURL start_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  GURL redirect_url(
      embedded_test_server()->GetURL("/cross-site/bar.com/title2.html"));
  TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
  NavigationHandleObserver observer(shell()->web_contents(), redirect_url);
  TestNavigationThrottleInstaller installer(
      shell()->web_contents(), NavigationThrottle::DEFER,
      NavigationThrottle::DEFER, NavigationThrottle::DEFER);

  shell()->LoadURL(redirect_url);

  // Wait for WillStartRequest.
  installer.WaitForThrottleWillStart();
  EXPECT_EQ(1, installer.will_start_called());
  EXPECT_EQ(0, installer.will_redirect_called());
  EXPECT_EQ(0, installer.will_process_called());
  installer.navigation_throttle()->Resume();

  // Wait for WillRedirectRequest.
  installer.WaitForThrottleWillRedirect();
  EXPECT_EQ(1, installer.will_start_called());
  EXPECT_EQ(1, installer.will_redirect_called());
  EXPECT_EQ(0, installer.will_process_called());
  installer.navigation_throttle()->Resume();

  // Wait for WillProcessResponse.
  installer.WaitForThrottleWillProcess();
  EXPECT_EQ(1, installer.will_start_called());
  EXPECT_EQ(1, installer.will_redirect_called());
  EXPECT_EQ(1, installer.will_process_called());
  installer.navigation_throttle()->Resume();

  // Wait for the end of the navigation.
  navigation_observer.Wait();

  EXPECT_TRUE(observer.has_committed());
  EXPECT_TRUE(observer.was_redirected());
  EXPECT_FALSE(observer.is_error());
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(),
            GURL(embedded_test_server()->GetURL("bar.com", "/title2.html")));
}

// Checks that the RequestContextType value is properly set.
IN_PROC_BROWSER_TEST_F(NavigationHandleImplBrowserTest,
                       VerifyRequestContextTypeForFrameTree) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c))"));
  GURL b_url(embedded_test_server()->GetURL(
      "b.com", "/cross_site_iframe_factory.html?b(c())"));
  GURL c_url(embedded_test_server()->GetURL(
      "c.com", "/cross_site_iframe_factory.html?c()"));

  TestNavigationThrottleInstaller installer(
      shell()->web_contents(), NavigationThrottle::PROCEED,
      NavigationThrottle::PROCEED, NavigationThrottle::PROCEED);
  TestNavigationManager main_manager(shell()->web_contents(), main_url);
  TestNavigationManager b_manager(shell()->web_contents(), b_url);
  TestNavigationManager c_manager(shell()->web_contents(), c_url);
  NavigationStartUrlRecorder url_recorder(shell()->web_contents());
  TestNavigationThrottle* previous_throttle = nullptr;

  // Starts and verifies the main frame navigation.
  shell()->LoadURL(main_url);
  EXPECT_TRUE(main_manager.WaitForRequestStart());
  // The throttle should not be null.
  EXPECT_NE(previous_throttle, installer.navigation_throttle());
  // Checks the only URL recorded so far is the one expected for the main frame.
  EXPECT_EQ(main_url, url_recorder.urls().back());
  EXPECT_EQ(1ul, url_recorder.urls().size());
  // Checks the main frame RequestContextType.
  EXPECT_EQ(REQUEST_CONTEXT_TYPE_LOCATION,
            installer.navigation_throttle()->request_context_type());
  // For each navigations the throttle should be a different instance.
  previous_throttle = installer.navigation_throttle();

  // Ditto for frame b navigation.
  main_manager.WaitForNavigationFinished();
  EXPECT_TRUE(b_manager.WaitForRequestStart());
  EXPECT_NE(previous_throttle, installer.navigation_throttle());
  EXPECT_EQ(b_url, url_recorder.urls().back());
  EXPECT_EQ(2ul, url_recorder.urls().size());
  EXPECT_EQ(REQUEST_CONTEXT_TYPE_LOCATION,
            installer.navigation_throttle()->request_context_type());
  previous_throttle = installer.navigation_throttle();

  // Ditto for frame c navigation.
  b_manager.WaitForNavigationFinished();
  EXPECT_TRUE(c_manager.WaitForRequestStart());
  EXPECT_NE(previous_throttle, installer.navigation_throttle());
  EXPECT_EQ(c_url, url_recorder.urls().back());
  EXPECT_EQ(3ul, url_recorder.urls().size());
  EXPECT_EQ(REQUEST_CONTEXT_TYPE_LOCATION,
            installer.navigation_throttle()->request_context_type());

  // Lets the final navigation finish so that we conclude running the
  // RequestContextType checks that happen in TestNavigationThrottle.
  c_manager.WaitForNavigationFinished();
  // Confirms the last navigation did finish.
  EXPECT_FALSE(installer.navigation_throttle());
}

// Checks that the RequestContextType value is properly set for an hyper-link.
IN_PROC_BROWSER_TEST_F(NavigationHandleImplBrowserTest,
                       VerifyHyperlinkRequestContextType) {
  GURL link_url(embedded_test_server()->GetURL("/title2.html"));
  GURL document_url(embedded_test_server()->GetURL("/simple_links.html"));

  TestNavigationThrottleInstaller installer(
      shell()->web_contents(), NavigationThrottle::PROCEED,
      NavigationThrottle::PROCEED, NavigationThrottle::PROCEED);
  TestNavigationManager link_manager(shell()->web_contents(), link_url);
  NavigationStartUrlRecorder url_recorder(shell()->web_contents());

  // Navigate to a page with a link.
  EXPECT_TRUE(NavigateToURL(shell(), document_url));
  EXPECT_EQ(document_url, url_recorder.urls().back());
  EXPECT_EQ(1ul, url_recorder.urls().size());

  // Starts the navigation from a link click and then check it.
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(), "window.domAutomationController.send(clickSameSiteLink());",
      &success));
  EXPECT_TRUE(success);
  EXPECT_TRUE(link_manager.WaitForRequestStart());
  EXPECT_EQ(link_url, url_recorder.urls().back());
  EXPECT_EQ(2ul, url_recorder.urls().size());
  EXPECT_EQ(REQUEST_CONTEXT_TYPE_HYPERLINK,
            installer.navigation_throttle()->request_context_type());

  // Finishes the last navigation.
  link_manager.WaitForNavigationFinished();
  EXPECT_FALSE(installer.navigation_throttle());
}

// Checks that the RequestContextType value is properly set for an form (POST).
IN_PROC_BROWSER_TEST_F(NavigationHandleImplBrowserTest,
                       VerifyFormRequestContextType) {
  GURL document_url(
      embedded_test_server()->GetURL("/session_history/form.html"));
  GURL post_url(embedded_test_server()->GetURL("/echotitle"));

  TestNavigationThrottleInstaller installer(
      shell()->web_contents(), NavigationThrottle::PROCEED,
      NavigationThrottle::PROCEED, NavigationThrottle::PROCEED);
  TestNavigationManager post_manager(shell()->web_contents(), post_url);
  NavigationStartUrlRecorder url_recorder(shell()->web_contents());

  // Navigate to a page with a form.
  EXPECT_TRUE(NavigateToURL(shell(), document_url));
  EXPECT_EQ(document_url, url_recorder.urls().back());
  EXPECT_EQ(1ul, url_recorder.urls().size());

  // Executes the form POST navigation and then check it.
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  GURL submit_url("javascript:submitForm('isubmit')");
  shell()->LoadURL(submit_url);
  EXPECT_TRUE(post_manager.WaitForRequestStart());
  EXPECT_EQ(post_url, url_recorder.urls().back());
  EXPECT_EQ(2ul, url_recorder.urls().size());
  EXPECT_EQ(REQUEST_CONTEXT_TYPE_FORM,
            installer.navigation_throttle()->request_context_type());

  // Finishes the last navigation.
  post_manager.WaitForNavigationFinished();
  EXPECT_FALSE(installer.navigation_throttle());
}

// Specialized test that verifies the NavigationHandle gets the HTTPS upgraded
// URL from the very beginning of the navigation.
class NavigationHandleImplHttpsUpgradeBrowserTest
    : public NavigationHandleImplBrowserTest {
 public:
  void CheckHttpsUpgradedIframeNavigation(const GURL& start_url,
                                          const GURL& iframe_secure_url) {
    ASSERT_TRUE(start_url.SchemeIs(url::kHttpScheme));
    ASSERT_TRUE(iframe_secure_url.SchemeIs(url::kHttpsScheme));

    NavigationStartUrlRecorder url_recorder(shell()->web_contents());
    TestNavigationThrottleInstaller installer(
        shell()->web_contents(), NavigationThrottle::PROCEED,
        NavigationThrottle::PROCEED, NavigationThrottle::PROCEED);
    TestNavigationManager navigation_manager(shell()->web_contents(),
                                             iframe_secure_url);

    // Load the page and wait for the frame load with the expected URL.
    // Note: if the test times out while waiting then a navigation to
    // iframe_secure_url never happened and the expected upgrade may not be
    // working.
    shell()->LoadURL(start_url);
    EXPECT_TRUE(navigation_manager.WaitForRequestStart());

    // The main frame should have finished navigating while the iframe should
    // have just started.
    EXPECT_EQ(2, installer.will_start_called());
    EXPECT_EQ(0, installer.will_redirect_called());
    EXPECT_EQ(1, installer.will_process_called());

    // Check the correct start URLs have been registered.
    EXPECT_EQ(iframe_secure_url, url_recorder.urls().back());
    EXPECT_EQ(start_url, url_recorder.urls().front());
    EXPECT_EQ(2ul, url_recorder.urls().size());
  }
};

// Tests that the start URL is HTTPS upgraded for a same site navigation.
IN_PROC_BROWSER_TEST_F(NavigationHandleImplHttpsUpgradeBrowserTest,
                       StartUrlIsHttpsUpgradedSameSite) {
  GURL start_url(
      embedded_test_server()->GetURL("/https_upgrade_same_site.html"));

  // Builds the expected upgraded same site URL.
  GURL::Replacements replace_scheme;
  replace_scheme.SetSchemeStr("https");
  GURL cross_site_iframe_secure_url = embedded_test_server()
                                          ->GetURL("/title1.html")
                                          .ReplaceComponents(replace_scheme);

  CheckHttpsUpgradedIframeNavigation(start_url, cross_site_iframe_secure_url);
}

// Tests that the start URL is HTTPS upgraded for a cross site navigation.
IN_PROC_BROWSER_TEST_F(NavigationHandleImplHttpsUpgradeBrowserTest,
                       StartUrlIsHttpsUpgradedCrossSite) {
  GURL start_url(
      embedded_test_server()->GetURL("/https_upgrade_cross_site.html"));
  GURL cross_site_iframe_secure_url("https://other.com/title1.html");

  CheckHttpsUpgradedIframeNavigation(start_url, cross_site_iframe_secure_url);
}

}  // namespace content
