// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A BrowserPluginGuest is the browser side of a browser <--> embedder
// renderer channel. A BrowserPlugin (a WebPlugin) is on the embedder
// renderer side of browser <--> embedder renderer communication.
//
// BrowserPluginGuest lives on the UI thread of the browser process. Any
// messages about the guest render process that the embedder might be interested
// in receiving should be listened for here.
//
// BrowserPluginGuest is a WebContentsObserver for the guest WebContents.
// BrowserPluginGuest operates under the assumption that the guest will be
// accessible through only one RenderViewHost for the lifetime of
// the guest WebContents. Thus, cross-process navigation is not supported.

#ifndef CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_GUEST_H_
#define CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_GUEST_H_

#include <map>
#include <queue>

#include "base/compiler_specific.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/common/edit_command.h"
#include "content/common/input/input_event_ack_state.h"
#include "content/public/browser/browser_plugin_guest_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/WebKit/public/web/WebCompositionUnderline.h"
#include "third_party/WebKit/public/web/WebDragOperation.h"
#include "third_party/WebKit/public/web/WebDragStatus.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/base/ime/text_input_mode.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/gfx/rect.h"

class SkBitmap;
struct BrowserPluginHostMsg_AutoSize_Params;
struct BrowserPluginHostMsg_Attach_Params;
struct BrowserPluginHostMsg_ResizeGuest_Params;
struct FrameHostMsg_CompositorFrameSwappedACK_Params;
struct FrameHostMsg_ReclaimCompositorResources_Params;
#if defined(OS_MACOSX)
struct ViewHostMsg_ShowPopup_Params;
#endif
struct ViewHostMsg_TextInputState_Params;
struct ViewHostMsg_UpdateRect_Params;

namespace blink {
class WebInputEvent;
}

namespace gfx {
class Range;
}

namespace content {

class BrowserPluginGuestManager;
class RenderViewHostImpl;
class RenderWidgetHostView;
class SiteInstance;
class WebCursor;
struct DropData;

// A browser plugin guest provides functionality for WebContents to operate in
// the guest role and implements guest-specific overrides for ViewHostMsg_*
// messages.
//
// When a guest is initially created, it is in an unattached state. That is,
// it is not visible anywhere and has no embedder WebContents assigned.
// A BrowserPluginGuest is said to be "attached" if it has an embedder.
// A BrowserPluginGuest can also create a new unattached guest via
// CreateNewWindow. The newly created guest will live in the same partition,
// which means it can share storage and can script this guest.
class CONTENT_EXPORT BrowserPluginGuest : public WebContentsObserver {
 public:
  virtual ~BrowserPluginGuest();

  // The WebContents passed into the factory method here has not been
  // initialized yet and so it does not yet hold a SiteInstance.
  // BrowserPluginGuest must be constructed and installed into a WebContents
  // prior to its initialization because WebContents needs to determine what
  // type of WebContentsView to construct on initialization. The content
  // embedder needs to be aware of |guest_site_instance| on the guest's
  // construction and so we pass it in here.
  static BrowserPluginGuest* Create(
      int instance_id,
      SiteInstance* guest_site_instance,
      WebContentsImpl* web_contents,
      scoped_ptr<base::DictionaryValue> extra_params,
      BrowserPluginGuest* opener);

  // Returns whether the given WebContents is a BrowserPlugin guest.
  static bool IsGuest(WebContentsImpl* web_contents);

  // Returns whether the given RenderviewHost is a BrowserPlugin guest.
  static bool IsGuest(RenderViewHostImpl* render_view_host);

  // Returns a WeakPtr to this BrowserPluginGuest.
  base::WeakPtr<BrowserPluginGuest> AsWeakPtr();

  // Sets the lock state of the pointer. Returns true if |allowed| is true and
  // the mouse has been successfully locked.
  bool LockMouse(bool allowed);

  // Called when the embedder WebContents changes visibility.
  void EmbedderVisibilityChanged(bool visible);

  // Destroys the guest WebContents and all its associated state, including
  // this BrowserPluginGuest, and its new unattached windows.
  void Destroy();

  // Returns the identifier that uniquely identifies a browser plugin guest
  // within an embedder.
  int instance_id() const { return instance_id_; }

  bool OnMessageReceivedFromEmbedder(const IPC::Message& message);

  WebContentsImpl* embedder_web_contents() const {
    return embedder_web_contents_;
  }

  // Returns the embedder's RenderWidgetHostView if it is available.
  // Returns NULL otherwise.
  RenderWidgetHostView* GetEmbedderRenderWidgetHostView();

  bool focused() const { return focused_; }
  bool visible() const { return guest_visible_; }
  bool is_in_destruction() { return is_in_destruction_; }

  // Returns the BrowserPluginGuest that created this guest, if any.
  BrowserPluginGuest* GetOpener() const;

  void UpdateVisibility();

  void CopyFromCompositingSurface(
      gfx::Rect src_subrect,
      gfx::Size dst_size,
      const base::Callback<void(bool, const SkBitmap&)>& callback);

