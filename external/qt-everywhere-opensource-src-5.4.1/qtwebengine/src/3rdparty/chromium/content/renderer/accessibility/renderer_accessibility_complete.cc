// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/renderer_accessibility_complete.h"

#include <queue>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "content/renderer/accessibility/blink_ax_enum_conversion.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/WebKit/public/web/WebAXObject.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebInputElement.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebNode.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "ui/accessibility/ax_tree.h"

using blink::WebAXObject;
using blink::WebDocument;
using blink::WebNode;
using blink::WebPoint;
using blink::WebRect;
using blink::WebView;

namespace content {

RendererAccessibilityComplete::RendererAccessibilityComplete(
    RenderViewImpl* render_view)
    : RendererAccessibility(render_view),
      weak_factory_(this),
      tree_source_(render_view),
      serializer_(&tree_source_),
      last_scroll_offset_(gfx::Size()),
      ack_pending_(false) {
  WebAXObject::enableAccessibility();

#if !defined(OS_ANDROID)
  // Skip inline text boxes on Android - since there are no native Android
  // APIs that compute the bounds of a range of text, it's a waste to
  // include these in the AX tree.
  WebAXObject::enableInlineTextBoxAccessibility();
#endif

  const WebDocument& document = GetMainDocument();
  if (!document.isNull()) {
    // It's possible that the webview has already loaded a webpage without
    // accessibility being enabled. Initialize the browser's cached
    // accessibility tree by sending it a notification.
    HandleAXEvent(document.accessibilityObject(),
                  ui::AX_EVENT_LAYOUT_COMPLETE);
  }
}

RendererAccessibilityComplete::~RendererAccessibilityComplete() {
}

bool RendererAccessibilityComplete::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RendererAccessibilityComplete, message)
    IPC_MESSAGE_HANDLER(AccessibilityMsg_SetFocus, OnSetFocus)
    IPC_MESSAGE_HANDLER(AccessibilityMsg_DoDefaultAction,
                        OnDoDefaultAction)
    IPC_MESSAGE_HANDLER(AccessibilityMsg_Events_ACK,
                        OnEventsAck)
    IPC_MESSAGE_HANDLER(AccessibilityMsg_ScrollToMakeVisible,
                        OnScrollToMakeVisible)
    IPC_MESSAGE_HANDLER(AccessibilityMsg_ScrollToPoint,
                        OnScrollToPoint)
    IPC_MESSAGE_HANDLER(AccessibilityMsg_SetTextSelection,
                        OnSetTextSelection)
    IPC_MESSAGE_HANDLER(AccessibilityMsg_HitTest, OnHitTest)
    IPC_MESSAGE_HANDLER(AccessibilityMsg_FatalError, OnFatalError)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void RendererAccessibilityComplete::FocusedNodeChanged(const WebNode& node) {
  const WebDocument& document = GetMainDocument();
  if (document.isNull())
    return;

  if (node.isNull()) {
    // When focus is cleared, implicitly focus the document.
    // TODO(dmazzoni): Make Blink send this notification instead.
    HandleAXEvent(document.accessibilityObject(), ui::AX_EVENT_BLUR);
  }
}

void RendererAccessibilityComplete::DidFinishLoad(blink::WebLocalFrame* frame) {
  const WebDocument& document = GetMainDocument();
  if (document.isNull())
    return;
}


void RendererAccessibilityComplete::HandleWebAccessibilityEvent(
    const blink::WebAXObject& obj, blink::WebAXEvent event) {
  HandleAXEvent(obj, AXEventFromBlink(event));
}

void RendererAccessibilityComplete::HandleAXEvent(
    const blink::WebAXObject& obj, ui::AXEvent event) {
  const WebDocument& document = GetMainDocument();
  if (document.isNull())
    return;

  gfx::Size scroll_offset = document.frame()->scrollOffset();
  if (scroll_offset != last_scroll_offset_) {
    // Make sure the browser is always aware of the scroll position of
    // the root document element by posting a generic notification that
    // will update it.
    // TODO(dmazzoni): remove this as soon as
    // https://bugs.webkit.org/show_bug.cgi?id=73460 is fixed.
    last_scroll_offset_ = scroll_offset;
    if (!obj.equals(document.accessibilityObject())) {
      HandleAXEvent(document.accessibilityObject(),
                    ui::AX_EVENT_LAYOUT_COMPLETE);
    }
  }

  // Add the accessibility object to our cache and ensure it's valid.
  AccessibilityHostMsg_EventParams acc_event;
  acc_event.id = obj.axID();
  acc_event.event_type = event;

  // Discard duplicate accessibility events.
  for (uint32 i = 0; i < pending_events_.size(); ++i) {
    if (pending_events_[i].id == acc_event.id &&
        pending_events_[i].event_type ==
            acc_event.event_type) {
      return;
    }
  }
  pending_events_.push_back(acc_event);

  if (!ack_pending_ && !weak_factory_.HasWeakPtrs()) {
    // When no accessibility events are in-flight post a task to send
    // the events to the browser. We use PostTask so that we can queue
    // up additional events.
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&RendererAccessibilityComplete::
                       SendPendingAccessibilityEvents,
                   weak_factory_.GetWeakPtr()));
  }
}

