// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/WebInputMethodControllerImpl.h"

#include "core/InputTypeNames.h"
#include "core/dom/DocumentUserGestureToken.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/Editor.h"
#include "core/editing/EphemeralRange.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/InputMethodController.h"
#include "core/editing/PlainTextRange.h"
#include "core/frame/LocalFrame.h"
#include "core/page/FocusController.h"
#include "core/page/Page.h"
#include "platform/UserGestureIndicator.h"
#include "public/platform/WebString.h"
#include "public/web/WebPlugin.h"
#include "public/web/WebRange.h"
#include "web/CompositionUnderlineVectorBuilder.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebPluginContainerImpl.h"

namespace blink {

WebInputMethodControllerImpl::WebInputMethodControllerImpl(
    WebLocalFrameImpl* webLocalFrame)
    : m_webLocalFrame(webLocalFrame), m_suppressNextKeypressEvent(false) {}

WebInputMethodControllerImpl::~WebInputMethodControllerImpl() {}

// static
WebInputMethodControllerImpl* WebInputMethodControllerImpl::fromFrame(
    LocalFrame* frame) {
  WebLocalFrameImpl* webLocalFrameImpl = WebLocalFrameImpl::fromFrame(frame);
  return webLocalFrameImpl ? webLocalFrameImpl->inputMethodController()
                           : nullptr;
}

DEFINE_TRACE(WebInputMethodControllerImpl) {
  visitor->trace(m_webLocalFrame);
}

bool WebInputMethodControllerImpl::setComposition(
    const WebString& text,
    const WebVector<WebCompositionUnderline>& underlines,
    int selectionStart,
    int selectionEnd) {
  if (WebPlugin* plugin = focusedPluginIfInputMethodSupported()) {
    return plugin->setComposition(text, underlines, selectionStart,
                                  selectionEnd);
  }

  // We should use this |editor| object only to complete the ongoing
  // composition.
  if (!frame()->editor().canEdit() && !inputMethodController().hasComposition())
    return false;

  // We should verify the parent node of this IME composition node are
  // editable because JavaScript may delete a parent node of the composition
  // node. In this case, WebKit crashes while deleting texts from the parent
  // node, which doesn't exist any longer.
  const EphemeralRange range =
      inputMethodController().compositionEphemeralRange();
  if (range.isNotNull()) {
    Node* node = range.startPosition().computeContainerNode();
    frame()->document()->updateStyleAndLayoutTree();
    if (!node || !hasEditableStyle(*node))
      return false;
  }

  // A keypress event is canceled. If an ongoing composition exists, then the
  // keydown event should have arisen from a handled key (e.g., backspace).
  // In this case we ignore the cancellation and continue; otherwise (no
  // ongoing composition) we exit and signal success only for attempts to
  // clear the composition.
  if (m_suppressNextKeypressEvent && !inputMethodController().hasComposition())
    return text.isEmpty();

  UserGestureIndicator gestureIndicator(DocumentUserGestureToken::create(
      frame()->document(), UserGestureToken::NewGesture));

  // When the range of composition underlines overlap with the range between
  // selectionStart and selectionEnd, WebKit somehow won't paint the selection
  // at all (see InlineTextBox::paint() function in InlineTextBox.cpp).
  // But the selection range actually takes effect.
  inputMethodController().setComposition(
      String(text), CompositionUnderlineVectorBuilder(underlines),
      selectionStart, selectionEnd);

  return text.isEmpty() || inputMethodController().hasComposition();
}

bool WebInputMethodControllerImpl::finishComposingText(
    ConfirmCompositionBehavior selectionBehavior) {
  // TODO(ekaramad): Here and in other IME calls we should expect the
  // call to be made when our frame is focused. This, however, is not the case
  // all the time. For instance, resetInputMethod call on RenderViewImpl could
  // be after losing the focus on frame. But since we return the core frame
  // in WebViewImpl::focusedLocalFrameInWidget(), we will reach here with
  // |m_webLocalFrame| not focused on page.

  if (WebPlugin* plugin = focusedPluginIfInputMethodSupported())
    return plugin->finishComposingText(selectionBehavior);

  // TODO(editing-dev): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  frame()->document()->updateStyleAndLayoutIgnorePendingStylesheets();

  return inputMethodController().finishComposingText(
      selectionBehavior == WebInputMethodController::KeepSelection
          ? InputMethodController::KeepSelection
          : InputMethodController::DoNotKeepSelection);
}

bool WebInputMethodControllerImpl::commitText(const WebString& text,
                                              int relativeCaretPosition) {
  UserGestureIndicator gestureIndicator(DocumentUserGestureToken::create(
      frame()->document(), UserGestureToken::NewGesture));

  if (WebPlugin* plugin = focusedPluginIfInputMethodSupported())
    return plugin->commitText(text, relativeCaretPosition);

  // TODO(xiaochengh): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  frame()->document()->updateStyleAndLayoutIgnorePendingStylesheets();

  return inputMethodController().commitText(text, relativeCaretPosition);
}

LocalFrame* WebInputMethodControllerImpl::frame() const {
  return m_webLocalFrame->frame();
}

InputMethodController& WebInputMethodControllerImpl::inputMethodController()
    const {
  return frame()->inputMethodController();
}

WebPlugin* WebInputMethodControllerImpl::focusedPluginIfInputMethodSupported()
    const {
  WebPluginContainerImpl* container =
      WebLocalFrameImpl::currentPluginContainer(frame());
  if (container && container->supportsInputMethod())
    return container->plugin();
  return nullptr;
}

}  // namespace blink
