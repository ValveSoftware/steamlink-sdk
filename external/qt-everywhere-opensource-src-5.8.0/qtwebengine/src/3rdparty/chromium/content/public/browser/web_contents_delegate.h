// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_DELEGATE_H_

#include <stdint.h>

#include <set>
#include <string>

#include "base/callback.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/browser/bluetooth_chooser.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/navigation_type.h"
#include "content/public/common/media_stream_request.h"
#include "content/public/common/security_style.h"
#include "content/public/common/window_container_type.h"
#include "third_party/WebKit/public/platform/WebDisplayMode.h"
#include "third_party/WebKit/public/platform/WebDragOperation.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/native_widget_types.h"

class GURL;

namespace base {
class FilePath;
class ListValue;
}

namespace content {
class BrowserContext;
class ColorChooser;
class DownloadItem;
class JavaScriptDialogManager;
class PageState;
class RenderFrameHost;
class RenderViewHost;
class SessionStorageNamespace;
class WebContents;
class WebContentsImpl;
struct ColorSuggestion;
struct ContextMenuParams;
struct DropData;
struct FileChooserParams;
struct NativeWebKeyboardEvent;
struct Referrer;
struct SecurityStyleExplanations;
struct SSLStatus;
}

namespace gfx {
class Point;
class Rect;
class Size;
}

namespace url {
class Origin;
}

namespace blink {
class WebGestureEvent;
}

namespace content {
class RenderWidgetHost;

struct OpenURLParams;

// Objects implement this interface to get notified about changes in the
// WebContents and to provide necessary functionality.
class CONTENT_EXPORT WebContentsDelegate {
 public:
  WebContentsDelegate();

  // Opens a new URL inside the passed in WebContents (if source is 0 open
  // in the current front-most tab), unless |disposition| indicates the url
  // should be opened in a new tab or window.
  //
  // A nullptr source indicates the current tab (callers should probably use
  // OpenURL() for these cases which does it for you).

  // Returns the WebContents the URL is opened in, or nullptr if the URL wasn't
  // opened immediately.
  virtual WebContents* OpenURLFromTab(WebContents* source,
                                      const OpenURLParams& params);

  // Allows the delegate to optionally cancel navigations that attempt to
  // transfer to a different process between the start of the network load and
  // commit.  Defaults to true.
  virtual bool ShouldTransferNavigation();

  // Called to inform the delegate that the WebContents's navigation state
  // changed. The |changed_flags| indicates the parts of the navigation state
  // that have been updated.
  virtual void NavigationStateChanged(WebContents* source,
                                      InvalidateTypes changed_flags) {}

  // Called to inform the delegate that the WebContent's visible SSL state (as
  // defined by SSLStatus) changed.
  virtual void VisibleSSLStateChanged(const WebContents* source) {}

  // Creates a new tab with the already-created WebContents 'new_contents'.
  // The window for the added contents should be reparented correctly when this
  // method returns.  If |disposition| is NEW_POPUP, |initial_rect| should hold
  // the initial position and size. If |was_blocked| is non-nullptr, then
  // |*was_blocked| will be set to true if the popup gets blocked, and left
  // unchanged otherwise.
  virtual void AddNewContents(WebContents* source,
                              WebContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_rect,
                              bool user_gesture,
                              bool* was_blocked) {}

  // Selects the specified contents, bringing its container to the front.
  virtual void ActivateContents(WebContents* contents) {}

  // Notifies the delegate that this contents is starting or is done loading
  // some resource. The delegate should use this notification to represent
  // loading feedback. See WebContents::IsLoading()
  // |to_different_document| will be true unless the load is a fragment
  // navigation, or triggered by history.pushState/replaceState.
  virtual void LoadingStateChanged(WebContents* source,
                                   bool to_different_document) {}

  // Notifies the delegate that the page has made some progress loading.
  // |progress| is a value between 0.0 (nothing loaded) to 1.0 (page fully
  // loaded).
  virtual void LoadProgressChanged(WebContents* source,
                                   double progress) {}

  // Request the delegate to close this web contents, and do whatever cleanup
  // it needs to do.
  virtual void CloseContents(WebContents* source) {}

