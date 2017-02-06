// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "KeyboardEventManager.h"

#include "core/dom/Element.h"
#include "core/editing/Editor.h"
#include "core/events/KeyboardEvent.h"
#include "core/html/HTMLDialogElement.h"
#include "core/input/EventHandler.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutTextControlSingleLine.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/page/ChromeClient.h"
#include "core/page/FocusController.h"
#include "core/page/Page.h"
#include "core/page/SpatialNavigation.h"
#include "platform/PlatformKeyboardEvent.h"
#include "platform/UserGestureIndicator.h"
#include "platform/WindowsKeyboardCodes.h"

namespace blink {

namespace {

const int kVKeyProcessKey = 229;

WebFocusType focusDirectionForKey(const String& key)
{
    WebFocusType retVal = WebFocusTypeNone;
    if (key == "ArrowDown")
        retVal = WebFocusTypeDown;
    else if (key == "ArrowUp")
        retVal = WebFocusTypeUp;
    else if (key == "ArrowLeft")
        retVal = WebFocusTypeLeft;
    else if (key == "ArrowRight")
        retVal = WebFocusTypeRight;
    return retVal;
}

} // namespace

KeyboardEventManager::KeyboardEventManager(
    LocalFrame* frame, ScrollManager* scrollManager)
: m_frame(frame)
, m_scrollManager(scrollManager)
{
}

KeyboardEventManager::~KeyboardEventManager()
{
}

bool KeyboardEventManager::handleAccessKey(const PlatformKeyboardEvent& evt)
{
    // FIXME: Ignoring the state of Shift key is what neither IE nor Firefox do.
    // IE matches lower and upper case access keys regardless of Shift key state - but if both upper and
    // lower case variants are present in a document, the correct element is matched based on Shift key state.
    // Firefox only matches an access key if Shift is not pressed, and does that case-insensitively.
    DCHECK(!(PlatformKeyboardEvent::accessKeyModifiers() & PlatformEvent::ShiftKey));
    if ((evt.getModifiers() & (PlatformEvent::KeyModifiers & ~PlatformEvent::ShiftKey)) != PlatformKeyboardEvent::accessKeyModifiers())
        return false;
    String key = evt.unmodifiedText();
    Element* elem = m_frame->document()->getElementByAccessKey(key.lower());
    if (!elem)
        return false;
    elem->accessKeyAction(false);
    return true;
}

WebInputEventResult KeyboardEventManager::keyEvent(
    const PlatformKeyboardEvent& initialKeyEvent)
{
    m_frame->chromeClient().clearToolTip();

    if (initialKeyEvent.windowsVirtualKeyCode() == VK_CAPITAL)
        capsLockStateMayHaveChanged();

#if OS(WIN)
    if (m_scrollManager->panScrollInProgress()) {
        // If a key is pressed while the panScroll is in progress then we want to stop
        if (initialKeyEvent.type() == PlatformEvent::KeyDown || initialKeyEvent.type() == PlatformEvent::RawKeyDown)
            m_scrollManager->stopAutoscroll();

        // If we were in panscroll mode, we swallow the key event
        return WebInputEventResult::HandledSuppressed;
    }
#endif

    // Check for cases where we are too early for events -- possible unmatched key up
    // from pressing return in the location bar.
    Node* node = eventTargetNodeForDocument(m_frame->document());
    if (!node)
        return WebInputEventResult::NotHandled;

    UserGestureIndicator gestureIndicator(DefinitelyProcessingUserGesture);

    // In IE, access keys are special, they are handled after default keydown processing, but cannot be canceled - this is hard to match.
    // On Mac OS X, we process them before dispatching keydown, as the default keydown handler implements Emacs key bindings, which may conflict
    // with access keys. Then we dispatch keydown, but suppress its default handling.
    // On Windows, WebKit explicitly calls handleAccessKey() instead of dispatching a keypress event for WM_SYSCHAR messages.
    // Other platforms currently match either Mac or Windows behavior, depending on whether they send combined KeyDown events.
    bool matchedAnAccessKey = false;
    if (initialKeyEvent.type() == PlatformEvent::KeyDown)
        matchedAnAccessKey = handleAccessKey(initialKeyEvent);

    // FIXME: it would be fair to let an input method handle KeyUp events before DOM dispatch.
    if (initialKeyEvent.type() == PlatformEvent::KeyUp || initialKeyEvent.type() == PlatformEvent::Char) {
        KeyboardEvent* domEvent = KeyboardEvent::create(initialKeyEvent, m_frame->document()->domWindow());

        return EventHandler::toWebInputEventResult(node->dispatchEvent(domEvent));
    }

    PlatformKeyboardEvent keyDownEvent = initialKeyEvent;
    if (keyDownEvent.type() != PlatformEvent::RawKeyDown)
        keyDownEvent.disambiguateKeyDownEvent(PlatformEvent::RawKeyDown);
    KeyboardEvent* keydown = KeyboardEvent::create(keyDownEvent, m_frame->document()->domWindow());
    if (matchedAnAccessKey)
        keydown->setDefaultPrevented(true);
    keydown->setTarget(node);

    DispatchEventResult dispatchResult = node->dispatchEvent(keydown);
    if (dispatchResult != DispatchEventResult::NotCanceled)
        return EventHandler::toWebInputEventResult(dispatchResult);
    // If frame changed as a result of keydown dispatch, then return early to avoid sending a subsequent keypress message to the new frame.
    bool changedFocusedFrame = m_frame->page() && m_frame != m_frame->page()->focusController().focusedOrMainFrame();
    if (changedFocusedFrame)
        return WebInputEventResult::HandledSystem;

    if (initialKeyEvent.type() == PlatformEvent::RawKeyDown)
        return WebInputEventResult::NotHandled;

    // Focus may have changed during keydown handling, so refetch node.
    // But if we are dispatching a fake backward compatibility keypress, then we pretend that the keypress happened on the original node.
    node = eventTargetNodeForDocument(m_frame->document());
    if (!node)
        return WebInputEventResult::NotHandled;

    PlatformKeyboardEvent keyPressEvent = initialKeyEvent;
    keyPressEvent.disambiguateKeyDownEvent(PlatformEvent::Char);
    if (keyPressEvent.text().isEmpty())
        return WebInputEventResult::NotHandled;
    KeyboardEvent* keypress = KeyboardEvent::create(keyPressEvent, m_frame->document()->domWindow());
    keypress->setTarget(node);
    return EventHandler::toWebInputEventResult(node->dispatchEvent(keypress));
}

void KeyboardEventManager::capsLockStateMayHaveChanged()
{
    if (Element* element = m_frame->document()->focusedElement()) {
        if (LayoutObject* r = element->layoutObject()) {
            if (r->isTextField())
                toLayoutTextControlSingleLine(r)->capsLockStateMayHaveChanged();
        }
    }
}

void KeyboardEventManager::defaultKeyboardEventHandler(
    KeyboardEvent* event, Node* possibleFocusedNode)
{
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
            WebFocusType type = focusDirectionForKey(event->key());
            if (type != WebFocusTypeNone)
                defaultArrowEventHandler(type, event);
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

void KeyboardEventManager::defaultSpaceEventHandler(
    KeyboardEvent* event, Node* possibleFocusedNode)
{
    DCHECK_EQ(event->type(), EventTypeNames::keypress);

    if (event->ctrlKey() || event->metaKey() || event->altKey())
        return;

    ScrollDirection direction = event->shiftKey() ? ScrollBlockDirectionBackward : ScrollBlockDirectionForward;

    // FIXME: enable scroll customization in this case. See crbug.com/410974.
    if (m_scrollManager->logicalScroll(direction, ScrollByPage, nullptr, possibleFocusedNode)) {
        event->setDefaultHandled();
        return;
    }
}

void KeyboardEventManager::defaultBackspaceEventHandler(KeyboardEvent* event)
{
    DCHECK_EQ(event->type(), EventTypeNames::keydown);

    if (!RuntimeEnabledFeatures::backspaceDefaultHandlerEnabled())
        return;

    if (event->ctrlKey() || event->metaKey() || event->altKey())
        return;

    if (!m_frame->editor().behavior().shouldNavigateBackOnBackspace())
        return;
    UseCounter::count(m_frame->document(), UseCounter::BackspaceNavigatedBack);
    if (m_frame->page()->chromeClient().hadFormInteraction())
        UseCounter::count(m_frame->document(), UseCounter::BackspaceNavigatedBackAfterFormInteraction);
    bool handledEvent = m_frame->loader().client()->navigateBackForward(event->shiftKey() ? 1 : -1);
    if (handledEvent)
        event->setDefaultHandled();
}

void KeyboardEventManager::defaultArrowEventHandler(WebFocusType focusType, KeyboardEvent* event)
{
    DCHECK_EQ(event->type(), EventTypeNames::keydown);

    if (event->ctrlKey() || event->metaKey() || event->shiftKey())
        return;

    Page* page = m_frame->page();
    if (!page)
        return;

    if (!isSpatialNavigationEnabled(m_frame))
        return;

    // Arrows and other possible directional navigation keys can be used in design
    // mode editing.
    if (m_frame->document()->inDesignMode())
        return;

    if (page->focusController().advanceFocus(focusType))
        event->setDefaultHandled();
}

void KeyboardEventManager::defaultTabEventHandler(KeyboardEvent* event)
{
    DCHECK_EQ(event->type(), EventTypeNames::keydown);

    // We should only advance focus on tabs if no special modifier keys are held down.
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

    WebFocusType focusType = event->shiftKey() ? WebFocusTypeBackward : WebFocusTypeForward;

    // Tabs can be used in design mode editing.
    if (m_frame->document()->inDesignMode())
        return;

    if (page->focusController().advanceFocus(focusType, InputDeviceCapabilities::doesntFireTouchEventsSourceCapabilities()))
        event->setDefaultHandled();
}

void KeyboardEventManager::defaultEscapeEventHandler(KeyboardEvent* event)
{
    if (HTMLDialogElement* dialog = m_frame->document()->activeModalDialog())
        dialog->dispatchEvent(Event::createCancelable(EventTypeNames::cancel));
}

DEFINE_TRACE(KeyboardEventManager)
{
    visitor->trace(m_frame);
}

} // namespace blink
