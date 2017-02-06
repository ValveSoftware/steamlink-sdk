// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/render_frame_host_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/hash_tables.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/process/kill.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/accessibility/accessibility_mode_helper.h"
#include "content/browser/accessibility/ax_tree_id_registry.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/browser/bluetooth/web_bluetooth_service_impl.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/download/mhtml_generation_manager.h"
#include "content/browser/frame_host/cross_process_frame_connector.h"
#include "content/browser/frame_host/cross_site_transferring_request.h"
#include "content/browser/frame_host/frame_mojo_shell.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/frame_host/navigation_handle_impl.h"
#include "content/browser/frame_host/navigation_request.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/frame_host/navigator_impl.h"
#include "content/browser/frame_host/render_frame_host_delegate.h"
#include "content/browser/frame_host/render_frame_proxy_host.h"
#include "content/browser/frame_host/render_widget_host_view_child_frame.h"
#include "content/browser/geolocation/geolocation_service_context.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/permissions/permission_service_context.h"
#include "content/browser/permissions/permission_service_impl.h"
#include "content/browser/presentation/presentation_service_impl.h"
#include "content/browser/renderer_host/input/input_router_impl.h"
#include "content/browser/renderer_host/input/timeout_monitor.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_delegate_view.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/wake_lock/wake_lock_service_context.h"
#include "content/browser/webui/web_ui_controller_factory_registry.h"
#include "content/common/accessibility_messages.h"
#include "content/common/frame_messages.h"
#include "content/common/input_messages.h"
#include "content/common/inter_process_time_ticks_converter.h"
#include "content/common/navigation_params.h"
#include "content/common/site_isolation_policy.h"
#include "content/common/swapped_out_messages.h"
#include "content/public/browser/ax_event_notification_details.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_plugin_guest_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/permission_manager.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/stream_handle.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/file_chooser_file_info.h"
#include "content/public/common/file_chooser_params.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "device/vibration/vibration_manager_impl.h"
#include "services/shell/public/cpp/interface_provider.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/gfx/geometry/quad_f.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "content/browser/mojo/service_registrar_android.h"
#endif

#if defined(OS_MACOSX)
#include "content/browser/frame_host/popup_menu_helper_mac.h"
#endif

#if defined(ENABLE_WEBVR)
#include "base/command_line.h"
#include "content/public/common/content_switches.h"
#include "device/vr/vr_device_manager.h" // nogncheck
#endif

using base::TimeDelta;

namespace content {

namespace {

// The next value to use for the accessibility reset token.
int g_next_accessibility_reset_token = 1;

// The next value to use for the javascript callback id.
int g_next_javascript_callback_id = 1;

// Whether to allow injecting javascript into any kind of frame (for Android
// WebView).
bool g_allow_injecting_javascript = false;

// The (process id, routing id) pair that identifies one RenderFrame.
typedef std::pair<int32_t, int32_t> RenderFrameHostID;
typedef base::hash_map<RenderFrameHostID, RenderFrameHostImpl*>
    RoutingIDFrameMap;
base::LazyInstance<RoutingIDFrameMap> g_routing_id_frame_map =
    LAZY_INSTANCE_INITIALIZER;

// Translate a WebKit text direction into a base::i18n one.
base::i18n::TextDirection WebTextDirectionToChromeTextDirection(
    blink::WebTextDirection dir) {
  switch (dir) {
    case blink::WebTextDirectionLeftToRight:
      return base::i18n::LEFT_TO_RIGHT;
    case blink::WebTextDirectionRightToLeft:
      return base::i18n::RIGHT_TO_LEFT;
    default:
      NOTREACHED();
      return base::i18n::UNKNOWN_DIRECTION;
  }
}

// Ensure that we reset nav_entry_id_ in OnDidCommitProvisionalLoad if any of
// the validations fail and lead to an early return.  Call disable() once we
// know the commit will be successful.  Resetting nav_entry_id_ avoids acting on
// any UpdateState or UpdateTitle messages after an ignored commit.
class ScopedCommitStateResetter {
 public:
  explicit ScopedCommitStateResetter(RenderFrameHostImpl* render_frame_host)
      : render_frame_host_(render_frame_host), disabled_(false) {}

  ~ScopedCommitStateResetter() {
    if (!disabled_) {
      render_frame_host_->set_nav_entry_id(0);
    }
  }

  void disable() { disabled_ = true; }

 private:
  RenderFrameHostImpl* render_frame_host_;
  bool disabled_;
};

void GrantFileAccess(int child_id,
                     const std::vector<base::FilePath>& file_paths) {
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();

  for (const auto& file : file_paths) {
    if (!policy->CanReadFile(child_id, file))
      policy->GrantReadFile(child_id, file);
  }
}

}  // namespace

// static
RenderFrameHost* RenderFrameHost::FromID(int render_process_id,
                                         int render_frame_id) {
  return RenderFrameHostImpl::FromID(render_process_id, render_frame_id);
}

#if defined(OS_ANDROID) || defined(TOOLKIT_QT)
// static
void RenderFrameHost::AllowInjectingJavaScriptForAndroidWebView() {
  g_allow_injecting_javascript = true;
}
#endif

// static
RenderFrameHostImpl* RenderFrameHostImpl::FromID(int process_id,
                                                 int routing_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RoutingIDFrameMap* frames = g_routing_id_frame_map.Pointer();
  RoutingIDFrameMap::iterator it = frames->find(
      RenderFrameHostID(process_id, routing_id));
  return it == frames->end() ? NULL : it->second;
}

// static
RenderFrameHost* RenderFrameHost::FromAXTreeID(
    int ax_tree_id) {
  return RenderFrameHostImpl::FromAXTreeID(ax_tree_id);
}

// static
RenderFrameHostImpl* RenderFrameHostImpl::FromAXTreeID(
    AXTreeIDRegistry::AXTreeID ax_tree_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  AXTreeIDRegistry::FrameID frame_id =
      AXTreeIDRegistry::GetInstance()->GetFrameID(ax_tree_id);
  return RenderFrameHostImpl::FromID(frame_id.first, frame_id.second);
}

RenderFrameHostImpl::RenderFrameHostImpl(SiteInstance* site_instance,
                                         RenderViewHostImpl* render_view_host,
                                         RenderFrameHostDelegate* delegate,
                                         RenderWidgetHostDelegate* rwh_delegate,
                                         FrameTree* frame_tree,
                                         FrameTreeNode* frame_tree_node,
                                         int32_t routing_id,
                                         int32_t widget_routing_id,
                                         bool hidden)
    : render_view_host_(render_view_host),
      delegate_(delegate),
      site_instance_(static_cast<SiteInstanceImpl*>(site_instance)),
      process_(site_instance->GetProcess()),
      cross_process_frame_connector_(NULL),
      render_frame_proxy_host_(NULL),
      frame_tree_(frame_tree),
      frame_tree_node_(frame_tree_node),
      parent_(nullptr),
      render_widget_host_(nullptr),
      routing_id_(routing_id),
      is_waiting_for_swapout_ack_(false),
      render_frame_created_(false),
      navigations_suspended_(false),
      is_waiting_for_beforeunload_ack_(false),
      unload_ack_is_for_navigation_(false),
      is_loading_(false),
      pending_commit_(false),
      nav_entry_id_(0),
      accessibility_reset_token_(0),
      accessibility_reset_count_(0),
      browser_plugin_embedder_ax_tree_id_(AXTreeIDRegistry::kNoAXTreeID),
      no_create_browser_accessibility_manager_for_testing_(false),
      web_ui_type_(WebUI::kNoWebUI),
      pending_web_ui_type_(WebUI::kNoWebUI),
      should_reuse_web_ui_(false),
      last_navigation_lofi_state_(LOFI_UNSPECIFIED),
      frame_host_binding_(this),
      weak_ptr_factory_(this) {
  frame_tree_->AddRenderViewHostRef(render_view_host_);
  GetProcess()->AddRoute(routing_id_, this);
  g_routing_id_frame_map.Get().insert(std::make_pair(
      RenderFrameHostID(GetProcess()->GetID(), routing_id_),
      this));
  site_instance_->AddObserver(this);
  GetSiteInstance()->IncrementActiveFrameCount();

  if (frame_tree_node_->parent()) {
    // Keep track of the parent RenderFrameHost, which shouldn't change even if
    // this RenderFrameHost is on the pending deletion list and the parent
    // FrameTreeNode has changed its current RenderFrameHost.
    parent_ = frame_tree_node_->parent()->current_frame_host();

    // New child frames should inherit the nav_entry_id of their parent.
    set_nav_entry_id(
        frame_tree_node_->parent()->current_frame_host()->nav_entry_id());
  }

  SetUpMojoIfNeeded();
  swapout_event_monitor_timeout_.reset(new TimeoutMonitor(base::Bind(
      &RenderFrameHostImpl::OnSwappedOut, weak_ptr_factory_.GetWeakPtr())));

  if (widget_routing_id != MSG_ROUTING_NONE) {
    // TODO(avi): Once RenderViewHostImpl has-a RenderWidgetHostImpl, the main
    // render frame should probably start owning the RenderWidgetHostImpl,
    // so this logic checking for an already existing RWHI should be removed.
    // https://crbug.com/545684
    render_widget_host_ =
        RenderWidgetHostImpl::FromID(GetProcess()->GetID(), widget_routing_id);
    if (!render_widget_host_) {
      DCHECK(frame_tree_node->parent());
      render_widget_host_ = new RenderWidgetHostImpl(rwh_delegate, GetProcess(),
                                                     widget_routing_id, hidden);
      render_widget_host_->set_owned_by_render_frame_host(true);
    } else {
      DCHECK(!render_widget_host_->owned_by_render_frame_host());
    }
    InputRouterImpl* ir =
        static_cast<InputRouterImpl*>(render_widget_host_->input_router());
    ir->SetFrameTreeNodeId(frame_tree_node_->frame_tree_node_id());
  }
}

RenderFrameHostImpl::~RenderFrameHostImpl() {
  // Release the WebUI instances before all else as the WebUI may accesses the
  // RenderFrameHost during cleanup.
  ClearAllWebUI();

  GetProcess()->RemoveRoute(routing_id_);
  g_routing_id_frame_map.Get().erase(
      RenderFrameHostID(GetProcess()->GetID(), routing_id_));

  site_instance_->RemoveObserver(this);

  if (delegate_ && render_frame_created_)
    delegate_->RenderFrameDeleted(this);

  // If this was the last active frame in the SiteInstance, the
  // DecrementActiveFrameCount call will trigger the deletion of the
  // SiteInstance's proxies.
  GetSiteInstance()->DecrementActiveFrameCount();

  // If this RenderFrameHost is swapping with a RenderFrameProxyHost, the
  // RenderFrame will already be deleted in the renderer process. Main frame
  // RenderFrames will be cleaned up as part of deleting its RenderView. In all
  // other cases, the RenderFrame should be cleaned up (if it exists).
  if (is_active() && !frame_tree_node_->IsMainFrame() && render_frame_created_)
    Send(new FrameMsg_Delete(routing_id_));

  // Null out the swapout timer; in crash dumps this member will be null only if
  // the dtor has run.  (It may also be null in tests.)
  swapout_event_monitor_timeout_.reset();

  for (const auto& iter: visual_state_callbacks_) {
    iter.second.Run(false);
  }

  if (render_widget_host_ &&
      render_widget_host_->owned_by_render_frame_host()) {
    // Shutdown causes the RenderWidgetHost to delete itself.
    render_widget_host_->ShutdownAndDestroyWidget(true);
  }

  // Notify the FrameTree that this RFH is going away, allowing it to shut down
  // the corresponding RenderViewHost if it is no longer needed.
  frame_tree_->ReleaseRenderViewHostRef(render_view_host_);
}

int RenderFrameHostImpl::GetRoutingID() {
  return routing_id_;
}

AXTreeIDRegistry::AXTreeID RenderFrameHostImpl::GetAXTreeID() {
  return AXTreeIDRegistry::GetInstance()->GetOrCreateAXTreeID(
      GetProcess()->GetID(), routing_id_);
}

SiteInstanceImpl* RenderFrameHostImpl::GetSiteInstance() {
  return site_instance_.get();
}

RenderProcessHost* RenderFrameHostImpl::GetProcess() {
  return process_;
}

RenderFrameHostImpl* RenderFrameHostImpl::GetParent() {
  return parent_;
}

int RenderFrameHostImpl::GetFrameTreeNodeId() {
  return frame_tree_node_->frame_tree_node_id();
}

const std::string& RenderFrameHostImpl::GetFrameName() {
  return frame_tree_node_->frame_name();
}

bool RenderFrameHostImpl::IsCrossProcessSubframe() {
  if (!parent_)
    return false;
  return GetSiteInstance() != parent_->GetSiteInstance();
}

const GURL& RenderFrameHostImpl::GetLastCommittedURL() {
  return last_committed_url();
}

url::Origin RenderFrameHostImpl::GetLastCommittedOrigin() {
  // Origin is stored per-FTN, so it's incorrect to call for a non-current RFH.
  CHECK(this == frame_tree_node_->current_frame_host());
  return frame_tree_node_->current_origin();
}

gfx::NativeView RenderFrameHostImpl::GetNativeView() {
  RenderWidgetHostView* view = render_view_host_->GetWidget()->GetView();
  if (!view)
    return NULL;
  return view->GetNativeView();
}

void RenderFrameHostImpl::AddMessageToConsole(ConsoleMessageLevel level,
                                              const std::string& message) {
  Send(new FrameMsg_AddMessageToConsole(routing_id_, level, message));
}

void RenderFrameHostImpl::ExecuteJavaScript(
    const base::string16& javascript) {
  CHECK(CanExecuteJavaScript());
  Send(new FrameMsg_JavaScriptExecuteRequest(routing_id_,
                                             javascript,
                                             0, false));
}

void RenderFrameHostImpl::ExecuteJavaScript(
     const base::string16& javascript,
     const JavaScriptResultCallback& callback) {
  CHECK(CanExecuteJavaScript());
  int key = g_next_javascript_callback_id++;
  Send(new FrameMsg_JavaScriptExecuteRequest(routing_id_,
                                             javascript,
                                             key, true));
  javascript_callbacks_.insert(std::make_pair(key, callback));
}

void RenderFrameHostImpl::ExecuteJavaScriptForTests(
    const base::string16& javascript) {
  Send(new FrameMsg_JavaScriptExecuteRequestForTests(routing_id_,
                                                     javascript,
                                                     0, false, false));
}

void RenderFrameHostImpl::ExecuteJavaScriptForTests(
     const base::string16& javascript,
     const JavaScriptResultCallback& callback) {
  int key = g_next_javascript_callback_id++;
  Send(new FrameMsg_JavaScriptExecuteRequestForTests(routing_id_, javascript,
                                                     key, true, false));
  javascript_callbacks_.insert(std::make_pair(key, callback));
}


void RenderFrameHostImpl::ExecuteJavaScriptWithUserGestureForTests(
    const base::string16& javascript) {
  Send(new FrameMsg_JavaScriptExecuteRequestForTests(routing_id_,
                                                     javascript,
                                                     0, false, true));
}

void RenderFrameHostImpl::ExecuteJavaScriptInIsolatedWorld(
    const base::string16& javascript,
    const JavaScriptResultCallback& callback,
    int world_id) {
  if (world_id <= ISOLATED_WORLD_ID_GLOBAL ||
      world_id > ISOLATED_WORLD_ID_MAX) {
    // Return if the world_id is not valid.
    NOTREACHED();
    return;
  }

  int key = 0;
  bool request_reply = false;
  if (!callback.is_null()) {
    request_reply = true;
    key = g_next_javascript_callback_id++;
    javascript_callbacks_.insert(std::make_pair(key, callback));
  }

  Send(new FrameMsg_JavaScriptExecuteRequestInIsolatedWorld(
      routing_id_, javascript, key, request_reply, world_id));
}

void RenderFrameHostImpl::CopyImageAt(int x, int y) {
  Send(new FrameMsg_CopyImageAt(routing_id_, x, y));
}

void RenderFrameHostImpl::SaveImageAt(int x, int y) {
  Send(new FrameMsg_SaveImageAt(routing_id_, x, y));
}

RenderViewHost* RenderFrameHostImpl::GetRenderViewHost() {
  return render_view_host_;
}

shell::InterfaceRegistry* RenderFrameHostImpl::GetInterfaceRegistry() {
  return interface_registry_.get();
}

shell::InterfaceProvider* RenderFrameHostImpl::GetRemoteInterfaces() {
  return remote_interfaces_.get();
}

blink::WebPageVisibilityState RenderFrameHostImpl::GetVisibilityState() {
  // Works around the crashes seen in https://crbug.com/501863, where the
  // active WebContents from a browser iterator may contain a render frame
  // detached from the frame tree. This tries to find a RenderWidgetHost
  // attached to an ancestor frame, and defaults to visibility hidden if
  // it fails.
  // TODO(yfriedman, peter): Ideally this would never be called on an
  // unattached frame and we could omit this check. See
  // https://crbug.com/615867.
  RenderFrameHostImpl* frame = this;
  while (frame) {
    if (frame->render_widget_host_)
      break;
    frame = frame->GetParent();
  }
  if (!frame)
    return blink::WebPageVisibilityStateHidden;

  blink::WebPageVisibilityState visibility_state =
      GetRenderWidgetHost()->is_hidden() ? blink::WebPageVisibilityStateHidden
                                         : blink::WebPageVisibilityStateVisible;
  GetContentClient()->browser()->OverridePageVisibilityState(this,
                                                             &visibility_state);
  return visibility_state;
}

bool RenderFrameHostImpl::Send(IPC::Message* message) {
  if (IPC_MESSAGE_ID_CLASS(message->type()) == InputMsgStart) {
    return render_view_host_->GetWidget()->input_router()->SendInput(
        base::WrapUnique(message));
  }

  return GetProcess()->Send(message);
}

bool RenderFrameHostImpl::OnMessageReceived(const IPC::Message &msg) {
  // Only process messages if the RenderFrame is alive.
  if (!render_frame_created_)
    return false;

  // This message map is for handling internal IPC messages which should not
  // be dispatched to other objects.
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderFrameHostImpl, msg)
    // This message is synthetic and doesn't come from RenderFrame, but from
    // RenderProcessHost.
    IPC_MESSAGE_HANDLER(FrameHostMsg_RenderProcessGone, OnRenderProcessGone)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  // Internal IPCs should not be leaked outside of this object, so return
  // early.
  if (handled)
    return true;

