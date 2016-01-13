// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_plugin/browser_plugin_guest.h"

#include <algorithm>

#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/browser_plugin/browser_plugin_embedder.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/frame_host/render_widget_host_view_guest.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view_guest.h"
#include "content/common/browser_plugin/browser_plugin_messages.h"
#include "content/common/content_constants_internal.h"
#include "content/common/drag_messages.h"
#include "content/common/input_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_plugin_guest_manager.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/drop_data.h"
#include "third_party/WebKit/public/platform/WebCursorInfo.h"

#if defined(OS_MACOSX)
#include "content/browser/browser_plugin/browser_plugin_popup_menu_helper_mac.h"
#endif

namespace content {

class BrowserPluginGuest::EmbedderWebContentsObserver
    : public WebContentsObserver {
 public:
  explicit EmbedderWebContentsObserver(BrowserPluginGuest* guest)
      : WebContentsObserver(guest->embedder_web_contents()),
        browser_plugin_guest_(guest) {
  }

  virtual ~EmbedderWebContentsObserver() {
  }

  // WebContentsObserver implementation.
  virtual void WasShown() OVERRIDE {
    browser_plugin_guest_->EmbedderVisibilityChanged(true);
  }

  virtual void WasHidden() OVERRIDE {
    browser_plugin_guest_->EmbedderVisibilityChanged(false);
  }

 private:
  BrowserPluginGuest* browser_plugin_guest_;

  DISALLOW_COPY_AND_ASSIGN(EmbedderWebContentsObserver);
};

BrowserPluginGuest::BrowserPluginGuest(
    int instance_id,
    bool has_render_view,
    WebContentsImpl* web_contents)
    : WebContentsObserver(web_contents),
      embedder_web_contents_(NULL),
      instance_id_(instance_id),
      guest_device_scale_factor_(1.0f),
      focused_(false),
      mouse_locked_(false),
      pending_lock_request_(false),
      guest_visible_(false),
      guest_opaque_(true),
      embedder_visible_(true),
      auto_size_enabled_(false),
      copy_request_id_(0),
      has_render_view_(has_render_view),
      last_seen_auto_size_enabled_(false),
      is_in_destruction_(false),
      last_text_input_type_(ui::TEXT_INPUT_TYPE_NONE),
      last_input_mode_(ui::TEXT_INPUT_MODE_DEFAULT),
      last_can_compose_inline_(true),
      delegate_(NULL),
      weak_ptr_factory_(this) {
  DCHECK(web_contents);
}

void BrowserPluginGuest::WillDestroy() {
  is_in_destruction_ = true;
  embedder_web_contents_ = NULL;
  delegate_ = NULL;
}

base::WeakPtr<BrowserPluginGuest> BrowserPluginGuest::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool BrowserPluginGuest::LockMouse(bool allowed) {
  if (!attached() || (mouse_locked_ == allowed))
    return false;

  return embedder_web_contents()->GotResponseToLockMouseRequest(allowed);
}

void BrowserPluginGuest::Destroy() {
  if (!delegate_)
    return;
  delegate_->Destroy();
}

bool BrowserPluginGuest::OnMessageReceivedFromEmbedder(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(BrowserPluginGuest, message)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_CompositorFrameSwappedACK,
                        OnCompositorFrameSwappedACK)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_CopyFromCompositingSurfaceAck,
                        OnCopyFromCompositingSurfaceAck)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_DragStatusUpdate,
                        OnDragStatusUpdate)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_ExecuteEditCommand,
                        OnExecuteEditCommand)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_ExtendSelectionAndDelete,
                        OnExtendSelectionAndDelete)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_HandleInputEvent,
                        OnHandleInputEvent)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_ImeConfirmComposition,
                        OnImeConfirmComposition)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_ImeSetComposition,
                        OnImeSetComposition)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_LockMouse_ACK, OnLockMouseAck)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_PluginDestroyed, OnPluginDestroyed)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_ReclaimCompositorResources,
                        OnReclaimCompositorResources)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_ResizeGuest, OnResizeGuest)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_SetAutoSize, OnSetAutoSize)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_SetEditCommandsForNextKeyEvent,
                        OnSetEditCommandsForNextKeyEvent)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_SetFocus, OnSetFocus)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_SetContentsOpaque,
                        OnSetContentsOpaque)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_SetVisibility, OnSetVisibility)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_UnlockMouse_ACK, OnUnlockMouseAck)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_UpdateGeometry, OnUpdateGeometry)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void BrowserPluginGuest::Initialize(
    const BrowserPluginHostMsg_Attach_Params& params,
    WebContentsImpl* embedder_web_contents,
    const base::DictionaryValue& extra_params) {
  focused_ = params.focused;
  guest_visible_ = params.visible;
  guest_opaque_ = params.opaque;
  guest_window_rect_ = gfx::Rect(params.origin,
                                 params.resize_guest_params.view_size);

  auto_size_enabled_ = params.auto_size_params.enable;
  max_auto_size_ = params.auto_size_params.max_size;
  min_auto_size_ = params.auto_size_params.min_size;

  // Once a BrowserPluginGuest has an embedder WebContents, it's considered to
  // be attached.
  embedder_web_contents_ = embedder_web_contents;

  WebContentsViewGuest* new_view =
      static_cast<WebContentsViewGuest*>(GetWebContents()->GetView());
  new_view->OnGuestInitialized(embedder_web_contents->GetView());

  RendererPreferences* renderer_prefs =
      GetWebContents()->GetMutableRendererPrefs();
  std::string guest_user_agent_override = renderer_prefs->user_agent_override;
  // Copy renderer preferences (and nothing else) from the embedder's
  // WebContents to the guest.
  //
  // For GTK and Aura this is necessary to get proper renderer configuration
  // values for caret blinking interval, colors related to selection and
  // focus.
  *renderer_prefs = *embedder_web_contents_->GetMutableRendererPrefs();
  renderer_prefs->user_agent_override = guest_user_agent_override;

  // We would like the guest to report changes to frame names so that we can
  // update the BrowserPlugin's corresponding 'name' attribute.
  // TODO(fsamuel): Remove this once http://crbug.com/169110 is addressed.
  renderer_prefs->report_frame_name_changes = true;
  // Navigation is disabled in Chrome Apps. We want to make sure guest-initiated
  // navigations still continue to function inside the app.
  renderer_prefs->browser_handles_all_top_level_requests = false;
  // Disable "client blocked" error page for browser plugin.
  renderer_prefs->disable_client_blocked_error_page = true;

  embedder_web_contents_observer_.reset(new EmbedderWebContentsObserver(this));

  OnSetAutoSize(
      instance_id_, params.auto_size_params, params.resize_guest_params);

  // Create a swapped out RenderView for the guest in the embedder render
  // process, so that the embedder can access the guest's window object.
  int guest_routing_id =
      GetWebContents()->CreateSwappedOutRenderView(
          embedder_web_contents_->GetSiteInstance());
  SendMessageToEmbedder(
      new BrowserPluginMsg_GuestContentWindowReady(instance_id_,
                                                   guest_routing_id));

  WebPreferences prefs = GetWebContents()->GetWebkitPrefs();
  prefs.navigate_on_drag_drop = false;
  GetWebContents()->GetRenderViewHost()->UpdateWebkitPreferences(prefs);

  // Enable input method for guest if it's enabled for the embedder.
  if (static_cast<RenderViewHostImpl*>(
      embedder_web_contents_->GetRenderViewHost())->input_method_active()) {
    RenderViewHostImpl* guest_rvh = static_cast<RenderViewHostImpl*>(
        GetWebContents()->GetRenderViewHost());
    guest_rvh->SetInputMethodActive(true);
  }

  // Inform the embedder of the guest's attachment.
  SendMessageToEmbedder(new BrowserPluginMsg_Attach_ACK(instance_id_));
}

