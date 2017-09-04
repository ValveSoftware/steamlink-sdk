// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_VIEW_IMPL_H_
#define CONTENT_RENDERER_RENDER_VIEW_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <deque>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/id_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/process/process.h"
#include "base/strings/string16.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "cc/input/browser_controls_state.h"
#include "cc/resources/shared_bitmap.h"
#include "content/common/content_export.h"
#include "content/common/frame_message_enums.h"
#include "content/common/navigation_gesture.h"
#include "content/common/page_message_enums.h"
#include "content/common/view_message_enums.h"
#include "content/public/common/browser_controls_state.h"
#include "content/public/common/drop_data.h"
#include "content/public/common/page_zoom.h"
#include "content/public/common/referrer.h"
#include "content/public/common/renderer_preferences.h"
#include "content/public/common/web_preferences.h"
#include "content/public/renderer/render_view.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_widget.h"
#include "content/renderer/render_widget_owner_delegate.h"
#include "content/renderer/stats_collection_observer.h"
#include "ipc/ipc_platform_file.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/web/WebAXObject.h"
#include "third_party/WebKit/public/web/WebConsoleMessage.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebFrameWidget.h"
#include "third_party/WebKit/public/web/WebHistoryItem.h"
#include "third_party/WebKit/public/web/WebIconURL.h"
#include "third_party/WebKit/public/web/WebNavigationType.h"
#include "third_party/WebKit/public/web/WebNode.h"
#include "third_party/WebKit/public/web/WebViewClient.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/surface/transport_dib.h"

#if defined(OS_ANDROID)
#include "content/renderer/android/content_detector.h"
#include "content/renderer/android/renderer_date_time_picker.h"
#endif

#if defined(COMPILER_MSVC)
// RenderViewImpl is a diamond-shaped hierarchy, with WebWidgetClient at the
// root. VS warns when we inherit the WebWidgetClient method implementations
// from RenderWidget.  It's safe to ignore that warning.
#pragma warning(disable: 4250)
#endif

namespace blink {
class WebApplicationCacheHost;
class WebDataSource;
class WebDateTimeChooserCompletion;
class WebGestureEvent;
class WebIconURL;
class WebImage;
class WebMouseEvent;
class WebSpeechRecognizer;
class WebStorageNamespace;
class WebURLRequest;
struct WebActiveWheelFlingParameters;
struct WebDateTimeChooserParams;
struct WebMediaPlayerAction;
struct WebPluginAction;
struct WebPoint;
struct WebWindowFeatures;

#if defined(OS_ANDROID)
class WebHitTestResult;
#endif
}  // namespace blink

