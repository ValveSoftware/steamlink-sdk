// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view.h"
#include "content/public/browser/load_notification_details.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace content {

void ResizeWebContentsView(Shell* shell, const gfx::Size& size,
                           bool set_start_page) {
  // Shell::SizeTo is not implemented on Aura; WebContentsView::SizeContents
  // works on Win and ChromeOS but not Linux - we need to resize the shell
  // window on Linux because if we don't, the next layout of the unchanged shell
  // window will resize WebContentsView back to the previous size.
  // SizeContents is a hack and should not be relied on.
#if defined(OS_MACOSX)
  shell->SizeTo(size);
  // If |set_start_page| is true, start with blank page to make sure resize
  // takes effect.
  if (set_start_page)
    NavigateToURL(shell, GURL("about://blank"));
#else
  static_cast<WebContentsImpl*>(shell->web_contents())->GetView()->
      SizeContents(size);
#endif  // defined(OS_MACOSX)
}

class WebContentsImplBrowserTest : public ContentBrowserTest {
 public:
  WebContentsImplBrowserTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(WebContentsImplBrowserTest);
};

// Keeps track of data from LoadNotificationDetails so we can later verify that
// they are correct, after the LoadNotificationDetails object is deleted.
class LoadStopNotificationObserver : public WindowedNotificationObserver {
 public:
  LoadStopNotificationObserver(NavigationController* controller)
      : WindowedNotificationObserver(NOTIFICATION_LOAD_STOP,
                                     Source<NavigationController>(controller)),
        session_index_(-1),
        controller_(NULL) {
  }
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE {
    if (type == NOTIFICATION_LOAD_STOP) {
      const Details<LoadNotificationDetails> load_details(details);
      url_ = load_details->url;
      session_index_ = load_details->session_index;
      controller_ = load_details->controller;
    }
    WindowedNotificationObserver::Observe(type, source, details);
  }

  GURL url_;
  int session_index_;
  NavigationController* controller_;
};

// Starts a new navigation as soon as the current one commits, but does not
// wait for it to complete.  This allows us to observe DidStopLoading while
// a pending entry is present.
class NavigateOnCommitObserver : public WebContentsObserver {
 public:
  NavigateOnCommitObserver(Shell* shell, GURL url)
      : WebContentsObserver(shell->web_contents()),
        shell_(shell),
        url_(url),
        done_(false) {
  }

  // WebContentsObserver:
  virtual void NavigationEntryCommitted(
      const LoadCommittedDetails& load_details) OVERRIDE {
    if (!done_) {
      done_ = true;
      shell_->Stop();
      shell_->LoadURL(url_);
    }
  }

  Shell* shell_;
  GURL url_;
  bool done_;
};

class RenderViewSizeDelegate : public WebContentsDelegate {
 public:
  void set_size_insets(const gfx::Size& size_insets) {
    size_insets_ = size_insets;
  }

  // WebContentsDelegate:
  virtual gfx::Size GetSizeForNewRenderView(
      WebContents* web_contents) const OVERRIDE {
    gfx::Size size(web_contents->GetContainerBounds().size());
    size.Enlarge(size_insets_.width(), size_insets_.height());
    return size;
  }

 private:
  gfx::Size size_insets_;
};

class RenderViewSizeObserver : public WebContentsObserver {
 public:
  RenderViewSizeObserver(Shell* shell, const gfx::Size& wcv_new_size)
      : WebContentsObserver(shell->web_contents()),
        shell_(shell),
        wcv_new_size_(wcv_new_size) {
  }

  // WebContentsObserver:
  virtual void RenderViewCreated(RenderViewHost* rvh) OVERRIDE {
    rwhv_create_size_ = rvh->GetView()->GetViewBounds().size();
  }

  virtual void DidStartNavigationToPendingEntry(
      const GURL& url,
      NavigationController::ReloadType reload_type) OVERRIDE {
    ResizeWebContentsView(shell_, wcv_new_size_, false);
  }

  gfx::Size rwhv_create_size() const { return rwhv_create_size_; }

 private:
  Shell* shell_;  // Weak ptr.
  gfx::Size wcv_new_size_;
  gfx::Size rwhv_create_size_;
};

class LoadingStateChangedDelegate : public WebContentsDelegate {
 public:
  LoadingStateChangedDelegate()
      : loadingStateChangedCount_(0)
      , loadingStateToDifferentDocumentCount_(0) {
  }

  // WebContentsDelegate:
  virtual void LoadingStateChanged(WebContents* contents,
                                   bool to_different_document) OVERRIDE {
      loadingStateChangedCount_++;
      if (to_different_document)
        loadingStateToDifferentDocumentCount_++;
  }

