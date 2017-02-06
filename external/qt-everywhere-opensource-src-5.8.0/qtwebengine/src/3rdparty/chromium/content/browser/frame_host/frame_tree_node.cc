// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/frame_tree_node.h"

#include <queue>
#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/profiler/scoped_tracker.h"
#include "base/stl_util.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/navigation_request.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/common/frame_messages.h"
#include "content/common/site_isolation_policy.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "third_party/WebKit/public/web/WebSandboxFlags.h"

namespace content {

namespace {

// This is a global map between frame_tree_node_ids and pointers to
// FrameTreeNodes.
typedef base::hash_map<int, FrameTreeNode*> FrameTreeNodeIdMap;

base::LazyInstance<FrameTreeNodeIdMap> g_frame_tree_node_id_map =
    LAZY_INSTANCE_INITIALIZER;

// These values indicate the loading progress status. The minimum progress
// value matches what Blink's ProgressTracker has traditionally used for a
// minimum progress value.
const double kLoadingProgressNotStarted = 0.0;
const double kLoadingProgressMinimum = 0.1;
const double kLoadingProgressDone = 1.0;

void RecordUniqueNameLength(size_t length) {
  UMA_HISTOGRAM_COUNTS("SessionRestore.FrameUniqueNameLength", length);
}

}  // namespace

// This observer watches the opener of its owner FrameTreeNode and clears the
// owner's opener if the opener is destroyed.
class FrameTreeNode::OpenerDestroyedObserver : public FrameTreeNode::Observer {
 public:
  OpenerDestroyedObserver(FrameTreeNode* owner) : owner_(owner) {}

  // FrameTreeNode::Observer
  void OnFrameTreeNodeDestroyed(FrameTreeNode* node) override {
    CHECK_EQ(owner_->opener(), node);
    owner_->SetOpener(nullptr);
  }

 private:
  FrameTreeNode* owner_;