namespace content {

class HistoryController;
class RenderViewImplTest;
class RenderViewObserver;
class RenderViewTest;
class RendererDateTimePicker;
class SpeechRecognitionDispatcher;
struct FaviconURL;
struct FileChooserParams;
struct ResizeParams;

namespace mojom {
class CreateViewParams;
}

//
// RenderView is an object that manages a WebView object, and provides a
// communication interface with an embedding application process.
//
// DEPRECATED: RenderViewImpl is being removed as part of the SiteIsolation
// project. New code should be added to RenderFrameImpl instead.
//
// For context, please see https://crbug.com/467770 and
// http://www.chromium.org/developers/design-documents/site-isolation.
class CONTENT_EXPORT RenderViewImpl
    : public RenderWidget,
      NON_EXPORTED_BASE(public blink::WebViewClient),
      public RenderWidgetOwnerDelegate,
      public RenderView,
      public base::SupportsWeakPtr<RenderViewImpl> {
 public:
  // Creates a new RenderView. |opener_id| is the routing ID of the RenderView
  // responsible for creating this RenderView. Note that if the original opener
  // has been closed, |window_was_created_with_opener| will be true and
  // |opener_id| will be MSG_ROUTING_NONE. When |swapped_out| is true, the
  // |proxy_routing_id| is specified, so a RenderFrameProxy can be created for
  // this RenderView's main RenderFrame.
  static RenderViewImpl* Create(CompositorDependencies* compositor_deps,
                                const mojom::CreateViewParams& params,
                                bool was_created_by_renderer);

  // Used by content_layouttest_support to hook into the creation of
  // RenderViewImpls.
  static void InstallCreateHook(RenderViewImpl* (*create_render_view_impl)(
      CompositorDependencies* compositor_deps,
      const mojom::CreateViewParams&));

  // Returns the RenderViewImpl containing the given WebView.
  static RenderViewImpl* FromWebView(blink::WebView* webview);

  // Returns the RenderViewImpl for the given routing ID.
  static RenderViewImpl* FromRoutingID(int routing_id);

  // May return NULL when the view is closing.
  blink::WebView* webview() const;

  // Returns the RenderWidget for this RenderView.
  RenderWidget* GetWidget() const;

  const WebPreferences& webkit_preferences() const {
    return webkit_preferences_;
  }

  const RendererPreferences& renderer_preferences() const {
    return renderer_preferences_;
  }

  void set_send_content_state_immediately(bool value) {
    send_content_state_immediately_ = value;
  }

  HistoryController* history_controller() {
    return history_controller_.get();
  }

  // Functions to add and remove observers for this object.
  void AddObserver(RenderViewObserver* observer);
  void RemoveObserver(RenderViewObserver* observer);

  // Returns the StatsCollectionObserver associated with this view, or NULL
  // if one wasn't created;
  StatsCollectionObserver* GetStatsCollectionObserver() {
    return stats_collection_observer_.get();
  }

  // Adds the given file chooser request to the file_chooser_completion_ queue
  // (see that var for more) and requests the chooser be displayed if there are
  // no other waiting items in the queue.
  //
  // Returns true if the chooser was successfully scheduled. False means we
  // didn't schedule anything.
  bool ScheduleFileChooser(const FileChooserParams& params,
                           blink::WebFileChooserCompletion* completion);

#if defined(OS_ANDROID)
  void DismissDateTimeDialog();
#endif

  bool is_loading() const { return frames_in_progress_ != 0; }

  void FrameDidStartLoading(blink::WebFrame* frame);
  void FrameDidStopLoading(blink::WebFrame* frame);

  // Sets the zoom level and notifies observers. Doesn't call zoomLevelChanged,
  // as that is only for changes that aren't initiated by the client.
  void SetZoomLevel(double zoom_level);

  double page_zoom_level() {
    return page_zoom_level_;
  }

  // Sets page-level focus in this view and notifies plugins and Blink's
  // FocusController.
  void SetFocus(bool enable);

  void AttachWebFrameWidget(blink::WebFrameWidget* frame_widget);

  void TransferActiveWheelFlingAnimation(
      const blink::WebActiveWheelFlingParameters& params);

  // Starts a timer to send an UpdateState message on behalf of |frame|, if the
  // timer isn't already running. This allows multiple state changing events to
  // be coalesced into one update.
  void StartNavStateSyncTimerIfNecessary(RenderFrameImpl* frame);

  // Synchronously sends the current navigation state to the browser.
  void SendUpdateState();

  // Returns the length of the session history of this RenderView. Note that
  // this only coincides with the actual length of the session history if this
  // RenderView is the currently active RenderView of a WebContents.
  unsigned GetLocalSessionHistoryLengthForTesting() const;

  // Invokes OnSetFocus and marks the widget as active depending on the value
  // of |enable|. This is used for layout tests that need to control the focus
  // synchronously from the renderer.
  void SetFocusAndActivateForTesting(bool enable);

  // Change the device scale factor and force the compositor to resize.
  void SetDeviceScaleFactorForTesting(float factor);

  // Used to force the size of a window when running layout tests.
  void ForceResizeForTesting(const gfx::Size& new_size);

  void UseSynchronousResizeModeForTesting(bool enable);

  // Control autoresize mode.
  void EnableAutoResizeForTesting(const gfx::Size& min_size,
                                  const gfx::Size& max_size);
  void DisableAutoResizeForTesting(const gfx::Size& new_size);

  // IPC::Listener implementation ----------------------------------------------

  bool OnMessageReceived(const IPC::Message& msg) override;

  // blink::WebWidgetClient implementation ------------------------------------

  // Most methods are handled by RenderWidget.
  void show(blink::WebNavigationPolicy policy) override;
  void didHandleGestureEvent(const blink::WebGestureEvent& event,
                             bool event_cancelled) override;
  void onMouseDown(const blink::WebNode& mouse_down_node) override;
  void initializeLayerTreeView() override;

  // TODO(lfg): Remove once WebViewClient no longer inherits from
  // WebWidgetClient.
  void closeWidgetSoon() override;
  void convertViewportToWindow(blink::WebRect* rect) override;
  void convertWindowToViewport(blink::WebFloatRect* rect) override;
  void didOverscroll(const blink::WebFloatSize& overscrollDelta,
                     const blink::WebFloatSize& accumulatedOverscroll,
                     const blink::WebFloatPoint& positionInViewport,
                     const blink::WebFloatSize& velocityInViewport) override;
  void hasTouchEventHandlers(bool has_handlers) override;
  void resetInputMethod() override;
  blink::WebScreenInfo screenInfo() override;
  void setToolTipText(const blink::WebString&,
                      blink::WebTextDirection hint) override;
  void setTouchAction(blink::WebTouchAction touchAction) override;
  void showImeIfNeeded() override;
  void showUnhandledTapUIIfNeeded(const blink::WebPoint& tappedPosition,
                                  const blink::WebNode& tappedNode,
                                  bool pageChanged) override;
  blink::WebWidgetClient* widgetClient() override;

  // blink::WebViewClient implementation --------------------------------------

  blink::WebView* createView(blink::WebLocalFrame* creator,
                             const blink::WebURLRequest& request,
                             const blink::WebWindowFeatures& features,
                             const blink::WebString& frame_name,
                             blink::WebNavigationPolicy policy,
                             bool suppress_opener) override;
  blink::WebWidget* createPopupMenu(blink::WebPopupType popup_type) override;
  blink::WebStorageNamespace* createSessionStorageNamespace() override;
  void printPage(blink::WebLocalFrame* frame) override;
  bool enumerateChosenDirectory(
      const blink::WebString& path,
      blink::WebFileChooserCompletion* chooser_completion) override;
  void didCancelCompositionOnSelectionChange() override;
  void SetValidationMessageDirection(base::string16* main_text,
                                     blink::WebTextDirection main_text_hint,
                                     base::string16* sub_text,
                                     blink::WebTextDirection sub_text_hint);
  void showValidationMessage(const blink::WebRect& anchor_in_viewport,
                             const blink::WebString& main_text,
                             blink::WebTextDirection main_text_hint,
                             const blink::WebString& sub_text,
                             blink::WebTextDirection hint) override;
  void hideValidationMessage() override;
  void moveValidationMessage(
      const blink::WebRect& anchor_in_viewport) override;
  void setStatusText(const blink::WebString& text) override;
  void setMouseOverURL(const blink::WebURL& url) override;
  void setKeyboardFocusURL(const blink::WebURL& url) override;
  bool acceptsLoadDrops() override;
  void focusNext() override;
  void focusPrevious() override;
  void focusedNodeChanged(const blink::WebNode& fromNode,
                          const blink::WebNode& toNode) override;
  void didUpdateLayout() override;
#if defined(OS_ANDROID)
  bool didTapMultipleTargets(
      const blink::WebSize& inner_viewport_offset,
      const blink::WebRect& touch_rect,
      const blink::WebVector<blink::WebRect>& target_rects) override;
#endif
  blink::WebString acceptLanguages() override;
  void navigateBackForwardSoon(int offset) override;
  int historyBackListCount() override;
  int historyForwardListCount() override;
  blink::WebSpeechRecognizer* speechRecognizer() override;
  void zoomLimitsChanged(double minimum_level, double maximum_level) override;
  virtual void zoomLevelChanged();
  void pageScaleFactorChanged() override;
  virtual double zoomLevelToZoomFactor(double zoom_level) const;
  virtual double zoomFactorToZoomLevel(double factor) const;
  void draggableRegionsChanged() override;
  void pageImportanceSignalsChanged() override;
  void didAutoResize(const blink::WebSize& newSize) override;
  blink::WebRect rootWindowRect() override;
  void didFocus() override;

#if defined(OS_ANDROID)
  void scheduleContentIntent(const blink::WebURL& intent,
                             bool is_main_frame) override;
  void cancelScheduledContentIntents() override;
  blink::WebURL detectContentIntentAt(
      const blink::WebHitTestResult& touch_hit) override;

  // Only used on Android since all other platforms implement
  // date and time input fields using MULTIPLE_FIELDS_UI
  bool openDateTimeChooser(const blink::WebDateTimeChooserParams&,
                           blink::WebDateTimeChooserCompletion*) override;
  virtual void didScrollWithKeyboard(const blink::WebSize& delta);
#endif

  // RenderView implementation -------------------------------------------------

  bool Send(IPC::Message* message) override;
  RenderFrameImpl* GetMainRenderFrame() override;
  int GetRoutingID() const override;
  gfx::Size GetSize() const override;
  float GetDeviceScaleFactor() const override;
  WebPreferences& GetWebkitPreferences() override;
  void SetWebkitPreferences(const WebPreferences& preferences) override;
  blink::WebView* GetWebView() override;
  blink::WebFrameWidget* GetWebFrameWidget() override;
  bool ShouldDisplayScrollbars(int width, int height) const override;
  int GetEnabledBindings() const override;
  bool GetContentStateImmediately() const override;
  void Repaint(const gfx::Size& size) override;
  void SetEditCommandForNextKeyEvent(const std::string& name,
                                     const std::string& value) override;
  void ClearEditCommands() override;
  const std::string& GetAcceptLanguages() const override;
#if defined(OS_ANDROID)
  void UpdateBrowserControlsState(BrowserControlsState constraints,
                                  BrowserControlsState current,
                                  bool animate) override;
#endif
  void ConvertViewportToWindowViaWidget(blink::WebRect* rect) override;
  gfx::RectF ElementBoundsInWindow(const blink::WebElement& element) override;
  bool HasAddedInputHandler() const override;

  bool uses_temporary_zoom_level() const { return uses_temporary_zoom_level_; }

  // Please do not add your stuff randomly to the end here. If there is an
  // appropriate section, add it there. If not, there are some random functions
  // nearer to the top you can add it to.

 protected:
  // RenderWidget overrides:
  blink::WebWidget* GetWebWidget() const override;
  void CloseForFrame() override;
  void Close() override;
  void OnResize(const ResizeParams& params) override;
  void OnSetFocus(bool enable) override;
  GURL GetURLForGraphicsContext3D() override;
  void OnOrientationChange() override;
  void DidCommitCompositorFrame() override;
  void DidCompletePageScaleAnimation() override;
  void OnDeviceScaleFactorChanged() override;
  void ResizeWebWidget() override;

  RenderViewImpl(CompositorDependencies* compositor_deps,
                 const mojom::CreateViewParams& params);

  void Initialize(const mojom::CreateViewParams& params,
                  bool was_created_by_renderer);
  void SetScreenMetricsEmulationParameters(
      bool enabled,
      const blink::WebDeviceEmulationParams& params) override;

  // Do not delete directly.  This class is reference counted.
  ~RenderViewImpl() override;

 private:
  // For unit tests.
  friend class DevToolsAgentTest;
  friend class RenderViewImplScaleFactorTest;
  friend class RenderViewImplTest;
  friend class RenderViewTest;
  friend class RendererAccessibilityTest;

  // TODO(nasko): Temporarily friend RenderFrameImpl, so we don't duplicate
  // utility functions needed in both classes, while we move frame specific
  // code away from this class.
  friend class RenderFrameImpl;

  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, RenderFrameMessageAfterDetach);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, DecideNavigationPolicyForWebUI);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest,
                           DidFailProvisionalLoadWithErrorForError);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest,
                           DidFailProvisionalLoadWithErrorForCancellation);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, ImeComposition);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, InsertCharacters);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, JSBlockSentAfterPageLoad);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, LastCommittedUpdateState);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, OnHandleKeyboardEvent);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, OnImeTypeChanged);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, OnNavStateChanged);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, OnSetTextDirection);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, OnUpdateWebPreferences);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest,
                           SetEditableSelectionAndComposition);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, StaleNavigationsIgnored);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest,
                           DontIgnoreBackAfterNavEntryLimit);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, UpdateTargetURLWithInvalidURL);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest,
                           GetCompositionCharacterBoundsTest);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, OnNavigationHttpPost);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplScaleFactorTest,
                           ScreenMetricsEmulationWithOriginalDSF1);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplScaleFactorTest,
                           ScreenMetricsEmulationWithOriginalDSF2);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest,
                           DecideNavigationPolicyHandlesAllTopLevel);