  // Request the delegate to move this WebContents to the specified position
  // in screen coordinates.
  virtual void MoveContents(WebContents* source, const gfx::Rect& pos) {}

  // Called to determine if the WebContents is contained in a popup window
  // or a panel window.
  virtual bool IsPopupOrPanel(const WebContents* source) const;

  // Notification that the target URL has changed.
  virtual void UpdateTargetURL(WebContents* source,
                               const GURL& url) {}

  // Notification that there was a mouse event, along with the absolute
  // coordinates of the mouse pointer and the type of event. If |motion| is
  // true, this is a normal motion event. If |exited| is true, the pointer left
  // the contents area.
  virtual void ContentsMouseEvent(WebContents* source,
                                  const gfx::Point& location,
                                  bool motion,
                                  bool exited) {}

  // Request the delegate to change the zoom level of the current tab.
  virtual void ContentsZoomChange(bool zoom_in) {}

  // Called to determine if the WebContents can be overscrolled with touch/wheel
  // gestures.
  virtual bool CanOverscrollContent() const;

  // Callback that allows vertical overscroll activies to be communicated to the
  // delegate. |delta_y| is the total amount of overscroll.
  virtual void OverscrollUpdate(float delta_y) {}

  // Invoked when a vertical overscroll completes.
  virtual void OverscrollComplete() {}

  // Return the rect where to display the resize corner, if any, otherwise
  // an empty rect.
  virtual gfx::Rect GetRootWindowResizerRect() const;

  // Invoked prior to showing before unload handler confirmation dialog.
  virtual void WillRunBeforeUnloadConfirm() {}

  // Returns true if javascript dialogs and unload alerts are suppressed.
  // Default is false.
  virtual bool ShouldSuppressDialogs(WebContents* source);

  // Returns whether pending NavigationEntries for aborted browser-initiated
  // navigations should be preserved (and thus returned from GetVisibleURL).
  // Defaults to false.
  virtual bool ShouldPreserveAbortedURLs(WebContents* source);

  // Add a message to the console. Returning true indicates that the delegate
  // handled the message. If false is returned the default logging mechanism
  // will be used for the message.
  virtual bool AddMessageToConsole(WebContents* source,
                                   int32_t level,
                                   const base::string16& message,
                                   int32_t line_no,
                                   const base::string16& source_id);

  // Tells us that we've finished firing this tab's beforeunload event.
  // The proceed bool tells us whether the user chose to proceed closing the
  // tab. Returns true if the tab can continue on firing its unload event.
  // If we're closing the entire browser, then we'll want to delay firing
  // unload events until all the beforeunload events have fired.
  virtual void BeforeUnloadFired(WebContents* tab,
                                 bool proceed,
                                 bool* proceed_to_fire_unload);

  // Returns true if the location bar should be focused by default rather than
  // the page contents. NOTE: this is only used if WebContents can't determine
  // for itself whether the location bar should be focused by default. For a
  // complete check, you should use WebContents::FocusLocationBarByDefault().
  virtual bool ShouldFocusLocationBarByDefault(WebContents* source);

  // Sets focus to the location bar or some other place that is appropriate.
  // This is called when the tab wants to encourage user input, like for the
  // new tab page.
  virtual void SetFocusToLocationBar(bool select_all) {}

  // Returns whether the page should be focused when transitioning from crashed
  // to live. Default is true.
  virtual bool ShouldFocusPageAfterCrash();

  // Returns whether the page should resume accepting requests for the new
  // window. This is used when window creation is asynchronous
  // and the navigations need to be delayed. Default is true.
  virtual bool ShouldResumeRequestsForCreatedWindow();

  // This is called when WebKit tells us that it is done tabbing through
  // controls on the page. Provides a way for WebContentsDelegates to handle
  // this. Returns true if the delegate successfully handled it.
  virtual bool TakeFocus(WebContents* source,
                         bool reverse);

  // Invoked when the page loses mouse capture.
  virtual void LostCapture() {}

  // Asks the delegate if the given tab can download.
  // Invoking the |callback| synchronously is OK.
  virtual void CanDownload(const GURL& url,
                           const std::string& request_method,
                           const base::Callback<void(bool)>& callback);

