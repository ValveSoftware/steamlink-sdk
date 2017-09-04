// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_NAVIGATOR_H_
#define CONTENT_BROWSER_FRAME_HOST_NAVIGATOR_H_

#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "content/browser/frame_host/navigation_handle_impl.h"
#include "content/browser/frame_host/navigator_delegate.h"
#include "content/common/content_export.h"
#include "content/public/browser/navigation_controller.h"
#include "ui/base/window_open_disposition.h"

class GURL;
struct FrameHostMsg_DidCommitProvisionalLoad_Params;
struct FrameHostMsg_DidFailProvisionalLoadWithError_Params;

namespace base {
class TimeTicks;
}

namespace content {

class FrameNavigationEntry;
class FrameTreeNode;
class NavigationRequest;
class RenderFrameHostImpl;
class ResourceRequestBodyImpl;
struct BeginNavigationParams;
struct CommonNavigationParams;

// Implementations of this interface are responsible for performing navigations
// in a node of the FrameTree. Its lifetime is bound to all FrameTreeNode
// objects that are using it and will be released once all nodes that use it are
// freed. The Navigator is bound to a single frame tree and cannot be used by
// multiple instances of FrameTree.
// TODO(nasko): Move all navigation methods, such as didStartProvisionalLoad
// from WebContentsImpl to this interface.
class CONTENT_EXPORT Navigator : public base::RefCounted<Navigator> {
 public:
  // Returns the delegate of this Navigator.
  virtual NavigatorDelegate* GetDelegate();

  // Returns the NavigationController associated with this Navigator.
  virtual NavigationController* GetController();

  // Notifications coming from the RenderFrameHosts ----------------------------

  // The RenderFrameHostImpl started a provisional load.
  virtual void DidStartProvisionalLoad(
      RenderFrameHostImpl* render_frame_host,
      const GURL& url,
      const base::TimeTicks& navigation_start) {};

  // The RenderFrameHostImpl has failed a provisional load.
  virtual void DidFailProvisionalLoadWithError(
      RenderFrameHostImpl* render_frame_host,
      const FrameHostMsg_DidFailProvisionalLoadWithError_Params& params) {};

  // The RenderFrameHostImpl has failed to load the document.
  virtual void DidFailLoadWithError(
      RenderFrameHostImpl* render_frame_host,
      const GURL& url,
      int error_code,
      const base::string16& error_description,
      bool was_ignored_by_handler) {}

  // The RenderFrameHostImpl has committed a navigation. The Navigator is
  // responsible for resetting |navigation_handle| at the end of this method and
  // should not attempt to keep it alive.
  // Note: it is possible that |navigation_handle| is not the NavigationHandle
  // stored in the RenderFrameHost that just committed. This happens for example
  // when a same-page navigation commits while another navigation is ongoing.
  // The Navigator should use the NavigationHandle provided by this method and
  // not attempt to access the RenderFrameHost's NavigationsHandle.
  virtual void DidNavigate(
      RenderFrameHostImpl* render_frame_host,
      const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
      std::unique_ptr<NavigationHandleImpl> navigation_handle) {}

  // Called by the NavigationController to cause the Navigator to navigate
  // to the current pending entry. The NavigationController should be called
  // back with RendererDidNavigate on success or DiscardPendingEntry on failure.
  // The callbacks can be inside of this function, or at some future time.
  //
  // If this method returns false, then the navigation is discarded (equivalent
  // to calling DiscardPendingEntry on the NavigationController).
  //
  // TODO(nasko): Remove this method from the interface, since Navigator and
  // NavigationController know about each other. This will be possible once
  // initialization of Navigator and NavigationController is properly done.
  virtual bool NavigateToPendingEntry(FrameTreeNode* frame_tree_node,
                                      const FrameNavigationEntry& frame_entry,
                                      ReloadType reload_type,
                                      bool is_same_document_history_load);