  if (delegate_->OnMessageReceived(this, msg))
    return true;

  RenderFrameProxyHost* proxy =
      frame_tree_node_->render_manager()->GetProxyToParent();
  if (proxy && proxy->cross_process_frame_connector() &&
      proxy->cross_process_frame_connector()->OnMessageReceived(msg))
    return true;

  handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderFrameHostImpl, msg)
    IPC_MESSAGE_HANDLER(FrameHostMsg_AddMessageToConsole, OnAddMessageToConsole)
    IPC_MESSAGE_HANDLER(FrameHostMsg_Detach, OnDetach)
    IPC_MESSAGE_HANDLER(FrameHostMsg_FrameFocused, OnFrameFocused)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DidStartProvisionalLoad,
                        OnDidStartProvisionalLoad)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DidFailProvisionalLoadWithError,
                        OnDidFailProvisionalLoadWithError)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DidFailLoadWithError,
                        OnDidFailLoadWithError)
    IPC_MESSAGE_HANDLER_GENERIC(FrameHostMsg_DidCommitProvisionalLoad,
                                OnDidCommitProvisionalLoad(msg))
    IPC_MESSAGE_HANDLER(FrameHostMsg_UpdateState, OnUpdateState)
    IPC_MESSAGE_HANDLER(FrameHostMsg_OpenURL, OnOpenURL)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DocumentOnLoadCompleted,
                        OnDocumentOnLoadCompleted)
    IPC_MESSAGE_HANDLER(FrameHostMsg_BeforeUnload_ACK, OnBeforeUnloadACK)
    IPC_MESSAGE_HANDLER(FrameHostMsg_SwapOut_ACK, OnSwapOutACK)
    IPC_MESSAGE_HANDLER(FrameHostMsg_ContextMenu, OnContextMenu)
    IPC_MESSAGE_HANDLER(FrameHostMsg_JavaScriptExecuteResponse,
                        OnJavaScriptExecuteResponse)
    IPC_MESSAGE_HANDLER(FrameHostMsg_VisualStateResponse,
                        OnVisualStateResponse)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(FrameHostMsg_RunJavaScriptMessage,
                                    OnRunJavaScriptMessage)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(FrameHostMsg_RunBeforeUnloadConfirm,
                                    OnRunBeforeUnloadConfirm)
    IPC_MESSAGE_HANDLER(FrameHostMsg_RunFileChooser, OnRunFileChooser)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DidAccessInitialDocument,
                        OnDidAccessInitialDocument)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DidChangeOpener, OnDidChangeOpener)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DidChangeName, OnDidChangeName)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DidAddContentSecurityPolicy,
                        OnDidAddContentSecurityPolicy)
    IPC_MESSAGE_HANDLER(FrameHostMsg_EnforceInsecureRequestPolicy,
                        OnEnforceInsecureRequestPolicy)
    IPC_MESSAGE_HANDLER(FrameHostMsg_UpdateToUniqueOrigin,
                        OnUpdateToUniqueOrigin)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DidAssignPageId, OnDidAssignPageId)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DidChangeSandboxFlags,
                        OnDidChangeSandboxFlags)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DidChangeFrameOwnerProperties,
                        OnDidChangeFrameOwnerProperties)
    IPC_MESSAGE_HANDLER(FrameHostMsg_UpdateTitle, OnUpdateTitle)
    IPC_MESSAGE_HANDLER(FrameHostMsg_UpdateEncoding, OnUpdateEncoding)
    IPC_MESSAGE_HANDLER(FrameHostMsg_BeginNavigation,
                        OnBeginNavigation)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DispatchLoad, OnDispatchLoad)
    IPC_MESSAGE_HANDLER(FrameHostMsg_TextSurroundingSelectionResponse,
                        OnTextSurroundingSelectionResponse)
    IPC_MESSAGE_HANDLER(AccessibilityHostMsg_Events, OnAccessibilityEvents)
    IPC_MESSAGE_HANDLER(AccessibilityHostMsg_LocationChanges,
                        OnAccessibilityLocationChanges)
    IPC_MESSAGE_HANDLER(AccessibilityHostMsg_FindInPageResult,
                        OnAccessibilityFindInPageResult)
    IPC_MESSAGE_HANDLER(AccessibilityHostMsg_ChildFrameHitTestResult,
                        OnAccessibilityChildFrameHitTestResult)
    IPC_MESSAGE_HANDLER(AccessibilityHostMsg_SnapshotResponse,
                        OnAccessibilitySnapshotResponse)
    IPC_MESSAGE_HANDLER(FrameHostMsg_ToggleFullscreen, OnToggleFullscreen)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DidStartLoading, OnDidStartLoading)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DidStopLoading, OnDidStopLoading)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DidChangeLoadProgress,
                        OnDidChangeLoadProgress)
    IPC_MESSAGE_HANDLER(FrameHostMsg_SerializeAsMHTMLResponse,
                        OnSerializeAsMHTMLResponse)
#if defined(USE_EXTERNAL_POPUP_MENU)
    IPC_MESSAGE_HANDLER(FrameHostMsg_ShowPopup, OnShowPopup)
    IPC_MESSAGE_HANDLER(FrameHostMsg_HidePopup, OnHidePopup)
#endif
  IPC_END_MESSAGE_MAP()

  // No further actions here, since we may have been deleted.
  return handled;
}

void RenderFrameHostImpl::AccessibilitySetFocus(int object_id) {
  Send(new AccessibilityMsg_SetFocus(routing_id_, object_id));
}

void RenderFrameHostImpl::AccessibilityDoDefaultAction(int object_id) {
  Send(new AccessibilityMsg_DoDefaultAction(routing_id_, object_id));
}

void RenderFrameHostImpl::AccessibilityShowContextMenu(int acc_obj_id) {
  Send(new AccessibilityMsg_ShowContextMenu(routing_id_, acc_obj_id));
}

void RenderFrameHostImpl::AccessibilityScrollToMakeVisible(
    int acc_obj_id, const gfx::Rect& subfocus) {
  Send(new AccessibilityMsg_ScrollToMakeVisible(
      routing_id_, acc_obj_id, subfocus));
}

void RenderFrameHostImpl::AccessibilityScrollToPoint(
    int acc_obj_id, const gfx::Point& point) {
  Send(new AccessibilityMsg_ScrollToPoint(
      routing_id_, acc_obj_id, point));
}

void RenderFrameHostImpl::AccessibilitySetScrollOffset(
    int acc_obj_id, const gfx::Point& offset) {
  Send(new AccessibilityMsg_SetScrollOffset(
      routing_id_, acc_obj_id, offset));
}

void RenderFrameHostImpl::AccessibilitySetSelection(int anchor_object_id,
                                                    int anchor_offset,
                                                    int focus_object_id,
                                                    int focus_offset) {
  Send(new AccessibilityMsg_SetSelection(routing_id_,
                                         anchor_object_id,
                                         anchor_offset,
                                         focus_object_id,
                                         focus_offset));
}

void RenderFrameHostImpl::AccessibilitySetValue(
    int object_id, const base::string16& value) {
  Send(new AccessibilityMsg_SetValue(routing_id_, object_id, value));
}

bool RenderFrameHostImpl::AccessibilityViewHasFocus() const {
  RenderWidgetHostView* view = render_view_host_->GetWidget()->GetView();
  if (view)
    return view->HasFocus();
  return false;
}

gfx::Rect RenderFrameHostImpl::AccessibilityGetViewBounds() const {
  RenderWidgetHostView* view = render_view_host_->GetWidget()->GetView();
  if (view)
    return view->GetViewBounds();
  return gfx::Rect();
}

gfx::Point RenderFrameHostImpl::AccessibilityOriginInScreen(
    const gfx::Rect& bounds) const {
  RenderWidgetHostViewBase* view = static_cast<RenderWidgetHostViewBase*>(
      render_view_host_->GetWidget()->GetView());
  if (view)
    return view->AccessibilityOriginInScreen(bounds);
  return gfx::Point();
}

gfx::Rect RenderFrameHostImpl::AccessibilityTransformToRootCoordSpace(
    const gfx::Rect& bounds) {
  RenderWidgetHostViewBase* view =
      static_cast<RenderWidgetHostViewBase*>(GetView());
  gfx::Point p1 = view->TransformPointToRootCoordSpace(bounds.origin());
  gfx::Point p2 = view->TransformPointToRootCoordSpace(bounds.top_right());
  gfx::Point p3 = view->TransformPointToRootCoordSpace(bounds.bottom_right());
  gfx::Point p4 = view->TransformPointToRootCoordSpace(bounds.bottom_left());
  gfx::QuadF transformed_quad = gfx::QuadF(
      gfx::PointF(p1), gfx::PointF(p2), gfx::PointF(p3), gfx::PointF(p4));
  gfx::RectF new_bounds = transformed_quad.BoundingBox();
  return gfx::Rect(new_bounds.x(), new_bounds.y(),
                   new_bounds.width(), new_bounds.height());
}

SiteInstance* RenderFrameHostImpl::AccessibilityGetSiteInstance() {
  return GetSiteInstance();
}

void RenderFrameHostImpl::AccessibilityHitTest(const gfx::Point& point) {
  Send(new AccessibilityMsg_HitTest(routing_id_, point));
}

void RenderFrameHostImpl::AccessibilitySetAccessibilityFocus(int acc_obj_id) {
  Send(new AccessibilityMsg_SetAccessibilityFocus(routing_id_, acc_obj_id));
}

void RenderFrameHostImpl::AccessibilityReset() {
  accessibility_reset_token_ = g_next_accessibility_reset_token++;
  Send(new AccessibilityMsg_Reset(routing_id_, accessibility_reset_token_));
}

void RenderFrameHostImpl::AccessibilityFatalError() {
  browser_accessibility_manager_.reset(NULL);
  if (accessibility_reset_token_)
    return;

  accessibility_reset_count_++;
  if (accessibility_reset_count_ >= kMaxAccessibilityResets) {
    Send(new AccessibilityMsg_FatalError(routing_id_));
  } else {
    accessibility_reset_token_ = g_next_accessibility_reset_token++;
    UMA_HISTOGRAM_COUNTS("Accessibility.FrameResetCount", 1);
    Send(new AccessibilityMsg_Reset(routing_id_, accessibility_reset_token_));
  }
}

gfx::AcceleratedWidget
    RenderFrameHostImpl::AccessibilityGetAcceleratedWidget() {
  // Only the main frame's current frame host is connected to the native
  // widget tree for accessibility, so return null if this is queried on
  // any other frame.
  if (frame_tree_node()->parent() ||
      frame_tree_node()->current_frame_host() != this) {
    return gfx::kNullAcceleratedWidget;
  }

  RenderWidgetHostViewBase* view = static_cast<RenderWidgetHostViewBase*>(
      render_view_host_->GetWidget()->GetView());
  if (view)
    return view->AccessibilityGetAcceleratedWidget();
  return gfx::kNullAcceleratedWidget;
}

gfx::NativeViewAccessible
    RenderFrameHostImpl::AccessibilityGetNativeViewAccessible() {
  RenderWidgetHostViewBase* view = static_cast<RenderWidgetHostViewBase*>(
      render_view_host_->GetWidget()->GetView());
  if (view)
    return view->AccessibilityGetNativeViewAccessible();
  return NULL;
}

void RenderFrameHostImpl::RenderProcessGone(SiteInstanceImpl* site_instance) {
  DCHECK_EQ(site_instance_.get(), site_instance);

  // The renderer process is gone, so this frame can no longer be loading.
  ResetLoadingState();

  // Any future UpdateState or UpdateTitle messages from this or a recreated
  // process should be ignored until the next commit.
  set_nav_entry_id(0);
}