  int loadingStateChangedCount() const { return loadingStateChangedCount_; }
  int loadingStateToDifferentDocumentCount() const {
    return loadingStateToDifferentDocumentCount_;
  }

 private:
  int loadingStateChangedCount_;
  int loadingStateToDifferentDocumentCount_;
};

// See: http://crbug.com/298193
#if defined(OS_WIN)
#define MAYBE_DidStopLoadingDetails DISABLED_DidStopLoadingDetails
#else
#define MAYBE_DidStopLoadingDetails DidStopLoadingDetails
#endif

// Test that DidStopLoading includes the correct URL in the details.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       MAYBE_DidStopLoadingDetails) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  LoadStopNotificationObserver load_observer(
      &shell()->web_contents()->GetController());
  NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html"));
  load_observer.Wait();

  EXPECT_EQ("/title1.html", load_observer.url_.path());
  EXPECT_EQ(0, load_observer.session_index_);
  EXPECT_EQ(&shell()->web_contents()->GetController(),
            load_observer.controller_);
}

// See: http://crbug.com/298193
#if defined(OS_WIN)
#define MAYBE_DidStopLoadingDetailsWithPending \
  DISABLED_DidStopLoadingDetailsWithPending
#else
#define MAYBE_DidStopLoadingDetailsWithPending DidStopLoadingDetailsWithPending
#endif

// Test that DidStopLoading includes the correct URL in the details when a
// pending entry is present.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       MAYBE_DidStopLoadingDetailsWithPending) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  GURL url("data:text/html,<div>test</div>");

  // Listen for the first load to stop.
  LoadStopNotificationObserver load_observer(
      &shell()->web_contents()->GetController());
  // Start a new pending navigation as soon as the first load commits.
  // We will hear a DidStopLoading from the first load as the new load
  // is started.
  NavigateOnCommitObserver commit_observer(
      shell(), embedded_test_server()->GetURL("/title2.html"));
  NavigateToURL(shell(), url);
  load_observer.Wait();

  EXPECT_EQ(url, load_observer.url_);
  EXPECT_EQ(0, load_observer.session_index_);
  EXPECT_EQ(&shell()->web_contents()->GetController(),
            load_observer.controller_);
}
// Test that a renderer-initiated navigation to an invalid URL does not leave
// around a pending entry that could be used in a URL spoof.  We test this in
// a browser test because our unit test framework incorrectly calls
// DidStartProvisionalLoadForFrame for in-page navigations.
// See http://crbug.com/280512.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       ClearNonVisiblePendingOnFail) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html"));

  // Navigate to an invalid URL and make sure it doesn't leave a pending entry.
  LoadStopNotificationObserver load_observer1(
      &shell()->web_contents()->GetController());
  ASSERT_TRUE(ExecuteScript(shell()->web_contents(),
                            "window.location.href=\"nonexistent:12121\";"));
  load_observer1.Wait();
  EXPECT_FALSE(shell()->web_contents()->GetController().GetPendingEntry());

  LoadStopNotificationObserver load_observer2(
      &shell()->web_contents()->GetController());
  ASSERT_TRUE(ExecuteScript(shell()->web_contents(),
                            "window.location.href=\"#foo\";"));
  load_observer2.Wait();
  EXPECT_EQ(embedded_test_server()->GetURL("/title1.html#foo"),
            shell()->web_contents()->GetVisibleURL());
}

// TODO(shrikant): enable this for Windows when issue with
// force-compositing-mode is resolved (http://crbug.com/281726).
// Also crashes under ThreadSanitizer, http://crbug.com/356758.
#if defined(OS_WIN) || defined(OS_ANDROID) \
    || defined(THREAD_SANITIZER)
