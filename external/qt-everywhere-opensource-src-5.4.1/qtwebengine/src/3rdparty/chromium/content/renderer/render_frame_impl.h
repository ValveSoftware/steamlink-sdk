// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_FRAME_IMPL_H_
#define CONTENT_RENDERER_RENDER_FRAME_IMPL_H_

#include <vector>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/id_map.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/process/process_handle.h"
#include "content/public/common/javascript_message_type.h"
#include "content/public/common/referrer.h"
#include "content/public/renderer/render_frame.h"
#include "content/renderer/media/webmediaplayer_delegate.h"
#include "content/renderer/render_frame_proxy.h"
#include "content/renderer/renderer_webcookiejar_impl.h"
#include "ipc/ipc_message.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "third_party/WebKit/public/web/WebFrameClient.h"
#include "third_party/WebKit/public/web/WebHistoryCommitType.h"
#include "ui/gfx/range/range.h"

#if defined(OS_ANDROID)
#include "content/renderer/media/android/renderer_media_player_manager.h"
#endif

class TransportDIB;
struct FrameMsg_Navigate_Params;

namespace blink {
class WebGeolocationClient;
class WebInputEvent;
class WebMouseEvent;
class WebContentDecryptionModule;
class WebMediaPlayer;
class WebNotificationPresenter;
class WebSecurityOrigin;
struct WebCompositionUnderline;
struct WebContextMenuData;
struct WebCursorInfo;
}

namespace gfx {
class Point;
class Range;
class Rect;
}