#if defined(OS_MACOSX)
  FRIEND_TEST_ALL_PREFIXES(RenderViewTest, MacTestCmdUp);
#endif
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, SetHistoryLengthAndOffset);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, NavigateFrame);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, BasicRenderFrame);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, TextInputTypeWithPepper);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest,
                           MessageOrderInDidChangeSelection);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, SendCandidateWindowEvents);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, RenderFrameClearedAfterClose);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, PaintAfterSwapOut);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest,
                           SetZoomLevelAfterCrossProcessNavigation);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplScaleFactorTest,
                           ConverViewportToScreenWithZoomForDSF);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplScaleFactorTest,
                           GetCompositionCharacterBoundsTest);

  enum ErrorPageType {
    DNS_ERROR,
    HTTP_404,
    CONNECTION_ERROR,
  };

  // RenderWidgetOwnerDelegate implementation ----------------------------------

  void RenderWidgetDidSetColorProfile(
      const std::vector<char>& color_profile) override;
  void RenderWidgetFocusChangeComplete() override;
  bool DoesRenderWidgetHaveTouchEventHandlersAt(
      const gfx::Point& point) const override;
  bool RenderWidgetWillHandleMouseEvent(
      const blink::WebMouseEvent& event) override;

  // Old WebFrameClient implementations ----------------------------------------

  // RenderViewImpl used to be a WebFrameClient, but now RenderFrameImpl is the
  // WebFrameClient. However, many implementations of WebFrameClient methods
  // still live here and are called from RenderFrameImpl. These implementations
  // are to be moved to RenderFrameImpl <http://crbug.com/361761>.

  void didChangeIcon(blink::WebLocalFrame*, blink::WebIconURL::Type);

  static Referrer GetReferrerFromRequest(
      blink::WebFrame* frame,
      const blink::WebURLRequest& request);

  static WindowOpenDisposition NavigationPolicyToDisposition(
      blink::WebNavigationPolicy policy);

  void ApplyWebPreferencesInternal(const WebPreferences& prefs,
                                   blink::WebView* web_view,
                                   CompositorDependencies* compositor_deps);

  // IPC message handlers ------------------------------------------------------
  //
  // The documentation for these functions should be in
  // content/common/*_messages.h for the message that the function is handling.
  void OnExecuteEditCommand(const std::string& name, const std::string& value);
  void OnMoveCaret(const gfx::Point& point);
  void OnScrollFocusedEditableNodeIntoRect(const gfx::Rect& rect);
  void OnAllowBindings(int enabled_bindings_flags);
  void OnAllowScriptToClose(bool script_can_close);
  void OnCancelDownload(int32_t download_id);
  void OnClosePage();
  void OnClose();

  void OnShowContextMenu(ui::MenuSourceType source_type,
                         const gfx::Point& location);
  void OnDeterminePageLanguage();
  void OnDisableScrollbarsForSmallWindows(
      const gfx::Size& disable_scrollbars_size_limit);
  void OnEnablePreferredSizeChangedMode();
  void OnEnableAutoResize(const gfx::Size& min_size, const gfx::Size& max_size);
  void OnDisableAutoResize(const gfx::Size& new_size);
  void OnEnumerateDirectoryResponse(int id,
                                    const std::vector<base::FilePath>& paths);
  void OnMediaPlayerActionAt(const gfx::Point& location,
                             const blink::WebMediaPlayerAction& action);
  void OnPluginActionAt(const gfx::Point& location,
                        const blink::WebPluginAction& action);
  void OnMoveOrResizeStarted();
  void OnReleaseDisambiguationPopupBitmap(const cc::SharedBitmapId& id);
  void OnSetActive(bool active);
  void OnSetBackgroundOpaque(bool opaque);
  void OnExitFullscreen();
  void OnSetHistoryOffsetAndLength(int history_offset, int history_length);
  void OnSetInitialFocus(bool reverse);
  void OnSetRendererPrefs(const RendererPreferences& renderer_prefs);
  void OnSetWebUIProperty(const std::string& name, const std::string& value);
  void OnSuppressDialogsUntilSwapOut();
  void OnUpdateTargetURLAck();
  void OnUpdateWebPreferences(const WebPreferences& prefs);
  void OnSetPageScale(float page_scale_factor);
  void OnZoom(PageZoom zoom);
  void OnForceRedraw(const ui::LatencyInfo& latency_info);
  void OnSelectWordAroundCaret();
  void OnAudioStateChanged(bool is_audio_playing);