BrowserPluginGuest::~BrowserPluginGuest() {
}

// static
BrowserPluginGuest* BrowserPluginGuest::Create(
    int instance_id,
    SiteInstance* guest_site_instance,
    WebContentsImpl* web_contents,
    scoped_ptr<base::DictionaryValue> extra_params,
    BrowserPluginGuest* opener) {
  RecordAction(base::UserMetricsAction("BrowserPlugin.Guest.Create"));
  BrowserPluginGuest* guest = new BrowserPluginGuest(
      instance_id, web_contents->opener() != NULL, web_contents);
  web_contents->SetBrowserPluginGuest(guest);
  WebContents* opener_web_contents = NULL;
  if (opener) {
    opener_web_contents = opener->GetWebContents();
    guest_site_instance = opener_web_contents->GetSiteInstance();
  }
  BrowserPluginGuestDelegate* delegate = NULL;
  GetContentClient()->browser()->GuestWebContentsCreated(
      instance_id,
      guest_site_instance,
      web_contents,
      opener_web_contents,
      &delegate,
      extra_params.Pass());
  if (delegate) {
    delegate->RegisterDestructionCallback(
        base::Bind(&BrowserPluginGuest::WillDestroy,
                   base::Unretained(guest)));
    guest->set_delegate(delegate);
  }
  return guest;
}

