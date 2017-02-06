/*
 * Copyright (C) 2006, 2007, 2009, 2010 Apple Inc. All rights reserved.
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

#ifndef LocalDOMWindow_h
#define LocalDOMWindow_h

#include "core/CoreExport.h"
#include "core/events/EventTarget.h"
#include "core/frame/DOMWindow.h"
#include "core/frame/DOMWindowLifecycleNotifier.h"
#include "core/frame/DOMWindowLifecycleObserver.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameLifecycleObserver.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"
#include "wtf/Assertions.h"
#include "wtf/Forward.h"
#include <memory>

namespace blink {

class CustomElementsRegistry;
class DOMWindowEventQueue;
class DOMWindowProperty;
class DocumentInit;
class EventListener;
class EventQueue;
class ExceptionState;
class FrameConsole;
class IntRect;
class MessageEvent;
class Page;
class PostMessageTimer;
class SecurityOrigin;
class SourceLocation;
class DOMVisualViewport;

enum PageshowEventPersistence {
    PageshowEventNotPersisted = 0,
    PageshowEventPersisted = 1
};

// Note: if you're thinking of returning something DOM-related by reference,
// please ping dcheng@chromium.org first. You probably don't want to do that.
class CORE_EXPORT LocalDOMWindow final : public DOMWindow, public Supplementable<LocalDOMWindow>, public DOMWindowLifecycleNotifier {
    USING_GARBAGE_COLLECTED_MIXIN(LocalDOMWindow);
    USING_PRE_FINALIZER(LocalDOMWindow, dispose);
public:
    static Document* createDocument(const String& mimeType, const DocumentInit&, bool forceXHTML);
    static LocalDOMWindow* create(LocalFrame& frame)
    {
        return new LocalDOMWindow(frame);
    }

    ~LocalDOMWindow() override;

    DECLARE_VIRTUAL_TRACE();

    Document* installNewDocument(const String& mimeType, const DocumentInit&, bool forceXHTML = false);

    // EventTarget overrides:
    ExecutionContext* getExecutionContext() const override;
    const LocalDOMWindow* toLocalDOMWindow() const override;
    LocalDOMWindow* toLocalDOMWindow() override;

    // DOMWindow overrides:
    LocalFrame* frame() const override;
    Screen* screen() const override;
    History* history() const override;
    BarProp* locationbar() const override;
    BarProp* menubar() const override;
    BarProp* personalbar() const override;
    BarProp* scrollbars() const override;
    BarProp* statusbar() const override;
    BarProp* toolbar() const override;
    Navigator* navigator() const override;
    bool offscreenBuffering() const override;
    int outerHeight() const override;
    int outerWidth() const override;
    int innerHeight() const override;
    int innerWidth() const override;
    int screenX() const override;
    int screenY() const override;
    double scrollX() const override;
    double scrollY() const override;
    DOMVisualViewport* visualViewport() override;
    const AtomicString& name() const override;
    void setName(const AtomicString&) override;
    String status() const override;
    void setStatus(const String&) override;
    String defaultStatus() const override;
    void setDefaultStatus(const String&) override;
    Document* document() const override;
    StyleMedia* styleMedia() const override;
    double devicePixelRatio() const override;
    ApplicationCache* applicationCache() const override;
    int orientation() const override;
    DOMSelection* getSelection() override;
    void blur() override;
    void print(ScriptState*) override;
    void stop() override;
    void alert(ScriptState*, const String& message = String()) override;
    bool confirm(ScriptState*, const String& message) override;
    String prompt(ScriptState*, const String& message, const String& defaultValue) override;
    bool find(const String&, bool caseSensitive, bool backwards, bool wrap, bool wholeWord, bool searchInFrames, bool showDialog) const override;

    // FIXME: ScrollBehaviorSmooth is currently unsupported in VisualViewport.
    // crbug.com/434497
    void scrollBy(double x, double y, ScrollBehavior = ScrollBehaviorAuto) const override;
    void scrollBy(const ScrollToOptions&) const override;
    void scrollTo(double x, double y) const override;
    void scrollTo(const ScrollToOptions&) const override;

    void moveBy(int x, int y) const override;
    void moveTo(int x, int y) const override;
    void resizeBy(int x, int y) const override;
    void resizeTo(int width, int height) const override;
    MediaQueryList* matchMedia(const String&) override;
    CSSStyleDeclaration* getComputedStyle(Element*, const String& pseudoElt) const override;
    CSSRuleList* getMatchedCSSRules(Element*, const String& pseudoElt) const override;
    int requestAnimationFrame(FrameRequestCallback*) override;
    int webkitRequestAnimationFrame(FrameRequestCallback*) override;
    void cancelAnimationFrame(int id) override;
    int requestIdleCallback(IdleRequestCallback*, const IdleRequestOptions&) override;
    void cancelIdleCallback(int id) override;
    CustomElementsRegistry* customElements(ScriptState*) const override;
    CustomElementsRegistry* customElements() const;

    void registerProperty(DOMWindowProperty*);
    void unregisterProperty(DOMWindowProperty*);

    void reset();

    unsigned pendingUnloadEventListeners() const;

    bool allowPopUp(); // Call on first window, not target window.
    static bool allowPopUp(LocalFrame& firstFrame);

    Element* frameElement() const;

    DOMWindow* open(const String& urlString, const AtomicString& frameName, const String& windowFeaturesString,
        LocalDOMWindow* callingWindow, LocalDOMWindow* enteredWindow);

    FrameConsole* frameConsole() const;

    void printErrorMessage(const String&) const;

    void postMessageTimerFired(PostMessageTimer*);
    void removePostMessageTimer(PostMessageTimer*);
    void dispatchMessageEventWithOriginCheck(SecurityOrigin* intendedTargetOrigin, Event*, std::unique_ptr<SourceLocation>);

    // Events
    // EventTarget API
    void removeAllEventListeners() override;

    using EventTarget::dispatchEvent;
    DispatchEventResult dispatchEvent(Event*, EventTarget*);

    void finishedLoading();

    // Dispatch the (deprecated) orientationchange event to this DOMWindow and
    // recurse on its child frames.
    void sendOrientationChangeEvent();

    void willDetachDocumentFromFrame();

    EventQueue* getEventQueue() const;
    void enqueueWindowEvent(Event*);
    void enqueueDocumentEvent(Event*);
    void enqueuePageshowEvent(PageshowEventPersistence);
    void enqueueHashchangeEvent(const String& oldURL, const String& newURL);
    void enqueuePopstateEvent(PassRefPtr<SerializedScriptValue>);
    void dispatchWindowLoadEvent();
    void documentWasClosed();
    void statePopped(PassRefPtr<SerializedScriptValue>);

    // FIXME: This shouldn't be public once LocalDOMWindow becomes ExecutionContext.
    void clearEventQueue();

    void acceptLanguagesChanged();

    FloatSize getViewportSize(IncludeScrollbarsInRect) const;

protected:
    // EventTarget overrides.
    void addedEventListener(const AtomicString& eventType, RegisteredEventListener&) override;
    void removedEventListener(const AtomicString& eventType, const RegisteredEventListener&) override;

    // Protected DOMWindow overrides.
    void schedulePostMessage(MessageEvent*, PassRefPtr<SecurityOrigin> target, Document* source) override;

private:
    class WindowFrameObserver;

    // Intentionally private to prevent redundant checks when the type is
    // already LocalDOMWindow.
    bool isLocalDOMWindow() const override { return true; }
    bool isRemoteDOMWindow() const override { return false; }

    explicit LocalDOMWindow(LocalFrame&);
    void dispose();

    void dispatchLoadEvent();
    void clearDocument();
    void willDestroyDocumentInFrame();

    void willDetachFrameHost();
    void frameDestroyed();

    Member<WindowFrameObserver> m_frameObserver;
    Member<Document> m_document;
    Member<DOMVisualViewport> m_visualViewport;

    bool m_shouldPrintWhenFinishedLoading;

    HeapHashSet<WeakMember<DOMWindowProperty>> m_properties;

    mutable Member<Screen> m_screen;
    mutable Member<History> m_history;
    mutable Member<BarProp> m_locationbar;
    mutable Member<BarProp> m_menubar;
    mutable Member<BarProp> m_personalbar;
    mutable Member<BarProp> m_scrollbars;
    mutable Member<BarProp> m_statusbar;
    mutable Member<BarProp> m_toolbar;
    mutable Member<Navigator> m_navigator;
    mutable Member<StyleMedia> m_media;
    mutable Member<CustomElementsRegistry> m_customElements;

    String m_status;
    String m_defaultStatus;

    mutable Member<ApplicationCache> m_applicationCache;

    Member<DOMWindowEventQueue> m_eventQueue;
    RefPtr<SerializedScriptValue> m_pendingStateObject;

    HeapHashSet<Member<PostMessageTimer>> m_postMessageTimers;
};

DEFINE_TYPE_CASTS(LocalDOMWindow, DOMWindow, x, x->isLocalDOMWindow(), x.isLocalDOMWindow());

inline String LocalDOMWindow::status() const
{
    return m_status;
}

inline String LocalDOMWindow::defaultStatus() const
{
    return m_defaultStatus;
}

} // namespace blink

#endif // LocalDOMWindow_h
