/*
 * Copyright (C) 2006, 2007, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/frame/LocalDOMWindow.h"

#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/SourceLocation.h"
#include "core/css/CSSComputedStyleDeclaration.h"
#include "core/css/CSSRuleList.h"
#include "core/css/DOMWindowCSS.h"
#include "core/css/MediaQueryList.h"
#include "core/css/MediaQueryMatcher.h"
#include "core/css/StyleMedia.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/DOMImplementation.h"
#include "core/dom/ExecutionContextTask.h"
#include "core/dom/FrameRequestCallback.h"
#include "core/dom/SandboxFlags.h"
#include "core/dom/custom/CustomElementsRegistry.h"
#include "core/editing/Editor.h"
#include "core/events/DOMWindowEventQueue.h"
#include "core/events/HashChangeEvent.h"
#include "core/events/MessageEvent.h"
#include "core/events/PageTransitionEvent.h"
#include "core/events/PopStateEvent.h"
#include "core/events/ScopedEventQueue.h"
#include "core/frame/BarProp.h"
#include "core/frame/DOMVisualViewport.h"
#include "core/frame/EventHandlerRegistry.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/FrameView.h"
#include "core/frame/History.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Navigator.h"
#include "core/frame/Screen.h"
#include "core/frame/ScrollToOptions.h"
#include "core/frame/Settings.h"
#include "core/frame/SuspendableTimer.h"
#include "core/frame/VisualViewport.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/input/EventHandler.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/loader/SinkDocument.h"
#include "core/loader/appcache/ApplicationCache.h"
#include "core/page/ChromeClient.h"
#include "core/page/CreateWindow.h"
#include "core/page/Page.h"
#include "core/page/WindowFeatures.h"
#include "core/page/scrolling/ScrollingCoordinator.h"
#include "platform/EventDispatchForbiddenScope.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/weborigin/Suborigin.h"
#include "public/platform/Platform.h"
#include "public/platform/WebFrameScheduler.h"
#include "public/platform/WebScreenInfo.h"
#include <memory>

namespace blink {

// Rather than simply inheriting LocalFrameLifecycleObserver like most other
// classes, LocalDOMWindow hides its LocalFrameLifecycleObserver with
// composition. This prevents conflicting overloads between DOMWindow, which
// has a frame() accessor that returns Frame* for bindings code, and
// LocalFrameLifecycleObserver, which has a frame() accessor that returns a
// LocalFrame*.
class LocalDOMWindow::WindowFrameObserver final : public GarbageCollected<LocalDOMWindow::WindowFrameObserver>, public LocalFrameLifecycleObserver {
    USING_GARBAGE_COLLECTED_MIXIN(WindowFrameObserver);
public:
    static WindowFrameObserver* create(LocalDOMWindow* window, LocalFrame& frame)
    {
        return new WindowFrameObserver(window, frame);
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_window);
        LocalFrameLifecycleObserver::trace(visitor);
    }

    // LocalFrameLifecycleObserver overrides:
    void willDetachFrameHost() override
    {
        m_window->willDetachFrameHost();
    }

    void contextDestroyed() override
    {
        m_window->frameDestroyed();
        LocalFrameLifecycleObserver::contextDestroyed();
    }

private:
    WindowFrameObserver(LocalDOMWindow* window, LocalFrame& frame)
        : LocalFrameLifecycleObserver(&frame)
        , m_window(window)
    {
    }

    Member<LocalDOMWindow> m_window;
};

class PostMessageTimer final : public GarbageCollectedFinalized<PostMessageTimer>, public SuspendableTimer {
    USING_GARBAGE_COLLECTED_MIXIN(PostMessageTimer);
public:
    PostMessageTimer(LocalDOMWindow& window, MessageEvent* event, PassRefPtr<SecurityOrigin> targetOrigin, std::unique_ptr<SourceLocation> location, UserGestureToken* userGestureToken)
        : SuspendableTimer(window.document())
        , m_event(event)
        , m_window(&window)
        , m_targetOrigin(targetOrigin)
        , m_location(std::move(location))
        , m_userGestureToken(userGestureToken)
        , m_disposalAllowed(true)
    {
        InspectorInstrumentation::asyncTaskScheduled(window.document(), "postMessage", this);
    }

    MessageEvent* event() const { return m_event; }
    SecurityOrigin* targetOrigin() const { return m_targetOrigin.get(); }
    std::unique_ptr<SourceLocation> takeLocation() { return std::move(m_location); }
    UserGestureToken* userGestureToken() const { return m_userGestureToken.get(); }
    void stop() override
    {
        SuspendableTimer::stop();

        if (m_disposalAllowed)
            dispose();
    }

    // Eager finalization is needed to promptly stop this timer object.
    // (see DOMTimer comment for more.)
    EAGERLY_FINALIZE();
    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_event);
        visitor->trace(m_window);
        SuspendableTimer::trace(visitor);
    }

    // TODO(alexclarke): Override timerTaskRunner() to pass in a document specific default task runner.

private:
    void fired() override
    {
        InspectorInstrumentation::AsyncTask asyncTask(m_window->document(), this);
        m_disposalAllowed = false;
        m_window->postMessageTimerFired(this);
        dispose();
        // Oilpan optimization: unregister as an observer right away.
        clearContext();
    }

    void dispose()
    {
        m_window->removePostMessageTimer(this);
    }

    Member<MessageEvent> m_event;
    Member<LocalDOMWindow> m_window;
    RefPtr<SecurityOrigin> m_targetOrigin;
    std::unique_ptr<SourceLocation> m_location;
    RefPtr<UserGestureToken> m_userGestureToken;
    bool m_disposalAllowed;
};

static void updateSuddenTerminationStatus(LocalDOMWindow* domWindow, bool addedListener, FrameLoaderClient::SuddenTerminationDisablerType disablerType)
{
    Platform::current()->suddenTerminationChanged(!addedListener);
    if (domWindow->frame() && domWindow->frame()->loader().client())
        domWindow->frame()->loader().client()->suddenTerminationDisablerChanged(addedListener, disablerType);
}

using DOMWindowSet = PersistentHeapHashCountedSet<WeakMember<LocalDOMWindow>>;

static DOMWindowSet& windowsWithUnloadEventListeners()
{
    DEFINE_STATIC_LOCAL(DOMWindowSet, windowsWithUnloadEventListeners, ());
    return windowsWithUnloadEventListeners;
}

static DOMWindowSet& windowsWithBeforeUnloadEventListeners()
{
    DEFINE_STATIC_LOCAL(DOMWindowSet, windowsWithBeforeUnloadEventListeners, ());
    return windowsWithBeforeUnloadEventListeners;
}

static void addUnloadEventListener(LocalDOMWindow* domWindow)
{
    DOMWindowSet& set = windowsWithUnloadEventListeners();
    if (set.isEmpty())
        updateSuddenTerminationStatus(domWindow, true, FrameLoaderClient::UnloadHandler);

    set.add(domWindow);
}

static void removeUnloadEventListener(LocalDOMWindow* domWindow)
{
    DOMWindowSet& set = windowsWithUnloadEventListeners();
    DOMWindowSet::iterator it = set.find(domWindow);
    if (it == set.end())
        return;
    set.remove(it);
    if (set.isEmpty())
        updateSuddenTerminationStatus(domWindow, false, FrameLoaderClient::UnloadHandler);
}

static void removeAllUnloadEventListeners(LocalDOMWindow* domWindow)
{
    DOMWindowSet& set = windowsWithUnloadEventListeners();
    DOMWindowSet::iterator it = set.find(domWindow);
    if (it == set.end())
        return;
    set.removeAll(it);
    if (set.isEmpty())
        updateSuddenTerminationStatus(domWindow, false, FrameLoaderClient::UnloadHandler);
}

static void addBeforeUnloadEventListener(LocalDOMWindow* domWindow)
{
    DOMWindowSet& set = windowsWithBeforeUnloadEventListeners();
    if (set.isEmpty())
        updateSuddenTerminationStatus(domWindow, true, FrameLoaderClient::BeforeUnloadHandler);

    set.add(domWindow);
}

static void removeBeforeUnloadEventListener(LocalDOMWindow* domWindow)
{
    DOMWindowSet& set = windowsWithBeforeUnloadEventListeners();
    DOMWindowSet::iterator it = set.find(domWindow);
    if (it == set.end())
        return;
    set.remove(it);
    if (set.isEmpty())
        updateSuddenTerminationStatus(domWindow, false, FrameLoaderClient::BeforeUnloadHandler);
}

static void removeAllBeforeUnloadEventListeners(LocalDOMWindow* domWindow)
{
    DOMWindowSet& set = windowsWithBeforeUnloadEventListeners();
    DOMWindowSet::iterator it = set.find(domWindow);
    if (it == set.end())
        return;
    set.removeAll(it);
    if (set.isEmpty())
        updateSuddenTerminationStatus(domWindow, false, FrameLoaderClient::BeforeUnloadHandler);
}

static bool allowsBeforeUnloadListeners(LocalDOMWindow* window)
{
    DCHECK(window);
    LocalFrame* frame = window->frame();
    if (!frame)
        return false;
    return frame->isMainFrame();
}

unsigned LocalDOMWindow::pendingUnloadEventListeners() const
{
    return windowsWithUnloadEventListeners().count(const_cast<LocalDOMWindow*>(this));
}

bool LocalDOMWindow::allowPopUp(LocalFrame& firstFrame)
{
    if (UserGestureIndicator::utilizeUserGesture())
        return true;

    Settings* settings = firstFrame.settings();
    return settings && settings->javaScriptCanOpenWindowsAutomatically();
}

bool LocalDOMWindow::allowPopUp()
{
    return frame() && allowPopUp(*frame());
}

LocalDOMWindow::LocalDOMWindow(LocalFrame& frame)
    : m_frameObserver(WindowFrameObserver::create(this, frame))
    , m_visualViewport(DOMVisualViewport::create(this))
    , m_shouldPrintWhenFinishedLoading(false)
{
    ThreadState::current()->registerPreFinalizer(this);
}

void LocalDOMWindow::clearDocument()
{
    if (!m_document)
        return;

    ASSERT(!m_document->isActive());

    // FIXME: This should be part of ActiveDOMObject shutdown
    clearEventQueue();

    m_document->clearDOMWindow();
    m_document = nullptr;
}

void LocalDOMWindow::clearEventQueue()
{
    if (!m_eventQueue)
        return;
    m_eventQueue->close();
    m_eventQueue.clear();
}

void LocalDOMWindow::acceptLanguagesChanged()
{
    if (m_navigator)
        m_navigator->setLanguagesChanged();

    dispatchEvent(Event::create(EventTypeNames::languagechange));
}

Document* LocalDOMWindow::createDocument(const String& mimeType, const DocumentInit& init, bool forceXHTML)
{
    Document* document = nullptr;
    if (forceXHTML) {
        // This is a hack for XSLTProcessor. See XSLTProcessor::createDocumentFromSource().
        document = Document::create(init);
    } else {
        document = DOMImplementation::createDocument(mimeType, init, init.frame() ? init.frame()->inViewSourceMode() : false);
        if (document->isPluginDocument() && document->isSandboxed(SandboxPlugins))
            document = SinkDocument::create(init);
    }

    return document;
}

Document* LocalDOMWindow::installNewDocument(const String& mimeType, const DocumentInit& init, bool forceXHTML)
{
    ASSERT(init.frame() == frame());

    clearDocument();

    m_document = createDocument(mimeType, init, forceXHTML);
    m_eventQueue = DOMWindowEventQueue::create(m_document.get());
    m_document->attach();

    if (!frame())
        return m_document;

    frame()->script().updateDocument();
    m_document->updateViewportDescription();

    if (frame()->page() && frame()->view()) {
        if (ScrollingCoordinator* scrollingCoordinator = frame()->page()->scrollingCoordinator()) {
            scrollingCoordinator->scrollableAreaScrollbarLayerDidChange(frame()->view(), HorizontalScrollbar);
            scrollingCoordinator->scrollableAreaScrollbarLayerDidChange(frame()->view(), VerticalScrollbar);
            scrollingCoordinator->scrollableAreaScrollLayerDidChange(frame()->view());
        }
    }

    frame()->selection().updateSecureKeyboardEntryIfActive();
    return m_document;
}

EventQueue* LocalDOMWindow::getEventQueue() const
{
    return m_eventQueue.get();
}

void LocalDOMWindow::enqueueWindowEvent(Event* event)
{
    if (!m_eventQueue)
        return;
    event->setTarget(this);
    m_eventQueue->enqueueEvent(event);
}

void LocalDOMWindow::enqueueDocumentEvent(Event* event)
{
    if (!m_eventQueue)
        return;
    event->setTarget(m_document.get());
    m_eventQueue->enqueueEvent(event);
}

void LocalDOMWindow::dispatchWindowLoadEvent()
{
    ASSERT(!EventDispatchForbiddenScope::isEventDispatchForbidden());
    // Delay 'load' event if we are in EventQueueScope.  This is a short-term
    // workaround to avoid Editing code crashes.  We should always dispatch
    // 'load' event asynchronously.  crbug.com/569511.
    if (ScopedEventQueue::instance()->shouldQueueEvents() && m_document) {
        m_document->postTask(BLINK_FROM_HERE, createSameThreadTask(&LocalDOMWindow::dispatchLoadEvent, wrapPersistent(this)));
        return;
    }
    dispatchLoadEvent();
}

void LocalDOMWindow::documentWasClosed()
{
    dispatchWindowLoadEvent();
    enqueuePageshowEvent(PageshowEventNotPersisted);
    if (m_pendingStateObject)
        enqueuePopstateEvent(m_pendingStateObject.release());
}

void LocalDOMWindow::enqueuePageshowEvent(PageshowEventPersistence persisted)
{
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=36334 Pageshow event needs to fire asynchronously.
    // As per spec pageshow must be triggered asynchronously.
    // However to be compatible with other browsers blink fires pageshow synchronously.
    dispatchEvent(PageTransitionEvent::create(EventTypeNames::pageshow, persisted), m_document.get());
}

void LocalDOMWindow::enqueueHashchangeEvent(const String& oldURL, const String& newURL)
{
    enqueueWindowEvent(HashChangeEvent::create(oldURL, newURL));
}

void LocalDOMWindow::enqueuePopstateEvent(PassRefPtr<SerializedScriptValue> stateObject)
{
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=36202 Popstate event needs to fire asynchronously
    dispatchEvent(PopStateEvent::create(stateObject, history()));
}

void LocalDOMWindow::statePopped(PassRefPtr<SerializedScriptValue> stateObject)
{
    if (!frame())
        return;

    // Per step 11 of section 6.5.9 (history traversal) of the HTML5 spec, we
    // defer firing of popstate until we're in the complete state.
    if (document()->isLoadCompleted())
        enqueuePopstateEvent(stateObject);
    else
        m_pendingStateObject = stateObject;
}

LocalDOMWindow::~LocalDOMWindow()
{
    // Cleared when detaching document.
    ASSERT(!m_eventQueue);
}

void LocalDOMWindow::dispose()
{
    // Oilpan: should the LocalDOMWindow be GCed along with its LocalFrame without the
    // frame having first notified its observers of imminent destruction, the
    // LocalDOMWindow will not have had an opportunity to remove event listeners.
    //
    // Arrange for that removal to happen using a prefinalizer action. Making LocalDOMWindow
    // eager finalizable is problematic as other eagerly finalized objects may well
    // want to access their associated LocalDOMWindow from their destructors.
    if (!frame())
        return;

    removeAllEventListeners();
}

ExecutionContext* LocalDOMWindow::getExecutionContext() const
{
    return m_document.get();
}

const LocalDOMWindow* LocalDOMWindow::toLocalDOMWindow() const
{
    return this;
}

LocalDOMWindow* LocalDOMWindow::toLocalDOMWindow()
{
    return this;
}

MediaQueryList* LocalDOMWindow::matchMedia(const String& media)
{
    return document() ? document()->mediaQueryMatcher().matchMedia(media) : nullptr;
}

void LocalDOMWindow::willDetachFrameHost()
{
    frame()->host()->eventHandlerRegistry().didRemoveAllEventHandlers(*this);
    LocalDOMWindow::notifyContextDestroyed();
}

void LocalDOMWindow::frameDestroyed()
{
    willDestroyDocumentInFrame();
    resetLocation();
    m_properties.clear();
    removeAllEventListeners();
}

void LocalDOMWindow::willDestroyDocumentInFrame()
{
    for (const auto& domWindowProperty : m_properties)
        domWindowProperty->willDestroyGlobalObjectInFrame();
}

void LocalDOMWindow::willDetachDocumentFromFrame()
{
    for (const auto& domWindowProperty : m_properties)
        domWindowProperty->willDetachGlobalObjectFromFrame();
}

void LocalDOMWindow::registerProperty(DOMWindowProperty* property)
{
    m_properties.add(property);
}

void LocalDOMWindow::unregisterProperty(DOMWindowProperty* property)
{
    m_properties.remove(property);
}

void LocalDOMWindow::reset()
{
    m_frameObserver->contextDestroyed();

    m_screen = nullptr;
    m_history = nullptr;
    m_locationbar = nullptr;
    m_menubar = nullptr;
    m_personalbar = nullptr;
    m_scrollbars = nullptr;
    m_statusbar = nullptr;
    m_toolbar = nullptr;
    m_navigator = nullptr;
    m_media = nullptr;
    m_customElements = nullptr;
    m_applicationCache = nullptr;

    LocalDOMWindow::notifyContextDestroyed();
}

void LocalDOMWindow::sendOrientationChangeEvent()
{
    ASSERT(RuntimeEnabledFeatures::orientationEventEnabled());
    ASSERT(frame()->isMainFrame());

    // Before dispatching the event, build a list of all frames in the page
    // to send the event to, to mitigate side effects from event handlers
    // potentially interfering with others.
    HeapVector<Member<Frame>> frames;
    for (Frame* f = frame(); f; f = f->tree().traverseNext())
        frames.append(f);

    for (size_t i = 0; i < frames.size(); ++i) {
        if (!frames[i]->isLocalFrame())
            continue;
        toLocalFrame(frames[i].get())->localDOMWindow()->dispatchEvent(Event::create(EventTypeNames::orientationchange));
    }
}

int LocalDOMWindow::orientation() const
{
    ASSERT(RuntimeEnabledFeatures::orientationEventEnabled());

    if (!frame() || !frame()->host())
        return 0;

    int orientation = frame()->host()->chromeClient().screenInfo().orientationAngle;
    // For backward compatibility, we want to return a value in the range of
    // [-90; 180] instead of [0; 360[ because window.orientation used to behave
    // like that in WebKit (this is a WebKit proprietary API).
    if (orientation == 270)
        return -90;
    return orientation;
}

Screen* LocalDOMWindow::screen() const
{
    if (!m_screen)
        m_screen = Screen::create(frame());
    return m_screen.get();
}

History* LocalDOMWindow::history() const
{
    if (!m_history)
        m_history = History::create(frame());
    return m_history.get();
}

BarProp* LocalDOMWindow::locationbar() const
{
    if (!m_locationbar)
        m_locationbar = BarProp::create(frame(), BarProp::Locationbar);
    return m_locationbar.get();
}

BarProp* LocalDOMWindow::menubar() const
{
    if (!m_menubar)
        m_menubar = BarProp::create(frame(), BarProp::Menubar);
    return m_menubar.get();
}

BarProp* LocalDOMWindow::personalbar() const
{
    if (!m_personalbar)
        m_personalbar = BarProp::create(frame(), BarProp::Personalbar);
    return m_personalbar.get();
}

BarProp* LocalDOMWindow::scrollbars() const
{
    if (!m_scrollbars)
        m_scrollbars = BarProp::create(frame(), BarProp::Scrollbars);
    return m_scrollbars.get();
}

BarProp* LocalDOMWindow::statusbar() const
{
    if (!m_statusbar)
        m_statusbar = BarProp::create(frame(), BarProp::Statusbar);
    return m_statusbar.get();
}

BarProp* LocalDOMWindow::toolbar() const
{
    if (!m_toolbar)
        m_toolbar = BarProp::create(frame(), BarProp::Toolbar);
    return m_toolbar.get();
}

FrameConsole* LocalDOMWindow::frameConsole() const
{
    if (!isCurrentlyDisplayedInFrame())
        return nullptr;
    return &frame()->console();
}

ApplicationCache* LocalDOMWindow::applicationCache() const
{
    if (!isCurrentlyDisplayedInFrame())
        return nullptr;
    if (!m_applicationCache)
        m_applicationCache = ApplicationCache::create(frame());
    return m_applicationCache.get();
}

Navigator* LocalDOMWindow::navigator() const
{
    if (!m_navigator)
        m_navigator = Navigator::create(frame());
    return m_navigator.get();
}

void LocalDOMWindow::schedulePostMessage(MessageEvent* event, PassRefPtr<SecurityOrigin> target, Document* source)
{
    // Allowing unbounded amounts of messages to build up for a suspended context
    // is problematic; consider imposing a limit or other restriction if this
    // surfaces often as a problem (see crbug.com/587012).
    PostMessageTimer* timer = new PostMessageTimer(*this, event, target, SourceLocation::capture(source), UserGestureIndicator::currentToken());
    timer->startOneShot(0, BLINK_FROM_HERE);
    timer->suspendIfNeeded();
    m_postMessageTimers.add(timer);
}

void LocalDOMWindow::postMessageTimerFired(PostMessageTimer* timer)
{
    if (!isCurrentlyDisplayedInFrame())
        return;

    MessageEvent* event = timer->event();

    UserGestureIndicator gestureIndicator(timer->userGestureToken());

    event->entangleMessagePorts(document());

    dispatchMessageEventWithOriginCheck(timer->targetOrigin(), event, timer->takeLocation());
}

void LocalDOMWindow::removePostMessageTimer(PostMessageTimer* timer)
{
    m_postMessageTimers.remove(timer);
}

void LocalDOMWindow::dispatchMessageEventWithOriginCheck(SecurityOrigin* intendedTargetOrigin, Event* event, std::unique_ptr<SourceLocation> location)
{
    if (intendedTargetOrigin) {
        // Check target origin now since the target document may have changed since the timer was scheduled.
        SecurityOrigin* securityOrigin = document()->getSecurityOrigin();
        bool validTarget = intendedTargetOrigin->isSameSchemeHostPortAndSuborigin(securityOrigin);
        if (securityOrigin->hasSuborigin() && securityOrigin->suborigin()->policyContains(Suborigin::SuboriginPolicyOptions::UnsafePostMessageReceive))
            validTarget = intendedTargetOrigin->isSameSchemeHostPort(securityOrigin);

        if (!validTarget) {
            String message = ExceptionMessages::failedToExecute("postMessage", "DOMWindow", "The target origin provided ('" + intendedTargetOrigin->toString() + "') does not match the recipient window's origin ('" + document()->getSecurityOrigin()->toString() + "').");
            ConsoleMessage* consoleMessage = ConsoleMessage::create(SecurityMessageSource, ErrorMessageLevel, message, std::move(location));
            frameConsole()->addMessage(consoleMessage);
            return;
        }
    }

    dispatchEvent(event);
}

DOMSelection* LocalDOMWindow::getSelection()
{
    if (!isCurrentlyDisplayedInFrame())
        return nullptr;

    return frame()->document()->getSelection();
}

Element* LocalDOMWindow::frameElement() const
{
    if (!(frame() && frame()->owner() && frame()->owner()->isLocal()))
        return nullptr;

    return toHTMLFrameOwnerElement(frame()->owner());
}

void LocalDOMWindow::blur()
{
}

void LocalDOMWindow::print(ScriptState* scriptState)
{
    if (!frame())
        return;

    FrameHost* host = frame()->host();
    if (!host)
        return;

    if (frame()->document()->isSandboxed(SandboxModals)) {
        UseCounter::count(frame()->document(), UseCounter::DialogInSandboxedContext);
        if (RuntimeEnabledFeatures::sandboxBlocksModalsEnabled()) {
            frameConsole()->addMessage(ConsoleMessage::create(SecurityMessageSource, ErrorMessageLevel, "Ignored call to 'print()'. The document is sandboxed, and the 'allow-modals' keyword is not set."));
            return;
        }
    }

    if (scriptState && v8::MicrotasksScope::IsRunningMicrotasks(scriptState->isolate())) {
        UseCounter::count(frame()->document(), UseCounter::During_Microtask_Print);
    }

    if (frame()->isLoading()) {
        m_shouldPrintWhenFinishedLoading = true;
        return;
    }

    if (frame()->isCrossOrigin())
        UseCounter::count(frame()->document(), UseCounter::CrossOriginWindowPrint);

    m_shouldPrintWhenFinishedLoading = false;
    host->chromeClient().print(frame());
}

void LocalDOMWindow::stop()
{
    if (!frame())
        return;
    frame()->loader().stopAllLoaders();
}

void LocalDOMWindow::alert(ScriptState* scriptState, const String& message)
{
    if (!frame())
        return;

    if (frame()->document()->isSandboxed(SandboxModals)) {
        UseCounter::count(frame()->document(), UseCounter::DialogInSandboxedContext);
        if (RuntimeEnabledFeatures::sandboxBlocksModalsEnabled()) {
            frameConsole()->addMessage(ConsoleMessage::create(SecurityMessageSource, ErrorMessageLevel, "Ignored call to 'alert()'. The document is sandboxed, and the 'allow-modals' keyword is not set."));
            return;
        }
    }

    if (v8::MicrotasksScope::IsRunningMicrotasks(scriptState->isolate())) {
        UseCounter::count(frame()->document(), UseCounter::During_Microtask_Alert);
    }

    frame()->document()->updateStyleAndLayoutTree();

    FrameHost* host = frame()->host();
    if (!host)
        return;

    if (frame()->isCrossOrigin())
        UseCounter::count(frame()->document(), UseCounter::CrossOriginWindowAlert);

    host->chromeClient().openJavaScriptAlert(frame(), message);
}

bool LocalDOMWindow::confirm(ScriptState* scriptState, const String& message)
{
    if (!frame())
        return false;

    if (frame()->document()->isSandboxed(SandboxModals)) {
        UseCounter::count(frame()->document(), UseCounter::DialogInSandboxedContext);
        if (RuntimeEnabledFeatures::sandboxBlocksModalsEnabled()) {
            frameConsole()->addMessage(ConsoleMessage::create(SecurityMessageSource, ErrorMessageLevel, "Ignored call to 'confirm()'. The document is sandboxed, and the 'allow-modals' keyword is not set."));
            return false;
        }
    }

    if (v8::MicrotasksScope::IsRunningMicrotasks(scriptState->isolate())) {
        UseCounter::count(frame()->document(), UseCounter::During_Microtask_Confirm);
    }

    frame()->document()->updateStyleAndLayoutTree();

    FrameHost* host = frame()->host();
    if (!host)
        return false;

    if (frame()->isCrossOrigin())
        UseCounter::count(frame()->document(), UseCounter::CrossOriginWindowConfirm);

    return host->chromeClient().openJavaScriptConfirm(frame(), message);
}

String LocalDOMWindow::prompt(ScriptState* scriptState, const String& message, const String& defaultValue)
{
    if (!frame())
        return String();

    if (frame()->document()->isSandboxed(SandboxModals)) {
        UseCounter::count(frame()->document(), UseCounter::DialogInSandboxedContext);
        if (RuntimeEnabledFeatures::sandboxBlocksModalsEnabled()) {
            frameConsole()->addMessage(ConsoleMessage::create(SecurityMessageSource, ErrorMessageLevel, "Ignored call to 'prompt()'. The document is sandboxed, and the 'allow-modals' keyword is not set."));
            return String();
        }
    }

    if (v8::MicrotasksScope::IsRunningMicrotasks(scriptState->isolate())) {
        UseCounter::count(frame()->document(), UseCounter::During_Microtask_Prompt);
    }

    frame()->document()->updateStyleAndLayoutTree();

    FrameHost* host = frame()->host();
    if (!host)
        return String();

    String returnValue;
    if (host->chromeClient().openJavaScriptPrompt(frame(), message, defaultValue, returnValue))
        return returnValue;

    if (frame()->isCrossOrigin())
        UseCounter::count(frame()->document(), UseCounter::CrossOriginWindowPrompt);

    return String();
}

bool LocalDOMWindow::find(const String& string, bool caseSensitive, bool backwards, bool wrap, bool wholeWord, bool /*searchInFrames*/, bool /*showDialog*/) const
{
    if (!isCurrentlyDisplayedInFrame())
        return false;

    // FIXME (13016): Support searchInFrames and showDialog
    FindOptions options = (backwards ? Backwards : 0) | (caseSensitive ? 0 : CaseInsensitive) | (wrap ? WrapAround : 0) | (wholeWord ? WholeWord | AtWordStarts : 0);
    return frame()->editor().findString(string, options);
}