#if defined(OS_ANDROID)
  void OnUndoScrollFocusedEditableNodeIntoRect();
  void OnUpdateBrowserControlsState(bool enable_hiding,
                                    bool enable_showing,
                                    bool animate);
  void OnExtractSmartClipData(const gfx::Rect& rect);
#elif defined(OS_MACOSX)
  void OnGetRenderedText();
#endif

  // Page message handlers -----------------------------------------------------
  void OnUpdateWindowScreenRect(gfx::Rect window_screen_rect);
  void OnSetZoomLevel(PageMsg_SetZoomLevel_Command command, double zoom_level);
  void OnPageWasHidden();
  void OnPageWasShown();

  // Adding a new message handler? Please add it in alphabetical order above
  // and put it in the same position in the .cc file.

  // Misc private functions ----------------------------------------------------
  // Check whether the preferred size has changed.
  void CheckPreferredSize();

#if defined(OS_ANDROID)
  // Launch an Android content intent with the given URL.
  void LaunchAndroidContentIntent(const GURL& intent_url,
                                  size_t request_id,
                                  bool is_main_frame);
#endif

#if defined(OS_WIN) || (defined(OS_POSIX) && !defined(OS_MACOSX))
  void UpdateFontRenderingFromRendererPrefs();
#else
  void UpdateFontRenderingFromRendererPrefs() {}
