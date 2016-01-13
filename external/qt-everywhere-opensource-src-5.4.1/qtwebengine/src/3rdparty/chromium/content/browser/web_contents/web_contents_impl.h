// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_IMPL_H_
#define CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_IMPL_H_

#include <map>
#include <set>
#include <string>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/process/process.h"
#include "base/values.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/navigation_controller_delegate.h"
#include "content/browser/frame_host/navigation_controller_impl.h"
#include "content/browser/frame_host/navigator_delegate.h"
#include "content/browser/frame_host/render_frame_host_delegate.h"
#include "content/browser/frame_host/render_frame_host_manager.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/common/content_export.h"
#include "content/public/browser/color_chooser.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/common/renderer_preferences.h"
#include "content/public/common/three_d_api_types.h"
#include "net/base/load_states.h"
#include "third_party/WebKit/public/web/WebDragOperation.h"
#include "ui/gfx/rect_f.h"
#include "ui/gfx/size.h"
#include "webkit/common/resource_type.h"

struct BrowserPluginHostMsg_ResizeGuest_Params;
struct ViewHostMsg_DateTimeDialogValue_Params;
struct ViewMsg_PostMessage_Params;

namespace content {
class BrowserPluginEmbedder;
class BrowserPluginGuest;
class BrowserPluginGuestManager;
class DateTimeChooserAndroid;
class DownloadItem;
class GeolocationDispatcherHost;
class InterstitialPageImpl;
class JavaScriptDialogManager;
class MidiDispatcherHost;
class PowerSaveBlocker;
class RenderViewHost;
class RenderViewHostDelegateView;
class RenderViewHostImpl;
class RenderWidgetHostImpl;
class SavePackage;
class ScreenOrientationDispatcherHost;
class SiteInstance;
class TestWebContents;
class WebContentsDelegate;
class WebContentsImpl;
class WebContentsObserver;
class WebContentsView;
class WebContentsViewDelegate;
struct AXEventNotificationDetails;
struct ColorSuggestion;
struct FaviconURL;
struct LoadNotificationDetails;
struct ResourceRedirectDetails;
struct ResourceRequestDetails;

// Factory function for the implementations that content knows about. Takes
// ownership of |delegate|.
WebContentsView* CreateWebContentsView(
    WebContentsImpl* web_contents,
    WebContentsViewDelegate* delegate,
    RenderViewHostDelegateView** render_view_host_delegate_view);

class CONTENT_EXPORT WebContentsImpl
    : public NON_EXPORTED_BASE(WebContents),
      public NON_EXPORTED_BASE(RenderFrameHostDelegate),
      public RenderViewHostDelegate,
      public RenderWidgetHostDelegate,
      public RenderFrameHostManager::Delegate,
      public NotificationObserver,
      public NON_EXPORTED_BASE(NavigationControllerDelegate),
      public NON_EXPORTED_BASE(NavigatorDelegate) {
 public:
  virtual ~WebContentsImpl();

  static WebContentsImpl* CreateWithOpener(
      const WebContents::CreateParams& params,
      WebContentsImpl* opener);

  // Returns the opener WebContentsImpl, if any. This can be set to null if the
  // opener is closed or the page clears its window.opener.
  WebContentsImpl* opener() const { return opener_; }

  // Creates a swapped out RenderView. This is used by the browser plugin to
  // create a swapped out RenderView in the embedder render process for the
  // guest, to expose the guest's window object to the embedder.
  // This returns the routing ID of the newly created swapped out RenderView.
  int CreateSwappedOutRenderView(SiteInstance* instance);

  // Complex initialization here. Specifically needed to avoid having
  // members call back into our virtual functions in the constructor.
  virtual void Init(const WebContents::CreateParams& params);

  // Returns the SavePackage which manages the page saving job. May be NULL.
  SavePackage* save_package() const { return save_package_.get(); }

#if defined(OS_ANDROID)
  // In Android WebView, the RenderView needs created even there is no
  // navigation entry, this allows Android WebViews to use
  // javascript: URLs that load into the DOMWindow before the first page
  // load. This is not safe to do in any context that a web page could get a
  // reference to the DOMWindow before the first page load.
  bool CreateRenderViewForInitialEmptyDocument();
#endif

  // Expose the render manager for testing.
  // TODO(creis): Remove this now that we can get to it via FrameTreeNode.
  RenderFrameHostManager* GetRenderManagerForTesting();

  // Returns guest browser plugin object, or NULL if this WebContents is not a
  // guest.
  BrowserPluginGuest* GetBrowserPluginGuest() const;

  // Sets a BrowserPluginGuest object for this WebContents. If this WebContents
  // has a BrowserPluginGuest then that implies that it is being hosted by
  // a BrowserPlugin object in an embedder renderer process.
  void SetBrowserPluginGuest(BrowserPluginGuest* guest);

  // Returns embedder browser plugin object, or NULL if this WebContents is not
  // an embedder.
  BrowserPluginEmbedder* GetBrowserPluginEmbedder() const;

  // Gets the current fullscreen render widget's routing ID. Returns
  // MSG_ROUTING_NONE when there is no fullscreen render widget.
  int GetFullscreenWidgetRoutingID() const;

  // Invoked when visible SSL state (as defined by SSLStatus) changes.
  void DidChangeVisibleSSLState();

  // Informs the render view host and the BrowserPluginEmbedder, if present, of
  // a Drag Source End.
  void DragSourceEndedAt(int client_x, int client_y, int screen_x,
      int screen_y, blink::WebDragOperation operation);

  // A response has been received for a resource request.
  void DidGetResourceResponseStart(
      const ResourceRequestDetails& details);

  // A redirect was received while requesting a resource.
  void DidGetRedirectForResourceRequest(
      RenderViewHost* render_view_host,
      const ResourceRedirectDetails& details);

  WebContentsView* GetView() const;

  GeolocationDispatcherHost* geolocation_dispatcher_host() {
    return geolocation_dispatcher_host_.get();
  }

  bool should_normally_be_visible() { return should_normally_be_visible_; }

  // WebContents ------------------------------------------------------
  virtual WebContentsDelegate* GetDelegate() OVERRIDE;
  virtual void SetDelegate(WebContentsDelegate* delegate) OVERRIDE;
  virtual NavigationControllerImpl& GetController() OVERRIDE;
  virtual const NavigationControllerImpl& GetController() const OVERRIDE;
  virtual BrowserContext* GetBrowserContext() const OVERRIDE;
  virtual const GURL& GetURL() const OVERRIDE;
  virtual const GURL& GetVisibleURL() const OVERRIDE;
  virtual const GURL& GetLastCommittedURL() const OVERRIDE;
  virtual RenderProcessHost* GetRenderProcessHost() const OVERRIDE;
  virtual RenderFrameHost* GetMainFrame() OVERRIDE;
  virtual RenderFrameHost* GetFocusedFrame() OVERRIDE;
  virtual void ForEachFrame(
      const base::Callback<void(RenderFrameHost*)>& on_frame) OVERRIDE;
  virtual void SendToAllFrames(IPC::Message* message) OVERRIDE;
  virtual RenderViewHost* GetRenderViewHost() const OVERRIDE;
  virtual int GetRoutingID() const OVERRIDE;
  virtual RenderWidgetHostView* GetRenderWidgetHostView() const OVERRIDE;
  virtual RenderWidgetHostView* GetFullscreenRenderWidgetHostView() const
      OVERRIDE;
  virtual WebUI* CreateWebUI(const GURL& url) OVERRIDE;
  virtual WebUI* GetWebUI() const OVERRIDE;
  virtual WebUI* GetCommittedWebUI() const OVERRIDE;
  virtual void SetUserAgentOverride(const std::string& override) OVERRIDE;
  virtual const std::string& GetUserAgentOverride() const OVERRIDE;
#if defined(OS_WIN)
  virtual void SetParentNativeViewAccessible(
      gfx::NativeViewAccessible accessible_parent) OVERRIDE;
#endif
  virtual const base::string16& GetTitle() const OVERRIDE;
  virtual int32 GetMaxPageID() OVERRIDE;
  virtual int32 GetMaxPageIDForSiteInstance(
      SiteInstance* site_instance) OVERRIDE;
  virtual SiteInstance* GetSiteInstance() const OVERRIDE;
  virtual SiteInstance* GetPendingSiteInstance() const OVERRIDE;
  virtual bool IsLoading() const OVERRIDE;
  virtual bool IsLoadingToDifferentDocument() const OVERRIDE;
  virtual bool IsWaitingForResponse() const OVERRIDE;
  virtual const net::LoadStateWithParam& GetLoadState() const OVERRIDE;
  virtual const base::string16& GetLoadStateHost() const OVERRIDE;
  virtual uint64 GetUploadSize() const OVERRIDE;
  virtual uint64 GetUploadPosition() const OVERRIDE;
  virtual std::set<GURL> GetSitesInTab() const OVERRIDE;
  virtual const std::string& GetEncoding() const OVERRIDE;
  virtual bool DisplayedInsecureContent() const OVERRIDE;
  virtual void IncrementCapturerCount(const gfx::Size& capture_size) OVERRIDE;
  virtual void DecrementCapturerCount() OVERRIDE;
  virtual int GetCapturerCount() const OVERRIDE;
  virtual bool IsCrashed() const OVERRIDE;
  virtual void SetIsCrashed(base::TerminationStatus status,
                            int error_code) OVERRIDE;
  virtual base::TerminationStatus GetCrashedStatus() const OVERRIDE;
  virtual bool IsBeingDestroyed() const OVERRIDE;
  virtual void NotifyNavigationStateChanged(unsigned changed_flags) OVERRIDE;
  virtual base::TimeTicks GetLastActiveTime() const OVERRIDE;
  virtual void WasShown() OVERRIDE;
  virtual void WasHidden() OVERRIDE;
  virtual bool NeedToFireBeforeUnload() OVERRIDE;
  virtual void DispatchBeforeUnload(bool for_cross_site_transition) OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual WebContents* Clone() OVERRIDE;
  virtual void ReloadFocusedFrame(bool ignore_cache) OVERRIDE;
  virtual void Undo() OVERRIDE;
  virtual void Redo() OVERRIDE;
  virtual void Cut() OVERRIDE;
  virtual void Copy() OVERRIDE;
  virtual void CopyToFindPboard() OVERRIDE;
  virtual void Paste() OVERRIDE;
  virtual void PasteAndMatchStyle() OVERRIDE;
  virtual void Delete() OVERRIDE;
  virtual void SelectAll() OVERRIDE;
  virtual void Unselect() OVERRIDE;
  virtual void Replace(const base::string16& word) OVERRIDE;
  virtual void ReplaceMisspelling(const base::string16& word) OVERRIDE;
  virtual void NotifyContextMenuClosed(
      const CustomContextMenuContext& context) OVERRIDE;
  virtual void ExecuteCustomContextMenuCommand(
      int action, const CustomContextMenuContext& context) OVERRIDE;
  virtual gfx::NativeView GetNativeView() OVERRIDE;
  virtual gfx::NativeView GetContentNativeView() OVERRIDE;
  virtual gfx::NativeWindow GetTopLevelNativeWindow() OVERRIDE;
  virtual gfx::Rect GetContainerBounds() OVERRIDE;
  virtual gfx::Rect GetViewBounds() OVERRIDE;
  virtual DropData* GetDropData() OVERRIDE;
  virtual void Focus() OVERRIDE;
  virtual void SetInitialFocus() OVERRIDE;
  virtual void StoreFocus() OVERRIDE;
  virtual void RestoreFocus() OVERRIDE;
  virtual void FocusThroughTabTraversal(bool reverse) OVERRIDE;
  virtual bool ShowingInterstitialPage() const OVERRIDE;
  virtual InterstitialPage* GetInterstitialPage() const OVERRIDE;
  virtual bool IsSavable() OVERRIDE;
  virtual void OnSavePage() OVERRIDE;
  virtual bool SavePage(const base::FilePath& main_file,
                        const base::FilePath& dir_path,
                        SavePageType save_type) OVERRIDE;
  virtual void SaveFrame(const GURL& url,
                         const Referrer& referrer) OVERRIDE;
  virtual void GenerateMHTML(
      const base::FilePath& file,
      const base::Callback<void(int64)>& callback)
          OVERRIDE;
  virtual const std::string& GetContentsMimeType() const OVERRIDE;
  virtual bool WillNotifyDisconnection() const OVERRIDE;
  virtual void SetOverrideEncoding(const std::string& encoding) OVERRIDE;
  virtual void ResetOverrideEncoding() OVERRIDE;
  virtual RendererPreferences* GetMutableRendererPrefs() OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual void SystemDragEnded() OVERRIDE;
  virtual void UserGestureDone() OVERRIDE;
  virtual void SetClosedByUserGesture(bool value) OVERRIDE;
  virtual bool GetClosedByUserGesture() const OVERRIDE;
  virtual int GetZoomPercent(bool* enable_increment,
                             bool* enable_decrement) const OVERRIDE;
  virtual void ViewSource() OVERRIDE;
  virtual void ViewFrameSource(const GURL& url,
                               const PageState& page_state) OVERRIDE;
  virtual int GetMinimumZoomPercent() const OVERRIDE;
  virtual int GetMaximumZoomPercent() const OVERRIDE;
  virtual gfx::Size GetPreferredSize() const OVERRIDE;
  virtual bool GotResponseToLockMouseRequest(bool allowed) OVERRIDE;
  virtual bool HasOpener() const OVERRIDE;
  virtual void DidChooseColorInColorChooser(SkColor color) OVERRIDE;
  virtual void DidEndColorChooser() OVERRIDE;
  virtual int DownloadImage(const GURL& url,
                            bool is_favicon,
                            uint32_t max_bitmap_size,
                            const ImageDownloadCallback& callback) OVERRIDE;
  virtual bool IsSubframe() const OVERRIDE;
  virtual void Find(int request_id,
                    const base::string16& search_text,
                    const blink::WebFindOptions& options) OVERRIDE;
  virtual void StopFinding(StopFindAction action) OVERRIDE;
  virtual void InsertCSS(const std::string& css) OVERRIDE;
#if defined(OS_ANDROID)
  virtual base::android::ScopedJavaLocalRef<jobject> GetJavaWebContents()
      OVERRIDE;
#elif defined(OS_MACOSX)
  virtual void SetAllowOverlappingViews(bool overlapping) OVERRIDE;
  virtual bool GetAllowOverlappingViews() OVERRIDE;
  virtual void SetOverlayView(WebContents* overlay,
                              const gfx::Point& offset) OVERRIDE;
  virtual void RemoveOverlayView() OVERRIDE;
#endif

  // Implementation of PageNavigator.
  virtual WebContents* OpenURL(const OpenURLParams& params) OVERRIDE;

  // Implementation of IPC::Sender.
  virtual bool Send(IPC::Message* message) OVERRIDE;

  // RenderFrameHostDelegate ---------------------------------------------------
  virtual bool OnMessageReceived(RenderFrameHost* render_frame_host,
                                 const IPC::Message& message) OVERRIDE;
  virtual const GURL& GetMainFrameLastCommittedURL() const OVERRIDE;
  virtual void RenderFrameCreated(RenderFrameHost* render_frame_host) OVERRIDE;
  virtual void RenderFrameDeleted(RenderFrameHost* render_frame_host) OVERRIDE;
  virtual void DidStartLoading(RenderFrameHost* render_frame_host,
                               bool to_different_document) OVERRIDE;
  virtual void SwappedOut(RenderFrameHost* render_frame_host) OVERRIDE;
  virtual void WorkerCrashed(RenderFrameHost* render_frame_host) OVERRIDE;
  virtual void ShowContextMenu(RenderFrameHost* render_frame_host,
                               const ContextMenuParams& params) OVERRIDE;
  virtual void RunJavaScriptMessage(RenderFrameHost* render_frame_host,
                                    const base::string16& message,
                                    const base::string16& default_prompt,
                                    const GURL& frame_url,
                                    JavaScriptMessageType type,
                                    IPC::Message* reply_msg) OVERRIDE;
  virtual void RunBeforeUnloadConfirm(RenderFrameHost* render_frame_host,
                                      const base::string16& message,
                                      bool is_reload,
                                      IPC::Message* reply_msg) OVERRIDE;
  virtual void DidAccessInitialDocument() OVERRIDE;
  virtual void DidDisownOpener(RenderFrameHost* render_frame_host) OVERRIDE;
  virtual void DocumentOnLoadCompleted(
      RenderFrameHost* render_frame_host) OVERRIDE;
  virtual void UpdateTitle(RenderFrameHost* render_frame_host,
                           int32 page_id,
                           const base::string16& title,
                           base::i18n::TextDirection title_direction) OVERRIDE;
  virtual void UpdateEncoding(RenderFrameHost* render_frame_host,
                              const std::string& encoding) OVERRIDE;
  virtual WebContents* GetAsWebContents() OVERRIDE;
  virtual bool IsNeverVisible() OVERRIDE;

  // RenderViewHostDelegate ----------------------------------------------------
  virtual RenderViewHostDelegateView* GetDelegateView() OVERRIDE;
  virtual bool OnMessageReceived(RenderViewHost* render_view_host,
                                 const IPC::Message& message) OVERRIDE;
  // RenderFrameHostDelegate has the same method, so list it there because this
  // interface is going away.
  // virtual WebContents* GetAsWebContents() OVERRIDE;
  virtual gfx::Rect GetRootWindowResizerRect() const OVERRIDE;
  virtual void RenderViewCreated(RenderViewHost* render_view_host) OVERRIDE;
  virtual void RenderViewReady(RenderViewHost* render_view_host) OVERRIDE;
  virtual void RenderViewTerminated(RenderViewHost* render_view_host,
                                    base::TerminationStatus status,
                                    int error_code) OVERRIDE;
  virtual void RenderViewDeleted(RenderViewHost* render_view_host) OVERRIDE;
  virtual void UpdateState(RenderViewHost* render_view_host,
                           int32 page_id,
                           const PageState& page_state) OVERRIDE;
  virtual void UpdateTargetURL(int32 page_id, const GURL& url) OVERRIDE;
  virtual void Close(RenderViewHost* render_view_host) OVERRIDE;
  virtual void RequestMove(const gfx::Rect& new_bounds) OVERRIDE;
  virtual void DidCancelLoading() OVERRIDE;
  virtual void DocumentAvailableInMainFrame(
      RenderViewHost* render_view_host) OVERRIDE;
  virtual void RouteCloseEvent(RenderViewHost* rvh) OVERRIDE;
  virtual void RouteMessageEvent(
      RenderViewHost* rvh,
      const ViewMsg_PostMessage_Params& params) OVERRIDE;
  virtual bool AddMessageToConsole(int32 level,
                                   const base::string16& message,
                                   int32 line_no,
                                   const base::string16& source_id) OVERRIDE;
  virtual RendererPreferences GetRendererPrefs(
      BrowserContext* browser_context) const OVERRIDE;
  virtual WebPreferences GetWebkitPrefs() OVERRIDE;
  virtual void OnUserGesture() OVERRIDE;
  virtual void OnIgnoredUIEvent() OVERRIDE;
  virtual void RendererUnresponsive(RenderViewHost* render_view_host,
                                    bool is_during_beforeunload,
                                    bool is_during_unload) OVERRIDE;
  virtual void RendererResponsive(RenderViewHost* render_view_host) OVERRIDE;
  virtual void LoadStateChanged(const GURL& url,
                                const net::LoadStateWithParam& load_state,
                                uint64 upload_position,
                                uint64 upload_size) OVERRIDE;
  virtual void Activate() OVERRIDE;
  virtual void Deactivate() OVERRIDE;
  virtual void LostCapture() OVERRIDE;
  virtual void HandleMouseDown() OVERRIDE;
  virtual void HandleMouseUp() OVERRIDE;
  virtual void HandlePointerActivate() OVERRIDE;
  virtual void HandleGestureBegin() OVERRIDE;
  virtual void HandleGestureEnd() OVERRIDE;
  virtual void RunFileChooser(
      RenderViewHost* render_view_host,
      const FileChooserParams& params) OVERRIDE;
  virtual void ToggleFullscreenMode(bool enter_fullscreen) OVERRIDE;
  virtual bool IsFullscreenForCurrentTab() const OVERRIDE;
  virtual void UpdatePreferredSize(const gfx::Size& pref_size) OVERRIDE;
  virtual void ResizeDueToAutoResize(const gfx::Size& new_size) OVERRIDE;
  virtual void RequestToLockMouse(bool user_gesture,
                                  bool last_unlocked_by_target) OVERRIDE;
  virtual void LostMouseLock() OVERRIDE;
  virtual void CreateNewWindow(
      int render_process_id,
      int route_id,
      int main_frame_route_id,
      const ViewHostMsg_CreateWindow_Params& params,
      SessionStorageNamespace* session_storage_namespace) OVERRIDE;
  virtual void CreateNewWidget(int render_process_id,
                               int route_id,
                               blink::WebPopupType popup_type) OVERRIDE;
  virtual void CreateNewFullscreenWidget(int render_process_id,
                                         int route_id) OVERRIDE;
  virtual void ShowCreatedWindow(int route_id,
                                 WindowOpenDisposition disposition,
                                 const gfx::Rect& initial_pos,
                                 bool user_gesture) OVERRIDE;
  virtual void ShowCreatedWidget(int route_id,
                                 const gfx::Rect& initial_pos) OVERRIDE;
  virtual void ShowCreatedFullscreenWidget(int route_id) OVERRIDE;
  virtual void RequestMediaAccessPermission(
      const MediaStreamRequest& request,
      const MediaResponseCallback& callback) OVERRIDE;
  virtual SessionStorageNamespace* GetSessionStorageNamespace(
      SiteInstance* instance) OVERRIDE;
  virtual SessionStorageNamespaceMap GetSessionStorageNamespaceMap() OVERRIDE;
  virtual FrameTree* GetFrameTree() OVERRIDE;
  virtual void AccessibilityEventReceived(
      const std::vector<AXEventNotificationDetails>& details) OVERRIDE;

  // NavigatorDelegate ---------------------------------------------------------

  virtual void DidStartProvisionalLoad(
      RenderFrameHostImpl* render_frame_host,
      int parent_routing_id,
      const GURL& validated_url,
      bool is_error_page,
      bool is_iframe_srcdoc) OVERRIDE;
  virtual void DidFailProvisionalLoadWithError(
      RenderFrameHostImpl* render_frame_host,
      const FrameHostMsg_DidFailProvisionalLoadWithError_Params& params)
      OVERRIDE;
  virtual void DidFailLoadWithError(
      RenderFrameHostImpl* render_frame_host,
      const GURL& url,
      int error_code,
      const base::string16& error_description) OVERRIDE;
  virtual void DidRedirectProvisionalLoad(
      RenderFrameHostImpl* render_frame_host,
      const GURL& validated_target_url) OVERRIDE;
  virtual void DidCommitProvisionalLoad(
      RenderFrameHostImpl* render_frame_host,
      const base::string16& frame_unique_name,
      bool is_main_frame,
      const GURL& url,
      PageTransition transition_type) OVERRIDE;
  virtual void DidNavigateMainFramePreCommit(
      const FrameHostMsg_DidCommitProvisionalLoad_Params& params) OVERRIDE;
  virtual void DidNavigateMainFramePostCommit(
      const LoadCommittedDetails& details,
      const FrameHostMsg_DidCommitProvisionalLoad_Params& params) OVERRIDE;
  virtual void DidNavigateAnyFramePostCommit(
      RenderFrameHostImpl* render_frame_host,
      const LoadCommittedDetails& details,
      const FrameHostMsg_DidCommitProvisionalLoad_Params& params) OVERRIDE;
  virtual void SetMainFrameMimeType(const std::string& mime_type) OVERRIDE;
  virtual bool CanOverscrollContent() const OVERRIDE;
  virtual void NotifyChangedNavigationState(
      InvalidateTypes changed_flags) OVERRIDE;
  virtual void AboutToNavigateRenderFrame(
      RenderFrameHostImpl* render_frame_host) OVERRIDE;
  virtual void DidStartNavigationToPendingEntry(
      RenderFrameHostImpl* render_frame_host,
      const GURL& url,
      NavigationController::ReloadType reload_type) OVERRIDE;
  virtual void RequestOpenURL(RenderFrameHostImpl* render_frame_host,
                              const OpenURLParams& params) OVERRIDE;
  virtual bool ShouldPreserveAbortedURLs() OVERRIDE;

  // RenderWidgetHostDelegate --------------------------------------------------

  virtual void RenderWidgetDeleted(
      RenderWidgetHostImpl* render_widget_host) OVERRIDE;
  virtual bool PreHandleKeyboardEvent(
      const NativeWebKeyboardEvent& event,
      bool* is_keyboard_shortcut) OVERRIDE;
  virtual void HandleKeyboardEvent(
      const NativeWebKeyboardEvent& event) OVERRIDE;
  virtual bool HandleWheelEvent(
      const blink::WebMouseWheelEvent& event) OVERRIDE;
  virtual bool PreHandleGestureEvent(
      const blink::WebGestureEvent& event) OVERRIDE;
  virtual bool HandleGestureEvent(
      const blink::WebGestureEvent& event) OVERRIDE;
  virtual void DidSendScreenRects(RenderWidgetHostImpl* rwh) OVERRIDE;
  virtual void OnTouchEmulationEnabled(bool enabled) OVERRIDE;
#if defined(OS_WIN)
  virtual gfx::NativeViewAccessible GetParentNativeViewAccessible() OVERRIDE;
#endif

  // RenderFrameHostManager::Delegate ------------------------------------------

  virtual bool CreateRenderViewForRenderManager(
      RenderViewHost* render_view_host,
      int opener_route_id,
      int proxy_routing_id,
      bool for_main_frame) OVERRIDE;
  virtual void BeforeUnloadFiredFromRenderManager(
      bool proceed, const base::TimeTicks& proceed_time,
      bool* proceed_to_fire_unload) OVERRIDE;
  virtual void RenderProcessGoneFromRenderManager(
      RenderViewHost* render_view_host) OVERRIDE;
  virtual void UpdateRenderViewSizeForRenderManager() OVERRIDE;
  virtual void CancelModalDialogsForRenderManager() OVERRIDE;
  virtual void NotifySwappedFromRenderManager(
      RenderViewHost* old_host, RenderViewHost* new_host) OVERRIDE;
  virtual int CreateOpenerRenderViewsForRenderManager(
      SiteInstance* instance) OVERRIDE;
  virtual NavigationControllerImpl&
      GetControllerForRenderManager() OVERRIDE;
  virtual WebUIImpl* CreateWebUIForRenderManager(const GURL& url) OVERRIDE;
  virtual NavigationEntry*
      GetLastCommittedNavigationEntryForRenderManager() OVERRIDE;
  virtual bool FocusLocationBarByDefault() OVERRIDE;
  virtual void SetFocusToLocationBar(bool select_all) OVERRIDE;
  virtual void CreateViewAndSetSizeForRVH(RenderViewHost* rvh) OVERRIDE;
  virtual bool IsHidden() OVERRIDE;

  // NotificationObserver ------------------------------------------------------

  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  // NavigationControllerDelegate ----------------------------------------------

  virtual WebContents* GetWebContents() OVERRIDE;
  virtual void NotifyNavigationEntryCommitted(
      const LoadCommittedDetails& load_details) OVERRIDE;

  // Invoked before a form repost warning is shown.
  virtual void NotifyBeforeFormRepostWarningShow() OVERRIDE;

  // Activate this WebContents and show a form repost warning.
  virtual void ActivateAndShowRepostFormWarningDialog() OVERRIDE;

  // Whether the initial empty page of this view has been accessed by another
  // page, making it unsafe to show the pending URL. Always false after the
  // first commit.
  virtual bool HasAccessedInitialDocument() OVERRIDE;

  // Updates the max page ID for the current SiteInstance in this
  // WebContentsImpl to be at least |page_id|.
  virtual void UpdateMaxPageID(int32 page_id) OVERRIDE;

  // Updates the max page ID for the given SiteInstance in this WebContentsImpl
  // to be at least |page_id|.
  virtual void UpdateMaxPageIDForSiteInstance(SiteInstance* site_instance,
                                              int32 page_id) OVERRIDE;

  // Copy the current map of SiteInstance ID to max page ID from another tab.
  // This is necessary when this tab adopts the NavigationEntries from
  // |web_contents|.
  virtual void CopyMaxPageIDsFrom(WebContents* web_contents) OVERRIDE;

  // Called by the NavigationController to cause the WebContentsImpl to navigate
  // to the current pending entry. The NavigationController should be called
  // back with RendererDidNavigate on success or DiscardPendingEntry on failure.
  // The callbacks can be inside of this function, or at some future time.
  //
  // The entry has a PageID of -1 if newly created (corresponding to navigation
  // to a new URL).
  //
  // If this method returns false, then the navigation is discarded (equivalent
  // to calling DiscardPendingEntry on the NavigationController).
  virtual bool NavigateToPendingEntry(
      NavigationController::ReloadType reload_type) OVERRIDE;

  // Sets the history for this WebContentsImpl to |history_length| entries, and
  // moves the current page_id to the last entry in the list if it's valid.
  // This is mainly used when a prerendered page is swapped into the current
  // tab. The method is virtual for testing.
  virtual void SetHistoryLengthAndPrune(
      const SiteInstance* site_instance,
      int merge_history_length,
      int32 minimum_page_id) OVERRIDE;

  // Called by InterstitialPageImpl when it creates a RenderFrameHost.
  virtual void RenderFrameForInterstitialPageCreated(
      RenderFrameHost* render_frame_host) OVERRIDE;

  // Sets the passed interstitial as the currently showing interstitial.
  // No interstitial page should already be attached.
  virtual void AttachInterstitialPage(
      InterstitialPageImpl* interstitial_page) OVERRIDE;

  // Unsets the currently showing interstitial.
  virtual void DetachInterstitialPage() OVERRIDE;

  // Changes the IsLoading state and notifies the delegate as needed.
  // |details| is used to provide details on the load that just finished
  // (but can be null if not applicable).
  virtual void SetIsLoading(RenderViewHost* render_view_host,
                            bool is_loading,
                            bool to_different_document,
                            LoadNotificationDetails* details) OVERRIDE;

  typedef base::Callback<void(WebContents*)> CreatedCallback;

  // Requests the renderer to select the region between two points in the
  // currently focused frame.
  void SelectRange(const gfx::Point& start, const gfx::Point& end);

 private:
  friend class TestNavigationObserver;
  friend class WebContentsAddedObserver;
  friend class WebContentsObserver;
  friend class WebContents;  // To implement factory methods.

  FRIEND_TEST_ALL_PREFIXES(WebContentsImplTest, NoJSMessageOnInterstitials);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplTest, UpdateTitle);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplTest, FindOpenerRVHWhenPending);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplTest,
                           CrossSiteCantPreemptAfterUnload);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplTest, PendingContents);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplTest, FrameTreeShape);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplTest, GetLastActiveTime);
  FRIEND_TEST_ALL_PREFIXES(FormStructureBrowserTest, HTMLFiles);
  FRIEND_TEST_ALL_PREFIXES(NavigationControllerTest, HistoryNavigate);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostManagerTest, PageDoesBackAndReload);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest, CrossSiteIframe);

  // So InterstitialPageImpl can access SetIsLoading.
  friend class InterstitialPageImpl;

  // TODO(brettw) TestWebContents shouldn't exist!
  friend class TestWebContents;

  class DestructionObserver;

  // See WebContents::Create for a description of these parameters.
  WebContentsImpl(BrowserContext* browser_context,
                  WebContentsImpl* opener);

  // Add and remove observers for page navigation notifications. The order in
  // which notifications are sent to observers is undefined. Clients must be
  // sure to remove the observer before they go away.
  void AddObserver(WebContentsObserver* observer);
  void RemoveObserver(WebContentsObserver* observer);

  // Clears this tab's opener if it has been closed.
  void OnWebContentsDestroyed(WebContentsImpl* web_contents);

  // Creates and adds to the map a destruction observer watching |web_contents|.
  // No-op if such an observer already exists.
  void AddDestructionObserver(WebContentsImpl* web_contents);

  // Deletes and removes from the map a destruction observer
  // watching |web_contents|. No-op if there is no such observer.
  void RemoveDestructionObserver(WebContentsImpl* web_contents);

  // Traverses all the RenderFrameHosts in the FrameTree and creates a set
  // all the unique RenderWidgetHostViews.
  std::set<RenderWidgetHostView*> GetRenderWidgetHostViewsInTree();

  // Callback function when showing JavaScript dialogs.  Takes in a routing ID
  // pair to identify the RenderFrameHost that opened the dialog, because it's
  // possible for the RenderFrameHost to be deleted by the time this is called.
  void OnDialogClosed(int render_process_id,
                      int render_frame_id,
                      IPC::Message* reply_msg,
                      bool dialog_was_suppressed,
                      bool success,
                      const base::string16& user_input);

  // Callback function when requesting permission to access the PPAPI broker.
  // |result| is true if permission was granted.
  void OnPpapiBrokerPermissionResult(int routing_id, bool result);

  bool OnMessageReceived(RenderViewHost* render_view_host,
                         RenderFrameHost* render_frame_host,
                         const IPC::Message& message);

  // IPC message handlers.
  void OnThemeColorChanged(SkColor theme_color);
  void OnDidLoadResourceFromMemoryCache(const GURL& url,
                                        const std::string& security_info,
                                        const std::string& http_request,
                                        const std::string& mime_type,
                                        ResourceType::Type resource_type);
  void OnDidDisplayInsecureContent();
  void OnDidRunInsecureContent(const std::string& security_origin,
                               const GURL& target_url);
  void OnDocumentLoadedInFrame();
  void OnDidFinishLoad(const GURL& url);
  void OnDidStartLoading(bool to_different_document);
  void OnDidStopLoading();
  void OnDidChangeLoadProgress(double load_progress);
  void OnGoToEntryAtOffset(int offset);
  void OnUpdateZoomLimits(int minimum_percent,
                          int maximum_percent);
  void OnEnumerateDirectory(int request_id, const base::FilePath& path);

  void OnRegisterProtocolHandler(const std::string& protocol,
                                 const GURL& url,
                                 const base::string16& title,
                                 bool user_gesture);
  void OnFindReply(int request_id,
                   int number_of_matches,
                   const gfx::Rect& selection_rect,
                   int active_match_ordinal,
                   bool final_update);