bool LocalDOMWindow::offscreenBuffering() const
{
    return true;
}

int LocalDOMWindow::outerHeight() const
{
    if (!frame())
        return 0;

    FrameHost* host = frame()->host();
    if (!host)
        return 0;

    ChromeClient& chromeClient = host->chromeClient();
    if (host->settings().reportScreenSizeInPhysicalPixelsQuirk())
        return lroundf(chromeClient.windowRect().height() * chromeClient.screenInfo().deviceScaleFactor);
    return chromeClient.windowRect().height();
}

int LocalDOMWindow::outerWidth() const
{
    if (!frame())
        return 0;

    FrameHost* host = frame()->host();
    if (!host)
        return 0;

    ChromeClient& chromeClient = host->chromeClient();
    if (host->settings().reportScreenSizeInPhysicalPixelsQuirk())
        return lroundf(chromeClient.windowRect().width() * chromeClient.screenInfo().deviceScaleFactor);

    return chromeClient.windowRect().width();
}

FloatSize LocalDOMWindow::getViewportSize(IncludeScrollbarsInRect scrollbarInclusion) const
{
    if (!frame())
        return FloatSize();

    FrameView* view = frame()->view();
    if (!view)
        return FloatSize();

    FrameHost* host = frame()->host();
    if (!host)
        return FloatSize();

    // The main frame's viewport size depends on the page scale. Since the
    // initial page scale depends on the content width and is set after a
    // layout, perform one now so queries during page load will use the up to
    // date viewport.
    if (host->settings().viewportEnabled() && frame()->isMainFrame())
        frame()->document()->updateStyleAndLayoutIgnorePendingStylesheets();

    // FIXME: This is potentially too much work. We really only need to know the dimensions of the parent frame's layoutObject.
    if (Frame* parent = frame()->tree().parent()) {
        if (parent && parent->isLocalFrame())
            toLocalFrame(parent)->document()->updateStyleAndLayoutIgnorePendingStylesheets();
    }

    return frame()->isMainFrame() && !host->settings().inertVisualViewport()
        ? FloatSize(host->visualViewport().visibleRect().size())
        : FloatSize(view->visibleContentRect(scrollbarInclusion).size());
}

