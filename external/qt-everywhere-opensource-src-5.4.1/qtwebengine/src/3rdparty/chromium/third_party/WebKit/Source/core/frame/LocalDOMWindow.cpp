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

#include "config.h"
#include "core/frame/LocalDOMWindow.h"

#include "bindings/v8/ExceptionMessages.h"
#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "bindings/v8/ScriptCallStackFactory.h"
#include "bindings/v8/ScriptController.h"
#include "bindings/v8/SerializedScriptValue.h"
#include "core/css/CSSComputedStyleDeclaration.h"
#include "core/css/CSSRuleList.h"
#include "core/css/DOMWindowCSS.h"
#include "core/css/MediaQueryList.h"
#include "core/css/MediaQueryMatcher.h"
#include "core/css/StyleMedia.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/ContextFeatures.h"
#include "core/dom/DOMImplementation.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/NoEventDispatchAssertion.h"
#include "core/dom/RequestAnimationFrameCallback.h"
#include "core/editing/Editor.h"
#include "core/events/DOMWindowEventQueue.h"
#include "core/events/EventListener.h"
#include "core/events/HashChangeEvent.h"
#include "core/events/MessageEvent.h"
#include "core/events/PageTransitionEvent.h"
#include "core/events/PopStateEvent.h"
#include "core/frame/BarProp.h"
#include "core/frame/Console.h"
#include "core/frame/DOMPoint.h"
#include "core/frame/DOMWindowLifecycleNotifier.h"
#include "core/frame/EventHandlerRegistry.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/History.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Location.h"
#include "core/frame/Navigator.h"
#include "core/frame/Screen.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/inspector/ScriptCallStack.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/loader/MixedContentChecker.h"
#include "core/loader/SinkDocument.h"
#include "core/loader/appcache/ApplicationCache.h"
#include "core/page/BackForwardClient.h"
#include "core/page/Chrome.h"
#include "core/page/ChromeClient.h"
#include "core/page/CreateWindow.h"
#include "core/page/EventHandler.h"
#include "core/page/FrameTree.h"
#include "core/page/Page.h"
#include "core/page/WindowFeatures.h"
#include "core/page/WindowFocusAllowedIndicator.h"
#include "core/page/scrolling/ScrollingCoordinator.h"
#include "core/storage/Storage.h"
#include "core/storage/StorageArea.h"
#include "core/storage/StorageNamespace.h"
#include "core/timing/Performance.h"
#include "platform/PlatformScreen.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/UserGestureIndicator.h"
#include "platform/geometry/FloatRect.h"
#include "platform/graphics/media/MediaPlayer.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "public/platform/Platform.h"
#include "wtf/MainThread.h"
#include "wtf/MathExtras.h"
#include "wtf/text/WTFString.h"
#include <algorithm>

using std::min;
using std::max;

namespace WebCore {

class PostMessageTimer FINAL : public SuspendableTimer {
public:
    PostMessageTimer(LocalDOMWindow& window, PassRefPtr<SerializedScriptValue> message, const String& sourceOrigin, PassRefPtrWillBeRawPtr<LocalDOMWindow> source, PassOwnPtr<MessagePortChannelArray> channels, SecurityOrigin* targetOrigin, PassRefPtrWillBeRawPtr<ScriptCallStack> stackTrace, UserGestureToken* userGestureToken)
        : SuspendableTimer(window.document())
        , m_window(&window)
        , m_message(message)
        , m_origin(sourceOrigin)
        , m_source(source)
        , m_channels(channels)
        , m_targetOrigin(targetOrigin)
        , m_stackTrace(stackTrace)
        , m_userGestureToken(userGestureToken)
    {
    }

    PassRefPtrWillBeRawPtr<MessageEvent> event()
    {
        return MessageEvent::create(m_channels.release(), m_message, m_origin, String(), m_source.get());

    }
    SecurityOrigin* targetOrigin() const { return m_targetOrigin.get(); }
    ScriptCallStack* stackTrace() const { return m_stackTrace.get(); }
    UserGestureToken* userGestureToken() const { return m_userGestureToken.get(); }

private:
    virtual void fired() OVERRIDE
    {
        m_window->postMessageTimerFired(this);
        // This object is deleted now.
    }

    // FIXME: Oilpan: This raw pointer is safe because the PostMessageTimer is
    // owned by the LocalDOMWindow. Ideally PostMessageTimer should be moved to
    // the heap and use Member<LocalDOMWindow>.
    LocalDOMWindow* m_window;
    RefPtr<SerializedScriptValue> m_message;
    String m_origin;
    RefPtrWillBePersistent<LocalDOMWindow> m_source;
    OwnPtr<MessagePortChannelArray> m_channels;
    RefPtr<SecurityOrigin> m_targetOrigin;
    RefPtrWillBePersistent<ScriptCallStack> m_stackTrace;
    RefPtr<UserGestureToken> m_userGestureToken;
};

static void disableSuddenTermination()
{
    blink::Platform::current()->suddenTerminationChanged(false);
}

static void enableSuddenTermination()
{
    blink::Platform::current()->suddenTerminationChanged(true);
}

typedef HashCountedSet<LocalDOMWindow*> DOMWindowSet;

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
        disableSuddenTermination();
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
        enableSuddenTermination();
}

static void removeAllUnloadEventListeners(LocalDOMWindow* domWindow)
{
    DOMWindowSet& set = windowsWithUnloadEventListeners();
    DOMWindowSet::iterator it = set.find(domWindow);
    if (it == set.end())
        return;
    set.removeAll(it);
    if (set.isEmpty())
        enableSuddenTermination();
}

static void addBeforeUnloadEventListener(LocalDOMWindow* domWindow)
{
    DOMWindowSet& set = windowsWithBeforeUnloadEventListeners();
    if (set.isEmpty())
        disableSuddenTermination();
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
        enableSuddenTermination();
}

static void removeAllBeforeUnloadEventListeners(LocalDOMWindow* domWindow)
{
    DOMWindowSet& set = windowsWithBeforeUnloadEventListeners();
    DOMWindowSet::iterator it = set.find(domWindow);
    if (it == set.end())
        return;
    set.removeAll(it);
    if (set.isEmpty())
        enableSuddenTermination();
}

static bool allowsBeforeUnloadListeners(LocalDOMWindow* window)
{
    ASSERT_ARG(window, window);
    LocalFrame* frame = window->frame();
    if (!frame)
        return false;
    return frame->isMainFrame();
}

unsigned LocalDOMWindow::pendingUnloadEventListeners() const
{
    return windowsWithUnloadEventListeners().count(const_cast<LocalDOMWindow*>(this));
}

