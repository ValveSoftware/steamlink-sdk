// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_NAVIGATION_HANDLE_IMPL_H_
#define CONTENT_BROWSER_FRAME_HOST_NAVIGATION_HANDLE_IMPL_H_

#include "content/public/browser/navigation_handle.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/navigation_data.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/common/request_context_type.h"
#include "url/gurl.h"

struct FrameHostMsg_DidCommitProvisionalLoad_Params;

namespace content {

class NavigationUIData;
class NavigatorDelegate;
class ResourceRequestBodyImpl;
class ServiceWorkerContextWrapper;
class ServiceWorkerNavigationHandle;

// This class keeps track of a single navigation. It is created upon receipt of
// a DidStartProvisionalLoad IPC in a RenderFrameHost. The RenderFrameHost owns
// the newly created NavigationHandleImpl as long as the navigation is ongoing.
// The NavigationHandleImpl in the RenderFrameHost will be reset when the
// navigation stops, that is if one of the following events happen:
//   - The RenderFrameHost receives a DidStartProvisionalLoad IPC for a new
//   navigation (see below for special cases where the DidStartProvisionalLoad
//   message does not indicate the start of a new navigation).
//   - The RenderFrameHost stops loading.
//   - The RenderFrameHost receives a DidDropNavigation IPC.
//
// When the navigation encounters an error, the DidStartProvisionalLoad marking
// the start of the load of the error page will not be considered as marking a
// new navigation. It will not reset the NavigationHandleImpl in the
// RenderFrameHost.
//
// If the navigation needs a cross-site transfer, then the NavigationHandleImpl
// will briefly be held by the RenderFrameHostManager, until a suitable
// RenderFrameHost for the navigation has been found. The ownership of the
// NavigationHandleImpl will then be transferred to the new RenderFrameHost.
// The DidStartProvisionalLoad received by the new RenderFrameHost for the
// transferring navigation will not reset the NavigationHandleImpl, as it does
// not mark the start of a new navigation.
//
// PlzNavigate: the NavigationHandleImpl is created just after creating a new
// NavigationRequest. It is then owned by the NavigationRequest until the
// navigation is ready to commit. The NavigationHandleImpl ownership is then
// transferred to the RenderFrameHost in which the navigation will commit.
//
// When PlzNavigate is enabled, the NavigationHandleImpl will never be reset
// following the receipt of a DidStartProvisionalLoad IPC. There are also no
// transferring navigations. The other causes of NavigationHandleImpl reset in
// the RenderFrameHost still apply.
class CONTENT_EXPORT NavigationHandleImpl : public NavigationHandle {
 public:
  // |navigation_start| comes from the DidStartProvisionalLoad IPC, which tracks
  // both renderer-initiated and browser-initiated navigation start.
  // PlzNavigate: This value always comes from the CommonNavigationParams
  // associated with this navigation.
  static std::unique_ptr<NavigationHandleImpl> Create(
      const GURL& url,
      FrameTreeNode* frame_tree_node,
      bool is_renderer_initiated,
      bool is_same_page,
      bool is_srcdoc,
      const base::TimeTicks& navigation_start,
      int pending_nav_entry_id,
      bool started_from_context_menu);
  ~NavigationHandleImpl() override;