int LocalDOMWindow::innerHeight() const
{
    if (!frame())
        return 0;

    FloatSize viewportSize = getViewportSize(IncludeScrollbars);
    return adjustForAbsoluteZoom(expandedIntSize(viewportSize).height(), frame()->pageZoomFactor());
}

int LocalDOMWindow::innerWidth() const
{
    if (!frame())
        return 0;

    FloatSize viewportSize = getViewportSize(IncludeScrollbars);
    return adjustForAbsoluteZoom(expandedIntSize(viewportSize).width(), frame()->pageZoomFactor());
}

int LocalDOMWindow::screenX() const
{
    if (!frame())
        return 0;

    FrameHost* host = frame()->host();
    if (!host)
        return 0;

    ChromeClient& chromeClient = host->chromeClient();
    if (host->settings().reportScreenSizeInPhysicalPixelsQuirk())
        return lroundf(chromeClient.windowRect().x() * chromeClient.screenInfo().deviceScaleFactor);
    return chromeClient.windowRect().x();
}

int LocalDOMWindow::screenY() const
{
    if (!frame())
        return 0;

    FrameHost* host = frame()->host();
    if (!host)
        return 0;

    ChromeClient& chromeClient = host->chromeClient();
    if (host->settings().reportScreenSizeInPhysicalPixelsQuirk())
        return lroundf(chromeClient.windowRect().y() * chromeClient.screenInfo().deviceScaleFactor);
    return chromeClient.windowRect().y();
}