  BrowserPluginGuestManager* GetBrowserPluginGuestManager() const;

  // WebContentsObserver implementation.
  virtual void DidCommitProvisionalLoadForFrame(
      int64 frame_id,
      const base::string16& frame_unique_name,
      bool is_main_frame,
      const GURL& url,
      PageTransition transition_type,
      RenderViewHost* render_view_host) OVERRIDE;

  virtual void RenderViewReady() OVERRIDE;
  virtual void RenderProcessGone(base::TerminationStatus status) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // Exposes the protected web_contents() from WebContentsObserver.
  WebContentsImpl* GetWebContents() const;

  gfx::Point GetScreenCoordinates(const gfx::Point& relative_position) const;

  // Helper to send messages to embedder. This methods fills the message with
  // the correct routing id.
  void SendMessageToEmbedder(IPC::Message* msg);

  // Returns whether the guest is attached to an embedder.
  bool attached() const { return embedder_web_contents_ != NULL; }

  // Attaches this BrowserPluginGuest to the provided |embedder_web_contents|
  // and initializes the guest with the provided |params|. Attaching a guest
  // to an embedder implies that this guest's lifetime is no longer managed
  // by its opener, and it can begin loading resources. |extra_params| are
  // parameters passed into BrowserPlugin from JavaScript to be forwarded to
  // the content embedder.
  void Attach(WebContentsImpl* embedder_web_contents,
              const BrowserPluginHostMsg_Attach_Params& params,
              const base::DictionaryValue& extra_params);

  // Returns whether BrowserPluginGuest is interested in receiving the given
  // |message|.
  static bool ShouldForwardToBrowserPluginGuest(const IPC::Message& message);
  gfx::Rect ToGuestRect(const gfx::Rect& rect);

  void DragSourceEndedAt(int client_x, int client_y, int screen_x,
      int screen_y, blink::WebDragOperation operation);

  // Called when the drag started by this guest ends at an OS-level.
  void EndSystemDrag();

  void set_delegate(BrowserPluginGuestDelegate* delegate) {
    DCHECK(!delegate_);
    delegate_ = delegate;
  }

  void RespondToPermissionRequest(int request_id,
                                  bool should_allow,
                                  const std::string& user_input);

  void PointerLockPermissionResponse(bool allow);

 private:
  class EmbedderWebContentsObserver;

  // BrowserPluginGuest is a WebContentsObserver of |web_contents| and
  // |web_contents| has to stay valid for the lifetime of BrowserPluginGuest.
  BrowserPluginGuest(int instance_id,
                     bool has_render_view,
                     WebContentsImpl* web_contents);

  void WillDestroy();

  void Initialize(const BrowserPluginHostMsg_Attach_Params& params,
                  WebContentsImpl* embedder_web_contents,
                  const base::DictionaryValue& extra_params);

  bool InAutoSizeBounds(const gfx::Size& size) const;

  // Message handlers for messages from embedder.

  void OnCompositorFrameSwappedACK(
      int instance_id,
      const FrameHostMsg_CompositorFrameSwappedACK_Params& params);
  void OnCopyFromCompositingSurfaceAck(int instance_id,
                                       int request_id,
                                       const SkBitmap& bitmap);
  // Handles drag events from the embedder.
  // When dragging, the drag events go to the embedder first, and if the drag
  // happens on the browser plugin, then the plugin sends a corresponding
  // drag-message to the guest. This routes the drag-message to the guest
  // renderer.
  void OnDragStatusUpdate(int instance_id,
                          blink::WebDragStatus drag_status,
                          const DropData& drop_data,
                          blink::WebDragOperationsMask drag_mask,
                          const gfx::Point& location);
  // Instructs the guest to execute an edit command decoded in the embedder.
  void OnExecuteEditCommand(int instance_id,
                            const std::string& command);

  // Returns compositor resources reclaimed in the embedder to the guest.
  void OnReclaimCompositorResources(
      int instance_id,
      const FrameHostMsg_ReclaimCompositorResources_Params& params);

