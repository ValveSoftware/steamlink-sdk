// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_FRAME_IMPL_H_
#define CONTENT_RENDERER_RENDER_FRAME_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/id_map.h"
#include "base/macros.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/process/process_handle.h"
#include "build/build_config.h"
#include "content/common/accessibility_mode_enums.h"
#include "content/common/frame.mojom.h"
#include "content/common/frame_message_enums.h"
#include "content/public/common/console_message_level.h"
#include "content/public/common/javascript_message_type.h"
#include "content/public/common/referrer.h"
#include "content/public/common/stop_find_action.h"
#include "content/public/renderer/render_frame.h"
#include "content/renderer/frame_blame_context.h"
#include "content/renderer/mojo/blink_service_registry_impl.h"
#include "content/renderer/renderer_webcookiejar_impl.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_platform_file.h"
#include "media/blink/webmediaplayer_delegate.h"
#include "media/blink/webmediaplayer_params.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/shell/public/interfaces/connector.mojom.h"
#include "services/shell/public/interfaces/interface_provider.mojom.h"
#include "third_party/WebKit/public/platform/WebEffectiveConnectionType.h"
#include "third_party/WebKit/public/platform/WebFocusType.h"
#include "third_party/WebKit/public/platform/WebLoadingBehaviorFlag.h"
#include "third_party/WebKit/public/platform/WebMediaPlayer.h"
#include "third_party/WebKit/public/platform/WebPageVisibilityState.h"
#include "third_party/WebKit/public/platform/modules/app_banner/WebAppBannerClient.h"
#include "third_party/WebKit/public/web/WebAXObject.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "third_party/WebKit/public/web/WebFrameClient.h"
#include "third_party/WebKit/public/web/WebFrameLoadType.h"
#include "third_party/WebKit/public/web/WebFrameOwnerProperties.h"
#include "third_party/WebKit/public/web/WebFrameSerializerClient.h"
#include "third_party/WebKit/public/web/WebHistoryCommitType.h"
#include "third_party/WebKit/public/web/WebMeaningfulLayout.h"
#include "third_party/WebKit/public/web/WebScriptExecutionCallback.h"
#include "ui/gfx/range/range.h"
#include "url/gurl.h"

#if defined(ENABLE_PLUGINS)
#include "content/renderer/pepper/plugin_power_saver_helper.h"
#endif

#if defined(OS_ANDROID)
#include "content/renderer/media/android/renderer_media_player_manager.h"
#endif

#if defined(ENABLE_MOJO_MEDIA)
#include "media/mojo/interfaces/service_factory.mojom.h"  // nogncheck
#endif

class TransportDIB;
struct FrameMsg_NewFrame_WidgetParams;
struct FrameMsg_PostMessage_Params;
struct FrameMsg_SerializeAsMHTML_Params;
struct FrameMsg_TextTrackSettings_Params;

namespace IPC {
class SyncMessage;
}

namespace blink {
class WebMouseEvent;
class WebContentDecryptionModule;
class WebPresentationClient;
class WebPushClient;
class WebSecurityOrigin;
enum class WebCachePolicy;
struct WebCompositionUnderline;
struct WebContextMenuData;
struct WebCursorInfo;
struct WebFindOptions;
struct WebScreenInfo;
}

namespace gfx {
class Point;
class Range;
class Rect;
}

namespace media {
class CdmFactory;
class DecoderFactory;
class MediaPermission;
class MediaServiceProvider;
class RendererWebMediaPlayerDelegate;
class SurfaceManager;
class UrlIndex;
class WebEncryptedMediaClientImpl;
}

namespace shell {
class InterfaceRegistry;
class InterfaceProvider;
}

namespace url {
class Origin;
}