double LocalDOMWindow::scrollX() const
{
    if (!frame())
        return 0;

    FrameView* view = frame()->view();
    if (!view)
        return 0;

    FrameHost* host = frame()->host();
    if (!host)
        return 0;

    frame()->document()->updateStyleAndLayoutIgnorePendingStylesheets();

    ScrollableArea* viewport = host->settings().inertVisualViewport() ? view->layoutViewportScrollableArea() : view->getScrollableArea();
    double viewportX = viewport->scrollPositionDouble().x();
    return adjustScrollForAbsoluteZoom(viewportX, frame()->pageZoomFactor());
}

double LocalDOMWindow::scrollY() const
{
    if (!frame())
        return 0;

    FrameView* view = frame()->view();
    if (!view)
        return 0;

    FrameHost* host = frame()->host();
    if (!host)
        return 0;

    frame()->document()->updateStyleAndLayoutIgnorePendingStylesheets();

    ScrollableArea* viewport = host->settings().inertVisualViewport() ? view->layoutViewportScrollableArea() : view->getScrollableArea();
    double viewportY = viewport->scrollPositionDouble().y();
    return adjustScrollForAbsoluteZoom(viewportY, frame()->pageZoomFactor());
}

DOMVisualViewport* LocalDOMWindow::visualViewport()
{
    if (!frame())
        return nullptr;

    return m_visualViewport;
}

