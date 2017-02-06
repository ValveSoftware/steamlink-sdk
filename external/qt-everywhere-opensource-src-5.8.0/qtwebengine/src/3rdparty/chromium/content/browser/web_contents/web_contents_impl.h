// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_IMPL_H_
#define CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_IMPL_H_

#include <stdint.h>

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/compiler_specific.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/process/process.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/navigation_controller_delegate.h"
#include "content/browser/frame_host/navigation_controller_impl.h"
#include "content/browser/frame_host/navigator_delegate.h"
#include "content/browser/frame_host/render_frame_host_delegate.h"
#include "content/browser/frame_host/render_frame_host_manager.h"
#include "content/browser/media/audio_stream_monitor.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/common/accessibility_mode_enums.h"
#include "content/common/content_export.h"
#include "content/public/browser/color_chooser.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/page_importance_signals.h"
#include "content/public/common/renderer_preferences.h"
#include "content/public/common/resource_type.h"
#include "content/public/common/three_d_api_types.h"
#include "net/base/load_states.h"
#include "net/http/http_response_headers.h"
#include "third_party/WebKit/public/platform/WebDragOperation.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"

struct ViewHostMsg_DateTimeDialogValue_Params;

namespace content {
class BrowserPluginEmbedder;
class BrowserPluginGuest;
class DateTimeChooserAndroid;
class DownloadItem;
class FindRequestManager;
class GeolocationServiceContext;
class InterstitialPageImpl;
class JavaScriptDialogManager;
class LoaderIOThreadNotifier;
class ManifestManagerHost;
class MediaWebContentsObserver;
class PluginContentOriginWhitelist;
class PowerSaveBlocker;
class RenderViewHost;
class RenderViewHostDelegateView;
class RenderWidgetHostImpl;
class RenderWidgetHostInputEventRouter;
class SavePackage;
class ScreenOrientationDispatcherHost;
class SiteInstance;
class TestWebContents;
class TextInputManager;
class WakeLockServiceContext;
class WebContentsAudioMuter;
class WebContentsDelegate;
class WebContentsImpl;
class WebContentsView;
class WebContentsViewDelegate;
struct AXEventNotificationDetails;
struct ColorSuggestion;
struct FaviconURL;
struct LoadNotificationDetails;
struct MHTMLGenerationParams;
struct ResourceRedirectDetails;
struct ResourceRequestDetails;

#if defined(OS_ANDROID)
class WebContentsAndroid;
#endif

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
  class FriendZone;

  ~WebContentsImpl() override;

  static WebContentsImpl* CreateWithOpener(
      const WebContents::CreateParams& params,
      FrameTreeNode* opener);

  static std::vector<WebContentsImpl*> GetAllWebContents();

  static WebContentsImpl* FromFrameTreeNode(FrameTreeNode* frame_tree_node);
  static WebContents* FromRenderFrameHostID(int render_process_host_id,
                                            int render_frame_host_id);

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

  // Returns the outer WebContents of this WebContents if any.
  // Note that without --site-per-process, this will return the WebContents
  // of the BrowserPluginEmbedder, if |this| is a guest.
  WebContentsImpl* GetOuterWebContents();

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

  // Creates a BrowserPluginEmbedder object for this WebContents if one doesn't
  // already exist.
  void CreateBrowserPluginEmbedderIfNecessary();