  // NavigationHandle implementation:
  const GURL& GetURL() override;
  SiteInstance* GetStartingSiteInstance() override;
  bool IsInMainFrame() override;
  bool IsParentMainFrame() override;
  bool IsRendererInitiated() override;
  bool IsSrcdoc() override;
  bool WasServerRedirect() override;
  int GetFrameTreeNodeId() override;
  int GetParentFrameTreeNodeId() override;
  const base::TimeTicks& NavigationStart() override;
  bool IsPost() override;
  const Referrer& GetReferrer() override;
  bool HasUserGesture() override;
  ui::PageTransition GetPageTransition() override;
  bool IsExternalProtocol() override;
  net::Error GetNetErrorCode() override;
  RenderFrameHostImpl* GetRenderFrameHost() override;
  bool IsSamePage() override;
  bool HasCommitted() override;
  bool IsErrorPage() override;
  const net::HttpResponseHeaders* GetResponseHeaders() override;
  net::HttpResponseInfo::ConnectionInfo GetConnectionInfo() override;
  void Resume() override;
  void CancelDeferredNavigation(
      NavigationThrottle::ThrottleCheckResult result) override;
  void RegisterThrottleForTesting(
      std::unique_ptr<NavigationThrottle> navigation_throttle) override;
  NavigationThrottle::ThrottleCheckResult CallWillStartRequestForTesting(
      bool is_post,
      const Referrer& sanitized_referrer,
      bool has_user_gesture,
      ui::PageTransition transition,
      bool is_external_protocol) override;
  NavigationThrottle::ThrottleCheckResult CallWillRedirectRequestForTesting(
      const GURL& new_url,
      bool new_method_is_post,
      const GURL& new_referrer_url,
      bool new_is_external_protocol) override;
  NavigationThrottle::ThrottleCheckResult CallWillProcessResponseForTesting(
      RenderFrameHost* render_frame_host,
      const std::string& raw_response_header) override;
  void CallDidCommitNavigationForTesting(const GURL& url) override;
  bool WasStartedFromContextMenu() const override;
  const GURL& GetSearchableFormURL() override;
  const std::string& GetSearchableFormEncoding() override;
  const GlobalRequestID& GetGlobalRequestID() override;

  NavigationData* GetNavigationData() override;

  // The NavigatorDelegate to notify/query for various navigation events.
  // Normally this is the WebContents, except if this NavigationHandle was
  // created during a navigation to an interstitial page. In this case it will
  // be the InterstitialPage itself.
  //
  // Note: due to the interstitial navigation case, all calls that can possibly
  // expose the NavigationHandle to code outside of content/ MUST go though the
  // NavigatorDelegate. In particular, the ContentBrowserClient should not be
  // called directly form the NavigationHandle code. Thus, these calls will not
  // expose the NavigationHandle when navigating to an InterstialPage.
  NavigatorDelegate* GetDelegate() const;

  RequestContextType GetRequestContextType() const;

  // Get the unique id from the NavigationEntry associated with this
  // NavigationHandle. Note that a synchronous, renderer-initiated navigation
  // will not have a NavigationEntry associated with it, and this will return 0.
  int pending_nav_entry_id() const { return pending_nav_entry_id_; }

  // Changes the pending NavigationEntry ID for this handle.  This is currently
  // required during transfer navigations.
  // TODO(creis): Remove this when transfer navigations do not require pending
  // entries.  See https://crbug.com/495161.
  void update_entry_id_for_transfer(int nav_entry_id) {
    pending_nav_entry_id_ = nav_entry_id;
  }

  void set_net_error_code(net::Error net_error_code) {
    net_error_code_ = net_error_code;
  }

  // Returns whether the navigation is currently being transferred from one
  // RenderFrameHost to another. In particular, a DidStartProvisionalLoad IPC
  // for the navigation URL, received in the new RenderFrameHost, should not
  // indicate the start of a new navigation in that case.
  bool is_transferring() const { return is_transferring_; }
  void set_is_transferring(bool is_transferring) {
    is_transferring_ = is_transferring;
  }

  // Updates the RenderFrameHost that is about to commit the navigation. This
  // is used during transfer navigations.
  void set_render_frame_host(RenderFrameHostImpl* render_frame_host) {
    render_frame_host_ = render_frame_host;
  }

  // Returns the POST body associated with this navigation.  This will be
  // null for GET and/or other non-POST requests (or if a response to a POST
  // request was a redirect that changed the method to GET - for example 302).
  const scoped_refptr<ResourceRequestBodyImpl>& resource_request_body() const {
    return resource_request_body_;
  }

  // PlzNavigate
  void InitServiceWorkerHandle(
      ServiceWorkerContextWrapper* service_worker_context);
  ServiceWorkerNavigationHandle* service_worker_handle() const {
    return service_worker_handle_.get();
  }

  typedef base::Callback<void(NavigationThrottle::ThrottleCheckResult)>
      ThrottleChecksFinishedCallback;

