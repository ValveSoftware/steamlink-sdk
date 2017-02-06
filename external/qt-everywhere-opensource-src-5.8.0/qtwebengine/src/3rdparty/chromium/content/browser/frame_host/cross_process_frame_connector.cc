// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/cross_process_frame_connector.h"

#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_manager.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_manager.h"
#include "content/browser/frame_host/render_frame_proxy_host.h"
#include "content/browser/frame_host/render_widget_host_view_child_frame.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/common/frame_messages.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace content {

CrossProcessFrameConnector::CrossProcessFrameConnector(
    RenderFrameProxyHost* frame_proxy_in_parent_renderer)
    : frame_proxy_in_parent_renderer_(frame_proxy_in_parent_renderer),
      view_(nullptr),
      device_scale_factor_(1) {
}

CrossProcessFrameConnector::~CrossProcessFrameConnector() {
  if (view_)
    view_->SetCrossProcessFrameConnector(nullptr);
}

bool CrossProcessFrameConnector::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;

  IPC_BEGIN_MESSAGE_MAP(CrossProcessFrameConnector, msg)
    IPC_MESSAGE_HANDLER(FrameHostMsg_ForwardInputEvent, OnForwardInputEvent)
    IPC_MESSAGE_HANDLER(FrameHostMsg_FrameRectChanged, OnFrameRectChanged)
    IPC_MESSAGE_HANDLER(FrameHostMsg_VisibilityChanged, OnVisibilityChanged)
    IPC_MESSAGE_HANDLER(FrameHostMsg_InitializeChildFrame,
                        OnInitializeChildFrame)
    IPC_MESSAGE_HANDLER(FrameHostMsg_SatisfySequence, OnSatisfySequence)
    IPC_MESSAGE_HANDLER(FrameHostMsg_RequireSequence, OnRequireSequence)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void CrossProcessFrameConnector::set_view(
    RenderWidgetHostViewChildFrame* view) {
  // Detach ourselves from the previous |view_|.
  if (view_)
    view_->SetCrossProcessFrameConnector(nullptr);

  view_ = view;

  // Attach ourselves to the new view and size it appropriately.
  if (view_) {
    view_->SetCrossProcessFrameConnector(this);
    SetDeviceScaleFactor(device_scale_factor_);
    SetRect(child_frame_rect_);
  }
}

void CrossProcessFrameConnector::RenderProcessGone() {
  frame_proxy_in_parent_renderer_->Send(new FrameMsg_ChildFrameProcessGone(
      frame_proxy_in_parent_renderer_->GetRoutingID()));
}

void CrossProcessFrameConnector::SetChildFrameSurface(
    const cc::SurfaceId& surface_id,
    const gfx::Size& frame_size,
    float scale_factor,
    const cc::SurfaceSequence& sequence) {
  frame_proxy_in_parent_renderer_->Send(new FrameMsg_SetChildFrameSurface(
      frame_proxy_in_parent_renderer_->GetRoutingID(), surface_id, frame_size,
      scale_factor, sequence));
}

void CrossProcessFrameConnector::OnSatisfySequence(
    const cc::SurfaceSequence& sequence) {
  std::vector<uint32_t> sequences;
  sequences.push_back(sequence.sequence);
  cc::SurfaceManager* manager = GetSurfaceManager();
  manager->DidSatisfySequences(sequence.id_namespace, &sequences);
}

void CrossProcessFrameConnector::OnRequireSequence(
    const cc::SurfaceId& id,
    const cc::SurfaceSequence& sequence) {
  cc::SurfaceManager* manager = GetSurfaceManager();
  cc::Surface* surface = manager->GetSurfaceForId(id);
  if (!surface) {
    LOG(ERROR) << "Attempting to require callback on nonexistent surface";
    return;
  }
  surface->AddDestructionDependency(sequence);
}

void CrossProcessFrameConnector::OnInitializeChildFrame(float scale_factor) {
  if (scale_factor != device_scale_factor_)
    SetDeviceScaleFactor(scale_factor);
}

gfx::Rect CrossProcessFrameConnector::ChildFrameRect() {
  return child_frame_rect_;
}

void CrossProcessFrameConnector::GetScreenInfo(blink::WebScreenInfo* results) {
  auto parent_view = GetParentRenderWidgetHostView();
  if (parent_view) {
    parent_view->GetScreenInfo(results);
  }
}

void CrossProcessFrameConnector::UpdateCursor(const WebCursor& cursor) {
  RenderWidgetHostViewBase* root_view = GetRootRenderWidgetHostView();
  if (root_view)
    root_view->UpdateCursor(cursor);
}