  DISALLOW_COPY_AND_ASSIGN(OpenerDestroyedObserver);
};

int FrameTreeNode::next_frame_tree_node_id_ = 1;

// static
FrameTreeNode* FrameTreeNode::GloballyFindByID(int frame_tree_node_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  FrameTreeNodeIdMap* nodes = g_frame_tree_node_id_map.Pointer();
  FrameTreeNodeIdMap::iterator it = nodes->find(frame_tree_node_id);
  return it == nodes->end() ? nullptr : it->second;
}

FrameTreeNode::FrameTreeNode(
    FrameTree* frame_tree,
    Navigator* navigator,
    RenderFrameHostDelegate* render_frame_delegate,
    RenderWidgetHostDelegate* render_widget_delegate,
    RenderFrameHostManager::Delegate* manager_delegate,
    FrameTreeNode* parent,
    blink::WebTreeScopeType scope,
    const std::string& name,
    const std::string& unique_name,
    const blink::WebFrameOwnerProperties& frame_owner_properties)
    : frame_tree_(frame_tree),
      navigator_(navigator),
      render_manager_(this,
                      render_frame_delegate,
                      render_widget_delegate,
                      manager_delegate),
      frame_tree_node_id_(next_frame_tree_node_id_++),
      parent_(parent),
      opener_(nullptr),
      opener_observer_(nullptr),
      has_committed_real_load_(false),
      replication_state_(
          scope,
          name,
          unique_name,
          blink::WebSandboxFlags::None,
          false /* should enforce strict mixed content checking */,
          false /* is a potentially trustworthy unique origin */),
      pending_sandbox_flags_(blink::WebSandboxFlags::None),
      frame_owner_properties_(frame_owner_properties),
      loading_progress_(kLoadingProgressNotStarted),
      blame_context_(frame_tree_node_id_, parent) {
  std::pair<FrameTreeNodeIdMap::iterator, bool> result =
      g_frame_tree_node_id_map.Get().insert(
          std::make_pair(frame_tree_node_id_, this));
  CHECK(result.second);

  RecordUniqueNameLength(unique_name.size());

  // Note: this should always be done last in the constructor.
  blame_context_.Initialize();
}

FrameTreeNode::~FrameTreeNode() {
  std::vector<std::unique_ptr<FrameTreeNode>>().swap(children_);
  frame_tree_->FrameRemoved(this);
  FOR_EACH_OBSERVER(Observer, observers_, OnFrameTreeNodeDestroyed(this));

  if (opener_)
    opener_->RemoveObserver(opener_observer_.get());

  g_frame_tree_node_id_map.Get().erase(frame_tree_node_id_);
}

void FrameTreeNode::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FrameTreeNode::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool FrameTreeNode::IsMainFrame() const {
  return frame_tree_->root() == this;
}

FrameTreeNode* FrameTreeNode::AddChild(std::unique_ptr<FrameTreeNode> child,
                                       int process_id,
                                       int frame_routing_id) {
  // Child frame must always be created in the same process as the parent.
  CHECK_EQ(process_id, render_manager_.current_host()->GetProcess()->GetID());

  // Initialize the RenderFrameHost for the new node.  We always create child
  // frames in the same SiteInstance as the current frame, and they can swap to
  // a different one if they navigate away.
  child->render_manager()->Init(
      render_manager_.current_host()->GetSiteInstance(),
      render_manager_.current_host()->GetRoutingID(), frame_routing_id,
      MSG_ROUTING_NONE);

  // Other renderer processes in this BrowsingInstance may need to find out
  // about the new frame.  Create a proxy for the child frame in all
  // SiteInstances that have a proxy for the frame's parent, since all frames
  // in a frame tree should have the same set of proxies.
  // TODO(alexmos, nick): We ought to do this for non-oopif too, for openers.
  if (SiteIsolationPolicy::AreCrossProcessFramesPossible())
    render_manager_.CreateProxiesForChildFrame(child.get());

  children_.push_back(std::move(child));
  return children_.back().get();
}

void FrameTreeNode::RemoveChild(FrameTreeNode* child) {
  for (auto iter = children_.begin(); iter != children_.end(); ++iter) {
    if (iter->get() == child) {
      // Subtle: we need to make sure the node is gone from the tree before
      // observers are notified of its deletion.
      std::unique_ptr<FrameTreeNode> node_to_delete(std::move(*iter));
      children_.erase(iter);
      node_to_delete.reset();
      return;
    }
  }
}

void FrameTreeNode::ResetForNewProcess() {
  current_frame_host()->set_last_committed_url(GURL());
  blame_context_.TakeSnapshot();

  // Remove child nodes from the tree, then delete them. This destruction
  // operation will notify observers.
  std::vector<std::unique_ptr<FrameTreeNode>>().swap(children_);
}

void FrameTreeNode::SetOpener(FrameTreeNode* opener) {
  if (opener_) {
    opener_->RemoveObserver(opener_observer_.get());
    opener_observer_.reset();
  }

  opener_ = opener;

  if (opener_) {
    if (!opener_observer_)
      opener_observer_ = base::WrapUnique(new OpenerDestroyedObserver(this));
    opener_->AddObserver(opener_observer_.get());
  }
}

void FrameTreeNode::SetCurrentURL(const GURL& url) {
  if (!has_committed_real_load_ && url != GURL(url::kAboutBlankURL))
    has_committed_real_load_ = true;
  current_frame_host()->set_last_committed_url(url);
  blame_context_.TakeSnapshot();
}

void FrameTreeNode::SetCurrentOrigin(
    const url::Origin& origin,
    bool is_potentially_trustworthy_unique_origin) {
  if (!origin.IsSameOriginWith(replication_state_.origin) ||
      replication_state_.has_potentially_trustworthy_unique_origin !=
          is_potentially_trustworthy_unique_origin) {
    render_manager_.OnDidUpdateOrigin(origin,
                                      is_potentially_trustworthy_unique_origin);
  }
  replication_state_.origin = origin;
  replication_state_.has_potentially_trustworthy_unique_origin =
      is_potentially_trustworthy_unique_origin;
}

void FrameTreeNode::SetFrameName(const std::string& name,
                                 const std::string& unique_name) {
  if (name == replication_state_.name) {
    // |unique_name| shouldn't change unless |name| changes.
    DCHECK_EQ(unique_name, replication_state_.unique_name);
    return;
  }
  RecordUniqueNameLength(unique_name.size());
  render_manager_.OnDidUpdateName(name, unique_name);
  replication_state_.name = name;
  replication_state_.unique_name = unique_name;
}

void FrameTreeNode::AddContentSecurityPolicy(
    const ContentSecurityPolicyHeader& header) {
  replication_state_.accumulated_csp_headers.push_back(header);
  render_manager_.OnDidAddContentSecurityPolicy(header);
}

void FrameTreeNode::ResetContentSecurityPolicy() {
  replication_state_.accumulated_csp_headers.clear();
  render_manager_.OnDidResetContentSecurityPolicy();
}

void FrameTreeNode::SetInsecureRequestPolicy(
    blink::WebInsecureRequestPolicy policy) {
  if (policy == replication_state_.insecure_request_policy)
    return;
  render_manager_.OnEnforceInsecureRequestPolicy(policy);
  replication_state_.insecure_request_policy = policy;
}

void FrameTreeNode::SetPendingSandboxFlags(
    blink::WebSandboxFlags sandbox_flags) {
  pending_sandbox_flags_ = sandbox_flags;

  // Subframes should always inherit their parent's sandbox flags.
  if (parent())
    pending_sandbox_flags_ |= parent()->effective_sandbox_flags();
}

bool FrameTreeNode::IsDescendantOf(FrameTreeNode* other) const {
  if (!other || !other->child_count())
    return false;

  for (FrameTreeNode* node = parent(); node; node = node->parent()) {
    if (node == other)
      return true;
  }

  return false;
}

FrameTreeNode* FrameTreeNode::PreviousSibling() const {
  return GetSibling(-1);
}

FrameTreeNode* FrameTreeNode::NextSibling() const {
  return GetSibling(1);
}

bool FrameTreeNode::IsLoading() const {
  RenderFrameHostImpl* current_frame_host =
      render_manager_.current_frame_host();
  RenderFrameHostImpl* pending_frame_host =
      render_manager_.pending_frame_host();

  DCHECK(current_frame_host);

  if (IsBrowserSideNavigationEnabled()) {
    if (navigation_request_)
      return true;

    RenderFrameHostImpl* speculative_frame_host =
        render_manager_.speculative_frame_host();
    if (speculative_frame_host && speculative_frame_host->is_loading())
      return true;
  } else {
    if (pending_frame_host && pending_frame_host->is_loading())
      return true;
  }
  return current_frame_host->is_loading();
}

bool FrameTreeNode::CommitPendingSandboxFlags() {
  bool did_change_flags =
      pending_sandbox_flags_ != replication_state_.sandbox_flags;
  replication_state_.sandbox_flags = pending_sandbox_flags_;
  return did_change_flags;
}

void FrameTreeNode::CreatedNavigationRequest(
    std::unique_ptr<NavigationRequest> navigation_request) {
  CHECK(IsBrowserSideNavigationEnabled());

  bool was_previously_loading = frame_tree()->IsLoading();

  // There's no need to reset the state: there's still an ongoing load, and the
  // RenderFrameHostManager will take care of updates to the speculative
  // RenderFrameHost in DidCreateNavigationRequest below.
  if (was_previously_loading)
    ResetNavigationRequest(true);

  navigation_request_ = std::move(navigation_request);
  render_manager()->DidCreateNavigationRequest(navigation_request_.get());

  // Force the throbber to start to keep it in sync with what is happening in
  // the UI. Blink doesn't send throb notifications for JavaScript URLs, so it
  // is not done here either.
  if (!navigation_request_->common_params().url.SchemeIs(
          url::kJavaScriptScheme)) {
    // TODO(fdegans): Check if this is a same-document navigation and set the
    // proper argument.
    DidStartLoading(true, was_previously_loading);
  }
}

void FrameTreeNode::ResetNavigationRequest(bool keep_state) {
  CHECK(IsBrowserSideNavigationEnabled());
  if (!navigation_request_)
    return;
  bool was_renderer_initiated = !navigation_request_->browser_initiated();
  NavigationRequest::AssociatedSiteInstanceType site_instance_type =
      navigation_request_->associated_site_instance_type();
  navigation_request_.reset();

  if (keep_state)
    return;

  // The RenderFrameHostManager should clean up any speculative RenderFrameHost
  // it created for the navigation. Also register that the load stopped.
  DidStopLoading();
  render_manager_.CleanUpNavigation();

  // When reusing the same SiteInstance, a pending WebUI may have been created
  // on behalf of the navigation in the current RenderFrameHost. Clear it.
  if (site_instance_type ==
      NavigationRequest::AssociatedSiteInstanceType::CURRENT) {
    current_frame_host()->ClearPendingWebUI();
  }

  // If the navigation is renderer-initiated, the renderer should also be
  // informed that the navigation stopped.
  if (was_renderer_initiated) {
    current_frame_host()->Send(
        new FrameMsg_Stop(current_frame_host()->GetRoutingID()));
  }

}

bool FrameTreeNode::has_started_loading() const {
  return loading_progress_ != kLoadingProgressNotStarted;
}

void FrameTreeNode::reset_loading_progress() {
  loading_progress_ = kLoadingProgressNotStarted;
}

void FrameTreeNode::DidStartLoading(bool to_different_document,
                                    bool was_previously_loading) {
  // Any main frame load to a new document should reset the load progress since
  // it will replace the current page and any frames. The WebContents will
  // be notified when DidChangeLoadProgress is called.
  if (to_different_document && IsMainFrame())
    frame_tree_->ResetLoadProgress();

  // Notify the WebContents.
  if (!was_previously_loading)
    navigator()->GetDelegate()->DidStartLoading(this, to_different_document);

  // Set initial load progress and update overall progress. This will notify
  // the WebContents of the load progress change.
  DidChangeLoadProgress(kLoadingProgressMinimum);

  // Notify the RenderFrameHostManager of the event.
  render_manager()->OnDidStartLoading();
}

void FrameTreeNode::DidStopLoading() {
  // TODO(erikchen): Remove ScopedTracker below once crbug.com/465796 is fixed.
  tracked_objects::ScopedTracker tracking_profile1(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "465796 FrameTreeNode::DidStopLoading::Start"));