  // Called when the URLRequest will start in the network stack.  |callback|
  // will be called when all throttle checks have completed. This will allow
  // the caller to cancel the navigation or let it proceed.
  void WillStartRequest(
      const std::string& method,
      scoped_refptr<content::ResourceRequestBodyImpl> resource_request_body,
      const Referrer& sanitized_referrer,
      bool has_user_gesture,
      ui::PageTransition transition,
      bool is_external_protocol,
      RequestContextType request_context_type,
      const ThrottleChecksFinishedCallback& callback);

  // Called when the URLRequest will be redirected in the network stack.
  // |callback| will be called when all throttles check have completed. This
  // will allow the caller to cancel the navigation or let it proceed.
  // This will also inform the delegate that the request was redirected.
  void WillRedirectRequest(
      const GURL& new_url,
      const std::string& new_method,
      const GURL& new_referrer_url,
      bool new_is_external_protocol,
      scoped_refptr<net::HttpResponseHeaders> response_headers,
      net::HttpResponseInfo::ConnectionInfo connection_info,
      const ThrottleChecksFinishedCallback& callback);

  // Called when the URLRequest has delivered response headers and metadata.
  // |callback| will be called when all throttle checks have completed,
  // allowing the caller to cancel the navigation or let it proceed.
  // NavigationHandle will not call |callback| with a result of DEFER.
  // If the result is PROCEED, then 'ReadyToCommitNavigation' will be called
  // with |render_frame_host| and |response_headers| just before calling
  // |callback|. Should a transfer navigation happen, |transfer_callback| will
  // be run on the IO thread.
  // PlzNavigate: transfer navigations are not possible.
  void WillProcessResponse(
      RenderFrameHostImpl* render_frame_host,
      scoped_refptr<net::HttpResponseHeaders> response_headers,
      net::HttpResponseInfo::ConnectionInfo connection_info,
      const SSLStatus& ssl_status,
      const GlobalRequestID& request_id,
      bool should_replace_current_entry,
      bool is_download,
      bool is_stream,
      const base::Closure& transfer_callback,
      const ThrottleChecksFinishedCallback& callback);

  // Returns the FrameTreeNode this navigation is happening in.
  FrameTreeNode* frame_tree_node() { return frame_tree_node_; }

  // Called when the navigation is ready to be committed in
  // |render_frame_host|. This will update the |state_| and inform the
  // delegate.
  void ReadyToCommitNavigation(RenderFrameHostImpl* render_frame_host);

  // Called when the navigation was committed in |render_frame_host|. This will
  // update the |state_|.
  void DidCommitNavigation(
      const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
      bool same_page,
      RenderFrameHostImpl* render_frame_host);

  // Called during commit. Takes ownership of the embedder's NavigationData
  // instance. This NavigationData may have been cloned prior to being added
  // here.
  void set_navigation_data(std::unique_ptr<NavigationData> navigation_data) {
    navigation_data_ = std::move(navigation_data);
  }

  SSLStatus ssl_status() { return ssl_status_; }

  // Called when the navigation is transferred to a different renderer.
  void Transfer();

  NavigationUIData* navigation_ui_data() const {
    return navigation_ui_data_.get();
  }

  void set_searchable_form_url(const GURL& url) { searchable_form_url_ = url; }
  void set_searchable_form_encoding(const std::string& encoding) {
    searchable_form_encoding_ = encoding;
  }

 private:
  friend class NavigationHandleImplTest;

  // Used to track the state the navigation is currently in.
  enum State {
    INITIAL = 0,
    WILL_SEND_REQUEST,
    DEFERRING_START,
    WILL_REDIRECT_REQUEST,
    DEFERRING_REDIRECT,
    CANCELING,
    WILL_PROCESS_RESPONSE,
    DEFERRING_RESPONSE,
    READY_TO_COMMIT,
    DID_COMMIT,
    DID_COMMIT_ERROR_PAGE,
  };

  NavigationHandleImpl(const GURL& url,
                       FrameTreeNode* frame_tree_node,
                       bool is_renderer_initiated,
                       bool is_same_page,
                       bool is_srcdoc,
                       const base::TimeTicks& navigation_start,
                       int pending_nav_entry_id,
                       bool started_from_context_menu);

  NavigationThrottle::ThrottleCheckResult CheckWillStartRequest();
  NavigationThrottle::ThrottleCheckResult CheckWillRedirectRequest();
  NavigationThrottle::ThrottleCheckResult CheckWillProcessResponse();