  // Cancels modal dialogs in this WebContents, as well as in any browser
  // plugins it is hosting.
  void CancelActiveAndPendingDialogs();

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
      RenderFrameHost* render_frame_host,
      const ResourceRedirectDetails& details);

  // Notify observers that the web contents has been focused.
  void NotifyWebContentsFocused();

  WebContentsView* GetView() const;

  ScreenOrientationDispatcherHost* screen_orientation_dispatcher_host() {
    return screen_orientation_dispatcher_host_.get();
  }

  bool should_normally_be_visible() { return should_normally_be_visible_; }

  // Indicate if the window has been occluded, and pass this to the views, only
  // if there is no active capture going on (otherwise it is dropped on the
  // floor).
  void WasOccluded();
  void WasUnOccluded();

  // Broadcasts the mode change to all frames.
  void SetAccessibilityMode(AccessibilityMode mode);

  // Adds the given accessibility mode to the current accessibility mode
  // bitmap.
  void AddAccessibilityMode(AccessibilityMode mode);

  // Removes the given accessibility mode from the current accessibility
  // mode bitmap, managing the bits that are shared with other modes such
  // that a bit will only be turned off when all modes that depend on it
  // have been removed.
  void RemoveAccessibilityMode(AccessibilityMode mode);

  // Request a one-time snapshot of the accessibility tree without changing
  // the accessibility mode.
  using AXTreeSnapshotCallback =
      base::Callback<void(
          const ui::AXTreeUpdate&)>;
  void RequestAXTreeSnapshot(AXTreeSnapshotCallback callback);

  // Set a temporary zoom level for the frames associated with this WebContents.
  // If |is_temporary| is true, we are setting a new temporary zoom level,
  // otherwise we are clearing a previously set temporary zoom level.
  void SetTemporaryZoomLevel(double level, bool temporary_zoom_enabled);

  // Sets the zoom level for frames associated with this WebContents.
  void UpdateZoom(double level);

  // Sets the zoom level for frames associated with this WebContents if it
  // matches |host| and (if non-empty) |scheme|. Matching is done on the
  // last committed entry.
  void UpdateZoomIfNecessary(const std::string& scheme,
                             const std::string& host,
                             double level);

  // WebContents ------------------------------------------------------
  WebContentsDelegate* GetDelegate() override;
  void SetDelegate(WebContentsDelegate* delegate) override;
  NavigationControllerImpl& GetController() override;
  const NavigationControllerImpl& GetController() const override;
  BrowserContext* GetBrowserContext() const override;
  const GURL& GetURL() const override;
  const GURL& GetVisibleURL() const override;
  const GURL& GetLastCommittedURL() const override;
  RenderProcessHost* GetRenderProcessHost() const override;
  RenderFrameHostImpl* GetMainFrame() override;
  RenderFrameHostImpl* GetFocusedFrame() override;
  RenderFrameHostImpl* FindFrameByFrameTreeNodeId(
      int frame_tree_node_id) override;
  void ForEachFrame(
      const base::Callback<void(RenderFrameHost*)>& on_frame) override;
  std::vector<RenderFrameHost*> GetAllFrames() override;
  int SendToAllFrames(IPC::Message* message) override;
  RenderViewHostImpl* GetRenderViewHost() const override;
  int GetRoutingID() const override;
  RenderWidgetHostView* GetRenderWidgetHostView() const override;
  RenderWidgetHostView* GetTopLevelRenderWidgetHostView() override;
  void ClosePage() override;
  RenderWidgetHostView* GetFullscreenRenderWidgetHostView() const override;
  SkColor GetThemeColor() const override;
  WebUI* CreateSubframeWebUI(const GURL& url,
                             const std::string& frame_name) override;
  WebUI* GetWebUI() const override;
  WebUI* GetCommittedWebUI() const override;
  void SetUserAgentOverride(const std::string& override) override;
  const std::string& GetUserAgentOverride() const override;
  void EnableTreeOnlyAccessibilityMode() override;
  bool IsTreeOnlyAccessibilityModeForTesting() const override;
  bool IsFullAccessibilityModeForTesting() const override;
  const PageImportanceSignals& GetPageImportanceSignals() const override;
  const base::string16& GetTitle() const override;
  int32_t GetMaxPageID() override;
  int32_t GetMaxPageIDForSiteInstance(SiteInstance* site_instance) override;
  SiteInstanceImpl* GetSiteInstance() const override;
  SiteInstanceImpl* GetPendingSiteInstance() const override;
  bool IsLoading() const override;
  bool IsLoadingToDifferentDocument() const override;
  bool IsWaitingForResponse() const override;
  const net::LoadStateWithParam& GetLoadState() const override;
  const base::string16& GetLoadStateHost() const override;
  uint64_t GetUploadSize() const override;
  uint64_t GetUploadPosition() const override;
  const std::string& GetEncoding() const override;
  bool DisplayedInsecureContent() const override;
  void IncrementCapturerCount(const gfx::Size& capture_size) override;
  void DecrementCapturerCount() override;
  int GetCapturerCount() const override;
  bool IsAudioMuted() const override;
  void SetAudioMuted(bool mute) override;
  bool IsConnectedToBluetoothDevice() const override;
  bool IsCrashed() const override;
  void SetIsCrashed(base::TerminationStatus status, int error_code) override;
  base::TerminationStatus GetCrashedStatus() const override;
  int GetCrashedErrorCode() const override;
  bool IsBeingDestroyed() const override;
  void NotifyNavigationStateChanged(InvalidateTypes changed_flags) override;
  base::TimeTicks GetLastActiveTime() const override;
  void SetLastActiveTime(base::TimeTicks last_active_time) override;
  void WasShown() override;
  void WasHidden() override;
  bool NeedToFireBeforeUnload() override;
  void DispatchBeforeUnload() override;
  void AttachToOuterWebContentsFrame(
      WebContents* outer_web_contents,
      RenderFrameHost* outer_contents_frame) override;
  void Stop() override;
  WebContents* Clone() override;
  void ReloadFocusedFrame(bool bypass_cache) override;
  void Undo() override;
  void Redo() override;
  void Cut() override;
  void Copy() override;
  void CopyToFindPboard() override;
  void Paste() override;
  void PasteAndMatchStyle() override;
  void Delete() override;
  void SelectAll() override;
  void Unselect() override;
  void Replace(const base::string16& word) override;
  void ReplaceMisspelling(const base::string16& word) override;
  void NotifyContextMenuClosed(
      const CustomContextMenuContext& context) override;
  void ReloadLoFiImages() override;
  void ExecuteCustomContextMenuCommand(
      int action,
      const CustomContextMenuContext& context) override;
  gfx::NativeView GetNativeView() override;
  gfx::NativeView GetContentNativeView() override;
  gfx::NativeWindow GetTopLevelNativeWindow() override;
  gfx::Rect GetContainerBounds() override;
  gfx::Rect GetViewBounds() override;
  DropData* GetDropData() override;
  void Focus() override;
  void SetInitialFocus() override;
  void StoreFocus() override;
  void RestoreFocus() override;
  void FocusThroughTabTraversal(bool reverse) override;
  bool ShowingInterstitialPage() const override;
  InterstitialPage* GetInterstitialPage() const override;
  bool IsSavable() override;
  void OnSavePage() override;
  bool SavePage(const base::FilePath& main_file,
                const base::FilePath& dir_path,
                SavePageType save_type) override;
  void SaveFrame(const GURL& url, const Referrer& referrer) override;
  void SaveFrameWithHeaders(const GURL& url,
                            const Referrer& referrer,
                            const std::string& headers) override;
  void GenerateMHTML(const MHTMLGenerationParams& params,
                     const base::Callback<void(int64_t)>& callback) override;
  const std::string& GetContentsMimeType() const override;
  bool WillNotifyDisconnection() const override;
  void SetOverrideEncoding(const std::string& encoding) override;
  void ResetOverrideEncoding() override;
  RendererPreferences* GetMutableRendererPrefs() override;
  void Close() override;
  void SystemDragEnded() override;
  void UserGestureDone() override;
  void SetClosedByUserGesture(bool value) override;
  bool GetClosedByUserGesture() const override;
  void ViewSource() override;
  void ViewFrameSource(const GURL& url, const PageState& page_state) override;
  int GetMinimumZoomPercent() const override;
  int GetMaximumZoomPercent() const override;
  void SetPageScale(float page_scale_factor) override;
  gfx::Size GetPreferredSize() const override;
  bool GotResponseToLockMouseRequest(bool allowed) override;
  bool HasOpener() const override;
  WebContentsImpl* GetOpener() const override;
  void DidChooseColorInColorChooser(SkColor color) override;
  void DidEndColorChooser() override;
  int DownloadImage(const GURL& url,
                    bool is_favicon,
                    uint32_t max_bitmap_size,
                    bool bypass_cache,
                    const ImageDownloadCallback& callback) override;
  bool IsSubframe() const override;
  void Find(int request_id,
            const base::string16& search_text,
            const blink::WebFindOptions& options) override;
  void StopFinding(StopFindAction action) override;
  void InsertCSS(const std::string& css) override;
  bool WasRecentlyAudible() override;
  void GetManifest(const GetManifestCallback& callback) override;
  void HasManifest(const HasManifestCallback& callback) override;
  void ExitFullscreen(bool will_cause_resize) override;
  void NotifyFullscreenChanged(bool will_cause_resize) override;
  void ResumeLoadingCreatedWebContents() override;
  void OnMediaSessionStateChanged();
  void ResumeMediaSession() override;
  void SuspendMediaSession() override;
  void StopMediaSession() override;

