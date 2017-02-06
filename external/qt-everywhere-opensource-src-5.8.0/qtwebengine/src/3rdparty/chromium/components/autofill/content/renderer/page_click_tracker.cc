// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/page_click_tracker.h"

#include "base/command_line.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/page_click_listener.h"
#include "components/autofill/core/common/autofill_util.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "third_party/WebKit/public/platform/WebPoint.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFormControlElement.h"
#include "third_party/WebKit/public/web/WebHitTestResult.h"
#include "third_party/WebKit/public/web/WebInputElement.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebUserGestureIndicator.h"
#include "third_party/WebKit/public/web/WebView.h"

using blink::WebElement;
using blink::WebFormControlElement;
using blink::WebGestureEvent;
using blink::WebInputElement;
using blink::WebInputEvent;
using blink::WebMouseEvent;
using blink::WebNode;
using blink::WebPoint;
using blink::WebSize;
using blink::WebUserGestureIndicator;

namespace autofill {

namespace {

// Casts |element| to a WebFormControlElement, but only if it's a text field.
// Returns an empty (isNull()) wrapper otherwise.
const WebFormControlElement GetTextFormControlElement(
    const WebElement& element) {
  if (!element.isFormControlElement())
    return WebFormControlElement();
  if (form_util::IsTextInput(blink::toWebInputElement(&element)) ||
      element.hasHTMLTagName("textarea"))
    return element.toConst<WebFormControlElement>();
  return WebFormControlElement();
}

}  // namespace

PageClickTracker::PageClickTracker(content::RenderFrame* render_frame,
                                   PageClickListener* listener)
    : content::RenderFrameObserver(render_frame),
      focused_node_was_last_clicked_(false),
      was_focused_before_now_(false),
      listener_(listener),
      legacy_(this) {
}

PageClickTracker::~PageClickTracker() {
}

void PageClickTracker::OnMouseDown(const WebNode& mouse_down_node) {
  focused_node_was_last_clicked_ = !mouse_down_node.isNull() &&
                                   mouse_down_node.focused();

  if (IsKeyboardAccessoryEnabled())
    DoFocusChangeComplete();
}

void PageClickTracker::FocusedNodeChanged(const WebNode& node) {
  was_focused_before_now_ = false;

  if (IsKeyboardAccessoryEnabled() &&
      WebUserGestureIndicator::isProcessingUserGesture()) {
    focused_node_was_last_clicked_ = true;
    DoFocusChangeComplete();
  }
}

void PageClickTracker::FocusChangeComplete() {
  if (IsKeyboardAccessoryEnabled())
    return;

  DoFocusChangeComplete();
}

void PageClickTracker::DoFocusChangeComplete() {
  WebElement focused_element =
      render_frame()->GetWebFrame()->document().focusedElement();
  if (focused_node_was_last_clicked_ && !focused_element.isNull()) {
    const WebFormControlElement control =
        GetTextFormControlElement(focused_element);
    if (!control.isNull()) {
      listener_->FormControlElementClicked(control,
                                           was_focused_before_now_);
    }
  }

  was_focused_before_now_ = true;
  focused_node_was_last_clicked_ = false;
}

void PageClickTracker::OnDestruct() {
  delete this;
}

// PageClickTracker::Legacy ----------------------------------------------------

PageClickTracker::Legacy::Legacy(PageClickTracker* tracker)
    : content::RenderViewObserver(tracker->render_frame()->GetRenderView()),
      tracker_(tracker) {
}

void PageClickTracker::Legacy::OnDestruct() {
  // No-op. Don't delete |this|.
}

void PageClickTracker::Legacy::OnMouseDown(const WebNode& mouse_down_node) {
  tracker_->OnMouseDown(mouse_down_node);
}

void PageClickTracker::Legacy::FocusChangeComplete() {
  tracker_->FocusChangeComplete();
}

}  // namespace autofill