bool RenderFrameHostImpl::CreateRenderFrame(int proxy_routing_id,
                                            int opener_routing_id,
                                            int parent_routing_id,
                                            int previous_sibling_routing_id) {
  TRACE_EVENT0("navigation", "RenderFrameHostImpl::CreateRenderFrame");
  DCHECK(!IsRenderFrameLive()) << "Creating frame twice";

  // The process may (if we're sharing a process with another host that already
  // initialized it) or may not (we have our own process or the old process
  // crashed) have been initialized. Calling Init multiple times will be
  // ignored, so this is safe.
  if (!GetProcess()->Init())
    return false;

  DCHECK(GetProcess()->HasConnection());

  FrameMsg_NewFrame_Params params;
  params.routing_id = routing_id_;
  params.proxy_routing_id = proxy_routing_id;
  params.opener_routing_id = opener_routing_id;
  params.parent_routing_id = parent_routing_id;
  params.previous_sibling_routing_id = previous_sibling_routing_id;
  params.replication_state = frame_tree_node()->current_replication_state();

  // Normally, the replication state contains effective sandbox flags,
  // excluding flags that were updated but have not taken effect.  However, a
  // new RenderFrame should use the pending sandbox flags, since it is being
  // created as part of the navigation that will commit these flags. (I.e., the
  // RenderFrame needs to know the flags to use when initializing the new
  // document once it commits).
  params.replication_state.sandbox_flags =
      frame_tree_node()->pending_sandbox_flags();

  params.frame_owner_properties = frame_tree_node()->frame_owner_properties();

  if (render_widget_host_) {
    params.widget_params.routing_id = render_widget_host_->GetRoutingID();
    params.widget_params.hidden = render_widget_host_->is_hidden();
  } else {
    // MSG_ROUTING_NONE will prevent a new RenderWidget from being created in
    // the renderer process.
    params.widget_params.routing_id = MSG_ROUTING_NONE;
    params.widget_params.hidden = true;
  }

  Send(new FrameMsg_NewFrame(params));

  // The RenderWidgetHost takes ownership of its view. It is tied to the
  // lifetime of the current RenderProcessHost for this RenderFrameHost.
  // TODO(avi): This will need to change to initialize a
  // RenderWidgetHostViewAura for the main frame once RenderViewHostImpl has-a
  // RenderWidgetHostImpl. https://crbug.com/545684
  if (parent_routing_id != MSG_ROUTING_NONE && render_widget_host_) {
    RenderWidgetHostView* rwhv =
        new RenderWidgetHostViewChildFrame(render_widget_host_);
    rwhv->Hide();
  }

  if (proxy_routing_id != MSG_ROUTING_NONE) {
    RenderFrameProxyHost* proxy = RenderFrameProxyHost::FromID(
        GetProcess()->GetID(), proxy_routing_id);
    // We have also created a RenderFrameProxy in FrameMsg_NewFrame above, so
    // remember that.
    proxy->set_render_frame_proxy_created(true);
  }

  // The renderer now has a RenderFrame for this RenderFrameHost.  Note that
  // this path is only used for out-of-process iframes.  Main frame RenderFrames
  // are created with their RenderView, and same-site iframes are created at the
  // time of OnCreateChildFrame.
  SetRenderFrameCreated(true);

  return true;
}

void RenderFrameHostImpl::SetRenderFrameCreated(bool created) {
  bool was_created = render_frame_created_;
  render_frame_created_ = created;

  // If the current status is different than the new status, the delegate
  // needs to be notified.
  if (delegate_ && (created != was_created)) {
    if (created)
      delegate_->RenderFrameCreated(this);
    else
      delegate_->RenderFrameDeleted(this);
  }

  if (created && render_widget_host_)
    render_widget_host_->InitForFrame();
}

void RenderFrameHostImpl::Init() {
  ResourceDispatcherHost::ResumeBlockedRequestsForFrameFromUI(this);
}

void RenderFrameHostImpl::OnAddMessageToConsole(
    int32_t level,
    const base::string16& message,
    int32_t line_no,
    const base::string16& source_id) {
  if (delegate_->AddMessageToConsole(level, message, line_no, source_id))
    return;

  // Pass through log level only on WebUI pages to limit console spew.
  const bool is_web_ui =
      HasWebUIScheme(delegate_->GetMainFrameLastCommittedURL());
  const int32_t resolved_level = is_web_ui ? level : ::logging::LOG_INFO;

  // LogMessages can be persisted so this shouldn't be logged in incognito mode.
  // This rule is not applied to WebUI pages, because source code of WebUI is a
  // part of Chrome source code, and we want to treat messages from WebUI the
  // same way as we treat log messages from native code.
  if (::logging::GetMinLogLevel() <= resolved_level &&
      (is_web_ui ||
       !GetSiteInstance()->GetBrowserContext()->IsOffTheRecord())) {
    logging::LogMessage("CONSOLE", line_no, resolved_level).stream()
        << "\"" << message << "\", source: " << source_id << " (" << line_no
        << ")";
  }
}

void RenderFrameHostImpl::OnCreateChildFrame(
    int new_routing_id,
    blink::WebTreeScopeType scope,
    const std::string& frame_name,
    const std::string& frame_unique_name,
    blink::WebSandboxFlags sandbox_flags,
    const blink::WebFrameOwnerProperties& frame_owner_properties) {
  // TODO(lukasza): Call ReceivedBadMessage when |frame_unique_name| is empty.
  DCHECK(!frame_unique_name.empty());

  // It is possible that while a new RenderFrameHost was committed, the
  // RenderFrame corresponding to this host sent an IPC message to create a
  // frame and it is delivered after this host is swapped out.
  // Ignore such messages, as we know this RenderFrameHost is going away.
  if (!is_active() || frame_tree_node_->current_frame_host() != this)
    return;

  frame_tree_->AddFrame(frame_tree_node_, GetProcess()->GetID(), new_routing_id,
                        scope, frame_name, frame_unique_name, sandbox_flags,
                        frame_owner_properties);
}

void RenderFrameHostImpl::OnDetach() {
  frame_tree_->RemoveFrame(frame_tree_node_);
}

void RenderFrameHostImpl::OnFrameFocused() {
  frame_tree_->SetFocusedFrame(frame_tree_node_, GetSiteInstance());
}

void RenderFrameHostImpl::OnOpenURL(const FrameHostMsg_OpenURL_Params& params) {
  if (params.is_history_navigation_in_new_child) {
    DCHECK(SiteIsolationPolicy::UseSubframeNavigationEntries());

    // Try to find a FrameNavigationEntry that matches this frame instead, based
    // on the frame's unique name.  If this can't be found, fall back to the
    // default params using OpenURL below.
    if (frame_tree_node_->navigator()->NavigateNewChildFrame(
            this, params.frame_unique_name))
      return;
  }

  OpenURL(params, GetSiteInstance());
}

void RenderFrameHostImpl::OnDocumentOnLoadCompleted(
    FrameMsg_UILoadMetricsReportType::Value report_type,
    base::TimeTicks ui_timestamp) {
  if (report_type == FrameMsg_UILoadMetricsReportType::REPORT_LINK) {
    UMA_HISTOGRAM_CUSTOM_TIMES("Navigation.UI_OnLoadComplete.Link",
                               base::TimeTicks::Now() - ui_timestamp,
                               base::TimeDelta::FromMilliseconds(10),
                               base::TimeDelta::FromMinutes(10), 100);
  } else if (report_type == FrameMsg_UILoadMetricsReportType::REPORT_INTENT) {
    UMA_HISTOGRAM_CUSTOM_TIMES("Navigation.UI_OnLoadComplete.Intent",
                               base::TimeTicks::Now() - ui_timestamp,
                               base::TimeDelta::FromMilliseconds(10),
                               base::TimeDelta::FromMinutes(10), 100);
  }
  // This message is only sent for top-level frames. TODO(avi): when frame tree
  // mirroring works correctly, add a check here to enforce it.
  delegate_->DocumentOnLoadCompleted(this);
}

void RenderFrameHostImpl::OnDidStartProvisionalLoad(
    const GURL& url,
    const base::TimeTicks& navigation_start) {
  frame_tree_node_->navigator()->DidStartProvisionalLoad(this, url,
                                                         navigation_start);
}

void RenderFrameHostImpl::OnDidFailProvisionalLoadWithError(
    const FrameHostMsg_DidFailProvisionalLoadWithError_Params& params) {
  // TODO(clamy): Kill the renderer with RFH_FAIL_PROVISIONAL_LOAD_NO_HANDLE and
  // return early if navigation_handle_ is null, once we prevent that case from
  // happening in practice.
  if (IsBrowserSideNavigationEnabled() && navigation_handle_ &&
      navigation_handle_->GetNetErrorCode() == net::OK) {
    // The renderer should not be sending this message unless asked to commit
    // an error page.
    // TODO(clamy): Stop sending DidFailProvisionalLoad IPCs at all when enough
    // observers have moved to DidFinishNavigation.
    bad_message::ReceivedBadMessage(
        GetProcess(), bad_message::RFH_FAIL_PROVISIONAL_LOAD_NO_ERROR);
    return;
  }

  // Update the error code in the NavigationHandle of the navigation.
  // PlzNavigate: this has already done in NavigationRequest::OnRequestFailed.
  if (!IsBrowserSideNavigationEnabled() && navigation_handle_) {
    navigation_handle_->set_net_error_code(
        static_cast<net::Error>(params.error_code));
  }

  frame_tree_node_->navigator()->DidFailProvisionalLoadWithError(this, params);
}

void RenderFrameHostImpl::OnDidFailLoadWithError(
    const GURL& url,
    int error_code,
    const base::string16& error_description,
    bool was_ignored_by_handler) {
  GURL validated_url(url);
  GetProcess()->FilterURL(false, &validated_url);

  frame_tree_node_->navigator()->DidFailLoadWithError(
      this, validated_url, error_code, error_description,
      was_ignored_by_handler);
}

// Called when the renderer navigates.  For every frame loaded, we'll get this
// notification containing parameters identifying the navigation.
//
// Subframes are identified by the page transition type.  For subframes loaded
// as part of a wider page load, the page_id will be the same as for the top
// level frame.  If the user explicitly requests a subframe navigation, we will
// get a new page_id because we need to create a new navigation entry for that
// action.
void RenderFrameHostImpl::OnDidCommitProvisionalLoad(const IPC::Message& msg) {
  ScopedCommitStateResetter commit_state_resetter(this);
  RenderProcessHost* process = GetProcess();

  // Read the parameters out of the IPC message directly to avoid making another
  // copy when we filter the URLs.
  base::PickleIterator iter(msg);
  FrameHostMsg_DidCommitProvisionalLoad_Params validated_params;
  if (!IPC::ParamTraits<FrameHostMsg_DidCommitProvisionalLoad_Params>::
      Read(&msg, &iter, &validated_params)) {
    bad_message::ReceivedBadMessage(
        process, bad_message::RFH_COMMIT_DESERIALIZATION_FAILED);
    return;
  }
  TRACE_EVENT1("navigation", "RenderFrameHostImpl::OnDidCommitProvisionalLoad",
               "url", validated_params.url.possibly_invalid_spec());

  // Sanity-check the page transition for frame type.
  DCHECK_EQ(ui::PageTransitionIsMainFrame(validated_params.transition),
            !GetParent());

  // If we're waiting for a cross-site beforeunload ack from this renderer and
  // we receive a Navigate message from the main frame, then the renderer was
  // navigating already and sent it before hearing the FrameMsg_Stop message.
  // Treat this as an implicit beforeunload ack to allow the pending navigation
  // to continue.
  if (is_waiting_for_beforeunload_ack_ &&
      unload_ack_is_for_navigation_ &&
      !GetParent()) {
    base::TimeTicks approx_renderer_start_time = send_before_unload_start_time_;
    OnBeforeUnloadACK(true, approx_renderer_start_time, base::TimeTicks::Now());
  }

  // If we're waiting for an unload ack from this renderer and we receive a
  // Navigate message, then the renderer was navigating before it received the
  // unload request.  It will either respond to the unload request soon or our
  // timer will expire.  Either way, we should ignore this message, because we
  // have already committed to closing this renderer.
  if (IsWaitingForUnloadACK())
    return;

  if (validated_params.report_type ==
      FrameMsg_UILoadMetricsReportType::REPORT_LINK) {
    UMA_HISTOGRAM_CUSTOM_TIMES(
        "Navigation.UI_OnCommitProvisionalLoad.Link",
        base::TimeTicks::Now() - validated_params.ui_timestamp,
        base::TimeDelta::FromMilliseconds(10), base::TimeDelta::FromMinutes(10),
        100);
  } else if (validated_params.report_type ==
             FrameMsg_UILoadMetricsReportType::REPORT_INTENT) {
    UMA_HISTOGRAM_CUSTOM_TIMES(
        "Navigation.UI_OnCommitProvisionalLoad.Intent",
        base::TimeTicks::Now() - validated_params.ui_timestamp,
        base::TimeDelta::FromMilliseconds(10), base::TimeDelta::FromMinutes(10),
        100);
  }

  // Attempts to commit certain off-limits URL should be caught more strictly
  // than our FilterURL checks below.  If a renderer violates this policy, it
  // should be killed.
  if (!CanCommitURL(validated_params.url)) {
    VLOG(1) << "Blocked URL " << validated_params.url.spec();
    validated_params.url = GURL(url::kAboutBlankURL);
    // Kills the process.
    bad_message::ReceivedBadMessage(process,
                                    bad_message::RFH_CAN_COMMIT_URL_BLOCKED);
    return;
  }

  // Verify that the origin passed from the renderer process is valid and can
  // be allowed to commit in this RenderFrameHost.
  if (!CanCommitOrigin(validated_params.origin, validated_params.url)) {
    bad_message::ReceivedBadMessage(GetProcess(),
                                    bad_message::RFH_INVALID_ORIGIN_ON_COMMIT);
    return;
  }

  // Without this check, an evil renderer can trick the browser into creating
  // a navigation entry for a banned URL.  If the user clicks the back button
  // followed by the forward button (or clicks reload, or round-trips through
  // session restore, etc), we'll think that the browser commanded the
  // renderer to load the URL and grant the renderer the privileges to request
  // the URL.  To prevent this attack, we block the renderer from inserting
  // banned URLs into the navigation controller in the first place.
  process->FilterURL(false, &validated_params.url);
  process->FilterURL(true, &validated_params.referrer.url);
  for (std::vector<GURL>::iterator it(validated_params.redirects.begin());
      it != validated_params.redirects.end(); ++it) {
    process->FilterURL(false, &(*it));
  }
  process->FilterURL(true, &validated_params.searchable_form_url);

  // Without this check, the renderer can trick the browser into using
  // filenames it can't access in a future session restore.
  if (!CanAccessFilesOfPageState(validated_params.page_state)) {
    bad_message::ReceivedBadMessage(
        GetProcess(), bad_message::RFH_CAN_ACCESS_FILES_OF_PAGE_STATE);
    return;
  }

  // If the URL does not match what the NavigationHandle expects, treat the
  // commit as a new navigation. This can happen if an ongoing slow
  // same-process navigation is interrupted by a synchronous renderer-initiated
  // navigation.
  // TODO(csharrison): Data navigations loaded with LoadDataWithBaseURL get
  // reset here, because the NavigationHandle tracks the URL but the
  // validated_params.url tracks the data. The trick of saving the old entry ids
  // for these navigations should go away when this is properly handled. See
  // crbug.com/588317.
  int entry_id_for_data_nav = 0;
  bool is_renderer_initiated = true;
  if (navigation_handle_ &&
      (navigation_handle_->GetURL() != validated_params.url)) {
    // Make sure that the pending entry was really loaded via
    // LoadDataWithBaseURL and that it matches this handle.
    NavigationEntryImpl* pending_entry =
        NavigationEntryImpl::FromNavigationEntry(
            frame_tree_node()->navigator()->GetController()->GetPendingEntry());
    bool pending_entry_matches_handle =
        pending_entry &&
        pending_entry->GetUniqueID() ==
            navigation_handle_->pending_nav_entry_id();
    // TODO(csharrison): The pending entry's base url should equal
    // |validated_params.base_url|. This is not the case for loads with invalid
    // base urls.
    if (navigation_handle_->GetURL() == validated_params.base_url &&
        pending_entry_matches_handle &&
        !pending_entry->GetBaseURLForDataURL().is_empty()) {
      entry_id_for_data_nav = navigation_handle_->pending_nav_entry_id();
      is_renderer_initiated = pending_entry->is_renderer_initiated();
    }
    navigation_handle_.reset();
  }

  // Synchronous renderer-initiated navigations will send a
  // DidCommitProvisionalLoad IPC without a prior DidStartProvisionalLoad
  // message.
  if (!navigation_handle_) {
    // There is no pending NavigationEntry in these cases, so pass 0 as the
    // nav_id. If the previous handle was a prematurely aborted navigation
    // loaded via LoadDataWithBaseURL, propogate the entry id.
    navigation_handle_ = NavigationHandleImpl::Create(
        validated_params.url, frame_tree_node_,
        is_renderer_initiated,
        true,  // is_synchronous
        validated_params.is_srcdoc, base::TimeTicks::Now(),
        entry_id_for_data_nav);
    // PlzNavigate
    if (IsBrowserSideNavigationEnabled()) {
      // PlzNavigate: synchronous loads happen in the renderer, and the browser
      // has not been notified about the start of the load yet. Do it now.
      if (!is_loading()) {
        bool was_loading = frame_tree_node()->frame_tree()->IsLoading();
        is_loading_ = true;
        frame_tree_node()->DidStartLoading(true, was_loading);
      }
      pending_commit_ = false;
    }
  }

  accessibility_reset_count_ = 0;
  frame_tree_node()->navigator()->DidNavigate(this, validated_params);

  // Since we didn't early return, it's safe to keep the commit state.
  commit_state_resetter.disable();

  // For a top-level frame, there are potential security concerns associated
  // with displaying graphics from a previously loaded page after the URL in
  // the omnibar has been changed. It is unappealing to clear the page
  // immediately, but if the renderer is taking a long time to issue any
  // compositor output (possibly because of script deliberately creating this
  // situation) then we clear it after a while anyway.
  // See https://crbug.com/497588.
  if (frame_tree_node_->IsMainFrame() && GetView() &&
      !validated_params.was_within_same_page) {
    RenderWidgetHostImpl::From(GetView()->GetRenderWidgetHost())
        ->StartNewContentRenderingTimeout();
  }
}

