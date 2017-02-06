// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_FRAME_TREE_NODE_H_
#define CONTENT_BROWSER_FRAME_HOST_FRAME_TREE_NODE_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/browser/frame_host/frame_tree_node_blame_context.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/frame_host/render_frame_host_manager.h"
#include "content/common/content_export.h"
#include "content/common/frame_replication_state.h"
#include "third_party/WebKit/public/platform/WebInsecureRequestPolicy.h"
#include "third_party/WebKit/public/web/WebFrameOwnerProperties.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class FrameTree;
class NavigationRequest;
class Navigator;
class RenderFrameHostImpl;
struct ContentSecurityPolicyHeader;

// When a page contains iframes, its renderer process maintains a tree structure
// of those frames. We are mirroring this tree in the browser process. This
// class represents a node in this tree and is a wrapper for all objects that
// are frame-specific (as opposed to page-specific).
class CONTENT_EXPORT FrameTreeNode {
 public:
  class Observer {
   public:
    // Invoked when a FrameTreeNode is being destroyed.
    virtual void OnFrameTreeNodeDestroyed(FrameTreeNode* node) {}

    // Invoked when a FrameTreeNode becomes focused.
    virtual void OnFrameTreeNodeFocused(FrameTreeNode* node) {}

    virtual ~Observer() {}
  };

  static const int kFrameTreeNodeInvalidId = -1;

  // Returns the FrameTreeNode with the given global |frame_tree_node_id|,
  // regardless of which FrameTree it is in.
  static FrameTreeNode* GloballyFindByID(int frame_tree_node_id);

  // Callers are are expected to initialize sandbox flags separately after
  // calling the constructor.
  FrameTreeNode(FrameTree* frame_tree,
                Navigator* navigator,
                RenderFrameHostDelegate* render_frame_delegate,
                RenderWidgetHostDelegate* render_widget_delegate,
                RenderFrameHostManager::Delegate* manager_delegate,
                FrameTreeNode* parent,
                blink::WebTreeScopeType scope,
                const std::string& name,
                const std::string& unique_name,
                const blink::WebFrameOwnerProperties& frame_owner_properties);

  ~FrameTreeNode();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  bool IsMainFrame() const;

  FrameTreeNode* AddChild(std::unique_ptr<FrameTreeNode> child,
                          int process_id,
                          int frame_routing_id);
  void RemoveChild(FrameTreeNode* child);

  // Clears process specific-state in this node to prepare for a new process.
  void ResetForNewProcess();

  FrameTree* frame_tree() const {
    return frame_tree_;
  }

  Navigator* navigator() {
    return navigator_.get();
  }

  RenderFrameHostManager* render_manager() {
    return &render_manager_;
  }

  int frame_tree_node_id() const {
    return frame_tree_node_id_;
  }

  const std::string& frame_name() const {
    return replication_state_.name;
  }

  const std::string& unique_name() const {
    return replication_state_.unique_name;
  }

  size_t child_count() const {
    return children_.size();
  }

  FrameTreeNode* parent() const { return parent_; }

  FrameTreeNode* opener() const { return opener_; }

  // Assigns a new opener for this node and, if |opener| is non-null, registers
  // an observer that will clear this node's opener if |opener| is ever
  // destroyed.
  void SetOpener(FrameTreeNode* opener);

  FrameTreeNode* child_at(size_t index) const {
    return children_[index].get();
  }

  // Returns the URL of the last committed page in the current frame.
  const GURL& current_url() const {
    return current_frame_host()->last_committed_url();
  }

  // Sets the last committed URL for this frame and updates
  // has_committed_real_load accordingly.
  void SetCurrentURL(const GURL& url);

  // Returns true iff SetCurrentURL has been called with a non-blank URL.
  bool has_committed_real_load() const {
    return has_committed_real_load_;
  }

  // Returns the origin of the last committed page in this frame.
  const url::Origin& current_origin() const {
    return replication_state_.origin;
  }

  // Set the current origin and notify proxies about the update.
  void SetCurrentOrigin(const url::Origin& origin,
                        bool is_potentially_trustworthy_unique_origin);

  // Set the current name and notify proxies about the update.
  void SetFrameName(const std::string& name, const std::string& unique_name);

  // Add CSP header to replication state and notify proxies about the update.
  void AddContentSecurityPolicy(const ContentSecurityPolicyHeader& header);

