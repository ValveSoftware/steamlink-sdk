// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_NAVIGATION_ENTRY_IMPL_H_
#define CONTENT_BROWSER_FRAME_HOST_NAVIGATION_ENTRY_IMPL_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/frame_host/frame_navigation_entry.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/site_instance_impl.h"
#include "content/common/frame_message_enums.h"
#include "content/common/resource_request_body_impl.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/common/page_state.h"
#include "content/public/common/ssl_status.h"

namespace content {
class ResourceRequestBodyImpl;
struct CommonNavigationParams;
struct RequestNavigationParams;
struct StartNavigationParams;

class CONTENT_EXPORT NavigationEntryImpl
    : public NON_EXPORTED_BASE(NavigationEntry) {
 public:
  // Represents a tree of FrameNavigationEntries that make up this joint session
  // history item.  The tree currently only tracks the main frame by default,
  // and is populated with subframe nodes in --site-per-process mode.
  struct TreeNode {
    TreeNode(FrameNavigationEntry* frame_entry);
    ~TreeNode();

    // Returns whether this TreeNode corresponds to |frame_tree_node|.  If this
    // is called on the root TreeNode, then |is_root_tree_node| should be true
    // and we only check if |frame_tree_node| is the main frame.  Otherwise, we
    // check if the unique name matches.
    bool MatchesFrame(FrameTreeNode* frame_tree_node,
                      bool is_root_tree_node) const;

    // Recursively makes a deep copy of TreeNode with copies of each of the
    // FrameNavigationEntries in the subtree.  Replaces the TreeNode
    // corresponding to |frame_tree_node| (and all of its children) with a new
    // TreeNode for |frame_navigation_entry|.  Pass nullptr for both parameters
    // to make a complete clone.
    // |is_root_tree_node| indicates whether this is being called on the root
    // NavigationEntryImpl::TreeNode.
    // TODO(creis): For --site-per-process, share FrameNavigationEntries between
    // NavigationEntries of the same tab.
    std::unique_ptr<TreeNode> CloneAndReplace(
        FrameTreeNode* frame_tree_node,
        FrameNavigationEntry* frame_navigation_entry,
        bool is_root_tree_node) const;

    // Ref counted pointer that keeps the FrameNavigationEntry alive as long as
    // it is needed by this node's NavigationEntry.
    scoped_refptr<FrameNavigationEntry> frame_entry;

    // List of child TreeNodes, which will be deleted when this node is.
    ScopedVector<TreeNode> children;
  };

  static NavigationEntryImpl* FromNavigationEntry(NavigationEntry* entry);
  static const NavigationEntryImpl* FromNavigationEntry(
      const NavigationEntry* entry);
  static std::unique_ptr<NavigationEntryImpl> FromNavigationEntry(
      std::unique_ptr<NavigationEntry> entry);

  // The value of bindings() before it is set during commit.
  static int kInvalidBindings;

  NavigationEntryImpl();
  NavigationEntryImpl(scoped_refptr<SiteInstanceImpl> instance,
                      int page_id,
                      const GURL& url,
                      const Referrer& referrer,
                      const base::string16& title,
                      ui::PageTransition transition_type,
                      bool is_renderer_initiated);
  ~NavigationEntryImpl() override;

  // NavigationEntry implementation:
  int GetUniqueID() const override;
  PageType GetPageType() const override;
  void SetURL(const GURL& url) override;
  const GURL& GetURL() const override;
  void SetBaseURLForDataURL(const GURL& url) override;
  const GURL& GetBaseURLForDataURL() const override;
#if defined(OS_ANDROID)
  void SetDataURLAsString(
      scoped_refptr<base::RefCountedString> data_url) override;
  const scoped_refptr<const base::RefCountedString> GetDataURLAsString()
      const override;
#endif
  void SetReferrer(const Referrer& referrer) override;
  const Referrer& GetReferrer() const override;
  void SetVirtualURL(const GURL& url) override;
  const GURL& GetVirtualURL() const override;
  void SetTitle(const base::string16& title) override;
  const base::string16& GetTitle() const override;
  void SetPageState(const PageState& state) override;
  PageState GetPageState() const override;
  void SetPageID(int page_id) override;
  int32_t GetPageID() const override;
  const base::string16& GetTitleForDisplay() const override;
  bool IsViewSourceMode() const override;
  void SetTransitionType(ui::PageTransition transition_type) override;
  ui::PageTransition GetTransitionType() const override;
  const GURL& GetUserTypedURL() const override;
  void SetHasPostData(bool has_post_data) override;
  bool GetHasPostData() const override;
  void SetPostID(int64_t post_id) override;
  int64_t GetPostID() const override;
  void SetPostData(const scoped_refptr<ResourceRequestBody>& data) override;
  scoped_refptr<ResourceRequestBody> GetPostData() const override;
  const FaviconStatus& GetFavicon() const override;
  FaviconStatus& GetFavicon() override;
  const SSLStatus& GetSSL() const override;
  SSLStatus& GetSSL() override;
  void SetOriginalRequestURL(const GURL& original_url) override;
  const GURL& GetOriginalRequestURL() const override;
  void SetIsOverridingUserAgent(bool override) override;
  bool GetIsOverridingUserAgent() const override;
  void SetTimestamp(base::Time timestamp) override;
  base::Time GetTimestamp() const override;
  void SetCanLoadLocalResources(bool allow) override;
  bool GetCanLoadLocalResources() const override;
  void SetExtraData(const std::string& key,
                    const base::string16& data) override;
  bool GetExtraData(const std::string& key,
                    base::string16* data) const override;
  void ClearExtraData(const std::string& key) override;
  void SetHttpStatusCode(int http_status_code) override;
  int GetHttpStatusCode() const override;
  void SetRedirectChain(const std::vector<GURL>& redirects) override;
  const std::vector<GURL>& GetRedirectChain() const override;
  bool IsRestored() const override;

  // Creates a copy of this NavigationEntryImpl that can be modified
  // independently from the original.  Does not copy any value that would be
  // cleared in ResetForCommit.
  std::unique_ptr<NavigationEntryImpl> Clone() const;

  // Like |Clone|, but replaces the FrameNavigationEntry corresponding to
  // |frame_tree_node| (and all its children) with |frame_entry|.
  // TODO(creis): Once we start sharing FrameNavigationEntries between
  // NavigationEntryImpls, we will need to support two versions of Clone: one
  // that shares the existing FrameNavigationEntries (for use within the same
  // tab) and one that draws them from a different pool (for use in a new tab).
  std::unique_ptr<NavigationEntryImpl> CloneAndReplace(
      FrameTreeNode* frame_tree_node,
      FrameNavigationEntry* frame_entry) const;

  // Helper functions to construct NavigationParameters for a navigation to this
  // NavigationEntry.
  CommonNavigationParams ConstructCommonNavigationParams(
      const FrameNavigationEntry& frame_entry,
      const scoped_refptr<ResourceRequestBodyImpl>& post_body,
      const GURL& dest_url,
      const Referrer& dest_referrer,
      FrameMsg_Navigate_Type::Value navigation_type,
      LoFiState lofi_state,
      const base::TimeTicks& navigation_start) const;
  StartNavigationParams ConstructStartNavigationParams() const;
  RequestNavigationParams ConstructRequestNavigationParams(
      const FrameNavigationEntry& frame_entry,
      bool is_same_document_history_load,
      bool has_committed_real_load,
      bool intended_as_new_entry,
      int pending_offset_to_send,
      int current_offset_to_send,
      int current_length_to_send) const;

  // Once a navigation entry is committed, we should no longer track several
  // pieces of non-persisted state, as documented on the members below.
  // |frame_entry| is the FrameNavigationEntry for the frame that committed
  // the navigation. It can be null.
  void ResetForCommit(FrameNavigationEntry* frame_entry);

  // Exposes the tree of FrameNavigationEntries that make up this joint session
  // history item.
  // In default Chrome, this tree only has a root node with an unshared
  // FrameNavigationEntry.  Subframes are only added to the tree if the
  // --site-per-process flag is passed.
  TreeNode* root_node() const {
    return frame_tree_.get();
  }

  // Finds the TreeNode associated with |frame_tree_node_id| to add or update
  // its FrameNavigationEntry.  A new FrameNavigationEntry is added if none
  // exists, or else the existing one (which might be shared with other
  // NavigationEntries) is updated with the given parameters.
  // Does nothing if there is no entry already and |url| is about:blank, since
  // that does not count as a real commit.
  void AddOrUpdateFrameEntry(
      FrameTreeNode* frame_tree_node,
      int64_t item_sequence_number,
      int64_t document_sequence_number,
      SiteInstanceImpl* site_instance,
      scoped_refptr<SiteInstanceImpl> source_site_instance,
      const GURL& url,
      const Referrer& referrer,
      const PageState& page_state,
      const std::string& method,
      int64_t post_id);

  // Returns the FrameNavigationEntry corresponding to |frame_tree_node|, if
  // there is one in this NavigationEntry.
  FrameNavigationEntry* GetFrameEntry(FrameTreeNode* frame_tree_node) const;

  void set_unique_id(int unique_id) {
    unique_id_ = unique_id;
  }

  // The SiteInstance represents which pages must share processes. This is a
  // reference counted pointer to a shared SiteInstance.
  //
  // Note that the SiteInstance should usually not be changed after it is set,
  // but this may happen if the NavigationEntry was cloned and needs to use a
  // different SiteInstance.
  void set_site_instance(scoped_refptr<SiteInstanceImpl> site_instance);
  SiteInstanceImpl* site_instance() const {
    return frame_tree_->frame_entry->site_instance();
  }

  // The |source_site_instance| is used to identify the SiteInstance of the
  // frame that initiated the navigation. It is set on the
  // FrameNavigationEntry for the main frame.
  void set_source_site_instance(
      scoped_refptr<SiteInstanceImpl> source_site_instance) {
    root_node()->frame_entry->set_source_site_instance(
        source_site_instance.get());
  }

  // Remember the set of bindings granted to this NavigationEntry at the time
  // of commit, to ensure that we do not grant it additional bindings if we
  // navigate back to it in the future.  This can only be changed once.
  void SetBindings(int bindings);
  int bindings() const {
    return bindings_;
  }

  void set_page_type(PageType page_type) {
    page_type_ = page_type;
  }

  bool has_virtual_url() const {
    return !virtual_url_.is_empty();
  }

  bool update_virtual_url_with_url() const {
    return update_virtual_url_with_url_;
  }
  void set_update_virtual_url_with_url(bool update) {
    update_virtual_url_with_url_ = update;
  }

  // Extra headers (separated by \n) to send during the request.
  void set_extra_headers(const std::string& extra_headers) {
    extra_headers_ = extra_headers;
  }
  const std::string& extra_headers() const {
    return extra_headers_;
  }

  // Whether this (pending) navigation is renderer-initiated.  Resets to false
  // for all types of navigations after commit.
  void set_is_renderer_initiated(bool is_renderer_initiated) {
    is_renderer_initiated_ = is_renderer_initiated;
  }
  bool is_renderer_initiated() const {
    return is_renderer_initiated_;
  }

  void set_user_typed_url(const GURL& user_typed_url) {
    user_typed_url_ = user_typed_url;
  }

  // Enumerations of the possible restore types.
  enum RestoreType {
    // Restore from the previous session.
    RESTORE_LAST_SESSION_EXITED_CLEANLY,
    RESTORE_LAST_SESSION_CRASHED,

    // The entry has been restored from the current session. This is used when
    // the user issues 'reopen closed tab'.
    RESTORE_CURRENT_SESSION,

    // The entry was not restored.
    RESTORE_NONE
  };

  // The RestoreType for this entry. This is set if the entry was retored. This
  // is set to RESTORE_NONE once the entry is loaded.
  void set_restore_type(RestoreType type) {
    restore_type_ = type;
  }
  RestoreType restore_type() const {
    return restore_type_;
  }

  void set_transferred_global_request_id(
      const GlobalRequestID& transferred_global_request_id) {
    transferred_global_request_id_ = transferred_global_request_id;
  }

  GlobalRequestID transferred_global_request_id() const {
    return transferred_global_request_id_;
  }

  // Whether this (pending) navigation needs to replace current entry.
  // Resets to false after commit.
  bool should_replace_entry() const {
    return should_replace_entry_;
  }

  void set_should_replace_entry(bool should_replace_entry) {
    should_replace_entry_ = should_replace_entry;
  }

  void SetScreenshotPNGData(scoped_refptr<base::RefCountedBytes> png_data);
  const scoped_refptr<base::RefCountedBytes> screenshot() const {
    return screenshot_;
  }

  // Whether this (pending) navigation should clear the session history. Resets
  // to false after commit.
  bool should_clear_history_list() const {
    return should_clear_history_list_;
  }
  void set_should_clear_history_list(bool should_clear_history_list) {
    should_clear_history_list_ = should_clear_history_list;
  }

  // Indicates which FrameTreeNode to navigate.  Currently only used if the
  // --site-per-process flag is passed.
  int frame_tree_node_id() const {
    return frame_tree_node_id_;
  }
  void set_frame_tree_node_id(int frame_tree_node_id) {
    frame_tree_node_id_ = frame_tree_node_id;
  }

  // Returns the history URL for a data URL to use in Blink.
  GURL GetHistoryURLForDataURL() const;

#if defined(OS_ANDROID)
  base::TimeTicks intent_received_timestamp() const {
    return intent_received_timestamp_;
  }

  void set_intent_received_timestamp(
      const base::TimeTicks intent_received_timestamp) {
    intent_received_timestamp_ = intent_received_timestamp;
  }

  bool has_user_gesture() const {
    return has_user_gesture_;
  }

  void set_has_user_gesture (bool has_user_gesture) {
    has_user_gesture_ = has_user_gesture;
  }
#endif

 private:
  // Finds the TreeNode associated with |frame_tree_node|, if any.
  NavigationEntryImpl::TreeNode* FindFrameEntry(
      FrameTreeNode* frame_tree_node) const;

  // WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
  // Session/Tab restore save portions of this class so that it can be recreated
  // later. If you add a new field that needs to be persisted you'll have to
  // update SessionService/TabRestoreService and Android WebView
  // state_serializer.cc appropriately.
  // For all new fields, update |Clone| and possibly |ResetForCommit|.
  // WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING

  // Tree of FrameNavigationEntries, one for each frame on the page.
  // TODO(creis): Once FrameNavigationEntries can be shared across multiple
  // NavigationEntries, we will need to update Session/Tab restore.  For now,
  // each NavigationEntry's tree has its own unshared FrameNavigationEntries.
  std::unique_ptr<TreeNode> frame_tree_;

  // See the accessors above for descriptions.
  int unique_id_;
  // TODO(creis): Persist bindings_. http://crbug.com/173672.
  int bindings_;
  PageType page_type_;
  GURL virtual_url_;
  bool update_virtual_url_with_url_;
  base::string16 title_;
  FaviconStatus favicon_;
  int32_t page_id_;
  SSLStatus ssl_;
  ui::PageTransition transition_type_;
  GURL user_typed_url_;
  RestoreType restore_type_;
  GURL original_request_url_;
  bool is_overriding_user_agent_;
  base::Time timestamp_;
  int http_status_code_;

  // This member is not persisted with session restore because it is transient.
  // If the post request succeeds, this field is cleared since the same
  // information is stored in PageState. It is also only shallow copied with
  // compiler provided copy constructor.  Cleared in |ResetForCommit|.
  scoped_refptr<ResourceRequestBodyImpl> post_data_;

  // This is also a transient member (i.e. is not persisted with session
  // restore). The screenshot of a page is taken when navigating away from the
  // page. This screenshot is displayed during an overscroll-navigation
  // gesture. |screenshot_| will be NULL when the screenshot is not available
  // (e.g. after a session restore, or if taking the screenshot of a page
  // failed). The UI is responsible for dealing with missing screenshots
  // appropriately (e.g. display a placeholder image instead).
  scoped_refptr<base::RefCountedBytes> screenshot_;

  // This member is not persisted with session restore.
  std::string extra_headers_;

  // Used for specifying base URL for pages loaded via data URLs. Only used and
  // persisted by Android WebView.
  GURL base_url_for_data_url_;

#if defined(OS_ANDROID)
  // Used for passing really big data URLs from browser to renderers. Only used
  // and persisted by Android WebView.
  scoped_refptr<const base::RefCountedString> data_url_as_string_;
#endif

  // Whether the entry, while loading, was created for a renderer-initiated
  // navigation.  This dictates whether the URL should be displayed before the
  // navigation commits.  It is cleared in |ResetForCommit| and not persisted.
  bool is_renderer_initiated_;

  // This is a cached version of the result of GetTitleForDisplay. It prevents
  // us from having to do URL formatting on the URL every time the title is
  // displayed. When the URL, virtual URL, or title is set, this should be
  // cleared to force a refresh.
  mutable base::string16 cached_display_title_;

  // In case a navigation is transferred to a new RVH but the request has
  // been generated in the renderer already, this identifies the old request so
  // that it can be resumed. The old request is stored until the
  // ResourceDispatcher receives the navigation from the renderer which
  // carries this |transferred_global_request_id_| annotation. Once the request
  // is transferred to the new process, this is cleared and the request
  // continues as normal.
  // Cleared in |ResetForCommit|.
  GlobalRequestID transferred_global_request_id_;

  // This is set to true when this entry is being reloaded and due to changes in
  // the state of the URL, it has to be reloaded in a different site instance.
  // In such case, we must treat it as an existing navigation in the new site
  // instance, instead of a new navigation. This value should not be persisted
  // and is cleared in |ResetForCommit|.
  //
  // We also use this flag for cross-process redirect navigations, so that the
  // browser will replace the current navigation entry (which is the page
  // doing the redirect).
  bool should_replace_entry_;

  // This is used when transferring a pending entry from one process to another.
  // We also send this data through session sync for offline analysis.
  // It is preserved after commit but should not be persisted.
  std::vector<GURL> redirect_chain_;

  // This is set to true when this entry's navigation should clear the session
  // history both on the renderer and browser side. The browser side history
  // won't be cleared until the renderer has committed this navigation. This
  // entry is not persisted by the session restore system, as it is always
  // cleared in |ResetForCommit|.
  bool should_clear_history_list_;

  // Set when this entry should be able to access local file:// resources. This
  // value is not needed after the entry commits and is not persisted.
  bool can_load_local_resources_;

  // If not -1, this indicates which FrameTreeNode to navigate.  This field is
  // not persisted because it is experimental and only used when the
  // --site-per-process flag is passed.  It is cleared in |ResetForCommit|
  // because we only use it while the navigation is pending.
  // TODO(creis): Move this to FrameNavigationEntry.
  int frame_tree_node_id_;

#if defined(OS_ANDROID)
  // The time at which Chrome received the Android Intent that triggered this
  // URL load operation. Reset at commit and not persisted.
  base::TimeTicks intent_received_timestamp_;

  // Whether the URL load carries a user gesture.
  bool has_user_gesture_;
#endif

  // Used to store extra data to support browser features. This member is not
  // persisted, unless specific data is taken out/put back in at save/restore
  // time (see TabNavigation for an example of this).
  std::map<std::string, base::string16> extra_data_;

  DISALLOW_COPY_AND_ASSIGN(NavigationEntryImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_NAVIGATION_ENTRY_IMPL_H_