#if defined(OS_ANDROID)
  base::android::ScopedJavaLocalRef<jobject> GetJavaWebContents() override;
  virtual WebContentsAndroid* GetWebContentsAndroid();
  void ActivateNearestFindResult(float x, float y) override;
  void RequestFindMatchRects(int current_version) override;
#elif defined(OS_MACOSX)
  void SetAllowOtherViews(bool allow) override;
  bool GetAllowOtherViews() override;
#endif

  // Implementation of PageNavigator.
  WebContents* OpenURL(const OpenURLParams& params) override;

  // Implementation of IPC::Sender.
  bool Send(IPC::Message* message) override;

  // RenderFrameHostDelegate ---------------------------------------------------
  bool OnMessageReceived(RenderFrameHost* render_frame_host,
                         const IPC::Message& message) override;
  const GURL& GetMainFrameLastCommittedURL() const override;
  void RenderFrameCreated(RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override;
  void ShowContextMenu(RenderFrameHost* render_frame_host,
                       const ContextMenuParams& params) override;
  void RunJavaScriptMessage(RenderFrameHost* render_frame_host,
                            const base::string16& message,
                            const base::string16& default_prompt,
                            const GURL& frame_url,
                            JavaScriptMessageType type,
                            IPC::Message* reply_msg) override;
  void RunBeforeUnloadConfirm(RenderFrameHost* render_frame_host,
                              bool is_reload,
                              IPC::Message* reply_msg) override;
  void RunFileChooser(RenderFrameHost* render_frame_host,
                      const FileChooserParams& params) override;
  void DidAccessInitialDocument() override;
  void DidChangeName(RenderFrameHost* render_frame_host,
                     const std::string& name) override;
  void DocumentOnLoadCompleted(RenderFrameHost* render_frame_host) override;
  void UpdateStateForFrame(RenderFrameHost* render_frame_host,
                           const PageState& page_state) override;
  void UpdateTitle(RenderFrameHost* render_frame_host,
                   int32_t page_id,
                   const base::string16& title,
                   base::i18n::TextDirection title_direction) override;
  void UpdateEncoding(RenderFrameHost* render_frame_host,
                      const std::string& encoding) override;
  WebContents* GetAsWebContents() override;
  bool IsNeverVisible() override;
  AccessibilityMode GetAccessibilityMode() const override;
  void AccessibilityEventReceived(
      const std::vector<AXEventNotificationDetails>& details) override;
  RenderFrameHost* GetGuestByInstanceID(
      RenderFrameHost* render_frame_host,
      int browser_plugin_instance_id) override;
  GeolocationServiceContext* GetGeolocationServiceContext() override;
  WakeLockServiceContext* GetWakeLockServiceContext() override;
  void EnterFullscreenMode(const GURL& origin) override;
  void ExitFullscreenMode(bool will_cause_resize) override;
  bool ShouldRouteMessageEvent(
      RenderFrameHost* target_rfh,
      SiteInstance* source_site_instance) const override;
  void EnsureOpenerProxiesExist(RenderFrameHost* source_rfh) override;
  std::unique_ptr<WebUIImpl> CreateWebUIForRenderFrameHost(
      const GURL& url) override;

  // RenderViewHostDelegate ----------------------------------------------------
  RenderViewHostDelegateView* GetDelegateView() override;
  bool OnMessageReceived(RenderViewHost* render_view_host,
                         const IPC::Message& message) override;
  // RenderFrameHostDelegate has the same method, so list it there because this
  // interface is going away.
  // WebContents* GetAsWebContents() override;
  void RenderViewCreated(RenderViewHost* render_view_host) override;
  void RenderViewReady(RenderViewHost* render_view_host) override;
  void RenderViewTerminated(RenderViewHost* render_view_host,
                            base::TerminationStatus status,
                            int error_code) override;
  void RenderViewDeleted(RenderViewHost* render_view_host) override;
  void UpdateState(RenderViewHost* render_view_host,
                   int32_t page_id,
                   const PageState& page_state) override;
  void UpdateTargetURL(RenderViewHost* render_view_host,
                       const GURL& url) override;
  void Close(RenderViewHost* render_view_host) override;
  void RequestMove(const gfx::Rect& new_bounds) override;
  void DidCancelLoading() override;
  void DocumentAvailableInMainFrame(RenderViewHost* render_view_host) override;
  void RouteCloseEvent(RenderViewHost* rvh) override;
  bool AddMessageToConsole(int32_t level,
                           const base::string16& message,
                           int32_t line_no,
                           const base::string16& source_id) override;
  RendererPreferences GetRendererPrefs(
      BrowserContext* browser_context) const override;
  void OnUserInteraction(RenderWidgetHostImpl* render_widget_host,
                         const blink::WebInputEvent::Type type) override;
  void OnIgnoredUIEvent() override;
  void LoadStateChanged(const GURL& url,
                        const net::LoadStateWithParam& load_state,
                        uint64_t upload_position,
                        uint64_t upload_size) override;
  void Activate() override;
  void UpdatePreferredSize(const gfx::Size& pref_size) override;
  void CreateNewWindow(
      SiteInstance* source_site_instance,
      int32_t route_id,
      int32_t main_frame_route_id,
      int32_t main_frame_widget_route_id,
      const ViewHostMsg_CreateWindow_Params& params,
      SessionStorageNamespace* session_storage_namespace) override;
  void CreateNewWidget(int32_t render_process_id,
                       int32_t route_id,
                       blink::WebPopupType popup_type) override;
  void CreateNewFullscreenWidget(int32_t render_process_id,
                                 int32_t route_id) override;
  void ShowCreatedWindow(int process_id,
                         int route_id,
                         WindowOpenDisposition disposition,
                         const gfx::Rect& initial_rect,
                         bool user_gesture) override;
  void ShowCreatedWidget(int process_id,
                         int route_id,
                         const gfx::Rect& initial_rect) override;
  void ShowCreatedFullscreenWidget(int process_id, int route_id) override;
  void RequestMediaAccessPermission(
      const MediaStreamRequest& request,
      const MediaResponseCallback& callback) override;
  bool CheckMediaAccessPermission(const GURL& security_origin,
                                  MediaStreamType type) override;
  SessionStorageNamespace* GetSessionStorageNamespace(
      SiteInstance* instance) override;
  SessionStorageNamespaceMap GetSessionStorageNamespaceMap() override;
  FrameTree* GetFrameTree() override;
  void SetIsVirtualKeyboardRequested(bool requested) override;
  bool IsVirtualKeyboardRequested() override;
  bool IsOverridingUserAgent() override;
  double GetPendingPageZoomLevel() override;

  // NavigatorDelegate ---------------------------------------------------------

  void DidStartNavigation(NavigationHandle* navigation_handle) override;
  void DidRedirectNavigation(NavigationHandle* navigation_handle) override;
  void ReadyToCommitNavigation(NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;
  void DidStartProvisionalLoad(RenderFrameHostImpl* render_frame_host,
                               const GURL& validated_url,
                               bool is_error_page,
                               bool is_iframe_srcdoc) override;
  void DidFailProvisionalLoadWithError(
      RenderFrameHostImpl* render_frame_host,
      const FrameHostMsg_DidFailProvisionalLoadWithError_Params& params)
      override;
  void DidFailLoadWithError(RenderFrameHostImpl* render_frame_host,
                            const GURL& url,
                            int error_code,
                            const base::string16& error_description,
                            bool was_ignored_by_handler) override;
  void DidCommitProvisionalLoad(RenderFrameHostImpl* render_frame_host,
                                const GURL& url,
                                ui::PageTransition transition_type) override;
  void DidNavigateMainFramePreCommit(bool navigation_is_within_page) override;
  void DidNavigateMainFramePostCommit(
      RenderFrameHostImpl* render_frame_host,
      const LoadCommittedDetails& details,
      const FrameHostMsg_DidCommitProvisionalLoad_Params& params) override;
  void DidNavigateAnyFramePostCommit(
      RenderFrameHostImpl* render_frame_host,
      const LoadCommittedDetails& details,
      const FrameHostMsg_DidCommitProvisionalLoad_Params& params) override;
  void SetMainFrameMimeType(const std::string& mime_type) override;
  bool CanOverscrollContent() const override;
  void NotifyChangedNavigationState(InvalidateTypes changed_flags) override;
  void DidStartNavigationToPendingEntry(
      const GURL& url,
      NavigationController::ReloadType reload_type) override;
  void RequestOpenURL(RenderFrameHostImpl* render_frame_host,
                      const OpenURLParams& params) override;
  bool ShouldTransferNavigation() override;
  bool ShouldPreserveAbortedURLs() override;
  void DidStartLoading(FrameTreeNode* frame_tree_node,
                       bool to_different_document) override;
  void DidStopLoading() override;
  void DidChangeLoadProgress() override;

  // RenderWidgetHostDelegate --------------------------------------------------

  void RenderWidgetCreated(RenderWidgetHostImpl* render_widget_host) override;
  void RenderWidgetDeleted(RenderWidgetHostImpl* render_widget_host) override;
  void RenderWidgetGotFocus(RenderWidgetHostImpl* render_widget_host) override;
  void RenderWidgetWasResized(RenderWidgetHostImpl* render_widget_host,
                              bool width_changed) override;
  void ResizeDueToAutoResize(RenderWidgetHostImpl* render_widget_host,
                             const gfx::Size& new_size) override;
  void ScreenInfoChanged() override;
  bool PreHandleKeyboardEvent(const NativeWebKeyboardEvent& event,
                              bool* is_keyboard_shortcut) override;
  void HandleKeyboardEvent(const NativeWebKeyboardEvent& event) override;
  bool HandleWheelEvent(const blink::WebMouseWheelEvent& event) override;
  bool PreHandleGestureEvent(const blink::WebGestureEvent& event) override;
  BrowserAccessibilityManager* GetRootBrowserAccessibilityManager() override;
  BrowserAccessibilityManager* GetOrCreateRootBrowserAccessibilityManager()
      override;
  // The following 4 functions are already listed under WebContents overrides:
  // void Cut() override;
  // void Copy() override;
  // void Paste() override;
  // void SelectAll() override;
  void MoveRangeSelectionExtent(const gfx::Point& extent) override;
  void SelectRange(const gfx::Point& base, const gfx::Point& extent) override;
  void AdjustSelectionByCharacterOffset(int start_adjust, int end_adjust)
      override;
  RenderWidgetHostInputEventRouter* GetInputEventRouter() override;
  void ReplicatePageFocus(bool is_focused) override;
  RenderWidgetHostImpl* GetFocusedRenderWidgetHost(
      RenderWidgetHostImpl* receiving_widget) override;
  void RendererUnresponsive(
      RenderWidgetHostImpl* render_widget_host,
      RenderWidgetHostDelegate::RendererUnresponsiveType type) override;
  void RendererResponsive(RenderWidgetHostImpl* render_widget_host) override;
  void RequestToLockMouse(RenderWidgetHostImpl* render_widget_host,
                          bool user_gesture,
                          bool last_unlocked_by_target,
                          bool privileged) override;
  gfx::Rect GetRootWindowResizerRect(
      RenderWidgetHostImpl* render_widget_host) const override;
  bool IsFullscreenForCurrentTab() const override;
  blink::WebDisplayMode GetDisplayMode(
      RenderWidgetHostImpl* render_widget_host) const override;
  void LostCapture(RenderWidgetHostImpl* render_widget_host) override;
  void LostMouseLock(RenderWidgetHostImpl* render_widget_host) override;
  bool HasMouseLock(RenderWidgetHostImpl* render_widget_host) override;
  void ForwardCompositorProto(RenderWidgetHostImpl* render_widget_host,
                              const std::vector<uint8_t>& proto) override;
  void OnRenderFrameProxyVisibilityChanged(bool visible) override;
  void SendScreenRects() override;
  void OnFirstPaintAfterLoad(RenderWidgetHostImpl* render_widget_host) override;
  TextInputManager* GetTextInputManager() override;

  // RenderFrameHostManager::Delegate ------------------------------------------

  bool CreateRenderViewForRenderManager(
      RenderViewHost* render_view_host,
      int opener_frame_routing_id,
      int proxy_routing_id,
      const FrameReplicationState& replicated_frame_state) override;
  void CreateRenderWidgetHostViewForRenderManager(
      RenderViewHost* render_view_host) override;
  bool CreateRenderFrameForRenderManager(
      RenderFrameHost* render_frame_host,
      int proxy_routing_id,
      int opener_routing_id,
      int parent_routing_id,
      int previous_sibling_routing_id) override;
  void BeforeUnloadFiredFromRenderManager(
      bool proceed,
      const base::TimeTicks& proceed_time,
      bool* proceed_to_fire_unload) override;
  void RenderProcessGoneFromRenderManager(
      RenderViewHost* render_view_host) override;
  void UpdateRenderViewSizeForRenderManager() override;
  void CancelModalDialogsForRenderManager() override;
  void NotifySwappedFromRenderManager(RenderFrameHost* old_host,
                                      RenderFrameHost* new_host,
                                      bool is_main_frame) override;
  void NotifyMainFrameSwappedFromRenderManager(
      RenderViewHost* old_host,
      RenderViewHost* new_host) override;
  NavigationControllerImpl& GetControllerForRenderManager() override;
  NavigationEntry* GetLastCommittedNavigationEntryForRenderManager() override;
  bool FocusLocationBarByDefault() override;
  void SetFocusToLocationBar(bool select_all) override;
  bool IsHidden() override;
  int GetOuterDelegateFrameTreeNodeId() override;

  // NotificationObserver ------------------------------------------------------

  void Observe(int type,
               const NotificationSource& source,
               const NotificationDetails& details) override;

  // NavigationControllerDelegate ----------------------------------------------

  WebContents* GetWebContents() override;
  void NotifyNavigationEntryCommitted(
      const LoadCommittedDetails& load_details) override;

  // Invoked before a form repost warning is shown.
  void NotifyBeforeFormRepostWarningShow() override;

  // Activate this WebContents and show a form repost warning.
  void ActivateAndShowRepostFormWarningDialog() override;

  // Whether the initial empty page of this view has been accessed by another
  // page, making it unsafe to show the pending URL. Always false after the
  // first commit.
  bool HasAccessedInitialDocument() override;

  // Updates the max page ID for the current SiteInstance in this
  // WebContentsImpl to be at least |page_id|.
  void UpdateMaxPageID(int32_t page_id) override;

  // Updates the max page ID for the given SiteInstance in this WebContentsImpl
  // to be at least |page_id|.
  void UpdateMaxPageIDForSiteInstance(SiteInstance* site_instance,
                                      int32_t page_id) override;

  // Copy the current map of SiteInstance ID to max page ID from another tab.
  // This is necessary when this tab adopts the NavigationEntries from
  // |web_contents|.
  void CopyMaxPageIDsFrom(WebContents* web_contents) override;

  // Sets the history for this WebContentsImpl to |history_length| entries, with
  // an offset of |history_offset|.
  void SetHistoryOffsetAndLength(int history_offset,
                                 int history_length) override;

  // Called by InterstitialPageImpl when it creates a RenderFrameHost.
  void RenderFrameForInterstitialPageCreated(
      RenderFrameHost* render_frame_host) override;

  // Sets the passed interstitial as the currently showing interstitial.
  // No interstitial page should already be attached.
  void AttachInterstitialPage(InterstitialPageImpl* interstitial_page) override;

  // Unsets the currently showing interstitial.
  void DetachInterstitialPage() override;

  void UpdateOverridingUserAgent() override;

  // Unpause the throbber if it was paused.
  void DidProceedOnInterstitial() override;

  typedef base::Callback<void(WebContents*)> CreatedCallback;

  // Forces overscroll to be disabled (used by touch emulation).
  void SetForceDisableOverscrollContent(bool force_disable);

  AudioStreamMonitor* audio_stream_monitor() {
    return &audio_stream_monitor_;
  }

  // Called by MediaWebContentsObserver when playback starts or stops.  See the
  // WebContentsObserver function stubs for more details.
  void MediaStartedPlaying(const WebContentsObserver::MediaPlayerId& id);
  void MediaStoppedPlaying(const WebContentsObserver::MediaPlayerId& id);

  MediaWebContentsObserver* media_web_contents_observer() {
    return media_web_contents_observer_.get();
  }

  // Update the web contents visibility.
  void UpdateWebContentsVisibility(bool visible);

  // Called by FindRequestManager when find replies come in from a renderer
  // process.
  void NotifyFindReply(int request_id,
                       int number_of_matches,
                       const gfx::Rect& selection_rect,
                       int active_match_ordinal,
                       bool final_update);

  // Modify the counter of connected devices for this WebContents.
  void IncrementBluetoothConnectedDeviceCount();
  void DecrementBluetoothConnectedDeviceCount();

#if defined(OS_ANDROID)
  // Called by FindRequestManager when all of the find match rects are in.
  void NotifyFindMatchRectsReply(int version,
                                 const std::vector<gfx::RectF>& rects,
                                 const gfx::RectF& active_rect);
#endif

 private:
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
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplTest,
                           LoadResourceFromMemoryCacheWithBadSecurityInfo);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplTest,
                           LoadResourceFromMemoryCacheWithEmptySecurityInfo);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplTest,
                           ResetJavaScriptDialogOnUserNavigate);
  FRIEND_TEST_ALL_PREFIXES(FormStructureBrowserTest, HTMLFiles);
  FRIEND_TEST_ALL_PREFIXES(NavigationControllerTest, HistoryNavigate);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostManagerTest, PageDoesBackAndReload);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest, CrossSiteIframe);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           TwoSubframesCreatePopupsSimultaneously);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           TwoSubframesCreatePopupMenuWidgetsSimultaneously);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessAccessibilityBrowserTest,
                           CrossSiteIframeAccessibility);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplBrowserTest,
                           JavaScriptDialogsInMainAndSubframes);

  // So |find_request_manager_| can be accessed for testing.
  friend class FindRequestManagerTest;

  // So InterstitialPageImpl can access SetIsLoading.
  friend class InterstitialPageImpl;

  // TODO(brettw) TestWebContents shouldn't exist!
  friend class TestWebContents;

  class DestructionObserver;

  // Represents a WebContents node in a tree of WebContents structure.
  //
  // Two WebContents with separate FrameTrees can be connected by
  // outer/inner relationship using this class. Note that their FrameTrees
  // still remain disjoint.
  // The parent is referred to as "outer WebContents" and the descendents are
  // referred to as "inner WebContents".
  // For each inner WebContents, the outer WebContents will have a
  // corresponding FrameTreeNode.
  struct WebContentsTreeNode {
   public:
    WebContentsTreeNode();
    ~WebContentsTreeNode();

    typedef std::set<WebContentsTreeNode*> ChildrenSet;

    void ConnectToOuterWebContents(WebContentsImpl* outer_web_contents,
                                   RenderFrameHostImpl* outer_contents_frame);

    WebContentsImpl* outer_web_contents() { return outer_web_contents_; }
    int outer_contents_frame_tree_node_id() {
      return outer_contents_frame_tree_node_id_;
    }

   private:
    // The outer WebContents.
    WebContentsImpl* outer_web_contents_;
    // The ID of the FrameTreeNode in outer WebContents that is hosting us.
    int outer_contents_frame_tree_node_id_;
    // List of inner WebContents that we host.
    ChildrenSet inner_web_contents_tree_nodes_;
  };

  // See WebContents::Create for a description of these parameters.
  WebContentsImpl(BrowserContext* browser_context);

  // Add and remove observers for page navigation notifications. The order in
  // which notifications are sent to observers is undefined. Clients must be
  // sure to remove the observer before they go away.
  void AddObserver(WebContentsObserver* observer);
  void RemoveObserver(WebContentsObserver* observer);

  // Clears a pending contents that has been closed before being shown.
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

  // Called with the result of a DownloadImage() request.
  void OnDidDownloadImage(const ImageDownloadCallback& callback,
                          int id,
                          const GURL& image_url,
                          int32_t http_status_code,
                          mojo::Array<SkBitmap> images,
                          mojo::Array<gfx::Size> original_image_sizes);

  // Callback function when showing JavaScript dialogs.  Takes in a routing ID
  // pair to identify the RenderFrameHost that opened the dialog, because it's
  // possible for the RenderFrameHost to be deleted by the time this is called.
  void OnDialogClosed(int render_process_id,
                      int render_frame_id,
                      IPC::Message* reply_msg,
                      bool dialog_was_suppressed,
                      bool success,
                      const base::string16& user_input);

  bool OnMessageReceived(RenderViewHost* render_view_host,
                         RenderFrameHost* render_frame_host,
                         const IPC::Message& message);

  // Checks whether render_frame_message_source_ is set to non-null value,
  // otherwise it terminates the main frame renderer process.
  bool HasValidFrameSource();

  // IPC message handlers.
  void OnThemeColorChanged(SkColor theme_color);
  void OnDidLoadResourceFromMemoryCache(const GURL& url,
                                        const std::string& security_info,
                                        const std::string& http_request,
                                        const std::string& mime_type,
                                        ResourceType resource_type);
  void OnDidDisplayInsecureContent();
  void OnDidRunInsecureContent(const GURL& security_origin,
                               const GURL& target_url);
  void OnDidDisplayContentWithCertificateErrors(
      const GURL& url,
      const std::string& security_info);
  void OnDidRunContentWithCertificateErrors(const GURL& url,
                                            const std::string& security_info);
  void OnDocumentLoadedInFrame();
  void OnDidFinishLoad(const GURL& url);
  void OnGoToEntryAtOffset(int offset);
  void OnUpdateZoomLimits(int minimum_percent,
                          int maximum_percent);
  void OnPageScaleFactorChanged(float page_scale_factor);
  void OnEnumerateDirectory(int request_id, const base::FilePath& path);

  void OnRegisterProtocolHandler(const std::string& protocol,
                                 const GURL& url,
                                 const base::string16& title,
                                 bool user_gesture);
  void OnUnregisterProtocolHandler(const std::string& protocol,
                                   const GURL& url,
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
  void OnGetNearestFindResultReply(int request_id, float distance);
  void OnOpenDateTimeDialog(
      const ViewHostMsg_DateTimeDialogValue_Params& value);
#endif
  void OnDomOperationResponse(const std::string& json_string);
  void OnAppCacheAccessed(const GURL& manifest_url, bool blocked_by_policy);
  void OnOpenColorChooser(int color_chooser_id,
                          SkColor color,
                          const std::vector<ColorSuggestion>& suggestions);
  void OnEndColorChooser(int color_chooser_id);
  void OnSetSelectedColorInColorChooser(int color_chooser_id, SkColor color);
  void OnWebUISend(const GURL& source_url,
                   const std::string& name,
                   const base::ListValue& args);
  void OnUpdatePageImportanceSignals(const PageImportanceSignals& signals);
#if defined(ENABLE_PLUGINS)
  void OnPepperInstanceCreated();
  void OnPepperInstanceDeleted();
  void OnPepperPluginHung(int plugin_child_id,
                          const base::FilePath& path,
                          bool is_hung);
  void OnPluginCrashed(const base::FilePath& plugin_path,
                       base::ProcessId plugin_pid);
  void OnRequestPpapiBrokerPermission(int routing_id,
                                      const GURL& url,
                                      const base::FilePath& plugin_path);

  // Callback function when requesting permission to access the PPAPI broker.
  // |result| is true if permission was granted.
  void OnPpapiBrokerPermissionResult(int routing_id, bool result);

  void OnBrowserPluginMessage(RenderFrameHost* render_frame_host,
                              const IPC::Message& message);
#endif  // defined(ENABLE_PLUGINS)
  void OnUpdateFaviconURL(const std::vector<FaviconURL>& candidates);
  void OnFirstVisuallyNonEmptyPaint();
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

  // Helper for CreateNewWidget/CreateNewFullscreenWidget.
  void CreateNewWidget(int32_t render_process_id,
                       int32_t route_id,
                       bool is_fullscreen,
                       blink::WebPopupType popup_type);

  // Helper for ShowCreatedWidget/ShowCreatedFullscreenWidget.
  void ShowCreatedWidget(int process_id,
                         int route_id,
                         bool is_fullscreen,
                         const gfx::Rect& initial_rect);

  // Finds the new RenderWidgetHost and returns it. Note that this can only be
  // called once as this call also removes it from the internal map.
  RenderWidgetHostView* GetCreatedWidget(int process_id, int route_id);

  // Finds the new WebContentsImpl by route_id, initializes it for
  // renderer-initiated creation, and returns it. Note that this can only be
  // called once as this call also removes it from the internal map.
  WebContentsImpl* GetCreatedWindow(int process_id, int route_id);

  // Sends a Page message IPC.
  void SendPageMessage(IPC::Message* msg);

  // Tracking loading progress -------------------------------------------------

  // Resets the tracking state of the current load progress.
  void ResetLoadProgressState();

  // Notifies the delegate that the load progress was updated.
  void SendChangeLoadProgress();

  // Notifies the delegate of a change in loading state.
  // |details| is used to provide details on the load that just finished
  // (but can be null if not applicable).
  // |due_to_interstitial| is true if the change in load state occurred because
  // an interstitial page started showing/proceeded.
  void LoadingStateChanged(bool to_different_document,
                           bool due_to_interstitial,
                           LoadNotificationDetails* details);

  // Misc non-view stuff -------------------------------------------------------

  // Sets the history for a specified RenderViewHost to |history_length|
  // entries, with an offset of |history_offset|.
  void SetHistoryOffsetAndLengthForView(RenderViewHost* render_view_host,
                                        int history_offset,
                                        int history_length);

  // Helper functions for sending notifications.
  void NotifyViewSwapped(RenderViewHost* old_host, RenderViewHost* new_host);
  void NotifyFrameSwapped(RenderFrameHost* old_host, RenderFrameHost* new_host);
  void NotifyDisconnected();

  void SetEncoding(const std::string& encoding);

  // TODO(creis): This should take in a FrameTreeNode to know which node's
  // render manager to return.  For now, we just return the root's.
  RenderFrameHostManager* GetRenderManager() const;

  // Removes browser plugin embedder if there is one.
  void RemoveBrowserPluginEmbedder();

  // Helper function to invoke WebContentsDelegate::GetSizeForNewRenderView().
  gfx::Size GetSizeForNewRenderView();

  void OnFrameRemoved(RenderFrameHost* render_frame_host);

  // Helper method that's called whenever |preferred_size_| or
  // |preferred_size_for_capture_| changes, to propagate the new value to the
  // |delegate_|.
  void OnPreferredSizeChanged(const gfx::Size& old_size);

  // Internal helper to create WebUI objects associated with |this|. |url| is
  // used to determine which WebUI should be created (if any). |frame_name|
  // corresponds to the name of a frame that the WebUI should be created for (or
  // the main frame if empty).
  WebUI* CreateWebUI(const GURL& url, const std::string& frame_name);

  void SetJavaScriptDialogManagerForTesting(
      JavaScriptDialogManager* dialog_manager);

  // Returns the outermost WebContents in this WebContents's tree.
  WebContentsImpl* GetOutermostWebContents();

  // Returns the FindRequestManager, or creates one if it doesn't already exist.
  FindRequestManager* GetOrCreateFindRequestManager();

  // Data for core operation ---------------------------------------------------

  // Delegate for notifying our owner about stuff. Not owned by us.
  WebContentsDelegate* delegate_;

  // Handles the back/forward list and loading.
  NavigationControllerImpl controller_;

  // The corresponding view.
  std::unique_ptr<WebContentsView> view_;

  // The view of the RVHD. Usually this is our WebContentsView implementation,
  // but if an embedder uses a different WebContentsView, they'll need to
  // provide this.
  RenderViewHostDelegateView* render_view_host_delegate_view_;

  // Tracks created WebContentsImpl objects that have not been shown yet. They
  // are identified by the process ID and routing ID passed to CreateNewWindow.
  typedef std::pair<int, int> ProcessRoutingIdPair;
  std::map<ProcessRoutingIdPair, WebContentsImpl*> pending_contents_;

  // This map holds widgets that were created on behalf of the renderer but
  // haven't been shown yet.
  std::map<ProcessRoutingIdPair, RenderWidgetHostView*> pending_widget_views_;

  typedef std::map<WebContentsImpl*, DestructionObserver*> DestructionObservers;
  DestructionObservers destruction_observers_;

  // A list of observers notified when page state changes. Weak references.
  // This MUST be listed above frame_tree_ since at destruction time the
  // latter might cause RenderViewHost's destructor to call us and we might use
  // the observer list then.
  base::ObserverList<WebContentsObserver> observers_;

  // True if this tab was opened by another tab. This is not unset if the opener
  // is closed.
  bool created_with_opener_;

  // Helper classes ------------------------------------------------------------

  // Manages the frame tree of the page and process swaps in each node.
  FrameTree frame_tree_;

  // If this WebContents is part of a "tree of WebContents", then this contains
  // information about the structure.
  std::unique_ptr<WebContentsTreeNode> node_;

  // SavePackage, lazily created.
  scoped_refptr<SavePackage> save_package_;

  // Manages/coordinates multi-process find-in-page requests. Created lazily.
  std::unique_ptr<FindRequestManager> find_request_manager_;

  // Data for loading state ----------------------------------------------------

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
  std::map<int32_t, int32_t> max_page_ids_;

  // The current load state and the URL associated with it.
  net::LoadStateWithParam load_state_;
  base::string16 load_state_host_;

  base::TimeTicks loading_last_progress_update_;

  // Upload progress, for displaying in the status bar.
  // Set to zero when there is no significant upload happening.
  uint64_t upload_size_;
  uint64_t upload_position_;

  // Tracks that this WebContents needs to unblock requests to the renderer.
  // See ResumeLoadingCreatedWebContents.
  bool is_resume_pending_;

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

  // The theme color for the underlying document as specified
  // by theme-color meta tag.
  SkColor theme_color_;

  // The last published theme color.
  SkColor last_sent_theme_color_;

  // Whether the first visually non-empty paint has occurred.
  bool did_first_visually_non_empty_paint_;

  // Data for misc internal state ----------------------------------------------

  // When > 0, the WebContents is currently being captured (e.g., for
  // screenshots or mirroring); and the underlying RenderWidgetHost should not
  // be told it is hidden.
  int capturer_count_;

  // Tracks whether RWHV should be visible once capturer_count_ becomes zero.
  bool should_normally_be_visible_;

  // Tracks whether this WebContents was ever set to be visible. Used to
  // facilitate WebContents being loaded in the background by setting
  // |should_normally_be_visible_|. Ensures WasShown() will trigger when first
  // becoming visible to the user, and prevents premature unloading.
  bool did_first_set_visible_;

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

  // Used to correctly handle integer zooming through a smooth scroll device.
  float zoom_scroll_remainder_;

  // The intrinsic size of the page.
  gfx::Size preferred_size_;

  // The preferred size for content screen capture.  When |capturer_count_| > 0,
  // this overrides |preferred_size_|.
  gfx::Size preferred_size_for_capture_;

#if defined(OS_ANDROID)
  // Date time chooser opened by this tab.
  // Only used in Android since all other platforms use a multi field UI.
  std::unique_ptr<DateTimeChooserAndroid> date_time_chooser_;
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
    std::unique_ptr<ColorChooser> chooser;

    // A unique identifier for the current color chooser.  Identifiers are
    // unique across a renderer process.  This avoids race conditions in
    // synchronizing the browser and renderer processes.  For example, if a
    // renderer closes one chooser and opens another, and simultaneously the
    // user picks a color in the first chooser, the IDs can be used to drop the
    // "chose a color" message rather than erroneously tell the renderer that
    // the user picked a color in the second chooser.
    int identifier;
  };

  std::unique_ptr<ColorChooserInfo> color_chooser_info_;

  // Manages the embedder state for browser plugins, if this WebContents is an
  // embedder; NULL otherwise.
  std::unique_ptr<BrowserPluginEmbedder> browser_plugin_embedder_;
  // Manages the guest state for browser plugin, if this WebContents is a guest;
  // NULL otherwise.
  std::unique_ptr<BrowserPluginGuest> browser_plugin_guest_;