  void OnHandleInputEvent(int instance_id,
                                  const gfx::Rect& guest_window_rect,
                                  const blink::WebInputEvent* event);
  void OnLockMouse(bool user_gesture,
                   bool last_unlocked_by_target,
                   bool privileged);
  void OnLockMouseAck(int instance_id, bool succeeded);
  void OnPluginDestroyed(int instance_id);
  // Resizes the guest's web contents.
  void OnResizeGuest(
      int instance_id, const BrowserPluginHostMsg_ResizeGuest_Params& params);
  void OnSetFocus(int instance_id, bool focused);
  // Sets the name of the guest so that other guests in the same partition can
  // access it.
  void OnSetName(int instance_id, const std::string& name);
  // Updates the size state of the guest.
  void OnSetAutoSize(
      int instance_id,
      const BrowserPluginHostMsg_AutoSize_Params& auto_size_params,
      const BrowserPluginHostMsg_ResizeGuest_Params& resize_guest_params);
  void OnSetEditCommandsForNextKeyEvent(
      int instance_id,
      const std::vector<EditCommand>& edit_commands);
  void OnSetContentsOpaque(int instance_id, bool opaque);
  // The guest WebContents is visible if both its embedder is visible and
  // the browser plugin element is visible. If either one is not then the
  // WebContents is marked as hidden. A hidden WebContents will consume
  // fewer GPU and CPU resources.
  //
  // When every WebContents in a RenderProcessHost is hidden, it will lower
  // the priority of the process (see RenderProcessHostImpl::WidgetHidden).
  //
  // It will also send a message to the guest renderer process to cleanup
  // resources such as dropping back buffers and adjusting memory limits (if in
  // compositing mode, see CCLayerTreeHost::setVisible).
  //
  // Additionally, it will slow down Javascript execution and garbage
  // collection. See RenderThreadImpl::IdleHandler (executed when hidden) and
  // RenderThreadImpl::IdleHandlerInForegroundTab (executed when visible).
  void OnSetVisibility(int instance_id, bool visible);
  void OnUnlockMouse();
  void OnUnlockMouseAck(int instance_id);
  void OnUpdateGeometry(int instance_id, const gfx::Rect& view_rect);

  void OnTextInputStateChanged(
      const ViewHostMsg_TextInputState_Params& params);

  void OnImeSetComposition(
      int instance_id,
      const std::string& text,
      const std::vector<blink::WebCompositionUnderline>& underlines,
      int selection_start,
      int selection_end);
  void OnImeConfirmComposition(
      int instance_id,
      const std::string& text,
      bool keep_selection);
  void OnExtendSelectionAndDelete(int instance_id, int before, int after);
  void OnImeCancelComposition();
#if defined(OS_MACOSX) || defined(USE_AURA)
  void OnImeCompositionRangeChanged(
      const gfx::Range& range,
      const std::vector<gfx::Rect>& character_bounds);
#endif

  // Message handlers for messages from guest.

  void OnDragStopped();
  void OnHandleInputEventAck(
      blink::WebInputEvent::Type event_type,
      InputEventAckState ack_result);
  void OnHasTouchEventHandlers(bool accept);
  void OnSetCursor(const WebCursor& cursor);
  // On MacOSX popups are painted by the browser process. We handle them here
  // so that they are positioned correctly.
#if defined(OS_MACOSX)
  void OnShowPopup(const ViewHostMsg_ShowPopup_Params& params);
#endif
  void OnShowWidget(int route_id, const gfx::Rect& initial_pos);
  void OnTakeFocus(bool reverse);
  void OnUpdateFrameName(int frame_id,
                         bool is_top_level,
                         const std::string& name);
  void OnUpdateRect(const ViewHostMsg_UpdateRect_Params& params);

  // Forwards all messages from the |pending_messages_| queue to the embedder.
  void SendQueuedMessages();

  scoped_ptr<EmbedderWebContentsObserver> embedder_web_contents_observer_;
  WebContentsImpl* embedder_web_contents_;

  // An identifier that uniquely identifies a browser plugin guest within an
  // embedder.
  int instance_id_;
  float guest_device_scale_factor_;
  gfx::Rect guest_window_rect_;
  gfx::Rect guest_screen_rect_;
  bool focused_;
  bool mouse_locked_;
  bool pending_lock_request_;
  bool guest_visible_;
  bool guest_opaque_;
  bool embedder_visible_;
  std::string name_;
  bool auto_size_enabled_;
  gfx::Size max_auto_size_;
  gfx::Size min_auto_size_;
  gfx::Size full_size_;

  // Each copy-request is identified by a unique number. The unique number is
  // used to keep track of the right callback.
  int copy_request_id_;
  typedef base::Callback<void(bool, const SkBitmap&)> CopyRequestCallback;
  typedef std::map<int, const CopyRequestCallback> CopyRequestMap;
  CopyRequestMap copy_request_callbacks_;

  // Indicates that this BrowserPluginGuest has associated renderer-side state.
  // This is used to determine whether or not to create a new RenderView when
  // this guest is attached. A BrowserPluginGuest would have renderer-side state
  // prior to attachment if it is created via a call to window.open and
  // maintains a JavaScript reference to its opener.
  bool has_render_view_;

  // Last seen size of guest contents (by OnUpdateRect).
  gfx::Size last_seen_view_size_;
  // Last seen autosize attribute state (by OnUpdateRect).
  bool last_seen_auto_size_enabled_;

  bool is_in_destruction_;

  // Text input type states.
  ui::TextInputType last_text_input_type_;
  ui::TextInputMode last_input_mode_;
  bool last_can_compose_inline_;

  // This is a queue of messages that are destined to be sent to the embedder
  // once the guest is attached to a particular embedder.
  std::deque<linked_ptr<IPC::Message> > pending_messages_;

  BrowserPluginGuestDelegate* delegate_;

  // Weak pointer used to ask GeolocationPermissionContext about geolocation
  // permission.
  base::WeakPtrFactory<BrowserPluginGuest> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BrowserPluginGuest);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_GUEST_H_