  // Returns true if the context menu operation was handled by the delegate.
  virtual bool HandleContextMenu(const content::ContextMenuParams& params);

  // Opens source view for given WebContents that is navigated to the given
  // page url.
  virtual void ViewSourceForTab(WebContents* source, const GURL& page_url);

  // Opens source view for the given subframe.
  virtual void ViewSourceForFrame(WebContents* source,
                                  const GURL& url,
                                  const PageState& page_state);

  // Allows delegates to handle keyboard events before sending to the renderer.
  // Returns true if the |event| was handled. Otherwise, if the |event| would be
  // handled in HandleKeyboardEvent() method as a normal keyboard shortcut,
  // |*is_keyboard_shortcut| should be set to true.
  virtual bool PreHandleKeyboardEvent(WebContents* source,
                                      const NativeWebKeyboardEvent& event,
                                      bool* is_keyboard_shortcut);

  // Allows delegates to handle unhandled keyboard messages coming back from
  // the renderer.
  virtual void HandleKeyboardEvent(WebContents* source,
                                   const NativeWebKeyboardEvent& event) {}

  // Allows delegates to handle gesture events before sending to the renderer.
  // Returns true if the |event| was handled and thus shouldn't be processed
  // by the renderer's event handler. Note that the touch events that create
  // the gesture are always passed to the renderer since the gesture is created
  // and dispatched after the touches return without being "preventDefault()"ed.
  virtual bool PreHandleGestureEvent(
      WebContents* source,
      const blink::WebGestureEvent& event);

  // Called when an external drag event enters the web contents window. Return
  // true to allow dragging and dropping on the web contents window or false to
  // cancel the operation. This method is used by Chromium Embedded Framework.
  virtual bool CanDragEnter(WebContents* source,
                            const DropData& data,
                            blink::WebDragOperationsMask operations_allowed);

  // Shows the repost form confirmation dialog box.
  virtual void ShowRepostFormWarningDialog(WebContents* source) {}

  // Allows delegate to override navigation to the history entries.
  // Returns true to allow WebContents to continue with the default processing.
  virtual bool OnGoToEntryOffset(int offset);

  // Allows delegate to control whether a WebContents will be created. Returns
  // true to allow the creation. Default is to allow it. In cases where the
  // delegate handles the creation/navigation itself, it will use |target_url|.
  // The embedder has to synchronously adopt |route_id| or else the view will
  // be destroyed.
  virtual bool ShouldCreateWebContents(
      WebContents* web_contents,
      int32_t route_id,
      int32_t main_frame_route_id,
      int32_t main_frame_widget_route_id,
      WindowContainerType window_container_type,
      const std::string& frame_name,
      const GURL& target_url,
      const std::string& partition_id,
      SessionStorageNamespace* session_storage_namespace);

  // Notifies the delegate about the creation of a new WebContents. This
  // typically happens when popups are created.
  virtual void WebContentsCreated(WebContents* source_contents,
                                  int opener_render_frame_id,
                                  const std::string& frame_name,
                                  const GURL& target_url,
                                  WebContents* new_contents) {}

  // Notification that the tab is hung.
  virtual void RendererUnresponsive(WebContents* source) {}

  // Notification that the tab is no longer hung.
  virtual void RendererResponsive(WebContents* source) {}

  // Invoked when a main fram navigation occurs.
  virtual void DidNavigateMainFramePostCommit(WebContents* source) {}

  // Returns a pointer to a service to manage JavaScript dialogs. May return
  // nullptr in which case dialogs aren't shown.
  virtual JavaScriptDialogManager* GetJavaScriptDialogManager(
      WebContents* source);

  // Called when color chooser should open. Returns the opened color chooser.
  // Returns nullptr if we failed to open the color chooser (e.g. when there is
  // a ColorChooserDialog already open on Windows). Ownership of the returned
  // pointer is transferred to the caller.
  virtual ColorChooser* OpenColorChooser(
      WebContents* web_contents,
      SkColor color,
      const std::vector<ColorSuggestion>& suggestions);

