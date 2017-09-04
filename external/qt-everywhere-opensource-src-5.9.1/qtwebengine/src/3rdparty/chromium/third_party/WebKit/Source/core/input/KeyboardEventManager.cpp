// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "KeyboardEventManager.h"

#include "core/dom/DocumentUserGestureToken.h"
#include "core/dom/Element.h"
#include "core/editing/Editor.h"
#include "core/events/KeyboardEvent.h"
#include "core/html/HTMLDialogElement.h"
#include "core/input/EventHandler.h"
#include "core/input/EventHandlingUtil.h"
#include "core/input/ScrollManager.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutTextControlSingleLine.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/page/ChromeClient.h"
#include "core/page/FocusController.h"
#include "core/page/Page.h"
#include "core/page/SpatialNavigation.h"
#include "platform/KeyboardCodes.h"
#include "platform/UserGestureIndicator.h"
#include "platform/WindowsKeyboardCodes.h"
#include "public/platform/WebInputEvent.h"

#if OS(WIN)
#include <windows.h>
#elif OS(MACOSX)
#import <Carbon/Carbon.h>
#endif

namespace blink {

namespace {

#if OS(WIN)
static const unsigned short HIGHBITMASKSHORT = 0x8000;
#endif

const int kVKeyProcessKey = 229;

WebFocusType focusDirectionForKey(KeyboardEvent* event) {
  if (event->ctrlKey() || event->metaKey() || event->shiftKey())
    return WebFocusTypeNone;

  WebFocusType retVal = WebFocusTypeNone;
  if (event->key() == "ArrowDown")
    retVal = WebFocusTypeDown;
  else if (event->key() == "ArrowUp")
    retVal = WebFocusTypeUp;
  else if (event->key() == "ArrowLeft")
    retVal = WebFocusTypeLeft;
  else if (event->key() == "ArrowRight")
    retVal = WebFocusTypeRight;
  return retVal;
}

bool mapKeyCodeForScroll(int keyCode,
                         PlatformEvent::Modifiers modifiers,
                         ScrollDirection* scrollDirection,
                         ScrollGranularity* scrollGranularity) {
  if (modifiers & PlatformEvent::ShiftKey || modifiers & PlatformEvent::MetaKey)
    return false;

  if (modifiers & PlatformEvent::AltKey) {
    // Alt-Up/Down should behave like PageUp/Down on Mac.  (Note that Alt-keys
    // on other platforms are suppressed due to isSystemKey being set.)
    if (keyCode == VKEY_UP)
      keyCode = VKEY_PRIOR;
    else if (keyCode == VKEY_DOWN)
      keyCode = VKEY_NEXT;
    else
      return false;
  }

  if (modifiers & PlatformEvent::CtrlKey) {
    // Match FF behavior in the sense that Ctrl+home/end are the only Ctrl
    // key combinations which affect scrolling.
    if (keyCode != VKEY_HOME && keyCode != VKEY_END)
      return false;
  }

  switch (keyCode) {
    case VKEY_LEFT:
      *scrollDirection = ScrollLeftIgnoringWritingMode;
      *scrollGranularity = ScrollByLine;
      break;
    case VKEY_RIGHT:
      *scrollDirection = ScrollRightIgnoringWritingMode;
      *scrollGranularity = ScrollByLine;
      break;
    case VKEY_UP:
      *scrollDirection = ScrollUpIgnoringWritingMode;
      *scrollGranularity = ScrollByLine;
      break;
    case VKEY_DOWN:
      *scrollDirection = ScrollDownIgnoringWritingMode;
      *scrollGranularity = ScrollByLine;
      break;
    case VKEY_HOME:
      *scrollDirection = ScrollUpIgnoringWritingMode;
      *scrollGranularity = ScrollByDocument;
      break;
    case VKEY_END:
      *scrollDirection = ScrollDownIgnoringWritingMode;
      *scrollGranularity = ScrollByDocument;
      break;
    case VKEY_PRIOR:  // page up
      *scrollDirection = ScrollUpIgnoringWritingMode;
      *scrollGranularity = ScrollByPage;
      break;
    case VKEY_NEXT:  // page down
      *scrollDirection = ScrollDownIgnoringWritingMode;
      *scrollGranularity = ScrollByPage;
      break;
    default:
      return false;
  }

  return true;
}

}  // namespace

KeyboardEventManager::KeyboardEventManager(LocalFrame* frame,
                                           ScrollManager* scrollManager)
    : m_frame(frame), m_scrollManager(scrollManager) {}

DEFINE_TRACE(KeyboardEventManager) {
  visitor->trace(m_frame);
  visitor->trace(m_scrollManager);
}

bool KeyboardEventManager::handleAccessKey(const WebKeyboardEvent& evt) {
  // FIXME: Ignoring the state of Shift key is what neither IE nor Firefox do.
  // IE matches lower and upper case access keys regardless of Shift key state -
  // but if both upper and lower case variants are present in a document, the
  // correct element is matched based on Shift key state.  Firefox only matches
  // an access key if Shift is not pressed, and does that case-insensitively.
  DCHECK(!(kAccessKeyModifiers & WebInputEvent::ShiftKey));
  if ((evt.modifiers & (WebKeyboardEvent::KeyModifiers &
                        ~WebInputEvent::ShiftKey)) != kAccessKeyModifiers)
    return false;
  String key = String(evt.unmodifiedText);
  Element* elem = m_frame->document()->getElementByAccessKey(key.lower());
  if (!elem)
    return false;
  elem->accessKeyAction(false);
  return true;
}

WebInputEventResult KeyboardEventManager::keyEvent(
    const WebKeyboardEvent& initialKeyEvent) {
  m_frame->chromeClient().clearToolTip(*m_frame);

  if (initialKeyEvent.windowsKeyCode == VK_CAPITAL)
    capsLockStateMayHaveChanged();

  if (m_scrollManager->middleClickAutoscrollInProgress()) {
    DCHECK(RuntimeEnabledFeatures::middleClickAutoscrollEnabled());
    // If a key is pressed while the middleClickAutoscroll is in progress then
    // we want to stop.
    if (initialKeyEvent.type == WebInputEvent::KeyDown ||
        initialKeyEvent.type == WebInputEvent::RawKeyDown)
      m_scrollManager->stopAutoscroll();

    // If we were in panscroll mode, we swallow the key event
    return WebInputEventResult::HandledSuppressed;
  }

  // Check for cases where we are too early for events -- possible unmatched key
  // up from pressing return in the location bar.
  Node* node = eventTargetNodeForDocument(m_frame->document());
  if (!node)
    return WebInputEventResult::NotHandled;

  UserGestureIndicator gestureIndicator(
      DocumentUserGestureToken::create(m_frame->document()));

  // In IE, access keys are special, they are handled after default keydown
  // processing, but cannot be canceled - this is hard to match.  On Mac OS X,
  // we process them before dispatching keydown, as the default keydown handler
  // implements Emacs key bindings, which may conflict with access keys. Then we
  // dispatch keydown, but suppress its default handling.
  // On Windows, WebKit explicitly calls handleAccessKey() instead of
  // dispatching a keypress event for WM_SYSCHAR messages.  Other platforms
  // currently match either Mac or Windows behavior, depending on whether they
  // send combined KeyDown events.
  bool matchedAnAccessKey = false;
  if (initialKeyEvent.type == WebInputEvent::KeyDown)
    matchedAnAccessKey = handleAccessKey(initialKeyEvent);

  // FIXME: it would be fair to let an input method handle KeyUp events before
  // DOM dispatch.
  if (initialKeyEvent.type == WebInputEvent::KeyUp ||
      initialKeyEvent.type == WebInputEvent::Char) {
    KeyboardEvent* domEvent = KeyboardEvent::create(
        initialKeyEvent, m_frame->document()->domWindow());

    return EventHandlingUtil::toWebInputEventResult(
        node->dispatchEvent(domEvent));
  }

  WebKeyboardEvent keyDownEvent = initialKeyEvent;
  if (keyDownEvent.type != WebInputEvent::RawKeyDown)
    keyDownEvent.type = WebInputEvent::RawKeyDown;
  KeyboardEvent* keydown =
      KeyboardEvent::create(keyDownEvent, m_frame->document()->domWindow());
  if (matchedAnAccessKey)
    keydown->setDefaultPrevented(true);
  keydown->setTarget(node);

  DispatchEventResult dispatchResult = node->dispatchEvent(keydown);
  if (dispatchResult != DispatchEventResult::NotCanceled)
    return EventHandlingUtil::toWebInputEventResult(dispatchResult);
  // If frame changed as a result of keydown dispatch, then return early to
  // avoid sending a subsequent keypress message to the new frame.
  bool changedFocusedFrame =
      m_frame->page() &&
      m_frame != m_frame->page()->focusController().focusedOrMainFrame();
  if (changedFocusedFrame)
    return WebInputEventResult::HandledSystem;

  if (initialKeyEvent.type == WebInputEvent::RawKeyDown)
    return WebInputEventResult::NotHandled;

  // Focus may have changed during keydown handling, so refetch node.
  // But if we are dispatching a fake backward compatibility keypress, then we
  // pretend that the keypress happened on the original node.
  node = eventTargetNodeForDocument(m_frame->document());
  if (!node)
    return WebInputEventResult::NotHandled;

#if OS(MACOSX)
  // According to NSEvents.h, OpenStep reserves the range 0xF700-0xF8FF for
  // function keys. However, some actual private use characters happen to be
  // in this range, e.g. the Apple logo (Option+Shift+K). 0xF7FF is an
  // arbitrary cut-off.
  if (initialKeyEvent.text[0U] >= 0xF700 &&
      initialKeyEvent.text[0U] <= 0xF7FF) {
    return WebInputEventResult::NotHandled;
  }
#endif

  WebKeyboardEvent keyPressEvent = initialKeyEvent;
  keyPressEvent.type = WebInputEvent::Char;
  if (keyPressEvent.text[0] == 0)
    return WebInputEventResult::NotHandled;
  KeyboardEvent* keypress =
      KeyboardEvent::create(keyPressEvent, m_frame->document()->domWindow());
  keypress->setTarget(node);
  return EventHandlingUtil::toWebInputEventResult(
      node->dispatchEvent(keypress));
}

void KeyboardEventManager::capsLockStateMayHaveChanged() {
  if (Element* element = m_frame->document()->focusedElement()) {
    if (LayoutObject* r = element->layoutObject()) {
      if (r->isTextField())
        toLayoutTextControlSingleLine(r)->capsLockStateMayHaveChanged();
    }
  }
}

void KeyboardEventManager::defaultKeyboardEventHandler(
    KeyboardEvent* event,
    Node* possibleFocusedNode) {
  if (event->type() == EventTypeNames::keydown) {
    m_frame->editor().handleKeyboardEvent(event);
    if (event->defaultHandled())
      return;

    // Do not perform the default action when inside a IME composition context.
    // TODO(dtapuska): Replace this with isComposing support. crbug.com/625686
    if (event->keyCode() == kVKeyProcessKey)
      return;
    if (event->key() == "Tab") {
      defaultTabEventHandler(event);
    } else if (event->key() == "Backspace") {
      defaultBackspaceEventHandler(event);
    } else if (event->key() == "Escape") {
      defaultEscapeEventHandler(event);
    } else {
      defaultArrowEventHandler(event, possibleFocusedNode);
    }
  }
  if (event->type() == EventTypeNames::keypress) {
    m_frame->editor().handleKeyboardEvent(event);
    if (event->defaultHandled())
      return;
    if (event->charCode() == ' ')
      defaultSpaceEventHandler(event, possibleFocusedNode);
  }
}

void KeyboardEventManager::defaultSpaceEventHandler(KeyboardEvent* event,
                                                    Node* possibleFocusedNode) {
  DCHECK_EQ(event->type(), EventTypeNames::keypress);

  if (event->ctrlKey() || event->metaKey() || event->altKey())
    return;

  ScrollDirection direction = event->shiftKey() ? ScrollBlockDirectionBackward
                                                : ScrollBlockDirectionForward;

  // FIXME: enable scroll customization in this case. See crbug.com/410974.
  if (m_scrollManager->logicalScroll(direction, ScrollByPage, nullptr,
                                     possibleFocusedNode)) {
    event->setDefaultHandled();
    return;
  }
}

void KeyboardEventManager::defaultBackspaceEventHandler(KeyboardEvent* event) {
  DCHECK_EQ(event->type(), EventTypeNames::keydown);

  if (!RuntimeEnabledFeatures::backspaceDefaultHandlerEnabled())
    return;

  if (event->ctrlKey() || event->metaKey() || event->altKey())
    return;

  if (!m_frame->editor().behavior().shouldNavigateBackOnBackspace())
    return;
  UseCounter::count(m_frame->document(), UseCounter::BackspaceNavigatedBack);
  if (m_frame->page()->chromeClient().hadFormInteraction())
    UseCounter::count(m_frame->document(),
                      UseCounter::BackspaceNavigatedBackAfterFormInteraction);
  bool handledEvent = m_frame->loader().client()->navigateBackForward(
      event->shiftKey() ? 1 : -1);
  if (handledEvent)
    event->setDefaultHandled();
}

void KeyboardEventManager::defaultArrowEventHandler(KeyboardEvent* event,
                                                    Node* possibleFocusedNode) {
  DCHECK_EQ(event->type(), EventTypeNames::keydown);

  Page* page = m_frame->page();
  if (!page)
    return;

  WebFocusType type = focusDirectionForKey(event);
  if (type != WebFocusTypeNone && isSpatialNavigationEnabled(m_frame) &&
      !m_frame->document()->inDesignMode()) {
    if (page->focusController().advanceFocus(type)) {
      event->setDefaultHandled();
      return;
    }
  }

  if (event->keyEvent() && event->keyEvent()->isSystemKey)
    return;

  ScrollDirection scrollDirection;
  ScrollGranularity scrollGranularity;
  if (!mapKeyCodeForScroll(event->keyCode(), event->modifiers(),
                           &scrollDirection, &scrollGranularity))
    return;

  if (m_scrollManager->bubblingScroll(scrollDirection, scrollGranularity,
                                      nullptr, possibleFocusedNode)) {
    event->setDefaultHandled();
    return;
  }
}

void KeyboardEventManager::defaultTabEventHandler(KeyboardEvent* event) {
  DCHECK_EQ(event->type(), EventTypeNames::keydown);

  // We should only advance focus on tabs if no special modifier keys are held
  // down.
  if (event->ctrlKey() || event->metaKey())
    return;

#if !OS(MACOSX)
  // Option-Tab is a shortcut based on a system-wide preference on Mac but
  // should be ignored on all other platforms.
  if (event->altKey())
    return;
#endif

  Page* page = m_frame->page();
  if (!page)
    return;
  if (!page->tabKeyCyclesThroughElements())
    return;

  WebFocusType focusType =
      event->shiftKey() ? WebFocusTypeBackward : WebFocusTypeForward;

  // Tabs can be used in design mode editing.
  if (m_frame->document()->inDesignMode())
    return;

  if (page->focusController().advanceFocus(
          focusType,
          InputDeviceCapabilities::doesntFireTouchEventsSourceCapabilities()))
    event->setDefaultHandled();
}

void KeyboardEventManager::defaultEscapeEventHandler(KeyboardEvent* event) {
  if (HTMLDialogElement* dialog = m_frame->document()->activeModalDialog())
    dialog->dispatchEvent(Event::createCancelable(EventTypeNames::cancel));
}

static OverrideCapsLockState s_overrideCapsLockState;

void KeyboardEventManager::setCurrentCapsLockState(
    OverrideCapsLockState state) {
  s_overrideCapsLockState = state;
}

bool KeyboardEventManager::currentCapsLockState() {
  switch (s_overrideCapsLockState) {
    case OverrideCapsLockState::Default:
#if OS(WIN)
      // FIXME: Does this even work inside the sandbox?
      return GetKeyState(VK_CAPITAL) & 1;
#elif OS(MACOSX)
      return GetCurrentKeyModifiers() & alphaLock;
#else
      // Caps lock state use is limited to Mac password input
      // fields, so just return false. See http://crbug.com/618739.
      return false;
#endif
    case OverrideCapsLockState::On:
      return true;
    case OverrideCapsLockState::Off:
    default:
      return false;
  }
}

WebInputEvent::Modifiers KeyboardEventManager::getCurrentModifierState() {
  unsigned modifiers = 0;
#if OS(WIN)
  if (GetKeyState(VK_SHIFT) & HIGHBITMASKSHORT)
    modifiers |= WebInputEvent::ShiftKey;
  if (GetKeyState(VK_CONTROL) & HIGHBITMASKSHORT)
    modifiers |= WebInputEvent::ControlKey;
  if (GetKeyState(VK_MENU) & HIGHBITMASKSHORT)
    modifiers |= WebInputEvent::AltKey;
#elif OS(MACOSX)
  UInt32 currentModifiers = GetCurrentKeyModifiers();
  if (currentModifiers & ::shiftKey)
    modifiers |= WebInputEvent::ShiftKey;
  if (currentModifiers & ::controlKey)
    modifiers |= WebInputEvent::ControlKey;
  if (currentModifiers & ::optionKey)
    modifiers |= WebInputEvent::AltKey;
  if (currentModifiers & ::cmdKey)
    modifiers |= WebInputEvent::MetaKey;
#else
  // TODO(crbug.com/538289): Implement on other platforms.
  return static_cast<WebInputEvent::Modifiers>(0);
#endif
  return static_cast<WebInputEvent::Modifiers>(modifiers);
}

}  // namespace blink
