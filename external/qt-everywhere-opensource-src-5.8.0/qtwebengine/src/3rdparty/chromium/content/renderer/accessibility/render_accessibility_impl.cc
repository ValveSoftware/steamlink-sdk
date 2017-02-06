// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/render_accessibility_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <queue>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/common/accessibility_messages.h"
#include "content/renderer/accessibility/blink_ax_enum_conversion.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/WebKit/public/web/WebAXObject.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebInputElement.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebSettings.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "ui/accessibility/ax_node.h"

using blink::WebAXObject;
using blink::WebDocument;
using blink::WebElement;
using blink::WebLocalFrame;
using blink::WebNode;
using blink::WebPoint;
using blink::WebRect;
using blink::WebScopedAXContext;
using blink::WebSettings;
using blink::WebView;

namespace content {

// Cap the number of nodes returned in an accessibility
// tree snapshot to avoid outrageous memory or bandwidth
// usage.
const size_t kMaxSnapshotNodeCount = 5000;

// static
void RenderAccessibilityImpl::SnapshotAccessibilityTree(
    RenderFrameImpl* render_frame,
    AXContentTreeUpdate* response) {
  DCHECK(render_frame);
  DCHECK(response);
  if (!render_frame->GetWebFrame())
    return;

  WebDocument document = render_frame->GetWebFrame()->document();
  WebScopedAXContext context(document);
  BlinkAXTreeSource tree_source(render_frame);
  tree_source.SetRoot(context.root());
  BlinkAXTreeSerializer serializer(&tree_source);
  serializer.set_max_node_count(kMaxSnapshotNodeCount);
  serializer.SerializeChanges(context.root(), response);
}

RenderAccessibilityImpl::RenderAccessibilityImpl(RenderFrameImpl* render_frame)
    : RenderFrameObserver(render_frame),
      render_frame_(render_frame),
      tree_source_(render_frame),
      serializer_(&tree_source_),
      pdf_tree_source_(nullptr),
      last_scroll_offset_(gfx::Size()),
      ack_pending_(false),
      reset_token_(0),
      weak_factory_(this) {
  // There's only one AXObjectCache for the root of a local frame tree,
  // so if this frame's parent is local we can safely do nothing.
  if (render_frame_ &&
      render_frame_->GetWebFrame() &&
      render_frame_->GetWebFrame()->parent() &&
      render_frame_->GetWebFrame()->parent()->isWebLocalFrame()) {
    return;
  }

  WebView* web_view = render_frame_->GetRenderView()->GetWebView();
  WebSettings* settings = web_view->settings();
  settings->setAccessibilityEnabled(true);

#if defined(OS_ANDROID)
  // Password values are only passed through on Android.
  settings->setAccessibilityPasswordValuesEnabled(true);
#endif

#if !defined(OS_ANDROID)
  // Inline text boxes are enabled for all nodes on all except Android.
  settings->setInlineTextBoxAccessibilityEnabled(true);
#endif

  const WebDocument& document = GetMainDocument();
  if (!document.isNull()) {
    // It's possible that the webview has already loaded a webpage without
    // accessibility being enabled. Initialize the browser's cached
    // accessibility tree by sending it a notification.
    HandleAXEvent(document.accessibilityObject(), ui::AX_EVENT_LAYOUT_COMPLETE);
  }
}

RenderAccessibilityImpl::~RenderAccessibilityImpl() {
}

bool RenderAccessibilityImpl::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderAccessibilityImpl, message)
    IPC_MESSAGE_HANDLER(AccessibilityMsg_SetFocus, OnSetFocus)
    IPC_MESSAGE_HANDLER(AccessibilityMsg_DoDefaultAction, OnDoDefaultAction)
    IPC_MESSAGE_HANDLER(AccessibilityMsg_Events_ACK, OnEventsAck)
    IPC_MESSAGE_HANDLER(AccessibilityMsg_ScrollToMakeVisible,
                        OnScrollToMakeVisible)
    IPC_MESSAGE_HANDLER(AccessibilityMsg_ScrollToPoint, OnScrollToPoint)
    IPC_MESSAGE_HANDLER(AccessibilityMsg_SetScrollOffset, OnSetScrollOffset)
    IPC_MESSAGE_HANDLER(AccessibilityMsg_SetSelection, OnSetSelection)
    IPC_MESSAGE_HANDLER(AccessibilityMsg_SetValue, OnSetValue)
    IPC_MESSAGE_HANDLER(AccessibilityMsg_ShowContextMenu, OnShowContextMenu)
    IPC_MESSAGE_HANDLER(AccessibilityMsg_HitTest, OnHitTest)
    IPC_MESSAGE_HANDLER(AccessibilityMsg_SetAccessibilityFocus,
                        OnSetAccessibilityFocus)
    IPC_MESSAGE_HANDLER(AccessibilityMsg_Reset, OnReset)
    IPC_MESSAGE_HANDLER(AccessibilityMsg_FatalError, OnFatalError)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void RenderAccessibilityImpl::HandleWebAccessibilityEvent(
    const blink::WebAXObject& obj, blink::WebAXEvent event) {
  HandleAXEvent(obj, AXEventFromBlink(event));
}