void RenderFrameHostImpl::OnUpdateState(const PageState& state) {
  // TODO(creis): Verify the state's ISN matches the last committed FNE.

  // Without this check, the renderer can trick the browser into using
  // filenames it can't access in a future session restore.
  if (!CanAccessFilesOfPageState(state)) {
    bad_message::ReceivedBadMessage(
        GetProcess(), bad_message::RFH_CAN_ACCESS_FILES_OF_PAGE_STATE);
    return;
  }

  delegate_->UpdateStateForFrame(this, state);
}

RenderWidgetHostImpl* RenderFrameHostImpl::GetRenderWidgetHost() {
  RenderFrameHostImpl* frame = this;
  while (frame) {
    if (frame->render_widget_host_)
      return frame->render_widget_host_;
    frame = frame->GetParent();
  }

  NOTREACHED();
  return nullptr;
}

RenderWidgetHostView* RenderFrameHostImpl::GetView() {
  return GetRenderWidgetHost()->GetView();
}

GlobalFrameRoutingId RenderFrameHostImpl::GetGlobalFrameRoutingId() {
  return GlobalFrameRoutingId(GetProcess()->GetID(), GetRoutingID());
}

int RenderFrameHostImpl::GetEnabledBindings() {
  return render_view_host_->GetEnabledBindings();
}

void RenderFrameHostImpl::SetNavigationHandle(
    std::unique_ptr<NavigationHandleImpl> navigation_handle) {
  navigation_handle_ = std::move(navigation_handle);
  if (navigation_handle_)
    navigation_handle_->set_render_frame_host(this);
}

std::unique_ptr<NavigationHandleImpl>
RenderFrameHostImpl::PassNavigationHandleOwnership() {
  DCHECK(!IsBrowserSideNavigationEnabled());
  if (navigation_handle_)
    navigation_handle_->set_is_transferring(true);
  return std::move(navigation_handle_);
}

void RenderFrameHostImpl::OnCrossSiteResponse(
    const GlobalRequestID& global_request_id,
    std::unique_ptr<CrossSiteTransferringRequest>
        cross_site_transferring_request,
    const std::vector<GURL>& transfer_url_chain,
    const Referrer& referrer,
    ui::PageTransition page_transition,
    bool should_replace_current_entry) {
  frame_tree_node_->render_manager()->OnCrossSiteResponse(
      this, global_request_id, std::move(cross_site_transferring_request),
      transfer_url_chain, referrer, page_transition,
      should_replace_current_entry);
}

void RenderFrameHostImpl::SwapOut(
    RenderFrameProxyHost* proxy,
    bool is_loading) {
  // The end of this event is in OnSwapOutACK when the RenderFrame has completed
  // the operation and sends back an IPC message.
  // The trace event may not end properly if the ACK times out.  We expect this
  // to be fixed when RenderViewHostImpl::OnSwapOut moves to RenderFrameHost.
  TRACE_EVENT_ASYNC_BEGIN0("navigation", "RenderFrameHostImpl::SwapOut", this);

  // If this RenderFrameHost is already pending deletion, it must have already
  // gone through this, therefore just return.
  if (!is_active()) {
    NOTREACHED() << "RFH should be in default state when calling SwapOut.";
    return;
  }

  if (swapout_event_monitor_timeout_) {
    swapout_event_monitor_timeout_->Start(base::TimeDelta::FromMilliseconds(
        RenderViewHostImpl::kUnloadTimeoutMS));
  }

  // There should always be a proxy to replace the old RenderFrameHost.  If
  // there are no remaining active views in the process, the proxy will be
  // short-lived and will be deleted when the SwapOut ACK is received.
  CHECK(proxy);

  set_render_frame_proxy_host(proxy);

  if (IsRenderFrameLive()) {
    FrameReplicationState replication_state =
        proxy->frame_tree_node()->current_replication_state();
    Send(new FrameMsg_SwapOut(routing_id_, proxy->GetRoutingID(), is_loading,
                              replication_state));
  }

  if (web_ui())
    web_ui()->RenderFrameHostSwappingOut();

  // TODO(nasko): If the frame is not live, the RFH should just be deleted by
  // simulating the receipt of swap out ack.
  is_waiting_for_swapout_ack_ = true;
  if (frame_tree_node_->IsMainFrame())
    render_view_host_->set_is_active(false);
}

void RenderFrameHostImpl::OnBeforeUnloadACK(
    bool proceed,
    const base::TimeTicks& renderer_before_unload_start_time,
    const base::TimeTicks& renderer_before_unload_end_time) {
  TRACE_EVENT_ASYNC_END1("navigation", "RenderFrameHostImpl BeforeUnload", this,
                         "FrameTreeNode id",
                         frame_tree_node_->frame_tree_node_id());
  DCHECK(!GetParent());
  // If this renderer navigated while the beforeunload request was in flight, we
  // may have cleared this state in OnDidCommitProvisionalLoad, in which case we
  // can ignore this message.
  // However renderer might also be swapped out but we still want to proceed
  // with navigation, otherwise it would block future navigations. This can
  // happen when pending cross-site navigation is canceled by a second one just
  // before OnDidCommitProvisionalLoad while current RVH is waiting for commit
  // but second navigation is started from the beginning.
  if (!is_waiting_for_beforeunload_ack_) {
    return;
  }
  DCHECK(!send_before_unload_start_time_.is_null());

  // Sets a default value for before_unload_end_time so that the browser
  // survives a hacked renderer.
  base::TimeTicks before_unload_end_time = renderer_before_unload_end_time;
  if (!renderer_before_unload_start_time.is_null() &&
      !renderer_before_unload_end_time.is_null()) {
    // When passing TimeTicks across process boundaries, we need to compensate
    // for any skew between the processes. Here we are converting the
    // renderer's notion of before_unload_end_time to TimeTicks in the browser
    // process. See comments in inter_process_time_ticks_converter.h for more.
    base::TimeTicks receive_before_unload_ack_time = base::TimeTicks::Now();
    InterProcessTimeTicksConverter converter(
        LocalTimeTicks::FromTimeTicks(send_before_unload_start_time_),
        LocalTimeTicks::FromTimeTicks(receive_before_unload_ack_time),
        RemoteTimeTicks::FromTimeTicks(renderer_before_unload_start_time),
        RemoteTimeTicks::FromTimeTicks(renderer_before_unload_end_time));
    LocalTimeTicks browser_before_unload_end_time =
        converter.ToLocalTimeTicks(
            RemoteTimeTicks::FromTimeTicks(renderer_before_unload_end_time));
    before_unload_end_time = browser_before_unload_end_time.ToTimeTicks();

    // Collect UMA on the inter-process skew.
    bool is_skew_additive = false;
    if (converter.IsSkewAdditiveForMetrics()) {
      is_skew_additive = true;
      base::TimeDelta skew = converter.GetSkewForMetrics();
      if (skew >= base::TimeDelta()) {
        UMA_HISTOGRAM_TIMES(
            "InterProcessTimeTicks.BrowserBehind_RendererToBrowser", skew);
      } else {
        UMA_HISTOGRAM_TIMES(
            "InterProcessTimeTicks.BrowserAhead_RendererToBrowser", -skew);
      }
    }
    UMA_HISTOGRAM_BOOLEAN(
        "InterProcessTimeTicks.IsSkewAdditive_RendererToBrowser",
        is_skew_additive);

    base::TimeDelta on_before_unload_overhead_time =
        (receive_before_unload_ack_time - send_before_unload_start_time_) -
        (renderer_before_unload_end_time - renderer_before_unload_start_time);
    UMA_HISTOGRAM_TIMES("Navigation.OnBeforeUnloadOverheadTime",
                        on_before_unload_overhead_time);

    frame_tree_node_->navigator()->LogBeforeUnloadTime(
        renderer_before_unload_start_time, renderer_before_unload_end_time);
  }
  // Resets beforeunload waiting state.
  is_waiting_for_beforeunload_ack_ = false;
  render_view_host_->GetWidget()->decrement_in_flight_event_count();
  render_view_host_->GetWidget()->StopHangMonitorTimeout();
  send_before_unload_start_time_ = base::TimeTicks();

  // PlzNavigate: if the ACK is for a navigation, send it to the Navigator to
  // have the current navigation stop/proceed. Otherwise, send it to the
  // RenderFrameHostManager which handles closing.
  if (IsBrowserSideNavigationEnabled() && unload_ack_is_for_navigation_) {
    // TODO(clamy): see if before_unload_end_time should be transmitted to the
    // Navigator.
    frame_tree_node_->navigator()->OnBeforeUnloadACK(
        frame_tree_node_, proceed);
  } else {
    frame_tree_node_->render_manager()->OnBeforeUnloadACK(
        unload_ack_is_for_navigation_, proceed,
        before_unload_end_time);
  }

  // If canceled, notify the delegate to cancel its pending navigation entry.
  if (!proceed)
    render_view_host_->GetDelegate()->DidCancelLoading();
}

bool RenderFrameHostImpl::IsWaitingForUnloadACK() const {
  return render_view_host_->is_waiting_for_close_ack_ ||
         is_waiting_for_swapout_ack_;
}

void RenderFrameHostImpl::OnSwapOutACK() {
  OnSwappedOut();
}

void RenderFrameHostImpl::OnRenderProcessGone(int status, int exit_code) {
  if (frame_tree_node_->IsMainFrame()) {
    // Keep the termination status so we can get at it later when we
    // need to know why it died.
    render_view_host_->render_view_termination_status_ =
        static_cast<base::TerminationStatus>(status);
  }

  // Reset frame tree state associated with this process.  This must happen
  // before RenderViewTerminated because observers expect the subframes of any
  // affected frames to be cleared first.
  frame_tree_node_->ResetForNewProcess();

  // Reset state for the current RenderFrameHost once the FrameTreeNode has been
  // reset.
  SetRenderFrameCreated(false);
  InvalidateMojoConnection();

  // Execute any pending AX tree snapshot callbacks with an empty response,
  // since we're never going to get a response from this renderer.
  for (const auto& iter : ax_tree_snapshot_callbacks_)
    iter.second.Run(ui::AXTreeUpdate());
  ax_tree_snapshot_callbacks_.clear();

  // If the process has died, we don't need to wait for the swap out ack from
  // this RenderFrame if it is pending deletion.  Complete the swap out to
  // destroy it.
  if (!is_active())
    OnSwappedOut();

  // Note: don't add any more code at this point in the function because
  // |this| may be deleted. Any additional cleanup should happen before
  // the last block of code here.
}

void RenderFrameHostImpl::OnSwappedOut() {
  // Ignore spurious swap out ack.
  if (!is_waiting_for_swapout_ack_)
    return;

  TRACE_EVENT_ASYNC_END0("navigation", "RenderFrameHostImpl::SwapOut", this);
  if (swapout_event_monitor_timeout_)
    swapout_event_monitor_timeout_->Stop();

  ClearAllWebUI();

  // If this is a main frame RFH that's about to be deleted, update its RVH's
  // swapped-out state here. https://crbug.com/505887
  if (frame_tree_node_->IsMainFrame()) {
    render_view_host_->set_is_active(false);
    render_view_host_->set_is_swapped_out(true);
  }

  bool deleted =
      frame_tree_node_->render_manager()->DeleteFromPendingList(this);
  CHECK(deleted);
}

void RenderFrameHostImpl::DisableSwapOutTimerForTesting() {
  swapout_event_monitor_timeout_.reset();
}

void RenderFrameHostImpl::OnContextMenu(const ContextMenuParams& params) {
  // Validate the URLs in |params|.  If the renderer can't request the URLs
  // directly, don't show them in the context menu.
  ContextMenuParams validated_params(params);
  RenderProcessHost* process = GetProcess();

  // We don't validate |unfiltered_link_url| so that this field can be used
  // when users want to copy the original link URL.
  process->FilterURL(true, &validated_params.link_url);
  process->FilterURL(true, &validated_params.src_url);
  process->FilterURL(false, &validated_params.page_url);
  process->FilterURL(true, &validated_params.frame_url);

  // It is necessary to transform the coordinates to account for nested
  // RenderWidgetHosts, such as with out-of-process iframes.
  gfx::Point original_point(validated_params.x, validated_params.y);
  gfx::Point transformed_point =
      static_cast<RenderWidgetHostViewBase*>(GetView())
          ->TransformPointToRootCoordSpace(original_point);
  validated_params.x = transformed_point.x();
  validated_params.y = transformed_point.y();

  delegate_->ShowContextMenu(this, validated_params);
}