// This function:
// 1) Validates the pending changes are not changing any value to NaN; in that case keep original value.
// 2) Constrains the window rect to the minimum window size and no bigger than the float rect's dimensions.
// 3) Constrains the window rect to within the top and left boundaries of the available screen rect.
// 4) Constrains the window rect to within the bottom and right boundaries of the available screen rect.
// 5) Translate the window rect coordinates to be within the coordinate space of the screen.
FloatRect LocalDOMWindow::adjustWindowRect(LocalFrame& frame, const FloatRect& pendingChanges)
{
    FrameHost* host = frame.host();
    ASSERT(host);

    FloatRect screen = screenAvailableRect(frame.view());
    FloatRect window = host->chrome().windowRect();

    // Make sure we're in a valid state before adjusting dimensions.
    ASSERT(std::isfinite(screen.x()));
    ASSERT(std::isfinite(screen.y()));
    ASSERT(std::isfinite(screen.width()));
    ASSERT(std::isfinite(screen.height()));
    ASSERT(std::isfinite(window.x()));
    ASSERT(std::isfinite(window.y()));
    ASSERT(std::isfinite(window.width()));
    ASSERT(std::isfinite(window.height()));

    // Update window values if new requested values are not NaN.
    if (!std::isnan(pendingChanges.x()))
        window.setX(pendingChanges.x());
    if (!std::isnan(pendingChanges.y()))
        window.setY(pendingChanges.y());
    if (!std::isnan(pendingChanges.width()))
        window.setWidth(pendingChanges.width());
    if (!std::isnan(pendingChanges.height()))
        window.setHeight(pendingChanges.height());

    FloatSize minimumSize = host->chrome().client().minimumWindowSize();
    // Let size 0 pass through, since that indicates default size, not minimum size.
    if (window.width())
        window.setWidth(min(max(minimumSize.width(), window.width()), screen.width()));
    if (window.height())
        window.setHeight(min(max(minimumSize.height(), window.height()), screen.height()));

    // Constrain the window position within the valid screen area.
    window.setX(max(screen.x(), min(window.x(), screen.maxX() - window.width())));
    window.setY(max(screen.y(), min(window.y(), screen.maxY() - window.height())));

    return window;
}

bool LocalDOMWindow::allowPopUp(LocalFrame& firstFrame)
{
    if (UserGestureIndicator::processingUserGesture())
        return true;

    Settings* settings = firstFrame.settings();
    return settings && settings->javaScriptCanOpenWindowsAutomatically();
}

bool LocalDOMWindow::allowPopUp()
{
    return m_frame && allowPopUp(*m_frame);
}

bool LocalDOMWindow::canShowModalDialogNow(const LocalFrame* frame)
{
    if (!frame)
        return false;
    FrameHost* host = frame->host();
    if (!host)
        return false;
    return host->chrome().canRunModalNow();
}

LocalDOMWindow::LocalDOMWindow(LocalFrame& frame)
    : FrameDestructionObserver(&frame)
    , m_shouldPrintWhenFinishedLoading(false)
#if ASSERT_ENABLED
    , m_hasBeenReset(false)
#endif
{
    ScriptWrappable::init(this);
}

