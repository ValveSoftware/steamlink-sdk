// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/interstitial_page_impl.h"

#include <vector>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread.h"
#include "content/browser/dom_storage/dom_storage_context_wrapper.h"
#include "content/browser/dom_storage/session_storage_namespace_impl.h"
#include "content/browser/frame_host/interstitial_page_navigator_impl.h"
#include "content/browser/frame_host/navigation_controller_impl.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_delegate_view.h"
#include "content/browser/renderer_host/render_view_host_factory.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view.h"
#include "content/common/frame_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/dom_operation_notification_details.h"
#include "content/public/browser/interstitial_page_delegate.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/page_transition_types.h"
#include "net/base/escape.h"
#include "net/url_request/url_request_context_getter.h"

using blink::WebDragOperation;
using blink::WebDragOperationsMask;

namespace content {
namespace {

void ResourceRequestHelper(ResourceDispatcherHostImpl* rdh,
                           int process_id,
                           int render_view_host_id,
                           ResourceRequestAction action) {
  switch (action) {
    case BLOCK:
      rdh->BlockRequestsForRoute(process_id, render_view_host_id);
      break;
    case RESUME:
      rdh->ResumeBlockedRequestsForRoute(process_id, render_view_host_id);
      break;
    case CANCEL:
      rdh->CancelBlockedRequestsForRoute(process_id, render_view_host_id);
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace

class InterstitialPageImpl::InterstitialPageRVHDelegateView
  : public RenderViewHostDelegateView {
 public:
  explicit InterstitialPageRVHDelegateView(InterstitialPageImpl* page);

  // RenderViewHostDelegateView implementation:
#if defined(OS_MACOSX) || defined(OS_ANDROID)
  virtual void ShowPopupMenu(const gfx::Rect& bounds,
                             int item_height,
                             double item_font_size,
                             int selected_item,
                             const std::vector<MenuItem>& items,
                             bool right_aligned,
                             bool allow_multiple_selection) OVERRIDE;
  virtual void HidePopupMenu() OVERRIDE;
#endif
  virtual void StartDragging(const DropData& drop_data,
                             WebDragOperationsMask operations_allowed,
                             const gfx::ImageSkia& image,
                             const gfx::Vector2d& image_offset,
                             const DragEventSourceInfo& event_info) OVERRIDE;
  virtual void UpdateDragCursor(WebDragOperation operation) OVERRIDE;
  virtual void GotFocus() OVERRIDE;
  virtual void TakeFocus(bool reverse) OVERRIDE;
  virtual void OnFindReply(int request_id,
                           int number_of_matches,
                           const gfx::Rect& selection_rect,
                           int active_match_ordinal,
                           bool final_update);

 private:
  InterstitialPageImpl* interstitial_page_;

  DISALLOW_COPY_AND_ASSIGN(InterstitialPageRVHDelegateView);
};


// We keep a map of the various blocking pages shown as the UI tests need to
// be able to retrieve them.
typedef std::map<WebContents*, InterstitialPageImpl*> InterstitialPageMap;
static InterstitialPageMap* g_web_contents_to_interstitial_page;

// Initializes g_web_contents_to_interstitial_page in a thread-safe manner.
// Should be called before accessing g_web_contents_to_interstitial_page.
static void InitInterstitialPageMap() {
  if (!g_web_contents_to_interstitial_page)
    g_web_contents_to_interstitial_page = new InterstitialPageMap;
}

InterstitialPage* InterstitialPage::Create(WebContents* web_contents,
                                           bool new_navigation,
                                           const GURL& url,
                                           InterstitialPageDelegate* delegate) {
  return new InterstitialPageImpl(
      web_contents,
      static_cast<RenderWidgetHostDelegate*>(
          static_cast<WebContentsImpl*>(web_contents)),
      new_navigation, url, delegate);
}

InterstitialPage* InterstitialPage::GetInterstitialPage(
    WebContents* web_contents) {
  InitInterstitialPageMap();
  InterstitialPageMap::const_iterator iter =
      g_web_contents_to_interstitial_page->find(web_contents);
  if (iter == g_web_contents_to_interstitial_page->end())
    return NULL;

  return iter->second;
}

InterstitialPageImpl::InterstitialPageImpl(
    WebContents* web_contents,
    RenderWidgetHostDelegate* render_widget_host_delegate,
    bool new_navigation,
    const GURL& url,
    InterstitialPageDelegate* delegate)
    : WebContentsObserver(web_contents),
      web_contents_(web_contents),
      controller_(static_cast<NavigationControllerImpl*>(
          &web_contents->GetController())),
      render_widget_host_delegate_(render_widget_host_delegate),
      url_(url),
      new_navigation_(new_navigation),
      should_discard_pending_nav_entry_(new_navigation),
      reload_on_dont_proceed_(false),
      enabled_(true),
      action_taken_(NO_ACTION),
      render_view_host_(NULL),
      // TODO(nasko): The InterstitialPageImpl will need to provide its own
      // NavigationControllerImpl to the Navigator, which is separate from
      // the WebContents one, so we can enforce no navigation policy here.
      // While we get the code to a point to do this, pass NULL for it.
      // TODO(creis): We will also need to pass delegates for the RVHM as we
      // start to use it.
      frame_tree_(new InterstitialPageNavigatorImpl(this, controller_),
                  this, this, this,
                  static_cast<WebContentsImpl*>(web_contents)),
      original_child_id_(web_contents->GetRenderProcessHost()->GetID()),
      original_rvh_id_(web_contents->GetRenderViewHost()->GetRoutingID()),
      should_revert_web_contents_title_(false),
      web_contents_was_loading_(false),
      resource_dispatcher_host_notified_(false),
      rvh_delegate_view_(new InterstitialPageRVHDelegateView(this)),
      create_view_(true),
      delegate_(delegate),
      weak_ptr_factory_(this) {
  InitInterstitialPageMap();
  // It would be inconsistent to create an interstitial with no new navigation
  // (which is the case when the interstitial was triggered by a sub-resource on
  // a page) when we have a pending entry (in the process of loading a new top
  // frame).
  DCHECK(new_navigation || !web_contents->GetController().GetPendingEntry());
}

InterstitialPageImpl::~InterstitialPageImpl() {
}

void InterstitialPageImpl::Show() {
  if (!enabled())
    return;

  // If an interstitial is already showing or about to be shown, close it before
  // showing the new one.
  // Be careful not to take an action on the old interstitial more than once.
  InterstitialPageMap::const_iterator iter =
      g_web_contents_to_interstitial_page->find(web_contents_);
  if (iter != g_web_contents_to_interstitial_page->end()) {
    InterstitialPageImpl* interstitial = iter->second;
    if (interstitial->action_taken_ != NO_ACTION) {
      interstitial->Hide();
    } else {
      // If we are currently showing an interstitial page for which we created
      // a transient entry and a new interstitial is shown as the result of a
      // new browser initiated navigation, then that transient entry has already
      // been discarded and a new pending navigation entry created.
      // So we should not discard that new pending navigation entry.
      // See http://crbug.com/9791
      if (new_navigation_ && interstitial->new_navigation_)
        interstitial->should_discard_pending_nav_entry_= false;
      interstitial->DontProceed();
    }
  }

  // Block the resource requests for the render view host while it is hidden.
  TakeActionOnResourceDispatcher(BLOCK);
  // We need to be notified when the RenderViewHost is destroyed so we can
  // cancel the blocked requests.  We cannot do that on
  // NOTIFY_WEB_CONTENTS_DESTROYED as at that point the RenderViewHost has
  // already been destroyed.
  notification_registrar_.Add(
      this, NOTIFICATION_RENDER_WIDGET_HOST_DESTROYED,
      Source<RenderWidgetHost>(controller_->delegate()->GetRenderViewHost()));

  // Update the g_web_contents_to_interstitial_page map.
  iter = g_web_contents_to_interstitial_page->find(web_contents_);
  DCHECK(iter == g_web_contents_to_interstitial_page->end());
  (*g_web_contents_to_interstitial_page)[web_contents_] = this;

  if (new_navigation_) {
    NavigationEntryImpl* entry = new NavigationEntryImpl;
    entry->SetURL(url_);
    entry->SetVirtualURL(url_);
    entry->set_page_type(PAGE_TYPE_INTERSTITIAL);

    // Give delegates a chance to set some states on the navigation entry.
    delegate_->OverrideEntry(entry);

    controller_->SetTransientEntry(entry);
  }

  DCHECK(!render_view_host_);
  render_view_host_ = static_cast<RenderViewHostImpl*>(CreateRenderViewHost());
  render_view_host_->AttachToFrameTree();
  CreateWebContentsView();

  std::string data_url = "data:text/html;charset=utf-8," +
                         net::EscapePath(delegate_->GetHTMLContents());
  render_view_host_->NavigateToURL(GURL(data_url));

  notification_registrar_.Add(this, NOTIFICATION_NAV_ENTRY_PENDING,
      Source<NavigationController>(controller_));
}

void InterstitialPageImpl::Hide() {
  // We may have already been hidden, and are just waiting to be deleted.
  // We can't check for enabled() here, because some callers have already
  // called Disable.
  if (!render_view_host_)
    return;

  Disable();

  RenderWidgetHostView* old_view =
      controller_->delegate()->GetRenderViewHost()->GetView();
  if (controller_->delegate()->GetInterstitialPage() == this &&
      old_view &&
      !old_view->IsShowing() &&
      !controller_->delegate()->IsHidden()) {
    // Show the original RVH since we're going away.  Note it might not exist if
    // the renderer crashed while the interstitial was showing.
    // Note that it is important that we don't call Show() if the view is
    // already showing. That would result in bad things (unparented HWND on
    // Windows for example) happening.
    old_view->Show();
  }

  // If the focus was on the interstitial, let's keep it to the page.
  // (Note that in unit-tests the RVH may not have a view).
  if (render_view_host_->GetView() &&
      render_view_host_->GetView()->HasFocus() &&
      controller_->delegate()->GetRenderViewHost()->GetView()) {
    controller_->delegate()->GetRenderViewHost()->GetView()->Focus();
  }

  // Delete this and call Shutdown on the RVH asynchronously, as we may have
  // been called from a RVH delegate method, and we can't delete the RVH out
  // from under itself.
  base::MessageLoop::current()->PostNonNestableTask(
      FROM_HERE,
      base::Bind(&InterstitialPageImpl::Shutdown,
                 weak_ptr_factory_.GetWeakPtr()));
  render_view_host_ = NULL;
  frame_tree_.ResetForMainFrameSwap();
  controller_->delegate()->DetachInterstitialPage();
  // Let's revert to the original title if necessary.
  NavigationEntry* entry = controller_->GetVisibleEntry();
  if (!new_navigation_ && should_revert_web_contents_title_) {
    entry->SetTitle(original_web_contents_title_);
    controller_->delegate()->NotifyNavigationStateChanged(
        INVALIDATE_TYPE_TITLE);
  }

  InterstitialPageMap::iterator iter =
      g_web_contents_to_interstitial_page->find(web_contents_);
  DCHECK(iter != g_web_contents_to_interstitial_page->end());
  if (iter != g_web_contents_to_interstitial_page->end())
    g_web_contents_to_interstitial_page->erase(iter);

  // Clear the WebContents pointer, because it may now be deleted.
  // This signifies that we are in the process of shutting down.
  web_contents_ = NULL;
}

void InterstitialPageImpl::Observe(
    int type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  switch (type) {
    case NOTIFICATION_NAV_ENTRY_PENDING:
      // We are navigating away from the interstitial (the user has typed a URL
      // in the location bar or clicked a bookmark).  Make sure clicking on the
      // interstitial will have no effect.  Also cancel any blocked requests
      // on the ResourceDispatcherHost.  Note that when we get this notification
      // the RenderViewHost has not yet navigated so we'll unblock the
      // RenderViewHost before the resource request for the new page we are
      // navigating arrives in the ResourceDispatcherHost.  This ensures that
      // request won't be blocked if the same RenderViewHost was used for the
      // new navigation.
      Disable();
      TakeActionOnResourceDispatcher(CANCEL);
      break;
    case NOTIFICATION_RENDER_WIDGET_HOST_DESTROYED:
      if (action_taken_ == NO_ACTION) {
        // The RenderViewHost is being destroyed (as part of the tab being
        // closed); make sure we clear the blocked requests.
        RenderViewHost* rvh = static_cast<RenderViewHost*>(
            static_cast<RenderViewHostImpl*>(
                RenderWidgetHostImpl::From(
                    Source<RenderWidgetHost>(source).ptr())));
        DCHECK(rvh->GetProcess()->GetID() == original_child_id_ &&
               rvh->GetRoutingID() == original_rvh_id_);
        TakeActionOnResourceDispatcher(CANCEL);
      }
      break;
    default:
      NOTREACHED();
  }
}

void InterstitialPageImpl::NavigationEntryCommitted(
    const LoadCommittedDetails& load_details) {
  OnNavigatingAwayOrTabClosing();
}

void InterstitialPageImpl::WebContentsDestroyed() {
  OnNavigatingAwayOrTabClosing();
}

bool InterstitialPageImpl::OnMessageReceived(
    const IPC::Message& message,
    RenderFrameHost* render_frame_host) {
  return OnMessageReceived(message);
}

bool InterstitialPageImpl::OnMessageReceived(RenderFrameHost* render_frame_host,
                                             const IPC::Message& message) {
  return OnMessageReceived(message);
}

bool InterstitialPageImpl::OnMessageReceived(RenderViewHost* render_view_host,
                                             const IPC::Message& message) {
  return OnMessageReceived(message);
}

bool InterstitialPageImpl::OnMessageReceived(const IPC::Message& message) {

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(InterstitialPageImpl, message)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DomOperationResponse,
                        OnDomOperationResponse)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void InterstitialPageImpl::RenderFrameCreated(
    RenderFrameHost* render_frame_host) {
  // Note this is only for subframes in the interstitial, the notification for
  // the main frame happens in RenderViewCreated.
  controller_->delegate()->RenderFrameForInterstitialPageCreated(
      render_frame_host);
}

void InterstitialPageImpl::UpdateTitle(
    RenderFrameHost* render_frame_host,
    int32 page_id,
    const base::string16& title,
    base::i18n::TextDirection title_direction) {
  if (!enabled())
    return;

  RenderViewHost* render_view_host = render_frame_host->GetRenderViewHost();
  DCHECK(render_view_host == render_view_host_);
  NavigationEntry* entry = controller_->GetVisibleEntry();
  if (!entry) {
    // Crash reports from the field indicate this can be NULL.
    // This is unexpected as InterstitialPages constructed with the
    // new_navigation flag set to true create a transient navigation entry
    // (that is returned as the active entry). And the only case so far of
    // interstitial created with that flag set to false is with the
    // SafeBrowsingBlockingPage, when the resource triggering the interstitial
    // is a sub-resource, meaning the main page has already been loaded and a
    // navigation entry should have been created.
    NOTREACHED();
    return;
  }

  // If this interstitial is shown on an existing navigation entry, we'll need
  // to remember its title so we can revert to it when hidden.
  if (!new_navigation_ && !should_revert_web_contents_title_) {
    original_web_contents_title_ = entry->GetTitle();
    should_revert_web_contents_title_ = true;
  }
  // TODO(evan): make use of title_direction.
  // http://code.google.com/p/chromium/issues/detail?id=27094
  entry->SetTitle(title);
  controller_->delegate()->NotifyNavigationStateChanged(INVALIDATE_TYPE_TITLE);
}

RenderViewHostDelegateView* InterstitialPageImpl::GetDelegateView() {
  return rvh_delegate_view_.get();
}

const GURL& InterstitialPageImpl::GetMainFrameLastCommittedURL() const {
  return url_;
}

void InterstitialPageImpl::RenderViewTerminated(
    RenderViewHost* render_view_host,
    base::TerminationStatus status,
    int error_code) {
  // Our renderer died. This should not happen in normal cases.
  // If we haven't already started shutdown, just dismiss the interstitial.
  // We cannot check for enabled() here, because we may have called Disable
  // without calling Hide.
  if (render_view_host_)
    DontProceed();
}

void InterstitialPageImpl::DidNavigate(
    RenderViewHost* render_view_host,
    const FrameHostMsg_DidCommitProvisionalLoad_Params& params) {
  // A fast user could have navigated away from the page that triggered the
  // interstitial while the interstitial was loading, that would have disabled
  // us. In that case we can dismiss ourselves.
  if (!enabled()) {
    DontProceed();
    return;
  }
  if (PageTransitionCoreTypeIs(params.transition,
                               PAGE_TRANSITION_AUTO_SUBFRAME)) {
    // No need to handle navigate message from iframe in the interstitial page.
    return;
  }

  // The RenderViewHost has loaded its contents, we can show it now.
  if (!controller_->delegate()->IsHidden())
    render_view_host_->GetView()->Show();
  controller_->delegate()->AttachInterstitialPage(this);

  RenderWidgetHostView* rwh_view =
      controller_->delegate()->GetRenderViewHost()->GetView();

  // The RenderViewHost may already have crashed before we even get here.
  if (rwh_view) {
    // If the page has focus, focus the interstitial.
    if (rwh_view->HasFocus())
      Focus();

    // Hide the original RVH since we're showing the interstitial instead.
    rwh_view->Hide();
  }

  // Notify the tab we are not loading so the throbber is stopped. It also
  // causes a WebContentsObserver::DidStopLoading callback that the
  // AutomationProvider (used by the UI tests) expects to consider a navigation
  // as complete. Without this, navigating in a UI test to a URL that triggers
  // an interstitial would hang.
  web_contents_was_loading_ = controller_->delegate()->IsLoading();
  controller_->delegate()->SetIsLoading(
      controller_->delegate()->GetRenderViewHost(), false, true, NULL);
}

RendererPreferences InterstitialPageImpl::GetRendererPrefs(
    BrowserContext* browser_context) const {
  delegate_->OverrideRendererPrefs(&renderer_preferences_);
  return renderer_preferences_;
}

WebPreferences InterstitialPageImpl::GetWebkitPrefs() {
  if (!enabled())
    return WebPreferences();

  return render_view_host_->GetWebkitPrefs(url_);
}

void InterstitialPageImpl::RenderWidgetDeleted(
    RenderWidgetHostImpl* render_widget_host) {
  // TODO(creis): Remove this method once we verify the shutdown path is sane.
  CHECK(!web_contents_);
}

bool InterstitialPageImpl::PreHandleKeyboardEvent(
    const NativeWebKeyboardEvent& event,
    bool* is_keyboard_shortcut) {
  if (!enabled())
    return false;
  return render_widget_host_delegate_->PreHandleKeyboardEvent(
      event, is_keyboard_shortcut);
}

void InterstitialPageImpl::HandleKeyboardEvent(
      const NativeWebKeyboardEvent& event) {
  if (enabled())
    render_widget_host_delegate_->HandleKeyboardEvent(event);
}

#if defined(OS_WIN)
gfx::NativeViewAccessible
InterstitialPageImpl::GetParentNativeViewAccessible() {
  return render_widget_host_delegate_->GetParentNativeViewAccessible();
}
#endif

WebContents* InterstitialPageImpl::web_contents() const {
  return web_contents_;
}

RenderViewHost* InterstitialPageImpl::CreateRenderViewHost() {
  if (!enabled())
    return NULL;

  // Interstitial pages don't want to share the session storage so we mint a
  // new one.
  BrowserContext* browser_context = web_contents()->GetBrowserContext();
  scoped_refptr<SiteInstance> site_instance =
      SiteInstance::Create(browser_context);
  DOMStorageContextWrapper* dom_storage_context =
      static_cast<DOMStorageContextWrapper*>(
          BrowserContext::GetStoragePartition(
              browser_context, site_instance.get())->GetDOMStorageContext());
  session_storage_namespace_ =
      new SessionStorageNamespaceImpl(dom_storage_context);

  // Use the RenderViewHost from our FrameTree.
  frame_tree_.root()->render_manager()->Init(
      browser_context, site_instance.get(), MSG_ROUTING_NONE, MSG_ROUTING_NONE);
  return frame_tree_.root()->current_frame_host()->render_view_host();
}

WebContentsView* InterstitialPageImpl::CreateWebContentsView() {
  if (!enabled() || !create_view_)
    return NULL;
  WebContentsView* wcv =
      static_cast<WebContentsImpl*>(web_contents())->GetView();
  RenderWidgetHostViewBase* view =
      wcv->CreateViewForWidget(render_view_host_);
  render_view_host_->SetView(view);
  render_view_host_->AllowBindings(BINDINGS_POLICY_DOM_AUTOMATION);

  int32 max_page_id = web_contents()->
      GetMaxPageIDForSiteInstance(render_view_host_->GetSiteInstance());
  render_view_host_->CreateRenderView(base::string16(),
                                      MSG_ROUTING_NONE,
                                      MSG_ROUTING_NONE,
                                      max_page_id,
                                      false);
  controller_->delegate()->RenderFrameForInterstitialPageCreated(
      frame_tree_.root()->current_frame_host());
  view->SetSize(web_contents()->GetContainerBounds().size());
  // Don't show the interstitial until we have navigated to it.
  view->Hide();
  return wcv;
}

void InterstitialPageImpl::Proceed() {
  // Don't repeat this if we are already shutting down.  We cannot check for
  // enabled() here, because we may have called Disable without calling Hide.
  if (!render_view_host_)
    return;

  if (action_taken_ != NO_ACTION) {
    NOTREACHED();
    return;
  }
  Disable();
  action_taken_ = PROCEED_ACTION;

  // Resumes the throbber, if applicable.
  if (web_contents_was_loading_)
    controller_->delegate()->SetIsLoading(
        controller_->delegate()->GetRenderViewHost(), true, true, NULL);

  // If this is a new navigation, the old page is going away, so we cancel any
  // blocked requests for it.  If it is not a new navigation, then it means the
  // interstitial was shown as a result of a resource loading in the page.
  // Since the user wants to proceed, we'll let any blocked request go through.
  if (new_navigation_)
    TakeActionOnResourceDispatcher(CANCEL);
  else
    TakeActionOnResourceDispatcher(RESUME);

  // No need to hide if we are a new navigation, we'll get hidden when the
  // navigation is committed.
  if (!new_navigation_) {
    Hide();
    delegate_->OnProceed();
    return;
  }

  delegate_->OnProceed();
}

void InterstitialPageImpl::DontProceed() {
  // Don't repeat this if we are already shutting down.  We cannot check for
  // enabled() here, because we may have called Disable without calling Hide.
  if (!render_view_host_)
    return;
  DCHECK(action_taken_ != DONT_PROCEED_ACTION);

  Disable();
  action_taken_ = DONT_PROCEED_ACTION;

  // If this is a new navigation, we are returning to the original page, so we
  // resume blocked requests for it.  If it is not a new navigation, then it
  // means the interstitial was shown as a result of a resource loading in the
  // page and we won't return to the original page, so we cancel blocked
  // requests in that case.
  if (new_navigation_)
    TakeActionOnResourceDispatcher(RESUME);
  else
    TakeActionOnResourceDispatcher(CANCEL);

  if (should_discard_pending_nav_entry_) {
    // Since no navigation happens we have to discard the transient entry
    // explicitely.  Note that by calling DiscardNonCommittedEntries() we also
    // discard the pending entry, which is what we want, since the navigation is
    // cancelled.
    controller_->DiscardNonCommittedEntries();
  }

  if (reload_on_dont_proceed_)
    controller_->Reload(true);

  Hide();
  delegate_->OnDontProceed();
}

void InterstitialPageImpl::CancelForNavigation() {
  // The user is trying to navigate away.  We should unblock the renderer and
  // disable the interstitial, but keep it visible until the navigation
  // completes.
  Disable();
  // If this interstitial was shown for a new navigation, allow any navigations
  // on the original page to resume (e.g., subresource requests, XHRs, etc).
  // Otherwise, cancel the pending, possibly dangerous navigations.
  if (new_navigation_)
    TakeActionOnResourceDispatcher(RESUME);
  else
    TakeActionOnResourceDispatcher(CANCEL);
}

void InterstitialPageImpl::SetSize(const gfx::Size& size) {
  if (!enabled())
    return;
#if !defined(OS_MACOSX)
  // When a tab is closed, we might be resized after our view was NULLed
  // (typically if there was an info-bar).
  if (render_view_host_->GetView())
    render_view_host_->GetView()->SetSize(size);
#else
  // TODO(port): Does Mac need to SetSize?
  NOTIMPLEMENTED();
#endif
}

void InterstitialPageImpl::Focus() {
  // Focus the native window.
  if (!enabled())
    return;
  render_view_host_->GetView()->Focus();
}

void InterstitialPageImpl::FocusThroughTabTraversal(bool reverse) {
  if (!enabled())
    return;
  render_view_host_->SetInitialFocus(reverse);
}

RenderWidgetHostView* InterstitialPageImpl::GetView() {
  return render_view_host_->GetView();
}

RenderViewHost* InterstitialPageImpl::GetRenderViewHostForTesting() const {
  return render_view_host_;
}

#if defined(OS_ANDROID)
RenderViewHost* InterstitialPageImpl::GetRenderViewHost() const {
  return render_view_host_;
}
#endif

InterstitialPageDelegate* InterstitialPageImpl::GetDelegateForTesting() {
  return delegate_.get();
}

void InterstitialPageImpl::DontCreateViewForTesting() {
  create_view_ = false;
}

gfx::Rect InterstitialPageImpl::GetRootWindowResizerRect() const {
  return gfx::Rect();
}

void InterstitialPageImpl::CreateNewWindow(
    int render_process_id,
    int route_id,
    int main_frame_route_id,
    const ViewHostMsg_CreateWindow_Params& params,
    SessionStorageNamespace* session_storage_namespace) {
  NOTREACHED() << "InterstitialPage does not support showing popups yet.";
}

void InterstitialPageImpl::CreateNewWidget(int render_process_id,
                                           int route_id,
                                           blink::WebPopupType popup_type) {
  NOTREACHED() << "InterstitialPage does not support showing drop-downs yet.";
}

void InterstitialPageImpl::CreateNewFullscreenWidget(int render_process_id,
                                                     int route_id) {
  NOTREACHED()
      << "InterstitialPage does not support showing full screen popups.";
}

void InterstitialPageImpl::ShowCreatedWindow(int route_id,
                                             WindowOpenDisposition disposition,
                                             const gfx::Rect& initial_pos,
                                             bool user_gesture) {
  NOTREACHED() << "InterstitialPage does not support showing popups yet.";
}

void InterstitialPageImpl::ShowCreatedWidget(int route_id,
                                             const gfx::Rect& initial_pos) {
  NOTREACHED() << "InterstitialPage does not support showing drop-downs yet.";
}

void InterstitialPageImpl::ShowCreatedFullscreenWidget(int route_id) {
  NOTREACHED()
      << "InterstitialPage does not support showing full screen popups.";
}

SessionStorageNamespace* InterstitialPageImpl::GetSessionStorageNamespace(
    SiteInstance* instance) {
  return session_storage_namespace_.get();
}

FrameTree* InterstitialPageImpl::GetFrameTree() {
  return &frame_tree_;
}

void InterstitialPageImpl::Disable() {
  enabled_ = false;
}

void InterstitialPageImpl::Shutdown() {
  delete this;
}

void InterstitialPageImpl::OnNavigatingAwayOrTabClosing() {
  if (action_taken_ == NO_ACTION) {
    // We are navigating away from the interstitial or closing a tab with an
    // interstitial.  Default to DontProceed(). We don't just call Hide as
    // subclasses will almost certainly override DontProceed to do some work
    // (ex: close pending connections).
    DontProceed();
  } else {
    // User decided to proceed and either the navigation was committed or
    // the tab was closed before that.
    Hide();
  }
}

void InterstitialPageImpl::TakeActionOnResourceDispatcher(
    ResourceRequestAction action) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI)) <<
      "TakeActionOnResourceDispatcher should be called on the main thread.";