void RenderFrameHostImpl::OnJavaScriptExecuteResponse(
    int id, const base::ListValue& result) {
  const base::Value* result_value;
  if (!result.Get(0, &result_value)) {
    // Programming error or rogue renderer.
    NOTREACHED() << "Got bad arguments for OnJavaScriptExecuteResponse";
    return;
  }

  std::map<int, JavaScriptResultCallback>::iterator it =
      javascript_callbacks_.find(id);
  if (it != javascript_callbacks_.end()) {
    it->second.Run(result_value);
    javascript_callbacks_.erase(it);
  } else {
    NOTREACHED() << "Received script response for unknown request";
  }
}

void RenderFrameHostImpl::OnVisualStateResponse(uint64_t id) {
  auto it = visual_state_callbacks_.find(id);
  if (it != visual_state_callbacks_.end()) {
    it->second.Run(true);
    visual_state_callbacks_.erase(it);
  } else {
    NOTREACHED() << "Received script response for unknown request";
  }
}

void RenderFrameHostImpl::OnRunJavaScriptMessage(
    const base::string16& message,
    const base::string16& default_prompt,
    const GURL& frame_url,
    JavaScriptMessageType type,
    IPC::Message* reply_msg) {
  // While a JS message dialog is showing, tabs in the same process shouldn't
  // process input events.
  GetProcess()->SetIgnoreInputEvents(true);
  render_view_host_->GetWidget()->StopHangMonitorTimeout();
  delegate_->RunJavaScriptMessage(this, message, default_prompt,
                                  frame_url, type, reply_msg);
}

void RenderFrameHostImpl::OnRunBeforeUnloadConfirm(
    const GURL& frame_url,
    bool is_reload,
    IPC::Message* reply_msg) {
  // While a JS beforeunload dialog is showing, tabs in the same process
  // shouldn't process input events.
  GetProcess()->SetIgnoreInputEvents(true);
  render_view_host_->GetWidget()->StopHangMonitorTimeout();
  delegate_->RunBeforeUnloadConfirm(this, is_reload, reply_msg);
}

void RenderFrameHostImpl::OnRunFileChooser(const FileChooserParams& params) {
  // Do not allow messages with absolute paths in them as this can permit a
  // renderer to coerce the browser to perform I/O on a renderer controlled
  // path.
  if (params.default_file_name != params.default_file_name.BaseName()) {
    bad_message::ReceivedBadMessage(GetProcess(),
                                    bad_message::RFH_FILE_CHOOSER_PATH);
    return;
  }

  delegate_->RunFileChooser(this, params);
}

void RenderFrameHostImpl::OnTextSurroundingSelectionResponse(
    const base::string16& content,
    uint32_t start_offset,
    uint32_t end_offset) {
  render_view_host_->OnTextSurroundingSelectionResponse(
      content, start_offset, end_offset);
}

void RenderFrameHostImpl::OnDidAccessInitialDocument() {
  delegate_->DidAccessInitialDocument();
}

void RenderFrameHostImpl::OnDidChangeOpener(int32_t opener_routing_id) {
  frame_tree_node_->render_manager()->DidChangeOpener(opener_routing_id,
                                                      GetSiteInstance());
}

void RenderFrameHostImpl::OnDidChangeName(const std::string& name,
                                          const std::string& unique_name) {
  if (GetParent() != nullptr) {
    // TODO(lukasza): Call ReceivedBadMessage when |unique_name| is empty.
    DCHECK(!unique_name.empty());
  }

  std::string old_name = frame_tree_node()->frame_name();
  frame_tree_node()->SetFrameName(name, unique_name);
  if (old_name.empty() && !name.empty())
    frame_tree_node_->render_manager()->CreateProxiesForNewNamedFrame();
  delegate_->DidChangeName(this, name);
}

void RenderFrameHostImpl::OnDidAddContentSecurityPolicy(
    const ContentSecurityPolicyHeader& header) {
  frame_tree_node()->AddContentSecurityPolicy(header);
}

void RenderFrameHostImpl::OnEnforceInsecureRequestPolicy(
    blink::WebInsecureRequestPolicy policy) {
  frame_tree_node()->SetInsecureRequestPolicy(policy);
}

void RenderFrameHostImpl::OnUpdateToUniqueOrigin(
    bool is_potentially_trustworthy_unique_origin) {
  url::Origin origin;
  DCHECK(origin.unique());
  frame_tree_node()->SetCurrentOrigin(origin,
                                      is_potentially_trustworthy_unique_origin);
}

void RenderFrameHostImpl::OnDidAssignPageId(int32_t page_id) {
  // Update the RVH's current page ID so that future IPCs from the renderer
  // correspond to the new page.
  render_view_host_->page_id_ = page_id;
}

FrameTreeNode* RenderFrameHostImpl::FindAndVerifyChild(
    int32_t child_frame_routing_id,
    bad_message::BadMessageReason reason) {
  FrameTreeNode* child = frame_tree_node()->frame_tree()->FindByRoutingID(
      GetProcess()->GetID(), child_frame_routing_id);
  // A race can result in |child| to be nullptr. Avoid killing the renderer in
  // that case.
  if (child && child->parent() != frame_tree_node()) {
    bad_message::ReceivedBadMessage(GetProcess(), reason);
    return nullptr;
  }
  return child;
}

void RenderFrameHostImpl::OnDidChangeSandboxFlags(
    int32_t frame_routing_id,
    blink::WebSandboxFlags flags) {
  // Ensure that a frame can only update sandbox flags for its immediate
  // children.  If this is not the case, the renderer is considered malicious
  // and is killed.
  FrameTreeNode* child = FindAndVerifyChild(
      frame_routing_id, bad_message::RFH_SANDBOX_FLAGS);
  if (!child)
    return;

  child->SetPendingSandboxFlags(flags);

  // Notify the RenderFrame if it lives in a different process from its
  // parent. The frame's proxies in other processes also need to learn about
  // the updated sandbox flags, but these notifications are sent later in
  // RenderFrameHostManager::CommitPendingSandboxFlags(), when the frame
  // navigates and the new sandbox flags take effect.
  RenderFrameHost* child_rfh = child->current_frame_host();
  if (child_rfh->GetSiteInstance() != GetSiteInstance()) {
    child_rfh->Send(
        new FrameMsg_DidUpdateSandboxFlags(child_rfh->GetRoutingID(), flags));
  }
}

void RenderFrameHostImpl::OnDidChangeFrameOwnerProperties(
    int32_t frame_routing_id,
    const blink::WebFrameOwnerProperties& properties) {
  FrameTreeNode* child = FindAndVerifyChild(
      frame_routing_id, bad_message::RFH_OWNER_PROPERTY);
  if (!child)
    return;

  child->set_frame_owner_properties(properties);

  child->render_manager()->OnDidUpdateFrameOwnerProperties(properties);
}

void RenderFrameHostImpl::OnUpdateTitle(
    const base::string16& title,
    blink::WebTextDirection title_direction) {
  // This message should only be sent for top-level frames.
  if (frame_tree_node_->parent())
    return;

  if (title.length() > kMaxTitleChars) {
    NOTREACHED() << "Renderer sent too many characters in title.";
    return;
  }

  delegate_->UpdateTitle(this, render_view_host_->page_id_, title,
                         WebTextDirectionToChromeTextDirection(
                             title_direction));
}

void RenderFrameHostImpl::OnUpdateEncoding(const std::string& encoding_name) {
  // This message is only sent for top-level frames. TODO(avi): when frame tree
  // mirroring works correctly, add a check here to enforce it.
  delegate_->UpdateEncoding(this, encoding_name);
}

void RenderFrameHostImpl::OnBeginNavigation(
    const CommonNavigationParams& common_params,
    const BeginNavigationParams& begin_params) {
  CHECK(IsBrowserSideNavigationEnabled());
  CommonNavigationParams validated_params = common_params;
  GetProcess()->FilterURL(false, &validated_params.url);
  frame_tree_node()->navigator()->OnBeginNavigation(
      frame_tree_node(), validated_params, begin_params);
}

void RenderFrameHostImpl::OnDispatchLoad() {
  CHECK(SiteIsolationPolicy::AreCrossProcessFramesPossible());
  // Only frames with an out-of-process parent frame should be sending this
  // message.
  RenderFrameProxyHost* proxy =
      frame_tree_node()->render_manager()->GetProxyToParent();
  if (!proxy) {
    bad_message::ReceivedBadMessage(GetProcess(),
                                    bad_message::RFH_NO_PROXY_TO_PARENT);
    return;
  }

  proxy->Send(new FrameMsg_DispatchLoad(proxy->GetRoutingID()));
}

RenderWidgetHostViewBase* RenderFrameHostImpl::GetViewForAccessibility() {
  return static_cast<RenderWidgetHostViewBase*>(
      frame_tree_node_->IsMainFrame()
          ? render_view_host_->GetWidget()->GetView()
          : frame_tree_node_->frame_tree()
                ->GetMainFrame()
                ->render_view_host_->GetWidget()
                ->GetView());
}

void RenderFrameHostImpl::OnAccessibilityEvents(
    const std::vector<AccessibilityHostMsg_EventParams>& params,
    int reset_token) {
  // Don't process this IPC if either we're waiting on a reset and this
  // IPC doesn't have the matching token ID, or if we're not waiting on a
  // reset but this message includes a reset token.
  if (accessibility_reset_token_ != reset_token) {
    Send(new AccessibilityMsg_Events_ACK(routing_id_));
    return;
  }
  accessibility_reset_token_ = 0;

  RenderWidgetHostViewBase* view = GetViewForAccessibility();

  AccessibilityMode accessibility_mode = delegate_->GetAccessibilityMode();
  if ((accessibility_mode != AccessibilityModeOff) && view && is_active()) {
    if (accessibility_mode & AccessibilityModeFlagPlatform)
      GetOrCreateBrowserAccessibilityManager();

    std::vector<AXEventNotificationDetails> details;
    details.reserve(params.size());
    for (size_t i = 0; i < params.size(); ++i) {
      const AccessibilityHostMsg_EventParams& param = params[i];
      AXEventNotificationDetails detail;
      detail.event_type = param.event_type;
      detail.id = param.id;
      detail.ax_tree_id = GetAXTreeID();
      if (param.update.has_tree_data) {
        detail.update.has_tree_data = true;
        ax_content_tree_data_ = param.update.tree_data;
        AXContentTreeDataToAXTreeData(&detail.update.tree_data);
      }
      detail.update.root_id = param.update.root_id;
      detail.update.node_id_to_clear = param.update.node_id_to_clear;
      detail.update.nodes.resize(param.update.nodes.size());
      for (size_t i = 0; i < param.update.nodes.size(); ++i) {
        AXContentNodeDataToAXNodeData(param.update.nodes[i],
                                      &detail.update.nodes[i]);
      }
      details.push_back(detail);
    }

    if (accessibility_mode & AccessibilityModeFlagPlatform) {
      if (browser_accessibility_manager_)
        browser_accessibility_manager_->OnAccessibilityEvents(details);
    }

    // Send the updates to the automation extension API.
    delegate_->AccessibilityEventReceived(details);

    // For testing only.
    if (!accessibility_testing_callback_.is_null()) {
      for (size_t i = 0; i < details.size(); i++) {
        const AXEventNotificationDetails& detail = details[i];
        if (static_cast<int>(detail.event_type) < 0)
          continue;

        if (!ax_tree_for_testing_) {
          if (browser_accessibility_manager_) {
            ax_tree_for_testing_.reset(new ui::AXTree(
                browser_accessibility_manager_->SnapshotAXTreeForTesting()));
          } else {
            ax_tree_for_testing_.reset(new ui::AXTree());
            CHECK(ax_tree_for_testing_->Unserialize(detail.update))
                << ax_tree_for_testing_->error();
          }
        } else {
          CHECK(ax_tree_for_testing_->Unserialize(detail.update))
              << ax_tree_for_testing_->error();
        }
        accessibility_testing_callback_.Run(this, detail.event_type, detail.id);
      }
    }
  }

  // Always send an ACK or the renderer can be in a bad state.
  Send(new AccessibilityMsg_Events_ACK(routing_id_));
}

void RenderFrameHostImpl::OnAccessibilityLocationChanges(
    const std::vector<AccessibilityHostMsg_LocationChangeParams>& params) {
  if (accessibility_reset_token_)
    return;

  RenderWidgetHostViewBase* view = static_cast<RenderWidgetHostViewBase*>(
      render_view_host_->GetWidget()->GetView());
  if (view && is_active()) {
    AccessibilityMode accessibility_mode = delegate_->GetAccessibilityMode();
    if (accessibility_mode & AccessibilityModeFlagPlatform) {
      BrowserAccessibilityManager* manager =
          GetOrCreateBrowserAccessibilityManager();
      if (manager)
        manager->OnLocationChanges(params);
    }
    // TODO(aboxhall): send location change events to web contents observers too
  }
}

void RenderFrameHostImpl::OnAccessibilityFindInPageResult(
    const AccessibilityHostMsg_FindInPageResultParams& params) {
  AccessibilityMode accessibility_mode = delegate_->GetAccessibilityMode();
  if (accessibility_mode & AccessibilityModeFlagPlatform) {
    BrowserAccessibilityManager* manager =
        GetOrCreateBrowserAccessibilityManager();
    if (manager) {
      manager->OnFindInPageResult(
          params.request_id, params.match_index, params.start_id,
          params.start_offset, params.end_id, params.end_offset);
    }
  }
}

void RenderFrameHostImpl::OnAccessibilityChildFrameHitTestResult(
    const gfx::Point& point,
    int hit_obj_id) {
  if (browser_accessibility_manager_) {
    browser_accessibility_manager_->OnChildFrameHitTestResult(point,
                                                              hit_obj_id);
  }
}

void RenderFrameHostImpl::OnAccessibilitySnapshotResponse(
    int callback_id,
    const AXContentTreeUpdate& snapshot) {
  const auto& it = ax_tree_snapshot_callbacks_.find(callback_id);
  if (it != ax_tree_snapshot_callbacks_.end()) {
    ui::AXTreeUpdate dst_snapshot;
    dst_snapshot.root_id = snapshot.root_id;
    dst_snapshot.nodes.resize(snapshot.nodes.size());
    for (size_t i = 0; i < snapshot.nodes.size(); ++i) {
      AXContentNodeDataToAXNodeData(snapshot.nodes[i],
                                    &dst_snapshot.nodes[i]);
    }
    if (snapshot.has_tree_data) {
      ax_content_tree_data_ = snapshot.tree_data;
      AXContentTreeDataToAXTreeData(&dst_snapshot.tree_data);
      dst_snapshot.has_tree_data = true;
    }
    it->second.Run(dst_snapshot);
    ax_tree_snapshot_callbacks_.erase(it);
  } else {
    NOTREACHED() << "Received AX tree snapshot response for unknown id";
  }
}

