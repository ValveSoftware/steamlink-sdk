// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/renderer_accessibility_focus_only.h"

#include "content/renderer/render_view_impl.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebNode.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "ui/accessibility/ax_node_data.h"

using blink::WebDocument;
using blink::WebElement;
using blink::WebNode;
using blink::WebView;

namespace {
// The root node will always have id 1. Let each child node have a new
// id starting with 2.
const int kInitialId = 2;
}

namespace content {

RendererAccessibilityFocusOnly::RendererAccessibilityFocusOnly(
    RenderViewImpl* render_view)
    : RendererAccessibility(render_view),
      next_id_(kInitialId) {
}

RendererAccessibilityFocusOnly::~RendererAccessibilityFocusOnly() {
}

void RendererAccessibilityFocusOnly::HandleWebAccessibilityEvent(
    const blink::WebAXObject& obj, blink::WebAXEvent event) {
  // Do nothing.
}

RendererAccessibilityType RendererAccessibilityFocusOnly::GetType() {
  return RendererAccessibilityTypeFocusOnly;
}

void RendererAccessibilityFocusOnly::FocusedNodeChanged(const WebNode& node) {
  // Send the new accessible tree and post a native focus event.
  HandleFocusedNodeChanged(node, true);
}

void RendererAccessibilityFocusOnly::DidFinishLoad(
    blink::WebLocalFrame* frame) {
  WebView* view = render_view()->GetWebView();
  if (view->focusedFrame() != frame)
    return;

  WebDocument document = frame->document();
  // Send an accessible tree to the browser, but do not post a native
  // focus event. This is important so that if focus is initially in an
  // editable text field, Windows will know to pop up the keyboard if the
  // user touches it and focus doesn't change.
  HandleFocusedNodeChanged(document.focusedElement(), false);
}

void RendererAccessibilityFocusOnly::HandleFocusedNodeChanged(
    const WebNode& node,
    bool send_focus_event) {
  const WebDocument& document = GetMainDocument();
  if (document.isNull())
    return;

  bool node_has_focus;
  bool node_is_editable_text;
  // Check HasIMETextFocus first, because it will correctly handle
  // focus in a text box inside a ppapi plug-in. Otherwise fall back on
  // checking the focused node in Blink.
  if (render_view_->HasIMETextFocus()) {
    node_has_focus = true;
    node_is_editable_text = true;
  } else {
    node_has_focus = !node.isNull();
    node_is_editable_text =
        node_has_focus && render_view_->IsEditableNode(node);
  }

  std::vector<AccessibilityHostMsg_EventParams> events;
  events.push_back(AccessibilityHostMsg_EventParams());
  AccessibilityHostMsg_EventParams& event = events[0];

  // If we want to update the browser's accessibility tree but not send a
  // native focus changed event, we can send a LayoutComplete
  // event, which doesn't post a native event on Windows.
  event.event_type =
      send_focus_event ? ui::AX_EVENT_FOCUS : ui::AX_EVENT_LAYOUT_COMPLETE;

  // Set the id that the event applies to: the root node if nothing
  // has focus, otherwise the focused node.
  event.id = node_has_focus ? next_id_ : 1;

  event.update.nodes.resize(2);
  ui::AXNodeData& root = event.update.nodes[0];
  ui::AXNodeData& child = event.update.nodes[1];

  // Always include the root of the tree, the document. It always has id 1.
  root.id = 1;
  root.role = ui::AX_ROLE_ROOT_WEB_AREA;
  root.state =
      (1 << ui::AX_STATE_READ_ONLY) |
      (1 << ui::AX_STATE_FOCUSABLE);
  if (!node_has_focus)
    root.state |= (1 << ui::AX_STATE_FOCUSED);
  root.location = gfx::Rect(render_view_->size());
  root.child_ids.push_back(next_id_);

  child.id = next_id_;
  child.role = ui::AX_ROLE_GROUP;

  if (!node.isNull() && node.isElementNode()) {
    child.location = gfx::Rect(
        const_cast<WebNode&>(node).to<WebElement>().boundsInViewportSpace());
  } else if (render_view_->HasIMETextFocus()) {
    child.location = root.location;
  } else {
    child.location = gfx::Rect();
  }

  if (node_has_focus) {
    child.state =
        (1 << ui::AX_STATE_FOCUSABLE) |
        (1 << ui::AX_STATE_FOCUSED);
    if (!node_is_editable_text)
      child.state |= (1 << ui::AX_STATE_READ_ONLY);
  }

#ifndef NDEBUG
  /**
  if (logging_) {
    VLOG(0) << "Accessibility update: \n"
        << "routing id=" << routing_id()
        << " event="
        << AccessibilityEventToString(event.event_type)
        << "\n" << event.nodes[0].DebugString(true);
  }
  **/
#endif

  Send(new AccessibilityHostMsg_Events(routing_id(), events));

  // Increment the id, wrap back when we get past a million.
  next_id_++;
  if (next_id_ > 1000000)
    next_id_ = kInitialId;
}

}  // namespace content