// static
bool BrowserPluginGuest::IsGuest(WebContentsImpl* web_contents) {
  return web_contents && web_contents->GetBrowserPluginGuest();
}

// static
bool BrowserPluginGuest::IsGuest(RenderViewHostImpl* render_view_host) {
  return render_view_host && IsGuest(
      static_cast<WebContentsImpl*>(WebContents::FromRenderViewHost(
          render_view_host)));
}

RenderWidgetHostView* BrowserPluginGuest::GetEmbedderRenderWidgetHostView() {
  if (!attached())
    return NULL;
  return embedder_web_contents_->GetRenderWidgetHostView();
}

void BrowserPluginGuest::UpdateVisibility() {
  OnSetVisibility(instance_id_, visible());
}

void BrowserPluginGuest::CopyFromCompositingSurface(
      gfx::Rect src_subrect,
      gfx::Size dst_size,
      const base::Callback<void(bool, const SkBitmap&)>& callback) {
  copy_request_callbacks_.insert(std::make_pair(++copy_request_id_, callback));
  SendMessageToEmbedder(
      new BrowserPluginMsg_CopyFromCompositingSurface(instance_id(),
          copy_request_id_, src_subrect, dst_size));
}

BrowserPluginGuestManager*
BrowserPluginGuest::GetBrowserPluginGuestManager() const {
  return GetWebContents()->GetBrowserContext()->GetGuestManager();
}

// screen.
gfx::Rect BrowserPluginGuest::ToGuestRect(const gfx::Rect& bounds) {
  gfx::Rect guest_rect(bounds);
  guest_rect.Offset(guest_window_rect_.OffsetFromOrigin());
  return guest_rect;
}

void BrowserPluginGuest::EmbedderVisibilityChanged(bool visible) {
  embedder_visible_ = visible;
  UpdateVisibility();
}

void BrowserPluginGuest::PointerLockPermissionResponse(bool allow) {
  SendMessageToEmbedder(
      new BrowserPluginMsg_SetMouseLock(instance_id(), allow));
}

WebContentsImpl* BrowserPluginGuest::GetWebContents() const {
  return static_cast<WebContentsImpl*>(web_contents());
}

gfx::Point BrowserPluginGuest::GetScreenCoordinates(
    const gfx::Point& relative_position) const {
  gfx::Point screen_pos(relative_position);
  screen_pos += guest_window_rect_.OffsetFromOrigin();
  return screen_pos;
}

bool BrowserPluginGuest::InAutoSizeBounds(const gfx::Size& size) const {
  return size.width() <= max_auto_size_.width() &&
      size.height() <= max_auto_size_.height();
}

void BrowserPluginGuest::SendMessageToEmbedder(IPC::Message* msg) {
  if (!attached()) {
    // Some pages such as data URLs, javascript URLs, and about:blank
    // do not load external resources and so they load prior to attachment.
    // As a result, we must save all these IPCs until attachment and then
    // forward them so that the embedder gets a chance to see and process
    // the load events.
    pending_messages_.push_back(linked_ptr<IPC::Message>(msg));
    return;
  }
  msg->set_routing_id(embedder_web_contents_->GetRoutingID());
  embedder_web_contents_->Send(msg);
}

void BrowserPluginGuest::DragSourceEndedAt(int client_x, int client_y,
    int screen_x, int screen_y, blink::WebDragOperation operation) {
  web_contents()->GetRenderViewHost()->DragSourceEndedAt(client_x, client_y,
      screen_x, screen_y, operation);
}

void BrowserPluginGuest::EndSystemDrag() {
  RenderViewHostImpl* guest_rvh = static_cast<RenderViewHostImpl*>(
      GetWebContents()->GetRenderViewHost());
  guest_rvh->DragSourceSystemDragEnded();
}

void BrowserPluginGuest::SendQueuedMessages() {
  if (!attached())
    return;

  while (!pending_messages_.empty()) {
    linked_ptr<IPC::Message> message_ptr = pending_messages_.front();
    pending_messages_.pop_front();
    SendMessageToEmbedder(message_ptr.release());
  }
}

