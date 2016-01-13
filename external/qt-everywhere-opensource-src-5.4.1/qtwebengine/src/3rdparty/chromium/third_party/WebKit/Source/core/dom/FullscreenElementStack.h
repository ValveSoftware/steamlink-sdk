/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef FullscreenElementStack_h
#define FullscreenElementStack_h

#include "core/dom/Document.h"
#include "core/dom/DocumentLifecycleObserver.h"
#include "core/dom/Element.h"
#include "platform/Supplementable.h"
#include "platform/Timer.h"
#include "platform/geometry/LayoutRect.h"
#include "wtf/Deque.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"

namespace WebCore {

class RenderFullScreen;
class RenderStyle;

class FullscreenElementStack FINAL
    : public NoBaseWillBeGarbageCollectedFinalized<FullscreenElementStack>
    , public DocumentSupplement
    , public DocumentLifecycleObserver {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(FullscreenElementStack);
public:
    virtual ~FullscreenElementStack();
    static const char* supplementName();
    static FullscreenElementStack& from(Document&);
    static FullscreenElementStack* fromIfExists(Document&);
    static Element* fullscreenElementFrom(Document&);
    static Element* currentFullScreenElementFrom(Document&);
    static bool isFullScreen(Document&);
    static bool isActiveFullScreenElement(const Element*);

    enum FullScreenCheckType {
        EnforceIFrameAllowFullScreenRequirement,
        ExemptIFrameAllowFullScreenRequirement,
    };

    void requestFullScreenForElement(Element*, unsigned short flags, FullScreenCheckType);
    void webkitCancelFullScreen();

    void webkitWillEnterFullScreenForElement(Element*);
    void webkitDidEnterFullScreenForElement(Element*);
    void webkitWillExitFullScreenForElement(Element*);
    void webkitDidExitFullScreenForElement(Element*);

    void setFullScreenRenderer(RenderFullScreen*);
    RenderFullScreen* fullScreenRenderer() const { return m_fullScreenRenderer; }
    void fullScreenRendererDestroyed();

    void clearFullscreenElementStack();
    void popFullscreenElementStack();
    void pushFullscreenElementStack(Element*);
    void addDocumentToFullScreenChangeEventQueue(Document*);

    bool fullScreenIsAllowedForElement(Element*) const;
    void fullScreenElementRemoved();
    void removeFullScreenElementOfSubtree(Node*, bool amongChildrenOnly = false);

    // W3C API
    static bool webkitFullscreenEnabled(Document&);
    Element* webkitFullscreenElement() const { return !m_fullScreenElementStack.isEmpty() ? m_fullScreenElementStack.last().get() : 0; }
    void webkitExitFullscreen();

    bool webkitIsFullScreen() const { return m_fullScreenElement.get(); }
    bool webkitFullScreenKeyboardInputAllowed() const { return m_fullScreenElement.get() && m_areKeysEnabledInFullScreen; }
    Element* webkitCurrentFullScreenElement() const { return m_fullScreenElement.get(); }

    virtual void documentWasDetached() OVERRIDE;
#if !ENABLE(OILPAN)
    virtual void documentWasDisposed() OVERRIDE;
#endif

    virtual void trace(Visitor*) OVERRIDE;

private:
    static FullscreenElementStack* fromIfExistsSlow(Document&);

    explicit FullscreenElementStack(Document&);

    Document* document();
    void fullScreenChangeDelayTimerFired(Timer<FullscreenElementStack>*);

    bool m_areKeysEnabledInFullScreen;
    RefPtrWillBeMember<Element> m_fullScreenElement;
    WillBeHeapVector<RefPtrWillBeMember<Element> > m_fullScreenElementStack;
    RenderFullScreen* m_fullScreenRenderer;
    Timer<FullscreenElementStack> m_fullScreenChangeDelayTimer;
    WillBeHeapDeque<RefPtrWillBeMember<Node> > m_fullScreenChangeEventTargetQueue;
    WillBeHeapDeque<RefPtrWillBeMember<Node> > m_fullScreenErrorEventTargetQueue;
    LayoutRect m_savedPlaceholderFrameRect;
    RefPtr<RenderStyle> m_savedPlaceholderRenderStyle;
};

inline bool FullscreenElementStack::isActiveFullScreenElement(const Element* element)
{
    FullscreenElementStack* controller = fromIfExists(element->document());
    if (!controller)
        return false;
    return controller->webkitIsFullScreen() && controller->webkitCurrentFullScreenElement() == element;
}

inline FullscreenElementStack* FullscreenElementStack::fromIfExists(Document& document)
{
    if (!document.hasFullscreenElementStack())
        return 0;
    return fromIfExistsSlow(document);
}

} // namespace WebCore

#endif // FullscreenElementStack_h