#define MAYBE_GetSizeForNewRenderView DISABLED_GetSizeForNewRenderView
#else
#define MAYBE_GetSizeForNewRenderView GetSizeForNewRenderView
#endif
// Test that RenderViewHost is created and updated at the size specified by
// WebContentsDelegate::GetSizeForNewRenderView().
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       MAYBE_GetSizeForNewRenderView) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  // Create a new server with a different site.
  net::SpawnedTestServer https_server(
      net::SpawnedTestServer::TYPE_HTTPS,
      net::SpawnedTestServer::kLocalhost,
      base::FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(https_server.Start());

  scoped_ptr<RenderViewSizeDelegate> delegate(new RenderViewSizeDelegate());
  shell()->web_contents()->SetDelegate(delegate.get());
  ASSERT_TRUE(shell()->web_contents()->GetDelegate() == delegate.get());

  // When no size is set, RenderWidgetHostView adopts the size of
  // WebContentsView.
  NavigateToURL(shell(), embedded_test_server()->GetURL("/title2.html"));
  EXPECT_EQ(shell()->web_contents()->GetContainerBounds().size(),
            shell()->web_contents()->GetRenderWidgetHostView()->GetViewBounds().
                size());

  // When a size is set, RenderWidgetHostView and WebContentsView honor this
  // size.
  gfx::Size size(300, 300);
  gfx::Size size_insets(10, 15);
  ResizeWebContentsView(shell(), size, true);
  delegate->set_size_insets(size_insets);
  NavigateToURL(shell(), https_server.GetURL("/"));
  size.Enlarge(size_insets.width(), size_insets.height());
  EXPECT_EQ(size,
            shell()->web_contents()->GetRenderWidgetHostView()->GetViewBounds().
                size());
  // The web_contents size is set by the embedder, and should not depend on the
  // rwhv size. The behavior is correct on OSX, but incorrect on other
  // platforms.
  gfx::Size exp_wcv_size(300, 300);
#if !defined(OS_MACOSX)
  exp_wcv_size.Enlarge(size_insets.width(), size_insets.height());
#endif

  EXPECT_EQ(exp_wcv_size,
            shell()->web_contents()->GetContainerBounds().size());

  // If WebContentsView is resized after RenderWidgetHostView is created but
  // before pending navigation entry is committed, both RenderWidgetHostView and
  // WebContentsView use the new size of WebContentsView.
  gfx::Size init_size(200, 200);
  gfx::Size new_size(100, 100);
  size_insets = gfx::Size(20, 30);
  ResizeWebContentsView(shell(), init_size, true);
  delegate->set_size_insets(size_insets);
  RenderViewSizeObserver observer(shell(), new_size);
  NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html"));
  // RenderWidgetHostView is created at specified size.
  init_size.Enlarge(size_insets.width(), size_insets.height());
  EXPECT_EQ(init_size, observer.rwhv_create_size());

// Once again, the behavior is correct on OSX. The embedder explicitly sets
// the size to (100,100) during navigation. Both the wcv and the rwhv should
// take on that size.
#if !defined(OS_MACOSX)
  new_size.Enlarge(size_insets.width(), size_insets.height());
#endif
  gfx::Size actual_size = shell()->web_contents()->GetRenderWidgetHostView()->
      GetViewBounds().size();

  EXPECT_EQ(new_size, actual_size);
  EXPECT_EQ(new_size, shell()->web_contents()->GetContainerBounds().size());
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, OpenURLSubframe) {

  // Navigate with FrameTreeNode ID 4.
  const GURL url("http://foo");
  OpenURLParams params(url, Referrer(), 4, CURRENT_TAB, PAGE_TRANSITION_LINK,
                       true);
  shell()->web_contents()->OpenURL(params);

  // Make sure the NavigationEntry ends up with the FrameTreeNode ID.
  NavigationController* controller = &shell()->web_contents()->GetController();
  EXPECT_TRUE(controller->GetPendingEntry());
  EXPECT_EQ(4, NavigationEntryImpl::FromNavigationEntry(
                controller->GetPendingEntry())->frame_tree_node_id());
}

// Observer class to track the creation of RenderFrameHost objects. It is used
// in subsequent tests.
class RenderFrameCreatedObserver : public WebContentsObserver {
 public:
  RenderFrameCreatedObserver(Shell* shell)
      : WebContentsObserver(shell->web_contents()),
        last_rfh_(NULL) {
  }

  virtual void RenderFrameCreated(RenderFrameHost* render_frame_host) OVERRIDE {
    last_rfh_ = render_frame_host;
  }

  RenderFrameHost* last_rfh() const { return last_rfh_; }

 private:
  RenderFrameHost* last_rfh_;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameCreatedObserver);
};