#endif

  // In OOPIF-enabled modes, this tells each RenderFrame with a pending state
  // update to inform the browser process.
  void SendFrameStateUpdates();

  // Update the target url and tell the browser that the target URL has changed.
  // If |url| is empty, show |fallback_url|.
  void UpdateTargetURL(const GURL& url, const GURL& fallback_url);

  // Tells the browser what the new list of favicons for the webpage is.
  void SendUpdateFaviconURL(const std::vector<FaviconURL>& urls);

  // Invoked from DidStopLoading(). Sends the current list of loaded favicons to
  // the browser.
  void DidStopLoadingIcons();

  // Coordinate conversion -----------------------------------------------------

  gfx::RectF ClientRectToPhysicalWindowRect(const gfx::RectF& rect) const;

  // RenderFrameImpl accessible state ------------------------------------------
  // The following section is the set of methods that RenderFrameImpl needs
  // to access RenderViewImpl state. The set of state variables are page-level
  // specific, so they don't belong in RenderFrameImpl and should remain in
  // this object.
  base::ObserverList<RenderViewObserver>& observers() { return observers_; }

  NavigationGesture navigation_gesture() {
    return navigation_gesture_;
  }
  void set_navigation_gesture(NavigationGesture gesture) {
    navigation_gesture_ = gesture;
  }