const AtomicString& LocalDOMWindow::name() const
{
    if (!isCurrentlyDisplayedInFrame())
        return nullAtom;

    return frame()->tree().name();
}

void LocalDOMWindow::setName(const AtomicString& name)
{
    if (!isCurrentlyDisplayedInFrame())
        return;

    if (name == frame()->tree().name())
        return;

    frame()->tree().setName(name);
    ASSERT(frame()->loader().client());
    frame()->loader().client()->didChangeName(name, frame()->tree().uniqueName());
}

void LocalDOMWindow::setStatus(const String& string)
{
    m_status = string;

    if (!frame())
        return;

    FrameHost* host = frame()->host();
    if (!host)
        return;

    ASSERT(frame()->document()); // Client calls shouldn't be made when the frame is in inconsistent state.
    host->chromeClient().setStatusbarText(m_status);
}

void LocalDOMWindow::setDefaultStatus(const String& string)
{
    m_defaultStatus = string;

    if (!frame())
        return;

    FrameHost* host = frame()->host();
    if (!host)
        return;

    ASSERT(frame()->document()); // Client calls shouldn't be made when the frame is in inconsistent state.
    host->chromeClient().setStatusbarText(m_defaultStatus);
}

Document* LocalDOMWindow::document() const
{
    return m_document.get();
}