void LocalDOMWindow::clearDocument()
{
    if (!m_document)
        return;

    if (m_document->isActive()) {
        // FIXME: We don't call willRemove here. Why is that OK?
        // This detach() call is also mostly redundant. Most of the calls to
        // this function come via DocumentLoader::createWriterFor, which
        // always detaches the previous Document first. Only XSLTProcessor
        // depends on this detach() call, so it seems like there's some room
        // for cleanup.
        m_document->detach();
    }

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

PassRefPtrWillBeRawPtr<Document> LocalDOMWindow::createDocument(const String& mimeType, const DocumentInit& init, bool forceXHTML)
{
    RefPtrWillBeRawPtr<Document> document = nullptr;
    if (forceXHTML) {
        // This is a hack for XSLTProcessor. See XSLTProcessor::createDocumentFromSource().
        document = Document::create(init);
    } else {
        document = DOMImplementation::createDocument(mimeType, init, init.frame() ? init.frame()->inViewSourceMode() : false);
        if (document->isPluginDocument() && document->isSandboxed(SandboxPlugins))
            document = SinkDocument::create(init);
    }

    return document.release();
}

PassRefPtrWillBeRawPtr<Document> LocalDOMWindow::installNewDocument(const String& mimeType, const DocumentInit& init, bool forceXHTML)
{
    ASSERT(init.frame() == m_frame);

    clearDocument();

    m_document = createDocument(mimeType, init, forceXHTML);
    m_eventQueue = DOMWindowEventQueue::create(m_document.get());
    m_document->attach();

    if (!m_frame) {
        // FIXME: Oilpan: Remove .get() when m_document becomes Member<>.
        return m_document.get();
    }

    m_frame->script().updateDocument();
    m_document->updateViewportDescription();

    if (m_frame->page() && m_frame->view()) {
        if (ScrollingCoordinator* scrollingCoordinator = m_frame->page()->scrollingCoordinator()) {
            scrollingCoordinator->scrollableAreaScrollbarLayerDidChange(m_frame->view(), HorizontalScrollbar);
            scrollingCoordinator->scrollableAreaScrollbarLayerDidChange(m_frame->view(), VerticalScrollbar);
            scrollingCoordinator->scrollableAreaScrollLayerDidChange(m_frame->view());
        }
    }

    m_frame->selection().updateSecureKeyboardEntryIfActive();

    if (m_frame->isMainFrame()) {
        if (m_document->hasTouchEventHandlers())
            m_frame->host()->chrome().client().needTouchEvents(true);
    }

    // FIXME: Oilpan: Remove .get() when m_document becomes Member<>.
    return m_document.get();
}

EventQueue* LocalDOMWindow::eventQueue() const
{
    return m_eventQueue.get();
}

void LocalDOMWindow::enqueueWindowEvent(PassRefPtrWillBeRawPtr<Event> event)
{
    if (!m_eventQueue)
        return;
    event->setTarget(this);
    m_eventQueue->enqueueEvent(event);
}

void LocalDOMWindow::enqueueDocumentEvent(PassRefPtrWillBeRawPtr<Event> event)
{
    if (!m_eventQueue)
        return;
    event->setTarget(m_document.get());
    m_eventQueue->enqueueEvent(event);
}

void LocalDOMWindow::dispatchWindowLoadEvent()
{
    ASSERT(!NoEventDispatchAssertion::isEventDispatchForbidden());
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
    if (!ContextFeatures::pushStateEnabled(document()))
        return;

    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=36202 Popstate event needs to fire asynchronously
    dispatchEvent(PopStateEvent::create(stateObject, &history()));
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
    ASSERT(m_hasBeenReset);
    reset();

#if ENABLE(OILPAN)
    // Oilpan: the frame host and document objects are
    // also garbage collected; cannot notify these
    // when removing event listeners.
    removeAllEventListenersInternal(DoNotBroadcastListenerRemoval);

    // Cleared when detaching document.
    ASSERT(!m_eventQueue);
#else
    removeAllEventListenersInternal(DoBroadcastListenerRemoval);

    ASSERT(m_document->isStopped());
    clearDocument();
#endif
}

const AtomicString& LocalDOMWindow::interfaceName() const
{
    return EventTargetNames::LocalDOMWindow;
}

ExecutionContext* LocalDOMWindow::executionContext() const
{
    return m_document.get();
}

LocalDOMWindow* LocalDOMWindow::toDOMWindow()
{
    return this;
}

PassRefPtrWillBeRawPtr<MediaQueryList> LocalDOMWindow::matchMedia(const String& media)
{
    return document() ? document()->mediaQueryMatcher().matchMedia(media) : nullptr;
}

Page* LocalDOMWindow::page()
{
    return frame() ? frame()->page() : 0;
}

void LocalDOMWindow::frameDestroyed()
{
    FrameDestructionObserver::frameDestroyed();
    reset();
}

void LocalDOMWindow::willDetachFrameHost()
{
    m_frame->host()->eventHandlerRegistry().didRemoveAllEventHandlers(*this);
    InspectorInstrumentation::frameWindowDiscarded(m_frame, this);
}

void LocalDOMWindow::willDestroyDocumentInFrame()
{
    // It is necessary to copy m_properties to a separate vector because the DOMWindowProperties may
    // unregister themselves from the LocalDOMWindow as a result of the call to willDestroyGlobalObjectInFrame.
    Vector<DOMWindowProperty*> properties;
    copyToVector(m_properties, properties);
    for (size_t i = 0; i < properties.size(); ++i)
        properties[i]->willDestroyGlobalObjectInFrame();
}

void LocalDOMWindow::willDetachDocumentFromFrame()
{
    // It is necessary to copy m_properties to a separate vector because the DOMWindowProperties may
    // unregister themselves from the LocalDOMWindow as a result of the call to willDetachGlobalObjectFromFrame.
    Vector<DOMWindowProperty*> properties;
    copyToVector(m_properties, properties);
    for (size_t i = 0; i < properties.size(); ++i)
        properties[i]->willDetachGlobalObjectFromFrame();
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
    willDestroyDocumentInFrame();
    resetDOMWindowProperties();
}

void LocalDOMWindow::resetDOMWindowProperties()
{
    m_properties.clear();

    m_screen = nullptr;
    m_history = nullptr;
    m_locationbar = nullptr;
    m_menubar = nullptr;
    m_personalbar = nullptr;
    m_scrollbars = nullptr;
    m_statusbar = nullptr;
    m_toolbar = nullptr;
    m_console = nullptr;
    m_navigator = nullptr;
    m_performance = nullptr;
    m_location = nullptr;
    m_media = nullptr;
    m_sessionStorage = nullptr;
    m_localStorage = nullptr;
    m_applicationCache = nullptr;
#if ASSERT_ENABLED
    m_hasBeenReset = true;
#endif
}

bool LocalDOMWindow::isCurrentlyDisplayedInFrame() const
{
    return m_frame && m_frame->domWindow() == this && m_frame->host();
}

int LocalDOMWindow::orientation() const
{
    ASSERT(RuntimeEnabledFeatures::orientationEventEnabled());

    if (!m_frame)
        return 0;

    int orientation = screenOrientationAngle(m_frame->view());
    // For backward compatibility, we want to return a value in the range of
    // [-90; 180] instead of [0; 360[ because window.orientation used to behave
    // like that in WebKit (this is a WebKit proprietary API).
    if (orientation == 270)
        return -90;
    return orientation;
}

Screen& LocalDOMWindow::screen() const
{
    if (!m_screen)
        m_screen = Screen::create(m_frame);
    return *m_screen;
}

History& LocalDOMWindow::history() const
{
    if (!m_history)
        m_history = History::create(m_frame);
    return *m_history;
}

BarProp& LocalDOMWindow::locationbar() const
{
    if (!m_locationbar)
        m_locationbar = BarProp::create(m_frame, BarProp::Locationbar);
    return *m_locationbar;
}

BarProp& LocalDOMWindow::menubar() const
{
    if (!m_menubar)
        m_menubar = BarProp::create(m_frame, BarProp::Menubar);
    return *m_menubar;
}

BarProp& LocalDOMWindow::personalbar() const
{
    if (!m_personalbar)
        m_personalbar = BarProp::create(m_frame, BarProp::Personalbar);
    return *m_personalbar;
}

BarProp& LocalDOMWindow::scrollbars() const
{
    if (!m_scrollbars)
        m_scrollbars = BarProp::create(m_frame, BarProp::Scrollbars);
    return *m_scrollbars;
}

BarProp& LocalDOMWindow::statusbar() const
{
    if (!m_statusbar)
        m_statusbar = BarProp::create(m_frame, BarProp::Statusbar);
    return *m_statusbar;
}

BarProp& LocalDOMWindow::toolbar() const
{
    if (!m_toolbar)
        m_toolbar = BarProp::create(m_frame, BarProp::Toolbar);
    return *m_toolbar;
}

Console& LocalDOMWindow::console() const
{
    if (!m_console)
        m_console = Console::create(m_frame);
    return *m_console;
}

FrameConsole* LocalDOMWindow::frameConsole() const
{
    if (!isCurrentlyDisplayedInFrame())
        return 0;
    return &m_frame->console();
}

ApplicationCache* LocalDOMWindow::applicationCache() const
{
    if (!isCurrentlyDisplayedInFrame())
        return 0;
    if (!m_applicationCache)
        m_applicationCache = ApplicationCache::create(m_frame);
    return m_applicationCache.get();
}

Navigator& LocalDOMWindow::navigator() const
{
    if (!m_navigator)
        m_navigator = Navigator::create(m_frame);
    return *m_navigator;
}

Performance& LocalDOMWindow::performance() const
{
    if (!m_performance)
        m_performance = Performance::create(m_frame);
    return *m_performance;
}

Location& LocalDOMWindow::location() const
{
    if (!m_location)
        m_location = Location::create(m_frame);
    return *m_location;
}

Storage* LocalDOMWindow::sessionStorage(ExceptionState& exceptionState) const
{
    if (!isCurrentlyDisplayedInFrame())
        return 0;

    Document* document = this->document();
    if (!document)
        return 0;

    String accessDeniedMessage = "Access is denied for this document.";
    if (!document->securityOrigin()->canAccessLocalStorage()) {
        if (document->isSandboxed(SandboxOrigin))
            exceptionState.throwSecurityError("The document is sandboxed and lacks the 'allow-same-origin' flag.");
        else if (document->url().protocolIs("data"))
            exceptionState.throwSecurityError("Storage is disabled inside 'data:' URLs.");
        else
            exceptionState.throwSecurityError(accessDeniedMessage);
        return 0;
    }

    if (m_sessionStorage) {
        if (!m_sessionStorage->area()->canAccessStorage(m_frame)) {
            exceptionState.throwSecurityError(accessDeniedMessage);
            return 0;
        }
        return m_sessionStorage.get();
    }

    Page* page = document->page();
    if (!page)
        return 0;

    OwnPtrWillBeRawPtr<StorageArea> storageArea = page->sessionStorage()->storageArea(document->securityOrigin());
    if (!storageArea->canAccessStorage(m_frame)) {
        exceptionState.throwSecurityError(accessDeniedMessage);
        return 0;
    }

    m_sessionStorage = Storage::create(m_frame, storageArea.release());
    return m_sessionStorage.get();
}

Storage* LocalDOMWindow::localStorage(ExceptionState& exceptionState) const
{
    if (!isCurrentlyDisplayedInFrame())
        return 0;

    Document* document = this->document();
    if (!document)
        return 0;

    String accessDeniedMessage = "Access is denied for this document.";
    if (!document->securityOrigin()->canAccessLocalStorage()) {
        if (document->isSandboxed(SandboxOrigin))
            exceptionState.throwSecurityError("The document is sandboxed and lacks the 'allow-same-origin' flag.");
        else if (document->url().protocolIs("data"))
            exceptionState.throwSecurityError("Storage is disabled inside 'data:' URLs.");
        else
            exceptionState.throwSecurityError(accessDeniedMessage);
        return 0;
    }

    if (m_localStorage) {
        if (!m_localStorage->area()->canAccessStorage(m_frame)) {
            exceptionState.throwSecurityError(accessDeniedMessage);
            return 0;
        }
        return m_localStorage.get();
    }

    // FIXME: Seems this check should be much higher?
    FrameHost* host = document->frameHost();
    if (!host || !host->settings().localStorageEnabled())
        return 0;

    OwnPtrWillBeRawPtr<StorageArea> storageArea = StorageNamespace::localStorageArea(document->securityOrigin());
    if (!storageArea->canAccessStorage(m_frame)) {
        exceptionState.throwSecurityError(accessDeniedMessage);
        return 0;
    }

    m_localStorage = Storage::create(m_frame, storageArea.release());
    return m_localStorage.get();
}

void LocalDOMWindow::postMessage(PassRefPtr<SerializedScriptValue> message, const MessagePortArray* ports, const String& targetOrigin, LocalDOMWindow* source, ExceptionState& exceptionState)
{
    if (!isCurrentlyDisplayedInFrame())
        return;

    Document* sourceDocument = source->document();

    // Compute the target origin.  We need to do this synchronously in order
    // to generate the SyntaxError exception correctly.
    RefPtr<SecurityOrigin> target;
    if (targetOrigin == "/") {
        if (!sourceDocument)
            return;
        target = sourceDocument->securityOrigin();
    } else if (targetOrigin != "*") {
        target = SecurityOrigin::createFromString(targetOrigin);
        // It doesn't make sense target a postMessage at a unique origin
        // because there's no way to represent a unique origin in a string.
        if (target->isUnique()) {
            exceptionState.throwDOMException(SyntaxError, "Invalid target origin '" + targetOrigin + "' in a call to 'postMessage'.");
            return;
        }
    }

    OwnPtr<MessagePortChannelArray> channels = MessagePort::disentanglePorts(ports, exceptionState);
    if (exceptionState.hadException())
        return;

    // Capture the source of the message.  We need to do this synchronously
    // in order to capture the source of the message correctly.
    if (!sourceDocument)
        return;
    String sourceOrigin = sourceDocument->securityOrigin()->toString();

    if (MixedContentChecker::isMixedContent(sourceDocument->securityOrigin(), document()->url()))
        UseCounter::count(document(), UseCounter::PostMessageFromSecureToInsecure);
    else if (MixedContentChecker::isMixedContent(document()->securityOrigin(), sourceDocument->url()))
        UseCounter::count(document(), UseCounter::PostMessageFromInsecureToSecure);

    // Capture stack trace only when inspector front-end is loaded as it may be time consuming.
    RefPtrWillBeRawPtr<ScriptCallStack> stackTrace = nullptr;
    if (InspectorInstrumentation::consoleAgentEnabled(sourceDocument))
        stackTrace = createScriptCallStack(ScriptCallStack::maxCallStackSizeToCapture, true);

    // Schedule the message.
    OwnPtr<PostMessageTimer> timer = adoptPtr(new PostMessageTimer(*this, message, sourceOrigin, source, channels.release(), target.get(), stackTrace.release(), UserGestureIndicator::currentToken()));
    timer->startOneShot(0, FROM_HERE);
    timer->suspendIfNeeded();
    m_postMessageTimers.add(timer.release());
}

void LocalDOMWindow::postMessageTimerFired(PostMessageTimer* timer)
{
    if (!isCurrentlyDisplayedInFrame()) {
        m_postMessageTimers.remove(timer);
        return;
    }

    RefPtrWillBeRawPtr<MessageEvent> event = timer->event();

    // Give the embedder a chance to intercept this postMessage because this
    // LocalDOMWindow might be a proxy for another in browsers that support
    // postMessage calls across WebKit instances.
    if (m_frame->loader().client()->willCheckAndDispatchMessageEvent(timer->targetOrigin(), event.get())) {
        m_postMessageTimers.remove(timer);
        return;
    }

    UserGestureIndicator gestureIndicator(timer->userGestureToken());

    event->entangleMessagePorts(document());
    dispatchMessageEventWithOriginCheck(timer->targetOrigin(), event, timer->stackTrace());
    m_postMessageTimers.remove(timer);
}

void LocalDOMWindow::dispatchMessageEventWithOriginCheck(SecurityOrigin* intendedTargetOrigin, PassRefPtrWillBeRawPtr<Event> event, PassRefPtrWillBeRawPtr<ScriptCallStack> stackTrace)
{
    if (intendedTargetOrigin) {
        // Check target origin now since the target document may have changed since the timer was scheduled.
        if (!intendedTargetOrigin->isSameSchemeHostPort(document()->securityOrigin())) {
            String message = ExceptionMessages::failedToExecute("postMessage", "DOMWindow", "The target origin provided ('" + intendedTargetOrigin->toString() + "') does not match the recipient window's origin ('" + document()->securityOrigin()->toString() + "').");
            frameConsole()->addMessage(SecurityMessageSource, ErrorMessageLevel, message, stackTrace);
            return;
        }
    }

    dispatchEvent(event);
}

DOMSelection* LocalDOMWindow::getSelection()
{
    if (!isCurrentlyDisplayedInFrame() || !m_frame)
        return 0;

    return m_frame->document()->getSelection();
}

Element* LocalDOMWindow::frameElement() const
{
    if (!m_frame)
        return 0;

    // The bindings security check should ensure we're same origin...
    ASSERT(!m_frame->owner() || m_frame->owner()->isLocal());
    return m_frame->deprecatedLocalOwner();
}

void LocalDOMWindow::focus(ExecutionContext* context)
{
    if (!m_frame)
        return;

    FrameHost* host = m_frame->host();
    if (!host)
        return;

    bool allowFocus = WindowFocusAllowedIndicator::windowFocusAllowed();
    if (context) {
        ASSERT(isMainThread());
        Document* activeDocument = toDocument(context);
        if (opener() && opener() != this && activeDocument->domWindow() == opener())
            allowFocus = true;
    }

    // If we're a top level window, bring the window to the front.
    if (m_frame->isMainFrame() && allowFocus)
        host->chrome().focus();

    if (!m_frame)
        return;

    m_frame->eventHandler().focusDocumentView();
}

void LocalDOMWindow::blur()
{
}

void LocalDOMWindow::close(ExecutionContext* context)
{
    if (!m_frame || !m_frame->isMainFrame())
        return;

    Page* page = m_frame->page();
    if (!page)
        return;

    if (context) {
        ASSERT(isMainThread());
        Document* activeDocument = toDocument(context);
        if (!activeDocument)
            return;

        if (!activeDocument->canNavigate(*m_frame))
            return;
    }

    Settings* settings = m_frame->settings();
    bool allowScriptsToCloseWindows = settings && settings->allowScriptsToCloseWindows();

    if (!(page->openedByDOM() || page->backForward().backForwardListCount() <= 1 || allowScriptsToCloseWindows)) {
        frameConsole()->addMessage(JSMessageSource, WarningMessageLevel, "Scripts may close only the windows that were opened by it.");
        return;
    }

    if (!m_frame->loader().shouldClose())
        return;

    page->chrome().closeWindowSoon();
}

void LocalDOMWindow::print()
{
    if (!m_frame)
        return;

    FrameHost* host = m_frame->host();
    if (!host)
        return;

    if (m_frame->loader().state() != FrameStateComplete) {
        m_shouldPrintWhenFinishedLoading = true;
        return;
    }
    m_shouldPrintWhenFinishedLoading = false;
    host->chrome().print(m_frame);
}

void LocalDOMWindow::stop()
{
    if (!m_frame)
        return;
    m_frame->loader().stopAllLoaders();
}

void LocalDOMWindow::alert(const String& message)
{
    if (!m_frame)
        return;

    m_frame->document()->updateRenderTreeIfNeeded();

    FrameHost* host = m_frame->host();
    if (!host)
        return;

    host->chrome().runJavaScriptAlert(m_frame, message);
}

bool LocalDOMWindow::confirm(const String& message)
{
    if (!m_frame)
        return false;

    m_frame->document()->updateRenderTreeIfNeeded();

    FrameHost* host = m_frame->host();
    if (!host)
        return false;

    return host->chrome().runJavaScriptConfirm(m_frame, message);
}

String LocalDOMWindow::prompt(const String& message, const String& defaultValue)
{
    if (!m_frame)
        return String();

    m_frame->document()->updateRenderTreeIfNeeded();

    FrameHost* host = m_frame->host();
    if (!host)
        return String();

    String returnValue;
    if (host->chrome().runJavaScriptPrompt(m_frame, message, defaultValue, returnValue))
        return returnValue;

    return String();
}

bool LocalDOMWindow::find(const String& string, bool caseSensitive, bool backwards, bool wrap, bool /*wholeWord*/, bool /*searchInFrames*/, bool /*showDialog*/) const
{
    if (!isCurrentlyDisplayedInFrame())
        return false;

    // |m_frame| can be destructed during |Editor::findString()| via
    // |Document::updateLayou()|, e.g. event handler removes a frame.
    RefPtr<LocalFrame> protectFrame(m_frame);

    // FIXME (13016): Support wholeWord, searchInFrames and showDialog
    return m_frame->editor().findString(string, !backwards, caseSensitive, wrap, false);
}

bool LocalDOMWindow::offscreenBuffering() const
{
    return true;
}

int LocalDOMWindow::outerHeight() const
{
    if (!m_frame)
        return 0;

    FrameHost* host = m_frame->host();
    if (!host)
        return 0;

    if (host->settings().reportScreenSizeInPhysicalPixelsQuirk())
        return lroundf(host->chrome().windowRect().height() * host->deviceScaleFactor());
    return static_cast<int>(host->chrome().windowRect().height());
}

int LocalDOMWindow::outerWidth() const
{
    if (!m_frame)
        return 0;

    FrameHost* host = m_frame->host();
    if (!host)
        return 0;

    if (host->settings().reportScreenSizeInPhysicalPixelsQuirk())
        return lroundf(host->chrome().windowRect().width() * host->deviceScaleFactor());
    return static_cast<int>(host->chrome().windowRect().width());
}

int LocalDOMWindow::innerHeight() const
{
    if (!m_frame)
        return 0;

    FrameView* view = m_frame->view();
    if (!view)
        return 0;

    // FIXME: This is potentially too much work. We really only need to know the dimensions of the parent frame's renderer.
    if (Frame* parent = m_frame->tree().parent()) {
        if (parent && parent->isLocalFrame())
            toLocalFrame(parent)->document()->updateLayoutIgnorePendingStylesheets();
    }

    return adjustForAbsoluteZoom(view->visibleContentRect(IncludeScrollbars).height(), m_frame->pageZoomFactor());
}

int LocalDOMWindow::innerWidth() const
{
    if (!m_frame)
        return 0;

    FrameView* view = m_frame->view();
    if (!view)
        return 0;

    // FIXME: This is potentially too much work. We really only need to know the dimensions of the parent frame's renderer.
    if (Frame* parent = m_frame->tree().parent()) {
        if (parent && parent->isLocalFrame())
            toLocalFrame(parent)->document()->updateLayoutIgnorePendingStylesheets();
    }

    return adjustForAbsoluteZoom(view->visibleContentRect(IncludeScrollbars).width(), m_frame->pageZoomFactor());
}

int LocalDOMWindow::screenX() const
{
    if (!m_frame)
        return 0;

    FrameHost* host = m_frame->host();
    if (!host)
        return 0;

    if (host->settings().reportScreenSizeInPhysicalPixelsQuirk())
        return lroundf(host->chrome().windowRect().x() * host->deviceScaleFactor());
    return static_cast<int>(host->chrome().windowRect().x());
}

int LocalDOMWindow::screenY() const
{
    if (!m_frame)
        return 0;

    FrameHost* host = m_frame->host();
    if (!host)
        return 0;

    if (host->settings().reportScreenSizeInPhysicalPixelsQuirk())
        return lroundf(host->chrome().windowRect().y() * host->deviceScaleFactor());
    return static_cast<int>(host->chrome().windowRect().y());
}

int LocalDOMWindow::scrollX() const
{
    if (!m_frame)
        return 0;

    FrameView* view = m_frame->view();
    if (!view)
        return 0;

    m_frame->document()->updateLayoutIgnorePendingStylesheets();

    return adjustForAbsoluteZoom(view->scrollX(), m_frame->pageZoomFactor());
}

int LocalDOMWindow::scrollY() const
{
    if (!m_frame)
        return 0;

    FrameView* view = m_frame->view();
    if (!view)
        return 0;

    m_frame->document()->updateLayoutIgnorePendingStylesheets();

    return adjustForAbsoluteZoom(view->scrollY(), m_frame->pageZoomFactor());
}

bool LocalDOMWindow::closed() const
{
    return !m_frame;
}

unsigned LocalDOMWindow::length() const
{
    if (!isCurrentlyDisplayedInFrame())
        return 0;

    return m_frame->tree().scopedChildCount();
}

const AtomicString& LocalDOMWindow::name() const
{
    if (!isCurrentlyDisplayedInFrame())
        return nullAtom;

    return m_frame->tree().name();
}

void LocalDOMWindow::setName(const AtomicString& name)
{
    if (!isCurrentlyDisplayedInFrame())
        return;

    m_frame->tree().setName(name);
    ASSERT(m_frame->loader().client());
    m_frame->loader().client()->didChangeName(name);
}

void LocalDOMWindow::setStatus(const String& string)
{
    m_status = string;

    if (!m_frame)
        return;

    FrameHost* host = m_frame->host();
    if (!host)
        return;

    ASSERT(m_frame->document()); // Client calls shouldn't be made when the frame is in inconsistent state.
    host->chrome().setStatusbarText(m_frame, m_status);
}

void LocalDOMWindow::setDefaultStatus(const String& string)
{
    m_defaultStatus = string;

    if (!m_frame)
        return;

    FrameHost* host = m_frame->host();
    if (!host)
        return;

    ASSERT(m_frame->document()); // Client calls shouldn't be made when the frame is in inconsistent state.
    host->chrome().setStatusbarText(m_frame, m_defaultStatus);
}

LocalDOMWindow* LocalDOMWindow::self() const
{
    if (!m_frame)
        return 0;

    return m_frame->domWindow();
}

LocalDOMWindow* LocalDOMWindow::opener() const
{
    if (!m_frame)
        return 0;

    LocalFrame* opener = m_frame->loader().opener();
    if (!opener)
        return 0;

    return opener->domWindow();
}

LocalDOMWindow* LocalDOMWindow::parent() const
{
    if (!m_frame)
        return 0;

    Frame* parent = m_frame->tree().parent();
    if (parent)
        return parent->domWindow();

    return m_frame->domWindow();
}

LocalDOMWindow* LocalDOMWindow::top() const
{
    if (!m_frame)
        return 0;

    return m_frame->tree().top()->domWindow();
}

Document* LocalDOMWindow::document() const
{
    return m_document.get();
}

StyleMedia& LocalDOMWindow::styleMedia() const
{
    if (!m_media)
        m_media = StyleMedia::create(m_frame);
    return *m_media;
}

PassRefPtrWillBeRawPtr<CSSStyleDeclaration> LocalDOMWindow::getComputedStyle(Element* elt, const String& pseudoElt) const
{
    if (!elt)
        return nullptr;

    return CSSComputedStyleDeclaration::create(elt, false, pseudoElt);
}

PassRefPtrWillBeRawPtr<CSSRuleList> LocalDOMWindow::getMatchedCSSRules(Element* element, const String& pseudoElement) const
{
    if (!element)
        return nullptr;

    if (!isCurrentlyDisplayedInFrame())
        return nullptr;

    unsigned colonStart = pseudoElement[0] == ':' ? (pseudoElement[1] == ':' ? 2 : 1) : 0;
    CSSSelector::PseudoType pseudoType = CSSSelector::parsePseudoType(AtomicString(pseudoElement.substring(colonStart)));
    if (pseudoType == CSSSelector::PseudoUnknown && !pseudoElement.isEmpty())
        return nullptr;

    unsigned rulesToInclude = StyleResolver::AuthorCSSRules;
    PseudoId pseudoId = CSSSelector::pseudoId(pseudoType);
    return m_frame->document()->ensureStyleResolver().pseudoCSSRulesForElement(element, pseudoId, rulesToInclude);
}

PassRefPtrWillBeRawPtr<DOMPoint> LocalDOMWindow::webkitConvertPointFromNodeToPage(Node* node, const DOMPoint* p) const
{
    if (!node || !p)
        return nullptr;

    if (!document())
        return nullptr;

    document()->updateLayoutIgnorePendingStylesheets();

    FloatPoint pagePoint(p->x(), p->y());
    pagePoint = node->convertToPage(pagePoint);
    return DOMPoint::create(pagePoint.x(), pagePoint.y());
}

PassRefPtrWillBeRawPtr<DOMPoint> LocalDOMWindow::webkitConvertPointFromPageToNode(Node* node, const DOMPoint* p) const
{
    if (!node || !p)
        return nullptr;

    if (!document())
        return nullptr;

    document()->updateLayoutIgnorePendingStylesheets();

    FloatPoint nodePoint(p->x(), p->y());
    nodePoint = node->convertFromPage(nodePoint);
    return DOMPoint::create(nodePoint.x(), nodePoint.y());
}

double LocalDOMWindow::devicePixelRatio() const
{
    if (!m_frame)
        return 0.0;

    return m_frame->devicePixelRatio();
}

static bool scrollBehaviorFromScrollOptions(const Dictionary& scrollOptions, ScrollBehavior& scrollBehavior, ExceptionState& exceptionState)
{
    String scrollBehaviorString;
    if (!scrollOptions.get("behavior", scrollBehaviorString)) {
        scrollBehavior = ScrollBehaviorAuto;
        return true;
    }

    if (ScrollableArea::scrollBehaviorFromString(scrollBehaviorString, scrollBehavior))
        return true;

    exceptionState.throwTypeError("The ScrollBehavior provided is invalid.");
    return false;
}

void LocalDOMWindow::scrollBy(int x, int y) const
{
    if (!isCurrentlyDisplayedInFrame())
        return;

    document()->updateLayoutIgnorePendingStylesheets();

    FrameView* view = m_frame->view();
    if (!view)
        return;

    IntSize scaledOffset(x * m_frame->pageZoomFactor(), y * m_frame->pageZoomFactor());
    // FIXME: Use scrollBehavior to decide whether to scroll smoothly or instantly.
    view->scrollBy(scaledOffset);
}

void LocalDOMWindow::scrollBy(int x, int y, const Dictionary& scrollOptions, ExceptionState &exceptionState) const
{
    ScrollBehavior scrollBehavior = ScrollBehaviorAuto;
    if (!scrollBehaviorFromScrollOptions(scrollOptions, scrollBehavior, exceptionState))
        return;
    scrollBy(x, y);
}

void LocalDOMWindow::scrollTo(int x, int y) const
{
    if (!isCurrentlyDisplayedInFrame())
        return;

    document()->updateLayoutIgnorePendingStylesheets();

    RefPtr<FrameView> view = m_frame->view();
    if (!view)
        return;

    IntPoint layoutPos(x * m_frame->pageZoomFactor(), y * m_frame->pageZoomFactor());
    // FIXME: Use scrollBehavior to decide whether to scroll smoothly or instantly.
    view->setScrollPosition(layoutPos);
}

void LocalDOMWindow::scrollTo(int x, int y, const Dictionary& scrollOptions, ExceptionState& exceptionState) const
{
    ScrollBehavior scrollBehavior = ScrollBehaviorAuto;
    if (!scrollBehaviorFromScrollOptions(scrollOptions, scrollBehavior, exceptionState))
        return;
    scrollTo(x, y);
}

void LocalDOMWindow::moveBy(float x, float y) const
{
    if (!m_frame || !m_frame->isMainFrame())
        return;

    FrameHost* host = m_frame->host();
    if (!host)
        return;

    FloatRect windowRect = host->chrome().windowRect();
    windowRect.move(x, y);
    // Security check (the spec talks about UniversalBrowserWrite to disable this check...)
    host->chrome().setWindowRect(adjustWindowRect(*m_frame, windowRect));
}

void LocalDOMWindow::moveTo(float x, float y) const
{
    if (!m_frame || !m_frame->isMainFrame())
        return;

    FrameHost* host = m_frame->host();
    if (!host)
        return;

    FloatRect windowRect = host->chrome().windowRect();
    windowRect.setLocation(FloatPoint(x, y));
    // Security check (the spec talks about UniversalBrowserWrite to disable this check...)
    host->chrome().setWindowRect(adjustWindowRect(*m_frame, windowRect));
}

void LocalDOMWindow::resizeBy(float x, float y) const
{
    if (!m_frame || !m_frame->isMainFrame())
        return;

    FrameHost* host = m_frame->host();
    if (!host)
        return;

    FloatRect fr = host->chrome().windowRect();
    FloatSize dest = fr.size() + FloatSize(x, y);
    FloatRect update(fr.location(), dest);
    host->chrome().setWindowRect(adjustWindowRect(*m_frame, update));
}

void LocalDOMWindow::resizeTo(float width, float height) const
{
    if (!m_frame || !m_frame->isMainFrame())
        return;

    FrameHost* host = m_frame->host();
    if (!host)
        return;

    FloatRect fr = host->chrome().windowRect();
    FloatSize dest = FloatSize(width, height);
    FloatRect update(fr.location(), dest);
    host->chrome().setWindowRect(adjustWindowRect(*m_frame, update));
}

int LocalDOMWindow::requestAnimationFrame(PassOwnPtr<RequestAnimationFrameCallback> callback)
{
    callback->m_useLegacyTimeBase = false;
    if (Document* d = document())
        return d->requestAnimationFrame(callback);
    return 0;
}

int LocalDOMWindow::webkitRequestAnimationFrame(PassOwnPtr<RequestAnimationFrameCallback> callback)
{
    callback->m_useLegacyTimeBase = true;
    if (Document* d = document())
        return d->requestAnimationFrame(callback);
    return 0;
}

void LocalDOMWindow::cancelAnimationFrame(int id)
{
    if (Document* d = document())
        d->cancelAnimationFrame(id);
}

DOMWindowCSS& LocalDOMWindow::css() const
{
    if (!m_css)
        m_css = DOMWindowCSS::create();
    return *m_css;
}

static void didAddStorageEventListener(LocalDOMWindow* window)
{
    // Creating these WebCore::Storage objects informs the system that we'd like to receive
    // notifications about storage events that might be triggered in other processes. Rather
    // than subscribe to these notifications explicitly, we subscribe to them implicitly to
    // simplify the work done by the system.
    window->localStorage(IGNORE_EXCEPTION);
    window->sessionStorage(IGNORE_EXCEPTION);
}

bool LocalDOMWindow::addEventListener(const AtomicString& eventType, PassRefPtr<EventListener> listener, bool useCapture)
{
    if (!EventTarget::addEventListener(eventType, listener, useCapture))
        return false;

    if (m_frame && m_frame->host())
        m_frame->host()->eventHandlerRegistry().didAddEventHandler(*this, eventType);

    if (Document* document = this->document()) {
        document->addListenerTypeIfNeeded(eventType);
        if (isTouchEventType(eventType))
            document->didAddTouchEventHandler(document);
        else if (eventType == EventTypeNames::storage)
            didAddStorageEventListener(this);
    }

    lifecycleNotifier().notifyAddEventListener(this, eventType);

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

    return true;
}

bool LocalDOMWindow::removeEventListener(const AtomicString& eventType, EventListener* listener, bool useCapture)
{
    if (!EventTarget::removeEventListener(eventType, listener, useCapture))
        return false;

    if (m_frame && m_frame->host())
        m_frame->host()->eventHandlerRegistry().didRemoveEventHandler(*this, eventType);

    if (Document* document = this->document()) {
        if (isTouchEventType(eventType))
            document->didRemoveTouchEventHandler(document);
    }

    lifecycleNotifier().notifyRemoveEventListener(this, eventType);

    if (eventType == EventTypeNames::unload) {
        removeUnloadEventListener(this);
    } else if (eventType == EventTypeNames::beforeunload && allowsBeforeUnloadListeners(this)) {
        removeBeforeUnloadEventListener(this);
    }

    return true;
}

void LocalDOMWindow::dispatchLoadEvent()
{
    RefPtrWillBeRawPtr<Event> loadEvent(Event::create(EventTypeNames::load));
    if (m_frame && m_frame->loader().documentLoader() && !m_frame->loader().documentLoader()->timing()->loadEventStart()) {
        // The DocumentLoader (and thus its DocumentLoadTiming) might get destroyed while dispatching
        // the event, so protect it to prevent writing the end time into freed memory.
        RefPtr<DocumentLoader> documentLoader = m_frame->loader().documentLoader();
        DocumentLoadTiming* timing = documentLoader->timing();
        timing->markLoadEventStart();
        dispatchEvent(loadEvent, document());
        timing->markLoadEventEnd();
    } else
        dispatchEvent(loadEvent, document());

    // For load events, send a separate load event to the enclosing frame only.
    // This is a DOM extension and is independent of bubbling/capturing rules of
    // the DOM.
    FrameOwner* owner = m_frame ? m_frame->owner() : 0;
    if (owner)
        owner->dispatchLoad();

    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "MarkLoad", "data", InspectorMarkLoadEvent::data(frame()));
    // FIXME(361045): remove InspectorInstrumentation calls once DevTools Timeline migrates to tracing.
    InspectorInstrumentation::loadEventFired(frame());
}