void RenderAccessibilityImpl::HandleAccessibilityFindInPageResult(
    int identifier,
    int match_index,
    const blink::WebAXObject& start_object,
    int start_offset,
    const blink::WebAXObject& end_object,
    int end_offset) {
  AccessibilityHostMsg_FindInPageResultParams params;
  params.request_id = identifier;
  params.match_index = match_index;
  params.start_id = start_object.axID();
  params.start_offset = start_offset;
  params.end_id = end_object.axID();
  params.end_offset = end_offset;
  Send(new AccessibilityHostMsg_FindInPageResult(routing_id(), params));
}

void RenderAccessibilityImpl::AccessibilityFocusedNodeChanged(
    const WebNode& node) {
  const WebDocument& document = GetMainDocument();
  if (document.isNull())
    return;

  if (node.isNull()) {
    // When focus is cleared, implicitly focus the document.
    // TODO(dmazzoni): Make Blink send this notification instead.
    HandleAXEvent(document.accessibilityObject(), ui::AX_EVENT_BLUR);
  }
}

void RenderAccessibilityImpl::DisableAccessibility() {
  RenderView* render_view = render_frame_->GetRenderView();
  if (!render_view)
    return;

  WebView* web_view = render_view->GetWebView();
  if (!web_view)
    return;

  WebSettings* settings = web_view->settings();
  if (!settings)
    return;

  settings->setAccessibilityEnabled(false);
}

void RenderAccessibilityImpl::HandleAXEvent(
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

#if defined(OS_ANDROID)
  // Force the newly focused node to be re-serialized so we include its
  // inline text boxes.
  if (event == ui::AX_EVENT_FOCUS)
    serializer_.DeleteClientSubtree(obj);
#endif

  // Add the accessibility object to our cache and ensure it's valid.
  AccessibilityHostMsg_EventParams acc_event;
  acc_event.id = obj.axID();
  acc_event.event_type = event;

  // Discard duplicate accessibility events.
  for (uint32_t i = 0; i < pending_events_.size(); ++i) {
    if (pending_events_[i].id == acc_event.id &&
        pending_events_[i].event_type == acc_event.event_type) {
      return;
    }
  }
  pending_events_.push_back(acc_event);

  if (!ack_pending_ && !weak_factory_.HasWeakPtrs()) {
    // When no accessibility events are in-flight post a task to send
    // the events to the browser. We use PostTask so that we can queue
    // up additional events.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&RenderAccessibilityImpl::SendPendingAccessibilityEvents,
                   weak_factory_.GetWeakPtr()));
  }
}

int RenderAccessibilityImpl::GenerateAXID() {
  WebAXObject root = tree_source_.GetRoot();
  return root.generateAXID();
}

void RenderAccessibilityImpl::SetPdfTreeSource(
    RenderAccessibilityImpl::PdfAXTreeSource* pdf_tree_source) {
  pdf_tree_source_ = pdf_tree_source;
  pdf_serializer_.reset(new PdfAXTreeSerializer(pdf_tree_source_));

  WebAXObject root = tree_source_.GetRoot();
  if (!root.updateLayoutAndCheckValidity())
    return;

  std::queue<WebAXObject> objs_to_explore;
  objs_to_explore.push(root);
  while (objs_to_explore.size()) {
    WebAXObject obj = objs_to_explore.front();
    objs_to_explore.pop();

    WebNode node = obj.node();
    if (!node.isNull() && node.isElementNode()) {
      WebElement element = node.to<WebElement>();
      if (element.hasHTMLTagName("embed")) {
        HandleAXEvent(obj, ui::AX_EVENT_CHILDREN_CHANGED);
        break;
      }
    }

    // Explore children of this object.
    std::vector<blink::WebAXObject> children;
    tree_source_.GetChildren(obj, &children);
    for (size_t i = 0; i < children.size(); ++i)
      objs_to_explore.push(children[i]);
  }
}

WebDocument RenderAccessibilityImpl::GetMainDocument() {
  if (render_frame_ && render_frame_->GetWebFrame())
    return render_frame_->GetWebFrame()->document();
  return WebDocument();
}

