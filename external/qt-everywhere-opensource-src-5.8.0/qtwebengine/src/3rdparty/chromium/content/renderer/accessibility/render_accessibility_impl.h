// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ACCESSIBILITY_RENDER_ACCESSIBILITY_IMPL_H_
#define CONTENT_RENDERER_ACCESSIBILITY_RENDER_ACCESSIBILITY_IMPL_H_

#include <vector>

#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/common/ax_content_node_data.h"
#include "content/public/renderer/render_accessibility.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/renderer/accessibility/blink_ax_tree_source.h"
#include "third_party/WebKit/public/web/WebAXObject.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_serializer.h"

struct AccessibilityHostMsg_EventParams;

namespace blink {
class WebDocument;
class WebNode;
};

namespace content {
class RenderFrameImpl;

// The browser process implements native accessibility APIs, allowing
// assistive technology (e.g., screen readers, magnifiers) to access and
// control the web contents with high-level APIs. These APIs are also used
// by automation tools, and Windows 8 uses them to determine when the
// on-screen keyboard should be shown.
//
// An instance of this class belongs to RenderFrameImpl. Accessibility is
// initialized based on the AccessibilityMode of RenderFrameImpl; it lazily
// starts as Off or EditableTextOnly depending on the operating system, and
// switches to Complete if assistive technology is detected or a flag is set.
//
// A tree of accessible objects is built here and sent to the browser process;
// the browser process maintains this as a tree of platform-native
// accessible objects that can be used to respond to accessibility requests
// from other processes.
//
// This class implements complete accessibility support for assistive
// technology. It turns on Blink's accessibility code and sends a serialized
// representation of that tree whenever it changes. It also handles requests
// from the browser to perform accessibility actions on nodes in the tree
// (e.g., change focus, or click on a button).
class CONTENT_EXPORT RenderAccessibilityImpl
    : public RenderAccessibility,
             RenderFrameObserver {
 public:
  // Request a one-time snapshot of the accessibility tree without
  // enabling accessibility if it wasn't already enabled.
  static void SnapshotAccessibilityTree(
      RenderFrameImpl* render_frame,
      AXContentTreeUpdate* response);

  explicit RenderAccessibilityImpl(RenderFrameImpl* render_frame);
  ~RenderAccessibilityImpl() override;

  // RenderAccessibility implementation.
  int GenerateAXID() override;
  void SetPdfTreeSource(PdfAXTreeSource* source) override;

  // RenderFrameObserver implementation.
  bool OnMessageReceived(const IPC::Message& message) override;

  // Called when an accessibility notification occurs in Blink.
  void HandleWebAccessibilityEvent(const blink::WebAXObject& obj,
                                   blink::WebAXEvent event);

  // Called when a new find in page result is highlighted.
  void HandleAccessibilityFindInPageResult(
      int identifier,
      int match_index,
      const blink::WebAXObject& start_object,
      int start_offset,
      const blink::WebAXObject& end_object,
      int end_offset);

  void AccessibilityFocusedNodeChanged(const blink::WebNode& node);

  // This can be called before deleting a RenderAccessibilityImpl instance due
  // to the accessibility mode changing, as opposed to during frame destruction
  // (when there'd be no point).
  void DisableAccessibility();

  void HandleAXEvent(const blink::WebAXObject& obj, ui::AXEvent event);

 protected:
  // Returns the main top-level document for this page, or NULL if there's
  // no view or frame.
  blink::WebDocument GetMainDocument();

  // Send queued events from the renderer to the browser.
  void SendPendingAccessibilityEvents();

  // Check the entire accessibility tree to see if any nodes have
  // changed location, by comparing their locations to the cached
  // versions. If any have moved, send an IPC with the new locations.
  void SendLocationChanges();

  // The RenderFrameImpl that owns us.
  RenderFrameImpl* render_frame_;

 private:
  // RenderFrameObserver implementation.
  void OnDestruct() override;

  // Handlers for messages from the browser to the renderer.
  void OnDoDefaultAction(int acc_obj_id);
  void OnEventsAck();
  void OnFatalError();
  void OnHitTest(gfx::Point point);
  void OnSetAccessibilityFocus(int acc_obj_id);
  void OnReset(int reset_token);
  void OnScrollToMakeVisible(int acc_obj_id, gfx::Rect subfocus);
  void OnScrollToPoint(int acc_obj_id, gfx::Point point);
  void OnSetScrollOffset(int acc_obj_id, gfx::Point offset);
  void OnSetFocus(int acc_obj_id);
  void OnSetSelection(int anchor_acc_obj_id,
                      int anchor_offset,
                      int focus_acc_obj_id,
                      int focus_offset);
  void OnSetValue(int acc_obj_id, base::string16 value);
  void OnShowContextMenu(int acc_obj_id);

  void AddPdfTreeToUpdate(AXContentTreeUpdate* update);

  // Events from Blink are collected until they are ready to be
  // sent to the browser.
  std::vector<AccessibilityHostMsg_EventParams> pending_events_;

  // The adapter that exposes Blink's accessibility tree to AXTreeSerializer.
  BlinkAXTreeSource tree_source_;

  // The serializer that sends accessibility messages to the browser process.
  using BlinkAXTreeSerializer =
      ui::AXTreeSerializer<blink::WebAXObject,
                           AXContentNodeData,
                           AXContentTreeData>;
  BlinkAXTreeSerializer serializer_;

  using PdfAXTreeSerializer = ui::AXTreeSerializer<const ui::AXNode*,
                                                   ui::AXNodeData,
                                                   ui::AXTreeData>;
  std::unique_ptr<PdfAXTreeSerializer> pdf_serializer_;
  PdfAXTreeSource* pdf_tree_source_;

  // Current location of every object, so we can detect when it moves.
  base::hash_map<int, gfx::Rect> locations_;

  // The most recently observed scroll offset of the root document element.
  // TODO(dmazzoni): remove once https://bugs.webkit.org/show_bug.cgi?id=73460
  // is fixed.
  gfx::Size last_scroll_offset_;

  // Set if we are waiting for an accessibility event ack.
  bool ack_pending_;

  // Nonzero if the browser requested we reset the accessibility state.
  // We need to return this token in the next IPC.
  int reset_token_;

  // So we can queue up tasks to be executed later.
  base::WeakPtrFactory<RenderAccessibilityImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RenderAccessibilityImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_ACCESSIBILITY_RENDERER_ACCESSIBILITY_H_