namespace content {

class ChildFrameCompositingHelper;
class CompositorDependencies;
class DevToolsAgent;
class DocumentState;
class ExternalPopupMenu;
class ManifestManager;
class MediaInterfaceProvider;
class MediaStreamDispatcher;
class MediaStreamRendererFactory;
class MediaPermissionDispatcher;
class MidiDispatcher;
class NavigationState;
class NotificationPermissionDispatcher;
class PageState;
class PepperPluginInstanceImpl;
class PermissionDispatcher;
class PresentationDispatcher;
class PushMessagingDispatcher;
class RenderAccessibilityImpl;
class RendererCdmManager;
class RendererMediaPlayerManager;
class RendererMediaSessionManager;
class RendererPpapiHost;
class RendererSurfaceViewManager;
class RenderFrameObserver;
class RenderViewImpl;
class RenderWidget;
class RenderWidgetFullscreenPepper;
class ResourceRequestBodyImpl;
class ScreenOrientationDispatcher;
class UserMediaClientImpl;
class WakeLockDispatcher;
struct CommonNavigationParams;
struct CustomContextMenuContext;
struct FileChooserFileInfo;
struct FileChooserParams;
struct FrameReplicationState;
struct NavigationParams;
struct RequestNavigationParams;
struct ResourceResponseHead;
struct StartNavigationParams;
struct StreamOverrideParameters;

class CONTENT_EXPORT RenderFrameImpl
    : public RenderFrame,
      NON_EXPORTED_BASE(mojom::Frame),
      NON_EXPORTED_BASE(public blink::WebFrameClient),
      NON_EXPORTED_BASE(public blink::WebFrameSerializerClient) {
 public:
  // Creates a new RenderFrame as the main frame of |render_view|.
  static RenderFrameImpl* CreateMainFrame(
      RenderViewImpl* render_view,
      int32_t routing_id,
      int32_t widget_routing_id,
      bool hidden,
      const blink::WebScreenInfo& screen_info,
      CompositorDependencies* compositor_deps,
      blink::WebFrame* opener);

  // Creates a new RenderFrame with |routing_id|.  If |proxy_routing_id| is
  // MSG_ROUTING_NONE, it creates the Blink WebLocalFrame and inserts it into
  // the frame tree after the frame identified by
  // |previous_sibling_routing_id|, or as the first child if
  // |previous_sibling_routing_id| is MSG_ROUTING_NONE. Otherwise, the frame is
  // semi-orphaned until it commits, at which point it replaces the proxy
  // identified by |proxy_routing_id|.  The frame's opener is set to the frame
  // identified by |opener_routing_id|.  The frame is created as a child of the
  // RenderFrame identified by |parent_routing_id| or as the top-level frame if
  // the latter is MSG_ROUTING_NONE.  Note: This is called only when
  // RenderFrame is being created in response to IPC message from the browser
  // process. All other frame creation is driven through Blink and Create.
  static void CreateFrame(
      int routing_id,
      int proxy_routing_id,
      int opener_routing_id,
      int parent_routing_id,
      int previous_sibling_routing_id,
      const FrameReplicationState& replicated_state,
      CompositorDependencies* compositor_deps,
      const FrameMsg_NewFrame_WidgetParams& params,
      const blink::WebFrameOwnerProperties& frameOwner_properties);

  // Returns the RenderFrameImpl for the given routing ID.
  static RenderFrameImpl* FromRoutingID(int routing_id);

  // Just like RenderFrame::FromWebFrame but returns the implementation.
  static RenderFrameImpl* FromWebFrame(blink::WebFrame* web_frame);

  // Used by content_layouttest_support to hook into the creation of
  // RenderFrameImpls.
  struct CreateParams {
    CreateParams(RenderViewImpl* render_view, int32_t routing_id)
        : render_view(render_view), routing_id(routing_id) {}
    ~CreateParams() {}

    RenderViewImpl* render_view;
    int32_t routing_id;
  };

  using CreateRenderFrameImplFunction =
      RenderFrameImpl* (*)(const CreateParams&);
  static void InstallCreateHook(
      CreateRenderFrameImplFunction create_render_frame_impl);

  // Looks up and returns the WebFrame corresponding to a given opener frame
  // routing ID.  Also stores the opener's RenderView routing ID into
  // |opener_view_routing_id|.
  //
  // TODO(alexmos): remove RenderViewImpl's dependency on
  // opener_view_routing_id.
  static blink::WebFrame* ResolveOpener(int opener_frame_routing_id,
                                        int* opener_view_routing_id);

  ~RenderFrameImpl() override;

  // Called by RenderWidget when meaningful layout has happened.
  // See RenderFrameObserver::DidMeaningfulLayout declaration for details.
  void DidMeaningfulLayout(blink::WebMeaningfulLayout layout_type);

  // Out-of-process child frames receive a signal from RenderWidgetCompositor
  // when a compositor frame has committed.
  void DidCommitCompositorFrame();

  // Draw commands have been issued by RenderWidgetCompositor.
  void DidCommitAndDrawCompositorFrame();

  // TODO(jam): this is a temporary getter until all the code is transitioned
  // to using RenderFrame instead of RenderView.
  RenderViewImpl* render_view() { return render_view_.get(); }

  const blink::WebHistoryItem& current_history_item() {
    return current_history_item_;
  }

  RendererWebCookieJarImpl* cookie_jar() { return &cookie_jar_; }

  // Returns the RenderWidget associated with this frame.
  RenderWidget* GetRenderWidget();

  DevToolsAgent* devtools_agent() { return devtools_agent_; }

  // This method must be called after the frame has been added to the frame
  // tree. It creates all objects that depend on the frame being at its proper
  // spot.
  void Initialize();

  // Notifications from RenderWidget.
  void WasHidden();
  void WasShown();
  void WidgetWillClose();

  // Start/Stop loading notifications.
  // TODO(nasko): Those are page-level methods at this time and come from
  // WebViewClient. We should move them to be WebFrameClient calls and put
  // logic in the browser side to balance starts/stops.
  // |to_different_document| will be true unless the load is a fragment
  // navigation, or triggered by history.pushState/replaceState.
  void didStartLoading(bool to_different_document) override;
  void didStopLoading() override;
  void didChangeLoadProgress(double load_progress) override;

  AccessibilityMode accessibility_mode() {
    return accessibility_mode_;
  }

  RenderAccessibilityImpl* render_accessibility() {
    return render_accessibility_;
  }

  void HandleWebAccessibilityEvent(const blink::WebAXObject& obj,
                                   blink::WebAXEvent event);

  // The focused node changed to |node|. If focus was lost from this frame,
  // |node| will be null.
  void FocusedNodeChanged(const blink::WebNode& node);

  // TODO(dmazzoni): the only reason this is here is to plumb it through to
  // RenderAccessibilityImpl. It should use the RenderFrameObserver method, once
  // blink has a separate accessibility tree per frame.
  void FocusedNodeChangedForAccessibility(const blink::WebNode& node);

  // Called when this frame's widget is focused.
  void RenderWidgetSetFocus(bool enable);

  // Called when the widget receives a mouse event.
  void RenderWidgetWillHandleMouseEvent();

#if defined(ENABLE_PLUGINS)
  // Get/set the plugin which will be used to handle document find requests.
  void set_plugin_find_handler(PepperPluginInstanceImpl* plugin) {
    plugin_find_handler_ = plugin;
  }
  PepperPluginInstanceImpl* plugin_find_handler() {
    return plugin_find_handler_;
  }

  // Notification that a PPAPI plugin has been created.
  void PepperPluginCreated(RendererPpapiHost* host);

  // Notifies that |instance| has changed the cursor.
  // This will update the cursor appearance if it is currently over the plugin
  // instance.
  void PepperDidChangeCursor(PepperPluginInstanceImpl* instance,
                             const blink::WebCursorInfo& cursor);

  // Notifies that |instance| has received a mouse event.
  void PepperDidReceiveMouseEvent(PepperPluginInstanceImpl* instance);

  // Informs the render view that a PPAPI plugin has changed text input status.
  void PepperTextInputTypeChanged(PepperPluginInstanceImpl* instance);
  void PepperCaretPositionChanged(PepperPluginInstanceImpl* instance);

  // Cancels current composition.
  void PepperCancelComposition(PepperPluginInstanceImpl* instance);

  // Informs the render view that a PPAPI plugin has changed selection.
  void PepperSelectionChanged(PepperPluginInstanceImpl* instance);

  // Creates a fullscreen container for a pepper plugin instance.
  RenderWidgetFullscreenPepper* CreatePepperFullscreenContainer(
      PepperPluginInstanceImpl* plugin);

  bool IsPepperAcceptingCompositionEvents() const;

  // Notification that the given plugin has crashed.
  void PluginCrashed(const base::FilePath& plugin_path,
                     base::ProcessId plugin_pid);

  // Simulates IME events for testing purpose.
  void SimulateImeSetComposition(
      const base::string16& text,
      const std::vector<blink::WebCompositionUnderline>& underlines,
      int selection_start,
      int selection_end);
  void SimulateImeConfirmComposition(const base::string16& text,
                                     const gfx::Range& replacement_range);

  // TODO(jam): remove these once the IPC handler moves from RenderView to
  // RenderFrame.
  void OnImeSetComposition(
      const base::string16& text,
      const std::vector<blink::WebCompositionUnderline>& underlines,
      int selection_start,
      int selection_end);
  void OnImeConfirmComposition(const base::string16& text,
                               const gfx::Range& replacement_range,
                               bool keep_selection);
#endif  // defined(ENABLE_PLUGINS)

  // May return NULL in some cases, especially if userMediaClient() returns
  // NULL.
  MediaStreamDispatcher* GetMediaStreamDispatcher();

#if defined(USE_EXTERNAL_POPUP_MENU)
  void DidHideExternalPopupMenu();
#endif

  // IPC::Sender
  bool Send(IPC::Message* msg) override;

  // IPC::Listener
  bool OnMessageReceived(const IPC::Message& msg) override;

  // RenderFrame implementation:
  RenderView* GetRenderView() override;
  RenderAccessibility* GetRenderAccessibility() override;
  int GetRoutingID() override;
  blink::WebLocalFrame* GetWebFrame() override;
  WebPreferences& GetWebkitPreferences() override;
  int ShowContextMenu(ContextMenuClient* client,
                      const ContextMenuParams& params) override;
  void CancelContextMenu(int request_id) override;
  blink::WebPlugin* CreatePlugin(
      blink::WebFrame* frame,
      const WebPluginInfo& info,
      const blink::WebPluginParams& params,
      std::unique_ptr<PluginInstanceThrottler> throttler) override;
  void LoadURLExternally(const blink::WebURLRequest& request,
                         blink::WebNavigationPolicy policy) override;
  void ExecuteJavaScript(const base::string16& javascript) override;
  bool IsMainFrame() override;
  bool IsHidden() override;
  shell::InterfaceRegistry* GetInterfaceRegistry() override;
  shell::InterfaceProvider* GetRemoteInterfaces() override;
#if defined(ENABLE_PLUGINS)
  void RegisterPeripheralPlugin(
      const url::Origin& content_origin,
      const base::Closure& unthrottle_callback) override;
  RenderFrame::PeripheralContentStatus GetPeripheralContentStatus(
      const url::Origin& main_frame_origin,
      const url::Origin& content_origin,
      const gfx::Size& unobscured_size) const override;
  void WhitelistContentOrigin(const url::Origin& content_origin) override;
#endif
  bool IsFTPDirectoryListing() override;
  void AttachGuest(int element_instance_id) override;
  void DetachGuest(int element_instance_id) override;
  void SetSelectedText(const base::string16& selection_text,
                       size_t offset,
                       const gfx::Range& range) override;
  void EnsureMojoBuiltinsAreAvailable(v8::Isolate* isolate,
                                      v8::Local<v8::Context> context) override;
  void AddMessageToConsole(ConsoleMessageLevel level,
                           const std::string& message) override;
  bool IsUsingLoFi() const override;
  bool IsPasting() const override;
  blink::WebPageVisibilityState GetVisibilityState() const override;

  // mojom::Frame implementation:
  void GetInterfaceProvider(
      shell::mojom::InterfaceProviderRequest request) override;

  // blink::WebFrameClient implementation:
  blink::WebPlugin* createPlugin(blink::WebLocalFrame* frame,
                                 const blink::WebPluginParams& params) override;
  blink::WebMediaPlayer* createMediaPlayer(
      const blink::WebMediaPlayerSource& source,
      blink::WebMediaPlayerClient* client,
      blink::WebMediaPlayerEncryptedMediaClient* encrypted_client,
      blink::WebContentDecryptionModule* initial_cdm,
      const blink::WebString& sink_id,
      blink::WebMediaSession* media_session) override;
  blink::WebMediaSession* createMediaSession() override;
  blink::WebApplicationCacheHost* createApplicationCacheHost(
      blink::WebApplicationCacheHostClient* client) override;
  blink::WebWorkerContentSettingsClientProxy*
  createWorkerContentSettingsClientProxy() override;
  blink::WebExternalPopupMenu* createExternalPopupMenu(
      const blink::WebPopupMenuInfo& popup_menu_info,
      blink::WebExternalPopupMenuClient* popup_menu_client) override;
  blink::WebCookieJar* cookieJar() override;
  blink::BlameContext* frameBlameContext() override;
  blink::WebServiceWorkerProvider* createServiceWorkerProvider() override;
  void didAccessInitialDocument() override;
  blink::WebFrame* createChildFrame(
      blink::WebLocalFrame* parent,
      blink::WebTreeScopeType scope,
      const blink::WebString& name,
      const blink::WebString& unique_name,
      blink::WebSandboxFlags sandbox_flags,
      const blink::WebFrameOwnerProperties& frame_owner_properties) override;
  void didChangeOpener(blink::WebFrame* frame) override;
  void frameDetached(blink::WebFrame* frame, DetachType type) override;
  void frameFocused() override;
  void willClose(blink::WebFrame* frame) override;
  void didChangeName(const blink::WebString& name,
                     const blink::WebString& unique_name) override;
  void didEnforceInsecureRequestPolicy(
      blink::WebInsecureRequestPolicy policy) override;
  void didUpdateToUniqueOrigin(
      bool is_potentially_trustworthy_unique_origin) override;
  void didChangeSandboxFlags(blink::WebFrame* child_frame,
                             blink::WebSandboxFlags flags) override;
  void didAddContentSecurityPolicy(
      const blink::WebString& header_value,
      blink::WebContentSecurityPolicyType type,
      blink::WebContentSecurityPolicySource source) override;
  void didChangeFrameOwnerProperties(
      blink::WebFrame* child_frame,
      const blink::WebFrameOwnerProperties& frame_owner_properties) override;
  void didMatchCSS(
      blink::WebLocalFrame* frame,
      const blink::WebVector<blink::WebString>& newly_matching_selectors,
      const blink::WebVector<blink::WebString>& stopped_matching_selectors)
      override;
  bool shouldReportDetailedMessageForSource(
      const blink::WebString& source) override;
  void didAddMessageToConsole(const blink::WebConsoleMessage& message,
                              const blink::WebString& source_name,
                              unsigned source_line,
                              const blink::WebString& stack_trace) override;
  void loadURLExternally(const blink::WebURLRequest& request,
                         blink::WebNavigationPolicy policy,
                         const blink::WebString& suggested_name,
                         bool should_replace_current_entry) override;
  blink::WebNavigationPolicy decidePolicyForNavigation(
      const NavigationPolicyInfo& info) override;
  blink::WebHistoryItem historyItemForNewChildFrame() override;
  void willSendSubmitEvent(const blink::WebFormElement& form) override;
  void willSubmitForm(const blink::WebFormElement& form) override;
  void didCreateDataSource(blink::WebLocalFrame* frame,
                           blink::WebDataSource* datasource) override;
  void didStartProvisionalLoad(blink::WebLocalFrame* frame,
                               double triggering_event_time) override;
  void didReceiveServerRedirectForProvisionalLoad(
      blink::WebLocalFrame* frame) override;
  void didFailProvisionalLoad(blink::WebLocalFrame* frame,
                              const blink::WebURLError& error,
                              blink::WebHistoryCommitType commit_type) override;
  void didCommitProvisionalLoad(
      blink::WebLocalFrame* frame,
      const blink::WebHistoryItem& item,
      blink::WebHistoryCommitType commit_type) override;
  void didCreateNewDocument(blink::WebLocalFrame* frame) override;
  void didClearWindowObject(blink::WebLocalFrame* frame) override;
  void didCreateDocumentElement(blink::WebLocalFrame* frame) override;
  void runScriptsAtDocumentElementAvailable(
      blink::WebLocalFrame* frame) override;
  void didReceiveTitle(blink::WebLocalFrame* frame,
                       const blink::WebString& title,
                       blink::WebTextDirection direction) override;
  void didChangeIcon(blink::WebLocalFrame* frame,
                     blink::WebIconURL::Type icon_type) override;
  void didFinishDocumentLoad(blink::WebLocalFrame* frame) override;
  void runScriptsAtDocumentReady(blink::WebLocalFrame* frame,
                                 bool document_is_empty) override;
  void didHandleOnloadEvents(blink::WebLocalFrame* frame) override;
  void didFailLoad(blink::WebLocalFrame* frame,
                   const blink::WebURLError& error,
                   blink::WebHistoryCommitType commit_type) override;
  void didFinishLoad(blink::WebLocalFrame* frame) override;
  void didNavigateWithinPage(blink::WebLocalFrame* frame,
                             const blink::WebHistoryItem& item,
                             blink::WebHistoryCommitType commit_type,
                             bool content_initiated) override;
  void didUpdateCurrentHistoryItem() override;
  void didChangeThemeColor() override;
  void dispatchLoad() override;
  blink::WebEffectiveConnectionType getEffectiveConnectionType() override;
  void requestNotificationPermission(
      const blink::WebSecurityOrigin& origin,
      blink::WebNotificationPermissionCallback* callback) override;
  void didChangeSelection(bool is_empty_selection) override;
  blink::WebColorChooser* createColorChooser(
      blink::WebColorChooserClient* client,
      const blink::WebColor& initial_color,
      const blink::WebVector<blink::WebColorSuggestion>& suggestions) override;
  void runModalAlertDialog(const blink::WebString& message) override;
  bool runModalConfirmDialog(const blink::WebString& message) override;
  bool runModalPromptDialog(const blink::WebString& message,
                            const blink::WebString& default_value,
                            blink::WebString* actual_value) override;
  bool runModalBeforeUnloadDialog(bool is_reload) override;
  bool runFileChooser(
      const blink::WebFileChooserParams& params,
      blink::WebFileChooserCompletion* chooser_completion) override;
  void showContextMenu(const blink::WebContextMenuData& data) override;
  void saveImageFromDataURL(const blink::WebString& data_url) override;
  void willSendRequest(blink::WebLocalFrame* frame,
                       unsigned identifier,
                       blink::WebURLRequest& request,
                       const blink::WebURLResponse& redirect_response) override;
  void didReceiveResponse(unsigned identifier,
                          const blink::WebURLResponse& response) override;
  void didLoadResourceFromMemoryCache(
      const blink::WebURLRequest& request,
      const blink::WebURLResponse& response) override;
  void didDisplayInsecureContent() override;
  void didRunInsecureContent(const blink::WebSecurityOrigin& origin,
                             const blink::WebURL& target) override;
  void didDisplayContentWithCertificateErrors(
      const blink::WebURL& url,
      const blink::WebCString& security_info) override;
  void didRunContentWithCertificateErrors(
      const blink::WebURL& url,
      const blink::WebCString& security_info) override;
  void didChangePerformanceTiming() override;
  void didObserveLoadingBehavior(
      blink::WebLoadingBehaviorFlag behavior) override;
  void didCreateScriptContext(blink::WebLocalFrame* frame,
                              v8::Local<v8::Context> context,
                              int extension_group,
                              int world_id) override;
  void willReleaseScriptContext(blink::WebLocalFrame* frame,
                                v8::Local<v8::Context> context,
                                int world_id) override;
  void didChangeScrollOffset(blink::WebLocalFrame* frame) override;
  void willInsertBody(blink::WebLocalFrame* frame) override;
  void reportFindInPageMatchCount(int request_id,
                                  int count,
                                  bool final_update) override;
  void reportFindInPageSelection(int request_id,
                                 int active_match_ordinal,
                                 const blink::WebRect& sel) override;
  void requestStorageQuota(blink::WebStorageQuotaType type,
                           unsigned long long requested_size,
                           blink::WebStorageQuotaCallbacks callbacks) override;
  void willOpenWebSocket(blink::WebSocketHandle* handle) override;
  blink::WebPushClient* pushClient() override;
  blink::WebPresentationClient* presentationClient() override;
  void willStartUsingPeerConnectionHandler(
      blink::WebRTCPeerConnectionHandler* handler) override;
  blink::WebUserMediaClient* userMediaClient() override;
  blink::WebEncryptedMediaClient* encryptedMediaClient() override;
  blink::WebMIDIClient* webMIDIClient() override;
  blink::WebString userAgentOverride() override;
  blink::WebString doNotTrackValue() override;
  bool allowWebGL(bool default_value) override;
  blink::WebScreenOrientationClient* webScreenOrientationClient() override;
  bool isControlledByServiceWorker(blink::WebDataSource& data_source) override;
  int64_t serviceWorkerID(blink::WebDataSource& data_source) override;
  void postAccessibilityEvent(const blink::WebAXObject& obj,
                              blink::WebAXEvent event) override;
  void handleAccessibilityFindInPageResult(
      int identifier,
      int match_index,
      const blink::WebAXObject& start_object,
      int start_offset,
      const blink::WebAXObject& end_object,
      int end_offset) override;
  void didChangeManifest() override;
  bool enterFullscreen() override;
  bool exitFullscreen() override;
  blink::WebPermissionClient* permissionClient() override;
  blink::WebAppBannerClient* appBannerClient() override;
  void registerProtocolHandler(const blink::WebString& scheme,
                               const blink::WebURL& url,
                               const blink::WebString& title) override;
  void unregisterProtocolHandler(const blink::WebString& scheme,
                                 const blink::WebURL& url) override;
  blink::WebBluetooth* bluetooth() override;
  void checkIfAudioSinkExistsAndIsAuthorized(
      const blink::WebString& sink_id,
      const blink::WebSecurityOrigin& security_origin,
      blink::WebSetSinkIdCallbacks* web_callbacks) override;
  blink::ServiceRegistry* serviceRegistry() override;
  blink::WebPageVisibilityState visibilityState() const override;

  // WebFrameSerializerClient implementation:
  void didSerializeDataForFrame(
      const blink::WebCString& data,
      blink::WebFrameSerializerClient::FrameSerializationStatus status)
      override;

  // Binds to the FrameHost in the browser.
  void Bind(mojom::FrameRequest frame, mojom::FrameHostPtr frame_host);

  ManifestManager* manifest_manager();

  // TODO(creis): Remove when the only caller, the HistoryController, is no
  // more.
  void SetPendingNavigationParams(
      std::unique_ptr<NavigationParams> navigation_params);

  media::MediaPermission* GetMediaPermission();

  // Sends the current frame's navigation state to the browser.
  void SendUpdateState();

  // Creates a MojoBindingsController if Mojo bindings have been enabled for
  // this frame. For WebUI, this allows the page to communicate with the browser
  // process; for layout tests, this allows the test to mock out services at
  // the Mojo IPC layer.
  void MaybeEnableMojoBindings();

  // Plugin-related functions --------------------------------------------------

#if defined(ENABLE_PLUGINS)
  PepperPluginInstanceImpl* focused_pepper_plugin() {
    return focused_pepper_plugin_;
  }
  PepperPluginInstanceImpl* pepper_last_mouse_event_target() {
    return pepper_last_mouse_event_target_;
  }
  void set_pepper_last_mouse_event_target(PepperPluginInstanceImpl* plugin) {
    pepper_last_mouse_event_target_ = plugin;
  }

  // Indicates that the given instance has been created.
  void PepperInstanceCreated(PepperPluginInstanceImpl* instance);

  // Indicates that the given instance is being destroyed. This is called from
  // the destructor, so it's important that the instance is not dereferenced
  // from this call.
  void PepperInstanceDeleted(PepperPluginInstanceImpl* instance);

  // Notification that the given plugin is focused or unfocused.
  void PepperFocusChanged(PepperPluginInstanceImpl* instance, bool focused);

  void PepperStartsPlayback(PepperPluginInstanceImpl* instance);
  void PepperStopsPlayback(PepperPluginInstanceImpl* instance);
  void OnSetPepperVolume(int32_t pp_instance, double volume);
#endif  // ENABLE_PLUGINS

 protected:
  explicit RenderFrameImpl(const CreateParams& params);

 private:
  friend class RenderFrameImplTest;
  friend class RenderFrameObserver;
  friend class RenderAccessibilityImplTest;
  friend class TestRenderFrame;
  FRIEND_TEST_ALL_PREFIXES(ExternalPopupMenuDisplayNoneTest, SelectItem);
  FRIEND_TEST_ALL_PREFIXES(ExternalPopupMenuRemoveTest, RemoveOnChange);
  FRIEND_TEST_ALL_PREFIXES(ExternalPopupMenuTest, NormalCase);
  FRIEND_TEST_ALL_PREFIXES(ExternalPopupMenuTest, ShowPopupThenNavigate);
  FRIEND_TEST_ALL_PREFIXES(RenderAccessibilityImplTest,
                           AccessibilityMessagesQueueWhileSwappedOut);

  // A wrapper class used as the callback for JavaScript executed
  // in an isolated world.
  class JavaScriptIsolatedWorldRequest
      : public blink::WebScriptExecutionCallback {
   public:
    JavaScriptIsolatedWorldRequest(
        int id,
        bool notify_result,
        int routing_id,
        base::WeakPtr<RenderFrameImpl> render_frame_impl);
    void completed(
        const blink::WebVector<v8::Local<v8::Value>>& result) override;

   private:
    ~JavaScriptIsolatedWorldRequest() override;

    int id_;
    bool notify_result_;
    int routing_id_;
    base::WeakPtr<RenderFrameImpl> render_frame_impl_;

    DISALLOW_COPY_AND_ASSIGN(JavaScriptIsolatedWorldRequest);
  };

  typedef std::map<GURL, double> HostZoomLevels;

  // Creates a new RenderFrame. |render_view| is the RenderView object that this
  // frame belongs to.
  // Callers *must* call |BindToWebFrame| immediately after creation.
  static RenderFrameImpl* Create(RenderViewImpl* render_view,
                                 int32_t routing_id);

  // This is called right after creation with the WebLocalFrame for this
  // RenderFrame. It must be called before Initialize.
  void BindToWebFrame(blink::WebLocalFrame* web_frame);

  // Functions to add and remove observers for this object.
  void AddObserver(RenderFrameObserver* observer);
  void RemoveObserver(RenderFrameObserver* observer);

  bool IsLocalRoot() const;

  // Builds and sends DidCommitProvisionalLoad to the host.
  void SendDidCommitProvisionalLoad(blink::WebFrame* frame,
                                    blink::WebHistoryCommitType commit_type,
                                    const blink::WebHistoryItem& item);

  // IPC message handlers ------------------------------------------------------
  //
  // The documentation for these functions should be in
  // content/common/*_messages.h for the message that the function is handling.
  void OnNavigate(const CommonNavigationParams& common_params,
                  const StartNavigationParams& start_params,
                  const RequestNavigationParams& request_params);
  void OnBeforeUnload(bool is_reload);
  void OnSwapOut(int proxy_routing_id,
                 bool is_loading,
                 const FrameReplicationState& replicated_frame_state);
  void OnDeleteFrame();
  void OnStop();
  void OnShowContextMenu(const gfx::Point& location);
  void OnContextMenuClosed(const CustomContextMenuContext& custom_context);
  void OnCustomContextMenuAction(const CustomContextMenuContext& custom_context,
                                 unsigned action);
  void OnUndo();
  void OnRedo();
  void OnCut();
  void OnCopy();
  void OnPaste();
  void OnPasteAndMatchStyle();
  void OnDelete();
  void OnSelectAll();
  void OnSelectRange(const gfx::Point& base, const gfx::Point& extent);
  void OnAdjustSelectionByCharacterOffset(int start_adjust, int end_adjust);
  void OnUnselect();
  void OnMoveRangeSelectionExtent(const gfx::Point& point);
  void OnReplace(const base::string16& text);
  void OnReplaceMisspelling(const base::string16& text);
  void OnCopyImageAt(int x, int y);
  void OnSaveImageAt(int x, int y);
  void OnCSSInsertRequest(const std::string& css);
  void OnAddMessageToConsole(ConsoleMessageLevel level,
                             const std::string& message);
  void OnJavaScriptExecuteRequest(const base::string16& javascript,
                                  int id,
                                  bool notify_result);
  void OnJavaScriptExecuteRequestForTests(const base::string16& javascript,
                                          int id,
                                          bool notify_result,
                                          bool has_user_gesture);
  void OnJavaScriptExecuteRequestInIsolatedWorld(const base::string16& jscript,
                                                 int id,
                                                 bool notify_result,
                                                 int world_id);
  void OnVisualStateRequest(uint64_t key);
  void OnSetEditableSelectionOffsets(int start, int end);
  void OnSetCompositionFromExistingText(
      int start, int end,
      const std::vector<blink::WebCompositionUnderline>& underlines);
  void OnExecuteNoValueEditCommand(const std::string& name);
  void OnExtendSelectionAndDelete(int before, int after);
  void OnReload(bool bypass_cache);
  void OnReloadLoFiImages();
  void OnTextSurroundingSelectionRequest(uint32_t max_length);
  void OnSetAccessibilityMode(AccessibilityMode new_mode);
  void OnSnapshotAccessibilityTree(int callback_id);
  void OnUpdateOpener(int opener_routing_id);
  void OnDidUpdateSandboxFlags(blink::WebSandboxFlags flags);
  void OnSetFrameOwnerProperties(
      const blink::WebFrameOwnerProperties& frame_owner_properties);
  void OnAdvanceFocus(blink::WebFocusType type, int32_t source_routing_id);
  void OnSetFocusedFrame();
  void OnTextTrackSettingsChanged(
      const FrameMsg_TextTrackSettings_Params& params);
  void OnPostMessageEvent(const FrameMsg_PostMessage_Params& params);
  void OnCommitNavigation(const ResourceResponseHead& response,
                          const GURL& stream_url,
                          const CommonNavigationParams& common_params,
                          const RequestNavigationParams& request_params);
  void OnFailedNavigation(const CommonNavigationParams& common_params,
                          const RequestNavigationParams& request_params,
                          bool has_stale_copy_in_cache,
                          int error_code);
  void OnGetSavableResourceLinks();
  void OnGetSerializedHtmlWithLocalLinks(
      const std::map<GURL, base::FilePath>& url_to_local_path,
      const std::map<int, base::FilePath>& frame_routing_id_to_local_path);
  void OnSerializeAsMHTML(const FrameMsg_SerializeAsMHTML_Params& params);
  void OnFind(int request_id,
              const base::string16& search_text,
              const blink::WebFindOptions& options);
  void OnClearActiveFindMatch();
  void OnStopFinding(StopFindAction action);
  void OnEnableViewSourceMode();
  void OnSuppressFurtherDialogs();
  void OnFileChooserResponse(
      const std::vector<content::FileChooserFileInfo>& files);
#if defined(OS_ANDROID)
  void OnActivateNearestFindResult(int request_id, float x, float y);
  void OnGetNearestFindResult(int request_id, float x, float y);
  void OnFindMatchRects(int current_version);
#endif

#if defined(USE_EXTERNAL_POPUP_MENU)
#if defined(OS_MACOSX)
  void OnSelectPopupMenuItem(int selected_index);
#else
  void OnSelectPopupMenuItems(bool canceled,
                              const std::vector<int>& selected_indices);
#endif
#endif

#if defined(OS_MACOSX)
  void OnCopyToFindPboard();
#endif

  // Requests that the browser process navigates to |url|. If
  // |is_history_navigation_in_new_child| is true, the browser process should
  // look for a matching FrameNavigationEntry in the last committed entry to use
  // instead of |url|.
  void OpenURL(
      const GURL& url,
      bool uses_post,
      const scoped_refptr<ResourceRequestBodyImpl>& resource_request_body,
      const Referrer& referrer,
      blink::WebNavigationPolicy policy,
      bool should_replace_current_entry,
      bool is_history_navigation_in_new_child);

  // Performs a navigation in the frame. This provides a unified function for
  // the current code path and the browser-side navigation path (in
  // development). Currently used by OnNavigate, with all *NavigationParams
  // provided by the browser. |stream_params| should be null.
  // PlzNavigate: used by OnCommitNavigation, with |common_params| and
  // |request_params| received by the browser. |stream_params| should be non
  // null and created from the information provided by the browser.
  // |start_params| is not used.
  void NavigateInternal(
      const CommonNavigationParams& common_params,
      const StartNavigationParams& start_params,
      const RequestNavigationParams& request_params,
      std::unique_ptr<StreamOverrideParameters> stream_params);

  // Update current main frame's encoding and send it to browser window.
  // Since we want to let users see the right encoding info from menu
  // before finishing loading, we call the UpdateEncoding in
  // a) function:DidCommitLoadForFrame. When this function is called,
  // that means we have got first data. In here we try to get encoding
  // of page if it has been specified in http header.
  // b) function:DidReceiveTitle. When this function is called,
  // that means we have got specified title. Because in most of webpages,
  // title tags will follow meta tags. In here we try to get encoding of
  // page if it has been specified in meta tag.
  // c) function:DidFinishDocumentLoadForFrame. When this function is
  // called, that means we have got whole html page. In here we should
  // finally get right encoding of page.
  void UpdateEncoding(blink::WebFrame* frame,
                      const std::string& encoding_name);

  // Dispatches the current state of selection on the webpage to the browser if
  // it has changed.
  // TODO(varunjain): delete this method once we figure out how to keep
  // selection handles in sync with the webpage.
  void SyncSelectionIfRequired(bool is_empty_selection);

  bool RunJavaScriptMessage(JavaScriptMessageType type,
                            const base::string16& message,
                            const base::string16& default_value,
                            const GURL& frame_url,
                            base::string16* result);

  // Adds the given file chooser request to the file_chooser_completion_ queue
  // (see that var for more) and requests the chooser be displayed if there are
  // no other waiting items in the queue.
  //
  // Returns true if the chooser was successfully scheduled. False means we
  // didn't schedule anything.
  bool ScheduleFileChooser(const FileChooserParams& params,
                           blink::WebFileChooserCompletion* completion);

  // Loads the appropriate error page for the specified failure into the frame.
  void LoadNavigationErrorPage(const blink::WebURLRequest& failed_request,
                               const blink::WebURLError& error,
                               bool replace);

  void HandleJavascriptExecutionResult(const base::string16& javascript,
                                       int id,
                                       bool notify_result,
                                       v8::Local<v8::Value> result);

  // Initializes |web_user_media_client_|. If this fails, because it wasn't
  // possible to create a MediaStreamClient (e.g., WebRTC is disabled), then
  // |web_user_media_client_| will remain NULL.
  void InitializeUserMediaClient();

  blink::WebMediaPlayer* CreateWebMediaPlayerForMediaStream(
      blink::WebMediaPlayerClient* client,
      const blink::WebString& sink_id,
      const blink::WebSecurityOrigin& security_origin);

  // Creates a factory object used for creating audio and video renderers.
  std::unique_ptr<MediaStreamRendererFactory> CreateRendererFactory();

  // Does preparation for the navigation to |url|.
  void PrepareRenderViewForNavigation(
      const GURL& url,
      const RequestNavigationParams& request_params);

  // PlzNavigate
  // Sends a FrameHostMsg_BeginNavigation to the browser based on the contents
  // of the WebURLRequest.
  // |should_replace_current_entry| indicates that the current history entry
  // will be replaced when the navigation commits.  |is_client_redirect|
  // indicates that the navigation originated as a client redirect.
  // Note: those two concepts are separate. For example, a location.assign is a
  // client redirect but it only replaces the current entry if called before
  // load stop. location.replace on the other hand is a client redirect and
  // always replaces the current entry.
  void BeginNavigation(blink::WebURLRequest* request,
                       bool should_replace_current_entry,
                       bool is_client_redirect);

  // Loads a data url.
  void LoadDataURL(const CommonNavigationParams& params,
                   const RequestNavigationParams& request_params,
                   blink::WebLocalFrame* frame,
                   blink::WebFrameLoadType load_type,
                   blink::WebHistoryItem item_for_history_navigation,
                   blink::WebHistoryLoadType history_load_type,
                   bool is_client_redirect);

  // Sends a proper FrameHostMsg_DidFailProvisionalLoadWithError_Params IPC for
  // the failed request |request|.
  void SendFailedProvisionalLoad(const blink::WebURLRequest& request,
                                 const blink::WebURLError& error,
                                 blink::WebLocalFrame* frame);

  bool ShouldDisplayErrorPageForFailedLoad(int error_code,
                                           const GURL& unreachable_url);

  // Returns the URL being loaded by the |frame_|'s request.
  GURL GetLoadingUrl() const;

  // If we initiated a navigation, this function will populate |document_state|
  // with the navigation information saved in OnNavigate().
  void PopulateDocumentStateFromPending(DocumentState* document_state);

  // Returns a new NavigationState populated with the navigation information
  // saved in OnNavigate().
  NavigationState* CreateNavigationStateFromPending();

  // Sets the NavigationState on the DocumentState based on
  // the value of |pending_navigation_params_|.
  void UpdateNavigationState(DocumentState* document_state,
                             bool was_within_same_page,
                             bool content_initiated);

#if defined(OS_ANDROID)
  blink::WebMediaPlayer* CreateAndroidWebMediaPlayer(
      blink::WebMediaPlayerClient* client,
      blink::WebMediaPlayerEncryptedMediaClient* encrypted_client,
      const media::WebMediaPlayerParams& params);

  RendererMediaPlayerManager* GetMediaPlayerManager();

  RendererMediaSessionManager* GetMediaSessionManager();
#endif

  bool AreSecureCodecsSupported();

#if defined(ENABLE_MOJO_MEDIA)
  shell::mojom::InterfaceProvider* GetMediaInterfaceProvider();
#endif

  media::CdmFactory* GetCdmFactory();
  media::DecoderFactory* GetDecoderFactory();

  void RegisterMojoInterfaces();

  // Connect to an interface provided by the service registry.
  template <typename Interface>
  void GetInterface(mojo::InterfaceRequest<Interface> request);

  // Connects to a Mojo application and returns a proxy to its exposed
  // ServiceProvider.
  shell::mojom::InterfaceProviderPtr ConnectToApplication(const GURL& url);

  // Returns the media delegate for WebMediaPlayer usage.  If
  // |media_player_delegate_| is NULL, one is created.
  media::RendererWebMediaPlayerDelegate* GetWebMediaPlayerDelegate();

  // Called to get the WebPlugin to handle find requests in the document.
  // Returns nullptr if there is no such WebPlugin.
  blink::WebPlugin* GetWebPluginForFind();

  // Sends a reply to the current find operation handling if it was a
  // synchronous find request.
  void SendFindReply(int request_id,
                     int match_count,
                     int ordinal,
                     const blink::WebRect& selection_rect,
                     bool final_status_update);

  void InitializeBlameContext(RenderFrameImpl* parent_frame);

  // Stores the WebLocalFrame we are associated with.  This is null from the
  // constructor until BindToWebFrame is called, and it is null after
  // frameDetached is called until destruction (which is asynchronous in the
  // case of the main frame, but not subframes).
  blink::WebLocalFrame* frame_;

  // Boolean value indicating whether this RenderFrameImpl object is for the
  // main frame or not. It remains accurate during destruction, even when
  // |frame_| has been invalidated.
  bool is_main_frame_;

  // When a frame is detached in response to a message from the browser process,
  // this RenderFrame should not be sending notifications back to it. This
  // boolean is used to indicate this case.
  bool in_browser_initiated_detach_;

  // Indicates whether the frame has been inserted into the frame tree yet or
  // not.
  //
  // When a frame is created by the browser process, it is for a pending
  // navigation. In this case, it is not immediately attached to the frame tree
  // if there is a RenderFrameProxy for the same frame. It is inserted into the
  // frame tree at the time the pending navigation commits.
  // Frames added by the parent document are created from the renderer process
  // and are immediately inserted in the frame tree.
  // TODO(dcheng): Remove this once we have FrameTreeHandle and can use the
  // Blink Web* layer to check for provisional frames.
  bool in_frame_tree_;

  base::WeakPtr<RenderViewImpl> render_view_;
  int routing_id_;

  // If this frame was created to replace a proxy, this will store the routing
  // id of the proxy to replace at commit-time, at which time it will be
  // cleared.
  // TODO(creis): Remove this after switching to PlzNavigate.
  int proxy_routing_id_;

  // Non-null when the RenderFrame is a local root for compositing, input,
  // layout, etc. A local frame is also a local root iff it does not have a
  // parent that is a local frame.
  scoped_refptr<RenderWidget> render_widget_;

  // Temporarily holds state pertaining to a navigation that has been initiated
  // until the NavigationState corresponding to the new navigation is created in
  // didCreateDataSource().
  std::unique_ptr<NavigationParams> pending_navigation_params_;

  // Stores the current history item for this frame, so that updates to it can
  // be reported to the browser process via SendUpdateState.
  blink::WebHistoryItem current_history_item_;

#if defined(ENABLE_PLUGINS)
  // Current text input composition text. Empty if no composition is in
  // progress.
  base::string16 pepper_composition_text_;

  PluginPowerSaverHelper* plugin_power_saver_helper_;

  PepperPluginInstanceImpl* plugin_find_handler_;
#endif

  RendererWebCookieJarImpl cookie_jar_;

  // All the registered observers.
  base::ObserverList<RenderFrameObserver> observers_;

  scoped_refptr<ChildFrameCompositingHelper> compositing_helper_;

  // External context menu requests we're waiting for. "Internal"
  // (WebKit-originated) context menu events will have an ID of 0 and will not
  // be in this map.
  //
  // We don't want to add internal ones since some of the "special" page
  // handlers in the browser process just ignore the context menu requests so
  // avoid showing context menus, and so this will cause right clicks to leak
  // entries in this map. Most users of the custom context menu (e.g. Pepper
  // plugins) are normally only on "regular" pages and the regular pages will
  // always respond properly to the request, so we don't have to worry so
  // much about leaks.
  IDMap<ContextMenuClient, IDMapExternalPointer> pending_context_menus_;

  // The text selection the last time DidChangeSelection got called. May contain
  // additional characters before and after the selected text, for IMEs. The
  // portion of this string that is the actual selected text starts at index
  // |selection_range_.GetMin() - selection_text_offset_| and has length
  // |selection_range_.length()|.
  base::string16 selection_text_;
  // The offset corresponding to the start of |selection_text_| in the document.
  size_t selection_text_offset_;
  // Range over the document corresponding to the actual selected text (which
  // could correspond to a substring of |selection_text_|; see above).
  gfx::Range selection_range_;
  // Used to inform didChangeSelection() when it is called in the context
  // of handling a InputMsg_SelectRange IPC.
  bool handling_select_range_;

  // The next group of objects all implement RenderFrameObserver, so are deleted
  // along with the RenderFrame automatically.  This is why we just store weak
  // references.

  // Dispatches permission requests for Web Notifications.
  NotificationPermissionDispatcher* notification_permission_dispatcher_;

  // Destroyed via the RenderFrameObserver::OnDestruct() mechanism.
  UserMediaClientImpl* web_user_media_client_;

  // EncryptedMediaClient attached to this frame; lazily initialized.
  std::unique_ptr<media::WebEncryptedMediaClientImpl>
      web_encrypted_media_client_;

  // The media permission dispatcher attached to this frame.
  std::unique_ptr<MediaPermissionDispatcher> media_permission_dispatcher_;

#if defined(ENABLE_MOJO_MEDIA)
  // The media interface provider attached to this frame, lazily initialized.
  std::unique_ptr<MediaInterfaceProvider> media_interface_provider_;
#endif

  // MidiClient attached to this frame; lazily initialized.
  MidiDispatcher* midi_dispatcher_;

#if defined(OS_ANDROID)
  // Manages all media players and sessions in this render frame for
  // communicating with the real media player and sessions in the
  // browser process. It's okay to use raw pointers since they're both
  // RenderFrameObservers.
  RendererMediaPlayerManager* media_player_manager_;
  RendererMediaSessionManager* media_session_manager_;
#endif

  media::SurfaceManager* media_surface_manager_;

#if defined(ENABLE_BROWSER_CDMS)
  // Manage all CDMs in this render frame for communicating with the real CDM in
  // the browser process. It's okay to use a raw pointer since it's a
  // RenderFrameObserver.
  RendererCdmManager* cdm_manager_;
#endif

  // The CDM and decoder factory attached to this frame, lazily initialized.
  std::unique_ptr<media::CdmFactory> cdm_factory_;
  std::unique_ptr<media::DecoderFactory> decoder_factory_;

  // Media resource cache, lazily initialized.
  linked_ptr<media::UrlIndex> url_index_;

#if defined(VIDEO_HOLE)
  // Whether or not this RenderFrameImpl contains a media player. Used to
  // register as an observer for video-hole-specific events.
  bool contains_media_player_;
#endif

  // The devtools agent for this frame; only created for main frame and
  // local roots.
  DevToolsAgent* devtools_agent_;

  // The push messaging dispatcher attached to this frame, lazily initialized.
  PushMessagingDispatcher* push_messaging_dispatcher_;

  // The presentation dispatcher implementation attached to this frame, lazily
  // initialized.
  PresentationDispatcher* presentation_dispatcher_;

  std::unique_ptr<shell::InterfaceRegistry> interface_registry_;
  std::unique_ptr<shell::InterfaceProvider> remote_interfaces_;
  std::unique_ptr<BlinkServiceRegistryImpl> blink_service_registry_;
  shell::mojom::InterfaceProviderRequest
      pending_remote_interface_provider_request_;

  // The shell proxy used to connect to Mojo applications.
  shell::mojom::ConnectorPtr connector_;

  // The screen orientation dispatcher attached to the frame, lazily
  // initialized.
  ScreenOrientationDispatcher* screen_orientation_dispatcher_;

  // The Manifest Manager handles the manifest requests from the browser
  // process.
  ManifestManager* manifest_manager_;

  // The current accessibility mode.
  AccessibilityMode accessibility_mode_;

  // Only valid if |accessibility_mode_| is anything other than
  // AccessibilityModeOff.
  RenderAccessibilityImpl* render_accessibility_;

  std::unique_ptr<PermissionDispatcher> permission_client_;

  std::unique_ptr<blink::WebAppBannerClient> app_banner_client_;

  std::unique_ptr<blink::WebBluetooth> bluetooth_;

  // Manages play, pause notifications for WebMediaPlayer implementations; its
  // lifetime is tied to the RenderFrame via the RenderFrameObserver interface.
  media::RendererWebMediaPlayerDelegate* media_player_delegate_;

  // Whether or not this RenderFrame is using Lo-Fi mode.
  bool is_using_lofi_;

  // Effective connection type when the document of this frame was fetched.
  blink::WebEffectiveConnectionType effective_connection_type_;

  // Whether or not this RenderFrame is currently pasting.
  bool is_pasting_;

  // Whether we must stop creating nested message loops for modal dialogs. This
  // is necessary because modal dialogs have a ScopedPageLoadDeferrer on the
  // stack that interferes with swapping out.
  bool suppress_further_dialogs_;

  // The current and pending file chooser completion objects. If the queue is
  // nonempty, the first item represents the currently running file chooser
  // callback, and the remaining elements are the other file chooser completion
  // still waiting to be run (in order).
  struct PendingFileChooser;
  std::deque<std::unique_ptr<PendingFileChooser>> file_chooser_completions_;

#if defined(USE_EXTERNAL_POPUP_MENU)
  // The external popup for the currently showing select popup.
  std::unique_ptr<ExternalPopupMenu> external_popup_menu_;
#endif

  std::unique_ptr<FrameBlameContext> blame_context_;

  // Plugins -------------------------------------------------------------------
#if defined(ENABLE_PLUGINS)
  typedef std::set<PepperPluginInstanceImpl*> PepperPluginSet;
  PepperPluginSet active_pepper_instances_;

  // Whether or not the focus is on a PPAPI plugin
  PepperPluginInstanceImpl* focused_pepper_plugin_;

  // The plugin instance that received the last mouse event. It is set to NULL
  // if the last mouse event went to elements other than Pepper plugins.
  // |pepper_last_mouse_event_target_| is not owned by this class. We depend on
  // the RenderFrameImpl to NULL it out when it destructs.
  PepperPluginInstanceImpl* pepper_last_mouse_event_target_;
#endif

  mojo::Binding<mojom::Frame> frame_binding_;
  mojom::FrameHostPtr frame_host_;

  base::WeakPtrFactory<RenderFrameImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_FRAME_IMPL_H_