bool LocalDOMWindow::dispatchEvent(PassRefPtrWillBeRawPtr<Event> prpEvent, PassRefPtrWillBeRawPtr<EventTarget> prpTarget)
{
    ASSERT(!NoEventDispatchAssertion::isEventDispatchForbidden());

    RefPtrWillBeRawPtr<EventTarget> protect(this);
    RefPtrWillBeRawPtr<Event> event = prpEvent;

    event->setTarget(prpTarget ? prpTarget : this);
    event->setCurrentTarget(this);
    event->setEventPhase(Event::AT_TARGET);

    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "EventDispatch", "type", event->type().ascii());
    // FIXME(361045): remove InspectorInstrumentation calls once DevTools Timeline migrates to tracing.
    InspectorInstrumentationCookie cookie = InspectorInstrumentation::willDispatchEventOnWindow(frame(), *event, this);

    bool result = fireEventListeners(event.get());

    InspectorInstrumentation::didDispatchEventOnWindow(cookie);

    return result;
}

void LocalDOMWindow::removeAllEventListenersInternal(BroadcastListenerRemoval mode)
{
    EventTarget::removeAllEventListeners();

    lifecycleNotifier().notifyRemoveAllEventListeners(this);

    if (mode == DoBroadcastListenerRemoval) {
        if (m_frame && m_frame->host())
            m_frame->host()->eventHandlerRegistry().didRemoveAllEventHandlers(*this);

        if (Document* document = this->document())
            document->didClearTouchEventHandlers(document);
    }

    removeAllUnloadEventListeners(this);
    removeAllBeforeUnloadEventListeners(this);
}