  // Set final load progress and update overall progress. This will notify
  // the WebContents of the load progress change.
  DidChangeLoadProgress(kLoadingProgressDone);

  // TODO(erikchen): Remove ScopedTracker below once crbug.com/465796 is fixed.
  tracked_objects::ScopedTracker tracking_profile2(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "465796 FrameTreeNode::DidStopLoading::WCIDidStopLoading"));

  // Notify the WebContents.
  if (!frame_tree_->IsLoading())
    navigator()->GetDelegate()->DidStopLoading();

  // TODO(erikchen): Remove ScopedTracker below once crbug.com/465796 is fixed.
  tracked_objects::ScopedTracker tracking_profile3(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "465796 FrameTreeNode::DidStopLoading::RFHMDidStopLoading"));

  // Notify the RenderFrameHostManager of the event.
  render_manager()->OnDidStopLoading();

  // TODO(erikchen): Remove ScopedTracker below once crbug.com/465796 is fixed.
  tracked_objects::ScopedTracker tracking_profile4(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "465796 FrameTreeNode::DidStopLoading::End"));
}

void FrameTreeNode::DidChangeLoadProgress(double load_progress) {
  loading_progress_ = load_progress;
  frame_tree_->UpdateLoadProgress();
}

bool FrameTreeNode::StopLoading() {
  if (IsBrowserSideNavigationEnabled())
    ResetNavigationRequest(false);

  // TODO(nasko): see if child frames should send IPCs in site-per-process
  // mode.
  if (!IsMainFrame())
    return true;

  render_manager_.Stop();
  return true;
}

