// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 *     (http://www.torchmobile.com/)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "content/browser/frame_host/navigation_controller_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"  // Temporary
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "components/mime_util/mime_util.h"
#include "content/browser/bad_message.h"
#include "content/browser/browser_url_handler_impl.h"
#include "content/browser/dom_storage/dom_storage_context_wrapper.h"
#include "content/browser/dom_storage/session_storage_namespace_impl.h"
#include "content/browser/frame_host/debug_urls.h"
#include "content/browser/frame_host/interstitial_page_impl.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/frame_host/navigation_entry_screenshot_manager.h"
#include "content/browser/frame_host/navigation_handle_impl.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/renderer_host/render_view_host_impl.h"  // Temporary
#include "content/browser/site_instance_impl.h"
#include "content/common/frame_messages.h"
#include "content/common/site_isolation_policy.h"
#include "content/common/ssl_status_serialization.h"
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
#include "content/public/common/content_features.h"
#include "media/base/mime_util.h"
#include "net/base/escape.h"
#include "skia/ext/platform_canvas.h"
#include "url/url_constants.h"

namespace content {
namespace {

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
    std::vector<std::unique_ptr<NavigationEntryImpl>>* entries,
    NavigationController::RestoreType type) {
  for (size_t i = 0; i < entries->size(); ++i) {
    // Use a transition type of reload so that we don't incorrectly increase
    // the typed count.
    (*entries)[i]->SetTransitionType(ui::PAGE_TRANSITION_RELOAD);
    (*entries)[i]->set_restore_type(ControllerRestoreTypeToEntryType(type));
    // NOTE(darin): This code is only needed for backwards compat.
    SetPageStateIfEmpty((*entries)[i].get());
  }
}

// Determines whether or not we should be carrying over a user agent override
// between two NavigationEntries.
bool ShouldKeepOverride(const NavigationEntry* last_entry) {
  return last_entry && last_entry->GetIsOverridingUserAgent();
}

}  // namespace

// NavigationControllerImpl ----------------------------------------------------

const size_t kMaxEntryCountForTestingNotSet = static_cast<size_t>(-1);

// static
size_t NavigationControllerImpl::max_entry_count_for_testing_ =
    kMaxEntryCountForTestingNotSet;

// Should Reload check for post data? The default is true, but is set to false
// when testing.
static bool g_check_for_repost = true;