#if defined(OS_ANDROID)
  void OnFindMatchRectsReply(int version,
                             const std::vector<gfx::RectF>& rects,
                             const gfx::RectF& active_rect);

  void OnOpenDateTimeDialog(
      const ViewHostMsg_DateTimeDialogValue_Params& value);
#endif
  void OnPepperPluginHung(int plugin_child_id,
                          const base::FilePath& path,
                          bool is_hung);
  void OnPluginCrashed(const base::FilePath& plugin_path,
                       base::ProcessId plugin_pid);
  void OnDomOperationResponse(const std::string& json_string,
                              int automation_id);
  void OnAppCacheAccessed(const GURL& manifest_url, bool blocked_by_policy);
  void OnOpenColorChooser(int color_chooser_id,
                          SkColor color,
                          const std::vector<ColorSuggestion>& suggestions);
  void OnEndColorChooser(int color_chooser_id);
  void OnSetSelectedColorInColorChooser(int color_chooser_id, SkColor color);
  void OnWebUISend(const GURL& source_url,
                   const std::string& name,
                   const base::ListValue& args);
  void OnRequestPpapiBrokerPermission(int routing_id,
                                      const GURL& url,
                                      const base::FilePath& plugin_path);
  void OnBrowserPluginMessage(const IPC::Message& message);
  void OnDidDownloadImage(int id,
                          int http_status_code,
                          const GURL& image_url,
                          const std::vector<SkBitmap>& bitmaps,
                          const std::vector<gfx::Size>& original_bitmap_sizes);
  void OnUpdateFaviconURL(const std::vector<FaviconURL>& candidates);
  void OnFirstVisuallyNonEmptyPaint();
  void OnMediaPlayingNotification(int64 player_cookie,
                                  bool has_video,
                                  bool has_audio);
  void OnMediaPausedNotification(int64 player_cookie);
  void OnShowValidationMessage(const gfx::Rect& anchor_in_root_view,
                               const base::string16& main_text,
                               const base::string16& sub_text);
  void OnHideValidationMessage();
  void OnMoveValidationMessage(const gfx::Rect& anchor_in_root_view);

  // Called by derived classes to indicate that we're no longer waiting for a
  // response. This won't actually update the throbber, but it will get picked
  // up at the next animation step if the throbber is going.
  void SetNotWaitingForResponse() { waiting_for_response_ = false; }

  // Navigation helpers --------------------------------------------------------
  //
  // These functions are helpers for Navigate() and DidNavigate().

  // Handles post-navigation tasks in DidNavigate AFTER the entry has been
  // committed to the navigation controller. Note that the navigation entry is
  // not provided since it may be invalid/changed after being committed. The
  // current navigation entry is in the NavigationController at this point.

  // If our controller was restored, update the max page ID associated with the
  // given RenderViewHost to be larger than the number of restored entries.
  // This is called in CreateRenderView before any navigations in the RenderView
  // have begun, to prevent any races in updating RenderView::next_page_id.
  void UpdateMaxPageIDIfNecessary(RenderViewHost* rvh);

  // Saves the given title to the navigation entry and does associated work. It
  // will update history and the view for the new title, and also synthesize
  // titles for file URLs that have none (so we require that the URL of the
  // entry already be set).
  //
  // This is used as the backend for state updates, which include a new title,
  // or the dedicated set title message. It returns true if the new title is
  // different and was therefore updated.
  bool UpdateTitleForEntry(NavigationEntryImpl* entry,
                           const base::string16& title);

  // Recursively creates swapped out RenderViews for this tab's opener chain
  // (including this tab) in the given SiteInstance, allowing other tabs to send
  // cross-process JavaScript calls to their opener(s).  Returns the route ID of
  // this tab's RenderView for |instance|.
  int CreateOpenerRenderViews(SiteInstance* instance);

  // Helper for CreateNewWidget/CreateNewFullscreenWidget.
  void CreateNewWidget(int render_process_id,
                       int route_id,
                       bool is_fullscreen,
                       blink::WebPopupType popup_type);

  // Helper for ShowCreatedWidget/ShowCreatedFullscreenWidget.
  void ShowCreatedWidget(int route_id,
                         bool is_fullscreen,
                         const gfx::Rect& initial_pos);

  // Finds the new RenderWidgetHost and returns it. Note that this can only be
  // called once as this call also removes it from the internal map.
  RenderWidgetHostView* GetCreatedWidget(int route_id);

  // Finds the new WebContentsImpl by route_id, initializes it for
  // renderer-initiated creation, and returns it. Note that this can only be
  // called once as this call also removes it from the internal map.
  WebContentsImpl* GetCreatedWindow(int route_id);

  // Tracking loading progress -------------------------------------------------

  // Resets the tracking state of the current load.
  void ResetLoadProgressState();

  // Calculates the progress of the current load and notifies the delegate.
  void SendLoadProgressChanged();

  // Called once when the last frame on the page has stopped loading.
  void DidStopLoading(RenderFrameHost* render_frame_host);

  // Misc non-view stuff -------------------------------------------------------

  // Helper functions for sending notifications.
  void NotifySwapped(RenderViewHost* old_host, RenderViewHost* new_host);
  void NotifyDisconnected();

  void SetEncoding(const std::string& encoding);

  // TODO(creis): This should take in a FrameTreeNode to know which node's
  // render manager to return.  For now, we just return the root's.
  RenderFrameHostManager* GetRenderManager() const;

  RenderViewHostImpl* GetRenderViewHostImpl();

  // Removes browser plugin embedder if there is one.
  void RemoveBrowserPluginEmbedder();

  // Clear |render_frame_host|'s PowerSaveBlockers.
  void ClearPowerSaveBlockers(RenderFrameHost* render_frame_host);

  // Clear all PowerSaveBlockers, leave power_save_blocker_ empty.
  void ClearAllPowerSaveBlockers();

  // Helper function to invoke WebContentsDelegate::GetSizeForNewRenderView().
  gfx::Size GetSizeForNewRenderView();

  void OnFrameRemoved(RenderViewHostImpl* render_view_host,
                      int frame_routing_id);

  // Helper method that's called whenever |preferred_size_| or
  // |preferred_size_for_capture_| changes, to propagate the new value to the
  // |delegate_|.
  void OnPreferredSizeChanged(const gfx::Size& old_size);

  // Adds/removes a callback called on creation of each new WebContents.
  // Deprecated, about to remove.
  static void AddCreatedCallback(const CreatedCallback& callback);
  static void RemoveCreatedCallback(const CreatedCallback& callback);

  // Data for core operation ---------------------------------------------------

  // Delegate for notifying our owner about stuff. Not owned by us.
  WebContentsDelegate* delegate_;

  // Handles the back/forward list and loading.
  NavigationControllerImpl controller_;

  // The corresponding view.
  scoped_ptr<WebContentsView> view_;

  // The view of the RVHD. Usually this is our WebContentsView implementation,
  // but if an embedder uses a different WebContentsView, they'll need to
  // provide this.
  RenderViewHostDelegateView* render_view_host_delegate_view_;

  // Tracks created WebContentsImpl objects that have not been shown yet. They
  // are identified by the route ID passed to CreateNewWindow.
  typedef std::map<int, WebContentsImpl*> PendingContents;
  PendingContents pending_contents_;

  // These maps hold on to the widgets that we created on behalf of the renderer
  // that haven't shown yet.
  typedef std::map<int, RenderWidgetHostView*> PendingWidgetViews;
  PendingWidgetViews pending_widget_views_;

  typedef std::map<WebContentsImpl*, DestructionObserver*> DestructionObservers;
  DestructionObservers destruction_observers_;

  // A list of observers notified when page state changes. Weak references.
  // This MUST be listed above frame_tree_ since at destruction time the
  // latter might cause RenderViewHost's destructor to call us and we might use
  // the observer list then.
  ObserverList<WebContentsObserver> observers_;

  // The tab that opened this tab, if any.  Will be set to null if the opener
  // is closed.
  WebContentsImpl* opener_;

  // True if this tab was opened by another tab. This is not unset if the opener
  // is closed.
  bool created_with_opener_;

