/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "core/html/forms/ChooserOnlyTemporalInputTypeView.h"

#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "core/dom/Document.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/frame/FrameHost.h"
#include "core/html/HTMLDivElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/page/ChromeClient.h"
#include "platform/UserGestureIndicator.h"

namespace blink {

ChooserOnlyTemporalInputTypeView::ChooserOnlyTemporalInputTypeView(HTMLInputElement& element, BaseTemporalInputType& inputType)
    : KeyboardClickableInputTypeView(element)
    ,  m_inputType(inputType)
{
    ThreadState::current()->registerPreFinalizer(this);
}

ChooserOnlyTemporalInputTypeView* ChooserOnlyTemporalInputTypeView::create(HTMLInputElement& element, BaseTemporalInputType& inputType)
{
    return new ChooserOnlyTemporalInputTypeView(element, inputType);
}

ChooserOnlyTemporalInputTypeView::~ChooserOnlyTemporalInputTypeView()
{
    DCHECK(!m_dateTimeChooser);
}

DEFINE_TRACE(ChooserOnlyTemporalInputTypeView)
{
    visitor->trace(m_inputType);
    visitor->trace(m_dateTimeChooser);
    InputTypeView::trace(visitor);
    DateTimeChooserClient::trace(visitor);
}

void ChooserOnlyTemporalInputTypeView::handleDOMActivateEvent(Event*)
{
    if (element().isDisabledOrReadOnly() || !element().layoutObject() || !UserGestureIndicator::processingUserGesture() || element().openShadowRoot())
        return;

    if (m_dateTimeChooser)
        return;
    if (!element().document().isActive())
        return;
    DateTimeChooserParameters parameters;
    if (!element().setupDateTimeChooserParameters(parameters))
        return;
    m_dateTimeChooser = element().document().frameHost()->chromeClient().openDateTimeChooser(this, parameters);
}

void ChooserOnlyTemporalInputTypeView::createShadowSubtree()
{
    DEFINE_STATIC_LOCAL(AtomicString, valueContainerPseudo, ("-webkit-date-and-time-value"));

    HTMLDivElement* valueContainer = HTMLDivElement::create(element().document());
    valueContainer->setShadowPseudoId(valueContainerPseudo);
    element().userAgentShadowRoot()->appendChild(valueContainer);
    updateView();
}

void ChooserOnlyTemporalInputTypeView::updateView()
{
    Node* node = element().userAgentShadowRoot()->firstChild();
    if (!node || !node->isHTMLElement())
        return;
    String displayValue;
    if (!element().suggestedValue().isNull())
        displayValue = element().suggestedValue();
    else
        displayValue = m_inputType->visibleValue();
    if (displayValue.isEmpty()) {
        // Need to put something to keep text baseline.
        displayValue = " ";
    }
    toHTMLElement(node)->setTextContent(displayValue);
}

void ChooserOnlyTemporalInputTypeView::didSetValue(const String& value, bool valueChanged)
{
    if (valueChanged)
        updateView();
}

void ChooserOnlyTemporalInputTypeView::closePopupView()
{
    closeDateTimeChooser();
}

Element& ChooserOnlyTemporalInputTypeView::ownerElement() const
{
    return element();
}

void ChooserOnlyTemporalInputTypeView::didChooseValue(const String& value)
{
    element().setValue(value, DispatchInputAndChangeEvent);
}

void ChooserOnlyTemporalInputTypeView::didChooseValue(double value)
{
    DCHECK(std::isfinite(value) || std::isnan(value));
    if (std::isnan(value))
        element().setValue(emptyString(), DispatchInputAndChangeEvent);
    else
        element().setValueAsNumber(value, ASSERT_NO_EXCEPTION, DispatchInputAndChangeEvent);
}

void ChooserOnlyTemporalInputTypeView::didEndChooser()
{
    m_dateTimeChooser.clear();
}

void ChooserOnlyTemporalInputTypeView::closeDateTimeChooser()
{
    if (m_dateTimeChooser)
        m_dateTimeChooser->endChooser();
}

} // namespace blink
