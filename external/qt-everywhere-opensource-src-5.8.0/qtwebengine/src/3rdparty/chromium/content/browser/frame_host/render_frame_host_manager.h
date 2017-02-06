// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_MANAGER_H_
#define CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_MANAGER_H_

#include <stdint.h>

#include <list>
#include <map>
#include <memory>
#include <unordered_map>

#include "base/containers/hash_tables.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/site_instance_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/common/referrer.h"
#include "ui/base/page_transition_types.h"
#include "url/origin.h"

namespace content {
class BrowserContext;
class CrossProcessFrameConnector;
class CrossSiteTransferringRequest;
class FrameNavigationEntry;
class FrameTreeNode;
class InterstitialPageImpl;
class NavigationControllerImpl;
class NavigationEntry;
class NavigationEntryImpl;
class NavigationHandleImpl;
class NavigationRequest;
class NavigatorTestWithBrowserSideNavigation;
class RenderFrameHostDelegate;
class RenderFrameHostManagerTest;
class RenderFrameProxyHost;
class RenderViewHost;
class RenderViewHostImpl;
class RenderWidgetHostDelegate;
class RenderWidgetHostView;
class TestWebContents;
class WebUIImpl;
struct CommonNavigationParams;
struct ContentSecurityPolicyHeader;
struct FrameReplicationState;

// Manages RenderFrameHosts for a FrameTreeNode. It maintains a
// current_frame_host() which is the content currently visible to the user. When
// a frame is told to navigate to a different web site (as determined by
// SiteInstance), it will replace its current RenderFrameHost with a new
// RenderFrameHost dedicated to the new SiteInstance, possibly in a new process.
//
// Cross-process navigation works like this:
//
// - RFHM::Navigate determines whether the destination is cross-site, and if so,
//   it creates a pending_render_frame_host_.
//
// - The pending RFH is created in the "navigations suspended" state, meaning no
//   navigation messages are sent to its renderer until the beforeunload handler
//   has a chance to run in the current RFH.
//
// - The current RFH runs its beforeunload handler. If it returns false, we
//   cancel all the pending logic. Otherwise we allow the pending RFH to send
//   the navigation request to its renderer.
//
// - ResourceDispatcherHost receives a ResourceRequest on the IO thread for the
//   main resource load from the pending RFH. It creates a
//   CrossSiteResourceHandler to check whether a process transfer is needed when
//   the request is ready to commit.
//
// - When RDH receives a response, the MimeTypeResourceHandler determines
//   whether it is a navigation type that doesn't commit (e.g. download, 204 or
//   error page). If so, it sends a message to the new renderer causing it to
//   cancel the request, and the request (e.g. the download) proceeds. In this
//   case, the pending RFH will never become the current RFH, but it remains
//   until the next DidNavigate event for this WebContentsImpl.
//
// - After RDH receives a response and determines that it is safe and not a
//   download, the CrossSiteResourceHandler checks whether a transfer for a
//   redirect is needed. If so, it pauses the network response and starts an
//   identical navigation in a new pending RFH. When the identical request is
//   later received by RDH, the response is transferred and unpaused.
//
// - Otherwise, the network response commits in the pending RFH's renderer,
//   which sends a DidCommitProvisionalLoad message back to the browser process.
//
// - RFHM::CommitPending makes visible the new RFH, and initiates the unload
//   handler in the old RFH. The unload handler will complete in the background.
//
// - RenderFrameHostManager may keep the previous RFH alive as a
//   RenderFrameProxyHost, to be used (for example) if the user goes back. The
//   process only stays live if another tab is using it, but if so, the existing
//   frame relationships will be maintained.
class CONTENT_EXPORT RenderFrameHostManager
    : public SiteInstanceImpl::Observer {
 public:
  // Functions implemented by our owner that we need.
  //
  // TODO(brettw) Clean this up! These are all the functions in WebContentsImpl
  // that are required to run this class. The design should probably be better
  // such that these are more clear.
  //
  // There is additional complexity that some of the functions we need in
  // WebContentsImpl are inherited and non-virtual. These are named with
  // "RenderManager" so that the duplicate implementation of them will be clear.
  //
  // Functions and parameters whose description are prefixed by PlzNavigate are
  // part of a navigation refactoring project, currently behind the
  // enable-browser-side-navigation flag. The idea is to move the logic behind
  // driving navigations from the renderer to the browser.
  class CONTENT_EXPORT Delegate {
   public:
    // Initializes the given renderer if necessary and creates the view ID
    // corresponding to this view host. If this method is not called and the
    // process is not shared, then the WebContentsImpl will act as though the
    // renderer is not running (i.e., it will render "sad tab"). This method is
    // automatically called from LoadURL.
    virtual bool CreateRenderViewForRenderManager(
        RenderViewHost* render_view_host,
        int opener_frame_routing_id,
        int proxy_routing_id,
        const FrameReplicationState& replicated_frame_state) = 0;
    virtual void CreateRenderWidgetHostViewForRenderManager(
        RenderViewHost* render_view_host) = 0;
    virtual bool CreateRenderFrameForRenderManager(
        RenderFrameHost* render_frame_host,
        int proxy_routing_id,
        int opener_routing_id,
        int parent_routing_id,
        int previous_sibling_routing_id) = 0;
    virtual void BeforeUnloadFiredFromRenderManager(
        bool proceed, const base::TimeTicks& proceed_time,
        bool* proceed_to_fire_unload) = 0;
    virtual void RenderProcessGoneFromRenderManager(
        RenderViewHost* render_view_host) = 0;
    virtual void UpdateRenderViewSizeForRenderManager() = 0;
    virtual void CancelModalDialogsForRenderManager() = 0;
    virtual void NotifySwappedFromRenderManager(RenderFrameHost* old_host,
                                                RenderFrameHost* new_host,
                                                bool is_main_frame) = 0;
    // TODO(nasko): This should be removed once extensions no longer use
    // NotificationService. See https://crbug.com/462682.
    virtual void NotifyMainFrameSwappedFromRenderManager(
        RenderViewHost* old_host,
        RenderViewHost* new_host) = 0;
    virtual NavigationControllerImpl&
        GetControllerForRenderManager() = 0;

    // Returns the navigation entry of the current navigation, or NULL if there
    // is none.
    virtual NavigationEntry*
        GetLastCommittedNavigationEntryForRenderManager() = 0;

    // Returns true if the location bar should be focused by default rather than
    // the page contents. The view calls this function when the tab is focused
    // to see what it should do.
    virtual bool FocusLocationBarByDefault() = 0;

    // Focuses the location bar.
    virtual void SetFocusToLocationBar(bool select_all) = 0;

    // Returns true if views created for this delegate should be created in a
    // hidden state.
    virtual bool IsHidden() = 0;

    // If the delegate is an inner WebContents, this method returns the
    // FrameTreeNode ID of the frame in the outer WebContents which hosts
    // the inner WebContents. Returns FrameTreeNode::kFrameTreeNodeInvalidId
    // if the delegate does not have an outer WebContents.
    virtual int GetOuterDelegateFrameTreeNodeId() = 0;

   protected:
    virtual ~Delegate() {}
  };

  // All three delegate pointers must be non-NULL and are not owned by this
  // class. They must outlive this class. The RenderViewHostDelegate and
  // RenderWidgetHostDelegate are what will be installed into all
  // RenderViewHosts that are created.
  //
  // You must call Init() before using this class.
  RenderFrameHostManager(
      FrameTreeNode* frame_tree_node,
      RenderFrameHostDelegate* render_frame_delegate,
      RenderWidgetHostDelegate* render_widget_delegate,
      Delegate* delegate);
  ~RenderFrameHostManager();

  // For arguments, see WebContentsImpl constructor.
  void Init(SiteInstance* site_instance,
            int32_t view_routing_id,
            int32_t frame_routing_id,
            int32_t widget_routing_id);

  // Returns the currently active RenderFrameHost.
  //
  // This will be non-NULL between Init() and Shutdown(). You may want to NULL
  // check it in many cases, however. Windows can send us messages during the
  // destruction process after it has been shut down.
  RenderFrameHostImpl* current_frame_host() const {
    return render_frame_host_.get();
  }

  // TODO(creis): Remove this when we no longer use RVH for navigation.
  RenderViewHostImpl* current_host() const;

  // Returns the view associated with the current RenderViewHost, or NULL if
  // there is no current one.
  RenderWidgetHostView* GetRenderWidgetHostView() const;

  // Returns whether this manager belongs to a FrameTreeNode that belongs to an
  // inner WebContents.
  bool ForInnerDelegate();

  // Returns the RenderWidgetHost of the outer WebContents (if any) that can be
  // used to fetch the last keyboard event.
  // TODO(lazyboy): This can be removed once input events are sent directly to
  // remote frames.
  RenderWidgetHostImpl* GetOuterRenderWidgetHostForKeyboardInput();

  // Return the FrameTreeNode for the frame in the outer WebContents (if any)
  // that contains the inner WebContents.
  FrameTreeNode* GetOuterDelegateNode();

  RenderFrameProxyHost* GetProxyToParent();

  // Returns the proxy to inner WebContents in the outer WebContents's
  // SiteInstance. Returns nullptr if this WebContents isn't part of inner/outer
  // relationship.
  RenderFrameProxyHost* GetProxyToOuterDelegate();

  // Removes the FrameTreeNode in the outer WebContents that represents this
  // FrameTreeNode.
  // TODO(lazyboy): This does not belong to RenderFrameHostManager, move it to
  // somehwere else.
  void RemoveOuterDelegateFrame();

  // Returns the pending RenderFrameHost, or null if there is no pending one.
  RenderFrameHostImpl* pending_frame_host() const {
    return pending_render_frame_host_.get();
  }

  // Returns the speculative RenderFrameHost, or null if there is no speculative
  // one.
  RenderFrameHostImpl* speculative_frame_host() const {
    return speculative_render_frame_host_.get();
  }

  // TODO(creis): Remove this when we no longer use RVH for navigation.
  RenderViewHostImpl* pending_render_view_host() const;

  // Returns the WebUI associated with the ongoing navigation, it being either
  // the active or the pending one from the navigating RenderFrameHost. Returns
  // null if there's no ongoing navigation or if no WebUI applies.
  WebUIImpl* GetNavigatingWebUI() const;

  // Called when we want to instruct the renderer to navigate to the given
  // navigation entry. It may create a new RenderFrameHost or re-use an existing
  // one. The RenderFrameHost to navigate will be returned. Returns NULL if one
  // could not be created. |dest_url| takes precedence over the |frame_entry|'s
  // url (this is necessary because ReloadOriginalRequest navigates to a
  // different URL than the last committed entry, without modifying it).
  RenderFrameHostImpl* Navigate(const GURL& dest_url,
                                const FrameNavigationEntry& frame_entry,
                                const NavigationEntryImpl& entry,
                                bool is_reload);

  // Instructs the various live views to stop. Called when the user directed the
  // page to stop loading.
  void Stop();

  // Notifies the regular and pending RenderViewHosts that a load is or is not
  // happening. Even though the message is only for one of them, we don't know
  // which one so we tell both.
  void SetIsLoading(bool is_loading);

  // Whether to close the tab or not when there is a hang during an unload
  // handler. If we are mid-crosssite navigation, then we should proceed
  // with the navigation instead of closing the tab.
  bool ShouldCloseTabOnUnresponsiveRenderer();

  // Confirms whether we should close the page or navigate away.  This is called
  // before a cross-site request or before a tab/window is closed (as indicated
  // by the first parameter) to allow the appropriate renderer to approve or
  // deny the request.  |proceed| indicates whether the user chose to proceed.
  // |proceed_time| is the time when the request was allowed to proceed.
  void OnBeforeUnloadACK(bool for_cross_site_transition,
                         bool proceed,
                         const base::TimeTicks& proceed_time);

  // The |pending_render_frame_host| is ready to commit a page.  We should
  // ensure that the old RenderFrameHost runs its unload handler first and
  // determine whether a RenderFrameHost transfer is needed.
  // |cross_site_transferring_request| is NULL if a request is not being
  // transferred between renderers.
  void OnCrossSiteResponse(RenderFrameHostImpl* pending_render_frame_host,
                           const GlobalRequestID& global_request_id,
                           std::unique_ptr<CrossSiteTransferringRequest>
                               cross_site_transferring_request,
                           const std::vector<GURL>& transfer_url_chain,
                           const Referrer& referrer,
                           ui::PageTransition page_transition,
                           bool should_replace_current_entry);

  // Determines whether a navigation to |dest_url| may be completed using an
  // existing RenderFrameHost, or whether transferring to a new RenderFrameHost
  // backed by a different render process is required. This is a security policy
  // check determined by the current site isolation mode, and must be done
  // before the resource at |dest_url| is delivered to |existing_rfh|.
  //
  // |existing_rfh| must belong to this RFHM, but it can be a pending or current
  // host.
  //
  // When this function returns true for a subframe, an out-of-process iframe
  // must be created.
  bool IsRendererTransferNeededForNavigation(RenderFrameHostImpl* existing_rfh,
                                             const GURL& dest_url);

  // Called when a renderer's frame navigates.
  void DidNavigateFrame(RenderFrameHostImpl* render_frame_host,
                        bool was_caused_by_user_gesture);

  // Called when this frame's opener is changed to the frame specified by
  // |opener_routing_id| in |source_site_instance|'s process.  This change
  // could come from either the current RenderFrameHost or one of the
  // proxies (e.g., window.open that targets a RemoteFrame by name).  The
  // updated opener will be forwarded to any other RenderFrameProxies and
  // RenderFrames for this FrameTreeNode.
  void DidChangeOpener(int opener_routing_id,
                       SiteInstance* source_site_instance);

  // Creates and initializes a RenderFrameHost. If |view_routing_id_ptr|
  // is not nullptr it will be set to the routing id of the view associated with
  // the frame.
  std::unique_ptr<RenderFrameHostImpl> CreateRenderFrame(
      SiteInstance* instance,
      bool hidden,
      int* view_routing_id_ptr);

  // Helper method to create and initialize a RenderFrameProxyHost and return
  // its routing id.
  int CreateRenderFrameProxy(SiteInstance* instance);

  // Creates proxies for a new child frame at FrameTreeNode |child| in all
  // SiteInstances for which the current frame has proxies.  This method is
  // called on the parent of a new child frame before the child leaves the
  // SiteInstance.
  void CreateProxiesForChildFrame(FrameTreeNode* child);

  // Sets the passed passed interstitial as the currently showing interstitial.
  // |interstitial_page| should be non NULL (use the remove_interstitial_page
  // method to unset the interstitial) and no interstitial page should be set
  // when there is already a non NULL interstitial page set.
  void set_interstitial_page(InterstitialPageImpl* interstitial_page) {
    DCHECK(!interstitial_page_ && interstitial_page);
    interstitial_page_ = interstitial_page;
  }

  // Unsets the currently showing interstitial.
  void remove_interstitial_page() {
    DCHECK(interstitial_page_);
    interstitial_page_ = NULL;
  }

  // Returns the currently showing interstitial, NULL if no interstitial is
  // showing.
  InterstitialPageImpl* interstitial_page() const { return interstitial_page_; }

  // Returns the swapped out RenderViewHost for the given SiteInstance, if any.
  // This method is *deprecated* and GetRenderFrameProxyHost should be used.
  RenderViewHostImpl* GetSwappedOutRenderViewHost(SiteInstance* instance) const;

  // Returns the RenderFrameProxyHost for the given SiteInstance, if any.
  RenderFrameProxyHost* GetRenderFrameProxyHost(
      SiteInstance* instance) const;

  // If |render_frame_host| is on the pending deletion list, this deletes it.
  // Returns whether it was deleted.
  bool DeleteFromPendingList(RenderFrameHostImpl* render_frame_host);

  // Deletes any proxy hosts associated with this node. Used during destruction
  // of WebContentsImpl.
  void ResetProxyHosts();

  void ClearRFHsPendingShutdown();
  void ClearWebUIInstances();

  // Returns the routing id for a RenderFrameHost or RenderFrameProxyHost
  // that has the given SiteInstance and is associated with this
  // RenderFrameHostManager. Returns MSG_ROUTING_NONE if none is found.
  int GetRoutingIdForSiteInstance(SiteInstance* site_instance);

  // PlzNavigate
  // Notifies the RenderFrameHostManager that a new NavigationRequest has been
  // created and set in the FrameTreeNode so that it can speculatively create a
  // new RenderFrameHost (and potentially a new process) if needed.
  void DidCreateNavigationRequest(NavigationRequest* request);

  // PlzNavigate
  // Called (possibly several times) during a navigation to select or create an
  // appropriate RenderFrameHost for the provided URL. The returned pointer will
  // be for the current or the speculative RenderFrameHost and the instance is
  // owned by this manager.
  RenderFrameHostImpl* GetFrameHostForNavigation(
      const NavigationRequest& request);

  // PlzNavigate
  // Clean up any state for any ongoing navigation.
  void CleanUpNavigation();

  // PlzNavigate
  // Clears the speculative members, returning the RenderFrameHost to the caller
  // for disposal.
  std::unique_ptr<RenderFrameHostImpl> UnsetSpeculativeRenderFrameHost();

  // Notification methods to tell this RenderFrameHostManager that the frame it
  // is responsible for has started or stopped loading a document.
  void OnDidStartLoading();
  void OnDidStopLoading();

  // OnDidUpdateName gets called when a frame changes its name - it gets the new
  // |name| and the recalculated |unique_name| and replicates them into all
  // frame proxies.
  void OnDidUpdateName(const std::string& name, const std::string& unique_name);

  // Sends the newly added Content Security Policy header to all the proxies.
  void OnDidAddContentSecurityPolicy(const ContentSecurityPolicyHeader& header);

  // Resets Content Security Policy in all the proxies.
  void OnDidResetContentSecurityPolicy();

  // Sends updated enforcement of insecure request policy to all frame proxies
  // when the frame changes its setting.
  void OnEnforceInsecureRequestPolicy(blink::WebInsecureRequestPolicy policy);

  // Called on a frame to notify it that its out-of-process parent frame
  // changed a property (such as allowFullscreen) on its <iframe> element.
  // Sends updated WebFrameOwnerProperties to the RenderFrame and to all
  // proxies, skipping the parent process.
  void OnDidUpdateFrameOwnerProperties(
      const blink::WebFrameOwnerProperties& properties);

  // Send updated origin to all frame proxies when the frame navigates to a new
  // origin.
  void OnDidUpdateOrigin(const url::Origin& origin,
                         bool is_potentially_trustworthy_unique_origin);

  void EnsureRenderViewInitialized(RenderViewHostImpl* render_view_host,
                                   SiteInstance* instance);

  // Creates swapped out RenderViews and RenderFrameProxies for this frame's
  // FrameTree and for its opener chain in the given SiteInstance. This allows
  // other tabs to send cross-process JavaScript calls to their opener(s) and
  // to any other frames in the opener's FrameTree (e.g., supporting calls like
  // window.opener.opener.frames[x][y]).  Does not create proxies for the
  // subtree rooted at |skip_this_node| (e.g., if a node is being navigated, it
  // can be passed here to prevent proxies from being created for it, in
  // case it is in the same FrameTree as another node on its opener chain).
  void CreateOpenerProxies(SiteInstance* instance,
                           FrameTreeNode* skip_this_node);

  // Ensure that this frame has proxies in all SiteInstances that can discover
  // this frame by name (e.g., via window.open("", "frame_name")).  See
  // https://crbug.com/511474.
  void CreateProxiesForNewNamedFrame();

  // Returns a routing ID for the current FrameTreeNode's opener node in the
  // given SiteInstance.  May return a routing ID of either a RenderFrameHost
  // (if opener's current or pending RFH has SiteInstance |instance|) or a
  // RenderFrameProxyHost.  Returns MSG_ROUTING_NONE if there is no opener, or
  // if the opener node doesn't have a proxy for |instance|.
  int GetOpenerRoutingID(SiteInstance* instance);

  // Called on the RFHM of the inner WebContents to create a
  // RenderFrameProxyHost in its outer WebContents's SiteInstance,
  // |outer_contents_site_instance|. The frame in outer WebContents that is
  // hosting the inner WebContents is |render_frame_host|, and the frame will
  // be swapped out with the proxy.
  void CreateOuterDelegateProxy(SiteInstance* outer_contents_site_instance,
                                RenderFrameHostImpl* render_frame_host);

  // Sets the child RenderWidgetHostView for this frame, which must be part of
  // an inner WebContents.
  void SetRWHViewForInnerContents(RenderWidgetHostView* child_rwhv);

  // Returns the number of RenderFrameProxyHosts for this frame.
  int GetProxyCount();

  // Sends an IPC message to every process in the FrameTree. This should only be
  // called in the top-level RenderFrameHostManager.
  void SendPageMessage(IPC::Message* msg);

  // Returns a const reference to the map of proxy hosts. The keys are
  // SiteInstance IDs, the values are RenderFrameProxyHosts.
  const std::unordered_map<int32_t, std::unique_ptr<RenderFrameProxyHost>>&
  GetAllProxyHostsForTesting() const {
    return proxy_hosts_;
  }

  // SiteInstanceImpl::Observer
  void ActiveFrameCountIsZero(SiteInstanceImpl* site_instance) override;
  void RenderProcessGone(SiteInstanceImpl* site_instance) override;

  // Sets up the necessary state for a new RenderViewHost.  If |proxy| is not
  // null, it creates a RenderFrameProxy in the target renderer process which is
  // used to route IPC messages when in swapped out state.  Returns early if the
  // RenderViewHost has already been initialized for another RenderFrameHost.
  bool InitRenderView(RenderViewHostImpl* render_view_host,
    RenderFrameProxyHost* proxy);

 private:
  friend class NavigatorTestWithBrowserSideNavigation;
  friend class RenderFrameHostManagerTest;
  friend class TestWebContents;

  enum class SiteInstanceRelation {
    // A SiteInstance in a different browsing instance from the current.
    UNRELATED,
    // A SiteInstance in the same browsing instance as the current.
    RELATED,
    // The default subframe SiteInstance for the current browsing instance.
    RELATED_DEFAULT_SUBFRAME,
  };

  // Stores information regarding a SiteInstance targeted at a specific URL to
  // allow for comparisons without having to actually create new instances. It
  // can point to an existing one or store the details needed to create a new
  // one.
  struct CONTENT_EXPORT SiteInstanceDescriptor {
    explicit SiteInstanceDescriptor(content::SiteInstance* site_instance)
        : existing_site_instance(site_instance),
          relation(SiteInstanceRelation::UNRELATED) {}

    SiteInstanceDescriptor(BrowserContext* browser_context,
                           GURL dest_url,
                           SiteInstanceRelation relation_to_current);

    // Set with an existing SiteInstance to be reused.
    content::SiteInstance* existing_site_instance;

    // In case |existing_site_instance| is null, specify a new site URL.
    GURL new_site_url;

    // In case |existing_site_instance| is null, specify how the new site is
    // related to the current BrowsingInstance.
    SiteInstanceRelation relation;
  };

  // Create a RenderFrameProxyHost owned by this object.
  RenderFrameProxyHost* CreateRenderFrameProxyHost(SiteInstance* site_instance,
                                                   RenderViewHostImpl* rvh);
  // Delete a RenderFrameProxyHost owned by this object.
  void DeleteRenderFrameProxyHost(SiteInstance* site_instance);

  // Returns whether this tab should transition to a new renderer for
  // cross-site URLs.  Enabled unless we see the --process-per-tab command line
  // switch.  Can be overridden in unit tests.
  bool ShouldTransitionCrossSite();

  // Returns true if for the navigation from |current_effective_url| to
  // |new_effective_url|, a new SiteInstance and BrowsingInstance should be
  // created (even if we are in a process model that doesn't usually swap).
  // This forces a process swap and severs script connections with existing
  // tabs.  Cases where this can happen include transitions between WebUI and
  // regular web pages. |new_site_instance| may be null.
  // If there is no current NavigationEntry, then |current_is_view_source_mode|
  // should be the same as |new_is_view_source_mode|.
  //
  // We use the effective URL here, since that's what is used in the
  // SiteInstance's site and when we later call IsSameWebSite.  If there is no
  // current NavigationEntry, check the current SiteInstance's site, which might
  // already be committed to a Web UI URL (such as the NTP).
  bool ShouldSwapBrowsingInstancesForNavigation(
      const GURL& current_effective_url,
      bool current_is_view_source_mode,
      SiteInstance* new_site_instance,
      const GURL& new_effective_url,
      bool new_is_view_source_mode) const;

  // Returns the SiteInstance to use for the navigation.
  scoped_refptr<SiteInstance> GetSiteInstanceForNavigation(
      const GURL& dest_url,
      SiteInstance* source_instance,
      SiteInstance* dest_instance,
      SiteInstance* candidate_instance,
      ui::PageTransition transition,
      bool dest_is_restore,
      bool dest_is_view_source_mode);

  // Returns a descriptor of the appropriate SiteInstance object for the given
  // |dest_url|, possibly reusing the current, source or destination
  // SiteInstance. The actual SiteInstance can then be obtained calling
  // ConvertToSiteInstance with the descriptor.
  //
  // |source_instance| is the SiteInstance of the frame that initiated the
  // navigation. |current_instance| is the SiteInstance of the frame that is
  // currently navigating. |dest_instance| is a predetermined SiteInstance that
  // will be used if not null.
  // For example, if you have a parent frame A, which has a child frame B, and
  // A is trying to change the src attribute of B, this will cause a navigation
  // where the source SiteInstance is A and B is the current SiteInstance.
  //
  // This is a helper function for GetSiteInstanceForNavigation.
  SiteInstanceDescriptor DetermineSiteInstanceForURL(
      const GURL& dest_url,
      SiteInstance* source_instance,
      SiteInstance* current_instance,
      SiteInstance* dest_instance,
      ui::PageTransition transition,
      bool dest_is_restore,
      bool dest_is_view_source_mode,
      bool force_browsing_instance_swap);

  // Converts a SiteInstanceDescriptor to the actual SiteInstance it describes.
  // If a |candidate_instance| is provided (is not nullptr) and it matches the
  // description, it is returned as is.
  scoped_refptr<SiteInstance> ConvertToSiteInstance(
      const SiteInstanceDescriptor& descriptor,
      SiteInstance* candidate_instance);

  // Returns true if |candidate| is currently on the same web site as dest_url.
  bool IsCurrentlySameSite(RenderFrameHostImpl* candidate,
                           const GURL& dest_url);

  // Creates a new RenderFrameHostImpl for the |new_instance| and assign it to
  // |pending_render_frame_host_| while respecting the opener route if needed
  // and stores it in pending_render_frame_host_.
  void CreatePendingRenderFrameHost(SiteInstance* old_instance,
                                    SiteInstance* new_instance);

  // Ensure that we have created all needed proxies for a new RFH with
  // SiteInstance |new_instance|: (1) create swapped-out RVHs and proxies for
  // the new RFH's opener chain if we are staying in the same BrowsingInstance;
  // (2) Create proxies for the new RFH's SiteInstance in its own frame tree.
  void CreateProxiesForNewRenderFrameHost(SiteInstance* old_instance,
                                          SiteInstance* new_instance);

  // Traverse the opener chain and populate |opener_frame_trees| with
  // all FrameTrees accessible by following frame openers of nodes in the
  // given node's FrameTree. |opener_frame_trees| is ordered so that openers
  // of smaller-indexed entries point to larger-indexed entries (i.e., this
  // node's FrameTree is at index 0, its opener's FrameTree is at index 1,
  // etc). If the traversal encounters a node with an opener pointing to a
  // FrameTree that has already been traversed (such as when there's a cycle),
  // the node is added to |nodes_with_back_links|.
  void CollectOpenerFrameTrees(
      std::vector<FrameTree*>* opener_frame_trees,
      base::hash_set<FrameTreeNode*>* nodes_with_back_links);

  // Create swapped out RenderViews and RenderFrameProxies in the given
  // SiteInstance for the current node's FrameTree.  Used as a helper function
  // in CreateOpenerProxies for creating proxies in each FrameTree on the
  // opener chain.  Don't create proxies for the subtree rooted at
  // |skip_this_node|.
  void CreateOpenerProxiesForFrameTree(SiteInstance* instance,
                                       FrameTreeNode* skip_this_node);

  // Creates a RenderFrameHost and corresponding RenderViewHost if necessary.
  std::unique_ptr<RenderFrameHostImpl> CreateRenderFrameHost(
      SiteInstance* instance,
      int32_t view_routing_id,
      int32_t frame_routing_id,
      int32_t widget_routing_id,
      bool hidden);

  // PlzNavigate
  // Create and initialize a speculative RenderFrameHost for an ongoing
  // navigation. It might be destroyed and re-created later if the navigation
  // is redirected to a different SiteInstance.
  bool CreateSpeculativeRenderFrameHost(SiteInstance* old_instance,
                                        SiteInstance* new_instance);

  // Initialization for RenderFrameHost uses the same sequence as InitRenderView
  // above.
  bool InitRenderFrame(RenderFrameHostImpl* render_frame_host);

  // Makes the pending WebUI on the current RenderFrameHost active. Call this
  // when the current RenderFrameHost commits and it has a pending WebUI.
  void CommitPendingWebUI();

  // Sets the pending RenderFrameHost to be the active one. Call when the
  // pending RenderFrameHost commits.
  // If PlzNavigate is enabled the method will set the speculative (not pending)
  // RenderFrameHost to be the active one.
  void CommitPending();

  // Helper to call CommitPending() in all necessary cases.
  void CommitPendingIfNecessary(RenderFrameHostImpl* render_frame_host,
                                bool was_caused_by_user_gesture);

  // Commits any pending sandbox flag updates when the renderer's frame
  // navigates.
  void CommitPendingSandboxFlags();

  // Runs the unload handler in the old RenderFrameHost, after the new
  // RenderFrameHost has committed.  |old_render_frame_host| will either be
  // deleted or put on the pending delete list during this call.
  void SwapOutOldFrame(
      std::unique_ptr<RenderFrameHostImpl> old_render_frame_host);

  // Discards a RenderFrameHost that was never made active (for active ones
  // SwapOutOldFrame is used instead).
  void DiscardUnusedFrame(
      std::unique_ptr<RenderFrameHostImpl> render_frame_host);

  // Helper method to terminate the pending RenderFrameHost. The frame may be
  // deleted immediately, or it may be kept around in hopes of later reuse.
  void CancelPending();

  // Clears pending_render_frame_host_, returning it to the caller for disposal.
  std::unique_ptr<RenderFrameHostImpl> UnsetPendingRenderFrameHost();

  // Helper method to set the active RenderFrameHost. Returns the old
  // RenderFrameHost and updates counts.
  std::unique_ptr<RenderFrameHostImpl> SetRenderFrameHost(
      std::unique_ptr<RenderFrameHostImpl> render_frame_host);

  RenderFrameHostImpl* UpdateStateForNavigate(
      const GURL& dest_url,
      SiteInstance* source_instance,
      SiteInstance* dest_instance,
      ui::PageTransition transition,
      bool dest_is_restore,
      bool dest_is_view_source_mode,
      const GlobalRequestID& transferred_request_id,
      int bindings,
      bool is_reload);

  // Updates the pending WebUI of the current RenderFrameHost for a same-site
  // navigation.
  void UpdatePendingWebUIOnCurrentFrameHost(const GURL& dest_url,
                                            int entry_bindings);

  // Returns true if a subframe can navigate cross-process.
  bool CanSubframeSwapProcess(const GURL& dest_url,
                              SiteInstance* source_instance,
                              SiteInstance* dest_instance);

  // For use in creating RenderFrameHosts.
  FrameTreeNode* frame_tree_node_;

  // Our delegate, not owned by us. Guaranteed non-NULL.
  Delegate* delegate_;

  // Implemented by the owner of this class.  These delegates are installed into
  // all the RenderFrameHosts that we create.
  RenderFrameHostDelegate* render_frame_delegate_;
  RenderWidgetHostDelegate* render_widget_delegate_;

  // Our RenderFrameHost which is responsible for all communication with a child
  // RenderFrame instance.
  // For now, RenderFrameHost keeps a RenderViewHost in its SiteInstance alive.
  // Eventually, RenderViewHost will be replaced with a page context.
  std::unique_ptr<RenderFrameHostImpl> render_frame_host_;

  // A RenderFrameHost used to load a cross-site page. This remains hidden
  // while a cross-site request is pending until it calls DidNavigate.
  // Note: This member is not used in PlzNavigate.
  std::unique_ptr<RenderFrameHostImpl> pending_render_frame_host_;

  // If a pending request needs to be transferred to another process, this
  // owns the request until it's transferred to the new process, so it will be
  // cleaned up if the navigation is cancelled.  Otherwise, this is NULL.
  std::unique_ptr<CrossSiteTransferringRequest>
      cross_site_transferring_request_;

  // This is used to temporarily store the NavigationHandle during
  // transferring navigations. The handle needs to be stored because the old
  // RenderFrameHost may be discarded before a new RenderFrameHost is created
  // for the navigation.
  // PlzNavigate: this will never be set since there are no transferring
  // navigations in PlzNavigate.
  std::unique_ptr<NavigationHandleImpl> transfer_navigation_handle_;

  // Proxy hosts, indexed by site instance ID.
  std::unordered_map<int32_t, std::unique_ptr<RenderFrameProxyHost>>
      proxy_hosts_;

  // A list of RenderFrameHosts waiting to shut down after swapping out.
  using RFHPendingDeleteList = std::list<std::unique_ptr<RenderFrameHostImpl>>;
  RFHPendingDeleteList pending_delete_hosts_;

  // The intersitial page currently shown if any, not own by this class
  // (the InterstitialPage is self-owned, it deletes itself when hidden).
  InterstitialPageImpl* interstitial_page_;

  // PlzNavigate
  // Stores a speculative RenderFrameHost which is created early in a navigation
  // so a renderer process can be started in parallel, if needed.
  // This is purely a performance optimization and is not required for correct
  // behavior. The speculative RenderFrameHost might be discarded later on if
  // the final URL's SiteInstance isn't compatible with the one used to create
  // it.
  // Note: PlzNavigate only uses the speculative RenderFrameHost, not the
  // pending one.
  std::unique_ptr<RenderFrameHostImpl> speculative_render_frame_host_;

  base::WeakPtrFactory<RenderFrameHostManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameHostManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_MANAGER_H_
