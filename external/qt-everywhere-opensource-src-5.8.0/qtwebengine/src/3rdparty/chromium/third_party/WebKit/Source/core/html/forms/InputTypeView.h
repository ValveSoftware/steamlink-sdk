/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Samsung Electronics. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef InputTypeView_h
#define InputTypeView_h

#include "core/CoreExport.h"
#include "core/events/EventDispatcher.h"
#include "platform/heap/Handle.h"
#include "platform/text/TextDirection.h"
#include "public/platform/WebFocusType.h"
#include "wtf/Allocator.h"
#include "wtf/Forward.h"
#include "wtf/Noncopyable.h"

namespace blink {

class AXObject;
class BeforeTextInsertedEvent;
class Element;
class Event;
class FormControlState;
class HTMLFormElement;
class HTMLInputElement;
class KeyboardEvent;
class MouseEvent;
class LayoutObject;
class ComputedStyle;
class TouchEvent;

class ClickHandlingState final : public EventDispatchHandlingState {
public:
    DECLARE_VIRTUAL_TRACE();

    bool checked;
    bool indeterminate;
    Member<HTMLInputElement> checkedRadioButton;
};

// An InputTypeView object represents the UI-specific part of an
// HTMLInputElement. Do not expose instances of InputTypeView and classes
// derived from it to classes other than HTMLInputElement.
class CORE_EXPORT InputTypeView : public GarbageCollectedMixin {
    WTF_MAKE_NONCOPYABLE(InputTypeView);
public:
    virtual ~InputTypeView();
    DECLARE_VIRTUAL_TRACE();

    virtual bool sizeShouldIncludeDecoration(int defaultSize, int& preferredSize) const;

    // Event handling functions

    virtual void handleClickEvent(MouseEvent*);
    virtual void handleMouseDownEvent(MouseEvent*);
    virtual ClickHandlingState* willDispatchClick();
    virtual void didDispatchClick(Event*, const ClickHandlingState&);
    virtual void handleKeydownEvent(KeyboardEvent*);
    virtual void handleKeypressEvent(KeyboardEvent*);
    virtual void handleKeyupEvent(KeyboardEvent*);
    virtual void handleBeforeTextInsertedEvent(BeforeTextInsertedEvent*);
    virtual void handleTouchEvent(TouchEvent*);
    virtual void forwardEvent(Event*);
    virtual bool shouldSubmitImplicitly(Event*);
    virtual HTMLFormElement* formForSubmission() const;
    virtual bool hasCustomFocusLogic() const;
    virtual void handleFocusEvent(Element* oldFocusedElement, WebFocusType);
    virtual void handleFocusInEvent(Element* oldFocusedElement, WebFocusType);
    virtual void handleBlurEvent();
    virtual void handleDOMActivateEvent(Event*);
    virtual void accessKeyAction(bool sendMouseEvents);
    virtual bool hasTouchEventHandler() const;
    virtual void blur();
    void dispatchSimulatedClickIfActive(KeyboardEvent*) const;

    virtual void subtreeHasChanged();
    virtual LayoutObject* createLayoutObject(const ComputedStyle&) const;
    virtual PassRefPtr<ComputedStyle> customStyleForLayoutObject(PassRefPtr<ComputedStyle>);
    virtual TextDirection computedTextDirection();
    virtual void startResourceLoading();
    virtual void closePopupView();
    virtual void createShadowSubtree();
    virtual void destroyShadowSubtree();
    virtual void minOrMaxAttributeChanged();
    virtual void stepAttributeChanged();
    virtual void altAttributeChanged();
    virtual void srcAttributeChanged();
    virtual void updateView();
    virtual void attributeChanged();
    virtual void multipleAttributeChanged();
    virtual void disabledAttributeChanged();
    virtual void readonlyAttributeChanged();
    virtual void requiredAttributeChanged();
    virtual void valueAttributeChanged();
    virtual void didSetValue(const String&, bool valueChanged);
    virtual void listAttributeTargetChanged();
    virtual void updateClearButtonVisibility();
    virtual void updatePlaceholderText();
    virtual AXObject* popupRootAXObject();
    virtual void ensureFallbackContent() { }
    virtual void ensurePrimaryContent() { }
    virtual bool hasFallbackContent() const { return false; }
    virtual FormControlState saveFormControlState() const;
    virtual void restoreFormControlState(const FormControlState&);

    // Validation functions
    virtual bool hasBadInput() const;

protected:
    InputTypeView(HTMLInputElement& element) : m_element(&element) { }
    HTMLInputElement& element() const { return *m_element; }

private:
    Member<HTMLInputElement> m_element;
};

} // namespace blink
#endif
