// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/cross_process_frame_connector.h"

#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_hittest.h"
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
#include "content/browser/renderer_host/render_widget_host_input_event_router.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/common/frame_messages.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "ui/gfx/geometry/dip_util.h"

namespace content {

CrossProcessFrameConnector::CrossProcessFrameConnector(
    RenderFrameProxyHost* frame_proxy_in_parent_renderer)
    : frame_proxy_in_parent_renderer_(frame_proxy_in_parent_renderer),
      view_(nullptr),
      is_scroll_bubbling_(false) {}

CrossProcessFrameConnector::~CrossProcessFrameConnector() {
  // Notify the view of this object being destroyed, if the view still exists.
  set_view(nullptr);
}

bool CrossProcessFrameConnector::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;

  IPC_BEGIN_MESSAGE_MAP(CrossProcessFrameConnector, msg)
    IPC_MESSAGE_HANDLER(FrameHostMsg_ForwardInputEvent, OnForwardInputEvent)
    IPC_MESSAGE_HANDLER(FrameHostMsg_FrameRectChanged, OnFrameRectChanged)
    IPC_MESSAGE_HANDLER(FrameHostMsg_VisibilityChanged, OnVisibilityChanged)
    IPC_MESSAGE_HANDLER(FrameHostMsg_SatisfySequence, OnSatisfySequence)
    IPC_MESSAGE_HANDLER(FrameHostMsg_RequireSequence, OnRequireSequence)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void CrossProcessFrameConnector::set_view(
    RenderWidgetHostViewChildFrame* view) {
  // Detach ourselves from the previous |view_|.
  if (view_) {
    // The RenderWidgetHostDelegate needs to be checked because set_view() can
    // be called during nested WebContents destruction. See
    // https://crbug.com/644306.
    if (is_scroll_bubbling_ && GetParentRenderWidgetHostView() &&
        RenderWidgetHostImpl::From(
            GetParentRenderWidgetHostView()->GetRenderWidgetHost())
            ->delegate()) {
      RenderWidgetHostImpl::From(
          GetParentRenderWidgetHostView()->GetRenderWidgetHost())
          ->delegate()
          ->GetInputEventRouter()
          ->CancelScrollBubbling(view_);
      is_scroll_bubbling_ = false;
    }
    view_->SetCrossProcessFrameConnector(nullptr);
  }

  view_ = view;

  // Attach ourselves to the new view and size it appropriately.
  if (view_) {
    view_->SetCrossProcessFrameConnector(this);
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
  manager->DidSatisfySequences(sequence.frame_sink_id, &sequences);
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

gfx::Rect CrossProcessFrameConnector::ChildFrameRect() {
  return child_frame_rect_;
}

void CrossProcessFrameConnector::UpdateCursor(const WebCursor& cursor) {
  RenderWidgetHostViewBase* root_view = GetRootRenderWidgetHostView();
  if (root_view)
    root_view->UpdateCursor(cursor);
}

gfx::Point CrossProcessFrameConnector::TransformPointToRootCoordSpace(
    const gfx::Point& point,
    const cc::SurfaceId& surface_id) {
  gfx::Point transformed_point;
  TransformPointToCoordSpaceForView(point, GetRootRenderWidgetHostView(),
                                    surface_id, &transformed_point);
  return transformed_point;
}

bool CrossProcessFrameConnector::TransformPointToLocalCoordSpace(
    const gfx::Point& point,
    const cc::SurfaceId& original_surface,
    const cc::SurfaceId& local_surface_id,
    gfx::Point* transformed_point) {
  if (original_surface == local_surface_id) {
    *transformed_point = point;
    return true;
  }

  // Transformations use physical pixels rather than DIP, so conversion
  // is necessary.
  *transformed_point =
      gfx::ConvertPointToPixel(view_->current_surface_scale_factor(), point);
  cc::SurfaceHittest hittest(nullptr, GetSurfaceManager());
  if (!hittest.TransformPointToTargetSurface(original_surface, local_surface_id,
                                             transformed_point))
    return false;

  *transformed_point = gfx::ConvertPointToDIP(
      view_->current_surface_scale_factor(), *transformed_point);
  return true;
}

bool CrossProcessFrameConnector::TransformPointToCoordSpaceForView(
    const gfx::Point& point,
    RenderWidgetHostViewBase* target_view,
    const cc::SurfaceId& local_surface_id,
    gfx::Point* transformed_point) {
  RenderWidgetHostViewBase* root_view = GetRootRenderWidgetHostView();
  if (!root_view)
    return false;

  // It is possible that neither the original surface or target surface is an
  // ancestor of the other in the RenderWidgetHostView tree (e.g. they could
  // be siblings). To account for this, the point is first transformed into the
  // root coordinate space and then the root is asked to perform the conversion.
  if (!root_view->TransformPointToLocalCoordSpace(point, local_surface_id,
                                                  transformed_point))
    return false;

  if (target_view == root_view)
    return true;

  return root_view->TransformPointToCoordSpaceForView(
      *transformed_point, target_view, transformed_point);
}

void CrossProcessFrameConnector::ForwardProcessAckedTouchEvent(
    const TouchEventWithLatencyInfo& touch,
    InputEventAckState ack_result) {
  auto* main_view = GetRootRenderWidgetHostView();
  if (main_view)
    main_view->ProcessAckedTouchEvent(touch, ack_result);
}

void CrossProcessFrameConnector::BubbleScrollEvent(
    const blink::WebGestureEvent& event) {
  DCHECK(event.type == blink::WebInputEvent::GestureScrollUpdate ||
         event.type == blink::WebInputEvent::GestureScrollEnd);
  auto* parent_view = GetParentRenderWidgetHostView();

  if (!parent_view)
    return;

  auto* event_router =
      RenderWidgetHostImpl::From(parent_view->GetRenderWidgetHost())
          ->delegate()
          ->GetInputEventRouter();

  gfx::Vector2d offset_from_parent = child_frame_rect_.OffsetFromOrigin();
  blink::WebGestureEvent resent_gesture_event(event);
  // TODO(kenrb, wjmaclean): Do we need to account for transforms here?
  // See https://crbug.com/626020.
  resent_gesture_event.x += offset_from_parent.x();
  resent_gesture_event.y += offset_from_parent.y();
  if (event.type == blink::WebInputEvent::GestureScrollUpdate) {
    event_router->BubbleScrollEvent(parent_view, resent_gesture_event);
    is_scroll_bubbling_ = true;
  } else if (event.type == blink::WebInputEvent::GestureScrollEnd &&
             is_scroll_bubbling_) {
    event_router->BubbleScrollEvent(parent_view, resent_gesture_event);
    is_scroll_bubbling_ = false;
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

  if (visible &&
      !RenderWidgetHostImpl::From(view_->GetRenderWidgetHost())
           ->delegate()
           ->IsHidden()) {
    view_->Show();
  } else if (!visible) {
    view_->Hide();
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