// TODO(alexmos): When the allowFullscreen flag is known in the browser
// process, use it to double-check that fullscreen can be entered here.
void RenderFrameHostImpl::OnToggleFullscreen(bool enter_fullscreen) {
  // Entering fullscreen from a cross-process subframe also affects all
  // renderers for ancestor frames, which will need to apply fullscreen CSS to
  // appropriate ancestor <iframe> elements, fire fullscreenchange events, etc.
  // Thus, walk through the ancestor chain of this frame and for each (parent,
  // child) pair, send a message about the pending fullscreen change to the
  // child's proxy in parent's SiteInstance. The renderer process will use this
  // to find the <iframe> element in the parent frame that will need fullscreen
  // styles. This is done at most once per SiteInstance: for example, with a
  // A-B-A-B hierarchy, if the bottom frame goes fullscreen, this only needs to
  // notify its parent, and Blink-side logic will take care of applying
  // necessary changes to the other two ancestors.
  if (enter_fullscreen &&
      SiteIsolationPolicy::AreCrossProcessFramesPossible()) {
    std::set<SiteInstance*> notified_instances;
    notified_instances.insert(GetSiteInstance());
    for (FrameTreeNode* node = frame_tree_node_; node->parent();
         node = node->parent()) {
      SiteInstance* parent_site_instance =
          node->parent()->current_frame_host()->GetSiteInstance();
      if (ContainsKey(notified_instances, parent_site_instance))
        continue;

      RenderFrameProxyHost* child_proxy =
          node->render_manager()->GetRenderFrameProxyHost(parent_site_instance);
      child_proxy->Send(
          new FrameMsg_WillEnterFullscreen(child_proxy->GetRoutingID()));
      notified_instances.insert(parent_site_instance);
    }
  }

  // TODO(alexmos): See if this can use the last committed origin instead.
  if (enter_fullscreen)
    delegate_->EnterFullscreenMode(last_committed_url().GetOrigin());
  else
    delegate_->ExitFullscreenMode(/* will_cause_resize */ false);
}

void RenderFrameHostImpl::OnDidStartLoading(bool to_different_document) {
  if (IsBrowserSideNavigationEnabled() && to_different_document) {
    bad_message::ReceivedBadMessage(GetProcess(),
                                    bad_message::RFH_UNEXPECTED_LOAD_START);
    return;
  }
  bool was_previously_loading = frame_tree_node_->frame_tree()->IsLoading();
  is_loading_ = true;

  // Only inform the FrameTreeNode of a change in load state if the load state
  // of this RenderFrameHost is being tracked.
  if (is_active()) {
    frame_tree_node_->DidStartLoading(to_different_document,
                                      was_previously_loading);
  }
}

void RenderFrameHostImpl::OnDidStopLoading() {
  // This method should never be called when the frame is not loading.
  // Unfortunately, it can happen if a history navigation happens during a
  // BeforeUnload or Unload event.
  // TODO(fdegans): Change this to a DCHECK after LoadEventProgress has been
  // refactored in Blink. See crbug.com/466089
  if (!is_loading_) {
    LOG(WARNING) << "OnDidStopLoading was called twice.";
    return;
  }

  is_loading_ = false;
  navigation_handle_.reset();

  // Only inform the FrameTreeNode of a change in load state if the load state
  // of this RenderFrameHost is being tracked.
  if (is_active())
    frame_tree_node_->DidStopLoading();
}

void RenderFrameHostImpl::OnDidChangeLoadProgress(double load_progress) {
  frame_tree_node_->DidChangeLoadProgress(load_progress);
}

void RenderFrameHostImpl::OnSerializeAsMHTMLResponse(
    int job_id,
    bool success,
    const std::set<std::string>& digests_of_uris_of_serialized_resources) {
  MHTMLGenerationManager::GetInstance()->OnSerializeAsMHTMLResponse(
      this, job_id, success, digests_of_uris_of_serialized_resources);
}

#if defined(USE_EXTERNAL_POPUP_MENU)
void RenderFrameHostImpl::OnShowPopup(
    const FrameHostMsg_ShowPopup_Params& params) {
  RenderViewHostDelegateView* view =
      render_view_host_->delegate_->GetDelegateView();
  if (view) {
    gfx::Point original_point(params.bounds.x(), params.bounds.y());
    gfx::Point transformed_point =
        static_cast<RenderWidgetHostViewBase*>(GetView())
            ->TransformPointToRootCoordSpace(original_point);
    gfx::Rect transformed_bounds(transformed_point.x(), transformed_point.y(),
                                 params.bounds.width(), params.bounds.height());
    view->ShowPopupMenu(this, transformed_bounds, params.item_height,
                        params.item_font_size, params.selected_item,
                        params.popup_items, params.right_aligned,
                        params.allow_multiple_selection);
  }
}

void RenderFrameHostImpl::OnHidePopup() {
  RenderViewHostDelegateView* view =
      render_view_host_->delegate_->GetDelegateView();
  if (view)
    view->HidePopupMenu();
}
#endif

void RenderFrameHostImpl::RegisterMojoInterfaces() {
  GeolocationServiceContext* geolocation_service_context =
      delegate_ ? delegate_->GetGeolocationServiceContext() : NULL;
  if (geolocation_service_context) {
    // TODO(creis): Bind process ID here so that GeolocationServiceImpl
    // can perform permissions checks once site isolation is complete.
    // crbug.com/426384
    // NOTE: At shutdown, there is no guaranteed ordering between destruction of
    // this object and destruction of any GeolocationServicesImpls created via
    // the below service registry, the reason being that the destruction of the
    // latter is triggered by receiving a message that the pipe was closed from
    // the renderer side. Hence, supply the reference to this object as a weak
    // pointer.
    GetInterfaceRegistry()->AddInterface(
        base::Bind(&GeolocationServiceContext::CreateService,
                   base::Unretained(geolocation_service_context),
                   base::Bind(&RenderFrameHostImpl::DidUseGeolocationPermission,
                              weak_ptr_factory_.GetWeakPtr())));
  }

  WakeLockServiceContext* wake_lock_service_context =
      delegate_ ? delegate_->GetWakeLockServiceContext() : nullptr;
  if (wake_lock_service_context) {
    // WakeLockServiceContext is owned by WebContentsImpl so it will outlive
    // this RenderFrameHostImpl, hence a raw pointer can be bound to service
    // factory callback.
    GetInterfaceRegistry()->AddInterface<blink::mojom::WakeLockService>(
        base::Bind(&WakeLockServiceContext::CreateService,
                   base::Unretained(wake_lock_service_context),
                   GetProcess()->GetID(), GetRoutingID()));
  }

  if (!permission_service_context_)
    permission_service_context_.reset(new PermissionServiceContext(this));

  GetInterfaceRegistry()->AddInterface(
      base::Bind(&PermissionServiceContext::CreateService,
                 base::Unretained(permission_service_context_.get())));

  GetInterfaceRegistry()->AddInterface(base::Bind(
      &PresentationServiceImpl::CreateMojoService, base::Unretained(this)));

#if !defined(OS_ANDROID)
  GetInterfaceRegistry()->AddInterface(
      base::Bind(&device::VibrationManagerImpl::Create));
#endif

  bool enable_web_bluetooth = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableWebBluetooth);
#if defined(OS_CHROMEOS) || defined(OS_ANDROID) || defined(OS_MACOSX)
  enable_web_bluetooth = true;
#endif

  if (enable_web_bluetooth) {
    GetInterfaceRegistry()->AddInterface(
        base::Bind(&RenderFrameHostImpl::CreateWebBluetoothService,
                   base::Unretained(this)));
  }

  if (!frame_mojo_shell_)
    frame_mojo_shell_.reset(new FrameMojoShell(this));

  GetInterfaceRegistry()->AddInterface<shell::mojom::Connector>(base::Bind(
      &FrameMojoShell::BindRequest, base::Unretained(frame_mojo_shell_.get())));

#if defined(ENABLE_WEBVR)
  const base::CommandLine& browser_command_line =
      *base::CommandLine::ForCurrentProcess();

  if (browser_command_line.HasSwitch(switches::kEnableWebVR)) {
    GetInterfaceRegistry()->AddInterface<device::VRService>(
        base::Bind(&device::VRDeviceManager::BindRequest));
  }
#endif

  GetContentClient()->browser()->RegisterRenderFrameMojoInterfaces(
      GetInterfaceRegistry(), this);
}

void RenderFrameHostImpl::ResetWaitingState() {
  DCHECK(is_active());

  // The active state of the RVH is determined by its main frame, since
  // subframes should have their own widgets.
  if (frame_tree_node_->IsMainFrame()) {
    render_view_host_->set_is_active(true);
    render_view_host_->set_is_swapped_out(false);
  }

  // Whenever we reset the RFH state, we should not be waiting for beforeunload
  // or close acks.  We clear them here to be safe, since they can cause
  // navigations to be ignored in OnDidCommitProvisionalLoad.
  if (is_waiting_for_beforeunload_ack_) {
    is_waiting_for_beforeunload_ack_ = false;
    render_view_host_->GetWidget()->decrement_in_flight_event_count();
    render_view_host_->GetWidget()->StopHangMonitorTimeout();
  }
  send_before_unload_start_time_ = base::TimeTicks();
  render_view_host_->is_waiting_for_close_ack_ = false;
}

bool RenderFrameHostImpl::CanCommitURL(const GURL& url) {
  // TODO(creis): We should also check for WebUI pages here.  Also, when the
  // out-of-process iframes implementation is ready, we should check for
  // cross-site URLs that are not allowed to commit in this process.

  // Give the client a chance to disallow URLs from committing.
  return GetContentClient()->browser()->CanCommitURL(GetProcess(), url);
}

bool RenderFrameHostImpl::CanCommitOrigin(
    const url::Origin& origin,
    const GURL& url) {
  // If the --disable-web-security flag is specified, all bets are off and the
  // renderer process can send any origin it wishes.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableWebSecurity)) {
    return true;
  }

  // file: URLs can be allowed to access any other origin, based on settings.
  if (origin.scheme() == url::kFileScheme) {
    WebPreferences prefs = render_view_host_->GetWebkitPreferences();
    if (prefs.allow_universal_access_from_file_urls)
      return true;
  }

  // It is safe to commit into a unique origin, regardless of the URL, as it is
  // restricted from accessing other origins.
  if (origin.unique())
    return true;

  // Standard URLs must match the reported origin.
  if (url.IsStandard() && !origin.IsSameOriginWith(url::Origin(url)))
    return false;

  // A non-unique origin must be a valid URL, which allows us to safely do a
  // conversion to GURL.
  GURL origin_url(origin.Serialize());

  // Verify that the origin is allowed to commit in this process.
  // Note: This also handles non-standard cases for |url|, such as
  // about:blank, data, and blob URLs.
  return CanCommitURL(origin_url);
}

void RenderFrameHostImpl::Navigate(
    const CommonNavigationParams& common_params,
    const StartNavigationParams& start_params,
    const RequestNavigationParams& request_params) {
  TRACE_EVENT0("navigation", "RenderFrameHostImpl::Navigate");
  DCHECK(!IsBrowserSideNavigationEnabled());

  UpdatePermissionsForNavigation(common_params, request_params);

  // Only send the message if we aren't suspended at the start of a cross-site
  // request.
  if (navigations_suspended_) {
    // This may replace an existing set of params, if this is a pending RFH that
    // is navigated twice consecutively.
    suspended_nav_params_.reset(
        new NavigationParams(common_params, start_params, request_params));
  } else {
    // Get back to a clean state, in case we start a new navigation without
    // completing an unload handler.
    ResetWaitingState();
    SendNavigateMessage(common_params, start_params, request_params);
  }

  // Force the throbber to start. This is done because Blink's "started loading"
  // message will be received asynchronously from the UI of the browser. But the
  // throbber needs to be kept in sync with what's happening in the UI. For
  // example, the throbber will start immediately when the user navigates even
  // if the renderer is delayed. There is also an issue with the throbber
  // starting because the WebUI (which controls whether the favicon is
  // displayed) happens synchronously. If the start loading messages was
  // asynchronous, then the default favicon would flash in.
  //
  // Blink doesn't send throb notifications for JavaScript URLs, so it is not
  // done here either.
  if (!common_params.url.SchemeIs(url::kJavaScriptScheme))
    OnDidStartLoading(true);
}

void RenderFrameHostImpl::NavigateToInterstitialURL(const GURL& data_url) {
  DCHECK(data_url.SchemeIs(url::kDataScheme));
  CommonNavigationParams common_params(
      data_url, Referrer(), ui::PAGE_TRANSITION_LINK,
      FrameMsg_Navigate_Type::NORMAL, false, false, base::TimeTicks::Now(),
      FrameMsg_UILoadMetricsReportType::NO_REPORT, GURL(), GURL(), LOFI_OFF,
      base::TimeTicks::Now(), "GET", nullptr);
  if (IsBrowserSideNavigationEnabled()) {
    CommitNavigation(nullptr, nullptr, common_params, RequestNavigationParams(),
                     false);
  } else {
    Navigate(common_params, StartNavigationParams(), RequestNavigationParams());
  }
}

void RenderFrameHostImpl::OpenURL(const FrameHostMsg_OpenURL_Params& params,
                                  SiteInstance* source_site_instance) {
  GURL validated_url(params.url);
  GetProcess()->FilterURL(false, &validated_url);

  TRACE_EVENT1("navigation", "RenderFrameHostImpl::OpenURL", "url",
               validated_url.possibly_invalid_spec());
  frame_tree_node_->navigator()->RequestOpenURL(
      this, validated_url, params.uses_post, params.resource_request_body,
      source_site_instance, params.referrer, params.disposition,
      params.should_replace_current_entry, params.user_gesture);
}

void RenderFrameHostImpl::Stop() {
  Send(new FrameMsg_Stop(routing_id_));
}

void RenderFrameHostImpl::DispatchBeforeUnload(bool for_navigation,
                                               bool is_reload) {
  DCHECK(for_navigation || !is_reload);

  // TODO(creis): Support beforeunload on subframes.  For now just pretend that
  // the handler ran and allowed the navigation to proceed.
  if (!ShouldDispatchBeforeUnload()) {
    DCHECK(!(IsBrowserSideNavigationEnabled() && for_navigation));
    frame_tree_node_->render_manager()->OnBeforeUnloadACK(
        for_navigation, true, base::TimeTicks::Now());
    return;
  }
  TRACE_EVENT_ASYNC_BEGIN1("navigation", "RenderFrameHostImpl BeforeUnload",
                           this, "&RenderFrameHostImpl", (void*)this);

  // This may be called more than once (if the user clicks the tab close button
  // several times, or if they click the tab close button then the browser close
  // button), and we only send the message once.
  if (is_waiting_for_beforeunload_ack_) {
    // Some of our close messages could be for the tab, others for cross-site
    // transitions. We always want to think it's for closing the tab if any
    // of the messages were, since otherwise it might be impossible to close
    // (if there was a cross-site "close" request pending when the user clicked
    // the close button). We want to keep the "for cross site" flag only if
    // both the old and the new ones are also for cross site.
    unload_ack_is_for_navigation_ =
        unload_ack_is_for_navigation_ && for_navigation;
  } else {
    // Start the hang monitor in case the renderer hangs in the beforeunload
    // handler.
    is_waiting_for_beforeunload_ack_ = true;
    unload_ack_is_for_navigation_ = for_navigation;
    // Increment the in-flight event count, to ensure that input events won't
    // cancel the timeout timer.
    render_view_host_->GetWidget()->increment_in_flight_event_count();
    render_view_host_->GetWidget()->StartHangMonitorTimeout(
        TimeDelta::FromMilliseconds(RenderViewHostImpl::kUnloadTimeoutMS),
        RenderWidgetHostDelegate::RENDERER_UNRESPONSIVE_BEFORE_UNLOAD);
    send_before_unload_start_time_ = base::TimeTicks::Now();
    Send(new FrameMsg_BeforeUnload(routing_id_, is_reload));
  }
}