  // Discards previous CSP headers and notifies proxies about the update.
  // Typically invoked after committing navigation to a new document (since the
  // new document comes with a fresh set of CSP http headers).
  void ResetContentSecurityPolicy();

  // Sets the current insecure request policy, and notifies proxies about the
  // update.
  void SetInsecureRequestPolicy(blink::WebInsecureRequestPolicy policy);

  // Returns the currently active sandbox flags for this frame.  This includes
  // flags inherited from parent frames and the currently active flags from the
  // <iframe> element hosting this frame.  This does not include flags that
  // have been updated in an <iframe> element but have not taken effect yet;
  // use pending_sandbox_flags() for those.
  blink::WebSandboxFlags effective_sandbox_flags() const {
    return replication_state_.sandbox_flags;
  }

  // Returns the latest sandbox flags for this frame.  This includes flags
  // inherited from parent frames and the latest flags from the <iframe>
  // element hosting this frame.  The returned flags may not yet have taken
  // effect, since sandbox flag updates in an <iframe> element take effect on
  // next navigation.  To retrieve the currently active sandbox flags for this
  // frame, use effective_sandbox_flags().
  blink::WebSandboxFlags pending_sandbox_flags() const {
    return pending_sandbox_flags_;
  }

  // Update this frame's sandbox flags.  This is used when a parent frame
  // updates sandbox flags in the <iframe> element for this frame.  These flags
  // won't take effect until next navigation.  If this frame's parent is itself
  // sandboxed, the parent's sandbox flags are combined with |sandbox_flags|.
  void SetPendingSandboxFlags(blink::WebSandboxFlags sandbox_flags);

  // Set any pending sandbox flags as active, and return true if the sandbox
  // flags were changed.
  bool CommitPendingSandboxFlags();

  const blink::WebFrameOwnerProperties& frame_owner_properties() {
    return frame_owner_properties_;
  }

  void set_frame_owner_properties(
      const blink::WebFrameOwnerProperties& frame_owner_properties) {
    frame_owner_properties_ = frame_owner_properties;
  }

  bool HasSameOrigin(const FrameTreeNode& node) const {
    return replication_state_.origin.IsSameOriginWith(
        node.replication_state_.origin);
  }

  const FrameReplicationState& current_replication_state() const {
    return replication_state_;
  }

  RenderFrameHostImpl* current_frame_host() const {
    return render_manager_.current_frame_host();
  }

  bool IsDescendantOf(FrameTreeNode* other) const;

  // Return the node immediately preceding this node in its parent's
  // |children_|, or nullptr if there is no such node.
  FrameTreeNode* PreviousSibling() const;

  // Return the node immediately following this node in its parent's
  // |children_|, or nullptr if there is no such node.
  FrameTreeNode* NextSibling() const;

  // Returns true if this node is in a loading state.
  bool IsLoading() const;

  // Returns this node's loading progress.
  double loading_progress() const { return loading_progress_; }

  NavigationRequest* navigation_request() { return navigation_request_.get(); }

  // PlzNavigate
  // Takes ownership of |navigation_request| and makes it the current
  // NavigationRequest of this frame. This corresponds to the start of a new
  // navigation. If there was an ongoing navigation request before calling this
  // function, it is canceled. |navigation_request| should not be null.
  void CreatedNavigationRequest(
      std::unique_ptr<NavigationRequest> navigation_request);

  // PlzNavigate
  // Resets the current navigation request. If |keep_state| is true, any state
  // created by the NavigationRequest (e.g. speculative RenderFrameHost,
  // loading state) will not be reset by the function.
  void ResetNavigationRequest(bool keep_state);

  // Returns true if this node is in a state where the loading progress is being
  // tracked.
  bool has_started_loading() const;

  // Resets this node's loading progress.
  void reset_loading_progress();

  // A RenderFrameHost in this node started loading.
  // |to_different_document| will be true unless the load is a fragment
  // navigation, or triggered by history.pushState/replaceState.
  // |was_previously_loading| is false if the FrameTree was not loading before.
  // The caller is required to provide this boolean as the delegate should only
  // be notified if the FrameTree went from non-loading to loading state.
  // However, when it is called, the FrameTree should be in a loading state.
  void DidStartLoading(bool to_different_document, bool was_previously_loading);