void BrowserPluginGuest::DidCommitProvisionalLoadForFrame(
    int64 frame_id,
    const base::string16& frame_unique_name,
    bool is_main_frame,
    const GURL& url,
    PageTransition transition_type,
    RenderViewHost* render_view_host) {
  RecordAction(base::UserMetricsAction("BrowserPlugin.Guest.DidNavigate"));
}

void BrowserPluginGuest::RenderViewReady() {
  RenderViewHost* rvh = GetWebContents()->GetRenderViewHost();
  // TODO(fsamuel): Investigate whether it's possible to update state earlier
  // here (see http://crbug.com/158151).
  Send(new InputMsg_SetFocus(routing_id(), focused_));
  UpdateVisibility();
  if (auto_size_enabled_)
    rvh->EnableAutoResize(min_auto_size_, max_auto_size_);
  else
    rvh->DisableAutoResize(full_size_);

  OnSetContentsOpaque(instance_id_, guest_opaque_);

  RenderWidgetHostImpl::From(rvh)->set_hung_renderer_delay_ms(
      base::TimeDelta::FromMilliseconds(kHungRendererDelayMs));
}

void BrowserPluginGuest::RenderProcessGone(base::TerminationStatus status) {
  SendMessageToEmbedder(new BrowserPluginMsg_GuestGone(instance_id()));
  switch (status) {
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED:
      RecordAction(base::UserMetricsAction("BrowserPlugin.Guest.Killed"));
      break;
    case base::TERMINATION_STATUS_PROCESS_CRASHED:
      RecordAction(base::UserMetricsAction("BrowserPlugin.Guest.Crashed"));
      break;
    case base::TERMINATION_STATUS_ABNORMAL_TERMINATION:
      RecordAction(
          base::UserMetricsAction("BrowserPlugin.Guest.AbnormalDeath"));
      break;
    default:
      break;
  }
}

// static
bool BrowserPluginGuest::ShouldForwardToBrowserPluginGuest(
    const IPC::Message& message) {
  switch (message.type()) {
    case BrowserPluginHostMsg_CompositorFrameSwappedACK::ID:
    case BrowserPluginHostMsg_CopyFromCompositingSurfaceAck::ID:
    case BrowserPluginHostMsg_DragStatusUpdate::ID:
    case BrowserPluginHostMsg_ExecuteEditCommand::ID:
    case BrowserPluginHostMsg_ExtendSelectionAndDelete::ID:
    case BrowserPluginHostMsg_HandleInputEvent::ID:
    case BrowserPluginHostMsg_ImeConfirmComposition::ID:
    case BrowserPluginHostMsg_ImeSetComposition::ID:
    case BrowserPluginHostMsg_LockMouse_ACK::ID:
    case BrowserPluginHostMsg_PluginDestroyed::ID:
    case BrowserPluginHostMsg_ReclaimCompositorResources::ID:
    case BrowserPluginHostMsg_ResizeGuest::ID:
    case BrowserPluginHostMsg_SetAutoSize::ID:
    case BrowserPluginHostMsg_SetEditCommandsForNextKeyEvent::ID:
    case BrowserPluginHostMsg_SetFocus::ID:
    case BrowserPluginHostMsg_SetContentsOpaque::ID:
    case BrowserPluginHostMsg_SetVisibility::ID:
    case BrowserPluginHostMsg_UnlockMouse_ACK::ID:
    case BrowserPluginHostMsg_UpdateGeometry::ID:
      return true;
    default:
      return false;
  }
}

bool BrowserPluginGuest::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(BrowserPluginGuest, message)
    IPC_MESSAGE_HANDLER(ViewHostMsg_HasTouchEventHandlers,
                        OnHasTouchEventHandlers)
    IPC_MESSAGE_HANDLER(ViewHostMsg_LockMouse, OnLockMouse)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SetCursor, OnSetCursor)
 #if defined(OS_MACOSX)
    // MacOSX creates and populates platform-specific select drop-down menus
    // whereas other platforms merely create a popup window that the guest
    // renderer process paints inside.
    IPC_MESSAGE_HANDLER(ViewHostMsg_ShowPopup, OnShowPopup)
 #endif
    IPC_MESSAGE_HANDLER(ViewHostMsg_ShowWidget, OnShowWidget)
    IPC_MESSAGE_HANDLER(ViewHostMsg_TakeFocus, OnTakeFocus)
    IPC_MESSAGE_HANDLER(ViewHostMsg_TextInputStateChanged,
                        OnTextInputStateChanged)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ImeCancelComposition,
                        OnImeCancelComposition)