void RenderFrameHostImpl::SimulateBeforeUnloadAck() {
  DCHECK(is_waiting_for_beforeunload_ack_);
  base::TimeTicks approx_renderer_start_time = send_before_unload_start_time_;
  OnBeforeUnloadACK(true, approx_renderer_start_time, base::TimeTicks::Now());
}

bool RenderFrameHostImpl::ShouldDispatchBeforeUnload() {
  // TODO(creis): Support beforeunload on subframes.
  return !GetParent() && IsRenderFrameLive();
}

void RenderFrameHostImpl::UpdateOpener() {
  // This frame (the frame whose opener is being updated) might not have had
  // proxies for the new opener chain in its SiteInstance.  Make sure they
  // exist.
  if (frame_tree_node_->opener()) {
    frame_tree_node_->opener()->render_manager()->CreateOpenerProxies(
        GetSiteInstance(), frame_tree_node_);
  }

  int opener_routing_id =
      frame_tree_node_->render_manager()->GetOpenerRoutingID(GetSiteInstance());
  Send(new FrameMsg_UpdateOpener(GetRoutingID(), opener_routing_id));
}

void RenderFrameHostImpl::SetFocusedFrame() {
  Send(new FrameMsg_SetFocusedFrame(routing_id_));
}

void RenderFrameHostImpl::ExtendSelectionAndDelete(size_t before,
                                                   size_t after) {
  Send(new InputMsg_ExtendSelectionAndDelete(routing_id_, before, after));
}

void RenderFrameHostImpl::JavaScriptDialogClosed(
    IPC::Message* reply_msg,
    bool success,
    const base::string16& user_input,
    bool dialog_was_suppressed) {
  GetProcess()->SetIgnoreInputEvents(false);
  bool is_waiting = is_waiting_for_beforeunload_ack_ || IsWaitingForUnloadACK();

  // If we are executing as part of (before)unload event handling, we don't
  // want to use the regular hung_renderer_delay_ms_ if the user has agreed to
  // leave the current page. In this case, use the regular timeout value used
  // during the (before)unload handling.
  if (is_waiting) {
    RenderWidgetHostDelegate::RendererUnresponsiveType type =
        RenderWidgetHostDelegate::RENDERER_UNRESPONSIVE_DIALOG_CLOSED;
    if (success) {
      type = is_waiting_for_beforeunload_ack_
                 ? RenderWidgetHostDelegate::RENDERER_UNRESPONSIVE_BEFORE_UNLOAD
                 : RenderWidgetHostDelegate::RENDERER_UNRESPONSIVE_UNLOAD;
    }
    render_view_host_->GetWidget()->StartHangMonitorTimeout(
        success
            ? TimeDelta::FromMilliseconds(RenderViewHostImpl::kUnloadTimeoutMS)
            : render_view_host_->GetWidget()->hung_renderer_delay(),
        type);
  }

  FrameHostMsg_RunJavaScriptMessage::WriteReplyParams(reply_msg,
                                                      success, user_input);
  Send(reply_msg);

  // If we are waiting for an unload or beforeunload ack and the user has
  // suppressed messages, kill the tab immediately; a page that's spamming
  // alerts in onbeforeunload is presumably malicious, so there's no point in
  // continuing to run its script and dragging out the process.
  // This must be done after sending the reply since RenderView can't close
  // correctly while waiting for a response.
  if (is_waiting && dialog_was_suppressed) {
    render_view_host_->GetWidget()->delegate()->RendererUnresponsive(
        render_view_host_->GetWidget(),
        RenderWidgetHostDelegate::RENDERER_UNRESPONSIVE_DIALOG_SUPPRESSED);
  }
}

// PlzNavigate
void RenderFrameHostImpl::CommitNavigation(
    ResourceResponse* response,
    std::unique_ptr<StreamHandle> body,
    const CommonNavigationParams& common_params,
    const RequestNavigationParams& request_params,
    bool is_view_source) {
  DCHECK((response && body.get()) ||
          !ShouldMakeNetworkRequestForURL(common_params.url));
  UpdatePermissionsForNavigation(common_params, request_params);

  // Get back to a clean state, in case we start a new navigation without
  // completing an unload handler.
  ResetWaitingState();

  // The renderer can exit view source mode when any error or cancellation
  // happen. When reusing the same renderer, overwrite to recover the mode.
  if (is_view_source &&
      this == frame_tree_node_->render_manager()->current_frame_host()) {
    DCHECK(!GetParent());
    render_view_host()->Send(new FrameMsg_EnableViewSourceMode(routing_id_));
  }

  const GURL body_url = body.get() ? body->GetURL() : GURL();
  const ResourceResponseHead head = response ?
      response->head : ResourceResponseHead();
  Send(new FrameMsg_CommitNavigation(routing_id_, head, body_url, common_params,
                                     request_params));

  // If a network request was made, update the LoFi state.
  if (ShouldMakeNetworkRequestForURL(common_params.url))
    last_navigation_lofi_state_ = common_params.lofi_state;

  // TODO(clamy): Release the stream handle once the renderer has finished
  // reading it.
  stream_handle_ = std::move(body);

  // When navigating to a Javascript url, no commit is expected from the
  // RenderFrameHost, nor should the throbber start. The NavigationRequest is
  // also not stored in the FrameTreeNode. Therefore do not reset it, as this
  // could cancel an existing pending navigation.
  if (!common_params.url.SchemeIs(url::kJavaScriptScheme)) {
    pending_commit_ = true;
    is_loading_ = true;
  }
}

void RenderFrameHostImpl::FailedNavigation(
    const CommonNavigationParams& common_params,
    const RequestNavigationParams& request_params,
    bool has_stale_copy_in_cache,
    int error_code) {
  // Get back to a clean state, in case a new navigation started without
  // completing an unload handler.
  ResetWaitingState();

  Send(new FrameMsg_FailedNavigation(routing_id_, common_params, request_params,
                                     has_stale_copy_in_cache, error_code));

  // An error page is expected to commit, hence why is_loading_ is set to true.
  is_loading_ = true;
  frame_tree_node_->ResetNavigationRequest(true);
}

void RenderFrameHostImpl::SetUpMojoIfNeeded() {
  if (interface_registry_.get())
    return;

  interface_registry_.reset(new shell::InterfaceRegistry(nullptr));
  if (!GetProcess()->GetRemoteInterfaces())
    return;

  RegisterMojoInterfaces();
  mojom::FrameFactoryPtr frame_factory;
  GetProcess()->GetRemoteInterfaces()->GetInterface(&frame_factory);
  frame_factory->CreateFrame(routing_id_, GetProxy(&frame_),
                             frame_host_binding_.CreateInterfacePtrAndBind());


  shell::mojom::InterfaceProviderPtr remote_interfaces;
  shell::mojom::InterfaceProviderRequest remote_interfaces_request =
      GetProxy(&remote_interfaces);
  remote_interfaces_.reset(new shell::InterfaceProvider);
  remote_interfaces_->Bind(std::move(remote_interfaces));
  frame_->GetInterfaceProvider(std::move(remote_interfaces_request));

#if defined(OS_ANDROID)
  service_registry_android_ =
      ServiceRegistryAndroid::Create(interface_registry_.get(),
                                     remote_interfaces_.get());
  ServiceRegistrarAndroid::RegisterFrameHostServices(
      service_registry_android_.get());
#endif
}

void RenderFrameHostImpl::InvalidateMojoConnection() {
#if defined(OS_ANDROID)
  // The Android-specific service registry has a reference to
  // |service_registry_| and thus must be torn down first.
  service_registry_android_.reset();
#endif

  interface_registry_.reset();
  frame_.reset();
  frame_host_binding_.Close();

  // Disconnect with ImageDownloader Mojo service in RenderFrame.
  mojo_image_downloader_.reset();
}

bool RenderFrameHostImpl::IsFocused() {
  return GetRenderWidgetHost()->is_focused() &&
         frame_tree_->GetFocusedFrame() &&
         (frame_tree_->GetFocusedFrame() == frame_tree_node() ||
          frame_tree_->GetFocusedFrame()->IsDescendantOf(frame_tree_node()));
}

bool RenderFrameHostImpl::UpdatePendingWebUI(const GURL& dest_url,
                                             int entry_bindings) {
  WebUI::TypeID new_web_ui_type =
      WebUIControllerFactoryRegistry::GetInstance()->GetWebUIType(
          GetSiteInstance()->GetBrowserContext(), dest_url);

  // If the required WebUI matches the pending WebUI or if it matches the
  // to-be-reused active WebUI, then leave everything as is.
  if (new_web_ui_type == pending_web_ui_type_ ||
      (should_reuse_web_ui_ && new_web_ui_type == web_ui_type_)) {
    return false;
  }

  // Reset the pending WebUI as from this point it will certainly not be reused.
  ClearPendingWebUI();

  // If this navigation is not to a WebUI, skip directly to bindings work.
  if (new_web_ui_type != WebUI::kNoWebUI) {
    if (new_web_ui_type == web_ui_type_) {
      // The active WebUI should be reused when dest_url requires a WebUI and
      // its type matches the current.
      DCHECK(web_ui_);
      should_reuse_web_ui_ = true;
    } else {
      // Otherwise create a new pending WebUI.
      pending_web_ui_ = delegate_->CreateWebUIForRenderFrameHost(dest_url);
      DCHECK(pending_web_ui_);
      pending_web_ui_type_ = new_web_ui_type;

      // If we have assigned (zero or more) bindings to the NavigationEntry in
      // the past, make sure we're not granting it different bindings than it
      // had before. If so, note it and don't give it any bindings, to avoid a
      // potential privilege escalation.
      if (entry_bindings != NavigationEntryImpl::kInvalidBindings &&
          pending_web_ui_->GetBindings() != entry_bindings) {
        RecordAction(
            base::UserMetricsAction("ProcessSwapBindingsMismatch_RVHM"));
        ClearPendingWebUI();
      }
    }
  }
  DCHECK_EQ(!pending_web_ui_, pending_web_ui_type_ == WebUI::kNoWebUI);

  // Either grant or check the RenderViewHost with/for proper bindings.
  if (pending_web_ui_ && !render_view_host_->GetProcess()->IsForGuestsOnly()) {
    // If a WebUI was created for the URL and the RenderView is not in a guest
    // process, then enable missing bindings with the RenderViewHost.
    int new_bindings = pending_web_ui_->GetBindings();
    if ((render_view_host_->GetEnabledBindings() & new_bindings) !=
        new_bindings) {
      render_view_host_->AllowBindings(new_bindings);
    }
  } else if (render_view_host_->is_active()) {
    // If the ongoing navigation is not to a WebUI or the RenderView is in a
    // guest process, ensure that we don't create an unprivileged RenderView in
    // a WebUI-enabled process unless it's swapped out.
    bool url_acceptable_for_webui =
        WebUIControllerFactoryRegistry::GetInstance()->IsURLAcceptableForWebUI(
            GetSiteInstance()->GetBrowserContext(), dest_url);
    if (!url_acceptable_for_webui) {
      CHECK(!ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
          GetProcess()->GetID()));
    }
  }
  return true;
}

void RenderFrameHostImpl::CommitPendingWebUI() {
  if (should_reuse_web_ui_) {
    should_reuse_web_ui_ = false;
  } else {
    web_ui_ = std::move(pending_web_ui_);
    web_ui_type_ = pending_web_ui_type_;
    pending_web_ui_type_ = WebUI::kNoWebUI;
  }
  DCHECK(!pending_web_ui_ && pending_web_ui_type_ == WebUI::kNoWebUI &&
         !should_reuse_web_ui_);
}

void RenderFrameHostImpl::ClearPendingWebUI() {
  pending_web_ui_.reset();
  pending_web_ui_type_ = WebUI::kNoWebUI;
  should_reuse_web_ui_ = false;
}

void RenderFrameHostImpl::ClearAllWebUI() {
  ClearPendingWebUI();
  web_ui_type_ = WebUI::kNoWebUI;
  web_ui_.reset();
}

const content::mojom::ImageDownloaderPtr&
RenderFrameHostImpl::GetMojoImageDownloader() {
  if (!mojo_image_downloader_.get() && GetRemoteInterfaces())
    GetRemoteInterfaces()->GetInterface(&mojo_image_downloader_);
  return mojo_image_downloader_;
}

void RenderFrameHostImpl::ResetLoadingState() {
  if (is_loading()) {
    // When pending deletion, just set the loading state to not loading.
    // Otherwise, OnDidStopLoading will take care of that, as well as sending
    // notification to the FrameTreeNode about the change in loading state.
    if (!is_active())
      is_loading_ = false;
    else
      OnDidStopLoading();
  }
}

void RenderFrameHostImpl::SuppressFurtherDialogs() {
  Send(new FrameMsg_SuppressFurtherDialogs(GetRoutingID()));
}

bool RenderFrameHostImpl::IsSameSiteInstance(
    RenderFrameHostImpl* other_render_frame_host) {
  // As a sanity check, make sure the frame belongs to the same BrowserContext.
  CHECK_EQ(GetSiteInstance()->GetBrowserContext(),
           other_render_frame_host->GetSiteInstance()->GetBrowserContext());
  return GetSiteInstance() == other_render_frame_host->GetSiteInstance();
}

void RenderFrameHostImpl::SetAccessibilityMode(AccessibilityMode mode) {
  Send(new FrameMsg_SetAccessibilityMode(routing_id_, mode));
}

void RenderFrameHostImpl::RequestAXTreeSnapshot(
    AXTreeSnapshotCallback callback) {
  static int next_id = 1;
  int callback_id = next_id++;
  Send(new AccessibilityMsg_SnapshotTree(routing_id_, callback_id));
  ax_tree_snapshot_callbacks_.insert(std::make_pair(callback_id, callback));
}

void RenderFrameHostImpl::SetAccessibilityCallbackForTesting(
    const base::Callback<void(RenderFrameHostImpl*, ui::AXEvent, int)>&
        callback) {
  accessibility_testing_callback_ = callback;
}

void RenderFrameHostImpl::UpdateAXTreeData() {
  AccessibilityMode accessibility_mode = delegate_->GetAccessibilityMode();
  if (accessibility_mode == AccessibilityModeOff || !is_active()) {
    return;
  }

  std::vector<AXEventNotificationDetails> details;
  details.reserve(1U);
  AXEventNotificationDetails detail;
  detail.ax_tree_id = GetAXTreeID();
  detail.update.has_tree_data = true;
  AXContentTreeDataToAXTreeData(&detail.update.tree_data);
  details.push_back(detail);

  if (browser_accessibility_manager_)
    browser_accessibility_manager_->OnAccessibilityEvents(details);

  delegate_->AccessibilityEventReceived(details);
}

