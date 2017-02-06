// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_NAVIGATION_REQUEST_H_
#define CONTENT_BROWSER_FRAME_HOST_NAVIGATION_REQUEST_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/loader/navigation_url_loader_delegate.h"
#include "content/common/content_export.h"
#include "content/common/frame_message_enums.h"
#include "content/common/navigation_params.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {

class FrameNavigationEntry;
class FrameTreeNode;
class NavigationControllerImpl;
class NavigationHandleImpl;
class NavigationURLLoader;
class NavigationData;
class NavigatorDelegate;
class ResourceRequestBody;
class SiteInstanceImpl;
class StreamHandle;
struct NavigationRequestInfo;

// PlzNavigate
// A UI thread object that owns a navigation request until it commits. It
// ensures the UI thread can start a navigation request in the
// ResourceDispatcherHost (that lives on the IO thread).
// TODO(clamy): Describe the interactions between the UI and IO thread during
// the navigation following its refactoring.
class CONTENT_EXPORT NavigationRequest : public NavigationURLLoaderDelegate {
 public:
  // Keeps track of the various stages of a NavigationRequest.
  enum NavigationState {
    // Initial state.
    NOT_STARTED = 0,

    // Waiting for a BeginNavigation IPC from the renderer in a
    // browser-initiated navigation. If there is no live renderer when the
    // request is created, this stage is skipped.
    WAITING_FOR_RENDERER_RESPONSE,

    // The request was sent to the IO thread.
    STARTED,

    // The response started on the IO thread and is ready to be committed. This
    // is one of the two final states for the request.
    RESPONSE_STARTED,

    // The request failed on the IO thread and an error page should be
    // displayed. This is one of the two final states for the request.
    FAILED,
  };

  // The SiteInstance currently associated with the navigation. Note that the
  // final value will only be known when the response is received, or the
  // navigation fails, as server redirects can modify the SiteInstance to use
  // for the navigation.
  enum class AssociatedSiteInstanceType {
    NONE = 0,
    CURRENT,
    SPECULATIVE,
  };

  // Creates a request for a browser-intiated navigation.
  static std::unique_ptr<NavigationRequest> CreateBrowserInitiated(
      FrameTreeNode* frame_tree_node,
      const GURL& dest_url,
      const Referrer& dest_referrer,
      const FrameNavigationEntry& frame_entry,
      const NavigationEntryImpl& entry,
      FrameMsg_Navigate_Type::Value navigation_type,
      LoFiState lofi_state,
      bool is_same_document_history_load,
      const base::TimeTicks& navigation_start,
      NavigationControllerImpl* controller);

  // Creates a request for a renderer-intiated navigation.
  // Note: |body| is sent to the IO thread when calling BeginNavigation, and
  // should no longer be manipulated afterwards on the UI thread.
  // TODO(clamy): see if ResourceRequestBody could be un-refcounted to avoid
  // threading subtleties.
  static std::unique_ptr<NavigationRequest> CreateRendererInitiated(
      FrameTreeNode* frame_tree_node,
      const CommonNavigationParams& common_params,
      const BeginNavigationParams& begin_params,
      int current_history_list_offset,
      int current_history_list_length);

  ~NavigationRequest() override;

  // Called on the UI thread by the Navigator to start the navigation.
  void BeginNavigation();

  const CommonNavigationParams& common_params() const { return common_params_; }

  const BeginNavigationParams& begin_params() const { return begin_params_; }

  const RequestNavigationParams& request_params() const {
    return request_params_;
  }

  NavigationURLLoader* loader_for_testing() const { return loader_.get(); }

  NavigationState state() const { return state_; }

  FrameTreeNode* frame_tree_node() const { return frame_tree_node_; }

  SiteInstanceImpl* source_site_instance() const {
    return source_site_instance_.get();
  }

  SiteInstanceImpl* dest_site_instance() const {
    return dest_site_instance_.get();
  }

  NavigationEntryImpl::RestoreType restore_type() const {
    return restore_type_;
  };

  bool is_view_source() const { return is_view_source_; };

  int bindings() const { return bindings_; };

  bool browser_initiated() const { return browser_initiated_ ; }

  void SetWaitingForRendererResponse() {
    DCHECK(state_ == NOT_STARTED);
    state_ = WAITING_FOR_RENDERER_RESPONSE;
  }