#if defined(OS_MACOSX) || defined(USE_AURA)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ImeCompositionRangeChanged,
                        OnImeCompositionRangeChanged)
#endif
    IPC_MESSAGE_HANDLER(ViewHostMsg_UnlockMouse, OnUnlockMouse)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateRect, OnUpdateRect)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void BrowserPluginGuest::Attach(
    WebContentsImpl* embedder_web_contents,
    const BrowserPluginHostMsg_Attach_Params& params,
    const base::DictionaryValue& extra_params) {
  if (attached())
    return;

  if (delegate_)
    delegate_->WillAttach(embedder_web_contents, extra_params);

  // If a RenderView has already been created for this new window, then we need
  // to initialize the browser-side state now so that the RenderFrameHostManager
  // does not create a new RenderView on navigation.
  if (has_render_view_) {
    static_cast<RenderViewHostImpl*>(
        GetWebContents()->GetRenderViewHost())->Init();
    WebContentsViewGuest* new_view =
        static_cast<WebContentsViewGuest*>(GetWebContents()->GetView());
    new_view->CreateViewForWidget(web_contents()->GetRenderViewHost());
  }

  Initialize(params, embedder_web_contents, extra_params);

  SendQueuedMessages();

  if (delegate_)
    delegate_->DidAttach();

  RecordAction(base::UserMetricsAction("BrowserPlugin.Guest.Attached"));
}

void BrowserPluginGuest::OnCompositorFrameSwappedACK(
    int instance_id,
    const FrameHostMsg_CompositorFrameSwappedACK_Params& params) {
  RenderWidgetHostImpl::SendSwapCompositorFrameAck(params.producing_route_id,
                                                   params.output_surface_id,
                                                   params.producing_host_id,
                                                   params.ack);
}

void BrowserPluginGuest::OnDragStatusUpdate(int instance_id,
                                            blink::WebDragStatus drag_status,
                                            const DropData& drop_data,
                                            blink::WebDragOperationsMask mask,
                                            const gfx::Point& location) {
  RenderViewHost* host = GetWebContents()->GetRenderViewHost();
  switch (drag_status) {
    case blink::WebDragStatusEnter:
      embedder_web_contents_->GetBrowserPluginEmbedder()->DragEnteredGuest(
          this);
      host->DragTargetDragEnter(drop_data, location, location, mask, 0);
      break;
    case blink::WebDragStatusOver:
      host->DragTargetDragOver(location, location, mask, 0);
      break;
    case blink::WebDragStatusLeave:
      embedder_web_contents_->GetBrowserPluginEmbedder()->DragLeftGuest(this);
      host->DragTargetDragLeave();
      break;
    case blink::WebDragStatusDrop:
      host->DragTargetDrop(location, location, 0);
      EndSystemDrag();
      break;
    case blink::WebDragStatusUnknown:
      NOTREACHED();
  }
}

void BrowserPluginGuest::OnExecuteEditCommand(int instance_id,
                                              const std::string& name) {
  Send(new InputMsg_ExecuteEditCommand(routing_id(), name, std::string()));
}

void BrowserPluginGuest::OnImeSetComposition(
    int instance_id,
    const std::string& text,
    const std::vector<blink::WebCompositionUnderline>& underlines,
    int selection_start,
    int selection_end) {
  Send(new ViewMsg_ImeSetComposition(routing_id(),
                                     base::UTF8ToUTF16(text), underlines,
                                     selection_start, selection_end));
}

void BrowserPluginGuest::OnImeConfirmComposition(
    int instance_id,
    const std::string& text,
    bool keep_selection) {
  Send(new ViewMsg_ImeConfirmComposition(routing_id(),
                                         base::UTF8ToUTF16(text),
                                         gfx::Range::InvalidRange(),
                                         keep_selection));
}

void BrowserPluginGuest::OnExtendSelectionAndDelete(
    int instance_id,
    int before,
    int after) {
  RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(
      web_contents()->GetFocusedFrame());
  if (rfh)
    rfh->ExtendSelectionAndDelete(before, after);
}