void LocalDOMWindow::removeAllEventListeners()
{
    removeAllEventListenersInternal(DoBroadcastListenerRemoval);
}

void LocalDOMWindow::finishedLoading()
{
    if (m_shouldPrintWhenFinishedLoading) {
        m_shouldPrintWhenFinishedLoading = false;
        print();
    }
}

void LocalDOMWindow::setLocation(const String& urlString, LocalDOMWindow* callingWindow, LocalDOMWindow* enteredWindow, SetLocationLocking locking)
{
    if (!isCurrentlyDisplayedInFrame())
        return;

    Document* activeDocument = callingWindow->document();
    if (!activeDocument)
        return;

    ASSERT(m_frame);
    if (!activeDocument->canNavigate(*m_frame))
        return;

    LocalFrame* firstFrame = enteredWindow->frame();
    if (!firstFrame)
        return;

    KURL completedURL = firstFrame->document()->completeURL(urlString);
    if (completedURL.isNull())
        return;

    if (isInsecureScriptAccess(*callingWindow, completedURL))
        return;

    // We want a new history item if we are processing a user gesture.
    m_frame->navigationScheduler().scheduleLocationChange(activeDocument,
        // FIXME: What if activeDocument()->frame() is 0?
        completedURL, Referrer(activeDocument->outgoingReferrer(), activeDocument->referrerPolicy()),
        locking != LockHistoryBasedOnGestureState);
}