  AssociatedSiteInstanceType associated_site_instance_type() const {
    return associated_site_instance_type_;
  }
  void set_associated_site_instance_type(AssociatedSiteInstanceType type) {
    associated_site_instance_type_ = type;
  }

  NavigationHandleImpl* navigation_handle() const {
    return navigation_handle_.get();
  }

  // Creates a NavigationHandle. This should be called after any previous
  // NavigationRequest for the FrameTreeNode has been destroyed.
  void CreateNavigationHandle(int pending_nav_entry_id);

  // Transfers the ownership of the NavigationHandle to |render_frame_host|.
  // This should be called when the navigation is ready to commit, because the
  // NavigationHandle outlives the NavigationRequest. The NavigationHandle's
  // lifetime is the entire navigation, while the NavigationRequest is
  // destroyed when a navigation is ready for commit.
  void TransferNavigationHandleOwnership(
      RenderFrameHostImpl* render_frame_host);

 private:
  NavigationRequest(FrameTreeNode* frame_tree_node,
                    const CommonNavigationParams& common_params,
                    const BeginNavigationParams& begin_params,
                    const RequestNavigationParams& request_params,
                    bool browser_initiated,
                    const FrameNavigationEntry* frame_navigation_entry,
                    const NavigationEntryImpl* navitation_entry);

  // NavigationURLLoaderDelegate implementation.
  void OnRequestRedirected(
      const net::RedirectInfo& redirect_info,
      const scoped_refptr<ResourceResponse>& response) override;
  void OnResponseStarted(
      const scoped_refptr<ResourceResponse>& response,
      std::unique_ptr<StreamHandle> body,
      std::unique_ptr<NavigationData> navigation_data) override;
  void OnRequestFailed(bool has_stale_copy_in_cache, int net_error) override;
  void OnRequestStarted(base::TimeTicks timestamp) override;
  void OnServiceWorkerEncountered() override;

  // Called when the NavigationThrottles have been checked by the
  // NavigationHandle.
  void OnStartChecksComplete(NavigationThrottle::ThrottleCheckResult result);
  void OnRedirectChecksComplete(NavigationThrottle::ThrottleCheckResult result);
  void OnWillProcessResponseChecksComplete(
      NavigationThrottle::ThrottleCheckResult result);

  // Have a RenderFrameHost commit the navigation. The NavigationRequest will
  // be destroyed after this call.
  void CommitNavigation();

  FrameTreeNode* frame_tree_node_;

  // Initialized on creation of the NavigationRequest. Sent to the renderer when
  // the navigation is ready to commit.
  // Note: When the navigation is ready to commit, the url in |common_params|
  // will be set to the final navigation url, obtained after following all
  // redirects.
  // Note: |common_params_| and |begin_params_| are not const as they can be
  // modified during redirects.
  // Note: |request_params_| is not const because service_worker_provider_id
  // and should_create_service_worker will be set in OnResponseStarted.
  CommonNavigationParams common_params_;
  BeginNavigationParams begin_params_;
  RequestNavigationParams request_params_;
  const bool browser_initiated_;

  NavigationState state_;


  // The parameters to send to the IO thread. |loader_| takes ownership of
  // |info_| after calling BeginNavigation.
  std::unique_ptr<NavigationRequestInfo> info_;

  std::unique_ptr<NavigationURLLoader> loader_;

  // These next items are used in browser-initiated navigations to store
  // information from the NavigationEntryImpl that is required after request
  // creation time.
  scoped_refptr<SiteInstanceImpl> source_site_instance_;
  scoped_refptr<SiteInstanceImpl> dest_site_instance_;
  NavigationEntryImpl::RestoreType restore_type_;
  bool is_view_source_;
  int bindings_;

  // The type of SiteInstance associated with this navigation.
  AssociatedSiteInstanceType associated_site_instance_type_;

  std::unique_ptr<NavigationHandleImpl> navigation_handle_;

  // Holds the ResourceResponse and the StreamHandle for the navigation while
  // the WillProcessResponse checks are performed by the NavigationHandle.
  scoped_refptr<ResourceResponse> response_;
  std::unique_ptr<StreamHandle> body_;

  DISALLOW_COPY_AND_ASSIGN(NavigationRequest);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_NAVIGATION_REQUEST_H_