  // Called when a file selection is to be done.
  virtual void RunFileChooser(RenderFrameHost* render_frame_host,
                              const FileChooserParams& params) {}

  // Request to enumerate a directory.  This is equivalent to running the file
  // chooser in directory-enumeration mode and having the user select the given
  // directory.
  virtual void EnumerateDirectory(WebContents* web_contents,
                                  int request_id,
                                  const base::FilePath& path) {}

  // Shows a chooser for the user to select a nearby Bluetooth device. The
  // observer must live at least as long as the returned chooser object.
  virtual std::unique_ptr<BluetoothChooser> RunBluetoothChooser(
      RenderFrameHost* frame,
      const BluetoothChooser::EventHandler& event_handler);

  // Returns true if the delegate will embed a WebContents-owned fullscreen
  // render widget.  In this case, the delegate may access the widget by calling
  // WebContents::GetFullscreenRenderWidgetHostView().  If false is returned,
  // WebContents will be responsible for showing the fullscreen widget.
  virtual bool EmbedsFullscreenWidget() const;

  // Called when the renderer puts a tab into fullscreen mode.
  // |origin| is the origin of the initiating frame inside the |web_contents|.
  // |origin| can be empty in which case the |web_contents| last committed
  // URL's origin should be used.
  virtual void EnterFullscreenModeForTab(WebContents* web_contents,
                                         const GURL& origin) {}

  // Called when the renderer puts a tab out of fullscreen mode.
  virtual void ExitFullscreenModeForTab(WebContents*) {}

  virtual bool IsFullscreenForTabOrPending(
      const WebContents* web_contents) const;

  // Returns the actual display mode of the top-level browsing context.
  // For example, it should return 'blink::WebDisplayModeFullscreen' whenever
  // the browser window is put to fullscreen mode (either by the end user,
  // or HTML API or from a web manifest setting).
  // See http://w3c.github.io/manifest/#dfn-display-mode
  virtual blink::WebDisplayMode GetDisplayMode(
      const WebContents* web_contents) const;

  // Register a new handler for URL requests with the given scheme.
  // |user_gesture| is true if the registration is made in the context of a user
  // gesture.
  virtual void RegisterProtocolHandler(WebContents* web_contents,
                                       const std::string& protocol,
                                       const GURL& url,
                                       bool user_gesture) {}

  // Unregister the registered handler for URL requests with the given scheme.
  // |user_gesture| is true if the registration is made in the context of a user
  // gesture.
  virtual void UnregisterProtocolHandler(WebContents* web_contents,
                                         const std::string& protocol,
                                         const GURL& url,
                                         bool user_gesture) {}

  // Result of string search in the page. This includes the number of matches
  // found and the selection rect (in screen coordinates) for the string found.
  // If |final_update| is false, it indicates that more results follow.
  virtual void FindReply(WebContents* web_contents,
                         int request_id,
                         int number_of_matches,
                         const gfx::Rect& selection_rect,
                         int active_match_ordinal,
                         bool final_update) {}

#if defined(OS_ANDROID)
  // Provides the rects of the current find-in-page matches.
  // Sent as a reply to RequestFindMatchRects.
  virtual void FindMatchRectsReply(WebContents* web_contents,
                                   int version,
                                   const std::vector<gfx::RectF>& rects,
                                   const gfx::RectF& active_rect) {}
#endif

  // Invoked when the preferred size of the contents has been changed.
  virtual void UpdatePreferredSize(WebContents* web_contents,
                                   const gfx::Size& pref_size) {}

  // Invoked when the contents auto-resized and the container should match it.
  virtual void ResizeDueToAutoResize(WebContents* web_contents,
                                     const gfx::Size& new_size) {}

  // Notification message from HTML UI.
  virtual void WebUISend(WebContents* web_contents,
                         const GURL& source_url,
                         const std::string& name,
                         const base::ListValue& args) {}

  // Requests to lock the mouse. Once the request is approved or rejected,
  // GotResponseToLockMouseRequest() will be called on the requesting tab
  // contents.
  virtual void RequestToLockMouse(WebContents* web_contents,
                                  bool user_gesture,
                                  bool last_unlocked_by_target) {}