// Platform specific theme preferences if any are updated here.
#if defined(OS_WIN)
  void UpdateThemePrefs();
#else
  void UpdateThemePrefs() {}
#endif

  void UpdateWebViewWithDeviceScaleFactor();

  // Send the appropriate ack to be able discard this input event message.
  void OnDiscardInputEvent(const blink::WebInputEvent* input_event,
                           const ui::LatencyInfo& latency_info,
                           InputEventDispatchType dispatch_type);

  // ---------------------------------------------------------------------------
  // ADDING NEW FUNCTIONS? Please keep private functions alphabetized and put
  // it in the same order in the .cc file as it was in the header.
  // ---------------------------------------------------------------------------

  // Settings ------------------------------------------------------------------

  WebPreferences webkit_preferences_;
  RendererPreferences renderer_preferences_;

  // Whether content state (such as form state, scroll position and page
  // contents) should be sent to the browser immediately. This is normally
  // false, but set to true by some tests.
  bool send_content_state_immediately_;

  // Bitwise-ORed set of extra bindings that have been enabled.  See
  // BindingsPolicy for details.
  int enabled_bindings_;

  // If true, we send IPC messages when |preferred_size_| changes.
  bool send_preferred_size_changes_;

  // If non-empty, and |send_preferred_size_changes_| is true, disable drawing
  // scroll bars on windows smaller than this size.  Used for windows that the
  // browser resizes to the size of the content, such as browser action popups.
  // If a render view is set to the minimum size of its content, webkit may add
  // scroll bars.  This makes sense for fixed sized windows, but it does not
  // make sense when the size of the view was chosen to fit the content.
  // This setting ensures that no scroll bars are drawn.  The size limit exists
  // because if the view grows beyond a size known to the browser, scroll bars
  // should be drawn.
  gfx::Size disable_scrollbars_size_limit_;

  // Loading state -------------------------------------------------------------

  // The gesture that initiated the current navigation.
  // TODO(nasko): Move to RenderFrame, as this is per-frame state.
  NavigationGesture navigation_gesture_;

  // Used for popups.
  bool opened_by_user_gesture_;

  // Timer used to delay the updating of nav state (see
  // StartNavStateSyncTimerIfNecessary).
  base::OneShotTimer nav_state_sync_timer_;

  // Set of RenderFrame routing IDs for frames that having pending UpdateState
  // messages to send when the next |nav_state_sync_timer_| fires.
  std::set<int> frames_with_pending_state_;

  // History list --------------------------------------------------------------

  // The offset of the current item in the history list.
  int history_list_offset_;

  // The RenderView's current impression of the history length.  This includes
  // any items that have committed in this process, but because of cross-process
  // navigations, the history may have some entries that were committed in other
  // processes.  We won't know about them until the next navigation in this
  // process.
  int history_list_length_;

  // Counter to track how many frames have sent start notifications but not stop
  // notifications. TODO(avi): Remove this once DidStartLoading/DidStopLoading
  // are gone.
  int frames_in_progress_;

  // UI state ------------------------------------------------------------------

  // The state of our target_url transmissions. When we receive a request to
  // send a URL to the browser, we set this to TARGET_INFLIGHT until an ACK
  // comes back - if a new request comes in before the ACK, we store the new
  // URL in pending_target_url_ and set the status to TARGET_PENDING. If an
  // ACK comes back and we are in TARGET_PENDING, we send the stored URL and
  // revert to TARGET_INFLIGHT.
  //
  // We don't need a queue of URLs to send, as only the latest is useful.
  enum {
    TARGET_NONE,
    TARGET_INFLIGHT,  // We have a request in-flight, waiting for an ACK
    TARGET_PENDING    // INFLIGHT + we have a URL waiting to be sent
  } target_url_status_;

  // The URL we show the user in the status bar. We use this to determine if we
  // want to send a new one (we do not need to send duplicates). It will be
  // equal to either |mouse_over_url_| or |focus_url_|, depending on which was
  // updated last.
  GURL target_url_;

  // The URL the user's mouse is hovering over.
  GURL mouse_over_url_;

  // The URL that has keyboard focus.
  GURL focus_url_;

  // The next target URL we want to send to the browser.
  GURL pending_target_url_;

  // Indicates whether this view overrides url-based zoom settings.
  bool uses_temporary_zoom_level_;