void RenderAccessibilityImpl::SendPendingAccessibilityEvents() {
  const WebDocument& document = GetMainDocument();
  if (document.isNull())
    return;

  if (pending_events_.empty())
    return;

  ack_pending_ = true;

  // Make a copy of the events, because it's possible that
  // actions inside this loop will cause more events to be
  // queued up.
  std::vector<AccessibilityHostMsg_EventParams> src_events = pending_events_;
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
    if (!obj.updateLayoutAndCheckValidity())
      continue;

    // If it's ignored, find the first ancestor that's not ignored.
    while (!obj.isDetached() && obj.accessibilityIsIgnored())
      obj = obj.parentObject();

    // Make sure it's a descendant of our root node - exceptions include the
    // scroll area that's the parent of the main document (we ignore it), and
    // possibly nodes attached to a different document.
    if (!tree_source_.IsInTree(obj))
      continue;

    AccessibilityHostMsg_EventParams event_msg;
    event_msg.event_type = event.event_type;
    event_msg.id = event.id;
    if (!serializer_.SerializeChanges(obj, &event_msg.update)) {
      LOG(ERROR) << "Failed to serialize one accessibility event.";
      continue;
    }

    if (pdf_tree_source_)
      AddPdfTreeToUpdate(&event_msg.update);

    event_msgs.push_back(event_msg);

    // For each node in the update, set the location in our map from
    // ids to locations.
    for (size_t i = 0; i < event_msg.update.nodes.size(); ++i) {
      locations_[event_msg.update.nodes[i].id] =
          event_msg.update.nodes[i].location;
    }

    DVLOG(1) << "Accessibility event: " << ui::ToString(event.event_type)
             << " on node id " << event_msg.id
             << "\n" << event_msg.update.ToString();
  }

  Send(new AccessibilityHostMsg_Events(routing_id(), event_msgs, reset_token_));
  reset_token_ = 0;

  if (had_layout_complete_messages)
    SendLocationChanges();
}