gfx::Point CrossProcessFrameConnector::TransformPointToRootCoordSpace(
    const gfx::Point& point,
    cc::SurfaceId surface_id) {
  gfx::Point transformed_point = point;
  RenderWidgetHostViewBase* root_view = GetRootRenderWidgetHostView();
  if (root_view)
    root_view->TransformPointToLocalCoordSpace(point, surface_id,
                                               &transformed_point);
  return transformed_point;
}

void CrossProcessFrameConnector::ForwardProcessAckedTouchEvent(
    const TouchEventWithLatencyInfo& touch,
    InputEventAckState ack_result) {
  auto main_view = GetRootRenderWidgetHostView();
  if (main_view)
    main_view->ProcessAckedTouchEvent(touch, ack_result);
}

void CrossProcessFrameConnector::BubbleScrollEvent(
    const blink::WebInputEvent& event) {
  auto parent_view = GetParentRenderWidgetHostView();

  if (!parent_view)
    return;

  gfx::Vector2d offset_from_parent = child_frame_rect_.OffsetFromOrigin();
  if (event.type == blink::WebInputEvent::GestureScrollUpdate) {
    blink::WebGestureEvent resent_gesture_event;
    memcpy(&resent_gesture_event, &event, sizeof(resent_gesture_event));
    resent_gesture_event.x += offset_from_parent.x();
    resent_gesture_event.y += offset_from_parent.y();
    // TODO(wjmaclean, kenrb): The resendingPluginId field is used by
    // BrowserPlugin to associate bubbled events with each plugin, which is
    // not needed for OOPIFs. However the field needs to be set in order
    // to prompt the parent frame's RenderWidgetHostImpl to
    // manage the gesture scroll event lifetime (in particular creating the
    // GestureScrollBegin and GestureScrollEnd events). This can be converted
    // to a flag or otherwise refactored out when BrowserPlugin supporting
    // code is eventually removed (https://crbug.com/533069).
    resent_gesture_event.resendingPluginId = 1;
    ui::LatencyInfo latency_info;
    parent_view->ProcessGestureEvent(resent_gesture_event, latency_info);
  } else if (event.type == blink::WebInputEvent::MouseWheel) {
    blink::WebMouseWheelEvent resent_wheel_event;
    memcpy(&resent_wheel_event, &event, sizeof(resent_wheel_event));
    resent_wheel_event.x += offset_from_parent.x();
    resent_wheel_event.y += offset_from_parent.y();
    // TODO(wjmaclean): Initialize latency info correctly for OOPIFs.
    // https://crbug.com/613628
    ui::LatencyInfo latency_info;
    parent_view->ProcessMouseWheelEvent(resent_wheel_event, latency_info);
  } else {
    NOTIMPLEMENTED();
  }
}

bool CrossProcessFrameConnector::HasFocus() {
  RenderWidgetHostViewBase* root_view = GetRootRenderWidgetHostView();
  if (root_view)
    return root_view->HasFocus();
  return false;
}

void CrossProcessFrameConnector::FocusRootView() {
  RenderWidgetHostViewBase* root_view = GetRootRenderWidgetHostView();
  if (root_view)
    root_view->Focus();
}

bool CrossProcessFrameConnector::LockMouse() {
  RenderWidgetHostViewBase* root_view = GetRootRenderWidgetHostView();
  if (root_view)
    return root_view->LockMouse();
  return false;
}

void CrossProcessFrameConnector::UnlockMouse() {
  RenderWidgetHostViewBase* root_view = GetRootRenderWidgetHostView();
  if (root_view)
    root_view->UnlockMouse();
}

void CrossProcessFrameConnector::OnForwardInputEvent(
    const blink::WebInputEvent* event) {
  if (!view_)
    return;

  RenderFrameHostManager* manager =
      frame_proxy_in_parent_renderer_->frame_tree_node()->render_manager();
  RenderWidgetHostImpl* parent_widget =
      manager->ForInnerDelegate()
          ? manager->GetOuterRenderWidgetHostForKeyboardInput()
          : frame_proxy_in_parent_renderer_->GetRenderViewHost()->GetWidget();

  // TODO(wjmaclean): We should remove these forwarding functions, since they
  // are directly target using RenderWidgetHostInputEventRouter. But neither
  // pathway is currently handling gesture events, so that needs to be fixed
  // in a subsequent CL.
  if (blink::WebInputEvent::isKeyboardEventType(event->type)) {
    if (!parent_widget->GetLastKeyboardEvent())
      return;
    NativeWebKeyboardEvent keyboard_event(
        *parent_widget->GetLastKeyboardEvent());
    view_->ProcessKeyboardEvent(keyboard_event);
    return;
  }

  if (blink::WebInputEvent::isMouseEventType(event->type)) {
    // TODO(wjmaclean): Initialize latency info correctly for OOPIFs.
    // https://crbug.com/613628
    ui::LatencyInfo latency_info;
    view_->ProcessMouseEvent(*static_cast<const blink::WebMouseEvent*>(event),
                              latency_info);
    return;
  }

  if (event->type == blink::WebInputEvent::MouseWheel) {
    // TODO(wjmaclean): Initialize latency info correctly for OOPIFs.
    // https://crbug.com/613628
    ui::LatencyInfo latency_info;
    view_->ProcessMouseWheelEvent(
        *static_cast<const blink::WebMouseWheelEvent*>(event), latency_info);
    return;
  }
}

