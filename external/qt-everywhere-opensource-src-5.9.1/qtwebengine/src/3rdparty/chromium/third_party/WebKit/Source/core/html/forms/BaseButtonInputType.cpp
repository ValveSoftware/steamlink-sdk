/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#include "core/html/forms/BaseButtonInputType.h"

#include "core/HTMLNames.h"
#include "core/dom/Text.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/layout/LayoutButton.h"

namespace blink {

using namespace HTMLNames;

BaseButtonInputType::BaseButtonInputType(HTMLInputElement& element)
    : InputType(element), KeyboardClickableInputTypeView(element) {}

DEFINE_TRACE(BaseButtonInputType) {
  KeyboardClickableInputTypeView::trace(visitor);
  InputType::trace(visitor);
}

InputTypeView* BaseButtonInputType::createView() {
  return this;
}

void BaseButtonInputType::createShadowSubtree() {
  DCHECK(element().userAgentShadowRoot());
  element().userAgentShadowRoot()->appendChild(
      Text::create(element().document(), displayValue()));
}

void BaseButtonInputType::valueAttributeChanged() {
  toTextOrDie(element().userAgentShadowRoot()->firstChild())
      ->setData(displayValue());
}

String BaseButtonInputType::displayValue() const {
  return element().valueOrDefaultLabel().removeCharacters(isHTMLLineBreak);
}

bool BaseButtonInputType::shouldSaveAndRestoreFormControlState() const {
  return false;
}

void BaseButtonInputType::appendToFormData(FormData&) const {}

LayoutObject* BaseButtonInputType::createLayoutObject(
    const ComputedStyle&) const {
  return new LayoutButton(&element());
}

InputType::ValueMode BaseButtonInputType::valueMode() const {
  return ValueMode::kDefault;
}

void BaseButtonInputType::setValue(const String& sanitizedValue,
                                   bool,
                                   TextFieldEventBehavior) {
  element().setAttribute(valueAttr, AtomicString(sanitizedValue));
}

bool BaseButtonInputType::matchesDefaultPseudoClass() {
  // HTMLFormElement::findDefaultButton() traverses the tree. So we check
  // canBeSuccessfulSubmitButton() first for early return.
  return canBeSuccessfulSubmitButton() && element().form() &&
         element().form()->findDefaultButton() == &element();
}

}  // namespace blink