#if defined(OS_ANDROID)
  // Cache the old browser controls state constraints. Used when updating
  // current value only without altering the constraints.
  BrowserControlsState top_controls_constraints_;
#endif

  // Whether or not Blink's viewport size should be shrunk by the height of the
  // URL-bar.
  bool browser_controls_shrink_blink_size_;

  // The height of the browser controls.
  float top_controls_height_;

  // View ----------------------------------------------------------------------

  blink::WebView* webview_;

  // Cache the preferred size of the page in order to prevent sending the IPC
  // when layout() recomputes but doesn't actually change sizes.
  gfx::Size preferred_size_;

  // Used to delay determining the preferred size (to avoid intermediate
  // states for the sizes).
  base::OneShotTimer check_preferred_size_timer_;

  // Bookkeeping to suppress redundant scroll and focus requests for an already
  // scrolled and focused editable node.
  bool has_scrolled_focused_editable_node_into_rect_;
  gfx::Rect rect_for_scrolled_focused_editable_node_;

  // Used to indicate the zoom level to be used during subframe loads, since
  // they should match page zoom level.
  double page_zoom_level_;

  // Helper objects ------------------------------------------------------------

  RenderFrameImpl* main_render_frame_;

  // Note: RenderViewImpl is pulling double duty: it's the RenderWidget for the
  // "view", but it's also the RenderWidget for the main frame.
  blink::WebFrameWidget* frame_widget_;

  // The next group of objects all implement RenderViewObserver, so are deleted
  // along with the RenderView automatically.  This is why we just store
  // weak references.

  // The speech recognition dispatcher attached to this view, lazily
  // initialized.
  SpeechRecognitionDispatcher* speech_recognition_dispatcher_;

  std::unique_ptr<HistoryController> history_controller_;

