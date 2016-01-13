// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/frame_host/cross_site_transferring_request.h"
#include "content/browser/frame_host/interstitial_page_impl.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/webui/web_ui_controller_factory_registry.h"
#include "content/common/frame_messages.h"
#include "content/common/input/synthetic_web_input_event_builders.h"
#include "content/common/view_messages.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/interstitial_page_delegate.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_utils.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_content_client.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

const char kTestWebUIUrl[] = "chrome://blah";

class WebContentsImplTestWebUIControllerFactory
    : public WebUIControllerFactory {
 public:
  virtual WebUIController* CreateWebUIControllerForURL(
      WebUI* web_ui, const GURL& url) const OVERRIDE {
    if (!UseWebUI(url))
      return NULL;
    return new WebUIController(web_ui);
  }

  virtual WebUI::TypeID GetWebUIType(BrowserContext* browser_context,
      const GURL& url) const OVERRIDE {
    return WebUI::kNoWebUI;
  }

  virtual bool UseWebUIForURL(BrowserContext* browser_context,
                              const GURL& url) const OVERRIDE {
    return UseWebUI(url);
  }

  virtual bool UseWebUIBindingsForURL(BrowserContext* browser_context,
                                      const GURL& url) const OVERRIDE {
    return UseWebUI(url);
  }

 private:
  bool UseWebUI(const GURL& url) const {
    return url == GURL(kTestWebUIUrl);
  }
};

class TestInterstitialPage;

class TestInterstitialPageDelegate : public InterstitialPageDelegate {
 public:
  explicit TestInterstitialPageDelegate(TestInterstitialPage* interstitial_page)
      : interstitial_page_(interstitial_page) {}
  virtual void CommandReceived(const std::string& command) OVERRIDE;
  virtual std::string GetHTMLContents() OVERRIDE { return std::string(); }
  virtual void OnDontProceed() OVERRIDE;
  virtual void OnProceed() OVERRIDE;
 private:
  TestInterstitialPage* interstitial_page_;
};

class TestInterstitialPage : public InterstitialPageImpl {
 public:
  enum InterstitialState {
    INVALID = 0,    // Hasn't yet been initialized.
    UNDECIDED,      // Initialized, but no decision taken yet.
    OKED,           // Proceed was called.
    CANCELED        // DontProceed was called.
  };

  class Delegate {
   public:
    virtual void TestInterstitialPageDeleted(
        TestInterstitialPage* interstitial) = 0;

   protected:
    virtual ~Delegate() {}
  };

  // IMPORTANT NOTE: if you pass stack allocated values for |state| and
  // |deleted| (like all interstitial related tests do at this point), make sure
  // to create an instance of the TestInterstitialPageStateGuard class on the
  // stack in your test.  This will ensure that the TestInterstitialPage states
  // are cleared when the test finishes.
  // Not doing so will cause stack trashing if your test does not hide the
  // interstitial, as in such a case it will be destroyed in the test TearDown
  // method and will dereference the |deleted| local variable which by then is
  // out of scope.
  TestInterstitialPage(WebContentsImpl* contents,
                       bool new_navigation,
                       const GURL& url,
                       InterstitialState* state,
                       bool* deleted)
      : InterstitialPageImpl(
            contents,
            static_cast<RenderWidgetHostDelegate*>(contents),
            new_navigation, url, new TestInterstitialPageDelegate(this)),
        state_(state),
        deleted_(deleted),
        command_received_count_(0),
        delegate_(NULL) {
    *state_ = UNDECIDED;
    *deleted_ = false;
  }

  virtual ~TestInterstitialPage() {
    if (deleted_)
      *deleted_ = true;
    if (delegate_)
      delegate_->TestInterstitialPageDeleted(this);
  }

  void OnDontProceed() {
    if (state_)
      *state_ = CANCELED;
  }
  void OnProceed() {
    if (state_)
      *state_ = OKED;
  }

  int command_received_count() const {
    return command_received_count_;
  }

  void TestDomOperationResponse(const std::string& json_string) {
    if (enabled())
      CommandReceived();
  }

  void TestDidNavigate(int page_id, const GURL& url) {
    FrameHostMsg_DidCommitProvisionalLoad_Params params;
    InitNavigateParams(&params, page_id, url, PAGE_TRANSITION_TYPED);
    DidNavigate(GetRenderViewHostForTesting(), params);
  }

  void TestRenderViewTerminated(base::TerminationStatus status,
                                int error_code) {
    RenderViewTerminated(GetRenderViewHostForTesting(), status, error_code);
  }

  bool is_showing() const {
    return static_cast<TestRenderWidgetHostView*>(
        GetRenderViewHostForTesting()->GetView())->is_showing();
  }

  void ClearStates() {
    state_ = NULL;
    deleted_ = NULL;
    delegate_ = NULL;
  }

  void CommandReceived() {
    command_received_count_++;
  }

  void set_delegate(Delegate* delegate) {
    delegate_ = delegate;
  }

 protected:
  virtual WebContentsView* CreateWebContentsView() OVERRIDE {
    return NULL;
  }

 private:
  InterstitialState* state_;
  bool* deleted_;
  int command_received_count_;
  Delegate* delegate_;
};

void TestInterstitialPageDelegate::CommandReceived(const std::string& command) {
  interstitial_page_->CommandReceived();
}

void TestInterstitialPageDelegate::OnDontProceed() {
  interstitial_page_->OnDontProceed();
}

void TestInterstitialPageDelegate::OnProceed() {
  interstitial_page_->OnProceed();
}

class TestInterstitialPageStateGuard : public TestInterstitialPage::Delegate {
 public:
  explicit TestInterstitialPageStateGuard(
      TestInterstitialPage* interstitial_page)
      : interstitial_page_(interstitial_page) {
    DCHECK(interstitial_page_);
    interstitial_page_->set_delegate(this);
  }
  virtual ~TestInterstitialPageStateGuard() {
    if (interstitial_page_)
      interstitial_page_->ClearStates();
  }

  virtual void TestInterstitialPageDeleted(
      TestInterstitialPage* interstitial) OVERRIDE {
    DCHECK(interstitial_page_ == interstitial);
    interstitial_page_ = NULL;
  }

 private:
  TestInterstitialPage* interstitial_page_;
};

class WebContentsImplTestBrowserClient : public TestContentBrowserClient {
 public:
  WebContentsImplTestBrowserClient()
      : assign_site_for_url_(false) {}

  virtual ~WebContentsImplTestBrowserClient() {}

  virtual bool ShouldAssignSiteForURL(const GURL& url) OVERRIDE {
    return assign_site_for_url_;
  }

  void set_assign_site_for_url(bool assign) {
    assign_site_for_url_ = assign;
  }

 private:
  bool assign_site_for_url_;
};

class WebContentsImplTest : public RenderViewHostImplTestHarness {
 public:
  virtual void SetUp() {
    RenderViewHostImplTestHarness::SetUp();
    WebUIControllerFactory::RegisterFactory(&factory_);
  }

  virtual void TearDown() {
    WebUIControllerFactory::UnregisterFactoryForTesting(&factory_);
    RenderViewHostImplTestHarness::TearDown();
  }

 private:
  WebContentsImplTestWebUIControllerFactory factory_;
};

class TestWebContentsObserver : public WebContentsObserver {
 public:
  explicit TestWebContentsObserver(WebContents* contents)
      : WebContentsObserver(contents) {
  }
  virtual ~TestWebContentsObserver() {}

  virtual void DidFinishLoad(int64 frame_id,
                             const GURL& validated_url,
                             bool is_main_frame,
                             RenderViewHost* render_view_host) OVERRIDE {
    last_url_ = validated_url;
  }
  virtual void DidFailLoad(int64 frame_id,
                           const GURL& validated_url,
                           bool is_main_frame,
                           int error_code,
                           const base::string16& error_description,
                           RenderViewHost* render_view_host) OVERRIDE {
    last_url_ = validated_url;
  }

  const GURL& last_url() const { return last_url_; }

 private:
  GURL last_url_;

  DISALLOW_COPY_AND_ASSIGN(TestWebContentsObserver);
};

// Pretends to be a normal browser that receives toggles and transitions to/from
// a fullscreened state.
class FakeFullscreenDelegate : public WebContentsDelegate {
 public:
  FakeFullscreenDelegate() : fullscreened_contents_(NULL) {}
  virtual ~FakeFullscreenDelegate() {}

  virtual void ToggleFullscreenModeForTab(WebContents* web_contents,
                                          bool enter_fullscreen) OVERRIDE {
    fullscreened_contents_ = enter_fullscreen ? web_contents : NULL;
  }

  virtual bool IsFullscreenForTabOrPending(const WebContents* web_contents)
      const OVERRIDE {
    return fullscreened_contents_ && web_contents == fullscreened_contents_;
  }

 private:
  WebContents* fullscreened_contents_;

  DISALLOW_COPY_AND_ASSIGN(FakeFullscreenDelegate);
};

class FakeValidationMessageDelegate : public WebContentsDelegate {
 public:
  FakeValidationMessageDelegate()
      : hide_validation_message_was_called_(false) {}
  virtual ~FakeValidationMessageDelegate() {}

  virtual void HideValidationMessage(WebContents* web_contents) OVERRIDE {
    hide_validation_message_was_called_ = true;
  }

  bool hide_validation_message_was_called() const {
    return hide_validation_message_was_called_;
  }

 private:
  bool hide_validation_message_was_called_;

  DISALLOW_COPY_AND_ASSIGN(FakeValidationMessageDelegate);
};

}  // namespace

// Test to make sure that title updates get stripped of whitespace.
TEST_F(WebContentsImplTest, UpdateTitle) {
  NavigationControllerImpl& cont =
      static_cast<NavigationControllerImpl&>(controller());
  FrameHostMsg_DidCommitProvisionalLoad_Params params;
  InitNavigateParams(
      &params, 0, GURL(url::kAboutBlankURL), PAGE_TRANSITION_TYPED);

  LoadCommittedDetails details;
  cont.RendererDidNavigate(main_test_rfh(), params, &details);

  contents()->UpdateTitle(main_test_rfh(), 0,
                          base::ASCIIToUTF16("    Lots O' Whitespace\n"),
                          base::i18n::LEFT_TO_RIGHT);
  EXPECT_EQ(base::ASCIIToUTF16("Lots O' Whitespace"), contents()->GetTitle());
}