// Test that creation of new RenderFrameHost objects sends the correct object
// to the WebContentObservers. See http://crbug.com/347339.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       RenderFrameCreatedCorrectProcessForObservers) {
  std::string foo_com("foo.com");
  GURL::Replacements replace_host;
  net::HostPortPair foo_host_port;
  GURL cross_site_url;

  // Setup the server to allow serving separate sites, so we can perform
  // cross-process navigation.
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());

  foo_host_port = test_server()->host_port_pair();
  foo_host_port.set_host(foo_com);

  GURL initial_url(test_server()->GetURL("/title1.html"));

  cross_site_url = test_server()->GetURL("/title2.html");
  replace_host.SetHostStr(foo_com);
  cross_site_url = cross_site_url.ReplaceComponents(replace_host);

  // Navigate to the initial URL and capture the RenderFrameHost for later
  // comparison.
  NavigateToURL(shell(), initial_url);
  RenderFrameHost* orig_rfh = shell()->web_contents()->GetMainFrame();

  // Install the observer and navigate cross-site.
  RenderFrameCreatedObserver observer(shell());
  NavigateToURL(shell(), cross_site_url);

  // The observer should've seen a RenderFrameCreated call for the new frame
  // and not the old one.
  EXPECT_NE(observer.last_rfh(), orig_rfh);
  EXPECT_EQ(observer.last_rfh(), shell()->web_contents()->GetMainFrame());
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       LoadingStateChangedForSameDocumentNavigation) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  scoped_ptr<LoadingStateChangedDelegate> delegate(
      new LoadingStateChangedDelegate());
  shell()->web_contents()->SetDelegate(delegate.get());

  LoadStopNotificationObserver load_observer(
      &shell()->web_contents()->GetController());
  TitleWatcher title_watcher(shell()->web_contents(),
                             base::ASCIIToUTF16("pushState"));
  NavigateToURL(shell(), embedded_test_server()->GetURL("/push_state.html"));
  load_observer.Wait();
  base::string16 title = title_watcher.WaitAndGetTitle();
  ASSERT_EQ(title, base::ASCIIToUTF16("pushState"));

  // LoadingStateChanged should be called 4 times: start and stop for the
  // initial load of push_state.html, and start and stop for the "navigation"
  // triggered by history.pushState(). However, the start notification for the
  // history.pushState() navigation should set to_different_document to false.
  EXPECT_EQ("pushState", shell()->web_contents()->GetURL().ref());
  EXPECT_EQ(4, delegate->loadingStateChangedCount());
  EXPECT_EQ(3, delegate->loadingStateToDifferentDocumentCount());
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       RenderViewCreatedForChildWindow) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  NavigateToURL(shell(),
                embedded_test_server()->GetURL("/title1.html"));

  WebContentsAddedObserver new_web_contents_observer;
  ASSERT_TRUE(ExecuteScript(shell()->web_contents(),
                            "var a = document.createElement('a');"
                            "a.href='./title2.html';"
                            "a.target = '_blank';"
                            "document.body.appendChild(a);"
                            "a.click();"));
  WebContents* new_web_contents = new_web_contents_observer.GetWebContents();
  WaitForLoadStop(new_web_contents);
  EXPECT_TRUE(new_web_contents_observer.RenderViewCreatedCalled());
}

struct LoadProgressDelegateAndObserver : public WebContentsDelegate,
                                         public WebContentsObserver {
  LoadProgressDelegateAndObserver(Shell* shell)
      : WebContentsObserver(shell->web_contents()),
        did_start_loading(false),
        did_stop_loading(false) {
    web_contents()->SetDelegate(this);
  }

  // WebContentsDelegate:
  virtual void LoadProgressChanged(WebContents* source,
                                   double progress) OVERRIDE {
    EXPECT_TRUE(did_start_loading);
    EXPECT_FALSE(did_stop_loading);
    progresses.push_back(progress);
  }

  // WebContentsObserver:
  virtual void DidStartLoading(RenderViewHost* render_view_host) OVERRIDE {
    EXPECT_FALSE(did_start_loading);
    EXPECT_EQ(0U, progresses.size());
    EXPECT_FALSE(did_stop_loading);
    did_start_loading = true;
  }

  virtual void DidStopLoading(RenderViewHost* render_view_host) OVERRIDE {
    EXPECT_TRUE(did_start_loading);
    EXPECT_GE(progresses.size(), 1U);
    EXPECT_FALSE(did_stop_loading);
    did_stop_loading = true;
  }

  bool did_start_loading;
  std::vector<double> progresses;
  bool did_stop_loading;
};

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, LoadProgress) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  scoped_ptr<LoadProgressDelegateAndObserver> delegate(
      new LoadProgressDelegateAndObserver(shell()));

  NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html"));

  const std::vector<double>& progresses = delegate->progresses;
  // All updates should be in order ...
  if (std::adjacent_find(progresses.begin(),
                         progresses.end(),
                         std::greater<double>()) != progresses.end()) {
    ADD_FAILURE() << "Progress values should be in order: "
                  << ::testing::PrintToString(progresses);
  }

  // ... and the last one should be 1.0, meaning complete.
  ASSERT_GE(progresses.size(), 1U)
      << "There should be at least one progress update";
  EXPECT_EQ(1.0, *progresses.rbegin());
}

}  // namespace content