void LocalDOMWindow::printErrorMessage(const String& message)
{
    if (message.isEmpty())
        return;

    frameConsole()->addMessage(JSMessageSource, ErrorMessageLevel, message);
}

// FIXME: Once we're throwing exceptions for cross-origin access violations, we will always sanitize the target
// frame details, so we can safely combine 'crossDomainAccessErrorMessage' with this method after considering
// exactly which details may be exposed to JavaScript.
//
// http://crbug.com/17325
String LocalDOMWindow::sanitizedCrossDomainAccessErrorMessage(LocalDOMWindow* callingWindow)
{
    if (!callingWindow || !callingWindow->document())
        return String();

    const KURL& callingWindowURL = callingWindow->document()->url();
    if (callingWindowURL.isNull())
        return String();

    ASSERT(!callingWindow->document()->securityOrigin()->canAccess(document()->securityOrigin()));

    SecurityOrigin* activeOrigin = callingWindow->document()->securityOrigin();
    String message = "Blocked a frame with origin \"" + activeOrigin->toString() + "\" from accessing a cross-origin frame.";

    // FIXME: Evaluate which details from 'crossDomainAccessErrorMessage' may safely be reported to JavaScript.

    return message;
}

String LocalDOMWindow::crossDomainAccessErrorMessage(LocalDOMWindow* callingWindow)
{
    if (!callingWindow || !callingWindow->document())
        return String();

    const KURL& callingWindowURL = callingWindow->document()->url();
    if (callingWindowURL.isNull())
        return String();

    ASSERT(!callingWindow->document()->securityOrigin()->canAccess(document()->securityOrigin()));

    // FIXME: This message, and other console messages, have extra newlines. Should remove them.
    SecurityOrigin* activeOrigin = callingWindow->document()->securityOrigin();
    SecurityOrigin* targetOrigin = document()->securityOrigin();
    String message = "Blocked a frame with origin \"" + activeOrigin->toString() + "\" from accessing a frame with origin \"" + targetOrigin->toString() + "\". ";

    // Sandbox errors: Use the origin of the frames' location, rather than their actual origin (since we know that at least one will be "null").
    KURL activeURL = callingWindow->document()->url();
    KURL targetURL = document()->url();
    if (document()->isSandboxed(SandboxOrigin) || callingWindow->document()->isSandboxed(SandboxOrigin)) {
        message = "Blocked a frame at \"" + SecurityOrigin::create(activeURL)->toString() + "\" from accessing a frame at \"" + SecurityOrigin::create(targetURL)->toString() + "\". ";
        if (document()->isSandboxed(SandboxOrigin) && callingWindow->document()->isSandboxed(SandboxOrigin))
            return "Sandbox access violation: " + message + " Both frames are sandboxed and lack the \"allow-same-origin\" flag.";
        if (document()->isSandboxed(SandboxOrigin))
            return "Sandbox access violation: " + message + " The frame being accessed is sandboxed and lacks the \"allow-same-origin\" flag.";
        return "Sandbox access violation: " + message + " The frame requesting access is sandboxed and lacks the \"allow-same-origin\" flag.";
    }

    // Protocol errors: Use the URL's protocol rather than the origin's protocol so that we get a useful message for non-heirarchal URLs like 'data:'.
    if (targetOrigin->protocol() != activeOrigin->protocol())
        return message + " The frame requesting access has a protocol of \"" + activeURL.protocol() + "\", the frame being accessed has a protocol of \"" + targetURL.protocol() + "\". Protocols must match.\n";

    // 'document.domain' errors.
    if (targetOrigin->domainWasSetInDOM() && activeOrigin->domainWasSetInDOM())
        return message + "The frame requesting access set \"document.domain\" to \"" + activeOrigin->domain() + "\", the frame being accessed set it to \"" + targetOrigin->domain() + "\". Both must set \"document.domain\" to the same value to allow access.";
    if (activeOrigin->domainWasSetInDOM())
        return message + "The frame requesting access set \"document.domain\" to \"" + activeOrigin->domain() + "\", but the frame being accessed did not. Both must set \"document.domain\" to the same value to allow access.";
    if (targetOrigin->domainWasSetInDOM())
        return message + "The frame being accessed set \"document.domain\" to \"" + targetOrigin->domain() + "\", but the frame requesting access did not. Both must set \"document.domain\" to the same value to allow access.";

    // Default.
    return message + "Protocols, domains, and ports must match.";
}