void RenderFrameHostImpl::SetTextTrackSettings(
    const FrameMsg_TextTrackSettings_Params& params) {
  DCHECK(!GetParent());
  Send(new FrameMsg_SetTextTrackSettings(routing_id_, params));
}

const ui::AXTree* RenderFrameHostImpl::GetAXTreeForTesting() {
  return ax_tree_for_testing_.get();
}

BrowserAccessibilityManager*
    RenderFrameHostImpl::GetOrCreateBrowserAccessibilityManager() {
  RenderWidgetHostViewBase* view = GetViewForAccessibility();
  if (view &&
      !browser_accessibility_manager_ &&
      !no_create_browser_accessibility_manager_for_testing_) {
    bool is_root_frame = !frame_tree_node()->parent();
    browser_accessibility_manager_.reset(
        view->CreateBrowserAccessibilityManager(this, is_root_frame));
    if (browser_accessibility_manager_)
      UMA_HISTOGRAM_COUNTS("Accessibility.FrameEnabledCount", 1);
    else
      UMA_HISTOGRAM_COUNTS("Accessibility.FrameDidNotEnableCount", 1);
  }
  return browser_accessibility_manager_.get();
}

void RenderFrameHostImpl::ActivateFindInPageResultForAccessibility(
    int request_id) {
  AccessibilityMode accessibility_mode = delegate_->GetAccessibilityMode();
  if (accessibility_mode & AccessibilityModeFlagPlatform) {
    BrowserAccessibilityManager* manager =
        GetOrCreateBrowserAccessibilityManager();
    if (manager)
      manager->ActivateFindInPageResult(request_id);
  }
}

void RenderFrameHostImpl::InsertVisualStateCallback(
    const VisualStateCallback& callback) {
  static uint64_t next_id = 1;
  uint64_t key = next_id++;
  Send(new FrameMsg_VisualStateRequest(routing_id_, key));
  visual_state_callbacks_.insert(std::make_pair(key, callback));
}

bool RenderFrameHostImpl::IsRenderFrameLive() {
  bool is_live = GetProcess()->HasConnection() && render_frame_created_;

  // Sanity check: the RenderView should always be live if the RenderFrame is.
  DCHECK(!is_live || render_view_host_->IsRenderViewLive());

  return is_live;
}

int RenderFrameHostImpl::GetProxyCount() {
  if (this != frame_tree_node_->current_frame_host())
    return 0;
  return frame_tree_node_->render_manager()->GetProxyCount();
}

void RenderFrameHostImpl::FilesSelectedInChooser(
    const std::vector<content::FileChooserFileInfo>& files,
    FileChooserParams::Mode permissions) {
  storage::FileSystemContext* const file_system_context =
      BrowserContext::GetStoragePartition(GetProcess()->GetBrowserContext(),
                                          GetSiteInstance())
          ->GetFileSystemContext();
  // Grant the security access requested to the given files.
  for (const auto& file : files) {
    if (permissions == FileChooserParams::Save) {
      ChildProcessSecurityPolicyImpl::GetInstance()->GrantCreateReadWriteFile(
          GetProcess()->GetID(), file.file_path);
    } else {
      ChildProcessSecurityPolicyImpl::GetInstance()->GrantReadFile(
          GetProcess()->GetID(), file.file_path);
    }
    if (file.file_system_url.is_valid()) {
      ChildProcessSecurityPolicyImpl::GetInstance()->GrantReadFileSystem(
          GetProcess()->GetID(),
          file_system_context->CrackURL(file.file_system_url)
              .mount_filesystem_id());
    }
  }

  Send(new FrameMsg_RunFileChooserResponse(routing_id_, files));
}

void RenderFrameHostImpl::GetInterfaceProvider(
    shell::mojom::InterfaceProviderRequest interfaces) {
  interface_registry_->Bind(std::move(interfaces));
}

#if defined(USE_EXTERNAL_POPUP_MENU)
#if defined(OS_MACOSX)

void RenderFrameHostImpl::DidSelectPopupMenuItem(int selected_index) {
  Send(new FrameMsg_SelectPopupMenuItem(routing_id_, selected_index));
}

void RenderFrameHostImpl::DidCancelPopupMenu() {
  Send(new FrameMsg_SelectPopupMenuItem(routing_id_, -1));
}

#else

void RenderFrameHostImpl::DidSelectPopupMenuItems(
    const std::vector<int>& selected_indices) {
  Send(new FrameMsg_SelectPopupMenuItems(routing_id_, false, selected_indices));
}

void RenderFrameHostImpl::DidCancelPopupMenu() {
  Send(new FrameMsg_SelectPopupMenuItems(
      routing_id_, true, std::vector<int>()));
}

#endif
#endif

void RenderFrameHostImpl::SetNavigationsSuspended(
    bool suspend,
    const base::TimeTicks& proceed_time) {
  // This should only be called to toggle the state.
  DCHECK(navigations_suspended_ != suspend);

  navigations_suspended_ = suspend;
  if (navigations_suspended_) {
    TRACE_EVENT_ASYNC_BEGIN0("navigation",
                             "RenderFrameHostImpl navigation suspended", this);
  } else {
    TRACE_EVENT_ASYNC_END0("navigation",
                           "RenderFrameHostImpl navigation suspended", this);
  }

  if (!suspend && suspended_nav_params_) {
    // There's navigation message params waiting to be sent. Now that we're not
    // suspended anymore, resume navigation by sending them.
    ResetWaitingState();

    DCHECK(!proceed_time.is_null());
    // TODO(csharrison): Make sure that PlzNavigate and the current architecture
    // measure navigation start in the same way in the presence of the
    // BeforeUnload event.
    suspended_nav_params_->common_params.navigation_start = proceed_time;
    SendNavigateMessage(suspended_nav_params_->common_params,
                        suspended_nav_params_->start_params,
                        suspended_nav_params_->request_params);
    suspended_nav_params_.reset();
  }
}

void RenderFrameHostImpl::CancelSuspendedNavigations() {
  // Clear any state if a pending navigation is canceled or preempted.
  if (suspended_nav_params_)
    suspended_nav_params_.reset();

  TRACE_EVENT_ASYNC_END0("navigation",
                         "RenderFrameHostImpl navigation suspended", this);
  navigations_suspended_ = false;
}

void RenderFrameHostImpl::SendNavigateMessage(
    const CommonNavigationParams& common_params,
    const StartNavigationParams& start_params,
    const RequestNavigationParams& request_params) {
  RenderFrameDevToolsAgentHost::OnBeforeNavigation(
      frame_tree_node_->current_frame_host(), this);
  Send(new FrameMsg_Navigate(
      routing_id_, common_params, start_params, request_params));
}

void RenderFrameHostImpl::DidUseGeolocationPermission() {
  PermissionManager* permission_manager =
      GetSiteInstance()->GetBrowserContext()->GetPermissionManager();
  if (!permission_manager)
    return;

  permission_manager->RegisterPermissionUsage(
      PermissionType::GEOLOCATION,
      last_committed_url().GetOrigin(),
      frame_tree_node()->frame_tree()->GetMainFrame()
          ->last_committed_url().GetOrigin());
}

bool RenderFrameHostImpl::CanAccessFilesOfPageState(const PageState& state) {
  return ChildProcessSecurityPolicyImpl::GetInstance()->CanReadAllFiles(
      GetProcess()->GetID(), state.GetReferencedFiles());
}

void RenderFrameHostImpl::GrantFileAccessFromPageState(const PageState& state) {
  GrantFileAccess(GetProcess()->GetID(), state.GetReferencedFiles());
}

void RenderFrameHostImpl::GrantFileAccessFromResourceRequestBody(
    const ResourceRequestBodyImpl& body) {
  GrantFileAccess(GetProcess()->GetID(), body.GetReferencedFiles());
}

void RenderFrameHostImpl::UpdatePermissionsForNavigation(
    const CommonNavigationParams& common_params,
    const RequestNavigationParams& request_params) {
  // Browser plugin guests are not allowed to navigate outside web-safe schemes,
  // so do not grant them the ability to request additional URLs.
  if (!GetProcess()->IsForGuestsOnly()) {
    ChildProcessSecurityPolicyImpl::GetInstance()->GrantRequestURL(
        GetProcess()->GetID(), common_params.url);
    if (common_params.url.SchemeIs(url::kDataScheme) &&
        common_params.base_url_for_data_url.SchemeIs(url::kFileScheme)) {
      // If 'data:' is used, and we have a 'file:' base url, grant access to
      // local files.
      ChildProcessSecurityPolicyImpl::GetInstance()->GrantRequestURL(
          GetProcess()->GetID(), common_params.base_url_for_data_url);
    }
  }

  // We may be returning to an existing NavigationEntry that had been granted
  // file access.  If this is a different process, we will need to grant the
  // access again.  Abuse is prevented, because the files listed in the page
  // state are validated earlier, when they are received from the renderer (in
  // RenderFrameHostImpl::CanAccessFilesOfPageState).
  if (request_params.page_state.IsValid())
    GrantFileAccessFromPageState(request_params.page_state);

  // We may be here after transferring navigation to a different renderer
  // process.  In this case, we need to ensure that the new renderer retains
  // ability to access files that the old renderer could access.  Abuse is
  // prevented, because the files listed in ResourceRequestBody are validated
  // earlier, when they are recieved from the renderer (in ShouldServiceRequest
  // called from ResourceDispatcherHostImpl::BeginRequest).
  if (common_params.post_data)
    GrantFileAccessFromResourceRequestBody(*common_params.post_data);
}

bool RenderFrameHostImpl::CanExecuteJavaScript() {
  return g_allow_injecting_javascript ||
         !frame_tree_node_->current_url().is_valid() ||
         frame_tree_node_->current_url().SchemeIs(kChromeDevToolsScheme) ||
         ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
             GetProcess()->GetID()) ||
         // It's possible to load about:blank in a Web UI renderer.
         // See http://crbug.com/42547
         (frame_tree_node_->current_url().spec() == url::kAboutBlankURL) ||
         // InterstitialPageImpl should be the only case matching this.
         (delegate_->GetAsWebContents() == nullptr);
}

AXTreeIDRegistry::AXTreeID RenderFrameHostImpl::RoutingIDToAXTreeID(
    int routing_id) {
  RenderFrameHostImpl* rfh = nullptr;
  RenderFrameProxyHost* rfph = RenderFrameProxyHost::FromID(
      GetProcess()->GetID(), routing_id);
  if (rfph) {
    FrameTree* frame_tree = rfph->frame_tree_node()->frame_tree();
    FrameTreeNode* frame_tree_node = frame_tree->FindByRoutingID(
        GetProcess()->GetID(), routing_id);
    rfh = frame_tree_node->render_manager()->current_frame_host();
  } else {
    rfh = RenderFrameHostImpl::FromID(GetProcess()->GetID(), routing_id);

    // As a sanity check, make sure we're within the same frame tree and
    // crash the renderer if not.
    if (rfh &&
        rfh->frame_tree_node()->frame_tree() !=
            frame_tree_node()->frame_tree()) {
      AccessibilityFatalError();
      return AXTreeIDRegistry::kNoAXTreeID;
    }
  }

  if (!rfh)
    return AXTreeIDRegistry::kNoAXTreeID;

  return rfh->GetAXTreeID();
}

AXTreeIDRegistry::AXTreeID
RenderFrameHostImpl::BrowserPluginInstanceIDToAXTreeID(
    int instance_id) {
  RenderFrameHostImpl* guest = static_cast<RenderFrameHostImpl*>(
      delegate()->GetGuestByInstanceID(this, instance_id));
  if (!guest)
    return AXTreeIDRegistry::kNoAXTreeID;

  guest->set_browser_plugin_embedder_ax_tree_id(GetAXTreeID());

  return guest->GetAXTreeID();
}

void RenderFrameHostImpl::AXContentNodeDataToAXNodeData(
    const AXContentNodeData& src,
    ui::AXNodeData* dst) {
  // Copy the common fields.
  *dst = src;

  // Map content-specific attributes based on routing IDs or browser plugin
  // instance IDs to generic attributes with global AXTreeIDs.
  for (auto iter : src.content_int_attributes) {
    AXContentIntAttribute attr = iter.first;
    int32_t value = iter.second;
    switch (attr) {
      case AX_CONTENT_ATTR_CHILD_ROUTING_ID:
        dst->int_attributes.push_back(std::make_pair(
            ui::AX_ATTR_CHILD_TREE_ID, RoutingIDToAXTreeID(value)));
        break;
      case AX_CONTENT_ATTR_CHILD_BROWSER_PLUGIN_INSTANCE_ID:
        dst->int_attributes.push_back(std::make_pair(
            ui::AX_ATTR_CHILD_TREE_ID,
            BrowserPluginInstanceIDToAXTreeID(value)));
        break;
      case AX_CONTENT_INT_ATTRIBUTE_LAST:
        NOTREACHED();
        break;
    }
  }
}

void RenderFrameHostImpl::AXContentTreeDataToAXTreeData(
    ui::AXTreeData* dst) {
  const AXContentTreeData& src = ax_content_tree_data_;

  // Copy the common fields.
  *dst = src;

  if (src.routing_id != -1)
    dst->tree_id = RoutingIDToAXTreeID(src.routing_id);

  if (src.parent_routing_id != -1)
    dst->parent_tree_id = RoutingIDToAXTreeID(src.parent_routing_id);

  if (browser_plugin_embedder_ax_tree_id_ != AXTreeIDRegistry::kNoAXTreeID)
    dst->parent_tree_id = browser_plugin_embedder_ax_tree_id_;

  // If this is not the root frame tree node, we're done.
  if (frame_tree_node()->parent())
    return;

  // For the root frame tree node, also store the AXTreeID of the focused frame.
  FrameTreeNode* focused_frame_tree_node = frame_tree_->GetFocusedFrame();
  if (!focused_frame_tree_node)
    return;
  RenderFrameHostImpl* focused_frame =
      focused_frame_tree_node->current_frame_host();
  DCHECK(focused_frame);
  dst->focused_tree_id = focused_frame->GetAXTreeID();
}

void RenderFrameHostImpl::CreateWebBluetoothService(
    blink::mojom::WebBluetoothServiceRequest request) {
  DCHECK(!web_bluetooth_service_);
  web_bluetooth_service_.reset(
      new WebBluetoothServiceImpl(this, std::move(request)));
  // RFHI owns web_bluetooth_service_ and web_bluetooth_service owns the
  // binding_ which may run the error handler. binding_ can't run the error
  // handler after it's destroyed so it can't run after the RFHI is destroyed.
  web_bluetooth_service_->SetClientConnectionErrorHandler(base::Bind(
      &RenderFrameHostImpl::DeleteWebBluetoothService, base::Unretained(this)));
}

void RenderFrameHostImpl::DeleteWebBluetoothService() {
  web_bluetooth_service_.reset();
}

}  // namespace content