void BrowserPluginGuest::OnReclaimCompositorResources(
    int instance_id,
    const FrameHostMsg_ReclaimCompositorResources_Params& params) {
  RenderWidgetHostImpl::SendReclaimCompositorResources(params.route_id,
                                                       params.output_surface_id,
                                                       params.renderer_host_id,
                                                       params.ack);
}

void BrowserPluginGuest::OnHandleInputEvent(
    int instance_id,
    const gfx::Rect& guest_window_rect,
    const blink::WebInputEvent* event) {
  guest_window_rect_ = guest_window_rect;
  // If the embedder's RWHV is destroyed then that means that the embedder's
  // window has been closed but the embedder's WebContents has not yet been
  // destroyed. Computing screen coordinates of a BrowserPlugin only makes sense
  // if there is a visible embedder.
  if (embedder_web_contents_->GetRenderWidgetHostView()) {
    guest_screen_rect_ = guest_window_rect;
    guest_screen_rect_.Offset(
        embedder_web_contents_->GetRenderWidgetHostView()->
            GetViewBounds().OffsetFromOrigin());
  }
  RenderViewHostImpl* guest_rvh = static_cast<RenderViewHostImpl*>(
      GetWebContents()->GetRenderViewHost());

  if (blink::WebInputEvent::isMouseEventType(event->type)) {
    guest_rvh->ForwardMouseEvent(
        *static_cast<const blink::WebMouseEvent*>(event));
    return;
  }

  if (event->type == blink::WebInputEvent::MouseWheel) {
    guest_rvh->ForwardWheelEvent(
        *static_cast<const blink::WebMouseWheelEvent*>(event));
    return;
  }

  if (blink::WebInputEvent::isKeyboardEventType(event->type)) {
    RenderViewHostImpl* embedder_rvh = static_cast<RenderViewHostImpl*>(
        embedder_web_contents_->GetRenderViewHost());
    if (!embedder_rvh->GetLastKeyboardEvent())
      return;
    NativeWebKeyboardEvent keyboard_event(
        *embedder_rvh->GetLastKeyboardEvent());
    guest_rvh->ForwardKeyboardEvent(keyboard_event);
    return;
  }

  if (blink::WebInputEvent::isTouchEventType(event->type)) {
    guest_rvh->ForwardTouchEventWithLatencyInfo(
        *static_cast<const blink::WebTouchEvent*>(event),
        ui::LatencyInfo());
    return;
  }

  if (blink::WebInputEvent::isGestureEventType(event->type)) {
    guest_rvh->ForwardGestureEvent(
        *static_cast<const blink::WebGestureEvent*>(event));
    return;
  }
}

void BrowserPluginGuest::OnLockMouse(bool user_gesture,
                                     bool last_unlocked_by_target,
                                     bool privileged) {
  if (pending_lock_request_) {
    // Immediately reject the lock because only one pointerLock may be active
    // at a time.
    Send(new ViewMsg_LockMouse_ACK(routing_id(), false));
    return;
  }

  if (!delegate_)
    return;

  pending_lock_request_ = true;

  delegate_->RequestPointerLockPermission(
      user_gesture,
      last_unlocked_by_target,
      base::Bind(&BrowserPluginGuest::PointerLockPermissionResponse,
                 weak_ptr_factory_.GetWeakPtr()));
}

void BrowserPluginGuest::OnLockMouseAck(int instance_id, bool succeeded) {
  Send(new ViewMsg_LockMouse_ACK(routing_id(), succeeded));
  pending_lock_request_ = false;
  if (succeeded)
    mouse_locked_ = true;
}

void BrowserPluginGuest::OnPluginDestroyed(int instance_id) {
  Destroy();
}

void BrowserPluginGuest::OnResizeGuest(
    int instance_id,
    const BrowserPluginHostMsg_ResizeGuest_Params& params) {
  if (!params.size_changed)
    return;
  // BrowserPlugin manages resize flow control itself and does not depend
  // on RenderWidgetHost's mechanisms for flow control, so we reset those flags
  // here. If we are setting the size for the first time before navigating then
  // BrowserPluginGuest does not yet have a RenderViewHost.
  if (GetWebContents()->GetRenderViewHost()) {
    RenderWidgetHostImpl* render_widget_host =
        RenderWidgetHostImpl::From(GetWebContents()->GetRenderViewHost());
    render_widget_host->ResetSizeAndRepaintPendingFlags();

    if (guest_device_scale_factor_ != params.scale_factor) {
      guest_device_scale_factor_ = params.scale_factor;
      render_widget_host->NotifyScreenInfoChanged();
    }
  }
  // When autosize is turned off and as a result there is a layout change, we
  // send a sizechanged event.
  if (!auto_size_enabled_ && last_seen_auto_size_enabled_ &&
      !params.view_size.IsEmpty() && delegate_) {
    delegate_->SizeChanged(last_seen_view_size_, params.view_size);
    last_seen_auto_size_enabled_ = false;
  }
  // Just resize the WebContents and repaint if needed.
  full_size_ = params.view_size;
  if (!params.view_size.IsEmpty())
    GetWebContents()->GetView()->SizeContents(params.view_size);
  if (params.repaint)
    Send(new ViewMsg_Repaint(routing_id(), params.view_size));
}