#if defined(ENABLE_PLUGINS)
  // Manages the whitelist of plugin content origins exempt from power saving.
  std::unique_ptr<PluginContentOriginWhitelist>
      plugin_content_origin_whitelist_;
#endif

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

  // Process id of the shown fullscreen widget, or kInvalidUniqueID if there is
  // no fullscreen widget.
  int fullscreen_widget_process_id_;

  // Routing id of the shown fullscreen widget or MSG_ROUTING_NONE otherwise.
  int fullscreen_widget_routing_id_;

  // At the time the fullscreen widget was being shut down, did it have focus?
  // This is used to restore focus to the WebContentsView after both: 1) the
  // fullscreen widget is destroyed, and 2) the WebContentsDelegate has
  // completed making layout changes to effect an exit from fullscreen mode.
  bool fullscreen_widget_had_focus_at_shutdown_;

  // Whether this WebContents is responsible for displaying a subframe in a
  // different process from its parent page.
  bool is_subframe_;

  // When a new tab is created asynchronously, stores the OpenURLParams needed
  // to continue loading the page once the tab is ready.
  std::unique_ptr<OpenURLParams> delayed_open_url_params_;

  // Whether overscroll should be unconditionally disabled.
  bool force_disable_overscroll_content_;

  // Whether the last JavaScript dialog shown was suppressed. Used for testing.
  bool last_dialog_suppressed_;

  std::unique_ptr<GeolocationServiceContext> geolocation_service_context_;

  std::unique_ptr<WakeLockServiceContext> wake_lock_service_context_;

  std::unique_ptr<ScreenOrientationDispatcherHost>
      screen_orientation_dispatcher_host_;

  std::unique_ptr<ManifestManagerHost> manifest_manager_host_;

  // The accessibility mode for all frames. This is queried when each frame
  // is created, and broadcast to all frames when it changes.
  AccessibilityMode accessibility_mode_;

  // Monitors power levels for audio streams associated with this WebContents.
  AudioStreamMonitor audio_stream_monitor_;

  // Created on-demand to mute all audio output from this WebContents.
  std::unique_ptr<WebContentsAudioMuter> audio_muter_;

  size_t bluetooth_connected_device_count_;

  bool virtual_keyboard_requested_;

  // Notifies ResourceDispatcherHostImpl of various events related to loading.
  std::unique_ptr<LoaderIOThreadNotifier> loader_io_thread_notifier_;

  // Manages media players, CDMs, and power save blockers for media.
  std::unique_ptr<MediaWebContentsObserver> media_web_contents_observer_;

  std::unique_ptr<RenderWidgetHostInputEventRouter> rwh_input_event_router_;

  PageImportanceSignals page_importance_signals_;

  bool page_scale_factor_is_one_;

  // TextInputManager tracks the IME-related state for all the
  // RenderWidgetHostViews on this WebContents. Only exists on the outermost
  // WebContents and is automatically destroyed when a WebContents becomes an
  // inner WebContents by attaching to an outer WebContents. Then the
  // IME-related state for RenderWidgetHosts on the inner WebContents is tracked
  // by the TextInputManager in the outer WebContents.
  std::unique_ptr<TextInputManager> text_input_manager_;

  // Stores the RenderWidgetHost that currently holds a mouse lock or nullptr if
  // there's no RenderWidgetHost holding a lock.
  RenderWidgetHostImpl* mouse_lock_widget_;

  base::WeakPtrFactory<WebContentsImpl> loading_weak_factory_;
  base::WeakPtrFactory<WebContentsImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsImpl);
};

// Dangerous methods which should never be made part of the public API, so we
// grant their use only to an explicit friend list (c++ attorney/client idiom).
class CONTENT_EXPORT WebContentsImpl::FriendZone {
 private:
  friend class TestNavigationObserver;
  friend class WebContentsAddedObserver;
  friend class ContentBrowserSanityChecker;

  FriendZone();  // Not instantiable.

  // Adds/removes a callback called on creation of each new WebContents.
  static void AddCreatedCallbackForTesting(const CreatedCallback& callback);
  static void RemoveCreatedCallbackForTesting(const CreatedCallback& callback);

  DISALLOW_COPY_AND_ASSIGN(FriendZone);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_IMPL_H_
