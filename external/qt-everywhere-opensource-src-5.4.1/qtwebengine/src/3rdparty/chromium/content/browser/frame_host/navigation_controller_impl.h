// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_NAVIGATION_CONTROLLER_IMPL_H_
#define CONTENT_BROWSER_FRAME_HOST_NAVIGATION_CONTROLLER_IMPL_H_

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/linked_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/frame_host/navigation_controller_delegate.h"
#include "content/browser/ssl/ssl_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_type.h"

struct FrameHostMsg_DidCommitProvisionalLoad_Params;

namespace content {
class NavigationEntryImpl;
class RenderViewHost;
class NavigationEntryScreenshotManager;
class SiteInstance;
struct LoadCommittedDetails;

class CONTENT_EXPORT NavigationControllerImpl
    : public NON_EXPORTED_BASE(NavigationController) {
 public:
  NavigationControllerImpl(
      NavigationControllerDelegate* delegate,
      BrowserContext* browser_context);
  virtual ~NavigationControllerImpl();

  // NavigationController implementation:
  virtual WebContents* GetWebContents() const OVERRIDE;
  virtual BrowserContext* GetBrowserContext() const OVERRIDE;
  virtual void SetBrowserContext(
      BrowserContext* browser_context) OVERRIDE;
  virtual void Restore(
      int selected_navigation,
      RestoreType type,
      std::vector<NavigationEntry*>* entries) OVERRIDE;
  virtual NavigationEntry* GetActiveEntry() const OVERRIDE;
  virtual NavigationEntry* GetVisibleEntry() const OVERRIDE;
  virtual int GetCurrentEntryIndex() const OVERRIDE;
  virtual NavigationEntry* GetLastCommittedEntry() const OVERRIDE;
  virtual int GetLastCommittedEntryIndex() const OVERRIDE;
  virtual bool CanViewSource() const OVERRIDE;
  virtual int GetEntryCount() const OVERRIDE;
  virtual NavigationEntry* GetEntryAtIndex(int index) const OVERRIDE;
  virtual NavigationEntry* GetEntryAtOffset(int offset) const OVERRIDE;
  virtual void DiscardNonCommittedEntries() OVERRIDE;
  virtual NavigationEntry* GetPendingEntry() const OVERRIDE;
  virtual int GetPendingEntryIndex() const OVERRIDE;
  virtual NavigationEntry* GetTransientEntry() const OVERRIDE;
  virtual void SetTransientEntry(NavigationEntry* entry) OVERRIDE;
  virtual void LoadURL(const GURL& url,
                       const Referrer& referrer,
                       PageTransition type,
                       const std::string& extra_headers) OVERRIDE;
  virtual void LoadURLWithParams(const LoadURLParams& params) OVERRIDE;
  virtual void LoadIfNecessary() OVERRIDE;
  virtual bool CanGoBack() const OVERRIDE;
  virtual bool CanGoForward() const OVERRIDE;
  virtual bool CanGoToOffset(int offset) const OVERRIDE;
  virtual void GoBack() OVERRIDE;
  virtual void GoForward() OVERRIDE;
  virtual void GoToIndex(int index) OVERRIDE;
  virtual void GoToOffset(int offset) OVERRIDE;
  virtual bool RemoveEntryAtIndex(int index) OVERRIDE;
  virtual const SessionStorageNamespaceMap&
      GetSessionStorageNamespaceMap() const OVERRIDE;
  virtual SessionStorageNamespace*
      GetDefaultSessionStorageNamespace() OVERRIDE;
  virtual void SetMaxRestoredPageID(int32 max_id) OVERRIDE;
  virtual int32 GetMaxRestoredPageID() const OVERRIDE;
  virtual bool NeedsReload() const OVERRIDE;
  virtual void SetNeedsReload() OVERRIDE;
  virtual void CancelPendingReload() OVERRIDE;
  virtual void ContinuePendingReload() OVERRIDE;
  virtual bool IsInitialNavigation() const OVERRIDE;
  virtual void Reload(bool check_for_repost) OVERRIDE;
  virtual void ReloadIgnoringCache(bool check_for_repost) OVERRIDE;
  virtual void ReloadOriginalRequestURL(bool check_for_repost) OVERRIDE;
  virtual void NotifyEntryChanged(const NavigationEntry* entry,
                                 int index) OVERRIDE;
  virtual void CopyStateFrom(
      const NavigationController& source) OVERRIDE;
  virtual void CopyStateFromAndPrune(NavigationController* source,
                                     bool replace_entry) OVERRIDE;
  virtual bool CanPruneAllButLastCommitted() OVERRIDE;
  virtual void PruneAllButLastCommitted() OVERRIDE;
  virtual void ClearAllScreenshots() OVERRIDE;

  // Whether this is the initial navigation in an unmodified new tab.  In this
  // case, we know there is no content displayed in the page.
  bool IsUnmodifiedBlankTab() const;

  // The session storage namespace that all child RenderViews belonging to
  // |instance| should use.
  SessionStorageNamespace* GetSessionStorageNamespace(
      SiteInstance* instance);

  // Returns the index of the specified entry, or -1 if entry is not contained
  // in this NavigationController.
  int GetIndexOfEntry(const NavigationEntryImpl* entry) const;

  // Return the index of the entry with the corresponding instance and page_id,
  // or -1 if not found.
  int GetEntryIndexWithPageID(SiteInstance* instance,
                              int32 page_id) const;

  // Return the entry with the corresponding instance and page_id, or NULL if
  // not found.
  NavigationEntryImpl* GetEntryWithPageID(
      SiteInstance* instance,
      int32 page_id) const;

  NavigationControllerDelegate* delegate() const {
    return delegate_;
  }

  // For use by WebContentsImpl ------------------------------------------------

  // Allow renderer-initiated navigations to create a pending entry when the
  // provisional load starts.
  void SetPendingEntry(content::NavigationEntryImpl* entry);

  // Handles updating the navigation state after the renderer has navigated.
  // This is used by the WebContentsImpl.
  //
  // If a new entry is created, it will return true and will have filled the
  // given details structure and broadcast the NOTIFY_NAV_ENTRY_COMMITTED
  // notification. The caller can then use the details without worrying about
  // listening for the notification.
  //
  // In the case that nothing has changed, the details structure is undefined
  // and it will return false.
  bool RendererDidNavigate(
      RenderFrameHost* rfh,
      const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
      LoadCommittedDetails* details);

  // Notifies us that we just became active. This is used by the WebContentsImpl
  // so that we know to load URLs that were pending as "lazy" loads.
  void SetActive(bool is_active);

  // Returns true if the given URL would be an in-page navigation (i.e. only
  // the reference fragment is different) from the "last committed entry". We do
  // not compare it against the "active entry" since the active entry can be
  // pending and in page navigations only happen on committed pages. If there
  // is no last committed entry, then nothing will be in-page.
  //
  // Special note: if the URLs are the same, it does NOT automatically count as
  // an in-page navigation. Neither does an input URL that has no ref, even if
  // the rest is the same. This may seem weird, but when we're considering
  // whether a navigation happened without loading anything, the same URL could
  // be a reload, while only a different ref would be in-page (pages can't clear
  // refs without reload, only change to "#" which we don't count as empty).
  //
  // The situation is made murkier by history.replaceState(), which could
  // provide the same URL as part of an in-page navigation, not a reload. So
  // we need to let the (untrustworthy) renderer resolve the ambiguity, but
  // only when the URLs are on the same origin.
  bool IsURLInPageNavigation(
      const GURL& url,
      bool renderer_says_in_page,
      NavigationType navigation_type) const;

  // Sets the SessionStorageNamespace for the given |partition_id|. This is
  // used during initialization of a new NavigationController to allow
  // pre-population of the SessionStorageNamespace objects. Session restore,
  // prerendering, and the implementaion of window.open() are the primary users
  // of this API.
  //
  // Calling this function when a SessionStorageNamespace has already been
  // associated with a |partition_id| will CHECK() fail.
  void SetSessionStorageNamespace(
      const std::string& partition_id,
      SessionStorageNamespace* session_storage_namespace);

  // Random data ---------------------------------------------------------------

  SSLManager* ssl_manager() { return &ssl_manager_; }

  // Maximum number of entries before we start removing entries from the front.
  static void set_max_entry_count_for_testing(size_t max_entry_count) {
    max_entry_count_for_testing_ = max_entry_count;
  }
  static size_t max_entry_count();

  void SetGetTimestampCallbackForTest(
      const base::Callback<base::Time()>& get_timestamp_callback);

  // Takes a screenshot of the page at the current state.
  void TakeScreenshot();

  // Sets the screenshot manager for this NavigationControllerImpl. The
  // controller takes ownership of the screenshot manager and destroys it when
  // a new screenshot-manager is set, or when the controller is destroyed.
  // Setting a NULL manager recreates the default screenshot manager and uses
  // that.
  void SetScreenshotManager(NavigationEntryScreenshotManager* manager);

  // Discards only the pending entry.
  void DiscardPendingEntry();

 private:
  friend class RestoreHelper;

  FRIEND_TEST_ALL_PREFIXES(NavigationControllerTest,
                           PurgeScreenshot);
  FRIEND_TEST_ALL_PREFIXES(TimeSmoother, Basic);
  FRIEND_TEST_ALL_PREFIXES(TimeSmoother, SingleDuplicate);
  FRIEND_TEST_ALL_PREFIXES(TimeSmoother, ManyDuplicates);
  FRIEND_TEST_ALL_PREFIXES(TimeSmoother, ClockBackwardsJump);

  // Helper class to smooth out runs of duplicate timestamps while still
  // allowing time to jump backwards.
  class CONTENT_EXPORT TimeSmoother {
   public:
    // Returns |t| with possibly some time added on.
    base::Time GetSmoothedTime(base::Time t);

   private:
    // |low_water_mark_| is the first time in a sequence of adjusted
    // times and |high_water_mark_| is the last.
    base::Time low_water_mark_;
    base::Time high_water_mark_;
  };

  // Classifies the given renderer navigation (see the NavigationType enum).
  NavigationType ClassifyNavigation(
      RenderFrameHost* rfh,
      const FrameHostMsg_DidCommitProvisionalLoad_Params& params) const;

  // Causes the controller to load the specified entry. The function assumes
  // ownership of the pointer since it is put in the navigation list.
  // NOTE: Do not pass an entry that the controller already owns!
  void LoadEntry(NavigationEntryImpl* entry);

  // Handlers for the different types of navigation types. They will actually
  // handle the navigations corresponding to the different NavClasses above.
  // They will NOT broadcast the commit notification, that should be handled by
  // the caller.
  //
  // RendererDidNavigateAutoSubframe is special, it may not actually change
  // anything if some random subframe is loaded. It will return true if anything
  // changed, or false if not.
  //
  // The functions taking |did_replace_entry| will fill into the given variable
  // whether the last entry has been replaced or not.
  // See LoadCommittedDetails.did_replace_entry.
  void RendererDidNavigateToNewPage(
      RenderFrameHost* rfh,
      const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
      bool replace_entry);
  void RendererDidNavigateToExistingPage(
      RenderFrameHost* rfh,
      const FrameHostMsg_DidCommitProvisionalLoad_Params& params);
  void RendererDidNavigateToSamePage(
      RenderFrameHost* rfh,
      const FrameHostMsg_DidCommitProvisionalLoad_Params& params);
  void RendererDidNavigateInPage(
      RenderFrameHost* rfh,
      const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
      bool* did_replace_entry);
  void RendererDidNavigateNewSubframe(
      RenderFrameHost* rfh,
      const FrameHostMsg_DidCommitProvisionalLoad_Params& params);
  bool RendererDidNavigateAutoSubframe(
      RenderFrameHost* rfh,
      const FrameHostMsg_DidCommitProvisionalLoad_Params& params);

  // Helper function for code shared between Reload() and ReloadIgnoringCache().
  void ReloadInternal(bool check_for_repost, ReloadType reload_type);

  // Actually issues the navigation held in pending_entry.
  void NavigateToPendingEntry(ReloadType reload_type);

  // Allows the derived class to issue notifications that a load has been
  // committed. This will fill in the active entry to the details structure.
  void NotifyNavigationEntryCommitted(LoadCommittedDetails* details);

  // Updates the virtual URL of an entry to match a new URL, for cases where
  // the real renderer URL is derived from the virtual URL, like view-source:
  void UpdateVirtualURLToURL(NavigationEntryImpl* entry,
                             const GURL& new_url);

  // Invoked after session/tab restore or cloning a tab. Resets the transition
  // type of the entries, updates the max page id and creates the active
  // contents.
  void FinishRestore(int selected_index, RestoreType type);

  // Inserts a new entry or replaces the current entry with a new one, removing
  // all entries after it. The new entry will become the active one.
  void InsertOrReplaceEntry(NavigationEntryImpl* entry, bool replace);

  // Removes the entry at |index|, as long as it is not the current entry.
  void RemoveEntryAtIndexInternal(int index);

  // Discards both the pending and transient entries.
  void DiscardNonCommittedEntriesInternal();

  // Discards only the transient entry.
  void DiscardTransientEntry();

  // If we have the maximum number of entries, remove the oldest one in
  // preparation to add another.
  void PruneOldestEntryIfFull();

  // Removes all entries except the last committed entry.  If there is a new
  // pending navigation it is preserved. In contrast to
  // PruneAllButLastCommitted() this does not update the session history of the
  // RenderView.  Callers must ensure that |CanPruneAllButLastCommitted| returns
  // true before calling this.
  void PruneAllButLastCommittedInternal();

  // Returns true if the navigation is likley to be automatic rather than
  // user-initiated.
  bool IsLikelyAutoNavigation(base::TimeTicks now);

  // Inserts up to |max_index| entries from |source| into this. This does NOT
  // adjust any of the members that reference entries_
  // (last_committed_entry_index_, pending_entry_index_ or
  // transient_entry_index_).
  void InsertEntriesFrom(const NavigationControllerImpl& source, int max_index);

  // Returns the navigation index that differs from the current entry by the
  // specified |offset|.  The index returned is not guaranteed to be valid.
  int GetIndexForOffset(int offset) const;

  // ---------------------------------------------------------------------------

  // The user browser context associated with this controller.
  BrowserContext* browser_context_;

  // List of NavigationEntry for this tab
  typedef std::vector<linked_ptr<NavigationEntryImpl> > NavigationEntries;
  NavigationEntries entries_;

  // An entry we haven't gotten a response for yet.  This will be discarded
  // when we navigate again.  It's used only so we know what the currently
  // displayed tab is.
  //
  // This may refer to an item in the entries_ list if the pending_entry_index_
  // == -1, or it may be its own entry that should be deleted. Be careful with
  // the memory management.
  NavigationEntryImpl* pending_entry_;

  // currently visible entry
  int last_committed_entry_index_;

  // index of pending entry if it is in entries_, or -1 if pending_entry_ is a
  // new entry (created by LoadURL).
  int pending_entry_index_;

  // The index for the entry that is shown until a navigation occurs.  This is
  // used for interstitial pages. -1 if there are no such entry.
  // Note that this entry really appears in the list of entries, but only
  // temporarily (until the next navigation).  Any index pointing to an entry
  // after the transient entry will become invalid if you navigate forward.
  int transient_entry_index_;

  // The delegate associated with the controller. Possibly NULL during
  // setup.
  NavigationControllerDelegate* delegate_;

  // The max restored page ID in this controller, if it was restored.  We must
  // store this so that WebContentsImpl can tell any renderer in charge of one
  // of the restored entries to update its max page ID.
  int32 max_restored_page_id_;

  // Manages the SSL security UI.
  SSLManager ssl_manager_;

  // Whether we need to be reloaded when made active.
  bool needs_reload_;

  // Whether this is the initial navigation.
  // Becomes false when initial navigation commits.
  bool is_initial_navigation_;

  // Prevent unsafe re-entrant calls to NavigateToPendingEntry.
  bool in_navigate_to_pending_entry_;

  // Used to find the appropriate SessionStorageNamespace for the storage
  // partition of a NavigationEntry.
  //
  // A NavigationController may contain NavigationEntries that correspond to
  // different StoragePartitions. Even though they are part of the same
  // NavigationController, only entries in the same StoragePartition may
  // share session storage state with one another.
  SessionStorageNamespaceMap session_storage_namespace_map_;

  // The maximum number of entries that a navigation controller can store.
  static size_t max_entry_count_for_testing_;

  // If a repost is pending, its type (RELOAD or RELOAD_IGNORING_CACHE),
  // NO_RELOAD otherwise.
  ReloadType pending_reload_;

  // Used to get timestamps for newly-created navigation entries.
  base::Callback<base::Time()> get_timestamp_callback_;

  // Used to smooth out timestamps from |get_timestamp_callback_|.
  // Without this, whenever there is a run of redirects or
  // code-generated navigations, those navigations may occur within
  // the timer resolution, leading to things sometimes showing up in
  // the wrong order in the history view.
  TimeSmoother time_smoother_;

  scoped_ptr<NavigationEntryScreenshotManager> screenshot_manager_;

  DISALLOW_COPY_AND_ASSIGN(NavigationControllerImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_NAVIGATION_CONTROLLER_IMPL_H_