void BrowserPluginGuest::OnSetFocus(int instance_id, bool focused) {
  focused_ = focused;
  Send(new InputMsg_SetFocus(routing_id(), focused));
  if (!focused && mouse_locked_)
    OnUnlockMouse();

  // Restore the last seen state of text input to the view.
  RenderWidgetHostViewBase* rwhv = static_cast<RenderWidgetHostViewBase*>(
      web_contents()->GetRenderWidgetHostView());
  if (rwhv) {
    ViewHostMsg_TextInputState_Params params;
    params.type = last_text_input_type_;
    params.mode = last_input_mode_;
    params.can_compose_inline = last_can_compose_inline_;
    rwhv->TextInputStateChanged(params);
  }
}

void BrowserPluginGuest::OnSetAutoSize(
    int instance_id,
    const BrowserPluginHostMsg_AutoSize_Params& auto_size_params,
    const BrowserPluginHostMsg_ResizeGuest_Params& resize_guest_params) {
  bool old_auto_size_enabled = auto_size_enabled_;
  gfx::Size old_max_size = max_auto_size_;
  gfx::Size old_min_size = min_auto_size_;
  auto_size_enabled_ = auto_size_params.enable;
  max_auto_size_ = auto_size_params.max_size;
  min_auto_size_ = auto_size_params.min_size;
  if (auto_size_enabled_ && (!old_auto_size_enabled ||
                             (old_max_size != max_auto_size_) ||
                             (old_min_size != min_auto_size_))) {
    RecordAction(
        base::UserMetricsAction("BrowserPlugin.Guest.EnableAutoResize"));
    GetWebContents()->GetRenderViewHost()->EnableAutoResize(
        min_auto_size_, max_auto_size_);
    // TODO(fsamuel): If we're changing autosize parameters, then we force
    // the guest to completely repaint itself.
    // Ideally, we shouldn't need to do this unless |max_auto_size_| has
    // changed.
    // However, even in that case, layout may not change and so we may
    // not get a full frame worth of pixels.
    Send(new ViewMsg_Repaint(routing_id(), max_auto_size_));
  } else if (!auto_size_enabled_ && old_auto_size_enabled) {
    GetWebContents()->GetRenderViewHost()->DisableAutoResize(
        resize_guest_params.view_size);
  }
  OnResizeGuest(instance_id_, resize_guest_params);
}

void BrowserPluginGuest::OnSetEditCommandsForNextKeyEvent(
    int instance_id,
    const std::vector<EditCommand>& edit_commands) {
  Send(new InputMsg_SetEditCommandsForNextKeyEvent(routing_id(),
                                                   edit_commands));
}

void BrowserPluginGuest::OnSetContentsOpaque(int instance_id, bool opaque) {
  guest_opaque_ = opaque;
  Send(new ViewMsg_SetBackgroundOpaque(routing_id(), guest_opaque_));
}

void BrowserPluginGuest::OnSetVisibility(int instance_id, bool visible) {
  guest_visible_ = visible;
  if (embedder_visible_ && guest_visible_)
    GetWebContents()->WasShown();
  else
    GetWebContents()->WasHidden();
}

void BrowserPluginGuest::OnUnlockMouse() {
  SendMessageToEmbedder(
      new BrowserPluginMsg_SetMouseLock(instance_id(), false));
}

void BrowserPluginGuest::OnUnlockMouseAck(int instance_id) {
  // mouse_locked_ could be false here if the lock attempt was cancelled due
  // to window focus, or for various other reasons before the guest was informed
  // of the lock's success.
  if (mouse_locked_)
    Send(new ViewMsg_MouseLockLost(routing_id()));
  mouse_locked_ = false;
}