namespace content {

class ChildFrameCompositingHelper;
class GeolocationDispatcher;
class MediaStreamRendererFactory;
class MidiDispatcher;
class NotificationProvider;
class PepperPluginInstanceImpl;
class RendererCdmManager;
class RendererMediaPlayerManager;
class RendererPpapiHost;
class RenderFrameObserver;
class RenderViewImpl;
class RenderWidget;
class RenderWidgetFullscreenPepper;
class ScreenOrientationDispatcher;
struct CustomContextMenuContext;

class CONTENT_EXPORT RenderFrameImpl
    : public RenderFrame,
      NON_EXPORTED_BASE(public blink::WebFrameClient),
      NON_EXPORTED_BASE(public WebMediaPlayerDelegate) {
 public:
  // Creates a new RenderFrame. |render_view| is the RenderView object that this
  // frame belongs to.
  // Callers *must* call |SetWebFrame| immediately after creation.
  // TODO(creis): We should structure this so that |SetWebFrame| isn't needed.
  static RenderFrameImpl* Create(RenderViewImpl* render_view, int32 routing_id);

  // Returns the RenderFrameImpl for the given routing ID.
  static RenderFrameImpl* FromRoutingID(int routing_id);

  // Just like RenderFrame::FromWebFrame but returns the implementation.
  static RenderFrameImpl* FromWebFrame(blink::WebFrame* web_frame);

  // Used by content_layouttest_support to hook into the creation of
  // RenderFrameImpls.
  static void InstallCreateHook(
      RenderFrameImpl* (*create_render_frame_impl)(RenderViewImpl*, int32));

  virtual ~RenderFrameImpl();

  bool is_swapped_out() const {
    return is_swapped_out_;
  }

  // TODO(nasko): This can be removed once we don't have a swapped out state on
  // RenderFrames. See https://crbug.com/357747.
  void set_render_frame_proxy(RenderFrameProxy* proxy) {
    render_frame_proxy_ = proxy;
  }

  // Out-of-process child frames receive a signal from RenderWidgetCompositor
  // when a compositor frame has committed.
  void DidCommitCompositorFrame();

  // TODO(jam): this is a temporary getter until all the code is transitioned
  // to using RenderFrame instead of RenderView.
  RenderViewImpl* render_view() { return render_view_.get(); }

  RendererWebCookieJarImpl* cookie_jar() { return &cookie_jar_; }

  // Returns the RenderWidget associated with this frame.
  RenderWidget* GetRenderWidget();

  // This is called right after creation with the WebLocalFrame for this
  // RenderFrame. It must be called before Initialize.
  void SetWebFrame(blink::WebLocalFrame* web_frame);

  // This method must be called after the frame has been added to the frame
  // tree. It creates all objects that depend on the frame being at its proper
  // spot.
  void Initialize();

  // Notification from RenderView.
  virtual void OnStop();

  // Notifications from RenderWidget.
  void WasHidden();
  void WasShown();

  // Start/Stop loading notifications.
  // TODO(nasko): Those are page-level methods at this time and come from
  // WebViewClient. We should move them to be WebFrameClient calls and put
  // logic in the browser side to balance starts/stops.
  // |to_different_document| will be true unless the load is a fragment
  // navigation, or triggered by history.pushState/replaceState.
  virtual void didStartLoading(bool to_different_document);
  virtual void didStopLoading();
  virtual void didChangeLoadProgress(double load_progress);

#if defined(ENABLE_PLUGINS)
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
 void OnImeConfirmComposition(
    const base::string16& text,
    const gfx::Range& replacement_range,
    bool keep_selection);
#endif  // ENABLE_PLUGINS

  // IPC::Sender
  virtual bool Send(IPC::Message* msg) OVERRIDE;

  // IPC::Listener
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

  // RenderFrame implementation:
  virtual RenderView* GetRenderView() OVERRIDE;
  virtual int GetRoutingID() OVERRIDE;
  virtual blink::WebFrame* GetWebFrame() OVERRIDE;
  virtual WebPreferences& GetWebkitPreferences() OVERRIDE;
  virtual int ShowContextMenu(ContextMenuClient* client,
                              const ContextMenuParams& params) OVERRIDE;
  virtual void CancelContextMenu(int request_id) OVERRIDE;
  virtual blink::WebNode GetContextMenuNode() const OVERRIDE;
  virtual blink::WebPlugin* CreatePlugin(
      blink::WebFrame* frame,
      const WebPluginInfo& info,
      const blink::WebPluginParams& params) OVERRIDE;
  virtual void LoadURLExternally(blink::WebLocalFrame* frame,
                                 const blink::WebURLRequest& request,
                                 blink::WebNavigationPolicy policy) OVERRIDE;
  virtual void ExecuteJavaScript(const base::string16& javascript) OVERRIDE;
  virtual bool IsHidden() OVERRIDE;

  // blink::WebFrameClient implementation:
  virtual blink::WebPlugin* createPlugin(blink::WebLocalFrame* frame,
                                         const blink::WebPluginParams& params);
  virtual blink::WebMediaPlayer* createMediaPlayer(
      blink::WebLocalFrame* frame,
      const blink::WebURL& url,
      blink::WebMediaPlayerClient* client);
  virtual blink::WebContentDecryptionModule* createContentDecryptionModule(
      blink::WebLocalFrame* frame,
      const blink::WebSecurityOrigin& security_origin,
      const blink::WebString& key_system);
  virtual blink::WebApplicationCacheHost* createApplicationCacheHost(
      blink::WebLocalFrame* frame,
      blink::WebApplicationCacheHostClient* client);
  virtual blink::WebWorkerPermissionClientProxy*
      createWorkerPermissionClientProxy(blink::WebLocalFrame* frame);
  virtual blink::WebCookieJar* cookieJar(blink::WebLocalFrame* frame);
  virtual blink::WebServiceWorkerProvider* createServiceWorkerProvider(
      blink::WebLocalFrame* frame);
  virtual void didAccessInitialDocument(blink::WebLocalFrame* frame);
  virtual blink::WebFrame* createChildFrame(blink::WebLocalFrame* parent,
                                            const blink::WebString& name);
  virtual void didDisownOpener(blink::WebLocalFrame* frame);
  virtual void frameDetached(blink::WebFrame* frame);
  virtual void frameFocused();
  virtual void willClose(blink::WebFrame* frame);
  virtual void didChangeName(blink::WebLocalFrame* frame,
                             const blink::WebString& name);
  virtual void didMatchCSS(
      blink::WebLocalFrame* frame,
      const blink::WebVector<blink::WebString>& newly_matching_selectors,
      const blink::WebVector<blink::WebString>& stopped_matching_selectors);
  virtual bool shouldReportDetailedMessageForSource(
      const blink::WebString& source);
  virtual void didAddMessageToConsole(const blink::WebConsoleMessage& message,
                                      const blink::WebString& source_name,
                                      unsigned source_line,
                                      const blink::WebString& stack_trace);
  virtual void loadURLExternally(blink::WebLocalFrame* frame,
                                 const blink::WebURLRequest& request,
                                 blink::WebNavigationPolicy policy,
                                 const blink::WebString& suggested_name);
  // The WebDataSource::ExtraData* is assumed to be a DocumentState* subclass.
  virtual blink::WebNavigationPolicy decidePolicyForNavigation(
      blink::WebLocalFrame* frame,
      blink::WebDataSource::ExtraData* extra_data,
      const blink::WebURLRequest& request,
      blink::WebNavigationType type,
      blink::WebNavigationPolicy default_policy,
      bool is_redirect);
  virtual blink::WebHistoryItem historyItemForNewChildFrame(
      blink::WebFrame* frame);
  virtual void willSendSubmitEvent(blink::WebLocalFrame* frame,
                                   const blink::WebFormElement& form);
  virtual void willSubmitForm(blink::WebLocalFrame* frame,
                              const blink::WebFormElement& form);
  virtual void didCreateDataSource(blink::WebLocalFrame* frame,
                                   blink::WebDataSource* datasource);
  virtual void didStartProvisionalLoad(blink::WebLocalFrame* frame);
  virtual void didReceiveServerRedirectForProvisionalLoad(
      blink::WebLocalFrame* frame);
  virtual void didFailProvisionalLoad(
      blink::WebLocalFrame* frame,
      const blink::WebURLError& error);
  virtual void didCommitProvisionalLoad(
      blink::WebLocalFrame* frame,
      const blink::WebHistoryItem& item,
      blink::WebHistoryCommitType commit_type);
  virtual void didClearWindowObject(blink::WebLocalFrame* frame);
  virtual void didCreateDocumentElement(blink::WebLocalFrame* frame);
  virtual void didReceiveTitle(blink::WebLocalFrame* frame,
                               const blink::WebString& title,
                               blink::WebTextDirection direction);
  virtual void didChangeIcon(blink::WebLocalFrame* frame,
                             blink::WebIconURL::Type icon_type);
  virtual void didFinishDocumentLoad(blink::WebLocalFrame* frame);
  virtual void didHandleOnloadEvents(blink::WebLocalFrame* frame);
  virtual void didFailLoad(blink::WebLocalFrame* frame,
                           const blink::WebURLError& error);
  virtual void didFinishLoad(blink::WebLocalFrame* frame);
  virtual void didNavigateWithinPage(blink::WebLocalFrame* frame,
                                     const blink::WebHistoryItem& item,
                                     blink::WebHistoryCommitType commit_type);
  virtual void didUpdateCurrentHistoryItem(blink::WebLocalFrame* frame);
  virtual void didChangeThemeColor();
  virtual blink::WebNotificationPresenter* notificationPresenter();
  virtual void didChangeSelection(bool is_empty_selection);
  virtual blink::WebColorChooser* createColorChooser(
      blink::WebColorChooserClient* client,
      const blink::WebColor& initial_color,
      const blink::WebVector<blink::WebColorSuggestion>& suggestions);
  virtual void runModalAlertDialog(const blink::WebString& message);
  virtual bool runModalConfirmDialog(const blink::WebString& message);
  virtual bool runModalPromptDialog(const blink::WebString& message,
                                    const blink::WebString& default_value,
                                    blink::WebString* actual_value);
  virtual bool runModalBeforeUnloadDialog(bool is_reload,
                                          const blink::WebString& message);
  virtual void showContextMenu(const blink::WebContextMenuData& data);
  virtual void clearContextMenu();
  virtual void willRequestAfterPreconnect(blink::WebLocalFrame* frame,
                                          blink::WebURLRequest& request);
  virtual void willSendRequest(blink::WebLocalFrame* frame,
                               unsigned identifier,
                               blink::WebURLRequest& request,
                               const blink::WebURLResponse& redirect_response);
  virtual void didReceiveResponse(blink::WebLocalFrame* frame,
                                  unsigned identifier,
                                  const blink::WebURLResponse& response);
  virtual void didFinishResourceLoad(blink::WebLocalFrame* frame,
                                     unsigned identifier);
  virtual void didLoadResourceFromMemoryCache(
      blink::WebLocalFrame* frame,
      const blink::WebURLRequest& request,
      const blink::WebURLResponse& response);
  virtual void didDisplayInsecureContent(blink::WebLocalFrame* frame);
  virtual void didRunInsecureContent(blink::WebLocalFrame* frame,
                                     const blink::WebSecurityOrigin& origin,
                                     const blink::WebURL& target);
  virtual void didAbortLoading(blink::WebLocalFrame* frame);
  virtual void didCreateScriptContext(blink::WebLocalFrame* frame,
                                      v8::Handle<v8::Context> context,
                                      int extension_group,
                                      int world_id);
  virtual void willReleaseScriptContext(blink::WebLocalFrame* frame,
                                        v8::Handle<v8::Context> context,
                                        int world_id);
  virtual void didFirstVisuallyNonEmptyLayout(blink::WebLocalFrame* frame);
  virtual void didChangeScrollOffset(blink::WebLocalFrame* frame);
  virtual void willInsertBody(blink::WebLocalFrame* frame);
  virtual void reportFindInPageMatchCount(int request_id,
                                          int count,
                                          bool final_update);
  virtual void reportFindInPageSelection(int request_id,
                                         int active_match_ordinal,
                                         const blink::WebRect& sel);
  virtual void requestStorageQuota(blink::WebLocalFrame* frame,
                                   blink::WebStorageQuotaType type,
                                   unsigned long long requested_size,
                                   blink::WebStorageQuotaCallbacks callbacks);
  virtual void willOpenSocketStream(
      blink::WebSocketStreamHandle* handle);
  virtual void willOpenWebSocket(blink::WebSocketHandle* handle);
  virtual blink::WebGeolocationClient* geolocationClient();
  virtual void willStartUsingPeerConnectionHandler(
      blink::WebLocalFrame* frame,
      blink::WebRTCPeerConnectionHandler* handler);
  virtual blink::WebUserMediaClient* userMediaClient();
  virtual blink::WebMIDIClient* webMIDIClient();
  virtual bool willCheckAndDispatchMessageEvent(
      blink::WebLocalFrame* source_frame,
      blink::WebFrame* target_frame,
      blink::WebSecurityOrigin target_origin,
      blink::WebDOMMessageEvent event);
  virtual blink::WebString userAgentOverride(blink::WebLocalFrame* frame,
                                             const blink::WebURL& url);
  virtual blink::WebString doNotTrackValue(blink::WebLocalFrame* frame);
  virtual bool allowWebGL(blink::WebLocalFrame* frame, bool default_value);
  virtual void didLoseWebGLContext(blink::WebLocalFrame* frame,
                                   int arb_robustness_status_code);
  virtual void forwardInputEvent(const blink::WebInputEvent* event);
  virtual void initializeChildFrame(const blink::WebRect& frame_rect,
                                    float scale_factor);
  virtual blink::WebScreenOrientationClient* webScreenOrientationClient();

  // WebMediaPlayerDelegate implementation:
  virtual void DidPlay(blink::WebMediaPlayer* player) OVERRIDE;
  virtual void DidPause(blink::WebMediaPlayer* player) OVERRIDE;
  virtual void PlayerGone(blink::WebMediaPlayer* player) OVERRIDE;

  // TODO(nasko): Make all tests in RenderViewImplTest friends and then move
  // this back to private member.
  void OnNavigate(const FrameMsg_Navigate_Params& params);

 protected:
  RenderFrameImpl(RenderViewImpl* render_view, int32 routing_id);

 private:
  friend class RenderFrameObserver;
  FRIEND_TEST_ALL_PREFIXES(RendererAccessibilityTest,
                           AccessibilityMessagesQueueWhileSwappedOut);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameImplTest,
                           ShouldUpdateSelectionTextFromContextMenuParams);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest,
                           OnExtendSelectionAndDelete);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, ReloadWhileSwappedOut);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, SendSwapOutACK);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest,
                           SetEditableSelectionAndComposition);

  typedef std::map<GURL, double> HostZoomLevels;

  // Functions to add and remove observers for this object.
  void AddObserver(RenderFrameObserver* observer);
  void RemoveObserver(RenderFrameObserver* observer);

  void UpdateURL(blink::WebFrame* frame);

  // Gets the focused element. If no such element exists then the element will
  // be NULL.
  blink::WebElement GetFocusedElement();

  // IPC message handlers ------------------------------------------------------
  //
  // The documentation for these functions should be in
  // content/common/*_messages.h for the message that the function is handling.
  void OnBeforeUnload();
  void OnSwapOut(int proxy_routing_id);
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
  void OnSelectRange(const gfx::Point& start, const gfx::Point& end);
  void OnUnselect();
  void OnReplace(const base::string16& text);
  void OnReplaceMisspelling(const base::string16& text);
  void OnCSSInsertRequest(const std::string& css);
  void OnJavaScriptExecuteRequest(const base::string16& javascript,
                                  int id,
                                  bool notify_result);
  void OnSetEditableSelectionOffsets(int start, int end);
  void OnSetCompositionFromExistingText(
      int start, int end,
      const std::vector<blink::WebCompositionUnderline>& underlines);
  void OnExtendSelectionAndDelete(int before, int after);
  void OnReload(bool ignore_cache);
  void OnTextSurroundingSelectionRequest(size_t max_length);
  void OnAddStyleSheetByURL(const std::string& url);