  if (action == CANCEL || action == RESUME) {
    if (resource_dispatcher_host_notified_)
      return;
    resource_dispatcher_host_notified_ = true;
  }

  // The tab might not have a render_view_host if it was closed (in which case,
  // we have taken care of the blocked requests when processing
  // NOTIFY_RENDER_WIDGET_HOST_DESTROYED.
  // Also we need to test there is a ResourceDispatcherHostImpl, as when unit-
  // tests we don't have one.
  RenderViewHostImpl* rvh = RenderViewHostImpl::FromID(original_child_id_,
                                                       original_rvh_id_);
  if (!rvh || !ResourceDispatcherHostImpl::Get())
    return;

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(
          &ResourceRequestHelper,
          ResourceDispatcherHostImpl::Get(),
          original_child_id_,
          original_rvh_id_,
          action));
}

void InterstitialPageImpl::OnDomOperationResponse(
    const std::string& json_string,
    int automation_id) {
  // Needed by test code.
  DomOperationNotificationDetails details(json_string, automation_id);
  NotificationService::current()->Notify(
      NOTIFICATION_DOM_OPERATION_RESPONSE,
      Source<WebContents>(web_contents()),
      Details<DomOperationNotificationDetails>(&details));

  if (!enabled())
    return;
  delegate_->CommandReceived(details.json);
}