StyleMedia* LocalDOMWindow::styleMedia() const
{
    if (!m_media)
        m_media = StyleMedia::create(frame());
    return m_media.get();
}

CSSStyleDeclaration* LocalDOMWindow::getComputedStyle(Element* elt, const String& pseudoElt) const
{
    ASSERT(elt);
    return CSSComputedStyleDeclaration::create(elt, false, pseudoElt);
}

CSSRuleList* LocalDOMWindow::getMatchedCSSRules(Element* element, const String& pseudoElement) const
{
    if (!element)
        return nullptr;

    if (!isCurrentlyDisplayedInFrame())
        return nullptr;

    unsigned colonStart = pseudoElement[0] == ':' ? (pseudoElement[1] == ':' ? 2 : 1) : 0;
    CSSSelector::PseudoType pseudoType = CSSSelector::parsePseudoType(AtomicString(pseudoElement.substring(colonStart)), false);
    if (pseudoType == CSSSelector::PseudoUnknown && !pseudoElement.isEmpty())
        return nullptr;

    unsigned rulesToInclude = StyleResolver::AuthorCSSRules;
    PseudoId pseudoId = CSSSelector::pseudoId(pseudoType);
    element->document().updateStyleAndLayoutTree();
    return frame()->document()->ensureStyleResolver().pseudoCSSRulesForElement(element, pseudoId, rulesToInclude);
}

double LocalDOMWindow::devicePixelRatio() const
{
    if (!frame())
        return 0.0;

    return frame()->devicePixelRatio();
}

void LocalDOMWindow::scrollBy(double x, double y, ScrollBehavior scrollBehavior) const
{
    if (!isCurrentlyDisplayedInFrame())
        return;

    document()->updateStyleAndLayoutIgnorePendingStylesheets();

    FrameView* view = frame()->view();
    if (!view)
        return;

    FrameHost* host = frame()->host();
    if (!host)
        return;

    x = ScrollableArea::normalizeNonFiniteScroll(x);
    y = ScrollableArea::normalizeNonFiniteScroll(y);

    ScrollableArea* viewport = host->settings().inertVisualViewport() ? view->layoutViewportScrollableArea() : view->getScrollableArea();

    DoublePoint currentOffset = viewport->scrollPositionDouble();
    DoubleSize scaledDelta(x * frame()->pageZoomFactor(), y * frame()->pageZoomFactor());

    viewport->setScrollPosition(currentOffset + scaledDelta, ProgrammaticScroll, scrollBehavior);
}

void LocalDOMWindow::scrollBy(const ScrollToOptions& scrollToOptions) const
{
    double x = 0.0;
    double y = 0.0;
    if (scrollToOptions.hasLeft())
        x = scrollToOptions.left();
    if (scrollToOptions.hasTop())
        y = scrollToOptions.top();
    ScrollBehavior scrollBehavior = ScrollBehaviorAuto;
    ScrollableArea::scrollBehaviorFromString(scrollToOptions.behavior(), scrollBehavior);
    scrollBy(x, y, scrollBehavior);
}

void LocalDOMWindow::scrollTo(double x, double y) const
{
    if (!isCurrentlyDisplayedInFrame())
        return;

    FrameView* view = frame()->view();
    if (!view)
        return;

    FrameHost* host = frame()->host();
    if (!host)
        return;

    x = ScrollableArea::normalizeNonFiniteScroll(x);
    y = ScrollableArea::normalizeNonFiniteScroll(y);

    // It is only necessary to have an up-to-date layout if the position may be clamped,
    // which is never the case for (0, 0).
    if (x || y)
        document()->updateStyleAndLayoutIgnorePendingStylesheets();

    DoublePoint layoutPos(x * frame()->pageZoomFactor(), y * frame()->pageZoomFactor());
    ScrollableArea* viewport = host->settings().inertVisualViewport() ? view->layoutViewportScrollableArea() : view->getScrollableArea();
    viewport->setScrollPosition(layoutPos, ProgrammaticScroll, ScrollBehaviorAuto);
}