#if defined(OS_MACOSX)
  void OnCopyToFindPboard();
#endif

  // Virtual since overridden by WebTestProxy for layout tests.
  virtual blink::WebNavigationPolicy DecidePolicyForNavigation(
      RenderFrame* render_frame,
      blink::WebFrame* frame,
      blink::WebDataSource::ExtraData* extraData,
      const blink::WebURLRequest& request,
      blink::WebNavigationType type,
      blink::WebNavigationPolicy default_policy,
      bool is_redirect);
  void OpenURL(blink::WebFrame* frame,
               const GURL& url,
               const Referrer& referrer,
               blink::WebNavigationPolicy policy);

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
  void SyncSelectionIfRequired();

  // Returns whether |params.selection_text| should be synchronized to the
  // browser before bringing up the context menu. Static for testing.
  static bool ShouldUpdateSelectionTextFromContextMenuParams(
      const base::string16& selection_text,
      size_t selection_text_offset,
      const gfx::Range& selection_range,
      const ContextMenuParams& params);

  bool RunJavaScriptMessage(JavaScriptMessageType type,
                            const base::string16& message,
                            const base::string16& default_value,
                            const GURL& frame_url,
                            base::string16* result);

  // Loads the appropriate error page for the specified failure into the frame.
  void LoadNavigationErrorPage(const blink::WebURLRequest& failed_request,
                               const blink::WebURLError& error,
                               bool replace);

  // Initializes |web_user_media_client_|, returning true if successful. Returns
  // false if it wasn't possible to create a MediaStreamClient (e.g., WebRTC is
  // disabled) in which case |web_user_media_client_| is NULL.
  bool InitializeUserMediaClient();

  blink::WebMediaPlayer* CreateWebMediaPlayerForMediaStream(
      const blink::WebURL& url,
      blink::WebMediaPlayerClient* client);

  // Creates a factory object used for creating audio and video renderers.
  // The method is virtual so that layouttests can override it.
  virtual scoped_ptr<MediaStreamRendererFactory> CreateRendererFactory();

  // Returns the URL being loaded by the |frame_|'s request.
  GURL GetLoadingUrl() const;