  // A RenderFrameHost in this node stopped loading.
  void DidStopLoading();

  // The load progress for a RenderFrameHost in this node was updated to
  // |load_progress|. This will notify the FrameTree which will in turn notify
  // the WebContents.
  void DidChangeLoadProgress(double load_progress);

  // Called when the user directed the page to stop loading. Stops all loads
  // happening in the FrameTreeNode. This method should be used with
  // FrameTree::ForEach to stop all loads in the entire FrameTree.
  bool StopLoading();

  // Returns the time this frame was last focused.
  base::TimeTicks last_focus_time() const { return last_focus_time_; }

  // Called when this node becomes focused.  Updates the node's last focused
  // time and notifies observers.
  void DidFocus();

  // Called when the user closed the modal dialogue for BeforeUnload and
  // cancelled the navigation. This should stop any load happening in the
  // FrameTreeNode.
  void BeforeUnloadCanceled();

  // Returns the BlameContext associated with this node.
  FrameTreeNodeBlameContext& blame_context() { return blame_context_; }

 private:
  class OpenerDestroyedObserver;

  FrameTreeNode* GetSibling(int relative_offset) const;

  // The next available browser-global FrameTreeNode ID.
  static int next_frame_tree_node_id_;

  // The FrameTree that owns us.
  FrameTree* frame_tree_;  // not owned.

  // The Navigator object responsible for managing navigations at this node
  // of the frame tree.
  scoped_refptr<Navigator> navigator_;

  // Manages creation and swapping of RenderFrameHosts for this frame.  This
  // must be declared before |children_| so that it gets deleted after them.
  // That's currently necessary so that RenderFrameHostImpl's destructor can
  // call GetProcess.
  RenderFrameHostManager render_manager_;

  // A browser-global identifier for the frame in the page, which stays stable
  // even if the frame does a cross-process navigation.
  const int frame_tree_node_id_;

  // The parent node of this frame. |nullptr| if this node is the root.
  FrameTreeNode* parent_;

  // The frame that opened this frame, if any.  Will be set to null if the
  // opener is closed, or if this frame disowns its opener by setting its
  // window.opener to null.
  FrameTreeNode* opener_;

  // An observer that clears this node's |opener_| if the opener is destroyed.
  // This observer is added to the |opener_|'s observer list when the |opener_|
  // is set to a non-null node, and it is removed from that list when |opener_|
  // changes or when this node is destroyed.  It is also cleared if |opener_|
  // is disowned.
  std::unique_ptr<OpenerDestroyedObserver> opener_observer_;

  // The immediate children of this specific frame.
  std::vector<std::unique_ptr<FrameTreeNode>> children_;

  // Whether this frame has committed any real load, replacing its initial
  // about:blank page.
  bool has_committed_real_load_;

  // Track information that needs to be replicated to processes that have
  // proxies for this frame.
  FrameReplicationState replication_state_;

  // Track the pending sandbox flags for this frame.  When a parent frame
  // dynamically updates sandbox flags in the <iframe> element for a child
  // frame, these updated flags are stored here and are transferred into
  // replication_state_.sandbox_flags when they take effect on the next frame
  // navigation.
  blink::WebSandboxFlags pending_sandbox_flags_;

  // Tracks the scrolling and margin properties for this frame.  These
  // properties affect the child renderer but are stored on its parent's
  // frame element.  When this frame's parent dynamically updates these
  // properties, we update them here too.
  //
  // Note that dynamic updates only take effect on the next frame navigation.
  blink::WebFrameOwnerProperties frame_owner_properties_;

  // Used to track this node's loading progress (from 0 to 1).
  double loading_progress_;

  // PlzNavigate
  // Owns an ongoing NavigationRequest until it is ready to commit. It will then
  // be reset and a RenderFrameHost will be responsible for the navigation.
  std::unique_ptr<NavigationRequest> navigation_request_;

  // List of objects observing this FrameTreeNode.
  base::ObserverList<Observer> observers_;

  base::TimeTicks last_focus_time_;

  // A helper for tracing the snapshots of this FrameTreeNode and attributing
  // browser process activities to this node (when possible).  It is unrelated
  // to the core logic of FrameTreeNode.
  FrameTreeNodeBlameContext blame_context_;

  DISALLOW_COPY_AND_ASSIGN(FrameTreeNode);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_FRAME_TREE_NODE_H_
