// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/navigation_controller_impl.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"  // Temporary
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "cc/base/switches.h"
#include "content/browser/browser_url_handler_impl.h"
#include "content/browser/dom_storage/dom_storage_context_wrapper.h"
#include "content/browser/dom_storage/session_storage_namespace_impl.h"
#include "content/browser/frame_host/debug_urls.h"
#include "content/browser/frame_host/interstitial_page_impl.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/frame_host/navigation_entry_screenshot_manager.h"
#include "content/browser/renderer_host/render_view_host_impl.h"  // Temporary
#include "content/browser/site_instance_impl.h"
#include "content/common/frame_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_constants.h"
#include "net/base/escape.h"
#include "net/base/mime_util.h"
#include "net/base/net_util.h"
#include "skia/ext/platform_canvas.h"
#include "url/url_constants.h"

namespace content {
namespace {

const int kInvalidateAll = 0xFFFFFFFF;

// Invoked when entries have been pruned, or removed. For example, if the
// current entries are [google, digg, yahoo], with the current entry google,
// and the user types in cnet, then digg and yahoo are pruned.
void NotifyPrunedEntries(NavigationControllerImpl* nav_controller,
                         bool from_front,
                         int count) {
  PrunedDetails details;
  details.from_front = from_front;
  details.count = count;
  NotificationService::current()->Notify(
      NOTIFICATION_NAV_LIST_PRUNED,
      Source<NavigationController>(nav_controller),
      Details<PrunedDetails>(&details));
}

// Ensure the given NavigationEntry has a valid state, so that WebKit does not
// get confused if we navigate back to it.
//
// An empty state is treated as a new navigation by WebKit, which would mean
// losing the navigation entries and generating a new navigation entry after
// this one. We don't want that. To avoid this we create a valid state which
// WebKit will not treat as a new navigation.
void SetPageStateIfEmpty(NavigationEntryImpl* entry) {
  if (!entry->GetPageState().IsValid())
    entry->SetPageState(PageState::CreateFromURL(entry->GetURL()));
}

NavigationEntryImpl::RestoreType ControllerRestoreTypeToEntryType(
    NavigationController::RestoreType type) {
  switch (type) {
    case NavigationController::RESTORE_CURRENT_SESSION:
      return NavigationEntryImpl::RESTORE_CURRENT_SESSION;
    case NavigationController::RESTORE_LAST_SESSION_EXITED_CLEANLY:
      return NavigationEntryImpl::RESTORE_LAST_SESSION_EXITED_CLEANLY;
    case NavigationController::RESTORE_LAST_SESSION_CRASHED:
      return NavigationEntryImpl::RESTORE_LAST_SESSION_CRASHED;
  }
  NOTREACHED();
  return NavigationEntryImpl::RESTORE_CURRENT_SESSION;
}

// Configure all the NavigationEntries in entries for restore. This resets
// the transition type to reload and makes sure the content state isn't empty.
void ConfigureEntriesForRestore(
    std::vector<linked_ptr<NavigationEntryImpl> >* entries,
    NavigationController::RestoreType type) {
  for (size_t i = 0; i < entries->size(); ++i) {
    // Use a transition type of reload so that we don't incorrectly increase
    // the typed count.
    (*entries)[i]->SetTransitionType(PAGE_TRANSITION_RELOAD);
    (*entries)[i]->set_restore_type(ControllerRestoreTypeToEntryType(type));
    // NOTE(darin): This code is only needed for backwards compat.
    SetPageStateIfEmpty((*entries)[i].get());
  }
}

// See NavigationController::IsURLInPageNavigation for how this works and why.
bool AreURLsInPageNavigation(const GURL& existing_url,
                             const GURL& new_url,
                             bool renderer_says_in_page,
                             NavigationType navigation_type) {
  if (existing_url.GetOrigin() == new_url.GetOrigin())
    return renderer_says_in_page;

  if (!new_url.has_ref()) {
    // When going back from the ref URL to the non ref one the navigation type
    // is IN_PAGE.
    return navigation_type == NAVIGATION_TYPE_IN_PAGE;
  }

  url::Replacements<char> replacements;
  replacements.ClearRef();
  return existing_url.ReplaceComponents(replacements) ==
      new_url.ReplaceComponents(replacements);
}

// Determines whether or not we should be carrying over a user agent override
// between two NavigationEntries.
bool ShouldKeepOverride(const NavigationEntry* last_entry) {
  return last_entry && last_entry->GetIsOverridingUserAgent();
}

}  // namespace

// NavigationControllerImpl ----------------------------------------------------

const size_t kMaxEntryCountForTestingNotSet = -1;

// static
size_t NavigationControllerImpl::max_entry_count_for_testing_ =
    kMaxEntryCountForTestingNotSet;

// Should Reload check for post data? The default is true, but is set to false
// when testing.
static bool g_check_for_repost = true;

// static
NavigationEntry* NavigationController::CreateNavigationEntry(
      const GURL& url,
      const Referrer& referrer,
      PageTransition transition,
      bool is_renderer_initiated,
      const std::string& extra_headers,
      BrowserContext* browser_context) {
  // Allow the browser URL handler to rewrite the URL. This will, for example,
  // remove "view-source:" from the beginning of the URL to get the URL that
  // will actually be loaded. This real URL won't be shown to the user, just
  // used internally.
  GURL loaded_url(url);
  bool reverse_on_redirect = false;
  BrowserURLHandlerImpl::GetInstance()->RewriteURLIfNecessary(
      &loaded_url, browser_context, &reverse_on_redirect);

  NavigationEntryImpl* entry = new NavigationEntryImpl(
      NULL,  // The site instance for tabs is sent on navigation
             // (WebContents::GetSiteInstance).
      -1,
      loaded_url,
      referrer,
      base::string16(),
      transition,
      is_renderer_initiated);
  entry->SetVirtualURL(url);
  entry->set_user_typed_url(url);
  entry->set_update_virtual_url_with_url(reverse_on_redirect);
  entry->set_extra_headers(extra_headers);
  return entry;
}

// static
void NavigationController::DisablePromptOnRepost() {
  g_check_for_repost = false;
}

base::Time NavigationControllerImpl::TimeSmoother::GetSmoothedTime(
    base::Time t) {
  // If |t| is between the water marks, we're in a run of duplicates
  // or just getting out of it, so increase the high-water mark to get
  // a time that probably hasn't been used before and return it.
  if (low_water_mark_ <= t && t <= high_water_mark_) {
    high_water_mark_ += base::TimeDelta::FromMicroseconds(1);
    return high_water_mark_;
  }

  // Otherwise, we're clear of the last duplicate run, so reset the
  // water marks.
  low_water_mark_ = high_water_mark_ = t;
  return t;
}

NavigationControllerImpl::NavigationControllerImpl(
    NavigationControllerDelegate* delegate,
    BrowserContext* browser_context)
    : browser_context_(browser_context),
      pending_entry_(NULL),
      last_committed_entry_index_(-1),
      pending_entry_index_(-1),
      transient_entry_index_(-1),
      delegate_(delegate),
      max_restored_page_id_(-1),
      ssl_manager_(this),
      needs_reload_(false),
      is_initial_navigation_(true),
      in_navigate_to_pending_entry_(false),
      pending_reload_(NO_RELOAD),
      get_timestamp_callback_(base::Bind(&base::Time::Now)),
      screenshot_manager_(new NavigationEntryScreenshotManager(this)) {
  DCHECK(browser_context_);
}

NavigationControllerImpl::~NavigationControllerImpl() {
  DiscardNonCommittedEntriesInternal();
}

WebContents* NavigationControllerImpl::GetWebContents() const {
  return delegate_->GetWebContents();
}

BrowserContext* NavigationControllerImpl::GetBrowserContext() const {
  return browser_context_;
}

void NavigationControllerImpl::SetBrowserContext(
    BrowserContext* browser_context) {
  browser_context_ = browser_context;
}

void NavigationControllerImpl::Restore(
    int selected_navigation,
    RestoreType type,
    std::vector<NavigationEntry*>* entries) {
  // Verify that this controller is unused and that the input is valid.
  DCHECK(GetEntryCount() == 0 && !GetPendingEntry());
  DCHECK(selected_navigation >= 0 &&
         selected_navigation < static_cast<int>(entries->size()));

  needs_reload_ = true;
  for (size_t i = 0; i < entries->size(); ++i) {
    NavigationEntryImpl* entry =
        NavigationEntryImpl::FromNavigationEntry((*entries)[i]);
    entries_.push_back(linked_ptr<NavigationEntryImpl>(entry));
  }
  entries->clear();

  // And finish the restore.
  FinishRestore(selected_navigation, type);
}

void NavigationControllerImpl::Reload(bool check_for_repost) {
  ReloadInternal(check_for_repost, RELOAD);
}
void NavigationControllerImpl::ReloadIgnoringCache(bool check_for_repost) {
  ReloadInternal(check_for_repost, RELOAD_IGNORING_CACHE);
}
void NavigationControllerImpl::ReloadOriginalRequestURL(bool check_for_repost) {
  ReloadInternal(check_for_repost, RELOAD_ORIGINAL_REQUEST_URL);
}

void NavigationControllerImpl::ReloadInternal(bool check_for_repost,
                                              ReloadType reload_type) {
  if (transient_entry_index_ != -1) {
    // If an interstitial is showing, treat a reload as a navigation to the
    // transient entry's URL.
    NavigationEntryImpl* transient_entry =
        NavigationEntryImpl::FromNavigationEntry(GetTransientEntry());
    if (!transient_entry)
      return;
    LoadURL(transient_entry->GetURL(),
            Referrer(),
            PAGE_TRANSITION_RELOAD,
            transient_entry->extra_headers());
    return;
  }

  NavigationEntryImpl* entry = NULL;
  int current_index = -1;

  // If we are reloading the initial navigation, just use the current
  // pending entry.  Otherwise look up the current entry.
  if (IsInitialNavigation() && pending_entry_) {
    entry = pending_entry_;
    // The pending entry might be in entries_ (e.g., after a Clone), so we
    // should also update the current_index.
    current_index = pending_entry_index_;
  } else {
    DiscardNonCommittedEntriesInternal();
    current_index = GetCurrentEntryIndex();
    if (current_index != -1) {
      entry = NavigationEntryImpl::FromNavigationEntry(
          GetEntryAtIndex(current_index));
    }
  }

  // If we are no where, then we can't reload.  TODO(darin): We should add a
  // CanReload method.
  if (!entry)
    return;

  if (reload_type == NavigationControllerImpl::RELOAD_ORIGINAL_REQUEST_URL &&
      entry->GetOriginalRequestURL().is_valid() && !entry->GetHasPostData()) {
    // We may have been redirected when navigating to the current URL.
    // Use the URL the user originally intended to visit, if it's valid and if a
    // POST wasn't involved; the latter case avoids issues with sending data to
    // the wrong page.
    entry->SetURL(entry->GetOriginalRequestURL());
    entry->SetReferrer(Referrer());
  }

  if (g_check_for_repost && check_for_repost &&
      entry->GetHasPostData()) {
    // The user is asking to reload a page with POST data. Prompt to make sure
    // they really want to do this. If they do, the dialog will call us back
    // with check_for_repost = false.
    delegate_->NotifyBeforeFormRepostWarningShow();

    pending_reload_ = reload_type;
    delegate_->ActivateAndShowRepostFormWarningDialog();
  } else {
    if (!IsInitialNavigation())
      DiscardNonCommittedEntriesInternal();

    // If we are reloading an entry that no longer belongs to the current
    // site instance (for example, refreshing a page for just installed app),
    // the reload must happen in a new process.
    // The new entry must have a new page_id and site instance, so it behaves
    // as new navigation (which happens to clear forward history).
    // Tabs that are discarded due to low memory conditions may not have a site
    // instance, and should not be treated as a cross-site reload.
    SiteInstanceImpl* site_instance = entry->site_instance();
    // Permit reloading guests without further checks.
    bool is_isolated_guest = site_instance && site_instance->HasProcess() &&
        site_instance->GetProcess()->IsIsolatedGuest();
    if (!is_isolated_guest && site_instance &&
        site_instance->HasWrongProcessForURL(entry->GetURL())) {
      // Create a navigation entry that resembles the current one, but do not
      // copy page id, site instance, content state, or timestamp.
      NavigationEntryImpl* nav_entry = NavigationEntryImpl::FromNavigationEntry(
          CreateNavigationEntry(
              entry->GetURL(), entry->GetReferrer(), entry->GetTransitionType(),
              false, entry->extra_headers(), browser_context_));

      // Mark the reload type as NO_RELOAD, so navigation will not be considered
      // a reload in the renderer.
      reload_type = NavigationController::NO_RELOAD;

      nav_entry->set_should_replace_entry(true);
      pending_entry_ = nav_entry;
    } else {
      pending_entry_ = entry;
      pending_entry_index_ = current_index;

      // The title of the page being reloaded might have been removed in the
      // meanwhile, so we need to revert to the default title upon reload and
      // invalidate the previously cached title (SetTitle will do both).
      // See Chromium issue 96041.
      pending_entry_->SetTitle(base::string16());

      pending_entry_->SetTransitionType(PAGE_TRANSITION_RELOAD);
    }

    NavigateToPendingEntry(reload_type);
  }
}

void NavigationControllerImpl::CancelPendingReload() {
  DCHECK(pending_reload_ != NO_RELOAD);
  pending_reload_ = NO_RELOAD;
}

void NavigationControllerImpl::ContinuePendingReload() {
  if (pending_reload_ == NO_RELOAD) {
    NOTREACHED();
  } else {
    ReloadInternal(false, pending_reload_);
    pending_reload_ = NO_RELOAD;
  }
}

bool NavigationControllerImpl::IsInitialNavigation() const {
  return is_initial_navigation_;
}

NavigationEntryImpl* NavigationControllerImpl::GetEntryWithPageID(
  SiteInstance* instance, int32 page_id) const {
  int index = GetEntryIndexWithPageID(instance, page_id);
  return (index != -1) ? entries_[index].get() : NULL;
}

void NavigationControllerImpl::LoadEntry(NavigationEntryImpl* entry) {
  // When navigating to a new page, we don't know for sure if we will actually
  // end up leaving the current page.  The new page load could for example
  // result in a download or a 'no content' response (e.g., a mailto: URL).
  SetPendingEntry(entry);
  NavigateToPendingEntry(NO_RELOAD);
}

void NavigationControllerImpl::SetPendingEntry(NavigationEntryImpl* entry) {
  DiscardNonCommittedEntriesInternal();
  pending_entry_ = entry;
  NotificationService::current()->Notify(
      NOTIFICATION_NAV_ENTRY_PENDING,
      Source<NavigationController>(this),
      Details<NavigationEntry>(entry));
}

NavigationEntry* NavigationControllerImpl::GetActiveEntry() const {
  if (transient_entry_index_ != -1)
    return entries_[transient_entry_index_].get();
  if (pending_entry_)
    return pending_entry_;
  return GetLastCommittedEntry();
}

NavigationEntry* NavigationControllerImpl::GetVisibleEntry() const {
  if (transient_entry_index_ != -1)
    return entries_[transient_entry_index_].get();
  // The pending entry is safe to return for new (non-history), browser-
  // initiated navigations.  Most renderer-initiated navigations should not
  // show the pending entry, to prevent URL spoof attacks.
  //
  // We make an exception for renderer-initiated navigations in new tabs, as
  // long as no other page has tried to access the initial empty document in
  // the new tab.  If another page modifies this blank page, a URL spoof is
  // possible, so we must stop showing the pending entry.
  bool safe_to_show_pending =
      pending_entry_ &&
      // Require a new navigation.
      pending_entry_->GetPageID() == -1 &&
      // Require either browser-initiated or an unmodified new tab.
      (!pending_entry_->is_renderer_initiated() || IsUnmodifiedBlankTab());

  // Also allow showing the pending entry for history navigations in a new tab,
  // such as Ctrl+Back.  In this case, no existing page is visible and no one
  // can script the new tab before it commits.
  if (!safe_to_show_pending &&
      pending_entry_ &&
      pending_entry_->GetPageID() != -1 &&
      IsInitialNavigation() &&
      !pending_entry_->is_renderer_initiated())
    safe_to_show_pending = true;

  if (safe_to_show_pending)
    return pending_entry_;
  return GetLastCommittedEntry();
}

int NavigationControllerImpl::GetCurrentEntryIndex() const {
  if (transient_entry_index_ != -1)
    return transient_entry_index_;
  if (pending_entry_index_ != -1)
    return pending_entry_index_;
  return last_committed_entry_index_;
}

NavigationEntry* NavigationControllerImpl::GetLastCommittedEntry() const {
  if (last_committed_entry_index_ == -1)
    return NULL;
  return entries_[last_committed_entry_index_].get();
}

bool NavigationControllerImpl::CanViewSource() const {
  const std::string& mime_type = delegate_->GetContentsMimeType();
  bool is_viewable_mime_type = net::IsSupportedNonImageMimeType(mime_type) &&
      !net::IsSupportedMediaMimeType(mime_type);
  NavigationEntry* visible_entry = GetVisibleEntry();
  return visible_entry && !visible_entry->IsViewSourceMode() &&
      is_viewable_mime_type && !delegate_->GetInterstitialPage();
}

int NavigationControllerImpl::GetLastCommittedEntryIndex() const {
  return last_committed_entry_index_;
}

int NavigationControllerImpl::GetEntryCount() const {
  DCHECK(entries_.size() <= max_entry_count());
  return static_cast<int>(entries_.size());
}

NavigationEntry* NavigationControllerImpl::GetEntryAtIndex(
    int index) const {
  return entries_.at(index).get();
}

NavigationEntry* NavigationControllerImpl::GetEntryAtOffset(
    int offset) const {
  int index = GetIndexForOffset(offset);
  if (index < 0 || index >= GetEntryCount())
    return NULL;

  return entries_[index].get();
}

int NavigationControllerImpl::GetIndexForOffset(int offset) const {
  return GetCurrentEntryIndex() + offset;
}

void NavigationControllerImpl::TakeScreenshot() {
  screenshot_manager_->TakeScreenshot();
}

void NavigationControllerImpl::SetScreenshotManager(
    NavigationEntryScreenshotManager* manager) {
  screenshot_manager_.reset(manager ? manager :
                            new NavigationEntryScreenshotManager(this));
}

bool NavigationControllerImpl::CanGoBack() const {
  return entries_.size() > 1 && GetCurrentEntryIndex() > 0;
}

bool NavigationControllerImpl::CanGoForward() const {
  int index = GetCurrentEntryIndex();
  return index >= 0 && index < (static_cast<int>(entries_.size()) - 1);
}

bool NavigationControllerImpl::CanGoToOffset(int offset) const {
  int index = GetIndexForOffset(offset);
  return index >= 0 && index < GetEntryCount();
}

void NavigationControllerImpl::GoBack() {
  if (!CanGoBack()) {
    NOTREACHED();
    return;
  }

  // Base the navigation on where we are now...
  int current_index = GetCurrentEntryIndex();

  DiscardNonCommittedEntries();

  pending_entry_index_ = current_index - 1;
  entries_[pending_entry_index_]->SetTransitionType(
      PageTransitionFromInt(
          entries_[pending_entry_index_]->GetTransitionType() |
          PAGE_TRANSITION_FORWARD_BACK));
  NavigateToPendingEntry(NO_RELOAD);
}

void NavigationControllerImpl::GoForward() {
  if (!CanGoForward()) {
    NOTREACHED();
    return;
  }

  bool transient = (transient_entry_index_ != -1);

  // Base the navigation on where we are now...
  int current_index = GetCurrentEntryIndex();

  DiscardNonCommittedEntries();

  pending_entry_index_ = current_index;
  // If there was a transient entry, we removed it making the current index
  // the next page.
  if (!transient)
    pending_entry_index_++;

  entries_[pending_entry_index_]->SetTransitionType(
      PageTransitionFromInt(
          entries_[pending_entry_index_]->GetTransitionType() |
          PAGE_TRANSITION_FORWARD_BACK));
  NavigateToPendingEntry(NO_RELOAD);
}

void NavigationControllerImpl::GoToIndex(int index) {
  if (index < 0 || index >= static_cast<int>(entries_.size())) {
    NOTREACHED();
    return;
  }

  if (transient_entry_index_ != -1) {
    if (index == transient_entry_index_) {
      // Nothing to do when navigating to the transient.
      return;
    }
    if (index > transient_entry_index_) {
      // Removing the transient is goint to shift all entries by 1.
      index--;
    }
  }

  DiscardNonCommittedEntries();

  pending_entry_index_ = index;
  entries_[pending_entry_index_]->SetTransitionType(
      PageTransitionFromInt(
          entries_[pending_entry_index_]->GetTransitionType() |
          PAGE_TRANSITION_FORWARD_BACK));
  NavigateToPendingEntry(NO_RELOAD);
}

void NavigationControllerImpl::GoToOffset(int offset) {
  if (!CanGoToOffset(offset))
    return;

  GoToIndex(GetIndexForOffset(offset));
}

bool NavigationControllerImpl::RemoveEntryAtIndex(int index) {
  if (index == last_committed_entry_index_ ||
      index == pending_entry_index_)
    return false;

  RemoveEntryAtIndexInternal(index);
  return true;
}

void NavigationControllerImpl::UpdateVirtualURLToURL(
    NavigationEntryImpl* entry, const GURL& new_url) {
  GURL new_virtual_url(new_url);
  if (BrowserURLHandlerImpl::GetInstance()->ReverseURLRewrite(
          &new_virtual_url, entry->GetVirtualURL(), browser_context_)) {
    entry->SetVirtualURL(new_virtual_url);
  }
}

void NavigationControllerImpl::LoadURL(
    const GURL& url,
    const Referrer& referrer,
    PageTransition transition,
    const std::string& extra_headers) {
  LoadURLParams params(url);
  params.referrer = referrer;
  params.transition_type = transition;
  params.extra_headers = extra_headers;
  LoadURLWithParams(params);
}

void NavigationControllerImpl::LoadURLWithParams(const LoadURLParams& params) {
  TRACE_EVENT0("browser", "NavigationControllerImpl::LoadURLWithParams");
  if (HandleDebugURL(params.url, params.transition_type))
    return;

  // Any renderer-side debug URLs or javascript: URLs should be ignored if the
  // renderer process is not live, unless it is the initial navigation of the
  // tab.
  if (IsRendererDebugURL(params.url)) {
    // TODO(creis): Find the RVH for the correct frame.
    if (!delegate_->GetRenderViewHost()->IsRenderViewLive() &&
        !IsInitialNavigation())
      return;
  }

  // Checks based on params.load_type.
  switch (params.load_type) {
    case LOAD_TYPE_DEFAULT:
      break;
    case LOAD_TYPE_BROWSER_INITIATED_HTTP_POST:
      if (!params.url.SchemeIs(url::kHttpScheme) &&
          !params.url.SchemeIs(url::kHttpsScheme)) {
        NOTREACHED() << "Http post load must use http(s) scheme.";
        return;
      }
      break;
    case LOAD_TYPE_DATA:
      if (!params.url.SchemeIs(url::kDataScheme)) {
        NOTREACHED() << "Data load must use data scheme.";
        return;
      }
      break;
    default:
      NOTREACHED();
      break;
  };

  // The user initiated a load, we don't need to reload anymore.
  needs_reload_ = false;

  bool override = false;
  switch (params.override_user_agent) {
    case UA_OVERRIDE_INHERIT:
      override = ShouldKeepOverride(GetLastCommittedEntry());
      break;
    case UA_OVERRIDE_TRUE:
      override = true;
      break;
    case UA_OVERRIDE_FALSE:
      override = false;
      break;
    default:
      NOTREACHED();
      break;
  }

  NavigationEntryImpl* entry = NavigationEntryImpl::FromNavigationEntry(
      CreateNavigationEntry(
          params.url,
          params.referrer,
          params.transition_type,
          params.is_renderer_initiated,
          params.extra_headers,
          browser_context_));
  if (params.frame_tree_node_id != -1)
    entry->set_frame_tree_node_id(params.frame_tree_node_id);
  if (params.redirect_chain.size() > 0)
    entry->SetRedirectChain(params.redirect_chain);
  if (params.should_replace_current_entry)
    entry->set_should_replace_entry(true);
  entry->set_should_clear_history_list(params.should_clear_history_list);
  entry->SetIsOverridingUserAgent(override);
  entry->set_transferred_global_request_id(
      params.transferred_global_request_id);
  entry->SetFrameToNavigate(params.frame_name);

  switch (params.load_type) {
    case LOAD_TYPE_DEFAULT:
      break;
    case LOAD_TYPE_BROWSER_INITIATED_HTTP_POST:
      entry->SetHasPostData(true);
      entry->SetBrowserInitiatedPostData(
          params.browser_initiated_post_data.get());
      break;
    case LOAD_TYPE_DATA:
      entry->SetBaseURLForDataURL(params.base_url_for_data_url);
      entry->SetVirtualURL(params.virtual_url_for_data_url);
      entry->SetCanLoadLocalResources(params.can_load_local_resources);
      break;
    default:
      NOTREACHED();
      break;
  };

  LoadEntry(entry);
}

bool NavigationControllerImpl::RendererDidNavigate(
    RenderFrameHost* rfh,
    const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
    LoadCommittedDetails* details) {
  is_initial_navigation_ = false;

  // Save the previous state before we clobber it.
  if (GetLastCommittedEntry()) {
    details->previous_url = GetLastCommittedEntry()->GetURL();
    details->previous_entry_index = GetLastCommittedEntryIndex();
  } else {
    details->previous_url = GURL();
    details->previous_entry_index = -1;
  }

  // If we have a pending entry at this point, it should have a SiteInstance.
  // Restored entries start out with a null SiteInstance, but we should have
  // assigned one in NavigateToPendingEntry.
  DCHECK(pending_entry_index_ == -1 || pending_entry_->site_instance());

  // If we are doing a cross-site reload, we need to replace the existing
  // navigation entry, not add another entry to the history. This has the side
  // effect of removing forward browsing history, if such existed.
  // Or if we are doing a cross-site redirect navigation,
  // we will do a similar thing.
  details->did_replace_entry =
      pending_entry_ && pending_entry_->should_replace_entry();

  // Do navigation-type specific actions. These will make and commit an entry.
  details->type = ClassifyNavigation(rfh, params);

  // is_in_page must be computed before the entry gets committed.
  details->is_in_page = IsURLInPageNavigation(
      params.url, params.was_within_same_page, details->type);

  switch (details->type) {
    case NAVIGATION_TYPE_NEW_PAGE:
      RendererDidNavigateToNewPage(rfh, params, details->did_replace_entry);
      break;
    case NAVIGATION_TYPE_EXISTING_PAGE:
      RendererDidNavigateToExistingPage(rfh, params);
      break;
    case NAVIGATION_TYPE_SAME_PAGE:
      RendererDidNavigateToSamePage(rfh, params);
      break;
    case NAVIGATION_TYPE_IN_PAGE:
      RendererDidNavigateInPage(rfh, params, &details->did_replace_entry);
      break;
    case NAVIGATION_TYPE_NEW_SUBFRAME:
      RendererDidNavigateNewSubframe(rfh, params);
      break;
    case NAVIGATION_TYPE_AUTO_SUBFRAME:
      if (!RendererDidNavigateAutoSubframe(rfh, params))
        return false;
      break;
    case NAVIGATION_TYPE_NAV_IGNORE:
      // If a pending navigation was in progress, this canceled it.  We should
      // discard it and make sure it is removed from the URL bar.  After that,
      // there is nothing we can do with this navigation, so we just return to
      // the caller that nothing has happened.
      if (pending_entry_) {
        DiscardNonCommittedEntries();
        delegate_->NotifyNavigationStateChanged(INVALIDATE_TYPE_URL);
      }
      return false;
    default:
      NOTREACHED();
  }

  // At this point, we know that the navigation has just completed, so
  // record the time.
  //
  // TODO(akalin): Use "sane time" as described in
  // http://www.chromium.org/developers/design-documents/sane-time .
  base::Time timestamp =
      time_smoother_.GetSmoothedTime(get_timestamp_callback_.Run());
  DVLOG(1) << "Navigation finished at (smoothed) timestamp "
           << timestamp.ToInternalValue();

  // We should not have a pending entry anymore.  Clear it again in case any
  // error cases above forgot to do so.
  DiscardNonCommittedEntriesInternal();

  // All committed entries should have nonempty content state so WebKit doesn't
  // get confused when we go back to them (see the function for details).
  DCHECK(params.page_state.IsValid());
  NavigationEntryImpl* active_entry =
      NavigationEntryImpl::FromNavigationEntry(GetLastCommittedEntry());
  active_entry->SetTimestamp(timestamp);
  active_entry->SetHttpStatusCode(params.http_status_code);
  active_entry->SetPageState(params.page_state);
  active_entry->SetRedirectChain(params.redirects);

  // Use histogram to track memory impact of redirect chain because it's now
  // not cleared for committed entries.
  size_t redirect_chain_size = 0;
  for (size_t i = 0; i < params.redirects.size(); ++i) {
    redirect_chain_size += params.redirects[i].spec().length();
  }
  UMA_HISTOGRAM_COUNTS("Navigation.RedirectChainSize", redirect_chain_size);

  // Once it is committed, we no longer need to track several pieces of state on
  // the entry.
  active_entry->ResetForCommit();

  // The active entry's SiteInstance should match our SiteInstance.
  // TODO(creis): This check won't pass for subframes until we create entries
  // for subframe navigations.
  if (PageTransitionIsMainFrame(params.transition))
    CHECK(active_entry->site_instance() == rfh->GetSiteInstance());

  // Remember the bindings the renderer process has at this point, so that
  // we do not grant this entry additional bindings if we come back to it.
  active_entry->SetBindings(
      static_cast<RenderFrameHostImpl*>(rfh)->GetEnabledBindings());

  // Now prep the rest of the details for the notification and broadcast.
  details->entry = active_entry;
  details->is_main_frame =
      PageTransitionIsMainFrame(params.transition);
  details->serialized_security_info = params.security_info;
  details->http_status_code = params.http_status_code;
  NotifyNavigationEntryCommitted(details);

  return true;
}

NavigationType NavigationControllerImpl::ClassifyNavigation(
    RenderFrameHost* rfh,
    const FrameHostMsg_DidCommitProvisionalLoad_Params& params) const {
  if (params.page_id == -1) {
    // The renderer generates the page IDs, and so if it gives us the invalid
    // page ID (-1) we know it didn't actually navigate. This happens in a few
    // cases:
    //
    // - If a page makes a popup navigated to about blank, and then writes
    //   stuff like a subframe navigated to a real page. We'll get the commit
    //   for the subframe, but there won't be any commit for the outer page.
    //
    // - We were also getting these for failed loads (for example, bug 21849).
    //   The guess is that we get a "load commit" for the alternate error page,
    //   but that doesn't affect the page ID, so we get the "old" one, which
    //   could be invalid. This can also happen for a cross-site transition
    //   that causes us to swap processes. Then the error page load will be in
    //   a new process with no page IDs ever assigned (and hence a -1 value),
    //   yet the navigation controller still might have previous pages in its
    //   list.
    //
    // In these cases, there's nothing we can do with them, so ignore.
    return NAVIGATION_TYPE_NAV_IGNORE;
  }

  if (params.page_id > delegate_->GetMaxPageIDForSiteInstance(
          rfh->GetSiteInstance())) {
    // Greater page IDs than we've ever seen before are new pages. We may or may
    // not have a pending entry for the page, and this may or may not be the
    // main frame.
    if (PageTransitionIsMainFrame(params.transition))
      return NAVIGATION_TYPE_NEW_PAGE;

    // When this is a new subframe navigation, we should have a committed page
    // for which it's a suframe in. This may not be the case when an iframe is
    // navigated on a popup navigated to about:blank (the iframe would be
    // written into the popup by script on the main page). For these cases,
    // there isn't any navigation stuff we can do, so just ignore it.
    if (!GetLastCommittedEntry())
      return NAVIGATION_TYPE_NAV_IGNORE;

    // Valid subframe navigation.
    return NAVIGATION_TYPE_NEW_SUBFRAME;
  }

  // We only clear the session history when navigating to a new page.
  DCHECK(!params.history_list_was_cleared);

  // Now we know that the notification is for an existing page. Find that entry.
  int existing_entry_index = GetEntryIndexWithPageID(
      rfh->GetSiteInstance(),
      params.page_id);
  if (existing_entry_index == -1) {
    // The page was not found. It could have been pruned because of the limit on
    // back/forward entries (not likely since we'll usually tell it to navigate
    // to such entries). It could also mean that the renderer is smoking crack.
    NOTREACHED();

    // Because the unknown entry has committed, we risk showing the wrong URL in
    // release builds. Instead, we'll kill the renderer process to be safe.
    LOG(ERROR) << "terminating renderer for bad navigation: " << params.url;
    RecordAction(base::UserMetricsAction("BadMessageTerminate_NC"));

    // Temporary code so we can get more information.  Format:
    //  http://url/foo.html#page1#max3#frame1#ids:2_Nx,1_1x,3_2
    std::string temp = params.url.spec();
    temp.append("#page");
    temp.append(base::IntToString(params.page_id));
    temp.append("#max");
    temp.append(base::IntToString(delegate_->GetMaxPageID()));
    temp.append("#frame");
    temp.append(base::IntToString(rfh->GetRoutingID()));
    temp.append("#ids");
    for (int i = 0; i < static_cast<int>(entries_.size()); ++i) {
      // Append entry metadata (e.g., 3_7x):
      //  3: page_id
      //  7: SiteInstance ID, or N for null
      //  x: appended if not from the current SiteInstance
      temp.append(base::IntToString(entries_[i]->GetPageID()));
      temp.append("_");
      if (entries_[i]->site_instance())
        temp.append(base::IntToString(entries_[i]->site_instance()->GetId()));
      else
        temp.append("N");
      if (entries_[i]->site_instance() != rfh->GetSiteInstance())
        temp.append("x");
      temp.append(",");
    }
    GURL url(temp);
    static_cast<RenderFrameHostImpl*>(rfh)->render_view_host()->Send(
        new ViewMsg_TempCrashWithData(url));
    return NAVIGATION_TYPE_NAV_IGNORE;
  }
  NavigationEntryImpl* existing_entry = entries_[existing_entry_index].get();

  if (!PageTransitionIsMainFrame(params.transition)) {
    // All manual subframes would get new IDs and were handled above, so we
    // know this is auto. Since the current page was found in the navigation
    // entry list, we're guaranteed to have a last committed entry.
    DCHECK(GetLastCommittedEntry());
    return NAVIGATION_TYPE_AUTO_SUBFRAME;
  }

  // Anything below here we know is a main frame navigation.
  if (pending_entry_ &&
      !pending_entry_->is_renderer_initiated() &&
      existing_entry != pending_entry_ &&
      pending_entry_->GetPageID() == -1 &&
      existing_entry == GetLastCommittedEntry()) {
    // In this case, we have a pending entry for a URL but WebCore didn't do a
    // new navigation. This happens when you press enter in the URL bar to
    // reload. We will create a pending entry, but WebKit will convert it to
    // a reload since it's the same page and not create a new entry for it
    // (the user doesn't want to have a new back/forward entry when they do
    // this). If this matches the last committed entry, we want to just ignore
    // the pending entry and go back to where we were (the "existing entry").
    return NAVIGATION_TYPE_SAME_PAGE;
  }

  // Any toplevel navigations with the same base (minus the reference fragment)
  // are in-page navigations. We weeded out subframe navigations above. Most of
  // the time this doesn't matter since WebKit doesn't tell us about subframe
  // navigations that don't actually navigate, but it can happen when there is
  // an encoding override (it always sends a navigation request).
  if (AreURLsInPageNavigation(existing_entry->GetURL(), params.url,
                              params.was_within_same_page,
                              NAVIGATION_TYPE_UNKNOWN)) {
    return NAVIGATION_TYPE_IN_PAGE;
  }

  // Since we weeded out "new" navigations above, we know this is an existing
  // (back/forward) navigation.
  return NAVIGATION_TYPE_EXISTING_PAGE;
}

void NavigationControllerImpl::RendererDidNavigateToNewPage(
    RenderFrameHost* rfh,
    const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
    bool replace_entry) {
  NavigationEntryImpl* new_entry;
  bool update_virtual_url;
  // Only make a copy of the pending entry if it is appropriate for the new page
  // that was just loaded.  We verify this at a coarse grain by checking that
  // the SiteInstance hasn't been assigned to something else.
  if (pending_entry_ &&
      (!pending_entry_->site_instance() ||
       pending_entry_->site_instance() == rfh->GetSiteInstance())) {
    new_entry = new NavigationEntryImpl(*pending_entry_);

    // Don't use the page type from the pending entry. Some interstitial page
    // may have set the type to interstitial. Once we commit, however, the page
    // type must always be normal.
    new_entry->set_page_type(PAGE_TYPE_NORMAL);
    update_virtual_url = new_entry->update_virtual_url_with_url();
  } else {
    new_entry = new NavigationEntryImpl;

    // Find out whether the new entry needs to update its virtual URL on URL
    // change and set up the entry accordingly. This is needed to correctly
    // update the virtual URL when replaceState is called after a pushState.
    GURL url = params.url;
    bool needs_update = false;
    // We call RewriteURLIfNecessary twice: once when page navigation
    // begins in CreateNavigationEntry, and once here when it commits.
    // With the kEnableGpuBenchmarking flag, the rewriting includes
    // handling debug URLs which cause an action to occur, and thus we
    // should not rewrite them a second time.
    bool skip_rewrite =
        IsDebugURL(url) && base::CommandLine::ForCurrentProcess()->HasSwitch(
            cc::switches::kEnableGpuBenchmarking);
    if (!skip_rewrite) {
      BrowserURLHandlerImpl::GetInstance()->RewriteURLIfNecessary(
          &url, browser_context_, &needs_update);
    }
    new_entry->set_update_virtual_url_with_url(needs_update);

    // When navigating to a new page, give the browser URL handler a chance to
    // update the virtual URL based on the new URL. For example, this is needed
    // to show chrome://bookmarks/#1 when the bookmarks webui extension changes
    // the URL.
    update_virtual_url = needs_update;
  }

  new_entry->SetURL(params.url);
  if (update_virtual_url)
    UpdateVirtualURLToURL(new_entry, params.url);
  new_entry->SetReferrer(params.referrer);
  new_entry->SetPageID(params.page_id);
  new_entry->SetTransitionType(params.transition);
  new_entry->set_site_instance(
      static_cast<SiteInstanceImpl*>(rfh->GetSiteInstance()));
  new_entry->SetHasPostData(params.is_post);
  new_entry->SetPostID(params.post_id);
  new_entry->SetOriginalRequestURL(params.original_request_url);
  new_entry->SetIsOverridingUserAgent(params.is_overriding_user_agent);

  // history.pushState() is classified as a navigation to a new page, but
  // sets was_within_same_page to true. In this case, we already have the
  // title available, so set it immediately.
  if (params.was_within_same_page && GetLastCommittedEntry())
    new_entry->SetTitle(GetLastCommittedEntry()->GetTitle());

  DCHECK(!params.history_list_was_cleared || !replace_entry);
  // The browser requested to clear the session history when it initiated the
  // navigation. Now we know that the renderer has updated its state accordingly
  // and it is safe to also clear the browser side history.
  if (params.history_list_was_cleared) {
    DiscardNonCommittedEntriesInternal();
    entries_.clear();
    last_committed_entry_index_ = -1;
  }

  InsertOrReplaceEntry(new_entry, replace_entry);
}

void NavigationControllerImpl::RendererDidNavigateToExistingPage(
    RenderFrameHost* rfh,
    const FrameHostMsg_DidCommitProvisionalLoad_Params& params) {
  // We should only get here for main frame navigations.
  DCHECK(PageTransitionIsMainFrame(params.transition));

  // This is a back/forward navigation. The existing page for the ID is
  // guaranteed to exist by ClassifyNavigation, and we just need to update it
  // with new information from the renderer.
  int entry_index = GetEntryIndexWithPageID(rfh->GetSiteInstance(),
                                            params.page_id);
  DCHECK(entry_index >= 0 &&
         entry_index < static_cast<int>(entries_.size()));
  NavigationEntryImpl* entry = entries_[entry_index].get();

  // The URL may have changed due to redirects.
  entry->SetURL(params.url);
  entry->SetReferrer(params.referrer);
  if (entry->update_virtual_url_with_url())
    UpdateVirtualURLToURL(entry, params.url);

  // The redirected to page should not inherit the favicon from the previous
  // page.
  if (PageTransitionIsRedirect(params.transition))
    entry->GetFavicon() = FaviconStatus();

  // The site instance will normally be the same except during session restore,
  // when no site instance will be assigned.
  DCHECK(entry->site_instance() == NULL ||
         entry->site_instance() == rfh->GetSiteInstance());
  entry->set_site_instance(
      static_cast<SiteInstanceImpl*>(rfh->GetSiteInstance()));

  entry->SetHasPostData(params.is_post);
  entry->SetPostID(params.post_id);

  // The entry we found in the list might be pending if the user hit
  // back/forward/reload. This load should commit it (since it's already in the
  // list, we can just discard the pending pointer).  We should also discard the
  // pending entry if it corresponds to a different navigation, since that one
  // is now likely canceled.  If it is not canceled, we will treat it as a new
  // navigation when it arrives, which is also ok.
  //
  // Note that we need to use the "internal" version since we don't want to
  // actually change any other state, just kill the pointer.
  DiscardNonCommittedEntriesInternal();

  // If a transient entry was removed, the indices might have changed, so we
  // have to query the entry index again.
  last_committed_entry_index_ =
      GetEntryIndexWithPageID(rfh->GetSiteInstance(), params.page_id);
}

void NavigationControllerImpl::RendererDidNavigateToSamePage(
    RenderFrameHost* rfh,
    const FrameHostMsg_DidCommitProvisionalLoad_Params& params) {
  // This mode implies we have a pending entry that's the same as an existing
  // entry for this page ID. This entry is guaranteed to exist by
  // ClassifyNavigation. All we need to do is update the existing entry.
  NavigationEntryImpl* existing_entry = GetEntryWithPageID(
      rfh->GetSiteInstance(), params.page_id);

  // We assign the entry's unique ID to be that of the new one. Since this is
  // always the result of a user action, we want to dismiss infobars, etc. like
  // a regular user-initiated navigation.
  existing_entry->set_unique_id(pending_entry_->GetUniqueID());

  // The URL may have changed due to redirects.
  if (existing_entry->update_virtual_url_with_url())
    UpdateVirtualURLToURL(existing_entry, params.url);
  existing_entry->SetURL(params.url);
  existing_entry->SetReferrer(params.referrer);

  // The page may have been requested with a different HTTP method.
  existing_entry->SetHasPostData(params.is_post);
  existing_entry->SetPostID(params.post_id);

  DiscardNonCommittedEntries();
}

void NavigationControllerImpl::RendererDidNavigateInPage(
    RenderFrameHost* rfh,
    const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
    bool* did_replace_entry) {
  DCHECK(PageTransitionIsMainFrame(params.transition)) <<
      "WebKit should only tell us about in-page navs for the main frame.";
  // We're guaranteed to have an entry for this one.
  NavigationEntryImpl* existing_entry = GetEntryWithPageID(
      rfh->GetSiteInstance(), params.page_id);

  // Reference fragment navigation. We're guaranteed to have the last_committed
  // entry and it will be the same page as the new navigation (minus the
  // reference fragments, of course).  We'll update the URL of the existing
  // entry without pruning the forward history.
  existing_entry->SetURL(params.url);
  if (existing_entry->update_virtual_url_with_url())
    UpdateVirtualURLToURL(existing_entry, params.url);

  existing_entry->SetHasPostData(params.is_post);
  existing_entry->SetPostID(params.post_id);

  // This replaces the existing entry since the page ID didn't change.
  *did_replace_entry = true;

  DiscardNonCommittedEntriesInternal();

  // If a transient entry was removed, the indices might have changed, so we
  // have to query the entry index again.
  last_committed_entry_index_ =
      GetEntryIndexWithPageID(rfh->GetSiteInstance(), params.page_id);
}

void NavigationControllerImpl::RendererDidNavigateNewSubframe(
    RenderFrameHost* rfh,
    const FrameHostMsg_DidCommitProvisionalLoad_Params& params) {
  if (PageTransitionCoreTypeIs(params.transition,
                               PAGE_TRANSITION_AUTO_SUBFRAME)) {
    // This is not user-initiated. Ignore.
    DiscardNonCommittedEntriesInternal();
    return;
  }

  // Manual subframe navigations just get the current entry cloned so the user
  // can go back or forward to it. The actual subframe information will be
  // stored in the page state for each of those entries. This happens out of
  // band with the actual navigations.
  DCHECK(GetLastCommittedEntry()) << "ClassifyNavigation should guarantee "
                                  << "that a last committed entry exists.";
  NavigationEntryImpl* new_entry = new NavigationEntryImpl(
      *NavigationEntryImpl::FromNavigationEntry(GetLastCommittedEntry()));
  new_entry->SetPageID(params.page_id);
  InsertOrReplaceEntry(new_entry, false);
}

bool NavigationControllerImpl::RendererDidNavigateAutoSubframe(
    RenderFrameHost* rfh,
    const FrameHostMsg_DidCommitProvisionalLoad_Params& params) {
  // We're guaranteed to have a previously committed entry, and we now need to
  // handle navigation inside of a subframe in it without creating a new entry.
  DCHECK(GetLastCommittedEntry());

  // Handle the case where we're navigating back/forward to a previous subframe
  // navigation entry. This is case "2." in NAV_AUTO_SUBFRAME comment in the
  // header file. In case "1." this will be a NOP.
  int entry_index = GetEntryIndexWithPageID(
      rfh->GetSiteInstance(),
      params.page_id);
  if (entry_index < 0 ||
      entry_index >= static_cast<int>(entries_.size())) {
    NOTREACHED();
    return false;
  }

  // Update the current navigation entry in case we're going back/forward.
  if (entry_index != last_committed_entry_index_) {
    last_committed_entry_index_ = entry_index;
    DiscardNonCommittedEntriesInternal();
    return true;
  }

  // We do not need to discard the pending entry in this case, since we will
  // not generate commit notifications for this auto-subframe navigation.
  return false;
}

int NavigationControllerImpl::GetIndexOfEntry(
    const NavigationEntryImpl* entry) const {
  const NavigationEntries::const_iterator i(std::find(
      entries_.begin(),
      entries_.end(),
      entry));
  return (i == entries_.end()) ? -1 : static_cast<int>(i - entries_.begin());
}

bool NavigationControllerImpl::IsURLInPageNavigation(
    const GURL& url,
    bool renderer_says_in_page,
    NavigationType navigation_type) const {
  NavigationEntry* last_committed = GetLastCommittedEntry();
  return last_committed && AreURLsInPageNavigation(
      last_committed->GetURL(), url, renderer_says_in_page, navigation_type);
}

void NavigationControllerImpl::CopyStateFrom(
    const NavigationController& temp) {
  const NavigationControllerImpl& source =
      static_cast<const NavigationControllerImpl&>(temp);
  // Verify that we look new.
  DCHECK(GetEntryCount() == 0 && !GetPendingEntry());

  if (source.GetEntryCount() == 0)
    return;  // Nothing new to do.

  needs_reload_ = true;
  InsertEntriesFrom(source, source.GetEntryCount());

  for (SessionStorageNamespaceMap::const_iterator it =
           source.session_storage_namespace_map_.begin();
       it != source.session_storage_namespace_map_.end();
       ++it) {
    SessionStorageNamespaceImpl* source_namespace =
        static_cast<SessionStorageNamespaceImpl*>(it->second.get());
    session_storage_namespace_map_[it->first] = source_namespace->Clone();
  }

  FinishRestore(source.last_committed_entry_index_, RESTORE_CURRENT_SESSION);

  // Copy the max page id map from the old tab to the new tab.  This ensures
  // that new and existing navigations in the tab's current SiteInstances
  // are identified properly.
  delegate_->CopyMaxPageIDsFrom(source.delegate()->GetWebContents());
}

void NavigationControllerImpl::CopyStateFromAndPrune(
    NavigationController* temp,
    bool replace_entry) {
  // It is up to callers to check the invariants before calling this.
  CHECK(CanPruneAllButLastCommitted());

  NavigationControllerImpl* source =
      static_cast<NavigationControllerImpl*>(temp);
  // The SiteInstance and page_id of the last committed entry needs to be
  // remembered at this point, in case there is only one committed entry
  // and it is pruned.  We use a scoped_refptr to ensure the SiteInstance
  // can't be freed during this time period.
  NavigationEntryImpl* last_committed =
      NavigationEntryImpl::FromNavigationEntry(GetLastCommittedEntry());
  scoped_refptr<SiteInstance> site_instance(
      last_committed->site_instance());
  int32 minimum_page_id = last_committed->GetPageID();
  int32 max_page_id =
      delegate_->GetMaxPageIDForSiteInstance(site_instance.get());

  // Remove all the entries leaving the active entry.
  PruneAllButLastCommittedInternal();

  // We now have one entry, possibly with a new pending entry.  Ensure that
  // adding the entries from source won't put us over the limit.
  DCHECK_EQ(1, GetEntryCount());
  if (!replace_entry)
    source->PruneOldestEntryIfFull();

  // Insert the entries from source. Don't use source->GetCurrentEntryIndex as
  // we don't want to copy over the transient entry.  Ignore any pending entry,
  // since it has not committed in source.
  int max_source_index = source->last_committed_entry_index_;
  if (max_source_index == -1)
    max_source_index = source->GetEntryCount();
  else
    max_source_index++;

  // Ignore the source's current entry if merging with replacement.
  // TODO(davidben): This should preserve entries forward of the current
  // too. http://crbug.com/317872
  if (replace_entry && max_source_index > 0)
    max_source_index--;

  InsertEntriesFrom(*source, max_source_index);

  // Adjust indices such that the last entry and pending are at the end now.
  last_committed_entry_index_ = GetEntryCount() - 1;

  delegate_->SetHistoryLengthAndPrune(site_instance.get(),
                                      max_source_index,
                                      minimum_page_id);

  // Copy the max page id map from the old tab to the new tab.  This ensures
  // that new and existing navigations in the tab's current SiteInstances
  // are identified properly.
  delegate_->CopyMaxPageIDsFrom(source->delegate()->GetWebContents());
  max_restored_page_id_ = source->max_restored_page_id_;

  // If there is a last committed entry, be sure to include it in the new
  // max page ID map.
  if (max_page_id > -1) {
    delegate_->UpdateMaxPageIDForSiteInstance(site_instance.get(),
                                              max_page_id);
  }
}

bool NavigationControllerImpl::CanPruneAllButLastCommitted() {
  // If there is no last committed entry, we cannot prune.  Even if there is a
  // pending entry, it may not commit, leaving this WebContents blank, despite
  // possibly giving it new entries via CopyStateFromAndPrune.
  if (last_committed_entry_index_ == -1)
    return false;

  // We cannot prune if there is a pending entry at an existing entry index.
  // It may not commit, so we have to keep the last committed entry, and thus
  // there is no sensible place to keep the pending entry.  It is ok to have
  // a new pending entry, which can optionally commit as a new navigation.
  if (pending_entry_index_ != -1)
    return false;

  // We should not prune if we are currently showing a transient entry.
  if (transient_entry_index_ != -1)
    return false;

  return true;
}

void NavigationControllerImpl::PruneAllButLastCommitted() {
  PruneAllButLastCommittedInternal();

  // We should still have a last committed entry.
  DCHECK_NE(-1, last_committed_entry_index_);

  // We pass 0 instead of GetEntryCount() for the history_length parameter of
  // SetHistoryLengthAndPrune, because it will create history_length additional
  // history entries.
  // TODO(jochen): This API is confusing and we should clean it up.
  // http://crbug.com/178491
  NavigationEntryImpl* entry =
      NavigationEntryImpl::FromNavigationEntry(GetVisibleEntry());
  delegate_->SetHistoryLengthAndPrune(
      entry->site_instance(), 0, entry->GetPageID());
}

void NavigationControllerImpl::PruneAllButLastCommittedInternal() {
  // It is up to callers to check the invariants before calling this.
  CHECK(CanPruneAllButLastCommitted());

  // Erase all entries but the last committed entry.  There may still be a
  // new pending entry after this.
  entries_.erase(entries_.begin(),
                 entries_.begin() + last_committed_entry_index_);
  entries_.erase(entries_.begin() + 1, entries_.end());
  last_committed_entry_index_ = 0;
}

void NavigationControllerImpl::ClearAllScreenshots() {
  screenshot_manager_->ClearAllScreenshots();
}

void NavigationControllerImpl::SetSessionStorageNamespace(
    const std::string& partition_id,
    SessionStorageNamespace* session_storage_namespace) {
  if (!session_storage_namespace)
    return;

  // We can't overwrite an existing SessionStorage without violating spec.
  // Attempts to do so may give a tab access to another tab's session storage
  // so die hard on an error.
  bool successful_insert = session_storage_namespace_map_.insert(
      make_pair(partition_id,
                static_cast<SessionStorageNamespaceImpl*>(
                    session_storage_namespace)))
          .second;
  CHECK(successful_insert) << "Cannot replace existing SessionStorageNamespace";
}

void NavigationControllerImpl::SetMaxRestoredPageID(int32 max_id) {
  max_restored_page_id_ = max_id;
}

int32 NavigationControllerImpl::GetMaxRestoredPageID() const {
  return max_restored_page_id_;
}

bool NavigationControllerImpl::IsUnmodifiedBlankTab() const {
  return IsInitialNavigation() &&
         !GetLastCommittedEntry() &&
         !delegate_->HasAccessedInitialDocument();
}

SessionStorageNamespace*
NavigationControllerImpl::GetSessionStorageNamespace(SiteInstance* instance) {
  std::string partition_id;
  if (instance) {
    // TODO(ajwong): When GetDefaultSessionStorageNamespace() goes away, remove
    // this if statement so |instance| must not be NULL.
    partition_id =
        GetContentClient()->browser()->GetStoragePartitionIdForSite(
            browser_context_, instance->GetSiteURL());
  }

  SessionStorageNamespaceMap::const_iterator it =
      session_storage_namespace_map_.find(partition_id);
  if (it != session_storage_namespace_map_.end())
    return it->second.get();

  // Create one if no one has accessed session storage for this partition yet.
  //
  // TODO(ajwong): Should this use the |partition_id| directly rather than
  // re-lookup via |instance|?  http://crbug.com/142685
  StoragePartition* partition =
              BrowserContext::GetStoragePartition(browser_context_, instance);
  SessionStorageNamespaceImpl* session_storage_namespace =
      new SessionStorageNamespaceImpl(
          static_cast<DOMStorageContextWrapper*>(
              partition->GetDOMStorageContext()));
  session_storage_namespace_map_[partition_id] = session_storage_namespace;

  return session_storage_namespace;
}

SessionStorageNamespace*
NavigationControllerImpl::GetDefaultSessionStorageNamespace() {
  // TODO(ajwong): Remove if statement in GetSessionStorageNamespace().
  return GetSessionStorageNamespace(NULL);
}

const SessionStorageNamespaceMap&
NavigationControllerImpl::GetSessionStorageNamespaceMap() const {
  return session_storage_namespace_map_;
}

bool NavigationControllerImpl::NeedsReload() const {
  return needs_reload_;
}

void NavigationControllerImpl::SetNeedsReload() {
  needs_reload_ = true;
}

void NavigationControllerImpl::RemoveEntryAtIndexInternal(int index) {
  DCHECK(index < GetEntryCount());
  DCHECK(index != last_committed_entry_index_);

  DiscardNonCommittedEntries();

  entries_.erase(entries_.begin() + index);
  if (last_committed_entry_index_ > index)
    last_committed_entry_index_--;
}

void NavigationControllerImpl::DiscardNonCommittedEntries() {
  bool transient = transient_entry_index_ != -1;
  DiscardNonCommittedEntriesInternal();

  // If there was a transient entry, invalidate everything so the new active
  // entry state is shown.
  if (transient) {
    delegate_->NotifyNavigationStateChanged(kInvalidateAll);
  }
}

NavigationEntry* NavigationControllerImpl::GetPendingEntry() const {
  return pending_entry_;
}

int NavigationControllerImpl::GetPendingEntryIndex() const {
  return pending_entry_index_;
}

void NavigationControllerImpl::InsertOrReplaceEntry(NavigationEntryImpl* entry,
                                                    bool replace) {
  DCHECK(entry->GetTransitionType() != PAGE_TRANSITION_AUTO_SUBFRAME);

  // Copy the pending entry's unique ID to the committed entry.
  // I don't know if pending_entry_index_ can be other than -1 here.
  const NavigationEntryImpl* const pending_entry =
      (pending_entry_index_ == -1) ?
          pending_entry_ : entries_[pending_entry_index_].get();
  if (pending_entry)
    entry->set_unique_id(pending_entry->GetUniqueID());

  DiscardNonCommittedEntriesInternal();

  int current_size = static_cast<int>(entries_.size());

  if (current_size > 0) {
    // Prune any entries which are in front of the current entry.
    // Also prune the current entry if we are to replace the current entry.
    // last_committed_entry_index_ must be updated here since calls to
    // NotifyPrunedEntries() below may re-enter and we must make sure
    // last_committed_entry_index_ is not left in an invalid state.
    if (replace)
      --last_committed_entry_index_;

    int num_pruned = 0;
    while (last_committed_entry_index_ < (current_size - 1)) {
      num_pruned++;
      entries_.pop_back();
      current_size--;
    }
    if (num_pruned > 0)  // Only notify if we did prune something.
      NotifyPrunedEntries(this, false, num_pruned);
  }

  PruneOldestEntryIfFull();

  entries_.push_back(linked_ptr<NavigationEntryImpl>(entry));
  last_committed_entry_index_ = static_cast<int>(entries_.size()) - 1;

  // This is a new page ID, so we need everybody to know about it.
  delegate_->UpdateMaxPageID(entry->GetPageID());
}

void NavigationControllerImpl::PruneOldestEntryIfFull() {
  if (entries_.size() >= max_entry_count()) {
    DCHECK_EQ(max_entry_count(), entries_.size());
    DCHECK_GT(last_committed_entry_index_, 0);
    RemoveEntryAtIndex(0);
    NotifyPrunedEntries(this, true, 1);
  }
}

void NavigationControllerImpl::NavigateToPendingEntry(ReloadType reload_type) {
  needs_reload_ = false;

  // If we were navigating to a slow-to-commit page, and the user performs
  // a session history navigation to the last committed page, RenderViewHost
  // will force the throbber to start, but WebKit will essentially ignore the
  // navigation, and won't send a message to stop the throbber. To prevent this
  // from happening, we drop the navigation here and stop the slow-to-commit
  // page from loading (which would normally happen during the navigation).
  if (pending_entry_index_ != -1 &&
      pending_entry_index_ == last_committed_entry_index_ &&
      (entries_[pending_entry_index_]->restore_type() ==
          NavigationEntryImpl::RESTORE_NONE) &&
      (entries_[pending_entry_index_]->GetTransitionType() &
          PAGE_TRANSITION_FORWARD_BACK)) {
    delegate_->Stop();

    // If an interstitial page is showing, we want to close it to get back
    // to what was showing before.
    if (delegate_->GetInterstitialPage())
      delegate_->GetInterstitialPage()->DontProceed();

    DiscardNonCommittedEntries();
    return;
  }

  // If an interstitial page is showing, the previous renderer is blocked and
  // cannot make new requests.  Unblock (and disable) it to allow this
  // navigation to succeed.  The interstitial will stay visible until the
  // resulting DidNavigate.
  if (delegate_->GetInterstitialPage()) {
    static_cast<InterstitialPageImpl*>(delegate_->GetInterstitialPage())->
        CancelForNavigation();
  }

  // For session history navigations only the pending_entry_index_ is set.
  if (!pending_entry_) {
    DCHECK_NE(pending_entry_index_, -1);
    pending_entry_ = entries_[pending_entry_index_].get();
  }

  // This call does not support re-entrancy.  See http://crbug.com/347742.
  CHECK(!in_navigate_to_pending_entry_);
  in_navigate_to_pending_entry_ = true;
  bool success = delegate_->NavigateToPendingEntry(reload_type);
  in_navigate_to_pending_entry_ = false;

  if (!success)
    DiscardNonCommittedEntries();

  // If the entry is being restored and doesn't have a SiteInstance yet, fill
  // it in now that we know. This allows us to find the entry when it commits.
  if (pending_entry_ && !pending_entry_->site_instance() &&
      pending_entry_->restore_type() != NavigationEntryImpl::RESTORE_NONE) {
    pending_entry_->set_site_instance(static_cast<SiteInstanceImpl*>(
        delegate_->GetPendingSiteInstance()));
    pending_entry_->set_restore_type(NavigationEntryImpl::RESTORE_NONE);
  }
}

void NavigationControllerImpl::NotifyNavigationEntryCommitted(
    LoadCommittedDetails* details) {
  details->entry = GetLastCommittedEntry();

  // We need to notify the ssl_manager_ before the web_contents_ so the
  // location bar will have up-to-date information about the security style
  // when it wants to draw.  See http://crbug.com/11157
  ssl_manager_.DidCommitProvisionalLoad(*details);

  delegate_->NotifyNavigationStateChanged(kInvalidateAll);
  delegate_->NotifyNavigationEntryCommitted(*details);

  // TODO(avi): Remove. http://crbug.com/170921
  NotificationDetails notification_details =
      Details<LoadCommittedDetails>(details);
  NotificationService::current()->Notify(
      NOTIFICATION_NAV_ENTRY_COMMITTED,
      Source<NavigationController>(this),
      notification_details);
}

// static
size_t NavigationControllerImpl::max_entry_count() {
  if (max_entry_count_for_testing_ != kMaxEntryCountForTestingNotSet)
     return max_entry_count_for_testing_;
  return kMaxSessionHistoryEntries;
}

void NavigationControllerImpl::SetActive(bool is_active) {
  if (is_active && needs_reload_)
    LoadIfNecessary();
}

void NavigationControllerImpl::LoadIfNecessary() {
  if (!needs_reload_)
    return;

  // Calling Reload() results in ignoring state, and not loading.
  // Explicitly use NavigateToPendingEntry so that the renderer uses the
  // cached state.
  pending_entry_index_ = last_committed_entry_index_;
  NavigateToPendingEntry(NO_RELOAD);
}

void NavigationControllerImpl::NotifyEntryChanged(const NavigationEntry* entry,
                                                  int index) {
  EntryChangedDetails det;
  det.changed_entry = entry;
  det.index = index;
  NotificationService::current()->Notify(
      NOTIFICATION_NAV_ENTRY_CHANGED,
      Source<NavigationController>(this),
      Details<EntryChangedDetails>(&det));
}

void NavigationControllerImpl::FinishRestore(int selected_index,
                                             RestoreType type) {
  DCHECK(selected_index >= 0 && selected_index < GetEntryCount());
  ConfigureEntriesForRestore(&entries_, type);

  SetMaxRestoredPageID(static_cast<int32>(GetEntryCount()));

  last_committed_entry_index_ = selected_index;
}

void NavigationControllerImpl::DiscardNonCommittedEntriesInternal() {
  DiscardPendingEntry();
  DiscardTransientEntry();
}

void NavigationControllerImpl::DiscardPendingEntry() {
  // It is not safe to call DiscardPendingEntry while NavigateToEntry is in
  // progress, since this will cause a use-after-free.  (We only allow this
  // when the tab is being destroyed for shutdown, since it won't return to
  // NavigateToEntry in that case.)  http://crbug.com/347742.
  CHECK(!in_navigate_to_pending_entry_ || delegate_->IsBeingDestroyed());

  if (pending_entry_index_ == -1)
    delete pending_entry_;
  pending_entry_ = NULL;
  pending_entry_index_ = -1;
}

void NavigationControllerImpl::DiscardTransientEntry() {
  if (transient_entry_index_ == -1)
    return;
  entries_.erase(entries_.begin() + transient_entry_index_);
  if (last_committed_entry_index_ > transient_entry_index_)
    last_committed_entry_index_--;
  transient_entry_index_ = -1;
}

int NavigationControllerImpl::GetEntryIndexWithPageID(
    SiteInstance* instance, int32 page_id) const {
  for (int i = static_cast<int>(entries_.size()) - 1; i >= 0; --i) {
    if ((entries_[i]->site_instance() == instance) &&
        (entries_[i]->GetPageID() == page_id))
      return i;
  }
  return -1;
}

NavigationEntry* NavigationControllerImpl::GetTransientEntry() const {
  if (transient_entry_index_ == -1)
    return NULL;
  return entries_[transient_entry_index_].get();
}

void NavigationControllerImpl::SetTransientEntry(NavigationEntry* entry) {
  // Discard any current transient entry, we can only have one at a time.
  int index = 0;
  if (last_committed_entry_index_ != -1)
    index = last_committed_entry_index_ + 1;
  DiscardTransientEntry();
  entries_.insert(
      entries_.begin() + index, linked_ptr<NavigationEntryImpl>(
          NavigationEntryImpl::FromNavigationEntry(entry)));
  transient_entry_index_ = index;
  delegate_->NotifyNavigationStateChanged(kInvalidateAll);
}

void NavigationControllerImpl::InsertEntriesFrom(
    const NavigationControllerImpl& source,
    int max_index) {
  DCHECK_LE(max_index, source.GetEntryCount());
  size_t insert_index = 0;
  for (int i = 0; i < max_index; i++) {
    // When cloning a tab, copy all entries except interstitial pages
    if (source.entries_[i].get()->GetPageType() !=
        PAGE_TYPE_INTERSTITIAL) {
      entries_.insert(entries_.begin() + insert_index++,
                      linked_ptr<NavigationEntryImpl>(
                          new NavigationEntryImpl(*source.entries_[i])));
    }
  }
}

void NavigationControllerImpl::SetGetTimestampCallbackForTest(
    const base::Callback<base::Time()>& get_timestamp_callback) {
  get_timestamp_callback_ = get_timestamp_callback;
}

}  // namespace content