void RenderAccessibilityImpl::SendLocationChanges() {
  std::vector<AccessibilityHostMsg_LocationChangeParams> messages;

  // Update layout on the root of the tree.
  WebAXObject root = tree_source_.GetRoot();
  if (!root.updateLayoutAndCheckValidity())
    return;

  // Do a breadth-first explore of the whole blink AX tree.
  base::hash_map<int, gfx::Rect> new_locations;
  std::queue<WebAXObject> objs_to_explore;
  objs_to_explore.push(root);
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

void RenderAccessibilityImpl::OnDoDefaultAction(int acc_obj_id) {
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

void RenderAccessibilityImpl::OnEventsAck() {
  DCHECK(ack_pending_);
  ack_pending_ = false;
  SendPendingAccessibilityEvents();
}

void RenderAccessibilityImpl::OnFatalError() {
  CHECK(false) << "Invalid accessibility tree.";
}

void RenderAccessibilityImpl::OnHitTest(gfx::Point point) {
  const WebDocument& document = GetMainDocument();
  if (document.isNull())
    return;
  WebAXObject root_obj = document.accessibilityObject();
  if (!root_obj.updateLayoutAndCheckValidity())
    return;

  WebAXObject obj = root_obj.hitTest(point);
  if (obj.isDetached())
    return;

  // If the object that was hit has a child frame, we have to send a
  // message back to the browser to do the hit test in the child frame,
  // recursively.
  AXContentNodeData data;
  tree_source_.SerializeNode(obj, &data);
  if (data.HasContentIntAttribute(AX_CONTENT_ATTR_CHILD_ROUTING_ID) ||
      data.HasContentIntAttribute(
          AX_CONTENT_ATTR_CHILD_BROWSER_PLUGIN_INSTANCE_ID)) {
    Send(new AccessibilityHostMsg_ChildFrameHitTestResult(routing_id(), point,
                                                          obj.axID()));
    return;
  }

  // Otherwise, send a HOVER event on the node that was hit.
  HandleAXEvent(obj, ui::AX_EVENT_HOVER);
}

void RenderAccessibilityImpl::OnSetAccessibilityFocus(int acc_obj_id) {
  if (tree_source_.accessibility_focus_id() == acc_obj_id)
    return;

  tree_source_.set_accessibility_focus_id(acc_obj_id);

  const WebDocument& document = GetMainDocument();
  if (document.isNull())
    return;

  WebAXObject obj = document.accessibilityObjectFromID(acc_obj_id);

  // This object may not be a leaf node. Force the whole subtree to be
  // re-serialized.
  serializer_.DeleteClientSubtree(obj);

  // Explicitly send a tree change update event now.
  HandleAXEvent(obj, ui::AX_EVENT_TREE_CHANGED);
}

void RenderAccessibilityImpl::OnReset(int reset_token) {
  reset_token_ = reset_token;
  serializer_.Reset();
  pending_events_.clear();

  const WebDocument& document = GetMainDocument();
  if (!document.isNull()) {
    // Tree-only mode gets used by the automation extension API which requires a
    // load complete event to invoke listener callbacks.
    ui::AXEvent evt = document.accessibilityObject().isLoaded()
        ? ui::AX_EVENT_LOAD_COMPLETE : ui::AX_EVENT_LAYOUT_COMPLETE;
    HandleAXEvent(document.accessibilityObject(), evt);
  }
}

void RenderAccessibilityImpl::OnScrollToMakeVisible(
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
      WebRect(subfocus.x(), subfocus.y(), subfocus.width(), subfocus.height()));

  // Make sure the browser gets an event when the scroll
  // position actually changes.
  // TODO(dmazzoni): remove this once this bug is fixed:
  // https://bugs.webkit.org/show_bug.cgi?id=73460
  HandleAXEvent(document.accessibilityObject(), ui::AX_EVENT_LAYOUT_COMPLETE);
}

void RenderAccessibilityImpl::OnScrollToPoint(
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
  HandleAXEvent(document.accessibilityObject(), ui::AX_EVENT_LAYOUT_COMPLETE);
}

void RenderAccessibilityImpl::OnSetScrollOffset(int acc_obj_id,
                                              gfx::Point offset) {
  const WebDocument& document = GetMainDocument();
  if (document.isNull())
    return;

  WebAXObject obj = document.accessibilityObjectFromID(acc_obj_id);
  if (obj.isDetached())
    return;

  obj.setScrollOffset(WebPoint(offset.x(), offset.y()));
}

void RenderAccessibilityImpl::OnSetFocus(int acc_obj_id) {
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
    render_frame_->GetRenderView()->GetWebView()->clearFocusedElement();
  else
    obj.setFocused(true);
}

void RenderAccessibilityImpl::OnSetSelection(int anchor_acc_obj_id,
                                           int anchor_offset,
                                           int focus_acc_obj_id,
                                           int focus_offset) {
  const WebDocument& document = GetMainDocument();
  if (document.isNull())
    return;

  WebAXObject anchor_obj =
      document.accessibilityObjectFromID(anchor_acc_obj_id);
  if (anchor_obj.isDetached()) {
#ifndef NDEBUG
    LOG(WARNING) << "SetTextSelection on invalid object id "
                 << anchor_acc_obj_id;
#endif
    return;
  }

  WebAXObject focus_obj = document.accessibilityObjectFromID(focus_acc_obj_id);
  if (focus_obj.isDetached()) {
#ifndef NDEBUG
    LOG(WARNING) << "SetTextSelection on invalid object id "
                 << focus_acc_obj_id;
#endif
    return;
  }

  anchor_obj.setSelection(anchor_obj, anchor_offset, focus_obj, focus_offset);
  WebAXObject root = document.accessibilityObject();
  if (root.isDetached()) {
#ifndef NDEBUG
    LOG(WARNING) << "OnSetAccessibilityFocus but root is invalid";
#endif
    return;
  }
  HandleAXEvent(root, ui::AX_EVENT_LAYOUT_COMPLETE);
}

void RenderAccessibilityImpl::OnSetValue(
    int acc_obj_id,
    base::string16 value) {
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

  obj.setValue(value);
  HandleAXEvent(obj, ui::AX_EVENT_VALUE_CHANGED);
}

void RenderAccessibilityImpl::OnShowContextMenu(int acc_obj_id) {
  const WebDocument& document = GetMainDocument();
  if (document.isNull())
    return;

  WebAXObject obj = document.accessibilityObjectFromID(acc_obj_id);
  if (obj.isDetached()) {
#ifndef NDEBUG
    LOG(WARNING) << "ShowContextMenu on invalid object id " << acc_obj_id;
#endif
    return;
  }

  obj.showContextMenu();
}

void RenderAccessibilityImpl::OnDestruct() {
  delete this;
}

void RenderAccessibilityImpl::AddPdfTreeToUpdate(AXContentTreeUpdate* update) {
  for (size_t i = 0; i < update->nodes.size(); ++i) {
    if (update->nodes[i].role == ui::AX_ROLE_EMBEDDED_OBJECT) {
      const ui::AXNode* root = pdf_tree_source_->GetRoot();
      update->nodes[i].child_ids.push_back(root->id());

      ui::AXTreeUpdate pdf_update;
      pdf_serializer_->SerializeChanges(root, &pdf_update);

      // We have to copy the updated nodes using a loop because we're
      // converting from a generic ui::AXNodeData to a vector of its
      // content-specific subclass AXContentNodeData.
      size_t old_count = update->nodes.size();
      size_t new_count = pdf_update.nodes.size();
      update->nodes.resize(old_count + new_count);
      for (size_t i = 0; i < new_count; ++i)
        update->nodes[old_count + i] = pdf_update.nodes[i];
      break;
    }
  }
}

}  // namespace content
