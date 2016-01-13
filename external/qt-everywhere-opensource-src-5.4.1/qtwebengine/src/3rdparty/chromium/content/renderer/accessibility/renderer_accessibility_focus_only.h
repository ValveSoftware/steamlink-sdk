// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ACCESSIBILITY_RENDERER_ACCESSIBILITY_FOCUS_ONLY_H_
#define CONTENT_RENDERER_ACCESSIBILITY_RENDERER_ACCESSIBILITY_FOCUS_ONLY_H_

#include "content/renderer/accessibility/renderer_accessibility.h"

namespace content {

// This is an accsessibility implementation that only handles whatever
// node has focus, ignoring everything else. It's here because on Windows 8,
// we need to use accessibility APIs to tell the operating system when a
// touch should pop up the on-screen keyboard, but it's not worth the
// performance overhead to enable full accessibility support.
//
// Here's how the on-screen keyboard works in Windows 8 "Metro-style" apps:
//
// 1. The user touches a control.
// 2. If the application determines focus moves to an editable text control,
//    it sends a native focus event, pointing to a native accessibility object
//    with information about the control that was just focused.
// 3. If the operating system sees that a focus event closely follows a
//    touch event, AND the bounding rectangle of the newly-focused control
//    contains the touch point, AND the focused object is a text control,
//    then Windows pops up the on-screen keyboard. In all other cases,
//    changing focus closes the on-screen keyboard.
//
// Alternatively:
// 1. The user touches a text control that already has focus.
// 2. The operating system uses accessibility APIs to query for the
//    currently focused object. If the touch falls within the bounds of
//    the focused object, the on-screen keyboard opens.
//
// In order to implement the accessibility APIs with minimal overhead, this
// class builds a "fake" accessibility tree consisting of only a single root
// node and optionally a single child node, representing the current focused
// element in the page (if any). Every time focus changes, this fake tree is
// sent from the renderer to the browser, along with a focus event - either
// on the child, or on the root of the tree if nothing is focused.
//
// Sometimes, touching an element other than a text box will result in a
// text box getting focus. We want the on-screen keyboard to pop up in those
// cases, so we "cheat" more and always send the dimensions of the whole
// window as the bounds of the child object. That way, a touch that leads
// to a text box getting focus will always open the on-screen keyboard,
// regardless of the relation between the touch location and the text box
// bounds.
class RendererAccessibilityFocusOnly : public RendererAccessibility {
 public:
  explicit RendererAccessibilityFocusOnly(RenderViewImpl* render_view);
  virtual ~RendererAccessibilityFocusOnly();

  // RendererAccessibility implementation.
  virtual void HandleWebAccessibilityEvent(
      const blink::WebAXObject& obj, blink::WebAXEvent event) OVERRIDE;
  virtual RendererAccessibilityType GetType() OVERRIDE;

  // RenderView::Observer implementation.
  virtual void FocusedNodeChanged(const blink::WebNode& node) OVERRIDE;
  virtual void DidFinishLoad(blink::WebLocalFrame* frame) OVERRIDE;

 private:
  void HandleFocusedNodeChanged(const blink::WebNode& node,
                                bool send_focus_event);

  int next_id_;

  DISALLOW_COPY_AND_ASSIGN(RendererAccessibilityFocusOnly);
};

}  // namespace content

#endif  // CONTENT_RENDERER_ACCESSIBILITY_RENDERER_ACCESSIBILITY_FOCUS_ONLY_H_