void LocalDOMWindow::scrollTo(const ScrollToOptions& scrollToOptions) const
{
    if (!isCurrentlyDisplayedInFrame())
        return;

    FrameView* view = frame()->view();
    if (!view)
        return;

    FrameHost* host = frame()->host();
    if (!host)
        return;

    // It is only necessary to have an up-to-date layout if the position may be clamped,
    // which is never the case for (0, 0).
    if (!scrollToOptions.hasLeft()
        || !scrollToOptions.hasTop()
        || scrollToOptions.left()
        || scrollToOptions.top()) {
        document()->updateStyleAndLayoutIgnorePendingStylesheets();
    }

    double scaledX = 0.0;
    double scaledY = 0.0;

    ScrollableArea* viewport = host->settings().inertVisualViewport() ? view->layoutViewportScrollableArea() : view->getScrollableArea();

    DoublePoint currentOffset = viewport->scrollPositionDouble();
    scaledX = currentOffset.x();
    scaledY = currentOffset.y();

    if (scrollToOptions.hasLeft())
        scaledX = ScrollableArea::normalizeNonFiniteScroll(scrollToOptions.left()) * frame()->pageZoomFactor();

    if (scrollToOptions.hasTop())
        scaledY = ScrollableArea::normalizeNonFiniteScroll(scrollToOptions.top()) * frame()->pageZoomFactor();

    ScrollBehavior scrollBehavior = ScrollBehaviorAuto;
    ScrollableArea::scrollBehaviorFromString(scrollToOptions.behavior(), scrollBehavior);

    viewport->setScrollPosition(DoublePoint(scaledX, scaledY), ProgrammaticScroll, scrollBehavior);
}

void LocalDOMWindow::moveBy(int x, int y) const
{
    if (!frame() || !frame()->isMainFrame())
        return;

    FrameHost* host = frame()->host();
    if (!host)
        return;

    IntRect windowRect = host->chromeClient().windowRect();
    windowRect.move(x, y);
    // Security check (the spec talks about UniversalBrowserWrite to disable this check...)
    host->chromeClient().setWindowRectWithAdjustment(windowRect);
}

void LocalDOMWindow::moveTo(int x, int y) const
{
    if (!frame() || !frame()->isMainFrame())
        return;

    FrameHost* host = frame()->host();
    if (!host)
        return;

    IntRect windowRect = host->chromeClient().windowRect();
    windowRect.setLocation(IntPoint(x, y));
    // Security check (the spec talks about UniversalBrowserWrite to disable this check...)
    host->chromeClient().setWindowRectWithAdjustment(windowRect);
}

void LocalDOMWindow::resizeBy(int x, int y) const
{
    if (!frame() || !frame()->isMainFrame())
        return;

    FrameHost* host = frame()->host();
    if (!host)
        return;

    IntRect fr = host->chromeClient().windowRect();
    IntSize dest = fr.size() + IntSize(x, y);
    IntRect update(fr.location(), dest);
    host->chromeClient().setWindowRectWithAdjustment(update);
}

void LocalDOMWindow::resizeTo(int width, int height) const
{
    if (!frame() || !frame()->isMainFrame())
        return;

    FrameHost* host = frame()->host();
    if (!host)
        return;

    IntRect fr = host->chromeClient().windowRect();
    IntSize dest = IntSize(width, height);
    IntRect update(fr.location(), dest);
    host->chromeClient().setWindowRectWithAdjustment(update);
}

int LocalDOMWindow::requestAnimationFrame(FrameRequestCallback* callback)
{
    callback->m_useLegacyTimeBase = false;
    if (Document* doc = document())
        return doc->requestAnimationFrame(callback);
    return 0;
}

int LocalDOMWindow::webkitRequestAnimationFrame(FrameRequestCallback* callback)
{
    callback->m_useLegacyTimeBase = true;
    if (Document* document = this->document())
        return document->requestAnimationFrame(callback);
    return 0;
}

void LocalDOMWindow::cancelAnimationFrame(int id)
{
    if (Document* document = this->document())
        document->cancelAnimationFrame(id);
}

int LocalDOMWindow::requestIdleCallback(IdleRequestCallback* callback, const IdleRequestOptions& options)
{
    if (Document* document = this->document())
        return document->requestIdleCallback(callback, options);
    return 0;
}

void LocalDOMWindow::cancelIdleCallback(int id)
{
    if (Document* document = this->document())
        document->cancelIdleCallback(id);
}

CustomElementsRegistry* LocalDOMWindow::customElements(ScriptState* scriptState) const
{
    if (!scriptState->world().isMainWorld())
        return nullptr;
    return customElements();
}

CustomElementsRegistry* LocalDOMWindow::customElements() const
{
    if (!m_customElements && m_document)
        m_customElements = CustomElementsRegistry::create(document());
    return m_customElements;
}

void LocalDOMWindow::addedEventListener(const AtomicString& eventType, RegisteredEventListener& registeredListener)
{
    DOMWindow::addedEventListener(eventType, registeredListener);
    if (frame() && frame()->host())
        frame()->host()->eventHandlerRegistry().didAddEventHandler(*this, eventType, registeredListener.options());

    if (Document* document = this->document())
        document->addListenerTypeIfNeeded(eventType);

    notifyAddEventListener(this, eventType);

    if (eventType == EventTypeNames::unload) {
        UseCounter::count(document(), UseCounter::DocumentUnloadRegistered);
        addUnloadEventListener(this);
    } else if (eventType == EventTypeNames::beforeunload) {
        UseCounter::count(document(), UseCounter::DocumentBeforeUnloadRegistered);
        if (allowsBeforeUnloadListeners(this)) {
            // This is confusingly named. It doesn't actually add the listener. It just increments a count
            // so that we know we have listeners registered for the purposes of determining if we can
            // fast terminate the renderer process.
            addBeforeUnloadEventListener(this);
        } else {
            // Subframes return false from allowsBeforeUnloadListeners.
            UseCounter::count(document(), UseCounter::SubFrameBeforeUnloadRegistered);
        }
    }
}