void BrowserPluginGuest::OnCopyFromCompositingSurfaceAck(
    int instance_id,
    int request_id,
    const SkBitmap& bitmap) {
  CHECK(copy_request_callbacks_.count(request_id));
  if (!copy_request_callbacks_.count(request_id))
    return;
  const CopyRequestCallback& callback = copy_request_callbacks_[request_id];
  callback.Run(!bitmap.empty() && !bitmap.isNull(), bitmap);
  copy_request_callbacks_.erase(request_id);
}

void BrowserPluginGuest::OnUpdateGeometry(int instance_id,
                                          const gfx::Rect& view_rect) {
  // The plugin has moved within the embedder without resizing or the
  // embedder/container's view rect changing.
  guest_window_rect_ = view_rect;
  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
      GetWebContents()->GetRenderViewHost());
  if (rvh)
    rvh->SendScreenRects();
}

void BrowserPluginGuest::OnHasTouchEventHandlers(bool accept) {
  SendMessageToEmbedder(
      new BrowserPluginMsg_ShouldAcceptTouchEvents(instance_id(), accept));
}

void BrowserPluginGuest::OnSetCursor(const WebCursor& cursor) {
  SendMessageToEmbedder(new BrowserPluginMsg_SetCursor(instance_id(), cursor));
}

#if defined(OS_MACOSX)
void BrowserPluginGuest::OnShowPopup(
    const ViewHostMsg_ShowPopup_Params& params) {
  gfx::Rect translated_bounds(params.bounds);
  translated_bounds.Offset(guest_window_rect_.OffsetFromOrigin());
  BrowserPluginPopupMenuHelper popup_menu_helper(
      embedder_web_contents_->GetRenderViewHost(),
      GetWebContents()->GetRenderViewHost());
  popup_menu_helper.ShowPopupMenu(translated_bounds,
                                  params.item_height,
                                  params.item_font_size,
                                  params.selected_item,
                                  params.popup_items,
                                  params.right_aligned,
                                  params.allow_multiple_selection);
}
#endif

void BrowserPluginGuest::OnShowWidget(int route_id,
                                      const gfx::Rect& initial_pos) {
  GetWebContents()->ShowCreatedWidget(route_id, initial_pos);
}

void BrowserPluginGuest::OnTakeFocus(bool reverse) {
  SendMessageToEmbedder(
      new BrowserPluginMsg_AdvanceFocus(instance_id(), reverse));
}

void BrowserPluginGuest::OnUpdateRect(
    const ViewHostMsg_UpdateRect_Params& params) {
  BrowserPluginMsg_UpdateRect_Params relay_params;
  relay_params.view_size = params.view_size;
  relay_params.scale_factor = params.scale_factor;
  relay_params.is_resize_ack = ViewHostMsg_UpdateRect_Flags::is_resize_ack(
      params.flags);

  bool size_changed = last_seen_view_size_ != params.view_size;
  gfx::Size old_size = last_seen_view_size_;
  last_seen_view_size_ = params.view_size;

  if ((auto_size_enabled_ || last_seen_auto_size_enabled_) &&
      size_changed && delegate_) {
    delegate_->SizeChanged(old_size, last_seen_view_size_);
  }
  last_seen_auto_size_enabled_ = auto_size_enabled_;

  SendMessageToEmbedder(
      new BrowserPluginMsg_UpdateRect(instance_id(), relay_params));
}

void BrowserPluginGuest::OnTextInputStateChanged(
    const ViewHostMsg_TextInputState_Params& params) {
  // Save the state of text input so we can restore it on focus.
  last_text_input_type_ = params.type;
  last_input_mode_ = params.mode;
  last_can_compose_inline_ = params.can_compose_inline;

  static_cast<RenderWidgetHostViewBase*>(
      web_contents()->GetRenderWidgetHostView())->TextInputStateChanged(params);
}

void BrowserPluginGuest::OnImeCancelComposition() {
  static_cast<RenderWidgetHostViewBase*>(
      web_contents()->GetRenderWidgetHostView())->ImeCancelComposition();
}

#if defined(OS_MACOSX) || defined(USE_AURA)
void BrowserPluginGuest::OnImeCompositionRangeChanged(
      const gfx::Range& range,
      const std::vector<gfx::Rect>& character_bounds) {
  static_cast<RenderWidgetHostViewBase*>(
      web_contents()->GetRenderWidgetHostView())->ImeCompositionRangeChanged(
          range, character_bounds);
}
#endif

}  // namespace content