InterstitialPageImpl::InterstitialPageRVHDelegateView::
    InterstitialPageRVHDelegateView(InterstitialPageImpl* page)
    : interstitial_page_(page) {
}

#if defined(OS_MACOSX) || defined(OS_ANDROID)
void InterstitialPageImpl::InterstitialPageRVHDelegateView::ShowPopupMenu(
    const gfx::Rect& bounds,
    int item_height,
    double item_font_size,
    int selected_item,
    const std::vector<MenuItem>& items,
    bool right_aligned,
    bool allow_multiple_selection) {
  NOTREACHED() << "InterstitialPage does not support showing popup menus.";
}

void InterstitialPageImpl::InterstitialPageRVHDelegateView::HidePopupMenu() {
  NOTREACHED() << "InterstitialPage does not support showing popup menus.";
}
#endif

void InterstitialPageImpl::InterstitialPageRVHDelegateView::StartDragging(
    const DropData& drop_data,
    WebDragOperationsMask allowed_operations,
    const gfx::ImageSkia& image,
    const gfx::Vector2d& image_offset,
    const DragEventSourceInfo& event_info) {
  interstitial_page_->render_view_host_->DragSourceSystemDragEnded();
  DVLOG(1) << "InterstitialPage does not support dragging yet.";
}

void InterstitialPageImpl::InterstitialPageRVHDelegateView::UpdateDragCursor(
    WebDragOperation) {
  NOTREACHED() << "InterstitialPage does not support dragging yet.";
}

void InterstitialPageImpl::InterstitialPageRVHDelegateView::GotFocus() {
  WebContents* web_contents = interstitial_page_->web_contents();
  if (web_contents && web_contents->GetDelegate())
    web_contents->GetDelegate()->WebContentsFocused(web_contents);
}

void InterstitialPageImpl::InterstitialPageRVHDelegateView::TakeFocus(
    bool reverse) {
  if (!interstitial_page_->web_contents())
    return;
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(interstitial_page_->web_contents());
  if (!web_contents->GetDelegateView())
    return;

  web_contents->GetDelegateView()->TakeFocus(reverse);
}

void InterstitialPageImpl::InterstitialPageRVHDelegateView::OnFindReply(
    int request_id, int number_of_matches, const gfx::Rect& selection_rect,
    int active_match_ordinal, bool final_update) {
}

}  // namespace content