bool LocalDOMWindow::isInsecureScriptAccess(LocalDOMWindow& callingWindow, const String& urlString)
{
    if (!protocolIsJavaScript(urlString))
        return false;

    // If this LocalDOMWindow isn't currently active in the LocalFrame, then there's no
    // way we should allow the access.
    // FIXME: Remove this check if we're able to disconnect LocalDOMWindow from
    // LocalFrame on navigation: https://bugs.webkit.org/show_bug.cgi?id=62054
    if (isCurrentlyDisplayedInFrame()) {
        // FIXME: Is there some way to eliminate the need for a separate "callingWindow == this" check?
        if (&callingWindow == this)
            return false;

        // FIXME: The name canAccess seems to be a roundabout way to ask "can execute script".
        // Can we name the SecurityOrigin function better to make this more clear?
        if (callingWindow.document()->securityOrigin()->canAccess(document()->securityOrigin()))
            return false;
    }

    printErrorMessage(crossDomainAccessErrorMessage(&callingWindow));
    return true;
}

PassRefPtrWillBeRawPtr<LocalDOMWindow> LocalDOMWindow::open(const String& urlString, const AtomicString& frameName, const String& windowFeaturesString,
    LocalDOMWindow* callingWindow, LocalDOMWindow* enteredWindow)
{
    if (!isCurrentlyDisplayedInFrame())
        return nullptr;
    Document* activeDocument = callingWindow->document();
    if (!activeDocument)
        return nullptr;
    LocalFrame* firstFrame = enteredWindow->frame();
    if (!firstFrame)
        return nullptr;

    if (!enteredWindow->allowPopUp()) {
        // Because FrameTree::find() returns true for empty strings, we must check for empty frame names.
        // Otherwise, illegitimate window.open() calls with no name will pass right through the popup blocker.
        if (frameName.isEmpty() || !m_frame->tree().find(frameName))
            return nullptr;
    }

    // Get the target frame for the special cases of _top and _parent.
    // In those cases, we schedule a location change right now and return early.
    Frame* targetFrame = 0;
    if (frameName == "_top")
        targetFrame = m_frame->tree().top();
    else if (frameName == "_parent") {
        if (Frame* parent = m_frame->tree().parent())
            targetFrame = parent;
        else
            targetFrame = m_frame;
    }
    // FIXME: Navigating RemoteFrames is not yet supported.
    if (targetFrame && targetFrame->isLocalFrame()) {
        if (!activeDocument->canNavigate(*targetFrame))
            return nullptr;

        KURL completedURL = firstFrame->document()->completeURL(urlString);

        if (targetFrame->domWindow()->isInsecureScriptAccess(*callingWindow, completedURL))
            return targetFrame->domWindow();

        if (urlString.isEmpty())
            return targetFrame->domWindow();

        // For whatever reason, Firefox uses the first window rather than the active window to
        // determine the outgoing referrer. We replicate that behavior here.
        toLocalFrame(targetFrame)->navigationScheduler().scheduleLocationChange(
            activeDocument,
            completedURL,
            Referrer(firstFrame->document()->outgoingReferrer(), firstFrame->document()->referrerPolicy()),
            false);
        return targetFrame->domWindow();
    }

    WindowFeatures windowFeatures(windowFeaturesString);
    LocalFrame* result = createWindow(urlString, frameName, windowFeatures, *callingWindow, *firstFrame, *m_frame);
    return result ? result->domWindow() : 0;
}

