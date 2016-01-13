/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef TextFieldInputType_h
#define TextFieldInputType_h

#include "core/html/forms/InputType.h"
#include "core/html/shadow/SpinButtonElement.h"

namespace WebCore {

class FormDataList;

// The class represents types of which UI contain text fields.
// It supports not only the types for BaseTextInputType but also type=number.
class TextFieldInputType : public InputType, protected SpinButtonElement::SpinButtonOwner {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(TextFieldInputType);
protected:
    TextFieldInputType(HTMLInputElement&);
    virtual ~TextFieldInputType();
    virtual bool canSetSuggestedValue() OVERRIDE;
    virtual void handleKeydownEvent(KeyboardEvent*) OVERRIDE;
    void handleKeydownEventForSpinButton(KeyboardEvent*);

protected:
    virtual bool needsContainer() const { return false; }
    bool shouldHaveSpinButton() const;
    virtual void createShadowSubtree() OVERRIDE;
    virtual void destroyShadowSubtree() OVERRIDE;
    virtual void attributeChanged() OVERRIDE;
    virtual void disabledAttributeChanged() OVERRIDE;
    virtual void readonlyAttributeChanged() OVERRIDE;
    virtual bool supportsReadOnly() const OVERRIDE;
    virtual void handleFocusEvent(Element* oldFocusedNode, FocusType) OVERRIDE FINAL;
    virtual void handleBlurEvent() OVERRIDE FINAL;
    virtual void setValue(const String&, bool valueChanged, TextFieldEventBehavior) OVERRIDE;
    virtual void updateView() OVERRIDE;

    virtual String convertFromVisibleValue(const String&) const;
    enum ValueChangeState {
        ValueChangeStateNone,
        ValueChangeStateChanged
    };
    virtual void didSetValueByUserEdit(ValueChangeState);

    Element* containerElement() const;

private:
    virtual bool shouldShowFocusRingOnMouseFocus() const OVERRIDE FINAL;
    virtual bool isTextField() const OVERRIDE FINAL;
    virtual bool valueMissing(const String&) const OVERRIDE;
    virtual void handleBeforeTextInsertedEvent(BeforeTextInsertedEvent*) OVERRIDE;
    virtual void forwardEvent(Event*) OVERRIDE FINAL;
    virtual bool shouldSubmitImplicitly(Event*) OVERRIDE FINAL;
    virtual RenderObject* createRenderer(RenderStyle*) const OVERRIDE;
    virtual bool shouldUseInputMethod() const OVERRIDE;
    virtual String sanitizeValue(const String&) const OVERRIDE;
    virtual bool shouldRespectListAttribute() OVERRIDE;
    virtual void listAttributeTargetChanged() OVERRIDE;
    virtual void updatePlaceholderText() OVERRIDE FINAL;
    virtual bool appendFormData(FormDataList&, bool multipart) const OVERRIDE;
    virtual void subtreeHasChanged() OVERRIDE FINAL;

    // SpinButtonElement::SpinButtonOwner functions.
    virtual void focusAndSelectSpinButtonOwner() OVERRIDE FINAL;
    virtual bool shouldSpinButtonRespondToMouseEvents() OVERRIDE FINAL;
    virtual bool shouldSpinButtonRespondToWheelEvents() OVERRIDE FINAL;
    virtual void spinButtonStepDown() OVERRIDE FINAL;
    virtual void spinButtonStepUp() OVERRIDE FINAL;
    virtual void spinButtonDidReleaseMouseCapture(SpinButtonElement::EventDispatch) OVERRIDE FINAL;

    SpinButtonElement* spinButtonElement() const;
};

} // namespace WebCore

#endif // TextFieldInputType_h