  // Notification that the page has lost the mouse lock.
  virtual void LostMouseLock() {}

  // Asks permission to use the camera and/or microphone. If permission is
  // granted, a call should be made to |callback| with the devices. If the
  // request is denied, a call should be made to |callback| with an empty list
  // of devices. |request| has the details of the request (e.g. which of audio
  // and/or video devices are requested, and lists of available devices).
  virtual void RequestMediaAccessPermission(
      WebContents* web_contents,
      const MediaStreamRequest& request,
      const MediaResponseCallback& callback);

  // Checks if we have permission to access the microphone or camera. Note that
  // this does not query the user. |type| must be MEDIA_DEVICE_AUDIO_CAPTURE
  // or MEDIA_DEVICE_VIDEO_CAPTURE.
  virtual bool CheckMediaAccessPermission(WebContents* web_contents,
                                          const GURL& security_origin,
                                          MediaStreamType type);

#if defined(OS_ANDROID)
  // Asks permission to decode media stream. After permission is determined,
  // |callback| will be called with the result.
  virtual void RequestMediaDecodePermission(
      WebContents* web_contents,
      const base::Callback<void(bool)>& callback);
#endif

  // Requests permission to access the PPAPI broker. The delegate should return
  // true and call the passed in |callback| with the result, or return false
  // to indicate that it does not support asking for permission.
  virtual bool RequestPpapiBrokerPermission(
      WebContents* web_contents,
      const GURL& url,
      const base::FilePath& plugin_path,
      const base::Callback<void(bool)>& callback);

  // Returns the size for the new render view created for the pending entry in
  // |web_contents|; if there's no size, returns an empty size.
  // This is optional for implementations of WebContentsDelegate; if the
  // delegate doesn't provide a size, the current WebContentsView's size will be
  // used.
  virtual gfx::Size GetSizeForNewRenderView(WebContents* web_contents) const;

  // Notification that validation of a form displayed by the |web_contents|
  // has failed. There can only be one message per |web_contents| at a time.
  virtual void ShowValidationMessage(WebContents* web_contents,
                                     const gfx::Rect& anchor_in_root_view,
                                     const base::string16& main_text,
                                     const base::string16& sub_text) {}

  // Notification that the delegate should hide any showing form validation
  // message.
  virtual void HideValidationMessage(WebContents* web_contents) {}

  // Notification that the form element that triggered the validation failure
  // has moved.
  virtual void MoveValidationMessage(WebContents* web_contents,
                                     const gfx::Rect& anchor_in_root_view) {}

  // Returns true if the WebContents is never visible.
  virtual bool IsNeverVisible(WebContents* web_contents);

  // Called in response to a request to save a frame. If this returns true, the
  // default behavior is suppressed.
  virtual bool SaveFrame(const GURL& url, const Referrer& referrer);

  // Can be overridden by a delegate to return the security style of the
  // given |web_contents|, populating |security_style_explanations| to
  // explain why the SecurityStyle was downgraded. Returns
  // SECURITY_STYLE_UNKNOWN if not overriden.
  virtual SecurityStyle GetSecurityStyle(
      WebContents* web_contents,
      SecurityStyleExplanations* security_style_explanations);

  // Displays platform-specific (OS) dialog with the certificate details.
  virtual void ShowCertificateViewerInDevTools(
      WebContents* web_contents,
      int cert_id);

  // Called when the active render widget is forwarding a RemoteChannel
  // compositor proto.  This is used in Blimp mode.
  virtual void ForwardCompositorProto(
      RenderWidgetHost* render_widget_host,
      const std::vector<uint8_t>& proto) {}

  // Requests the app banner. This method is called from the DevTools.
  virtual void RequestAppBannerFromDevTools(content::WebContents* web_contents);

 protected:
  virtual ~WebContentsDelegate();

 private:
  friend class WebContentsImpl;

  // Called when |this| becomes the WebContentsDelegate for |source|.
  void Attach(WebContents* source);

  // Called when |this| is no longer the WebContentsDelegate for |source|.
  void Detach(WebContents* source);

  // The WebContents that this is currently a delegate for.
  std::set<WebContents*> attached_contents_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_DELEGATE_H_