#if defined(OS_WIN)
  gfx::NativeViewAccessible accessible_parent_;
#endif

  // Helper classes ------------------------------------------------------------

  // Maps the RenderFrameHost to its media_player_cookie and PowerSaveBlocker
  // pairs. Key is the RenderFrameHost, value is the map which maps
  // player_cookie on to PowerSaveBlocker.
  typedef std::map<RenderFrameHost*, std::map<int64, PowerSaveBlocker*> >
      PowerSaveBlockerMap;
  PowerSaveBlockerMap power_save_blockers_;

  // Manages the frame tree of the page and process swaps in each node.
  FrameTree frame_tree_;

  // SavePackage, lazily created.
  scoped_refptr<SavePackage> save_package_;

  // Data for loading state ----------------------------------------------------

  // Indicates whether we're currently loading a resource.
  bool is_loading_;

  // Indicates whether the current load is to a different document. Only valid
  // if is_loading_ is true.
  bool is_load_to_different_document_;

  // Indicates if the tab is considered crashed.
  base::TerminationStatus crashed_status_;
  int crashed_error_code_;

  // Whether this WebContents is waiting for a first-response for the
  // main resource of the page. This controls whether the throbber state is
  // "waiting" or "loading."
  bool waiting_for_response_;

  // Map of SiteInstance ID to max page ID for this tab. A page ID is specific
  // to a given tab and SiteInstance, and must be valid for the lifetime of the
  // WebContentsImpl.
  std::map<int32, int32> max_page_ids_;

  // The current load state and the URL associated with it.
  net::LoadStateWithParam load_state_;
  base::string16 load_state_host_;

  // LoadingProgressMap maps FrameTreeNode IDs to a double representing that
  // frame's completion (from 0 to 1).
  typedef base::hash_map<int64, double> LoadingProgressMap;
  LoadingProgressMap loading_progresses_;
  double loading_total_progress_;

  base::TimeTicks loading_last_progress_update_;

  base::WeakPtrFactory<WebContentsImpl> loading_weak_factory_;

  // Counter to track how many frames have sent start notifications but not
  // stop notifications.
  int loading_frames_in_progress_;

  // Upload progress, for displaying in the status bar.
  // Set to zero when there is no significant upload happening.
  uint64 upload_size_;
  uint64 upload_position_;

  // Data for current page -----------------------------------------------------

  // When a title cannot be taken from any entry, this title will be used.
  base::string16 page_title_when_no_navigation_entry_;

  // When a navigation occurs, we record its contents MIME type. It can be
  // used to check whether we can do something for some special contents.
  std::string contents_mime_type_;

  // The last reported character encoding, not canonicalized.
  std::string last_reported_encoding_;

  // The canonicalized character encoding.
  std::string canonical_encoding_;

  // True if this is a secure page which displayed insecure content.
  bool displayed_insecure_content_;

  // Whether the initial empty page has been accessed by another page, making it
  // unsafe to show the pending URL. Usually false unless another window tries
  // to modify the blank page.  Always false after the first commit.
  bool has_accessed_initial_document_;

  // Data for misc internal state ----------------------------------------------

  // When > 0, the WebContents is currently being captured (e.g., for
  // screenshots or mirroring); and the underlying RenderWidgetHost should not
  // be told it is hidden.
  int capturer_count_;

  // Tracks whether RWHV should be visible once capturer_count_ becomes zero.
  bool should_normally_be_visible_;

  // See getter above.
  bool is_being_destroyed_;

  // Indicates whether we should notify about disconnection of this
  // WebContentsImpl. This is used to ensure disconnection notifications only
  // happen if a connection notification has happened and that they happen only
  // once.
  bool notify_disconnection_;

  // Pointer to the JavaScript dialog manager, lazily assigned. Used because the
  // delegate of this WebContentsImpl is nulled before its destructor is called.
  JavaScriptDialogManager* dialog_manager_;

  // Set to true when there is an active "before unload" dialog.  When true,
  // we've forced the throbber to start in Navigate, and we need to remember to
  // turn it off in OnJavaScriptMessageBoxClosed if the navigation is canceled.
  bool is_showing_before_unload_dialog_;

  // Settings that get passed to the renderer process.
  RendererPreferences renderer_preferences_;

  // The time that this WebContents was last made active. The initial value is
  // the WebContents creation time.
  base::TimeTicks last_active_time_;

  // See description above setter.
  bool closed_by_user_gesture_;

  // Minimum/maximum zoom percent.
  int minimum_zoom_percent_;
  int maximum_zoom_percent_;

  // The raw accumulated zoom value and the actual zoom increments made for an
  // an in-progress pinch gesture.
  float totalPinchGestureAmount_;
  int currentPinchZoomStepDelta_;

  // The intrinsic size of the page.
  gfx::Size preferred_size_;

  // The preferred size for content screen capture.  When |capturer_count_| > 0,
  // this overrides |preferred_size_|.
  gfx::Size preferred_size_for_capture_;