TEST_F(WebContentsImplTest, DontUseTitleFromPendingEntry) {
  const GURL kGURL("chrome://blah");
  controller().LoadURL(
      kGURL, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  EXPECT_EQ(base::string16(), contents()->GetTitle());
}

TEST_F(WebContentsImplTest, UseTitleFromPendingEntryIfSet) {
  const GURL kGURL("chrome://blah");
  const base::string16 title = base::ASCIIToUTF16("My Title");
  controller().LoadURL(
      kGURL, Referrer(), PAGE_TRANSITION_TYPED, std::string());

  NavigationEntry* entry = controller().GetVisibleEntry();
  ASSERT_EQ(kGURL, entry->GetURL());
  entry->SetTitle(title);

  EXPECT_EQ(title, contents()->GetTitle());
}

// Test view source mode for a webui page.
TEST_F(WebContentsImplTest, NTPViewSource) {
  NavigationControllerImpl& cont =
      static_cast<NavigationControllerImpl&>(controller());
  const char kUrl[] = "view-source:chrome://blah";
  const GURL kGURL(kUrl);

  process()->sink().ClearMessages();

  cont.LoadURL(
      kGURL, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  rvh()->GetDelegate()->RenderViewCreated(rvh());
  // Did we get the expected message?
  EXPECT_TRUE(process()->sink().GetFirstMessageMatching(
      ViewMsg_EnableViewSourceMode::ID));

  FrameHostMsg_DidCommitProvisionalLoad_Params params;
  InitNavigateParams(&params, 0, kGURL, PAGE_TRANSITION_TYPED);
  LoadCommittedDetails details;
  cont.RendererDidNavigate(main_test_rfh(), params, &details);
  // Also check title and url.
  EXPECT_EQ(base::ASCIIToUTF16(kUrl), contents()->GetTitle());
}

// Test to ensure UpdateMaxPageID is working properly.
TEST_F(WebContentsImplTest, UpdateMaxPageID) {
  SiteInstance* instance1 = contents()->GetSiteInstance();
  scoped_refptr<SiteInstance> instance2(SiteInstance::Create(NULL));

  // Starts at -1.
  EXPECT_EQ(-1, contents()->GetMaxPageID());
  EXPECT_EQ(-1, contents()->GetMaxPageIDForSiteInstance(instance1));
  EXPECT_EQ(-1, contents()->GetMaxPageIDForSiteInstance(instance2.get()));

  // Make sure max_page_id_ is monotonically increasing per SiteInstance.
  contents()->UpdateMaxPageID(3);
  contents()->UpdateMaxPageID(1);
  EXPECT_EQ(3, contents()->GetMaxPageID());
  EXPECT_EQ(3, contents()->GetMaxPageIDForSiteInstance(instance1));
  EXPECT_EQ(-1, contents()->GetMaxPageIDForSiteInstance(instance2.get()));

  contents()->UpdateMaxPageIDForSiteInstance(instance2.get(), 7);
  EXPECT_EQ(3, contents()->GetMaxPageID());
  EXPECT_EQ(3, contents()->GetMaxPageIDForSiteInstance(instance1));
  EXPECT_EQ(7, contents()->GetMaxPageIDForSiteInstance(instance2.get()));
}

// Test simple same-SiteInstance navigation.
TEST_F(WebContentsImplTest, SimpleNavigation) {
  TestRenderViewHost* orig_rvh = test_rvh();
  SiteInstance* instance1 = contents()->GetSiteInstance();
  EXPECT_TRUE(contents()->GetPendingRenderViewHost() == NULL);

  // Navigate to URL
  const GURL url("http://www.google.com");
  controller().LoadURL(
      url, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_EQ(instance1, orig_rvh->GetSiteInstance());
  // Controller's pending entry will have a NULL site instance until we assign
  // it in DidNavigate.
  EXPECT_TRUE(
      NavigationEntryImpl::FromNavigationEntry(controller().GetVisibleEntry())->
          site_instance() == NULL);

  // DidNavigate from the page
  contents()->TestDidNavigate(orig_rvh, 1, url, PAGE_TRANSITION_TYPED);
  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_EQ(orig_rvh, contents()->GetRenderViewHost());
  EXPECT_EQ(instance1, orig_rvh->GetSiteInstance());
  // Controller's entry should now have the SiteInstance, or else we won't be
  // able to find it later.
  EXPECT_EQ(
      instance1,
      NavigationEntryImpl::FromNavigationEntry(controller().GetVisibleEntry())->
          site_instance());
}

// Test that we reject NavigateToEntry if the url is over kMaxURLChars.
TEST_F(WebContentsImplTest, NavigateToExcessivelyLongURL) {
  // Construct a URL that's kMaxURLChars + 1 long of all 'a's.
  const GURL url(std::string("http://example.org/").append(
      GetMaxURLChars() + 1, 'a'));

  controller().LoadURL(
      url, Referrer(), PAGE_TRANSITION_GENERATED, std::string());
  EXPECT_TRUE(controller().GetVisibleEntry() == NULL);
}

// Test that navigating across a site boundary creates a new RenderViewHost
// with a new SiteInstance.  Going back should do the same.
TEST_F(WebContentsImplTest, CrossSiteBoundaries) {
  TestRenderViewHost* orig_rvh = test_rvh();
  RenderFrameHostImpl* orig_rfh =
      contents()->GetFrameTree()->root()->current_frame_host();
  int orig_rvh_delete_count = 0;
  orig_rvh->set_delete_counter(&orig_rvh_delete_count);
  SiteInstance* instance1 = contents()->GetSiteInstance();

  // Navigate to URL.  First URL should use first RenderViewHost.
  const GURL url("http://www.google.com");
  controller().LoadURL(
      url, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  contents()->TestDidNavigate(orig_rvh, 1, url, PAGE_TRANSITION_TYPED);

  // Keep the number of active views in orig_rvh's SiteInstance
  // non-zero so that orig_rvh doesn't get deleted when it gets
  // swapped out.
  static_cast<SiteInstanceImpl*>(orig_rvh->GetSiteInstance())->
      increment_active_view_count();

  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_EQ(orig_rvh, contents()->GetRenderViewHost());
  EXPECT_EQ(url, contents()->GetLastCommittedURL());
  EXPECT_EQ(url, contents()->GetVisibleURL());

  // Navigate to new site
  const GURL url2("http://www.yahoo.com");
  controller().LoadURL(
      url2, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  EXPECT_TRUE(contents()->cross_navigation_pending());
  EXPECT_EQ(url, contents()->GetLastCommittedURL());
  EXPECT_EQ(url2, contents()->GetVisibleURL());
  TestRenderViewHost* pending_rvh =
      static_cast<TestRenderViewHost*>(contents()->GetPendingRenderViewHost());
  int pending_rvh_delete_count = 0;
  pending_rvh->set_delete_counter(&pending_rvh_delete_count);
  RenderFrameHostImpl* pending_rfh = contents()->GetFrameTree()->root()->
      render_manager()->pending_frame_host();

  // Navigations should be suspended in pending_rvh until BeforeUnloadACK.
  EXPECT_TRUE(pending_rvh->are_navigations_suspended());
  orig_rvh->SendBeforeUnloadACK(true);
  EXPECT_FALSE(pending_rvh->are_navigations_suspended());

  // DidNavigate from the pending page
  contents()->TestDidNavigate(
      pending_rvh, 1, url2, PAGE_TRANSITION_TYPED);
  SiteInstance* instance2 = contents()->GetSiteInstance();

  // Keep the number of active views in pending_rvh's SiteInstance
  // non-zero so that orig_rvh doesn't get deleted when it gets
  // swapped out.
  static_cast<SiteInstanceImpl*>(pending_rvh->GetSiteInstance())->
      increment_active_view_count();

  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_EQ(pending_rvh, contents()->GetRenderViewHost());
  EXPECT_EQ(url2, contents()->GetLastCommittedURL());
  EXPECT_EQ(url2, contents()->GetVisibleURL());
  EXPECT_NE(instance1, instance2);
  EXPECT_TRUE(contents()->GetPendingRenderViewHost() == NULL);
  // We keep the original RFH around, swapped out.
  EXPECT_TRUE(contents()->GetRenderManagerForTesting()->IsOnSwappedOutList(
      orig_rfh));
  EXPECT_EQ(orig_rvh_delete_count, 0);

  // Going back should switch SiteInstances again.  The first SiteInstance is
  // stored in the NavigationEntry, so it should be the same as at the start.
  // We should use the same RVH as before, swapping it back in.
  controller().GoBack();
  TestRenderViewHost* goback_rvh =
      static_cast<TestRenderViewHost*>(contents()->GetPendingRenderViewHost());
  EXPECT_EQ(orig_rvh, goback_rvh);
  EXPECT_TRUE(contents()->cross_navigation_pending());

  // Navigations should be suspended in goback_rvh until BeforeUnloadACK.
  EXPECT_TRUE(goback_rvh->are_navigations_suspended());
  pending_rvh->SendBeforeUnloadACK(true);
  EXPECT_FALSE(goback_rvh->are_navigations_suspended());

  // DidNavigate from the back action
  contents()->TestDidNavigate(
      goback_rvh, 1, url2, PAGE_TRANSITION_TYPED);
  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_EQ(goback_rvh, contents()->GetRenderViewHost());
  EXPECT_EQ(instance1, contents()->GetSiteInstance());
  // The pending RFH should now be swapped out, not deleted.
  EXPECT_TRUE(contents()->GetRenderManagerForTesting()->
      IsOnSwappedOutList(pending_rfh));
  EXPECT_EQ(pending_rvh_delete_count, 0);
  pending_rvh->OnSwappedOut(false);

  // Close contents and ensure RVHs are deleted.
  DeleteContents();
  EXPECT_EQ(orig_rvh_delete_count, 1);
  EXPECT_EQ(pending_rvh_delete_count, 1);
}

// Test that navigating across a site boundary after a crash creates a new
// RVH without requiring a cross-site transition (i.e., PENDING state).
TEST_F(WebContentsImplTest, CrossSiteBoundariesAfterCrash) {
  TestRenderViewHost* orig_rvh = test_rvh();
  int orig_rvh_delete_count = 0;
  orig_rvh->set_delete_counter(&orig_rvh_delete_count);
  SiteInstance* instance1 = contents()->GetSiteInstance();

  // Navigate to URL.  First URL should use first RenderViewHost.
  const GURL url("http://www.google.com");
  controller().LoadURL(
      url, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  contents()->TestDidNavigate(orig_rvh, 1, url, PAGE_TRANSITION_TYPED);

  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_EQ(orig_rvh, contents()->GetRenderViewHost());

  // Crash the renderer.
  orig_rvh->set_render_view_created(false);

  // Navigate to new site.  We should not go into PENDING.
  const GURL url2("http://www.yahoo.com");
  controller().LoadURL(
      url2, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  RenderViewHost* new_rvh = rvh();
  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_TRUE(contents()->GetPendingRenderViewHost() == NULL);
  EXPECT_NE(orig_rvh, new_rvh);
  EXPECT_EQ(orig_rvh_delete_count, 1);

  // DidNavigate from the new page
  contents()->TestDidNavigate(new_rvh, 1, url2, PAGE_TRANSITION_TYPED);
  SiteInstance* instance2 = contents()->GetSiteInstance();

  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_EQ(new_rvh, rvh());
  EXPECT_NE(instance1, instance2);
  EXPECT_TRUE(contents()->GetPendingRenderViewHost() == NULL);

  // Close contents and ensure RVHs are deleted.
  DeleteContents();
  EXPECT_EQ(orig_rvh_delete_count, 1);
}

// Test that opening a new contents in the same SiteInstance and then navigating
// both contentses to a new site will place both contentses in a single
// SiteInstance.
TEST_F(WebContentsImplTest, NavigateTwoTabsCrossSite) {
  TestRenderViewHost* orig_rvh = test_rvh();
  SiteInstance* instance1 = contents()->GetSiteInstance();

  // Navigate to URL.  First URL should use first RenderViewHost.
  const GURL url("http://www.google.com");
  controller().LoadURL(
      url, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  contents()->TestDidNavigate(orig_rvh, 1, url, PAGE_TRANSITION_TYPED);

  // Open a new contents with the same SiteInstance, navigated to the same site.
  scoped_ptr<TestWebContents> contents2(
      TestWebContents::Create(browser_context(), instance1));
  contents2->GetController().LoadURL(url, Referrer(),
                                    PAGE_TRANSITION_TYPED,
                                    std::string());
  // Need this page id to be 2 since the site instance is the same (which is the
  // scope of page IDs) and we want to consider this a new page.
  contents2->TestDidNavigate(
      contents2->GetRenderViewHost(), 2, url, PAGE_TRANSITION_TYPED);

  // Navigate first contents to a new site.
  const GURL url2a("http://www.yahoo.com");
  controller().LoadURL(
      url2a, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  orig_rvh->SendBeforeUnloadACK(true);
  TestRenderViewHost* pending_rvh_a =
      static_cast<TestRenderViewHost*>(contents()->GetPendingRenderViewHost());
  contents()->TestDidNavigate(
      pending_rvh_a, 1, url2a, PAGE_TRANSITION_TYPED);
  SiteInstance* instance2a = contents()->GetSiteInstance();
  EXPECT_NE(instance1, instance2a);

  // Navigate second contents to the same site as the first tab.
  const GURL url2b("http://mail.yahoo.com");
  contents2->GetController().LoadURL(url2b, Referrer(),
                                    PAGE_TRANSITION_TYPED,
                                    std::string());
  TestRenderViewHost* rvh2 =
      static_cast<TestRenderViewHost*>(contents2->GetRenderViewHost());
  rvh2->SendBeforeUnloadACK(true);
  TestRenderViewHost* pending_rvh_b =
      static_cast<TestRenderViewHost*>(contents2->GetPendingRenderViewHost());
  EXPECT_TRUE(pending_rvh_b != NULL);
  EXPECT_TRUE(contents2->cross_navigation_pending());

  // NOTE(creis): We used to be in danger of showing a crash page here if the
  // second contents hadn't navigated somewhere first (bug 1145430).  That case
  // is now covered by the CrossSiteBoundariesAfterCrash test.
  contents2->TestDidNavigate(
      pending_rvh_b, 2, url2b, PAGE_TRANSITION_TYPED);
  SiteInstance* instance2b = contents2->GetSiteInstance();
  EXPECT_NE(instance1, instance2b);

  // Both contentses should now be in the same SiteInstance.
  EXPECT_EQ(instance2a, instance2b);
}

TEST_F(WebContentsImplTest, NavigateDoesNotUseUpSiteInstance) {
  WebContentsImplTestBrowserClient browser_client;
  SetBrowserClientForTesting(&browser_client);

  TestRenderViewHost* orig_rvh = test_rvh();
  RenderFrameHostImpl* orig_rfh =
      contents()->GetFrameTree()->root()->current_frame_host();
  int orig_rvh_delete_count = 0;
  orig_rvh->set_delete_counter(&orig_rvh_delete_count);
  SiteInstanceImpl* orig_instance =
      static_cast<SiteInstanceImpl*>(contents()->GetSiteInstance());

  browser_client.set_assign_site_for_url(false);
  // Navigate to an URL that will not assign a new SiteInstance.
  const GURL native_url("non-site-url://stuffandthings");
  controller().LoadURL(
      native_url, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  contents()->TestDidNavigate(orig_rvh, 1, native_url, PAGE_TRANSITION_TYPED);

  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_EQ(orig_rvh, contents()->GetRenderViewHost());
  EXPECT_EQ(native_url, contents()->GetLastCommittedURL());
  EXPECT_EQ(native_url, contents()->GetVisibleURL());
  EXPECT_EQ(orig_instance, contents()->GetSiteInstance());
  EXPECT_EQ(GURL(), contents()->GetSiteInstance()->GetSiteURL());
  EXPECT_FALSE(orig_instance->HasSite());

  browser_client.set_assign_site_for_url(true);
  // Navigate to new site (should keep same site instance).
  const GURL url("http://www.google.com");
  controller().LoadURL(
      url, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_EQ(native_url, contents()->GetLastCommittedURL());
  EXPECT_EQ(url, contents()->GetVisibleURL());
  EXPECT_FALSE(contents()->GetPendingRenderViewHost());
  contents()->TestDidNavigate(orig_rvh, 1, url, PAGE_TRANSITION_TYPED);

  // Keep the number of active views in orig_rvh's SiteInstance
  // non-zero so that orig_rvh doesn't get deleted when it gets
  // swapped out.
  static_cast<SiteInstanceImpl*>(orig_rvh->GetSiteInstance())->
      increment_active_view_count();

  EXPECT_EQ(orig_instance, contents()->GetSiteInstance());
  EXPECT_TRUE(
      contents()->GetSiteInstance()->GetSiteURL().DomainIs("google.com"));
  EXPECT_EQ(url, contents()->GetLastCommittedURL());

  // Navigate to another new site (should create a new site instance).
  const GURL url2("http://www.yahoo.com");
  controller().LoadURL(
      url2, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  EXPECT_TRUE(contents()->cross_navigation_pending());
  EXPECT_EQ(url, contents()->GetLastCommittedURL());
  EXPECT_EQ(url2, contents()->GetVisibleURL());
  TestRenderViewHost* pending_rvh =
      static_cast<TestRenderViewHost*>(contents()->GetPendingRenderViewHost());
  int pending_rvh_delete_count = 0;
  pending_rvh->set_delete_counter(&pending_rvh_delete_count);

  // Navigations should be suspended in pending_rvh until BeforeUnloadACK.
  EXPECT_TRUE(pending_rvh->are_navigations_suspended());
  orig_rvh->SendBeforeUnloadACK(true);
  EXPECT_FALSE(pending_rvh->are_navigations_suspended());

  // DidNavigate from the pending page.
  contents()->TestDidNavigate(
      pending_rvh, 1, url2, PAGE_TRANSITION_TYPED);
  SiteInstance* new_instance = contents()->GetSiteInstance();

  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_EQ(pending_rvh, contents()->GetRenderViewHost());
  EXPECT_EQ(url2, contents()->GetLastCommittedURL());
  EXPECT_EQ(url2, contents()->GetVisibleURL());
  EXPECT_NE(new_instance, orig_instance);
  EXPECT_FALSE(contents()->GetPendingRenderViewHost());
  // We keep the original RFH around, swapped out.
  EXPECT_TRUE(contents()->GetRenderManagerForTesting()->IsOnSwappedOutList(
      orig_rfh));
  EXPECT_EQ(orig_rvh_delete_count, 0);
  orig_rvh->OnSwappedOut(false);

  // Close contents and ensure RVHs are deleted.
  DeleteContents();
  EXPECT_EQ(orig_rvh_delete_count, 1);
  EXPECT_EQ(pending_rvh_delete_count, 1);
}

// Test that we can find an opener RVH even if it's pending.
// http://crbug.com/176252.
TEST_F(WebContentsImplTest, FindOpenerRVHWhenPending) {
  TestRenderViewHost* orig_rvh = test_rvh();

  // Navigate to a URL.
  const GURL url("http://www.google.com");
  controller().LoadURL(
      url, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  contents()->TestDidNavigate(orig_rvh, 1, url, PAGE_TRANSITION_TYPED);

  // Start to navigate first tab to a new site, so that it has a pending RVH.
  const GURL url2("http://www.yahoo.com");
  controller().LoadURL(
      url2, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  orig_rvh->SendBeforeUnloadACK(true);
  TestRenderViewHost* pending_rvh =
      static_cast<TestRenderViewHost*>(contents()->GetPendingRenderViewHost());

  // While it is still pending, simulate opening a new tab with the first tab
  // as its opener.  This will call WebContentsImpl::CreateOpenerRenderViews
  // on the opener to ensure that an RVH exists.
  int opener_routing_id = contents()->CreateOpenerRenderViews(
      pending_rvh->GetSiteInstance());

  // We should find the pending RVH and not create a new one.
  EXPECT_EQ(pending_rvh->GetRoutingID(), opener_routing_id);
}

// Tests that WebContentsImpl uses the current URL, not the SiteInstance's site,
// to determine whether a navigation is cross-site.
TEST_F(WebContentsImplTest, CrossSiteComparesAgainstCurrentPage) {
  RenderViewHost* orig_rvh = rvh();
  SiteInstance* instance1 = contents()->GetSiteInstance();

  // Navigate to URL.
  const GURL url("http://www.google.com");
  controller().LoadURL(
      url, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  contents()->TestDidNavigate(
      orig_rvh, 1, url, PAGE_TRANSITION_TYPED);

  // Open a related contents to a second site.
  scoped_ptr<TestWebContents> contents2(
      TestWebContents::Create(browser_context(), instance1));
  const GURL url2("http://www.yahoo.com");
  contents2->GetController().LoadURL(url2, Referrer(),
                                    PAGE_TRANSITION_TYPED,
                                    std::string());
  // The first RVH in contents2 isn't live yet, so we shortcut the cross site
  // pending.
  TestRenderViewHost* rvh2 = static_cast<TestRenderViewHost*>(
      contents2->GetRenderViewHost());
  EXPECT_FALSE(contents2->cross_navigation_pending());
  contents2->TestDidNavigate(rvh2, 2, url2, PAGE_TRANSITION_TYPED);
  SiteInstance* instance2 = contents2->GetSiteInstance();
  EXPECT_NE(instance1, instance2);
  EXPECT_FALSE(contents2->cross_navigation_pending());

  // Simulate a link click in first contents to second site.  Doesn't switch
  // SiteInstances, because we don't intercept WebKit navigations.
  contents()->TestDidNavigate(
      orig_rvh, 2, url2, PAGE_TRANSITION_TYPED);
  SiteInstance* instance3 = contents()->GetSiteInstance();
  EXPECT_EQ(instance1, instance3);
  EXPECT_FALSE(contents()->cross_navigation_pending());

  // Navigate to the new site.  Doesn't switch SiteInstancees, because we
  // compare against the current URL, not the SiteInstance's site.
  const GURL url3("http://mail.yahoo.com");
  controller().LoadURL(
      url3, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  EXPECT_FALSE(contents()->cross_navigation_pending());
  contents()->TestDidNavigate(
      orig_rvh, 3, url3, PAGE_TRANSITION_TYPED);
  SiteInstance* instance4 = contents()->GetSiteInstance();
  EXPECT_EQ(instance1, instance4);
}

// Test that the onbeforeunload and onunload handlers run when navigating
// across site boundaries.
TEST_F(WebContentsImplTest, CrossSiteUnloadHandlers) {
  TestRenderViewHost* orig_rvh = test_rvh();
  SiteInstance* instance1 = contents()->GetSiteInstance();

  // Navigate to URL.  First URL should use first RenderViewHost.
  const GURL url("http://www.google.com");
  controller().LoadURL(
      url, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  contents()->TestDidNavigate(orig_rvh, 1, url, PAGE_TRANSITION_TYPED);
  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_EQ(orig_rvh, contents()->GetRenderViewHost());

  // Navigate to new site, but simulate an onbeforeunload denial.
  const GURL url2("http://www.yahoo.com");
  controller().LoadURL(
      url2, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  EXPECT_TRUE(orig_rvh->is_waiting_for_beforeunload_ack());
  base::TimeTicks now = base::TimeTicks::Now();
  orig_rvh->GetMainFrame()->OnMessageReceived(
      FrameHostMsg_BeforeUnload_ACK(0, false, now, now));
  EXPECT_FALSE(orig_rvh->is_waiting_for_beforeunload_ack());
  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_EQ(orig_rvh, contents()->GetRenderViewHost());

  // Navigate again, but simulate an onbeforeunload approval.
  controller().LoadURL(
      url2, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  EXPECT_TRUE(orig_rvh->is_waiting_for_beforeunload_ack());
  now = base::TimeTicks::Now();
  orig_rvh->GetMainFrame()->OnMessageReceived(
      FrameHostMsg_BeforeUnload_ACK(0, true, now, now));
  EXPECT_FALSE(orig_rvh->is_waiting_for_beforeunload_ack());
  EXPECT_TRUE(contents()->cross_navigation_pending());
  TestRenderViewHost* pending_rvh = static_cast<TestRenderViewHost*>(
      contents()->GetPendingRenderViewHost());

  // We won't hear DidNavigate until the onunload handler has finished running.

  // DidNavigate from the pending page.
  contents()->TestDidNavigate(
      pending_rvh, 1, url2, PAGE_TRANSITION_TYPED);
  SiteInstance* instance2 = contents()->GetSiteInstance();
  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_EQ(pending_rvh, rvh());
  EXPECT_NE(instance1, instance2);
  EXPECT_TRUE(contents()->GetPendingRenderViewHost() == NULL);
}

// Test that during a slow cross-site navigation, the original renderer can
// navigate to a different URL and have it displayed, canceling the slow
// navigation.
TEST_F(WebContentsImplTest, CrossSiteNavigationPreempted) {
  TestRenderViewHost* orig_rvh = test_rvh();
  SiteInstance* instance1 = contents()->GetSiteInstance();

  // Navigate to URL.  First URL should use first RenderViewHost.
  const GURL url("http://www.google.com");
  controller().LoadURL(
      url, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  contents()->TestDidNavigate(orig_rvh, 1, url, PAGE_TRANSITION_TYPED);
  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_EQ(orig_rvh, contents()->GetRenderViewHost());

  // Navigate to new site, simulating an onbeforeunload approval.
  const GURL url2("http://www.yahoo.com");
  controller().LoadURL(
      url2, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  EXPECT_TRUE(orig_rvh->is_waiting_for_beforeunload_ack());
  base::TimeTicks now = base::TimeTicks::Now();
  orig_rvh->GetMainFrame()->OnMessageReceived(
      FrameHostMsg_BeforeUnload_ACK(0, true, now, now));
  EXPECT_TRUE(contents()->cross_navigation_pending());

  // Suppose the original renderer navigates before the new one is ready.
  orig_rvh->SendNavigate(2, GURL("http://www.google.com/foo"));

  // Verify that the pending navigation is cancelled.
  EXPECT_FALSE(orig_rvh->is_waiting_for_beforeunload_ack());
  SiteInstance* instance2 = contents()->GetSiteInstance();
  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_EQ(orig_rvh, rvh());
  EXPECT_EQ(instance1, instance2);
  EXPECT_TRUE(contents()->GetPendingRenderViewHost() == NULL);
}

TEST_F(WebContentsImplTest, CrossSiteNavigationBackPreempted) {
  // Start with a web ui page, which gets a new RVH with WebUI bindings.
  const GURL url1("chrome://blah");
  controller().LoadURL(
      url1, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  TestRenderViewHost* ntp_rvh = test_rvh();
  contents()->TestDidNavigate(ntp_rvh, 1, url1, PAGE_TRANSITION_TYPED);
  NavigationEntry* entry1 = controller().GetLastCommittedEntry();
  SiteInstance* instance1 = contents()->GetSiteInstance();

  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_EQ(ntp_rvh, contents()->GetRenderViewHost());
  EXPECT_EQ(url1, entry1->GetURL());
  EXPECT_EQ(instance1,
            NavigationEntryImpl::FromNavigationEntry(entry1)->site_instance());
  EXPECT_TRUE(ntp_rvh->GetEnabledBindings() & BINDINGS_POLICY_WEB_UI);

  // Navigate to new site.
  const GURL url2("http://www.google.com");
  controller().LoadURL(
      url2, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  EXPECT_TRUE(contents()->cross_navigation_pending());
  TestRenderViewHost* google_rvh =
      static_cast<TestRenderViewHost*>(contents()->GetPendingRenderViewHost());

  // Simulate beforeunload approval.
  EXPECT_TRUE(ntp_rvh->is_waiting_for_beforeunload_ack());
  base::TimeTicks now = base::TimeTicks::Now();
  ntp_rvh->GetMainFrame()->OnMessageReceived(
      FrameHostMsg_BeforeUnload_ACK(0, true, now, now));

  // DidNavigate from the pending page.
  contents()->TestDidNavigate(
      google_rvh, 1, url2, PAGE_TRANSITION_TYPED);
  NavigationEntry* entry2 = controller().GetLastCommittedEntry();
  SiteInstance* instance2 = contents()->GetSiteInstance();

  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_EQ(google_rvh, contents()->GetRenderViewHost());
  EXPECT_NE(instance1, instance2);
  EXPECT_FALSE(contents()->GetPendingRenderViewHost());
  EXPECT_EQ(url2, entry2->GetURL());
  EXPECT_EQ(instance2,
            NavigationEntryImpl::FromNavigationEntry(entry2)->site_instance());
  EXPECT_FALSE(google_rvh->GetEnabledBindings() & BINDINGS_POLICY_WEB_UI);

  // Navigate to third page on same site.
  const GURL url3("http://news.google.com");
  controller().LoadURL(
      url3, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  EXPECT_FALSE(contents()->cross_navigation_pending());
  contents()->TestDidNavigate(
      google_rvh, 2, url3, PAGE_TRANSITION_TYPED);
  NavigationEntry* entry3 = controller().GetLastCommittedEntry();
  SiteInstance* instance3 = contents()->GetSiteInstance();

  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_EQ(google_rvh, contents()->GetRenderViewHost());
  EXPECT_EQ(instance2, instance3);
  EXPECT_FALSE(contents()->GetPendingRenderViewHost());
  EXPECT_EQ(url3, entry3->GetURL());
  EXPECT_EQ(instance3,
            NavigationEntryImpl::FromNavigationEntry(entry3)->site_instance());

  // Go back within the site.
  controller().GoBack();
  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_EQ(entry2, controller().GetPendingEntry());

  // Before that commits, go back again.
  controller().GoBack();
  EXPECT_TRUE(contents()->cross_navigation_pending());
  EXPECT_TRUE(contents()->GetPendingRenderViewHost());
  EXPECT_EQ(entry1, controller().GetPendingEntry());

  // Simulate beforeunload approval.
  EXPECT_TRUE(google_rvh->is_waiting_for_beforeunload_ack());
  now = base::TimeTicks::Now();
  google_rvh->GetMainFrame()->OnMessageReceived(
      FrameHostMsg_BeforeUnload_ACK(0, true, now, now));

  // DidNavigate from the first back. This aborts the second back's pending RVH.
  contents()->TestDidNavigate(google_rvh, 1, url2, PAGE_TRANSITION_TYPED);

  // We should commit this page and forget about the second back.
  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_FALSE(controller().GetPendingEntry());
  EXPECT_EQ(google_rvh, contents()->GetRenderViewHost());
  EXPECT_EQ(url2, controller().GetLastCommittedEntry()->GetURL());

  // We should not have corrupted the NTP entry.
  EXPECT_EQ(instance3,
            NavigationEntryImpl::FromNavigationEntry(entry3)->site_instance());
  EXPECT_EQ(instance2,
            NavigationEntryImpl::FromNavigationEntry(entry2)->site_instance());
  EXPECT_EQ(instance1,
            NavigationEntryImpl::FromNavigationEntry(entry1)->site_instance());
  EXPECT_EQ(url1, entry1->GetURL());
}

// Test that during a slow cross-site navigation, a sub-frame navigation in the
// original renderer will not cancel the slow navigation (bug 42029).
TEST_F(WebContentsImplTest, CrossSiteNavigationNotPreemptedByFrame) {
  TestRenderViewHost* orig_rvh = test_rvh();

  // Navigate to URL.  First URL should use first RenderViewHost.
  const GURL url("http://www.google.com");
  controller().LoadURL(
      url, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  contents()->TestDidNavigate(orig_rvh, 1, url, PAGE_TRANSITION_TYPED);
  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_EQ(orig_rvh, contents()->GetRenderViewHost());

  // Start navigating to new site.
  const GURL url2("http://www.yahoo.com");
  controller().LoadURL(
      url2, Referrer(), PAGE_TRANSITION_TYPED, std::string());

  // Simulate a sub-frame navigation arriving and ensure the RVH is still
  // waiting for a before unload response.
  orig_rvh->SendNavigateWithTransition(1, GURL("http://google.com/frame"),
                                       PAGE_TRANSITION_AUTO_SUBFRAME);
  EXPECT_TRUE(orig_rvh->is_waiting_for_beforeunload_ack());

  // Now simulate the onbeforeunload approval and verify the navigation is
  // not canceled.
  base::TimeTicks now = base::TimeTicks::Now();
  orig_rvh->GetMainFrame()->OnMessageReceived(
      FrameHostMsg_BeforeUnload_ACK(0, true, now, now));
  EXPECT_FALSE(orig_rvh->is_waiting_for_beforeunload_ack());
  EXPECT_TRUE(contents()->cross_navigation_pending());
}

// Test that a cross-site navigation is not preempted if the previous
// renderer sends a FrameNavigate message just before being told to stop.
// We should only preempt the cross-site navigation if the previous renderer
// has started a new navigation.  See http://crbug.com/79176.
TEST_F(WebContentsImplTest, CrossSiteNotPreemptedDuringBeforeUnload) {
  // Navigate to NTP URL.
  const GURL url("chrome://blah");
  controller().LoadURL(
      url, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  TestRenderViewHost* orig_rvh = test_rvh();
  EXPECT_FALSE(contents()->cross_navigation_pending());

  // Navigate to new site, with the beforeunload request in flight.
  const GURL url2("http://www.yahoo.com");
  controller().LoadURL(
      url2, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  TestRenderViewHost* pending_rvh =
      static_cast<TestRenderViewHost*>(contents()->GetPendingRenderViewHost());
  EXPECT_TRUE(contents()->cross_navigation_pending());
  EXPECT_TRUE(orig_rvh->is_waiting_for_beforeunload_ack());

  // Suppose the first navigation tries to commit now, with a
  // ViewMsg_Stop in flight.  This should not cancel the pending navigation,
  // but it should act as if the beforeunload ack arrived.
  orig_rvh->SendNavigate(1, GURL("chrome://blah"));
  EXPECT_TRUE(contents()->cross_navigation_pending());
  EXPECT_EQ(orig_rvh, contents()->GetRenderViewHost());
  EXPECT_FALSE(orig_rvh->is_waiting_for_beforeunload_ack());

  // The pending navigation should be able to commit successfully.
  contents()->TestDidNavigate(pending_rvh, 1, url2, PAGE_TRANSITION_TYPED);
  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_EQ(pending_rvh, contents()->GetRenderViewHost());
}

// Test that the original renderer cannot preempt a cross-site navigation once
// the unload request has been made.  At this point, the cross-site navigation
// is almost ready to be displayed, and the original renderer is only given a
// short chance to run an unload handler.  Prevents regression of bug 23942.
TEST_F(WebContentsImplTest, CrossSiteCantPreemptAfterUnload) {
  TestRenderViewHost* orig_rvh = test_rvh();
  SiteInstance* instance1 = contents()->GetSiteInstance();

  // Navigate to URL.  First URL should use first RenderViewHost.
  const GURL url("http://www.google.com");
  controller().LoadURL(
      url, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  contents()->TestDidNavigate(orig_rvh, 1, url, PAGE_TRANSITION_TYPED);
  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_EQ(orig_rvh, contents()->GetRenderViewHost());

  // Navigate to new site, simulating an onbeforeunload approval.
  const GURL url2("http://www.yahoo.com");
  controller().LoadURL(
      url2, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  base::TimeTicks now = base::TimeTicks::Now();
  orig_rvh->GetMainFrame()->OnMessageReceived(
      FrameHostMsg_BeforeUnload_ACK(0, true, now, now));
  EXPECT_TRUE(contents()->cross_navigation_pending());
  TestRenderViewHost* pending_rvh = static_cast<TestRenderViewHost*>(
      contents()->GetPendingRenderViewHost());

  // Simulate the pending renderer's response, which leads to an unload request
  // being sent to orig_rvh.
  std::vector<GURL> url_chain;
  url_chain.push_back(GURL());
  contents()->GetRenderManagerForTesting()->OnCrossSiteResponse(
      contents()->GetRenderManagerForTesting()->pending_frame_host(),
      GlobalRequestID(0, 0), scoped_ptr<CrossSiteTransferringRequest>(),
      url_chain, Referrer(), PAGE_TRANSITION_TYPED, false);

  // Suppose the original renderer navigates now, while the unload request is in
  // flight.  We should ignore it, wait for the unload ack, and let the pending
  // request continue.  Otherwise, the contents may close spontaneously or stop
  // responding to navigation requests.  (See bug 23942.)
  FrameHostMsg_DidCommitProvisionalLoad_Params params1a;
  InitNavigateParams(&params1a, 2, GURL("http://www.google.com/foo"),
                     PAGE_TRANSITION_TYPED);
  orig_rvh->SendNavigate(2, GURL("http://www.google.com/foo"));

  // Verify that the pending navigation is still in progress.
  EXPECT_TRUE(contents()->cross_navigation_pending());
  EXPECT_TRUE(contents()->GetPendingRenderViewHost() != NULL);

  // DidNavigate from the pending page should commit it.
  contents()->TestDidNavigate(
      pending_rvh, 1, url2, PAGE_TRANSITION_TYPED);
  SiteInstance* instance2 = contents()->GetSiteInstance();
  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_EQ(pending_rvh, rvh());
  EXPECT_NE(instance1, instance2);
  EXPECT_TRUE(contents()->GetPendingRenderViewHost() == NULL);
}

// Test that a cross-site navigation that doesn't commit after the unload
// handler doesn't leave the contents in a stuck state.  http://crbug.com/88562
TEST_F(WebContentsImplTest, CrossSiteNavigationCanceled) {
  TestRenderViewHost* orig_rvh = test_rvh();
  SiteInstance* instance1 = contents()->GetSiteInstance();

  // Navigate to URL.  First URL should use first RenderViewHost.
  const GURL url("http://www.google.com");
  controller().LoadURL(
      url, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  contents()->TestDidNavigate(orig_rvh, 1, url, PAGE_TRANSITION_TYPED);
  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_EQ(orig_rvh, contents()->GetRenderViewHost());

  // Navigate to new site, simulating an onbeforeunload approval.
  const GURL url2("http://www.yahoo.com");
  controller().LoadURL(url2, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  EXPECT_TRUE(orig_rvh->is_waiting_for_beforeunload_ack());
  base::TimeTicks now = base::TimeTicks::Now();
  orig_rvh->GetMainFrame()->OnMessageReceived(
      FrameHostMsg_BeforeUnload_ACK(0, true, now, now));
  EXPECT_TRUE(contents()->cross_navigation_pending());

  // Simulate swap out message when the response arrives.
  orig_rvh->OnSwappedOut(false);

  // Suppose the navigation doesn't get a chance to commit, and the user
  // navigates in the current RVH's SiteInstance.
  controller().LoadURL(url, Referrer(), PAGE_TRANSITION_TYPED, std::string());

  // Verify that the pending navigation is cancelled and the renderer is no
  // longer swapped out.
  EXPECT_FALSE(orig_rvh->is_waiting_for_beforeunload_ack());
  SiteInstance* instance2 = contents()->GetSiteInstance();
  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_EQ(orig_rvh, rvh());
  EXPECT_EQ(RenderViewHostImpl::STATE_DEFAULT, orig_rvh->rvh_state());
  EXPECT_EQ(instance1, instance2);
  EXPECT_TRUE(contents()->GetPendingRenderViewHost() == NULL);
}

// Test that NavigationEntries have the correct page state after going
// forward and back.  Prevents regression for bug 1116137.
TEST_F(WebContentsImplTest, NavigationEntryContentState) {
  TestRenderViewHost* orig_rvh = test_rvh();

  // Navigate to URL.  There should be no committed entry yet.
  const GURL url("http://www.google.com");
  controller().LoadURL(url, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  NavigationEntry* entry = controller().GetLastCommittedEntry();
  EXPECT_TRUE(entry == NULL);

  // Committed entry should have page state after DidNavigate.
  contents()->TestDidNavigate(orig_rvh, 1, url, PAGE_TRANSITION_TYPED);
  entry = controller().GetLastCommittedEntry();
  EXPECT_TRUE(entry->GetPageState().IsValid());

  // Navigate to same site.
  const GURL url2("http://images.google.com");
  controller().LoadURL(url2, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  entry = controller().GetLastCommittedEntry();
  EXPECT_TRUE(entry->GetPageState().IsValid());

  // Committed entry should have page state after DidNavigate.
  contents()->TestDidNavigate(orig_rvh, 2, url2, PAGE_TRANSITION_TYPED);
  entry = controller().GetLastCommittedEntry();
  EXPECT_TRUE(entry->GetPageState().IsValid());

  // Now go back.  Committed entry should still have page state.
  controller().GoBack();
  contents()->TestDidNavigate(orig_rvh, 1, url, PAGE_TRANSITION_TYPED);
  entry = controller().GetLastCommittedEntry();
  EXPECT_TRUE(entry->GetPageState().IsValid());
}

// Test that NavigationEntries have the correct page state and SiteInstance
// state after opening a new window to about:blank.  Prevents regression for
// bugs b/1116137 and http://crbug.com/111975.
TEST_F(WebContentsImplTest, NavigationEntryContentStateNewWindow) {
  TestRenderViewHost* orig_rvh = test_rvh();

  // When opening a new window, it is navigated to about:blank internally.
  // Currently, this results in two DidNavigate events.
  const GURL url(url::kAboutBlankURL);
  contents()->TestDidNavigate(orig_rvh, 1, url, PAGE_TRANSITION_TYPED);
  contents()->TestDidNavigate(orig_rvh, 1, url, PAGE_TRANSITION_TYPED);

  // Should have a page state here.
  NavigationEntry* entry = controller().GetLastCommittedEntry();
  EXPECT_TRUE(entry->GetPageState().IsValid());

  // The SiteInstance should be available for other navigations to use.
  NavigationEntryImpl* entry_impl =
      NavigationEntryImpl::FromNavigationEntry(entry);
  EXPECT_FALSE(entry_impl->site_instance()->HasSite());
  int32 site_instance_id = entry_impl->site_instance()->GetId();

  // Navigating to a normal page should not cause a process swap.
  const GURL new_url("http://www.google.com");
  controller().LoadURL(new_url, Referrer(),
                       PAGE_TRANSITION_TYPED, std::string());
  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_EQ(orig_rvh, contents()->GetRenderViewHost());
  contents()->TestDidNavigate(orig_rvh, 1, new_url, PAGE_TRANSITION_TYPED);
  NavigationEntryImpl* entry_impl2 = NavigationEntryImpl::FromNavigationEntry(
      controller().GetLastCommittedEntry());
  EXPECT_EQ(site_instance_id, entry_impl2->site_instance()->GetId());
  EXPECT_TRUE(entry_impl2->site_instance()->HasSite());
}

// Tests that fullscreen is exited throughout the object hierarchy when
// navigating to a new page.
TEST_F(WebContentsImplTest, NavigationExitsFullscreen) {
  FakeFullscreenDelegate fake_delegate;
  contents()->SetDelegate(&fake_delegate);
  TestRenderViewHost* orig_rvh = test_rvh();

  // Navigate to a site.
  const GURL url("http://www.google.com");
  controller().LoadURL(
      url, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  contents()->TestDidNavigate(orig_rvh, 1, url, PAGE_TRANSITION_TYPED);
  EXPECT_EQ(orig_rvh, contents()->GetRenderViewHost());

  // Toggle fullscreen mode on (as if initiated via IPC from renderer).
  EXPECT_FALSE(orig_rvh->IsFullscreen());
  EXPECT_FALSE(contents()->IsFullscreenForCurrentTab());
  EXPECT_FALSE(fake_delegate.IsFullscreenForTabOrPending(contents()));
  orig_rvh->OnMessageReceived(
      ViewHostMsg_ToggleFullscreen(orig_rvh->GetRoutingID(), true));
  EXPECT_TRUE(orig_rvh->IsFullscreen());
  EXPECT_TRUE(contents()->IsFullscreenForCurrentTab());
  EXPECT_TRUE(fake_delegate.IsFullscreenForTabOrPending(contents()));

  // Navigate to a new site.
  const GURL url2("http://www.yahoo.com");
  controller().LoadURL(
      url2, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  RenderViewHost* const pending_rvh = contents()->GetPendingRenderViewHost();
  contents()->TestDidNavigate(
      pending_rvh, 1, url2, PAGE_TRANSITION_TYPED);

  // Confirm fullscreen has exited.
  EXPECT_FALSE(orig_rvh->IsFullscreen());
  EXPECT_FALSE(contents()->IsFullscreenForCurrentTab());
  EXPECT_FALSE(fake_delegate.IsFullscreenForTabOrPending(contents()));

  contents()->SetDelegate(NULL);
}

// Tests that fullscreen is exited throughout the object hierarchy when
// instructing NavigationController to GoBack() or GoForward().
TEST_F(WebContentsImplTest, HistoryNavigationExitsFullscreen) {
  FakeFullscreenDelegate fake_delegate;
  contents()->SetDelegate(&fake_delegate);
  TestRenderViewHost* const orig_rvh = test_rvh();

  // Navigate to a site.
  const GURL url("http://www.google.com");
  controller().LoadURL(url, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  contents()->TestDidNavigate(orig_rvh, 1, url, PAGE_TRANSITION_TYPED);
  EXPECT_EQ(orig_rvh, contents()->GetRenderViewHost());

  // Now, navigate to another page on the same site.
  const GURL url2("http://www.google.com/search?q=kittens");
  controller().LoadURL(url2, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  EXPECT_FALSE(contents()->cross_navigation_pending());
  contents()->TestDidNavigate(orig_rvh, 2, url2, PAGE_TRANSITION_TYPED);
  EXPECT_EQ(orig_rvh, contents()->GetRenderViewHost());

  // Sanity-check: Confirm we're not starting out in fullscreen mode.
  EXPECT_FALSE(orig_rvh->IsFullscreen());
  EXPECT_FALSE(contents()->IsFullscreenForCurrentTab());
  EXPECT_FALSE(fake_delegate.IsFullscreenForTabOrPending(contents()));

  for (int i = 0; i < 2; ++i) {
    // Toggle fullscreen mode on (as if initiated via IPC from renderer).
    orig_rvh->OnMessageReceived(
        ViewHostMsg_ToggleFullscreen(orig_rvh->GetRoutingID(), true));
    EXPECT_TRUE(orig_rvh->IsFullscreen());
    EXPECT_TRUE(contents()->IsFullscreenForCurrentTab());
    EXPECT_TRUE(fake_delegate.IsFullscreenForTabOrPending(contents()));

    // Navigate backward (or forward).
    if (i == 0)
      controller().GoBack();
    else
      controller().GoForward();
    EXPECT_FALSE(contents()->cross_navigation_pending());
    EXPECT_EQ(orig_rvh, contents()->GetRenderViewHost());
    contents()->TestDidNavigate(
        orig_rvh, i + 1, url, PAGE_TRANSITION_FORWARD_BACK);

    // Confirm fullscreen has exited.
    EXPECT_FALSE(orig_rvh->IsFullscreen());
    EXPECT_FALSE(contents()->IsFullscreenForCurrentTab());
    EXPECT_FALSE(fake_delegate.IsFullscreenForTabOrPending(contents()));
  }

  contents()->SetDelegate(NULL);
}

TEST_F(WebContentsImplTest, TerminateHidesValidationMessage) {
  FakeValidationMessageDelegate fake_delegate;
  contents()->SetDelegate(&fake_delegate);
  EXPECT_FALSE(fake_delegate.hide_validation_message_was_called());

  // Crash the renderer.
  test_rvh()->OnMessageReceived(
      ViewHostMsg_RenderProcessGone(
          0, base::TERMINATION_STATUS_PROCESS_CRASHED, -1));

  // Confirm HideValidationMessage was called.
  EXPECT_TRUE(fake_delegate.hide_validation_message_was_called());

  contents()->SetDelegate(NULL);
}

// Tests that fullscreen is exited throughout the object hierarchy on a renderer
// crash.
TEST_F(WebContentsImplTest, CrashExitsFullscreen) {
  FakeFullscreenDelegate fake_delegate;
  contents()->SetDelegate(&fake_delegate);

  // Navigate to a site.
  const GURL url("http://www.google.com");
  controller().LoadURL(
      url, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  contents()->TestDidNavigate(test_rvh(), 1, url, PAGE_TRANSITION_TYPED);
  EXPECT_EQ(test_rvh(), contents()->GetRenderViewHost());

  // Toggle fullscreen mode on (as if initiated via IPC from renderer).
  EXPECT_FALSE(test_rvh()->IsFullscreen());
  EXPECT_FALSE(contents()->IsFullscreenForCurrentTab());
  EXPECT_FALSE(fake_delegate.IsFullscreenForTabOrPending(contents()));
  test_rvh()->OnMessageReceived(
      ViewHostMsg_ToggleFullscreen(test_rvh()->GetRoutingID(), true));
  EXPECT_TRUE(test_rvh()->IsFullscreen());
  EXPECT_TRUE(contents()->IsFullscreenForCurrentTab());
  EXPECT_TRUE(fake_delegate.IsFullscreenForTabOrPending(contents()));

  // Crash the renderer.
  test_rvh()->OnMessageReceived(
      ViewHostMsg_RenderProcessGone(
          0, base::TERMINATION_STATUS_PROCESS_CRASHED, -1));

  // Confirm fullscreen has exited.
  EXPECT_FALSE(test_rvh()->IsFullscreen());
  EXPECT_FALSE(contents()->IsFullscreenForCurrentTab());
  EXPECT_FALSE(fake_delegate.IsFullscreenForTabOrPending(contents()));

  contents()->SetDelegate(NULL);
}

////////////////////////////////////////////////////////////////////////////////
// Interstitial Tests
////////////////////////////////////////////////////////////////////////////////

// Test navigating to a page (with the navigation initiated from the browser,
// as when a URL is typed in the location bar) that shows an interstitial and
// creates a new navigation entry, then hiding it without proceeding.
TEST_F(WebContentsImplTest,
       ShowInterstitialFromBrowserWithNewNavigationDontProceed) {
  // Navigate to a page.
  GURL url1("http://www.google.com");
  test_rvh()->SendNavigate(1, url1);
  EXPECT_EQ(1, controller().GetEntryCount());

  // Initiate a browser navigation that will trigger the interstitial
  controller().LoadURL(GURL("http://www.evil.com"), Referrer(),
                        PAGE_TRANSITION_TYPED, std::string());

  // Show an interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::INVALID;
  bool deleted = false;
  GURL url2("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents(), true, url2, &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  // The interstitial should not show until its navigation has committed.
  EXPECT_FALSE(interstitial->is_showing());
  EXPECT_FALSE(contents()->ShowingInterstitialPage());
  EXPECT_TRUE(contents()->GetInterstitialPage() == NULL);
  // Let's commit the interstitial navigation.
  interstitial->TestDidNavigate(1, url2);
  EXPECT_TRUE(interstitial->is_showing());
  EXPECT_TRUE(contents()->ShowingInterstitialPage());
  EXPECT_TRUE(contents()->GetInterstitialPage() == interstitial);
  NavigationEntry* entry = controller().GetVisibleEntry();
  ASSERT_TRUE(entry != NULL);
  EXPECT_TRUE(entry->GetURL() == url2);

  // Now don't proceed.
  interstitial->DontProceed();
  EXPECT_EQ(TestInterstitialPage::CANCELED, state);
  EXPECT_FALSE(contents()->ShowingInterstitialPage());
  EXPECT_TRUE(contents()->GetInterstitialPage() == NULL);
  entry = controller().GetVisibleEntry();
  ASSERT_TRUE(entry != NULL);
  EXPECT_TRUE(entry->GetURL() == url1);
  EXPECT_EQ(1, controller().GetEntryCount());

  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted);
}

// Test navigating to a page (with the navigation initiated from the renderer,
// as when clicking on a link in the page) that shows an interstitial and
// creates a new navigation entry, then hiding it without proceeding.
TEST_F(WebContentsImplTest,
       ShowInterstitiaFromRendererlWithNewNavigationDontProceed) {
  // Navigate to a page.
  GURL url1("http://www.google.com");
  test_rvh()->SendNavigate(1, url1);
  EXPECT_EQ(1, controller().GetEntryCount());

  // Show an interstitial (no pending entry, the interstitial would have been
  // triggered by clicking on a link).
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::INVALID;
  bool deleted = false;
  GURL url2("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents(), true, url2, &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  // The interstitial should not show until its navigation has committed.
  EXPECT_FALSE(interstitial->is_showing());
  EXPECT_FALSE(contents()->ShowingInterstitialPage());
  EXPECT_TRUE(contents()->GetInterstitialPage() == NULL);
  // Let's commit the interstitial navigation.
  interstitial->TestDidNavigate(1, url2);
  EXPECT_TRUE(interstitial->is_showing());
  EXPECT_TRUE(contents()->ShowingInterstitialPage());
  EXPECT_TRUE(contents()->GetInterstitialPage() == interstitial);
  NavigationEntry* entry = controller().GetVisibleEntry();
  ASSERT_TRUE(entry != NULL);
  EXPECT_TRUE(entry->GetURL() == url2);

  // Now don't proceed.
  interstitial->DontProceed();
  EXPECT_EQ(TestInterstitialPage::CANCELED, state);
  EXPECT_FALSE(contents()->ShowingInterstitialPage());
  EXPECT_TRUE(contents()->GetInterstitialPage() == NULL);
  entry = controller().GetVisibleEntry();
  ASSERT_TRUE(entry != NULL);
  EXPECT_TRUE(entry->GetURL() == url1);
  EXPECT_EQ(1, controller().GetEntryCount());

  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted);
}

// Test navigating to a page that shows an interstitial without creating a new
// navigation entry (this happens when the interstitial is triggered by a
// sub-resource in the page), then hiding it without proceeding.
TEST_F(WebContentsImplTest, ShowInterstitialNoNewNavigationDontProceed) {
  // Navigate to a page.
  GURL url1("http://www.google.com");
  test_rvh()->SendNavigate(1, url1);
  EXPECT_EQ(1, controller().GetEntryCount());

  // Show an interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::INVALID;
  bool deleted = false;
  GURL url2("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents(), false, url2, &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  // The interstitial should not show until its navigation has committed.
  EXPECT_FALSE(interstitial->is_showing());
  EXPECT_FALSE(contents()->ShowingInterstitialPage());
  EXPECT_TRUE(contents()->GetInterstitialPage() == NULL);
  // Let's commit the interstitial navigation.
  interstitial->TestDidNavigate(1, url2);
  EXPECT_TRUE(interstitial->is_showing());
  EXPECT_TRUE(contents()->ShowingInterstitialPage());
  EXPECT_TRUE(contents()->GetInterstitialPage() == interstitial);
  NavigationEntry* entry = controller().GetVisibleEntry();
  ASSERT_TRUE(entry != NULL);
  // The URL specified to the interstitial should have been ignored.
  EXPECT_TRUE(entry->GetURL() == url1);

  // Now don't proceed.
  interstitial->DontProceed();
  EXPECT_EQ(TestInterstitialPage::CANCELED, state);
  EXPECT_FALSE(contents()->ShowingInterstitialPage());
  EXPECT_TRUE(contents()->GetInterstitialPage() == NULL);
  entry = controller().GetVisibleEntry();
  ASSERT_TRUE(entry != NULL);
  EXPECT_TRUE(entry->GetURL() == url1);
  EXPECT_EQ(1, controller().GetEntryCount());

  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted);
}

// Test navigating to a page (with the navigation initiated from the browser,
// as when a URL is typed in the location bar) that shows an interstitial and
// creates a new navigation entry, then proceeding.
TEST_F(WebContentsImplTest,
       ShowInterstitialFromBrowserNewNavigationProceed) {
  // Navigate to a page.
  GURL url1("http://www.google.com");
  test_rvh()->SendNavigate(1, url1);
  EXPECT_EQ(1, controller().GetEntryCount());

  // Initiate a browser navigation that will trigger the interstitial
  controller().LoadURL(GURL("http://www.evil.com"), Referrer(),
                        PAGE_TRANSITION_TYPED, std::string());

  // Show an interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::INVALID;
  bool deleted = false;
  GURL url2("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents(), true, url2, &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  // The interstitial should not show until its navigation has committed.
  EXPECT_FALSE(interstitial->is_showing());
  EXPECT_FALSE(contents()->ShowingInterstitialPage());
  EXPECT_TRUE(contents()->GetInterstitialPage() == NULL);
  // Let's commit the interstitial navigation.
  interstitial->TestDidNavigate(1, url2);
  EXPECT_TRUE(interstitial->is_showing());
  EXPECT_TRUE(contents()->ShowingInterstitialPage());
  EXPECT_TRUE(contents()->GetInterstitialPage() == interstitial);
  NavigationEntry* entry = controller().GetVisibleEntry();
  ASSERT_TRUE(entry != NULL);
  EXPECT_TRUE(entry->GetURL() == url2);

  // Then proceed.
  interstitial->Proceed();
  // The interstitial should show until the new navigation commits.
  RunAllPendingInMessageLoop();
  ASSERT_FALSE(deleted);
  EXPECT_EQ(TestInterstitialPage::OKED, state);
  EXPECT_TRUE(contents()->ShowingInterstitialPage());
  EXPECT_TRUE(contents()->GetInterstitialPage() == interstitial);

  // Simulate the navigation to the page, that's when the interstitial gets
  // hidden.
  GURL url3("http://www.thepage.com");
  test_rvh()->SendNavigate(2, url3);

  EXPECT_FALSE(contents()->ShowingInterstitialPage());
  EXPECT_TRUE(contents()->GetInterstitialPage() == NULL);
  entry = controller().GetVisibleEntry();
  ASSERT_TRUE(entry != NULL);
  EXPECT_TRUE(entry->GetURL() == url3);

  EXPECT_EQ(2, controller().GetEntryCount());

  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted);
}

// Test navigating to a page (with the navigation initiated from the renderer,
// as when clicking on a link in the page) that shows an interstitial and
// creates a new navigation entry, then proceeding.
TEST_F(WebContentsImplTest,
       ShowInterstitialFromRendererNewNavigationProceed) {
  // Navigate to a page.
  GURL url1("http://www.google.com");
  test_rvh()->SendNavigate(1, url1);
  EXPECT_EQ(1, controller().GetEntryCount());

  // Show an interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::INVALID;
  bool deleted = false;
  GURL url2("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents(), true, url2, &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  // The interstitial should not show until its navigation has committed.
  EXPECT_FALSE(interstitial->is_showing());
  EXPECT_FALSE(contents()->ShowingInterstitialPage());
  EXPECT_TRUE(contents()->GetInterstitialPage() == NULL);
  // Let's commit the interstitial navigation.
  interstitial->TestDidNavigate(1, url2);
  EXPECT_TRUE(interstitial->is_showing());
  EXPECT_TRUE(contents()->ShowingInterstitialPage());
  EXPECT_TRUE(contents()->GetInterstitialPage() == interstitial);
  NavigationEntry* entry = controller().GetVisibleEntry();
  ASSERT_TRUE(entry != NULL);
  EXPECT_TRUE(entry->GetURL() == url2);

  // Then proceed.
  interstitial->Proceed();
  // The interstitial should show until the new navigation commits.
  RunAllPendingInMessageLoop();
  ASSERT_FALSE(deleted);
  EXPECT_EQ(TestInterstitialPage::OKED, state);
  EXPECT_TRUE(contents()->ShowingInterstitialPage());
  EXPECT_TRUE(contents()->GetInterstitialPage() == interstitial);

  // Simulate the navigation to the page, that's when the interstitial gets
  // hidden.
  GURL url3("http://www.thepage.com");
  test_rvh()->SendNavigate(2, url3);

  EXPECT_FALSE(contents()->ShowingInterstitialPage());
  EXPECT_TRUE(contents()->GetInterstitialPage() == NULL);
  entry = controller().GetVisibleEntry();
  ASSERT_TRUE(entry != NULL);
  EXPECT_TRUE(entry->GetURL() == url3);

  EXPECT_EQ(2, controller().GetEntryCount());

  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted);
}

// Test navigating to a page that shows an interstitial without creating a new
// navigation entry (this happens when the interstitial is triggered by a
// sub-resource in the page), then proceeding.
TEST_F(WebContentsImplTest, ShowInterstitialNoNewNavigationProceed) {
  // Navigate to a page so we have a navigation entry in the controller.
  GURL url1("http://www.google.com");
  test_rvh()->SendNavigate(1, url1);
  EXPECT_EQ(1, controller().GetEntryCount());

  // Show an interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::INVALID;
  bool deleted = false;
  GURL url2("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents(), false, url2, &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  // The interstitial should not show until its navigation has committed.
  EXPECT_FALSE(interstitial->is_showing());
  EXPECT_FALSE(contents()->ShowingInterstitialPage());
  EXPECT_TRUE(contents()->GetInterstitialPage() == NULL);
  // Let's commit the interstitial navigation.
  interstitial->TestDidNavigate(1, url2);
  EXPECT_TRUE(interstitial->is_showing());
  EXPECT_TRUE(contents()->ShowingInterstitialPage());
  EXPECT_TRUE(contents()->GetInterstitialPage() == interstitial);
  NavigationEntry* entry = controller().GetVisibleEntry();
  ASSERT_TRUE(entry != NULL);
  // The URL specified to the interstitial should have been ignored.
  EXPECT_TRUE(entry->GetURL() == url1);

  // Then proceed.
  interstitial->Proceed();
  // Since this is not a new navigation, the previous page is dismissed right
  // away and shows the original page.
  EXPECT_EQ(TestInterstitialPage::OKED, state);
  EXPECT_FALSE(contents()->ShowingInterstitialPage());
  EXPECT_TRUE(contents()->GetInterstitialPage() == NULL);
  entry = controller().GetVisibleEntry();
  ASSERT_TRUE(entry != NULL);
  EXPECT_TRUE(entry->GetURL() == url1);

  EXPECT_EQ(1, controller().GetEntryCount());

  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted);
}

// Test navigating to a page that shows an interstitial, then navigating away.
TEST_F(WebContentsImplTest, ShowInterstitialThenNavigate) {
  // Show interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::INVALID;
  bool deleted = false;
  GURL url("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents(), true, url, &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  interstitial->TestDidNavigate(1, url);

  // While interstitial showing, navigate to a new URL.
  const GURL url2("http://www.yahoo.com");
  test_rvh()->SendNavigate(1, url2);

  EXPECT_EQ(TestInterstitialPage::CANCELED, state);

  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted);
}

// Test navigating to a page that shows an interstitial, then going back.
TEST_F(WebContentsImplTest, ShowInterstitialThenGoBack) {
  // Navigate to a page so we have a navigation entry in the controller.
  GURL url1("http://www.google.com");
  test_rvh()->SendNavigate(1, url1);
  EXPECT_EQ(1, controller().GetEntryCount());

  // Show interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::INVALID;
  bool deleted = false;
  GURL interstitial_url("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents(), true, interstitial_url,
                               &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  interstitial->TestDidNavigate(2, interstitial_url);

  // While the interstitial is showing, go back.
  controller().GoBack();
  test_rvh()->SendNavigate(1, url1);

  // Make sure we are back to the original page and that the interstitial is
  // gone.
  EXPECT_EQ(TestInterstitialPage::CANCELED, state);
  NavigationEntry* entry = controller().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(url1.spec(), entry->GetURL().spec());

  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted);
}

// Test navigating to a page that shows an interstitial, has a renderer crash,
// and then goes back.
TEST_F(WebContentsImplTest, ShowInterstitialCrashRendererThenGoBack) {
  // Navigate to a page so we have a navigation entry in the controller.
  GURL url1("http://www.google.com");
  test_rvh()->SendNavigate(1, url1);
  EXPECT_EQ(1, controller().GetEntryCount());

  // Show interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::INVALID;
  bool deleted = false;
  GURL interstitial_url("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents(), true, interstitial_url,
                               &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  interstitial->TestDidNavigate(2, interstitial_url);

  // Crash the renderer
  test_rvh()->OnMessageReceived(
      ViewHostMsg_RenderProcessGone(
          0, base::TERMINATION_STATUS_PROCESS_CRASHED, -1));

  // While the interstitial is showing, go back.
  controller().GoBack();
  test_rvh()->SendNavigate(1, url1);

  // Make sure we are back to the original page and that the interstitial is
  // gone.
  EXPECT_EQ(TestInterstitialPage::CANCELED, state);
  NavigationEntry* entry = controller().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(url1.spec(), entry->GetURL().spec());

  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted);
}

// Test navigating to a page that shows an interstitial, has the renderer crash,
// and then navigates to the interstitial.
TEST_F(WebContentsImplTest, ShowInterstitialCrashRendererThenNavigate) {
  // Navigate to a page so we have a navigation entry in the controller.
  GURL url1("http://www.google.com");
  test_rvh()->SendNavigate(1, url1);
  EXPECT_EQ(1, controller().GetEntryCount());

  // Show interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::INVALID;
  bool deleted = false;
  GURL interstitial_url("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents(), true, interstitial_url,
                               &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();

  // Crash the renderer
  test_rvh()->OnMessageReceived(
      ViewHostMsg_RenderProcessGone(
          0, base::TERMINATION_STATUS_PROCESS_CRASHED, -1));

  interstitial->TestDidNavigate(2, interstitial_url);
}

// Test navigating to a page that shows an interstitial, then close the
// contents.
TEST_F(WebContentsImplTest, ShowInterstitialThenCloseTab) {
  // Show interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::INVALID;
  bool deleted = false;
  GURL url("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents(), true, url, &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  interstitial->TestDidNavigate(1, url);

  // Now close the contents.
  DeleteContents();
  EXPECT_EQ(TestInterstitialPage::CANCELED, state);

  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted);
}

// Test navigating to a page that shows an interstitial, then close the
// contents.
TEST_F(WebContentsImplTest, ShowInterstitialThenCloseAndShutdown) {
  // Show interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::INVALID;
  bool deleted = false;
  GURL url("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents(), true, url, &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  interstitial->TestDidNavigate(1, url);
  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
      interstitial->GetRenderViewHostForTesting());

  // Now close the contents.
  DeleteContents();
  EXPECT_EQ(TestInterstitialPage::CANCELED, state);

  // Before the interstitial has a chance to process its shutdown task,
  // simulate quitting the browser.  This goes through all processes and
  // tells them to destruct.
  rvh->OnMessageReceived(
        ViewHostMsg_RenderProcessGone(0, 0, 0));

  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted);
}

// Test that after Proceed is called and an interstitial is still shown, no more
// commands get executed.
TEST_F(WebContentsImplTest, ShowInterstitialProceedMultipleCommands) {
  // Navigate to a page so we have a navigation entry in the controller.
  GURL url1("http://www.google.com");
  test_rvh()->SendNavigate(1, url1);
  EXPECT_EQ(1, controller().GetEntryCount());

  // Show an interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::INVALID;
  bool deleted = false;
  GURL url2("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents(), true, url2, &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  interstitial->TestDidNavigate(1, url2);

  // Run a command.
  EXPECT_EQ(0, interstitial->command_received_count());
  interstitial->TestDomOperationResponse("toto");
  EXPECT_EQ(1, interstitial->command_received_count());

  // Then proceed.
  interstitial->Proceed();
  RunAllPendingInMessageLoop();
  ASSERT_FALSE(deleted);

  // While the navigation to the new page is pending, send other commands, they
  // should be ignored.
  interstitial->TestDomOperationResponse("hello");
  interstitial->TestDomOperationResponse("hi");
  EXPECT_EQ(1, interstitial->command_received_count());
}

// Test showing an interstitial while another interstitial is already showing.
TEST_F(WebContentsImplTest, ShowInterstitialOnInterstitial) {
  // Navigate to a page so we have a navigation entry in the controller.
  GURL start_url("http://www.google.com");
  test_rvh()->SendNavigate(1, start_url);
  EXPECT_EQ(1, controller().GetEntryCount());

  // Show an interstitial.
  TestInterstitialPage::InterstitialState state1 =
      TestInterstitialPage::INVALID;
  bool deleted1 = false;
  GURL url1("http://interstitial1");
  TestInterstitialPage* interstitial1 =
      new TestInterstitialPage(contents(), true, url1, &state1, &deleted1);
  TestInterstitialPageStateGuard state_guard1(interstitial1);
  interstitial1->Show();
  interstitial1->TestDidNavigate(1, url1);

  // Now show another interstitial.
  TestInterstitialPage::InterstitialState state2 =
      TestInterstitialPage::INVALID;
  bool deleted2 = false;
  GURL url2("http://interstitial2");
  TestInterstitialPage* interstitial2 =
      new TestInterstitialPage(contents(), true, url2, &state2, &deleted2);
  TestInterstitialPageStateGuard state_guard2(interstitial2);
  interstitial2->Show();
  interstitial2->TestDidNavigate(1, url2);

  // Showing interstitial2 should have caused interstitial1 to go away.
  EXPECT_EQ(TestInterstitialPage::CANCELED, state1);
  EXPECT_EQ(TestInterstitialPage::UNDECIDED, state2);

  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted1);
  ASSERT_FALSE(deleted2);

  // Let's make sure interstitial2 is working as intended.
  interstitial2->Proceed();
  GURL landing_url("http://www.thepage.com");
  test_rvh()->SendNavigate(2, landing_url);

  EXPECT_FALSE(contents()->ShowingInterstitialPage());
  EXPECT_TRUE(contents()->GetInterstitialPage() == NULL);
  NavigationEntry* entry = controller().GetVisibleEntry();
  ASSERT_TRUE(entry != NULL);
  EXPECT_TRUE(entry->GetURL() == landing_url);
  EXPECT_EQ(2, controller().GetEntryCount());
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted2);
}

// Test showing an interstitial, proceeding and then navigating to another
// interstitial.
TEST_F(WebContentsImplTest, ShowInterstitialProceedShowInterstitial) {
  // Navigate to a page so we have a navigation entry in the controller.
  GURL start_url("http://www.google.com");
  test_rvh()->SendNavigate(1, start_url);
  EXPECT_EQ(1, controller().GetEntryCount());

  // Show an interstitial.
  TestInterstitialPage::InterstitialState state1 =
      TestInterstitialPage::INVALID;
  bool deleted1 = false;
  GURL url1("http://interstitial1");
  TestInterstitialPage* interstitial1 =
      new TestInterstitialPage(contents(), true, url1, &state1, &deleted1);
  TestInterstitialPageStateGuard state_guard1(interstitial1);
  interstitial1->Show();
  interstitial1->TestDidNavigate(1, url1);

  // Take action.  The interstitial won't be hidden until the navigation is
  // committed.
  interstitial1->Proceed();
  EXPECT_EQ(TestInterstitialPage::OKED, state1);

  // Now show another interstitial (simulating the navigation causing another
  // interstitial).
  TestInterstitialPage::InterstitialState state2 =
      TestInterstitialPage::INVALID;
  bool deleted2 = false;
  GURL url2("http://interstitial2");
  TestInterstitialPage* interstitial2 =
      new TestInterstitialPage(contents(), true, url2, &state2, &deleted2);
  TestInterstitialPageStateGuard state_guard2(interstitial2);
  interstitial2->Show();
  interstitial2->TestDidNavigate(1, url2);

  // Showing interstitial2 should have caused interstitial1 to go away.
  EXPECT_EQ(TestInterstitialPage::UNDECIDED, state2);
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted1);
  ASSERT_FALSE(deleted2);

  // Let's make sure interstitial2 is working as intended.
  interstitial2->Proceed();
  GURL landing_url("http://www.thepage.com");
  test_rvh()->SendNavigate(2, landing_url);

  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted2);
  EXPECT_FALSE(contents()->ShowingInterstitialPage());
  EXPECT_TRUE(contents()->GetInterstitialPage() == NULL);
  NavigationEntry* entry = controller().GetVisibleEntry();
  ASSERT_TRUE(entry != NULL);
  EXPECT_TRUE(entry->GetURL() == landing_url);
  EXPECT_EQ(2, controller().GetEntryCount());
}

// Test that navigating away from an interstitial while it's loading cause it
// not to show.
TEST_F(WebContentsImplTest, NavigateBeforeInterstitialShows) {
  // Show an interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::INVALID;
  bool deleted = false;
  GURL interstitial_url("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents(), true, interstitial_url,
                               &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();

  // Let's simulate a navigation initiated from the browser before the
  // interstitial finishes loading.
  const GURL url("http://www.google.com");
  controller().LoadURL(url, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  EXPECT_FALSE(interstitial->is_showing());
  RunAllPendingInMessageLoop();
  ASSERT_FALSE(deleted);

  // Now let's make the interstitial navigation commit.
  interstitial->TestDidNavigate(1, interstitial_url);

  // After it loaded the interstitial should be gone.
  EXPECT_EQ(TestInterstitialPage::CANCELED, state);

  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted);
}

// Test that a new request to show an interstitial while an interstitial is
// pending does not cause problems. htp://crbug/29655 and htp://crbug/9442.
TEST_F(WebContentsImplTest, TwoQuickInterstitials) {
  GURL interstitial_url("http://interstitial");

  // Show a first interstitial.
  TestInterstitialPage::InterstitialState state1 =
      TestInterstitialPage::INVALID;
  bool deleted1 = false;
  TestInterstitialPage* interstitial1 =
      new TestInterstitialPage(contents(), true, interstitial_url,
                               &state1, &deleted1);
  TestInterstitialPageStateGuard state_guard1(interstitial1);
  interstitial1->Show();

  // Show another interstitial on that same contents before the first one had
  // time to load.
  TestInterstitialPage::InterstitialState state2 =
      TestInterstitialPage::INVALID;
  bool deleted2 = false;
  TestInterstitialPage* interstitial2 =
      new TestInterstitialPage(contents(), true, interstitial_url,
                               &state2, &deleted2);
  TestInterstitialPageStateGuard state_guard2(interstitial2);
  interstitial2->Show();

  // The first interstitial should have been closed and deleted.
  EXPECT_EQ(TestInterstitialPage::CANCELED, state1);
  // The 2nd one should still be OK.
  EXPECT_EQ(TestInterstitialPage::UNDECIDED, state2);

  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted1);
  ASSERT_FALSE(deleted2);

  // Make the interstitial navigation commit it should be showing.
  interstitial2->TestDidNavigate(1, interstitial_url);
  EXPECT_EQ(interstitial2, contents()->GetInterstitialPage());
}

// Test showing an interstitial and have its renderer crash.
TEST_F(WebContentsImplTest, InterstitialCrasher) {
  // Show an interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::INVALID;
  bool deleted = false;
  GURL url("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents(), true, url, &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  // Simulate a renderer crash before the interstitial is shown.
  interstitial->TestRenderViewTerminated(
      base::TERMINATION_STATUS_PROCESS_CRASHED, -1);
  // The interstitial should have been dismissed.
  EXPECT_EQ(TestInterstitialPage::CANCELED, state);
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted);

  // Now try again but this time crash the intersitial after it was shown.
  interstitial =
      new TestInterstitialPage(contents(), true, url, &state, &deleted);
  interstitial->Show();
  interstitial->TestDidNavigate(1, url);
  // Simulate a renderer crash.
  interstitial->TestRenderViewTerminated(
      base::TERMINATION_STATUS_PROCESS_CRASHED, -1);
  // The interstitial should have been dismissed.
  EXPECT_EQ(TestInterstitialPage::CANCELED, state);
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted);
}

// Tests that showing an interstitial as a result of a browser initiated
// navigation while an interstitial is showing does not remove the pending
// entry (see http://crbug.com/9791).
TEST_F(WebContentsImplTest, NewInterstitialDoesNotCancelPendingEntry) {
  const char kUrl[] = "http://www.badguys.com/";
  const GURL kGURL(kUrl);

  // Start a navigation to a page
  contents()->GetController().LoadURL(
      kGURL, Referrer(), PAGE_TRANSITION_TYPED, std::string());

  // Simulate that navigation triggering an interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::INVALID;
  bool deleted = false;
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents(), true, kGURL, &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  interstitial->TestDidNavigate(1, kGURL);

  // Initiate a new navigation from the browser that also triggers an
  // interstitial.
  contents()->GetController().LoadURL(
      kGURL, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  TestInterstitialPage::InterstitialState state2 =
      TestInterstitialPage::INVALID;
  bool deleted2 = false;
  TestInterstitialPage* interstitial2 =
      new TestInterstitialPage(contents(), true, kGURL, &state2, &deleted2);
  TestInterstitialPageStateGuard state_guard2(interstitial2);
  interstitial2->Show();
  interstitial2->TestDidNavigate(1, kGURL);

  // Make sure we still have an entry.
  NavigationEntry* entry = contents()->GetController().GetPendingEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(kUrl, entry->GetURL().spec());

  // And that the first interstitial is gone, but not the second.
  EXPECT_EQ(TestInterstitialPage::CANCELED, state);
  EXPECT_EQ(TestInterstitialPage::UNDECIDED, state2);
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted);
  EXPECT_FALSE(deleted2);
}

// Tests that Javascript messages are not shown while an interstitial is
// showing.
TEST_F(WebContentsImplTest, NoJSMessageOnInterstitials) {
  const char kUrl[] = "http://www.badguys.com/";
  const GURL kGURL(kUrl);

  // Start a navigation to a page
  contents()->GetController().LoadURL(
      kGURL, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  // DidNavigate from the page
  contents()->TestDidNavigate(rvh(), 1, kGURL, PAGE_TRANSITION_TYPED);

  // Simulate showing an interstitial while the page is showing.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::INVALID;
  bool deleted = false;
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents(), true, kGURL, &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  interstitial->TestDidNavigate(1, kGURL);

  // While the interstitial is showing, let's simulate the hidden page
  // attempting to show a JS message.
  IPC::Message* dummy_message = new IPC::Message;
  contents()->RunJavaScriptMessage(contents()->GetMainFrame(),
      base::ASCIIToUTF16("This is an informative message"),
      base::ASCIIToUTF16("OK"),
      kGURL, JAVASCRIPT_MESSAGE_TYPE_ALERT, dummy_message);
  EXPECT_TRUE(contents()->last_dialog_suppressed_);
}

// Makes sure that if the source passed to CopyStateFromAndPrune has an
// interstitial it isn't copied over to the destination.
TEST_F(WebContentsImplTest, CopyStateFromAndPruneSourceInterstitial) {
  // Navigate to a page.
  GURL url1("http://www.google.com");
  test_rvh()->SendNavigate(1, url1);
  EXPECT_EQ(1, controller().GetEntryCount());

  // Initiate a browser navigation that will trigger the interstitial
  controller().LoadURL(GURL("http://www.evil.com"), Referrer(),
                        PAGE_TRANSITION_TYPED, std::string());

  // Show an interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::INVALID;
  bool deleted = false;
  GURL url2("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents(), true, url2, &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  interstitial->TestDidNavigate(1, url2);
  EXPECT_TRUE(interstitial->is_showing());
  EXPECT_EQ(2, controller().GetEntryCount());

  // Create another NavigationController.
  GURL url3("http://foo2");
  scoped_ptr<TestWebContents> other_contents(
      static_cast<TestWebContents*>(CreateTestWebContents()));
  NavigationControllerImpl& other_controller = other_contents->GetController();
  other_contents->NavigateAndCommit(url3);
  other_contents->ExpectSetHistoryLengthAndPrune(
      NavigationEntryImpl::FromNavigationEntry(
          other_controller.GetEntryAtIndex(0))->site_instance(), 1,
      other_controller.GetEntryAtIndex(0)->GetPageID());
  other_controller.CopyStateFromAndPrune(&controller(), false);

  // The merged controller should only have two entries: url1 and url2.
  ASSERT_EQ(2, other_controller.GetEntryCount());
  EXPECT_EQ(1, other_controller.GetCurrentEntryIndex());
  EXPECT_EQ(url1, other_controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(url3, other_controller.GetEntryAtIndex(1)->GetURL());

  // And the merged controller shouldn't be showing an interstitial.
  EXPECT_FALSE(other_contents->ShowingInterstitialPage());
}

// Makes sure that CopyStateFromAndPrune cannot be called if the target is
// showing an interstitial.
TEST_F(WebContentsImplTest, CopyStateFromAndPruneTargetInterstitial) {
  // Navigate to a page.
  GURL url1("http://www.google.com");
  contents()->NavigateAndCommit(url1);

  // Create another NavigationController.
  scoped_ptr<TestWebContents> other_contents(
      static_cast<TestWebContents*>(CreateTestWebContents()));
  NavigationControllerImpl& other_controller = other_contents->GetController();

  // Navigate it to url2.
  GURL url2("http://foo2");
  other_contents->NavigateAndCommit(url2);

  // Show an interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::INVALID;
  bool deleted = false;
  GURL url3("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(other_contents.get(), true, url3, &state,
                               &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  interstitial->TestDidNavigate(1, url3);
  EXPECT_TRUE(interstitial->is_showing());
  EXPECT_EQ(2, other_controller.GetEntryCount());

  // Ensure that we do not allow calling CopyStateFromAndPrune when an
  // interstitial is showing in the target.
  EXPECT_FALSE(other_controller.CanPruneAllButLastCommitted());
}

// Regression test for http://crbug.com/168611 - the URLs passed by the
// DidFinishLoad and DidFailLoadWithError IPCs should get filtered.
TEST_F(WebContentsImplTest, FilterURLs) {
  TestWebContentsObserver observer(contents());

  // A navigation to about:whatever should always look like a navigation to
  // about:blank
  GURL url_normalized(url::kAboutBlankURL);
  GURL url_from_ipc("about:whatever");

  // We navigate the test WebContents to about:blank, since NavigateAndCommit
  // will use the given URL to create the NavigationEntry as well, and that
  // entry should contain the filtered URL.
  contents()->NavigateAndCommit(url_normalized);

  // Check that an IPC with about:whatever is correctly normalized.
  contents()->TestDidFinishLoad(url_from_ipc);

  EXPECT_EQ(url_normalized, observer.last_url());

  // Create and navigate another WebContents.
  scoped_ptr<TestWebContents> other_contents(
      static_cast<TestWebContents*>(CreateTestWebContents()));
  TestWebContentsObserver other_observer(other_contents.get());
  other_contents->NavigateAndCommit(url_normalized);

  // Check that an IPC with about:whatever is correctly normalized.
  other_contents->TestDidFailLoadWithError(
      url_from_ipc, 1, base::string16());
  EXPECT_EQ(url_normalized, other_observer.last_url());
}

// Test that if a pending contents is deleted before it is shown, we don't
// crash.
TEST_F(WebContentsImplTest, PendingContents) {
  scoped_ptr<TestWebContents> other_contents(
      static_cast<TestWebContents*>(CreateTestWebContents()));
  contents()->AddPendingContents(other_contents.get());
  int route_id = other_contents->GetRenderViewHost()->GetRoutingID();
  other_contents.reset();
  EXPECT_EQ(NULL, contents()->GetCreatedWindow(route_id));
}

TEST_F(WebContentsImplTest, CapturerOverridesPreferredSize) {
  const gfx::Size original_preferred_size(1024, 768);
  contents()->UpdatePreferredSize(original_preferred_size);

  // With no capturers, expect the preferred size to be the one propagated into
  // WebContentsImpl via the RenderViewHostDelegate interface.
  EXPECT_EQ(contents()->GetCapturerCount(), 0);
  EXPECT_EQ(original_preferred_size, contents()->GetPreferredSize());

  // Increment capturer count, but without specifying a capture size.  Expect
  // a "not set" preferred size.
  contents()->IncrementCapturerCount(gfx::Size());
  EXPECT_EQ(1, contents()->GetCapturerCount());
  EXPECT_EQ(gfx::Size(), contents()->GetPreferredSize());

  // Increment capturer count again, but with an overriding capture size.
  // Expect preferred size to now be overridden to the capture size.
  const gfx::Size capture_size(1280, 720);
  contents()->IncrementCapturerCount(capture_size);
  EXPECT_EQ(2, contents()->GetCapturerCount());
  EXPECT_EQ(capture_size, contents()->GetPreferredSize());

  // Increment capturer count a third time, but the expect that the preferred
  // size is still the first capture size.
  const gfx::Size another_capture_size(720, 480);
  contents()->IncrementCapturerCount(another_capture_size);
  EXPECT_EQ(3, contents()->GetCapturerCount());
  EXPECT_EQ(capture_size, contents()->GetPreferredSize());

  // Decrement capturer count twice, but expect the preferred size to still be
  // overridden.
  contents()->DecrementCapturerCount();
  contents()->DecrementCapturerCount();
  EXPECT_EQ(1, contents()->GetCapturerCount());
  EXPECT_EQ(capture_size, contents()->GetPreferredSize());

  // Decrement capturer count, and since the count has dropped to zero, the
  // original preferred size should be restored.
  contents()->DecrementCapturerCount();
  EXPECT_EQ(0, contents()->GetCapturerCount());
  EXPECT_EQ(original_preferred_size, contents()->GetPreferredSize());
}

// Tests that GetLastActiveTime starts with a real, non-zero time and updates
// on activity.
TEST_F(WebContentsImplTest, GetLastActiveTime) {
  // The WebContents starts with a valid creation time.
  EXPECT_FALSE(contents()->GetLastActiveTime().is_null());

  // Reset the last active time to a known-bad value.
  contents()->last_active_time_ = base::TimeTicks();
  ASSERT_TRUE(contents()->GetLastActiveTime().is_null());

  // Simulate activating the WebContents. The active time should update.
  contents()->WasShown();
  EXPECT_FALSE(contents()->GetLastActiveTime().is_null());
}

class ContentsZoomChangedDelegate : public WebContentsDelegate {
 public:
  ContentsZoomChangedDelegate() :
    contents_zoom_changed_call_count_(0),
    last_zoom_in_(false) {
  }

  int GetAndResetContentsZoomChangedCallCount() {
    int count = contents_zoom_changed_call_count_;
    contents_zoom_changed_call_count_ = 0;
    return count;
  }

  bool last_zoom_in() const {
    return last_zoom_in_;
  }

  // WebContentsDelegate:
  virtual void ContentsZoomChange(bool zoom_in) OVERRIDE {
    contents_zoom_changed_call_count_++;
    last_zoom_in_ = zoom_in;
  }

 private:
  int contents_zoom_changed_call_count_;
  bool last_zoom_in_;

  DISALLOW_COPY_AND_ASSIGN(ContentsZoomChangedDelegate);
};

// Tests that some mouseehweel events get turned into browser zoom requests.
TEST_F(WebContentsImplTest, HandleWheelEvent) {
  using blink::WebInputEvent;

  scoped_ptr<ContentsZoomChangedDelegate> delegate(
      new ContentsZoomChangedDelegate());
  contents()->SetDelegate(delegate.get());

  int modifiers = 0;
  // Verify that normal mouse wheel events do nothing to change the zoom level.
  blink::WebMouseWheelEvent event =
      SyntheticWebMouseWheelEventBuilder::Build(0, 1, modifiers, false);
  EXPECT_FALSE(contents()->HandleWheelEvent(event));
  EXPECT_EQ(0, delegate->GetAndResetContentsZoomChangedCallCount());

  modifiers = WebInputEvent::ShiftKey | WebInputEvent::AltKey;
  event = SyntheticWebMouseWheelEventBuilder::Build(0, 1, modifiers, false);
  EXPECT_FALSE(contents()->HandleWheelEvent(event));
  EXPECT_EQ(0, delegate->GetAndResetContentsZoomChangedCallCount());

  // But whenever the ctrl modifier is applied, they can increase/decrease zoom.
  // Except on MacOS where we never want to adjust zoom with mousewheel.
  modifiers = WebInputEvent::ControlKey;
  event = SyntheticWebMouseWheelEventBuilder::Build(0, 1, modifiers, false);
  bool handled = contents()->HandleWheelEvent(event);
#if defined(OS_MACOSX)
  EXPECT_FALSE(handled);
  EXPECT_EQ(0, delegate->GetAndResetContentsZoomChangedCallCount());
#else
  EXPECT_TRUE(handled);
  EXPECT_EQ(1, delegate->GetAndResetContentsZoomChangedCallCount());
  EXPECT_TRUE(delegate->last_zoom_in());
#endif

  modifiers = WebInputEvent::ControlKey | WebInputEvent::ShiftKey |
      WebInputEvent::AltKey;
  event = SyntheticWebMouseWheelEventBuilder::Build(2, -5, modifiers, false);
  handled = contents()->HandleWheelEvent(event);
#if defined(OS_MACOSX)
  EXPECT_FALSE(handled);
  EXPECT_EQ(0, delegate->GetAndResetContentsZoomChangedCallCount());
#else
  EXPECT_TRUE(handled);
  EXPECT_EQ(1, delegate->GetAndResetContentsZoomChangedCallCount());
  EXPECT_FALSE(delegate->last_zoom_in());
#endif

  // Unless there is no vertical movement.
  event = SyntheticWebMouseWheelEventBuilder::Build(2, 0, modifiers, false);
  EXPECT_FALSE(contents()->HandleWheelEvent(event));
  EXPECT_EQ(0, delegate->GetAndResetContentsZoomChangedCallCount());

  // Events containing precise scrolling deltas also shouldn't result in the
  // zoom being adjusted, to avoid accidental adjustments caused by
  // two-finger-scrolling on a touchpad.
  modifiers = WebInputEvent::ControlKey;
  event = SyntheticWebMouseWheelEventBuilder::Build(0, 5, modifiers, true);
  EXPECT_FALSE(contents()->HandleWheelEvent(event));
  EXPECT_EQ(0, delegate->GetAndResetContentsZoomChangedCallCount());

  // Ensure pointers to the delegate aren't kept beyond its lifetime.
  contents()->SetDelegate(NULL);
}

// Tests that trackpad GesturePinchUpdate events get turned into browser zoom.
TEST_F(WebContentsImplTest, HandleGestureEvent) {
  using blink::WebGestureEvent;
  using blink::WebInputEvent;

  scoped_ptr<ContentsZoomChangedDelegate> delegate(
      new ContentsZoomChangedDelegate());
  contents()->SetDelegate(delegate.get());

  const float kZoomStepValue = 0.6f;
  blink::WebGestureEvent event = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::GesturePinchUpdate, blink::WebGestureDeviceTouchpad);

  // A pinch less than the step value doesn't change the zoom level.
  event.data.pinchUpdate.scale = 1.0f + kZoomStepValue * 0.8f;
  EXPECT_TRUE(contents()->HandleGestureEvent(event));
  EXPECT_EQ(0, delegate->GetAndResetContentsZoomChangedCallCount());

  // But repeating the event so the combined scale is greater does.
  EXPECT_TRUE(contents()->HandleGestureEvent(event));
  EXPECT_EQ(1, delegate->GetAndResetContentsZoomChangedCallCount());
  EXPECT_TRUE(delegate->last_zoom_in());

  // Pinching back out one step goes back to 100%.
  event.data.pinchUpdate.scale = 1.0f - kZoomStepValue;
  EXPECT_TRUE(contents()->HandleGestureEvent(event));
  EXPECT_EQ(1, delegate->GetAndResetContentsZoomChangedCallCount());
  EXPECT_FALSE(delegate->last_zoom_in());

  // Pinching out again doesn't zoom (step is twice as large around 100%).
  EXPECT_TRUE(contents()->HandleGestureEvent(event));
  EXPECT_EQ(0, delegate->GetAndResetContentsZoomChangedCallCount());

  // And again now it zooms once per step.
  EXPECT_TRUE(contents()->HandleGestureEvent(event));
  EXPECT_EQ(1, delegate->GetAndResetContentsZoomChangedCallCount());
  EXPECT_FALSE(delegate->last_zoom_in());

  // No other type of gesture event is handled by WebContentsImpl (for example
  // a touchscreen pinch gesture).
  event = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::GesturePinchUpdate, blink::WebGestureDeviceTouchscreen);
  event.data.pinchUpdate.scale = 1.0f + kZoomStepValue * 3;
  EXPECT_FALSE(contents()->HandleGestureEvent(event));
  EXPECT_EQ(0, delegate->GetAndResetContentsZoomChangedCallCount());

  // Ensure pointers to the delegate aren't kept beyond it's lifetime.
  contents()->SetDelegate(NULL);
}

// Tests that GetRelatedActiveContentsCount is shared between related
// SiteInstances and includes WebContents that have not navigated yet.
TEST_F(WebContentsImplTest, ActiveContentsCountBasic) {
  scoped_refptr<SiteInstance> instance1(
      SiteInstance::CreateForURL(browser_context(), GURL("http://a.com")));
  scoped_refptr<SiteInstance> instance2(
      instance1->GetRelatedSiteInstance(GURL("http://b.com")));

  EXPECT_EQ(0u, instance1->GetRelatedActiveContentsCount());
  EXPECT_EQ(0u, instance2->GetRelatedActiveContentsCount());

  scoped_ptr<TestWebContents> contents1(
      TestWebContents::Create(browser_context(), instance1));
  EXPECT_EQ(1u, instance1->GetRelatedActiveContentsCount());
  EXPECT_EQ(1u, instance2->GetRelatedActiveContentsCount());

  scoped_ptr<TestWebContents> contents2(
      TestWebContents::Create(browser_context(), instance1));
  EXPECT_EQ(2u, instance1->GetRelatedActiveContentsCount());
  EXPECT_EQ(2u, instance2->GetRelatedActiveContentsCount());

  contents1.reset();
  EXPECT_EQ(1u, instance1->GetRelatedActiveContentsCount());
  EXPECT_EQ(1u, instance2->GetRelatedActiveContentsCount());

  contents2.reset();
  EXPECT_EQ(0u, instance1->GetRelatedActiveContentsCount());
  EXPECT_EQ(0u, instance2->GetRelatedActiveContentsCount());
}

// Tests that GetRelatedActiveContentsCount is preserved correctly across
// same-site and cross-site navigations.
TEST_F(WebContentsImplTest, ActiveContentsCountNavigate) {
  scoped_refptr<SiteInstance> instance(
      SiteInstance::Create(browser_context()));

  EXPECT_EQ(0u, instance->GetRelatedActiveContentsCount());

  scoped_ptr<TestWebContents> contents(
      TestWebContents::Create(browser_context(), instance));
  EXPECT_EQ(1u, instance->GetRelatedActiveContentsCount());

  // Navigate to a URL.
  contents->GetController().LoadURL(
      GURL("http://a.com/1"), Referrer(), PAGE_TRANSITION_TYPED, std::string());
  EXPECT_EQ(1u, instance->GetRelatedActiveContentsCount());
  contents->CommitPendingNavigation();
  EXPECT_EQ(1u, instance->GetRelatedActiveContentsCount());

  // Navigate to a URL in the same site.
  contents->GetController().LoadURL(
      GURL("http://a.com/2"), Referrer(), PAGE_TRANSITION_TYPED, std::string());
  EXPECT_EQ(1u, instance->GetRelatedActiveContentsCount());
  contents->CommitPendingNavigation();
  EXPECT_EQ(1u, instance->GetRelatedActiveContentsCount());

  // Navigate to a URL in a different site.
  contents->GetController().LoadURL(
      GURL("http://b.com"), Referrer(), PAGE_TRANSITION_TYPED, std::string());
  EXPECT_TRUE(contents->cross_navigation_pending());
  EXPECT_EQ(1u, instance->GetRelatedActiveContentsCount());
  contents->CommitPendingNavigation();
  EXPECT_EQ(1u, instance->GetRelatedActiveContentsCount());

  contents.reset();
  EXPECT_EQ(0u, instance->GetRelatedActiveContentsCount());
}

// Tests that GetRelatedActiveContentsCount tracks BrowsingInstance changes
// from WebUI.
TEST_F(WebContentsImplTest, ActiveContentsCountChangeBrowsingInstance) {
  scoped_refptr<SiteInstance> instance(
      SiteInstance::Create(browser_context()));

  EXPECT_EQ(0u, instance->GetRelatedActiveContentsCount());

  scoped_ptr<TestWebContents> contents(
      TestWebContents::Create(browser_context(), instance));
  EXPECT_EQ(1u, instance->GetRelatedActiveContentsCount());

  // Navigate to a URL.
  contents->NavigateAndCommit(GURL("http://a.com"));
  EXPECT_EQ(1u, instance->GetRelatedActiveContentsCount());

  // Navigate to a URL with WebUI. This will change BrowsingInstances.
  contents->GetController().LoadURL(
      GURL(kTestWebUIUrl), Referrer(), PAGE_TRANSITION_TYPED, std::string());
  EXPECT_TRUE(contents->cross_navigation_pending());
  scoped_refptr<SiteInstance> instance_webui(
      contents->GetPendingRenderViewHost()->GetSiteInstance());
  EXPECT_FALSE(instance->IsRelatedSiteInstance(instance_webui.get()));

  // At this point, contents still counts for the old BrowsingInstance.
  EXPECT_EQ(1u, instance->GetRelatedActiveContentsCount());
  EXPECT_EQ(0u, instance_webui->GetRelatedActiveContentsCount());

  // Commit and contents counts for the new one.
  contents->CommitPendingNavigation();
  EXPECT_EQ(0u, instance->GetRelatedActiveContentsCount());
  EXPECT_EQ(1u, instance_webui->GetRelatedActiveContentsCount());

  contents.reset();
  EXPECT_EQ(0u, instance->GetRelatedActiveContentsCount());
  EXPECT_EQ(0u, instance_webui->GetRelatedActiveContentsCount());
}

}  // namespace content