void LocalDOMWindow::showModalDialog(const String& urlString, const String& dialogFeaturesString,
    LocalDOMWindow* callingWindow, LocalDOMWindow* enteredWindow, PrepareDialogFunction function, void* functionContext)
{
    if (!isCurrentlyDisplayedInFrame())
        return;
    LocalFrame* activeFrame = callingWindow->frame();
    if (!activeFrame)
        return;
    LocalFrame* firstFrame = enteredWindow->frame();
    if (!firstFrame)
        return;

    if (!canShowModalDialogNow(m_frame) || !enteredWindow->allowPopUp())
        return;

    UseCounter::countDeprecation(this, UseCounter::ShowModalDialog);

    WindowFeatures windowFeatures(dialogFeaturesString, screenAvailableRect(m_frame->view()));
    LocalFrame* dialogFrame = createWindow(urlString, emptyAtom, windowFeatures,
        *callingWindow, *firstFrame, *m_frame, function, functionContext);
    if (!dialogFrame)
        return;
    UserGestureIndicatorDisabler disabler;
    dialogFrame->host()->chrome().runModal();
}

LocalDOMWindow* LocalDOMWindow::anonymousIndexedGetter(uint32_t index)
{
    LocalFrame* frame = this->frame();
    if (!frame)
        return 0;

    Frame* child = frame->tree().scopedChild(index);
    if (child)
        return child->domWindow();

    return 0;
}

DOMWindowLifecycleNotifier& LocalDOMWindow::lifecycleNotifier()
{
    return static_cast<DOMWindowLifecycleNotifier&>(LifecycleContext<LocalDOMWindow>::lifecycleNotifier());
}

PassOwnPtr<LifecycleNotifier<LocalDOMWindow> > LocalDOMWindow::createLifecycleNotifier()
{
    return DOMWindowLifecycleNotifier::create(this);
}

void LocalDOMWindow::trace(Visitor* visitor)
{
    visitor->trace(m_document);
    visitor->trace(m_screen);
    visitor->trace(m_history);
    visitor->trace(m_locationbar);
    visitor->trace(m_menubar);
    visitor->trace(m_personalbar);
    visitor->trace(m_scrollbars);
    visitor->trace(m_statusbar);
    visitor->trace(m_toolbar);
    visitor->trace(m_console);
    visitor->trace(m_navigator);
    visitor->trace(m_location);
    visitor->trace(m_media);
    visitor->trace(m_sessionStorage);
    visitor->trace(m_localStorage);
    visitor->trace(m_applicationCache);
    visitor->trace(m_performance);
    visitor->trace(m_css);
    visitor->trace(m_eventQueue);
    WillBeHeapSupplementable<LocalDOMWindow>::trace(visitor);
    EventTargetWithInlineData::trace(visitor);
}

} // namespace WebCore