  // Called on a newly created subframe during a history navigation. The browser
  // process looks up the corresponding FrameNavigationEntry for the new frame
  // navigates it in the correct process. Returns false if the
  // FrameNavigationEntry can't be found or the navigation fails. This is only
  // used in OOPIF-enabled modes.
  // TODO(creis): Remove |default_url| once we have collected UMA stats on the
  // cases that we use a different URL from history than the frame's src.
  virtual bool NavigateNewChildFrame(RenderFrameHostImpl* render_frame_host,
                                     const GURL& default_url);

  // Navigation requests -------------------------------------------------------

  virtual base::TimeTicks GetCurrentLoadStart();

  // The RenderFrameHostImpl has received a request to open a URL with the
  // specified |disposition|.
  virtual void RequestOpenURL(
      RenderFrameHostImpl* render_frame_host,
      const GURL& url,
      bool uses_post,
      const scoped_refptr<ResourceRequestBodyImpl>& body,
      const std::string& extra_headers,
      const Referrer& referrer,
      WindowOpenDisposition disposition,
      bool should_replace_current_entry,
      bool user_gesture) {}

  // The RenderFrameHostImpl wants to transfer the request to a new renderer.
  // |redirect_chain| contains any redirect URLs (excluding |url|) that happened
  // before the transfer.  If |method| is "POST", then |post_body| needs to
  // specify the request body, otherwise |post_body| should be null.
  virtual void RequestTransferURL(
      RenderFrameHostImpl* render_frame_host,
      const GURL& url,
      SiteInstance* source_site_instance,
      const std::vector<GURL>& redirect_chain,
      const Referrer& referrer,
      ui::PageTransition page_transition,
      const GlobalRequestID& transferred_global_request_id,
      bool should_replace_current_entry,
      const std::string& method,
      scoped_refptr<ResourceRequestBodyImpl> post_body,
      const std::string& extra_headers) {}

  // PlzNavigate
  // Called after receiving a BeforeUnloadACK IPC from the renderer. If
  // |frame_tree_node| has a NavigationRequest waiting for the renderer
  // response, then the request is either started or canceled, depending on the
  // value of |proceed|.
  virtual void OnBeforeUnloadACK(FrameTreeNode* frame_tree_node,
                                 bool proceed) {}

  // PlzNavigate
  // Used to start a new renderer-initiated navigation, following a
  // BeginNavigation IPC from the renderer.
  virtual void OnBeginNavigation(FrameTreeNode* frame_tree_node,
                                 const CommonNavigationParams& common_params,
                                 const BeginNavigationParams& begin_params);

  // PlzNavigate
  // Called when a NavigationRequest for |frame_tree_node| failed. An
  // appropriate RenderFrameHost should be selected and asked to show an error
  // page. |has_stale_copy_in_cache| is true if there is a stale copy of the
  // unreachable page in cache.
  virtual void FailedNavigation(FrameTreeNode* frame_tree_node,
                                bool has_stale_copy_in_cache,
                                int error_code) {}

  // PlzNavigate
  // Cancel a NavigationRequest for |frame_tree_node|. Called when
  // |frame_tree_node| is destroyed.
  virtual void CancelNavigation(FrameTreeNode* frame_tree_node) {}

  // Called when the network stack started handling the navigation request
  // so that the |timestamp| when it happened can be recorded into an histogram.
  // The |url| is used to verify we're tracking the correct navigation.
  // TODO(carlosk): once PlzNavigate is the only navigation implementation
  // remove the URL parameter and rename this method to better suit its naming
  // conventions.
  virtual void LogResourceRequestTime(
    base::TimeTicks timestamp, const GURL& url) {};

  // Called to record the time it took to execute the before unload hook for the
  // current navigation.
  virtual void LogBeforeUnloadTime(
      const base::TimeTicks& renderer_before_unload_start_time,
      const base::TimeTicks& renderer_before_unload_end_time) {}

  // Called when a navigation has failed or the response is 204/205 to discard
  // the pending entry in order to avoid url spoofs.
  virtual void DiscardPendingEntryIfNeeded(NavigationHandleImpl* handle) {}

 protected:
  friend class base::RefCounted<Navigator>;
  virtual ~Navigator() {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_NAVIGATOR_H_