  // Called when WillProcessResponse checks are done, to find the final
  // RenderFrameHost for the navigation. Checks whether the navigation should be
  // transferred. Returns false if the transfer attempt results in the
  // destruction of this NavigationHandle and the navigation should no longer
  // proceed. This can happen when the RenderFrameHostManager determines a
  // transfer is needed, but WebContentsDelegate::ShouldTransferNavigation
  // returns false.
  bool MaybeTransferAndProceed();

  // Helper method for MaybeTransferAndProceed. Returns false if the transfer
  // attempt results in the destruction of this NavigationHandle.
  bool MaybeTransferAndProceedInternal();

  // Helper function to run and reset the |complete_callback_|. This marks the
  // end of a round of NavigationThrottleChecks.
  void RunCompleteCallback(NavigationThrottle::ThrottleCheckResult result);

  // Used in tests.
  State state() const { return state_; }

  // Populates |throttles_| with the throttles for this navigation.
  void RegisterNavigationThrottles();

  // See NavigationHandle for a description of those member variables.
  GURL url_;
  scoped_refptr<SiteInstance> starting_site_instance_;
  Referrer sanitized_referrer_;
  bool has_user_gesture_;
  ui::PageTransition transition_;
  bool is_external_protocol_;
  net::Error net_error_code_;
  RenderFrameHostImpl* render_frame_host_;
  const bool is_renderer_initiated_;
  const bool is_same_page_;
  const bool is_srcdoc_;
  bool was_redirected_;
  scoped_refptr<net::HttpResponseHeaders> response_headers_;
  net::HttpResponseInfo::ConnectionInfo connection_info_;

  // The original url of the navigation. This may differ from |url_| if the
  // navigation encounters redirects.
  const GURL original_url_;

  // The HTTP method used for the navigation.
  std::string method_;

  // The POST body associated with this navigation.  This will be null for GET
  // and/or other non-POST requests (or if a response to a POST request was a
  // redirect that changed the method to GET - for example 302).
  scoped_refptr<ResourceRequestBodyImpl> resource_request_body_;

  // The state the navigation is in.
  State state_;

  // Whether the navigation is in the middle of a transfer. Set to false when
  // the DidStartProvisionalLoad is received from the new renderer.
  bool is_transferring_;

  // The FrameTreeNode this navigation is happening in.
  FrameTreeNode* frame_tree_node_;

  // A list of Throttles registered for this navigation.
  ScopedVector<NavigationThrottle> throttles_;

  // The index of the next throttle to check.
  size_t next_index_;

  // The time this navigation started.
  const base::TimeTicks navigation_start_;

  // The unique id of the corresponding NavigationEntry.
  int pending_nav_entry_id_;

  // The fetch request context type.
  RequestContextType request_context_type_;

  // This callback will be run when all throttle checks have been performed.
  ThrottleChecksFinishedCallback complete_callback_;

  // PlzNavigate
  // Manages the lifetime of a pre-created ServiceWorkerProviderHost until a
  // corresponding ServiceWorkerNetworkProvider is created in the renderer.
  std::unique_ptr<ServiceWorkerNavigationHandle> service_worker_handle_;

  // Embedder data from the IO thread tied to this navigation.
  std::unique_ptr<NavigationData> navigation_data_;

  // PlzNavigate
  // Embedder data from the UI thread tied to this navigation.
  std::unique_ptr<NavigationUIData> navigation_ui_data_;

  SSLStatus ssl_status_;

  // The id of the URLRequest tied to this navigation.
  GlobalRequestID request_id_;

  // Whether the current NavigationEntry should be replaced upon commit.
  bool should_replace_current_entry_;

  // The chain of redirects.
  std::vector<GURL> redirect_chain_;

  // A callback to run on the IO thread if the navigation transfers.
  base::Closure transfer_callback_;

  // Whether the navigation ended up being a download or a stream.
  bool is_download_;
  bool is_stream_;

  // False by default unless the navigation started within a context menu.
  bool started_from_context_menu_;

  GURL searchable_form_url_;
  std::string searchable_form_encoding_;

  base::WeakPtrFactory<NavigationHandleImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(NavigationHandleImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_NAVIGATION_HANDLE_IMPL_H_