void LocalDOMWindow::removedEventListener(const AtomicString& eventType, const RegisteredEventListener& registeredListener)
{
    DOMWindow::removedEventListener(eventType, registeredListener);
    if (frame() && frame()->host())
        frame()->host()->eventHandlerRegistry().didRemoveEventHandler(*this, eventType, registeredListener.options());

    notifyRemoveEventListener(this, eventType);

    if (eventType == EventTypeNames::unload) {
        removeUnloadEventListener(this);
    } else if (eventType == EventTypeNames::beforeunload && allowsBeforeUnloadListeners(this)) {
        removeBeforeUnloadEventListener(this);
    }
}

void LocalDOMWindow::dispatchLoadEvent()
{
    Event* loadEvent(Event::create(EventTypeNames::load));
    if (frame() && frame()->loader().documentLoader() && !frame()->loader().documentLoader()->timing().loadEventStart()) {
        DocumentLoader* documentLoader = frame()->loader().documentLoader();
        DocumentLoadTiming& timing = documentLoader->timing();
        timing.markLoadEventStart();
        dispatchEvent(loadEvent, document());
        timing.markLoadEventEnd();
    } else {
        dispatchEvent(loadEvent, document());
    }

    // For load events, send a separate load event to the enclosing frame only.
    // This is a DOM extension and is independent of bubbling/capturing rules of
    // the DOM.
    FrameOwner* owner = frame() ? frame()->owner() : nullptr;
    if (owner)
        owner->dispatchLoad();

    TRACE_EVENT_INSTANT1("devtools.timeline", "MarkLoad", TRACE_EVENT_SCOPE_THREAD, "data", InspectorMarkLoadEvent::data(frame()));
    InspectorInstrumentation::loadEventFired(frame());
}

DispatchEventResult LocalDOMWindow::dispatchEvent(Event* event, EventTarget* target)
{
    ASSERT(!EventDispatchForbiddenScope::isEventDispatchForbidden());

    event->setTrusted(true);
    event->setTarget(target ? target : this);
    event->setCurrentTarget(this);
    event->setEventPhase(Event::AT_TARGET);

    TRACE_EVENT1("devtools.timeline", "EventDispatch", "data", InspectorEventDispatchEvent::data(*event));
    return fireEventListeners(event);
}

void LocalDOMWindow::removeAllEventListeners()
{
    EventTarget::removeAllEventListeners();

    notifyRemoveAllEventListeners(this);
    if (frame() && frame()->host())
        frame()->host()->eventHandlerRegistry().didRemoveAllEventHandlers(*this);

    removeAllUnloadEventListeners(this);
    removeAllBeforeUnloadEventListeners(this);
}

void LocalDOMWindow::finishedLoading()
{
    if (m_shouldPrintWhenFinishedLoading) {
        m_shouldPrintWhenFinishedLoading = false;
        print(nullptr);
    }
}

void LocalDOMWindow::printErrorMessage(const String& message) const
{
    if (!isCurrentlyDisplayedInFrame())
        return;

    if (message.isEmpty())
        return;

    frameConsole()->addMessage(ConsoleMessage::create(JSMessageSource, ErrorMessageLevel, message));
}

DOMWindow* LocalDOMWindow::open(const String& urlString, const AtomicString& frameName, const String& windowFeaturesString,
    LocalDOMWindow* callingWindow, LocalDOMWindow* enteredWindow)
{
    if (!isCurrentlyDisplayedInFrame())
        return nullptr;
    if (!callingWindow->frame())
        return nullptr;
    Document* activeDocument = callingWindow->document();
    if (!activeDocument)
        return nullptr;
    LocalFrame* firstFrame = enteredWindow->frame();
    if (!firstFrame)
        return nullptr;

    UseCounter::count(*activeDocument, UseCounter::DOMWindowOpen);
    if (!windowFeaturesString.isEmpty())
        UseCounter::count(*activeDocument, UseCounter::DOMWindowOpenFeatures);

    if (!enteredWindow->allowPopUp()) {
        // Because FrameTree::find() returns true for empty strings, we must check for empty frame names.
        // Otherwise, illegitimate window.open() calls with no name will pass right through the popup blocker.
        if (frameName.isEmpty() || !frame()->tree().find(frameName))
            return nullptr;
    }

    // Get the target frame for the special cases of _top and _parent.
    // In those cases, we schedule a location change right now and return early.
    Frame* targetFrame = nullptr;
    if (frameName == "_top") {
        targetFrame = frame()->tree().top();
    } else if (frameName == "_parent") {
        if (Frame* parent = frame()->tree().parent())
            targetFrame = parent;
        else
            targetFrame = frame();
    }

    if (targetFrame) {
        if (!activeDocument->frame() || !activeDocument->frame()->canNavigate(*targetFrame))
            return nullptr;

        KURL completedURL = firstFrame->document()->completeURL(urlString);

        if (targetFrame->domWindow()->isInsecureScriptAccess(*callingWindow, completedURL))
            return targetFrame->domWindow();

        if (urlString.isEmpty())
            return targetFrame->domWindow();

        targetFrame->navigate(*activeDocument, completedURL, false, UserGestureStatus::None);
        return targetFrame->domWindow();
    }

    WindowFeatures features(windowFeaturesString);
    DOMWindow* newWindow = createWindow(urlString, frameName, features, *callingWindow, *firstFrame, *frame());
    return features.noopener ? nullptr : newWindow;
}

DEFINE_TRACE(LocalDOMWindow)
{
    visitor->trace(m_frameObserver);
    visitor->trace(m_document);
    visitor->trace(m_properties);
    visitor->trace(m_screen);
    visitor->trace(m_history);
    visitor->trace(m_locationbar);
    visitor->trace(m_menubar);
    visitor->trace(m_personalbar);
    visitor->trace(m_scrollbars);
    visitor->trace(m_statusbar);
    visitor->trace(m_toolbar);
    visitor->trace(m_navigator);
    visitor->trace(m_media);
    visitor->trace(m_customElements);
    visitor->trace(m_applicationCache);
    visitor->trace(m_eventQueue);
    visitor->trace(m_postMessageTimers);
    visitor->trace(m_visualViewport);
    DOMWindow::trace(visitor);
    Supplementable<LocalDOMWindow>::trace(visitor);
    DOMWindowLifecycleNotifier::trace(visitor);
}

LocalFrame* LocalDOMWindow::frame() const
{
    // If the LocalDOMWindow still has a frame reference, that frame must point
    // back to this LocalDOMWindow: otherwise, it's easy to get into a situation
    // where script execution leaks between different LocalDOMWindows.
    if (m_frameObserver->frame())
        ASSERT_WITH_SECURITY_IMPLICATION(m_frameObserver->frame()->domWindow() == this);
    return m_frameObserver->frame();
}

} // namespace blink