#if defined(OS_ANDROID)
  // Android Specific ---------------------------------------------------------

  // Expected id of the next content intent launched. Used to prevent scheduled
  // intents to be launched if aborted.
  size_t expected_content_intent_id_;

  // List of click-based content detectors.
  std::vector<std::unique_ptr<ContentDetector>> content_detectors_;

  // A date/time picker object for date and time related input elements.
  std::unique_ptr<RendererDateTimePicker> date_time_picker_client_;

#endif

  // Misc ----------------------------------------------------------------------

  // The current directory enumeration callback
  std::map<int, blink::WebFileChooserCompletion*> enumeration_completions_;
  int enumeration_completion_id_;

  // The SessionStorage namespace that we're assigned to has an ID, and that ID
  // is passed to us upon creation.  WebKit asks for this ID upon first use and
  // uses it whenever asking the browser process to allocate new storage areas.
  int64_t session_storage_namespace_id_;

  // All the registered observers.  We expect this list to be small, so vector
  // is fine.
  base::ObserverList<RenderViewObserver> observers_;

  // NOTE: stats_collection_observer_ should be the last members because their
  // constructors call the AddObservers method of RenderViewImpl.
  std::unique_ptr<StatsCollectionObserver> stats_collection_observer_;

  typedef std::map<cc::SharedBitmapId, cc::SharedBitmap*> BitmapMap;
  BitmapMap disambiguation_bitmaps_;

  bool has_added_input_handler_;

  // ---------------------------------------------------------------------------
  // ADDING NEW DATA? Please see if it fits appropriately in one of the above
  // sections rather than throwing it randomly at the end. If you're adding a
  // bunch of stuff, you should probably create a helper class and put your
  // data and methods on that to avoid bloating RenderView more.  You can
  // use the Observer interface to filter IPC messages and receive frame change
  // notifications.
  // ---------------------------------------------------------------------------

  DISALLOW_COPY_AND_ASSIGN(RenderViewImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_VIEW_IMPL_H_