RendererAccessibilityType RendererAccessibilityComplete::GetType() {
  return RendererAccessibilityTypeComplete;
}

void RendererAccessibilityComplete::SendPendingAccessibilityEvents() {
  const WebDocument& document = GetMainDocument();
  if (document.isNull())
    return;

  if (pending_events_.empty())
    return;

  if (render_view_->is_swapped_out())
    return;

  ack_pending_ = true;

  // Make a copy of the events, because it's possible that
  // actions inside this loop will cause more events to be
  // queued up.
  std::vector<AccessibilityHostMsg_EventParams> src_events =
      pending_events_;
  pending_events_.clear();

  // Generate an event message from each Blink event.
  std::vector<AccessibilityHostMsg_EventParams> event_msgs;

  // If there's a layout complete message, we need to send location changes.
  bool had_layout_complete_messages = false;

  // Loop over each event and generate an updated event message.
  for (size_t i = 0; i < src_events.size(); ++i) {
    AccessibilityHostMsg_EventParams& event = src_events[i];
    if (event.event_type == ui::AX_EVENT_LAYOUT_COMPLETE)
      had_layout_complete_messages = true;

    WebAXObject obj = document.accessibilityObjectFromID(event.id);

    // Make sure the object still exists.
    if (!obj.updateBackingStoreAndCheckValidity())
      continue;

    // Make sure it's a descendant of our root node - exceptions include the
    // scroll area that's the parent of the main document (we ignore it), and
    // possibly nodes attached to a different document.
    if (!tree_source_.IsInTree(obj))
      continue;

    // When we get a "selected children changed" event, Blink
    // doesn't also send us events for each child that changed
    // selection state, so make sure we re-send that whole subtree.
    if (event.event_type ==
        ui::AX_EVENT_SELECTED_CHILDREN_CHANGED) {
      serializer_.DeleteClientSubtree(obj);
    }

    AccessibilityHostMsg_EventParams event_msg;
    event_msg.event_type = event.event_type;
    event_msg.id = event.id;
    serializer_.SerializeChanges(obj, &event_msg.update);
    event_msgs.push_back(event_msg);

    // For each node in the update, set the location in our map from
    // ids to locations.
    for (size_t i = 0; i < event_msg.update.nodes.size(); ++i) {
      locations_[event_msg.update.nodes[i].id] =
          event_msg.update.nodes[i].location;
    }

    VLOG(0) << "Accessibility event: " << ui::ToString(event.event_type)
            << " on node id " << event_msg.id
            << "\n" << event_msg.update.ToString();
  }

  Send(new AccessibilityHostMsg_Events(routing_id(), event_msgs));

  if (had_layout_complete_messages)
    SendLocationChanges();
}

void RendererAccessibilityComplete::SendLocationChanges() {
  std::vector<AccessibilityHostMsg_LocationChangeParams> messages;

  // Do a breadth-first explore of the whole blink AX tree.
  base::hash_map<int, gfx::Rect> new_locations;
  std::queue<WebAXObject> objs_to_explore;
  objs_to_explore.push(tree_source_.GetRoot());
  while (objs_to_explore.size()) {
    WebAXObject obj = objs_to_explore.front();
    objs_to_explore.pop();

    // See if we had a previous location. If not, this whole subtree must
    // be new, so don't continue to explore this branch.
    int id = obj.axID();
    base::hash_map<int, gfx::Rect>::iterator iter = locations_.find(id);
    if (iter == locations_.end())
      continue;

    // If the location has changed, append it to the IPC message.
    gfx::Rect new_location = obj.boundingBoxRect();
    if (iter != locations_.end() && iter->second != new_location) {
      AccessibilityHostMsg_LocationChangeParams message;
      message.id = id;
      message.new_location = new_location;
      messages.push_back(message);
    }

    // Save the new location.
    new_locations[id] = new_location;

    // Explore children of this object.
    std::vector<blink::WebAXObject> children;
    tree_source_.GetChildren(obj, &children);
    for (size_t i = 0; i < children.size(); ++i)
      objs_to_explore.push(children[i]);
  }
  locations_.swap(new_locations);

  Send(new AccessibilityHostMsg_LocationChanges(routing_id(), messages));
}