// static
std::unique_ptr<NavigationEntry> NavigationController::CreateNavigationEntry(
    const GURL& url,
    const Referrer& referrer,
    ui::PageTransition transition,
    bool is_renderer_initiated,
    const std::string& extra_headers,
    BrowserContext* browser_context) {
  // Fix up the given URL before letting it be rewritten, so that any minor
  // cleanup (e.g., removing leading dots) will not lead to a virtual URL.
  GURL dest_url(url);
  BrowserURLHandlerImpl::GetInstance()->FixupURLBeforeRewrite(&dest_url,
                                                              browser_context);

  // Allow the browser URL handler to rewrite the URL. This will, for example,
  // remove "view-source:" from the beginning of the URL to get the URL that
  // will actually be loaded. This real URL won't be shown to the user, just
  // used internally.
  GURL loaded_url(dest_url);
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
  entry->SetVirtualURL(dest_url);
  entry->set_user_typed_url(dest_url);
  entry->set_update_virtual_url_with_url(reverse_on_redirect);
  entry->set_extra_headers(extra_headers);
  return base::WrapUnique(entry);
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
      failed_pending_entry_id_(0),
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

void NavigationControllerImpl::Restore(
    int selected_navigation,
    RestoreType type,
    std::vector<std::unique_ptr<NavigationEntry>>* entries) {
  // Verify that this controller is unused and that the input is valid.
  DCHECK(GetEntryCount() == 0 && !GetPendingEntry());
  DCHECK(selected_navigation >= 0 &&
         selected_navigation < static_cast<int>(entries->size()));

  needs_reload_ = true;
  entries_.reserve(entries->size());
  for (auto& entry : *entries)
    entries_.push_back(
        NavigationEntryImpl::FromNavigationEntry(std::move(entry)));

  // At this point, the |entries| is full of empty scoped_ptrs, so it can be
  // cleared out safely.
  entries->clear();

  // And finish the restore.
  FinishRestore(selected_navigation, type);
}

void NavigationControllerImpl::Reload(bool check_for_repost) {
  ReloadType type = RELOAD;
  if (base::FeatureList::IsEnabled(
        features::kNonValidatingReloadOnNormalReload)) {
    type = RELOAD_MAIN_RESOURCE;
  }
  ReloadInternal(check_for_repost, type);
}
void NavigationControllerImpl::ReloadToRefreshContent(bool check_for_repost) {
  ReloadType type = RELOAD;
  if (base::FeatureList::IsEnabled(
        features::kNonValidatingReloadOnRefreshContent)) {
    type = RELOAD_MAIN_RESOURCE;
  }
  ReloadInternal(check_for_repost, type);
}
void NavigationControllerImpl::ReloadBypassingCache(bool check_for_repost) {
  ReloadInternal(check_for_repost, RELOAD_BYPASSING_CACHE);
}
void NavigationControllerImpl::ReloadOriginalRequestURL(bool check_for_repost) {
  ReloadInternal(check_for_repost, RELOAD_ORIGINAL_REQUEST_URL);
}
void NavigationControllerImpl::ReloadDisableLoFi(bool check_for_repost) {
  ReloadInternal(check_for_repost, RELOAD_DISABLE_LOFI_MODE);
}

void NavigationControllerImpl::ReloadInternal(bool check_for_repost,
                                              ReloadType reload_type) {
  if (transient_entry_index_ != -1) {
    // If an interstitial is showing, treat a reload as a navigation to the
    // transient entry's URL.
    NavigationEntryImpl* transient_entry = GetTransientEntry();
    if (!transient_entry)
      return;
    LoadURL(transient_entry->GetURL(),
            Referrer(),
            ui::PAGE_TRANSITION_RELOAD,
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
      entry = GetEntryAtIndex(current_index);
    }
  }

  // If we are no where, then we can't reload.  TODO(darin): We should add a
  // CanReload method.
  if (!entry)
    return;

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
    bool is_for_guests_only = site_instance && site_instance->HasProcess() &&
        site_instance->GetProcess()->IsForGuestsOnly();
    if (!is_for_guests_only && site_instance &&
        site_instance->HasWrongProcessForURL(entry->GetURL())) {
      // Create a navigation entry that resembles the current one, but do not
      // copy page id, site instance, content state, or timestamp.
      NavigationEntryImpl* nav_entry = NavigationEntryImpl::FromNavigationEntry(
          CreateNavigationEntry(
              entry->GetURL(), entry->GetReferrer(), entry->GetTransitionType(),
              false, entry->extra_headers(), browser_context_).release());

      // Mark the reload type as NO_RELOAD, so navigation will not be considered
      // a reload in the renderer.
      reload_type = NavigationController::NO_RELOAD;

      nav_entry->set_should_replace_entry(true);
      pending_entry_ = nav_entry;
      DCHECK_EQ(-1, pending_entry_index_);
    } else {
      pending_entry_ = entry;
      pending_entry_index_ = current_index;

      // The title of the page being reloaded might have been removed in the
      // meanwhile, so we need to revert to the default title upon reload and
      // invalidate the previously cached title (SetTitle will do both).
      // See Chromium issue 96041.
      pending_entry_->SetTitle(base::string16());

      pending_entry_->SetTransitionType(ui::PAGE_TRANSITION_RELOAD);
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

bool NavigationControllerImpl::IsInitialBlankNavigation() const {
  // TODO(creis): Once we create a NavigationEntry for the initial blank page,
  // we'll need to check for entry count 1 and restore_type RESTORE_NONE (to
  // exclude the cloned tab case).
  return IsInitialNavigation() && GetEntryCount() == 0;
}

NavigationEntryImpl* NavigationControllerImpl::GetEntryWithPageID(
    SiteInstance* instance,
    int32_t page_id) const {
  int index = GetEntryIndexWithPageID(instance, page_id);
  return (index != -1) ? entries_[index].get() : nullptr;
}

NavigationEntryImpl*
NavigationControllerImpl::GetEntryWithUniqueID(int nav_entry_id) const {
  int index = GetEntryIndexWithUniqueID(nav_entry_id);
  return (index != -1) ? entries_[index].get() : nullptr;
}

void NavigationControllerImpl::LoadEntry(
    std::unique_ptr<NavigationEntryImpl> entry) {
  // When navigating to a new page, we don't know for sure if we will actually
  // end up leaving the current page.  The new page load could for example
  // result in a download or a 'no content' response (e.g., a mailto: URL).
  SetPendingEntry(std::move(entry));
  NavigateToPendingEntry(NO_RELOAD);
}

void NavigationControllerImpl::SetPendingEntry(
    std::unique_ptr<NavigationEntryImpl> entry) {
  DiscardNonCommittedEntriesInternal();
  pending_entry_ = entry.release();
  NotificationService::current()->Notify(
      NOTIFICATION_NAV_ENTRY_PENDING,
      Source<NavigationController>(this),
      Details<NavigationEntry>(pending_entry_));
}

NavigationEntryImpl* NavigationControllerImpl::GetActiveEntry() const {
  if (transient_entry_index_ != -1)
    return entries_[transient_entry_index_].get();
  if (pending_entry_)
    return pending_entry_;
  return GetLastCommittedEntry();
}

NavigationEntryImpl* NavigationControllerImpl::GetVisibleEntry() const {
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
      pending_entry_index_ == -1 &&
      // Require either browser-initiated or an unmodified new tab.
      (!pending_entry_->is_renderer_initiated() || IsUnmodifiedBlankTab());

  // Also allow showing the pending entry for history navigations in a new tab,
  // such as Ctrl+Back.  In this case, no existing page is visible and no one
  // can script the new tab before it commits.
  if (!safe_to_show_pending &&
      pending_entry_ &&
      pending_entry_index_ != -1 &&
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

NavigationEntryImpl* NavigationControllerImpl::GetLastCommittedEntry() const {
  if (last_committed_entry_index_ == -1)
    return NULL;
  return entries_[last_committed_entry_index_].get();
}

bool NavigationControllerImpl::CanViewSource() const {
  const std::string& mime_type = delegate_->GetContentsMimeType();
  bool is_viewable_mime_type =
      mime_util::IsSupportedNonImageMimeType(mime_type) &&
      !media::IsSupportedMediaMimeType(mime_type);
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

NavigationEntryImpl* NavigationControllerImpl::GetEntryAtIndex(
    int index) const {
  if (index < 0 || index >= GetEntryCount())
    return nullptr;

  return entries_[index].get();
}

NavigationEntryImpl* NavigationControllerImpl::GetEntryAtOffset(
    int offset) const {
  return GetEntryAtIndex(GetIndexForOffset(offset));
}

int NavigationControllerImpl::GetIndexForOffset(int offset) const {
  return GetCurrentEntryIndex() + offset;
}

void NavigationControllerImpl::TakeScreenshot() {
  screenshot_manager_->TakeScreenshot();
}

void NavigationControllerImpl::SetScreenshotManager(
    std::unique_ptr<NavigationEntryScreenshotManager> manager) {
  if (manager.get())
    screenshot_manager_ = std::move(manager);
  else
    screenshot_manager_.reset(new NavigationEntryScreenshotManager(this));
}

bool NavigationControllerImpl::CanGoBack() const {
  return CanGoToOffset(-1);
}

bool NavigationControllerImpl::CanGoForward() const {
  return CanGoToOffset(1);
}

bool NavigationControllerImpl::CanGoToOffset(int offset) const {
  int index = GetIndexForOffset(offset);
  return index >= 0 && index < GetEntryCount();
}

void NavigationControllerImpl::GoBack() {
  // Call GoToIndex rather than GoToOffset to get the NOTREACHED() check.
  GoToIndex(GetIndexForOffset(-1));
}

void NavigationControllerImpl::GoForward() {
  // Call GoToIndex rather than GoToOffset to get the NOTREACHED() check.
  GoToIndex(GetIndexForOffset(1));
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
      ui::PageTransitionFromInt(
          entries_[pending_entry_index_]->GetTransitionType() |
          ui::PAGE_TRANSITION_FORWARD_BACK));
  NavigateToPendingEntry(NO_RELOAD);
}

void NavigationControllerImpl::GoToOffset(int offset) {
  // Note: This is actually reached in unit tests.
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
    ui::PageTransition transition,
    const std::string& extra_headers) {
  LoadURLParams params(url);
  params.referrer = referrer;
  params.transition_type = transition;
  params.extra_headers = extra_headers;
  LoadURLWithParams(params);
}

void NavigationControllerImpl::LoadURLWithParams(const LoadURLParams& params) {
  TRACE_EVENT1("browser,navigation",
               "NavigationControllerImpl::LoadURLWithParams",
               "url", params.url.possibly_invalid_spec());
  if (HandleDebugURL(params.url, params.transition_type)) {
    // If Telemetry is running, allow the URL load to proceed as if it's
    // unhandled, otherwise Telemetry can't tell if Navigation completed.
    if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
            cc::switches::kEnableGpuBenchmarking))
      return;
  }

  // Checks based on params.load_type.
  switch (params.load_type) {
    case LOAD_TYPE_DEFAULT:
      break;
    case LOAD_TYPE_HTTP_POST:
      // TODO(lukasza): This assertion is false - it is also possible to POST to
      // an chrome-extension://... URI.  This might be more common when
      // allowing renderer-initiated POST after fixing https://crbug.com/344348.
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

  std::unique_ptr<NavigationEntryImpl> entry;

  // For subframes, create a pending entry with a corresponding frame entry.
  int frame_tree_node_id = params.frame_tree_node_id;
  if (frame_tree_node_id != -1 || !params.frame_name.empty()) {
    FrameTreeNode* node =
        params.frame_tree_node_id != -1
            ? delegate_->GetFrameTree()->FindByID(params.frame_tree_node_id)
            : delegate_->GetFrameTree()->FindByName(params.frame_name);
    if (node && !node->IsMainFrame()) {
      DCHECK(GetLastCommittedEntry());

      // Update the FTN ID to use below in case we found a named frame.
      frame_tree_node_id = node->frame_tree_node_id();

      // In --site-per-process, create an identical NavigationEntry with a
      // new FrameNavigationEntry for the target subframe.
      if (SiteIsolationPolicy::UseSubframeNavigationEntries()) {
        entry = GetLastCommittedEntry()->Clone();
        entry->SetPageID(-1);
        entry->AddOrUpdateFrameEntry(
            node, -1, -1, nullptr,
            static_cast<SiteInstanceImpl*>(params.source_site_instance.get()),
            params.url, params.referrer, PageState(), "GET", -1);
      }
    }
  }

  // Otherwise, create a pending entry for the main frame.
  if (!entry) {
    entry = NavigationEntryImpl::FromNavigationEntry(CreateNavigationEntry(
        params.url, params.referrer, params.transition_type,
        params.is_renderer_initiated, params.extra_headers, browser_context_));
    entry->set_source_site_instance(
        static_cast<SiteInstanceImpl*>(params.source_site_instance.get()));
  }

  // Set the FTN ID (only used in non-site-per-process, for tests).
  entry->set_frame_tree_node_id(frame_tree_node_id);
  if (params.redirect_chain.size() > 0)
    entry->SetRedirectChain(params.redirect_chain);
  // Don't allow an entry replacement if there is no entry to replace.
  // http://crbug.com/457149
  if (params.should_replace_current_entry && entries_.size() > 0)
    entry->set_should_replace_entry(true);
  entry->set_should_clear_history_list(params.should_clear_history_list);
  entry->SetIsOverridingUserAgent(override);
  entry->set_transferred_global_request_id(
      params.transferred_global_request_id);

#if defined(OS_ANDROID)
  if (params.intent_received_timestamp > 0) {
    entry->set_intent_received_timestamp(
        base::TimeTicks() +
        base::TimeDelta::FromMilliseconds(params.intent_received_timestamp));
  }
  entry->set_has_user_gesture(params.has_user_gesture);
#endif

  switch (params.load_type) {
    case LOAD_TYPE_DEFAULT:
      break;
    case LOAD_TYPE_HTTP_POST:
      entry->SetHasPostData(true);
      entry->SetPostData(params.post_data);
      break;
    case LOAD_TYPE_DATA:
      entry->SetBaseURLForDataURL(params.base_url_for_data_url);
      entry->SetVirtualURL(params.virtual_url_for_data_url);
#if defined(OS_ANDROID)
      entry->SetDataURLAsString(params.data_url_as_string);
#endif
      entry->SetCanLoadLocalResources(params.can_load_local_resources);
      break;
    default:
      NOTREACHED();
      break;
  };

  LoadEntry(std::move(entry));
}

bool NavigationControllerImpl::PendingEntryMatchesHandle(
    NavigationHandleImpl* handle) const {
  return pending_entry_ &&
         pending_entry_->GetUniqueID() == handle->pending_nav_entry_id();
}

bool NavigationControllerImpl::RendererDidNavigate(
    RenderFrameHostImpl* rfh,
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

  // If there is a pending entry at this point, it should have a SiteInstance,
  // except for restored entries.
  DCHECK(pending_entry_index_ == -1 ||
         pending_entry_->site_instance() ||
         pending_entry_->restore_type() != NavigationEntryImpl::RESTORE_NONE);
  if (pending_entry_ &&
      pending_entry_->restore_type() != NavigationEntryImpl::RESTORE_NONE)
    pending_entry_->set_restore_type(NavigationEntryImpl::RESTORE_NONE);

  // The renderer tells us whether the navigation replaces the current entry.
  details->did_replace_entry = params.should_replace_current_entry;

  // Do navigation-type specific actions. These will make and commit an entry.
  details->type = ClassifyNavigation(rfh, params);

  // is_in_page must be computed before the entry gets committed.
  details->is_in_page = IsURLInPageNavigation(params.url, params.origin,
                                              params.was_within_same_page, rfh);

  switch (details->type) {
    case NAVIGATION_TYPE_NEW_PAGE:
      RendererDidNavigateToNewPage(rfh, params, details->did_replace_entry);
      break;
    case NAVIGATION_TYPE_EXISTING_PAGE:
      details->did_replace_entry = details->is_in_page;
      RendererDidNavigateToExistingPage(rfh, params);
      break;
    case NAVIGATION_TYPE_SAME_PAGE:
      RendererDidNavigateToSamePage(rfh, params);
      break;
    case NAVIGATION_TYPE_NEW_SUBFRAME:
      RendererDidNavigateNewSubframe(rfh, params, details->did_replace_entry);
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
  NavigationEntryImpl* active_entry = GetLastCommittedEntry();
  active_entry->SetTimestamp(timestamp);
  active_entry->SetHttpStatusCode(params.http_status_code);

  FrameNavigationEntry* frame_entry =
      active_entry->GetFrameEntry(rfh->frame_tree_node());
  if (SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    // Update the frame-specific PageState.
    // We may not find a frame_entry in some cases; ignore the PageState if so.
    // TODO(creis): Remove the "if" once https://crbug.com/522193 is fixed.
    if (frame_entry)
      frame_entry->set_page_state(params.page_state);
  } else {
    active_entry->SetPageState(params.page_state);
  }
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
  active_entry->ResetForCommit(frame_entry);

  // The active entry's SiteInstance should match our SiteInstance.
  // TODO(creis): This check won't pass for subframes until we create entries
  // for subframe navigations.
  if (!rfh->GetParent())
    CHECK_EQ(active_entry->site_instance(), rfh->GetSiteInstance());

  // Remember the bindings the renderer process has at this point, so that
  // we do not grant this entry additional bindings if we come back to it.
  active_entry->SetBindings(rfh->GetEnabledBindings());

  // Now prep the rest of the details for the notification and broadcast.
  details->entry = active_entry;
  details->is_main_frame = !rfh->GetParent();
  details->http_status_code = params.http_status_code;

  // Deserialize the security info and kill the renderer if
  // deserialization fails. The navigation will continue with default
  // SSLStatus values.
  if (!DeserializeSecurityInfo(params.security_info, &details->ssl_status)) {
    bad_message::ReceivedBadMessage(
        rfh->GetProcess(),
        bad_message::WC_RENDERER_DID_NAVIGATE_BAD_SECURITY_INFO);
  }

  NotifyNavigationEntryCommitted(details);

  // Update the nav_entry_id for each RenderFrameHost in the tree, so that each
  // one knows the latest NavigationEntry it is showing (whether it has
  // committed anything in this navigation or not). This allows things like
  // state and title updates from RenderFrames to apply to the latest relevant
  // NavigationEntry.
  int nav_entry_id = active_entry->GetUniqueID();
  for (FrameTreeNode* node : delegate_->GetFrameTree()->Nodes())
    node->current_frame_host()->set_nav_entry_id(nav_entry_id);
  return true;
}

NavigationType NavigationControllerImpl::ClassifyNavigation(
    RenderFrameHostImpl* rfh,
    const FrameHostMsg_DidCommitProvisionalLoad_Params& params) const {
  if (params.did_create_new_entry) {
    // A new entry. We may or may not have a pending entry for the page, and
    // this may or may not be the main frame.
    if (!rfh->GetParent()) {
      return NAVIGATION_TYPE_NEW_PAGE;
    }

    // When this is a new subframe navigation, we should have a committed page
    // in which it's a subframe. This may not be the case when an iframe is
    // navigated on a popup navigated to about:blank (the iframe would be
    // written into the popup by script on the main page). For these cases,
    // there isn't any navigation stuff we can do, so just ignore it.
    if (!GetLastCommittedEntry())
      return NAVIGATION_TYPE_NAV_IGNORE;

    // Valid subframe navigation.
    return NAVIGATION_TYPE_NEW_SUBFRAME;
  }

  // Cross-process location.replace navigations should be classified as New with
  // replacement rather than ExistingPage, since it is not safe to reuse the
  // NavigationEntry.
  // TODO(creis): Have the renderer classify location.replace as
  // did_create_new_entry for all cases and eliminate this special case.  This
  // requires updating several test expectations.  See https://crbug.com/596707.
  if (!rfh->GetParent() && GetLastCommittedEntry() &&
      GetLastCommittedEntry()->site_instance() != rfh->GetSiteInstance() &&
      params.should_replace_current_entry) {
    return NAVIGATION_TYPE_NEW_PAGE;
  }

  // We only clear the session history when navigating to a new page.
  DCHECK(!params.history_list_was_cleared);

  if (rfh->GetParent()) {
    // All manual subframes would be did_create_new_entry and handled above, so
    // we know this is auto.
    if (GetLastCommittedEntry()) {
      return NAVIGATION_TYPE_AUTO_SUBFRAME;
    } else {
      // We ignore subframes created in non-committed pages; we'd appreciate if
      // people stopped doing that.
      return NAVIGATION_TYPE_NAV_IGNORE;
    }
  }

  if (params.nav_entry_id == 0) {
    // This is a renderer-initiated navigation (nav_entry_id == 0), but didn't
    // create a new page.

    // Just like above in the did_create_new_entry case, it's possible to
    // scribble onto an uncommitted page. Again, there isn't any navigation
    // stuff that we can do, so ignore it here as well.
    NavigationEntry* last_committed = GetLastCommittedEntry();
    if (!last_committed)
      return NAVIGATION_TYPE_NAV_IGNORE;

    // This is history.replaceState(), history.reload(), or a client-side
    // redirect.
    return NAVIGATION_TYPE_EXISTING_PAGE;
  }

  if (pending_entry_ && pending_entry_index_ == -1 &&
      pending_entry_->GetUniqueID() == params.nav_entry_id) {
    // In this case, we have a pending entry for a load of a new URL but Blink
    // didn't do a new navigation (params.did_create_new_entry). First check to
    // make sure Blink didn't treat a new cross-process navigation as inert, and
    // thus set params.did_create_new_entry to false. In that case, we must
    // treat it as NEW since the SiteInstance doesn't match the entry.
    if (GetLastCommittedEntry()->site_instance() != rfh->GetSiteInstance())
      return NAVIGATION_TYPE_NEW_PAGE;

    // Otherwise, this happens when you press enter in the URL bar to reload. We
    // will create a pending entry, but Blink will convert it to a reload since
    // it's the same page and not create a new entry for it (the user doesn't
    // want to have a new back/forward entry when they do this). Therefore we
    // want to just ignore the pending entry and go back to where we were (the
    // "existing entry").
    // TODO(creis,avi): Eliminate SAME_PAGE in https://crbug.com/536102.
    return NAVIGATION_TYPE_SAME_PAGE;
  }

  if (params.intended_as_new_entry) {
    // This was intended to be a navigation to a new entry but the pending entry
    // got cleared in the meanwhile. Classify as EXISTING_PAGE because we may or
    // may not have a pending entry.
    return NAVIGATION_TYPE_EXISTING_PAGE;
  }

  if (params.url_is_unreachable && failed_pending_entry_id_ != 0 &&
      params.nav_entry_id == failed_pending_entry_id_) {
    // If the renderer was going to a new pending entry that got cleared because
    // of an error, this is the case of the user trying to retry a failed load
    // by pressing return. Classify as EXISTING_PAGE because we probably don't
    // have a pending entry.
    return NAVIGATION_TYPE_EXISTING_PAGE;
  }

  // Now we know that the notification is for an existing page. Find that entry.
  int existing_entry_index = GetEntryIndexWithUniqueID(params.nav_entry_id);
  if (existing_entry_index == -1) {
    // The renderer has committed a navigation to an entry that no longer
    // exists. Because the renderer is showing that page, resurrect that entry.
    return NAVIGATION_TYPE_NEW_PAGE;
  }

  // Since we weeded out "new" navigations above, we know this is an existing
  // (back/forward) navigation.
  return NAVIGATION_TYPE_EXISTING_PAGE;
}

void NavigationControllerImpl::RendererDidNavigateToNewPage(
    RenderFrameHostImpl* rfh,
    const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
    bool replace_entry) {
  std::unique_ptr<NavigationEntryImpl> new_entry;
  bool update_virtual_url;
  // Only make a copy of the pending entry if it is appropriate for the new page
  // that was just loaded. Verify this by checking if the entry corresponds
  // to the current navigation handle. Note that in some tests the render frame
  // host does not have a valid handle. Additionally, coarsely check that:
  // 1. The SiteInstance hasn't been assigned to something else.
  // 2. The pending entry was intended as a new entry, rather than being a
  // history navigation that was interrupted by an unrelated,
  // renderer-initiated navigation.
  // TODO(csharrison): Investigate whether we can remove some of the coarser
  // checks.
  NavigationHandleImpl* handle = rfh->navigation_handle();
  DCHECK(handle);
  if (PendingEntryMatchesHandle(handle) && pending_entry_index_ == -1 &&
      (!pending_entry_->site_instance() ||
       pending_entry_->site_instance() == rfh->GetSiteInstance())) {
    new_entry = pending_entry_->Clone();

    update_virtual_url = new_entry->update_virtual_url_with_url();
  } else {
    new_entry = base::WrapUnique(new NavigationEntryImpl);

    // Find out whether the new entry needs to update its virtual URL on URL
    // change and set up the entry accordingly. This is needed to correctly
    // update the virtual URL when replaceState is called after a pushState.
    GURL url = params.url;
    bool needs_update = false;
    BrowserURLHandlerImpl::GetInstance()->RewriteURLIfNecessary(
        &url, browser_context_, &needs_update);
    new_entry->set_update_virtual_url_with_url(needs_update);

    // When navigating to a new page, give the browser URL handler a chance to
    // update the virtual URL based on the new URL. For example, this is needed
    // to show chrome://bookmarks/#1 when the bookmarks webui extension changes
    // the URL.
    update_virtual_url = needs_update;
  }

  // Don't use the page type from the pending entry. Some interstitial page
  // may have set the type to interstitial. Once we commit, however, the page
  // type must always be normal or error.
  new_entry->set_page_type(params.url_is_unreachable ? PAGE_TYPE_ERROR
                                                     : PAGE_TYPE_NORMAL);
  new_entry->SetURL(params.url);
  if (update_virtual_url)
    UpdateVirtualURLToURL(new_entry.get(), params.url);
  new_entry->SetReferrer(params.referrer);
  new_entry->SetPageID(params.page_id);
  new_entry->SetTransitionType(params.transition);
  new_entry->set_site_instance(
      static_cast<SiteInstanceImpl*>(rfh->GetSiteInstance()));
  new_entry->SetOriginalRequestURL(params.original_request_url);
  new_entry->SetIsOverridingUserAgent(params.is_overriding_user_agent);

  // Update the FrameNavigationEntry for new main frame commits.
  FrameNavigationEntry* frame_entry =
      new_entry->GetFrameEntry(rfh->frame_tree_node());
  frame_entry->set_frame_unique_name(params.frame_unique_name);
  frame_entry->set_item_sequence_number(params.item_sequence_number);
  frame_entry->set_document_sequence_number(params.document_sequence_number);
  frame_entry->set_method(params.method);
  frame_entry->set_post_id(params.post_id);

  // history.pushState() is classified as a navigation to a new page, but
  // sets was_within_same_page to true. In this case, we already have the
  // title and favicon available, so set them immediately.
  if (params.was_within_same_page && GetLastCommittedEntry()) {
    new_entry->SetTitle(GetLastCommittedEntry()->GetTitle());
    new_entry->GetFavicon() = GetLastCommittedEntry()->GetFavicon();
  }

  DCHECK(!params.history_list_was_cleared || !replace_entry);
  // The browser requested to clear the session history when it initiated the
  // navigation. Now we know that the renderer has updated its state accordingly
  // and it is safe to also clear the browser side history.
  if (params.history_list_was_cleared) {
    DiscardNonCommittedEntriesInternal();
    entries_.clear();
    last_committed_entry_index_ = -1;
  }

  InsertOrReplaceEntry(std::move(new_entry), replace_entry);
}

void NavigationControllerImpl::RendererDidNavigateToExistingPage(
    RenderFrameHostImpl* rfh,
    const FrameHostMsg_DidCommitProvisionalLoad_Params& params) {
  // We should only get here for main frame navigations.
  DCHECK(!rfh->GetParent());

  // TODO(creis): Classify location.replace as NEW_PAGE instead of EXISTING_PAGE
  // in https://crbug.com/596707.

  NavigationEntryImpl* entry;
  if (params.intended_as_new_entry) {
    // This was intended as a new entry but the pending entry was lost in the
    // meanwhile and no new page was created. We are stuck at the last committed
    // entry.
    entry = GetLastCommittedEntry();
  } else if (params.nav_entry_id) {
    // This is a browser-initiated navigation (back/forward/reload).
    entry = GetEntryWithUniqueID(params.nav_entry_id);
  } else {
    // This is renderer-initiated. The only kinds of renderer-initated
    // navigations that are EXISTING_PAGE are reloads and location.replace,
    // which land us at the last committed entry.
    entry = GetLastCommittedEntry();
  }
  DCHECK(entry);

  // The URL may have changed due to redirects.
  entry->set_page_type(params.url_is_unreachable ? PAGE_TYPE_ERROR
                                                 : PAGE_TYPE_NORMAL);
  entry->SetURL(params.url);
  entry->SetReferrer(params.referrer);
  if (entry->update_virtual_url_with_url())
    UpdateVirtualURLToURL(entry, params.url);

  // The site instance will normally be the same except during session restore,
  // when no site instance will be assigned.
  DCHECK(entry->site_instance() == nullptr ||
         entry->site_instance() == rfh->GetSiteInstance());

  // Update the existing FrameNavigationEntry to ensure all of its members
  // reflect the parameters coming from the renderer process.
  entry->AddOrUpdateFrameEntry(
      rfh->frame_tree_node(), params.item_sequence_number,
      params.document_sequence_number, rfh->GetSiteInstance(), nullptr,
      params.url, params.referrer, params.page_state, params.method,
      params.post_id);

  // The redirected to page should not inherit the favicon from the previous
  // page.
  if (ui::PageTransitionIsRedirect(params.transition))
    entry->GetFavicon() = FaviconStatus();

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
  last_committed_entry_index_ = GetIndexOfEntry(entry);
}

void NavigationControllerImpl::RendererDidNavigateToSamePage(
    RenderFrameHostImpl* rfh,
    const FrameHostMsg_DidCommitProvisionalLoad_Params& params) {
  // This classification says that we have a pending entry that's the same as
  // the last committed entry. This entry is guaranteed to exist by
  // ClassifyNavigation. All we need to do is update the existing entry.
  NavigationEntryImpl* existing_entry = GetLastCommittedEntry();

  // If we classified this correctly, the SiteInstance should not have changed.
  CHECK_EQ(existing_entry->site_instance(), rfh->GetSiteInstance());

  // We assign the entry's unique ID to be that of the new one. Since this is
  // always the result of a user action, we want to dismiss infobars, etc. like
  // a regular user-initiated navigation.
  DCHECK_EQ(pending_entry_->GetUniqueID(), params.nav_entry_id);
  existing_entry->set_unique_id(pending_entry_->GetUniqueID());

  // The URL may have changed due to redirects.
  existing_entry->set_page_type(params.url_is_unreachable ? PAGE_TYPE_ERROR
                                                          : PAGE_TYPE_NORMAL);
  if (existing_entry->update_virtual_url_with_url())
    UpdateVirtualURLToURL(existing_entry, params.url);
  existing_entry->SetURL(params.url);

  // Update the existing FrameNavigationEntry to ensure all of its members
  // reflect the parameters coming from the renderer process.
  existing_entry->AddOrUpdateFrameEntry(
      rfh->frame_tree_node(), params.item_sequence_number,
      params.document_sequence_number, rfh->GetSiteInstance(), nullptr,
      params.url, params.referrer, params.page_state, params.method,
      params.post_id);

  DiscardNonCommittedEntries();
}

void NavigationControllerImpl::RendererDidNavigateNewSubframe(
    RenderFrameHostImpl* rfh,
    const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
    bool replace_entry) {
  DCHECK(ui::PageTransitionCoreTypeIs(params.transition,
                                      ui::PAGE_TRANSITION_MANUAL_SUBFRAME));

  // Manual subframe navigations just get the current entry cloned so the user
  // can go back or forward to it. The actual subframe information will be
  // stored in the page state for each of those entries. This happens out of
  // band with the actual navigations.
  DCHECK(GetLastCommittedEntry()) << "ClassifyNavigation should guarantee "
                                  << "that a last committed entry exists.";

  std::unique_ptr<NavigationEntryImpl> new_entry;
  if (SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    // Make sure we don't leak frame_entry if new_entry doesn't take ownership.
    scoped_refptr<FrameNavigationEntry> frame_entry(new FrameNavigationEntry(
        params.frame_unique_name, params.item_sequence_number,
        params.document_sequence_number, rfh->GetSiteInstance(), nullptr,
        params.url, params.referrer, params.method, params.post_id));
    new_entry = GetLastCommittedEntry()->CloneAndReplace(rfh->frame_tree_node(),
                                                         frame_entry.get());

    // TODO(creis): Update this to add the frame_entry if we can't find the one
    // to replace, which can happen due to a unique name change.  See
    // https://crbug.com/607205.  For now, frame_entry will be deleted when it
    // goes out of scope if it doesn't get used.
  } else {
    new_entry = GetLastCommittedEntry()->Clone();
  }

  new_entry->SetPageID(params.page_id);
  InsertOrReplaceEntry(std::move(new_entry), replace_entry);
}

bool NavigationControllerImpl::RendererDidNavigateAutoSubframe(
    RenderFrameHostImpl* rfh,
    const FrameHostMsg_DidCommitProvisionalLoad_Params& params) {
  DCHECK(ui::PageTransitionCoreTypeIs(params.transition,
                                      ui::PAGE_TRANSITION_AUTO_SUBFRAME));

  // We're guaranteed to have a previously committed entry, and we now need to
  // handle navigation inside of a subframe in it without creating a new entry.
  DCHECK(GetLastCommittedEntry());

  if (params.nav_entry_id) {
    int entry_index = GetEntryIndexWithUniqueID(params.nav_entry_id);

    // If the |nav_entry_id| is non-zero and matches an existing entry, this is
    // a history auto" navigation.  Update the last committed index accordingly.
    // If we don't recognize the |nav_entry_id|, it might be either a pending
    // entry for a transfer or a recently pruned entry.  We'll handle it below.
    if (entry_index != -1 && entry_index != last_committed_entry_index_) {
      // Make sure that a subframe commit isn't changing the main frame's
      // origin. Otherwise the renderer process may be confused, leading to a
      // URL spoof. We can't check the path since that may change
      // (https://crbug.com/373041).
      // TODO(creis): For now, restrict this check to HTTP(S) origins, because
      // about:blank, file, and unique origins are more subtle to get right.
      // We'll abstract out the relevant checks from IsURLInPageNavigation and
      // share them here.  See https://crbug.com/618104.
      const GURL& dest_top_url = GetEntryAtIndex(entry_index)->GetURL();
      const GURL& current_top_url = GetLastCommittedEntry()->GetURL();
      if (current_top_url.SchemeIsHTTPOrHTTPS() &&
          dest_top_url.SchemeIsHTTPOrHTTPS() &&
          current_top_url.GetOrigin() != dest_top_url.GetOrigin()) {
        bad_message::ReceivedBadMessage(rfh->GetProcess(),
                                        bad_message::NC_AUTO_SUBFRAME);
      }

      // TODO(creis): Update the FrameNavigationEntry in --site-per-process.
      last_committed_entry_index_ = entry_index;
      DiscardNonCommittedEntriesInternal();
      return true;
    }
  }

  if (SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    // This may be a "new auto" case where we add a new FrameNavigationEntry, or
    // it may be a "history auto" case where we update an existing one.
    NavigationEntryImpl* last_committed = GetLastCommittedEntry();
    last_committed->AddOrUpdateFrameEntry(
        rfh->frame_tree_node(), params.item_sequence_number,
        params.document_sequence_number, rfh->GetSiteInstance(), nullptr,
        params.url, params.referrer, params.page_state, params.method,
        params.post_id);

    // Cross-process subframe navigations may leave a pending entry around.
    // Clear it if it's actually for the subframe.
    // TODO(creis): Don't use pending entries for subframe navigations.
    // See https://crbug.com/495161.
    if (pending_entry_ &&
        pending_entry_->frame_tree_node_id() ==
            rfh->frame_tree_node()->frame_tree_node_id()) {
      DiscardPendingEntry(false);
    }
  }

  // We do not need to discard the pending entry in this case, since we will
  // not generate commit notifications for this auto-subframe navigation.
  return false;
}

int NavigationControllerImpl::GetIndexOfEntry(
    const NavigationEntryImpl* entry) const {
  for (size_t i = 0; i < entries_.size(); ++i) {
    if (entries_[i].get() == entry)
      return i;
  }
  return -1;
}

// There are two general cases where a navigation is "in page":
// 1. A fragment navigation, in which the url is kept the same except for the
//    reference fragment.
// 2. A history API navigation (pushState and replaceState). This case is
//    always in-page, but the urls are not guaranteed to match excluding the
//    fragment. The relevant spec allows pushState/replaceState to any URL on
//    the same origin.
// However, due to reloads, even identical urls are *not* guaranteed to be
// in-page navigations, we have to trust the renderer almost entirely.
// The one thing we do know is that cross-origin navigations will *never* be
// in-page. Therefore, trust the renderer if the URLs are on the same origin,
// and assume the renderer is malicious if a cross-origin navigation claims to
// be in-page.
//
// TODO(creis): Clean up and simplify the about:blank and origin checks below,
// which are likely redundant with each other.  Be careful about data URLs vs
// about:blank, both of which are unique origins and thus not considered equal.
bool NavigationControllerImpl::IsURLInPageNavigation(
    const GURL& url,
    const url::Origin& origin,
    bool renderer_says_in_page,
    RenderFrameHost* rfh) const {
  RenderFrameHostImpl* rfhi = static_cast<RenderFrameHostImpl*>(rfh);
  GURL last_committed_url;
  if (rfh->GetParent()) {
    // Use the FrameTreeNode's current_url and not rfh->GetLastCommittedURL(),
    // which might be empty in a new RenderFrameHost after a process swap.
    // Here, we care about the last committed URL in the FrameTreeNode,
    // regardless of which process it is in.
    last_committed_url = rfhi->frame_tree_node()->current_url();
  } else {
    NavigationEntry* last_committed = GetLastCommittedEntry();
    // There must be a last-committed entry to compare URLs to. TODO(avi): When
    // might Blink say that a navigation is in-page yet there be no last-
    // committed entry?
    if (!last_committed)
      return false;
    last_committed_url = last_committed->GetURL();
  }

  WebPreferences prefs = rfh->GetRenderViewHost()->GetWebkitPreferences();
  const url::Origin& committed_origin =
      rfhi->frame_tree_node()->current_origin();
  bool is_same_origin = last_committed_url.is_empty() ||
                        // TODO(japhet): We should only permit navigations
                        // originating from about:blank to be in-page if the
                        // about:blank is the first document that frame loaded.
                        // We don't have sufficient information to identify
                        // that case at the moment, so always allow about:blank
                        // for now.
                        last_committed_url == GURL(url::kAboutBlankURL) ||
                        last_committed_url.GetOrigin() == url.GetOrigin() ||
                        committed_origin == origin ||
                        !prefs.web_security_enabled ||
                        (prefs.allow_universal_access_from_file_urls &&
                         committed_origin.scheme() == url::kFileScheme);
  if (!is_same_origin && renderer_says_in_page) {
    bad_message::ReceivedBadMessage(rfh->GetProcess(),
                                    bad_message::NC_IN_PAGE_NAVIGATION);
  }
  return is_same_origin && renderer_says_in_page;
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

  // Remove all the entries leaving the last committed entry.
  PruneAllButLastCommittedInternal();

  // We now have one entry, possibly with a new pending entry.  Ensure that
  // adding the entries from source won't put us over the limit.
  DCHECK_EQ(1, GetEntryCount());
  if (!replace_entry)
    source->PruneOldestEntryIfFull();

  // Insert the entries from source. Don't use source->GetCurrentEntryIndex as
  // we don't want to copy over the transient entry. Ignore any pending entry,
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

  delegate_->SetHistoryOffsetAndLength(last_committed_entry_index_,
                                       GetEntryCount());

  // Copy the max page id map from the old tab to the new tab. This ensures that
  // new and existing navigations in the tab's current SiteInstances are
  // identified properly.
  NavigationEntryImpl* last_committed = GetLastCommittedEntry();
  int32_t site_max_page_id =
      delegate_->GetMaxPageIDForSiteInstance(last_committed->site_instance());
  delegate_->CopyMaxPageIDsFrom(source->delegate()->GetWebContents());
  delegate_->UpdateMaxPageIDForSiteInstance(last_committed->site_instance(),
                                            site_max_page_id);
  max_restored_page_id_ = source->max_restored_page_id_;
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

  DCHECK_EQ(0, last_committed_entry_index_);
  DCHECK_EQ(1, GetEntryCount());

  delegate_->SetHistoryOffsetAndLength(last_committed_entry_index_,
                                       GetEntryCount());
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

void NavigationControllerImpl::SetMaxRestoredPageID(int32_t max_id) {
  max_restored_page_id_ = max_id;
}

int32_t NavigationControllerImpl::GetMaxRestoredPageID() const {
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

  // TODO(ajwong): Should this use the |partition_id| directly rather than
  // re-lookup via |instance|?  http://crbug.com/142685
  StoragePartition* partition =
      BrowserContext::GetStoragePartition(browser_context_, instance);
  DOMStorageContextWrapper* context_wrapper =
      static_cast<DOMStorageContextWrapper*>(partition->GetDOMStorageContext());

  SessionStorageNamespaceMap::const_iterator it =
      session_storage_namespace_map_.find(partition_id);
  if (it != session_storage_namespace_map_.end()) {
    // Ensure that this namespace actually belongs to this partition.
    DCHECK(static_cast<SessionStorageNamespaceImpl*>(it->second.get())->
        IsFromContext(context_wrapper));
    return it->second.get();
  }

  // Create one if no one has accessed session storage for this partition yet.
  SessionStorageNamespaceImpl* session_storage_namespace =
      new SessionStorageNamespaceImpl(context_wrapper);
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

  if (last_committed_entry_index_ != -1) {
    entries_[last_committed_entry_index_]->SetTransitionType(
        ui::PAGE_TRANSITION_RELOAD);
  }
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
    delegate_->NotifyNavigationStateChanged(INVALIDATE_TYPE_ALL);
  }
}

NavigationEntryImpl* NavigationControllerImpl::GetPendingEntry() const {
  return pending_entry_;
}

int NavigationControllerImpl::GetPendingEntryIndex() const {
  return pending_entry_index_;
}

void NavigationControllerImpl::InsertOrReplaceEntry(
    std::unique_ptr<NavigationEntryImpl> entry,
    bool replace) {
  DCHECK(!ui::PageTransitionCoreTypeIs(entry->GetTransitionType(),
                                       ui::PAGE_TRANSITION_AUTO_SUBFRAME));

  // If the pending_entry_index_ is -1, the navigation was to a new page, and we
  // need to keep continuity with the pending entry, so copy the pending entry's
  // unique ID to the committed entry. If the pending_entry_index_ isn't -1,
  // then the renderer navigated on its own, independent of the pending entry,
  // so don't copy anything.
  if (pending_entry_ && pending_entry_index_ == -1)
    entry->set_unique_id(pending_entry_->GetUniqueID());

  DiscardNonCommittedEntriesInternal();

  int current_size = static_cast<int>(entries_.size());

  // When replacing, don't prune the forward history.
  if (replace && current_size > 0) {
    int32_t page_id = entry->GetPageID();

    entries_[last_committed_entry_index_] = std::move(entry);

    // This is a new page ID, so we need everybody to know about it.
    delegate_->UpdateMaxPageID(page_id);
    return;
  }

  // We shouldn't see replace == true when there's no committed entries.
  DCHECK(!replace);

  if (current_size > 0) {
    // Prune any entries which are in front of the current entry.
    // last_committed_entry_index_ must be updated here since calls to
    // NotifyPrunedEntries() below may re-enter and we must make sure
    // last_committed_entry_index_ is not left in an invalid state.
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

  int32_t page_id = entry->GetPageID();
  entries_.push_back(std::move(entry));
  last_committed_entry_index_ = static_cast<int>(entries_.size()) - 1;

  // This is a new page ID, so we need everybody to know about it.
  delegate_->UpdateMaxPageID(page_id);
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
          ui::PAGE_TRANSITION_FORWARD_BACK)) {
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
    CHECK_NE(pending_entry_index_, -1);
    pending_entry_ = entries_[pending_entry_index_].get();
  }

  // Any renderer-side debug URLs or javascript: URLs should be ignored if the
  // renderer process is not live, unless it is the initial navigation of the
  // tab.
  if (IsRendererDebugURL(pending_entry_->GetURL())) {
    // TODO(creis): Find the RVH for the correct frame.
    if (!delegate_->GetRenderViewHost()->IsRenderViewLive() &&
        !IsInitialNavigation()) {
      DiscardNonCommittedEntries();
      return;
    }
  }

  // This call does not support re-entrancy.  See http://crbug.com/347742.
  CHECK(!in_navigate_to_pending_entry_);
  in_navigate_to_pending_entry_ = true;
  bool success = NavigateToPendingEntryInternal(reload_type);
  in_navigate_to_pending_entry_ = false;

  if (!success)
    DiscardNonCommittedEntries();
}

bool NavigationControllerImpl::NavigateToPendingEntryInternal(
    ReloadType reload_type) {
  DCHECK(pending_entry_);
  FrameTreeNode* root = delegate_->GetFrameTree()->root();

  // In default Chrome, there are no subframe FrameNavigationEntries.  Either
  // navigate the main frame or use the main frame's FrameNavigationEntry to
  // tell the indicated frame where to go.
  if (!SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    FrameNavigationEntry* frame_entry = GetPendingEntry()->GetFrameEntry(root);
    FrameTreeNode* frame = root;
    int ftn_id = GetPendingEntry()->frame_tree_node_id();
    if (ftn_id != -1) {
      frame = delegate_->GetFrameTree()->FindByID(ftn_id);
      DCHECK(frame);
    }
    return frame->navigator()->NavigateToPendingEntry(frame, *frame_entry,
                                                      reload_type, false);
  }

  // In --site-per-process, we compare FrameNavigationEntries to see which
  // frames in the tree need to be navigated.
  FrameLoadVector same_document_loads;
  FrameLoadVector different_document_loads;
  if (GetLastCommittedEntry()) {
    FindFramesToNavigate(root, &same_document_loads, &different_document_loads);
  }

  if (same_document_loads.empty() && different_document_loads.empty()) {
    // If we don't have any frames to navigate at this point, either
    // (1) there is no previous history entry to compare against, or
    // (2) we were unable to match any frames by name. In the first case,
    // doing a different document navigation to the root item is the only valid
    // thing to do. In the second case, we should have been able to find a
    // frame to navigate based on names if this were a same document
    // navigation, so we can safely assume this is the different document case.
    different_document_loads.push_back(
        std::make_pair(root, pending_entry_->GetFrameEntry(root)));
  }

  // If all the frame loads fail, we will discard the pending entry.
  bool success = false;

  // Send all the same document frame loads before the different document loads.
  for (const auto& item : same_document_loads) {
    FrameTreeNode* frame = item.first;
    success |= frame->navigator()->NavigateToPendingEntry(frame, *item.second,
                                                          reload_type, true);
  }
  for (const auto& item : different_document_loads) {
    FrameTreeNode* frame = item.first;
    success |= frame->navigator()->NavigateToPendingEntry(frame, *item.second,
                                                          reload_type, false);
  }
  return success;
}

void NavigationControllerImpl::FindFramesToNavigate(
    FrameTreeNode* frame,
    FrameLoadVector* same_document_loads,
    FrameLoadVector* different_document_loads) {
  DCHECK(pending_entry_);
  DCHECK_GE(last_committed_entry_index_, 0);
  FrameNavigationEntry* new_item = pending_entry_->GetFrameEntry(frame);
  // TODO(creis): Store the last committed FrameNavigationEntry to use here,
  // rather than assuming the NavigationEntry has up to date info on subframes.
  FrameNavigationEntry* old_item =
      GetLastCommittedEntry()->GetFrameEntry(frame);
  if (!new_item)
    return;

  // Schedule a load in this frame if the new item isn't for the same item
  // sequence number in the same SiteInstance. Newly restored items may not have
  // a SiteInstance yet, in which case it will be assigned on first commit.
  if (!old_item ||
      new_item->item_sequence_number() != old_item->item_sequence_number() ||
      (new_item->site_instance() != nullptr &&
       new_item->site_instance() != old_item->site_instance())) {
    if (old_item &&
        new_item->document_sequence_number() ==
            old_item->document_sequence_number()) {
      same_document_loads->push_back(std::make_pair(frame, new_item));

      // TODO(avi, creis): This is a bug; we should not return here. Rather, we
      // should continue on and navigate all child frames which have also
      // changed. This bug is the cause of <https://crbug.com/542299>, which is
      // a NC_IN_PAGE_NAVIGATION renderer kill.
      //
      // However, this bug is a bandaid over a deeper and worse problem. Doing a
      // pushState immediately after loading a subframe is a race, one that no
      // web page author expects. If we fix this bug, many large websites break.
      // For example, see <https://crbug.com/598043> and the spec discussion at
      // <https://github.com/whatwg/html/issues/1191>.
      //
      // For now, we accept this bug, and hope to resolve the race in a
      // different way that will one day allow us to fix this.
      return;
    } else {
      different_document_loads->push_back(std::make_pair(frame, new_item));
      // For a different document, the subframes will be destroyed, so there's
      // no need to consider them.
      return;
    }
  }

  for (size_t i = 0; i < frame->child_count(); i++) {
    FindFramesToNavigate(frame->child_at(i), same_document_loads,
                         different_document_loads);
  }
}

void NavigationControllerImpl::NotifyNavigationEntryCommitted(
    LoadCommittedDetails* details) {
  details->entry = GetLastCommittedEntry();

  // We need to notify the ssl_manager_ before the web_contents_ so the
  // location bar will have up-to-date information about the security style
  // when it wants to draw.  See http://crbug.com/11157
  ssl_manager_.DidCommitProvisionalLoad(*details);

  delegate_->NotifyNavigationStateChanged(INVALIDATE_TYPE_ALL);
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
  if (pending_entry_) {
    NavigateToPendingEntry(NO_RELOAD);
  } else if (last_committed_entry_index_ != -1) {
    pending_entry_index_ = last_committed_entry_index_;
    NavigateToPendingEntry(NO_RELOAD);
  } else {
    // If there is something to reload, the successful reload will clear the
    // |needs_reload_| flag. Otherwise, just do it here.
    needs_reload_ = false;
  }
}

void NavigationControllerImpl::NotifyEntryChanged(
    const NavigationEntry* entry) {
  EntryChangedDetails det;
  det.changed_entry = entry;
  det.index = GetIndexOfEntry(
      NavigationEntryImpl::FromNavigationEntry(entry));
  NotificationService::current()->Notify(
      NOTIFICATION_NAV_ENTRY_CHANGED,
      Source<NavigationController>(this),
      Details<EntryChangedDetails>(&det));
}

void NavigationControllerImpl::FinishRestore(int selected_index,
                                             RestoreType type) {
  DCHECK(selected_index >= 0 && selected_index < GetEntryCount());
  ConfigureEntriesForRestore(&entries_, type);

  SetMaxRestoredPageID(static_cast<int32_t>(GetEntryCount()));

  last_committed_entry_index_ = selected_index;
}

void NavigationControllerImpl::DiscardNonCommittedEntriesInternal() {
  DiscardPendingEntry(false);
  DiscardTransientEntry();
}

void NavigationControllerImpl::DiscardPendingEntry(bool was_failure) {
  // It is not safe to call DiscardPendingEntry while NavigateToEntry is in
  // progress, since this will cause a use-after-free.  (We only allow this
  // when the tab is being destroyed for shutdown, since it won't return to
  // NavigateToEntry in that case.)  http://crbug.com/347742.
  CHECK(!in_navigate_to_pending_entry_ || delegate_->IsBeingDestroyed());

  if (was_failure && pending_entry_) {
    failed_pending_entry_id_ = pending_entry_->GetUniqueID();
  } else {
    failed_pending_entry_id_ = 0;
  }

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

int NavigationControllerImpl::GetEntryIndexWithPageID(SiteInstance* instance,
                                                      int32_t page_id) const {
  for (int i = static_cast<int>(entries_.size()) - 1; i >= 0; --i) {
    if ((entries_[i]->site_instance() == instance) &&
        (entries_[i]->GetPageID() == page_id))
      return i;
  }
  return -1;
}

int NavigationControllerImpl::GetEntryIndexWithUniqueID(
    int nav_entry_id) const {
  for (int i = static_cast<int>(entries_.size()) - 1; i >= 0; --i) {
    if (entries_[i]->GetUniqueID() == nav_entry_id)
      return i;
  }
  return -1;
}

NavigationEntryImpl* NavigationControllerImpl::GetTransientEntry() const {
  if (transient_entry_index_ == -1)
    return NULL;
  return entries_[transient_entry_index_].get();
}

void NavigationControllerImpl::SetTransientEntry(
    std::unique_ptr<NavigationEntry> entry) {
  // Discard any current transient entry, we can only have one at a time.
  int index = 0;
  if (last_committed_entry_index_ != -1)
    index = last_committed_entry_index_ + 1;
  DiscardTransientEntry();
  entries_.insert(entries_.begin() + index,
                  NavigationEntryImpl::FromNavigationEntry(std::move(entry)));
  transient_entry_index_ = index;
  delegate_->NotifyNavigationStateChanged(INVALIDATE_TYPE_ALL);
}

void NavigationControllerImpl::InsertEntriesFrom(
    const NavigationControllerImpl& source,
    int max_index) {
  DCHECK_LE(max_index, source.GetEntryCount());
  size_t insert_index = 0;
  for (int i = 0; i < max_index; i++) {
    // When cloning a tab, copy all entries except interstitial pages.
    if (source.entries_[i]->GetPageType() != PAGE_TYPE_INTERSTITIAL) {
      // TODO(creis): Once we start sharing FrameNavigationEntries between
      // NavigationEntries, it will not be safe to share them with another tab.
      // Must have a version of Clone that recreates them.
      entries_.insert(entries_.begin() + insert_index++,
                      source.entries_[i]->Clone());
    }
  }
}

void NavigationControllerImpl::SetGetTimestampCallbackForTest(
    const base::Callback<base::Time()>& get_timestamp_callback) {
  get_timestamp_callback_ = get_timestamp_callback;
}

}  // namespace content
