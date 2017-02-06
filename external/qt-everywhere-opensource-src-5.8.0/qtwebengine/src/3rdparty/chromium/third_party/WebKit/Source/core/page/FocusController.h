/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
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

#ifndef FocusController_h
#define FocusController_h

#include "core/CoreExport.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebFocusType.h"
#include "wtf/Forward.h"
#include "wtf/Noncopyable.h"
#include "wtf/RefPtr.h"

namespace blink {

struct FocusCandidate;
struct FocusParams;
class Document;
class Element;
class Frame;
class HTMLFrameOwnerElement;
class InputDeviceCapabilities;
class LocalFrame;
class Node;
class Page;
class RemoteFrame;

class CORE_EXPORT FocusController final : public GarbageCollected<FocusController> {
    WTF_MAKE_NONCOPYABLE(FocusController);
public:
    static FocusController* create(Page*);

    void setFocusedFrame(Frame*, bool notifyEmbedder = true);
    void focusDocumentView(Frame*, bool notifyEmbedder = true);
    LocalFrame* focusedFrame() const;
    Frame* focusedOrMainFrame() const;

    // Finds the focused HTMLFrameOwnerElement, if any, in the provided frame.
    // An HTMLFrameOwnerElement is considered focused if the frame it owns, or
    // one of its descendant frames, is currently focused.
    HTMLFrameOwnerElement* focusedFrameOwnerElement(LocalFrame& currentFrame) const;

    // Determines whether the provided Document has focus according to
    // http://www.w3.org/TR/html5/editing.html#dom-document-hasfocus
    bool isDocumentFocused(const Document&) const;

    bool setInitialFocus(WebFocusType);
    bool advanceFocus(WebFocusType type, InputDeviceCapabilities* sourceCapabilities = nullptr) { return advanceFocus(type, false, sourceCapabilities); }
    bool advanceFocusAcrossFrames(WebFocusType, RemoteFrame* from, LocalFrame* to, InputDeviceCapabilities* sourceCapabilities = nullptr);
    Element* findFocusableElementInShadowHost(const Element& shadowHost);

    bool setFocusedElement(Element*, Frame*, const FocusParams&);
    // |setFocusedElement| variant with SelectionBehaviorOnFocus::None,
    // |WebFocusTypeNone, and null InputDeviceCapabilities.
    bool setFocusedElement(Element*, Frame*);

    void setActive(bool);
    bool isActive() const { return m_isActive; }

    void setFocused(bool);
    bool isFocused() const { return m_isFocused; }

    DECLARE_TRACE();

private:
    explicit FocusController(Page*);

    Element* findFocusableElement(WebFocusType, Element&);

    bool advanceFocus(WebFocusType, bool initialFocus, InputDeviceCapabilities* sourceCapabilities = nullptr);
    bool advanceFocusDirectionally(WebFocusType);
    bool advanceFocusInDocumentOrder(LocalFrame*, Element* start, WebFocusType, bool initialFocus, InputDeviceCapabilities* sourceCapabilities);

    bool advanceFocusDirectionallyInContainer(Node* container, const LayoutRect& startingRect, WebFocusType);
    void findFocusCandidateInContainer(Node& container, const LayoutRect& startingRect, WebFocusType, FocusCandidate& closest);

    Member<Page> m_page;
    Member<Frame> m_focusedFrame;
    bool m_isActive;
    bool m_isFocused;
    bool m_isChangingFocusedFrame;
};

} // namespace blink

#endif // FocusController_h