void RendererAccessibilityComplete::OnDoDefaultAction(int acc_obj_id) {
  const WebDocument& document = GetMainDocument();
  if (document.isNull())
    return;

  WebAXObject obj = document.accessibilityObjectFromID(acc_obj_id);
  if (obj.isDetached()) {
#ifndef NDEBUG
    LOG(WARNING) << "DoDefaultAction on invalid object id " << acc_obj_id;
#endif
    return;
  }

  obj.performDefaultAction();
}

void RendererAccessibilityComplete::OnScrollToMakeVisible(
    int acc_obj_id, gfx::Rect subfocus) {
  const WebDocument& document = GetMainDocument();
  if (document.isNull())
    return;

  WebAXObject obj = document.accessibilityObjectFromID(acc_obj_id);
  if (obj.isDetached()) {
#ifndef NDEBUG
    LOG(WARNING) << "ScrollToMakeVisible on invalid object id " << acc_obj_id;
#endif
    return;
  }

  obj.scrollToMakeVisibleWithSubFocus(
      WebRect(subfocus.x(), subfocus.y(),
              subfocus.width(), subfocus.height()));

  // Make sure the browser gets an event when the scroll
  // position actually changes.
  // TODO(dmazzoni): remove this once this bug is fixed:
  // https://bugs.webkit.org/show_bug.cgi?id=73460
  HandleAXEvent(document.accessibilityObject(),
                ui::AX_EVENT_LAYOUT_COMPLETE);
}

void RendererAccessibilityComplete::OnScrollToPoint(
    int acc_obj_id, gfx::Point point) {
  const WebDocument& document = GetMainDocument();
  if (document.isNull())
    return;

  WebAXObject obj = document.accessibilityObjectFromID(acc_obj_id);
  if (obj.isDetached()) {
#ifndef NDEBUG
    LOG(WARNING) << "ScrollToPoint on invalid object id " << acc_obj_id;
#endif
    return;
  }

  obj.scrollToGlobalPoint(WebPoint(point.x(), point.y()));

  // Make sure the browser gets an event when the scroll
  // position actually changes.
  // TODO(dmazzoni): remove this once this bug is fixed:
  // https://bugs.webkit.org/show_bug.cgi?id=73460
  HandleAXEvent(document.accessibilityObject(),
                ui::AX_EVENT_LAYOUT_COMPLETE);
}

void RendererAccessibilityComplete::OnSetTextSelection(
    int acc_obj_id, int start_offset, int end_offset) {
  const WebDocument& document = GetMainDocument();
  if (document.isNull())
    return;

  WebAXObject obj = document.accessibilityObjectFromID(acc_obj_id);
  if (obj.isDetached()) {
#ifndef NDEBUG
    LOG(WARNING) << "SetTextSelection on invalid object id " << acc_obj_id;
#endif
    return;
  }

  // TODO(dmazzoni): support elements other than <input>.
  blink::WebNode node = obj.node();
  if (!node.isNull() && node.isElementNode()) {
    blink::WebElement element = node.to<blink::WebElement>();
    blink::WebInputElement* input_element =
        blink::toWebInputElement(&element);
    if (input_element && input_element->isTextField())
      input_element->setSelectionRange(start_offset, end_offset);
  }
}

void RendererAccessibilityComplete::OnHitTest(gfx::Point point) {
  const WebDocument& document = GetMainDocument();
  if (document.isNull())
    return;
  WebAXObject root_obj = document.accessibilityObject();
  if (!root_obj.updateBackingStoreAndCheckValidity())
    return;

  WebAXObject obj = root_obj.hitTest(point);
  if (!obj.isDetached())
    HandleAXEvent(obj, ui::AX_EVENT_HOVER);
}

void RendererAccessibilityComplete::OnEventsAck() {
  DCHECK(ack_pending_);
  ack_pending_ = false;
  SendPendingAccessibilityEvents();
}

void RendererAccessibilityComplete::OnSetFocus(int acc_obj_id) {
  const WebDocument& document = GetMainDocument();
  if (document.isNull())
    return;

  WebAXObject obj = document.accessibilityObjectFromID(acc_obj_id);
  if (obj.isDetached()) {
#ifndef NDEBUG
    LOG(WARNING) << "OnSetAccessibilityFocus on invalid object id "
                 << acc_obj_id;
#endif
    return;
  }

  WebAXObject root = document.accessibilityObject();
  if (root.isDetached()) {
#ifndef NDEBUG
    LOG(WARNING) << "OnSetAccessibilityFocus but root is invalid";
#endif
    return;
  }

  // By convention, calling SetFocus on the root of the tree should clear the
  // current focus. Otherwise set the focus to the new node.
  if (acc_obj_id == root.axID())
    render_view()->GetWebView()->clearFocusedElement();
  else
    obj.setFocused(true);
}

void RendererAccessibilityComplete::OnFatalError() {
  CHECK(false) << "Invalid accessibility tree.";
}

}  // namespace content