void FrameTreeNode::DidFocus() {
  last_focus_time_ = base::TimeTicks::Now();
  FOR_EACH_OBSERVER(Observer, observers_, OnFrameTreeNodeFocused(this));
}

void FrameTreeNode::BeforeUnloadCanceled() {
  // TODO(clamy): Support BeforeUnload in subframes.
  if (!IsMainFrame())
    return;

  RenderFrameHostImpl* current_frame_host =
      render_manager_.current_frame_host();
  DCHECK(current_frame_host);
  current_frame_host->ResetLoadingState();

  if (IsBrowserSideNavigationEnabled()) {
    RenderFrameHostImpl* speculative_frame_host =
        render_manager_.speculative_frame_host();
    if (speculative_frame_host)
      speculative_frame_host->ResetLoadingState();
  } else {
    RenderFrameHostImpl* pending_frame_host =
        render_manager_.pending_frame_host();
    if (pending_frame_host)
      pending_frame_host->ResetLoadingState();
  }
}

FrameTreeNode* FrameTreeNode::GetSibling(int relative_offset) const {
  if (!parent_ || !parent_->child_count())
    return nullptr;

  for (size_t i = 0; i < parent_->child_count(); ++i) {
    if (parent_->child_at(i) == this) {
      if ((relative_offset < 0 && static_cast<size_t>(-relative_offset) > i) ||
          i + relative_offset >= parent_->child_count()) {
        return nullptr;
      }
      return parent_->child_at(i + relative_offset);
    }
  }

  NOTREACHED() << "FrameTreeNode not found in its parent's children.";
  return nullptr;
}

}  // namespace content
