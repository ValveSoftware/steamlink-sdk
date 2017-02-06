// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/swapped_out_messages.h"

#include "content/common/accessibility_messages.h"
#include "content/common/frame_messages.h"
#include "content/common/input_messages.h"
#include "content/common/view_messages.h"
#include "content/public/common/content_client.h"

namespace content {

bool SwappedOutMessages::CanSendWhileSwappedOut(const IPC::Message* msg) {
  // We filter out most IPC messages when swapped out.  However, some are
  // important (e.g., ACKs) for keeping the browser and renderer state
  // consistent in case we later return to the same renderer.
  switch (msg->type()) {
    // Handled by RenderWidgetHost.
    case InputHostMsg_HandleInputEvent_ACK::ID:
    case ViewHostMsg_UpdateRect::ID:
    // Allow targeted navigations while swapped out.
    case FrameHostMsg_OpenURL::ID:
    case ViewHostMsg_Focus::ID:
    // Handled by RenderViewHost.
    case ViewHostMsg_ClosePage_ACK::ID:
    case ViewHostMsg_SwapCompositorFrame::ID:
    // Handled by SharedWorkerMessageFilter.
    case ViewHostMsg_DocumentDetached::ID:
    // Allow cross-process JavaScript calls.
    case ViewHostMsg_RouteCloseEvent::ID:
    // Send page scale factor reset notification upon cross-process navigations.
    case ViewHostMsg_PageScaleFactorChanged::ID:
    // Handled by RenderFrameHost.
    case FrameHostMsg_BeforeUnload_ACK::ID:
    case FrameHostMsg_SwapOut_ACK::ID:
    case FrameHostMsg_RenderProcessGone::ID:
    // Frame detach must occur after the RenderView has swapped out.
    case FrameHostMsg_Detach::ID:
    case FrameHostMsg_DomOperationResponse::ID:
    // Input events propagate from parent to child.
    case FrameHostMsg_ForwardInputEvent::ID:
    case FrameHostMsg_InitializeChildFrame::ID:
    // The browser should always have an accurate mirror of the renderer's
    // notion of the current page id.
    case FrameHostMsg_DidAssignPageId::ID:
    // A swapped-out frame's opener might be updated with window.open.
    case FrameHostMsg_DidChangeOpener::ID:
    // For handling pop-ups from cross-site frames.
    case ViewHostMsg_CreateWidget::ID:
      return true;
    default:
      break;
  }

  // Check with the embedder as well.
  ContentClient* client = GetContentClient();
  return client->CanSendWhileSwappedOut(msg);
}

bool SwappedOutMessages::CanHandleWhileSwappedOut(
    const IPC::Message& msg) {
  // Any message the renderer is allowed to send while swapped out should
  // be handled by the browser.
  if (CanSendWhileSwappedOut(&msg))
    return true;

  // We drop most other messages that arrive from a swapped out renderer.
  // However, some are important (e.g., ACKs) for keeping the browser and
  // renderer state consistent in case we later return to the renderer.
  // Note that synchronous messages that are not handled will receive an
  // error reply instead, to avoid leaving the renderer in a stuck state.
  switch (msg.type()) {
    // Sends an ACK.
    case ViewHostMsg_ShowView::ID:
    // Sends an ACK.
    case ViewHostMsg_ShowWidget::ID:
    // Sends an ACK.
    case ViewHostMsg_ShowFullscreenWidget::ID:
    // Updates the previous navigation entry.
    case ViewHostMsg_UpdateState::ID:
    // Sends an ACK.
    case ViewHostMsg_UpdateTargetURL::ID:
    // We allow closing even if we are in the process of swapping out.
    case ViewHostMsg_Close::ID:
    // Sends an ACK.
    case ViewHostMsg_RequestMove::ID:
    // Sends an ACK.
    case AccessibilityHostMsg_Events::ID:
      return true;
    default:
      break;
  }

  return false;
}

}  // namespace content