#if defined(OS_ANDROID)
  // Date time chooser opened by this tab.
  // Only used in Android since all other platforms use a multi field UI.
  scoped_ptr<DateTimeChooserAndroid> date_time_chooser_;
#endif

  // Holds information about a current color chooser dialog, if one is visible.
  struct ColorChooserInfo {
    ColorChooserInfo(int render_process_id,
                     int render_frame_id,
                     ColorChooser* chooser,
                     int identifier);
    ~ColorChooserInfo();

    int render_process_id;
    int render_frame_id;

    // Color chooser that was opened by this tab.
    scoped_ptr<ColorChooser> chooser;

    // A unique identifier for the current color chooser.  Identifiers are
    // unique across a renderer process.  This avoids race conditions in
    // synchronizing the browser and renderer processes.  For example, if a
    // renderer closes one chooser and opens another, and simultaneously the
    // user picks a color in the first chooser, the IDs can be used to drop the
    // "chose a color" message rather than erroneously tell the renderer that
    // the user picked a color in the second chooser.
    int identifier;
  };

  scoped_ptr<ColorChooserInfo> color_chooser_info_;

  // Manages the embedder state for browser plugins, if this WebContents is an
  // embedder; NULL otherwise.
  scoped_ptr<BrowserPluginEmbedder> browser_plugin_embedder_;
  // Manages the guest state for browser plugin, if this WebContents is a guest;
  // NULL otherwise.
  scoped_ptr<BrowserPluginGuest> browser_plugin_guest_;

  // This must be at the end, or else we might get notifications and use other
  // member variables that are gone.
  NotificationRegistrar registrar_;

  // Used during IPC message dispatching from the RenderView/RenderFrame so that
  // the handlers can get a pointer to the RVH through which the message was
  // received.
  RenderViewHost* render_view_message_source_;
  RenderFrameHost* render_frame_message_source_;

  // All live RenderWidgetHostImpls that are created by this object and may
  // outlive it.
  std::set<RenderWidgetHostImpl*> created_widgets_;

  // Routing id of the shown fullscreen widget or MSG_ROUTING_NONE otherwise.
  int fullscreen_widget_routing_id_;

  // Maps the ids of pending image downloads to their callbacks
  typedef std::map<int, ImageDownloadCallback> ImageDownloadMap;
  ImageDownloadMap image_download_map_;

  // Whether this WebContents is responsible for displaying a subframe in a
  // different process from its parent page.
  bool is_subframe_;

  // Whether touch emulation is enabled in RenderWidgetHost.
  bool touch_emulation_enabled_;

  // Whether the last JavaScript dialog shown was suppressed. Used for testing.
  bool last_dialog_suppressed_;

  scoped_ptr<GeolocationDispatcherHost> geolocation_dispatcher_host_;

  scoped_ptr<MidiDispatcherHost> midi_dispatcher_host_;

  scoped_ptr<ScreenOrientationDispatcherHost>
      screen_orientation_dispatcher_host_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_IMPL_H_