void CrossProcessFrameConnector::OnFrameRectChanged(
    const gfx::Rect& frame_rect) {
  if (!frame_rect.size().IsEmpty())
    SetRect(frame_rect);
}

void CrossProcessFrameConnector::OnVisibilityChanged(bool visible) {
  if (!view_)
    return;

  // If there is an inner WebContents, it should be notified of the change in
  // the visibility. The Show/Hide methods will not be called if an inner
  // WebContents exists since the corresponding WebContents will itself call
  // Show/Hide on all the RenderWidgetHostViews (including this) one.
  if (frame_proxy_in_parent_renderer_->frame_tree_node()
          ->render_manager()
          ->ForInnerDelegate()) {
    RenderWidgetHostImpl::From(view_->GetRenderWidgetHost())
        ->delegate()
        ->OnRenderFrameProxyVisibilityChanged(visible);
    return;
  }

  if (visible)
    view_->Show();
  else
    view_->Hide();
}

void CrossProcessFrameConnector::SetDeviceScaleFactor(float scale_factor) {
  device_scale_factor_ = scale_factor;
  // The RenderWidgetHost is null in unit tests.
  if (view_ && view_->GetRenderWidgetHost()) {
    RenderWidgetHostImpl* child_widget =
        RenderWidgetHostImpl::From(view_->GetRenderWidgetHost());
    child_widget->NotifyScreenInfoChanged();
  }
}

void CrossProcessFrameConnector::SetRect(const gfx::Rect& frame_rect) {
  gfx::Rect old_rect = child_frame_rect_;
  child_frame_rect_ = frame_rect;
  if (view_) {
    view_->SetBounds(frame_rect);

    // Other local root frames nested underneath this one implicitly have their
    // view rects changed when their ancestor is repositioned, and therefore
    // need to have their screen rects updated.
    FrameTreeNode* proxy_node =
        frame_proxy_in_parent_renderer_->frame_tree_node();
    if (old_rect.x() != child_frame_rect_.x() ||
        old_rect.y() != child_frame_rect_.y()) {
      for (FrameTreeNode* node :
           proxy_node->frame_tree()->SubtreeNodes(proxy_node)) {
        if (node != proxy_node && node->current_frame_host()->is_local_root())
          node->current_frame_host()->GetRenderWidgetHost()->SendScreenRects();
      }
    }
  }
}

RenderWidgetHostViewBase*
CrossProcessFrameConnector::GetRootRenderWidgetHostView() {
  RenderFrameHostImpl* top_host = frame_proxy_in_parent_renderer_->
      frame_tree_node()->frame_tree()->root()->current_frame_host();

  // This method should return the root RWHV from the top-level WebContents,
  // in the case of nested WebContents.
  while (top_host->frame_tree_node()->render_manager()->ForInnerDelegate()) {
    top_host = top_host->frame_tree_node()->render_manager()->
        GetOuterDelegateNode()->frame_tree()->root()->current_frame_host();
  }

  return static_cast<RenderWidgetHostViewBase*>(top_host->GetView());
}

RenderWidgetHostViewBase*
CrossProcessFrameConnector::GetParentRenderWidgetHostView() {
  FrameTreeNode* parent =
      frame_proxy_in_parent_renderer_->frame_tree_node()->parent();

  if (!parent &&
      frame_proxy_in_parent_renderer_->frame_tree_node()
          ->render_manager()
          ->GetOuterDelegateNode()) {
    parent = frame_proxy_in_parent_renderer_->frame_tree_node()
                 ->render_manager()
                 ->GetOuterDelegateNode()
                 ->parent();
  }

  if (parent) {
    return static_cast<RenderWidgetHostViewBase*>(
        parent->current_frame_host()->GetView());
  }

  return nullptr;
}

}  // namespace content
