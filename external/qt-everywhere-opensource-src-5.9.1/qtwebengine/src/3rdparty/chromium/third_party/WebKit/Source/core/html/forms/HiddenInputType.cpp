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

#include "core/html/forms/HiddenInputType.h"

#include "core/HTMLNames.h"
#include "core/InputTypeNames.h"
#include "core/html/FormData.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/forms/FormController.h"

namespace blink {

using namespace HTMLNames;

InputType* HiddenInputType::create(HTMLInputElement& element) {
  return new HiddenInputType(element);
}

DEFINE_TRACE(HiddenInputType) {
  InputTypeView::trace(visitor);
  InputType::trace(visitor);
}

InputTypeView* HiddenInputType::createView() {
  return this;
}

const AtomicString& HiddenInputType::formControlType() const {
  return InputTypeNames::hidden;
}

FormControlState HiddenInputType::saveFormControlState() const {
  // valueAttributeWasUpdatedAfterParsing() never be true for form
  // controls create by createElement() or cloneNode(). It's ok for
  // now because we restore values only to form controls created by
  // parsing.
  return element().valueAttributeWasUpdatedAfterParsing()
             ? FormControlState(element().value())
             : FormControlState();
}

void HiddenInputType::restoreFormControlState(const FormControlState& state) {
  element().setAttribute(valueAttr, AtomicString(state[0]));
}

bool HiddenInputType::supportsValidation() const {
  return false;
}

LayoutObject* HiddenInputType::createLayoutObject(const ComputedStyle&) const {
  NOTREACHED();
  return nullptr;
}

void HiddenInputType::accessKeyAction(bool) {}

bool HiddenInputType::layoutObjectIsNeeded() {
  return false;
}

InputType::ValueMode HiddenInputType::valueMode() const {
  return ValueMode::kDefault;
}

void HiddenInputType::setValue(const String& sanitizedValue,
                               bool,
                               TextFieldEventBehavior) {
  element().setAttribute(valueAttr, AtomicString(sanitizedValue));
}

void HiddenInputType::appendToFormData(FormData& formData) const {
  if (equalIgnoringCase(element().name(), "_charset_")) {
    formData.append(element().name(), String(formData.encoding().name()));
    return;
  }
  InputType::appendToFormData(formData);
}

bool HiddenInputType::shouldRespectHeightAndWidthAttributes() {
  return true;
}

}  // namespace blink