#if defined(OS_ANDROID)
  blink::WebMediaPlayer* CreateAndroidWebMediaPlayer(
      const blink::WebURL& url,
      blink::WebMediaPlayerClient* client);

  RendererMediaPlayerManager* GetMediaPlayerManager();
#endif

#if defined(ENABLE_BROWSER_CDMS)
  RendererCdmManager* GetCdmManager();
#endif

  // Stores the WebLocalFrame we are associated with.
  blink::WebLocalFrame* frame_;

  base::WeakPtr<RenderViewImpl> render_view_;
  int routing_id_;
  bool is_swapped_out_;
  // RenderFrameProxy exists only when is_swapped_out_ is true.
  // TODO(nasko): This can be removed once we don't have a swapped out state on
  // RenderFrame. See https://crbug.com/357747.
  RenderFrameProxy* render_frame_proxy_;
  bool is_detaching_;

#if defined(ENABLE_PLUGINS)
  // Current text input composition text. Empty if no composition is in
  // progress.
  base::string16 pepper_composition_text_;
#endif

  RendererWebCookieJarImpl cookie_jar_;

  // All the registered observers.
  ObserverList<RenderFrameObserver> observers_;

  scoped_refptr<ChildFrameCompositingHelper> compositing_helper_;

  // The node that the context menu was pressed over.
  blink::WebNode context_menu_node_;

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

  // Holds a reference to the service which provides desktop notifications.
  NotificationProvider* notification_provider_;

  blink::WebUserMediaClient* web_user_media_client_;

  // MidiClient attached to this frame; lazily initialized.
  MidiDispatcher* midi_dispatcher_;

#if defined(OS_ANDROID)
  // Manages all media players in this render frame for communicating with the
  // real media player in the browser process. It's okay to use a raw pointer
  // since it's a RenderFrameObserver.
  RendererMediaPlayerManager* media_player_manager_;
#endif

#if defined(ENABLE_BROWSER_CDMS)
  // Manage all CDMs in this render frame for communicating with the real CDM in
  // the browser process. It's okay to use a raw pointer since it's a
  // RenderFrameObserver.
  RendererCdmManager* cdm_manager_;
#endif

  // The geolocation dispatcher attached to this view, lazily initialized.
  GeolocationDispatcher* geolocation_dispatcher_;

  // The screen orientation dispatcher attached to the view, lazily initialized.
  ScreenOrientationDispatcher* screen_orientation_dispatcher_;

  base::WeakPtrFactory<RenderFrameImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_FRAME_IMPL_H_
