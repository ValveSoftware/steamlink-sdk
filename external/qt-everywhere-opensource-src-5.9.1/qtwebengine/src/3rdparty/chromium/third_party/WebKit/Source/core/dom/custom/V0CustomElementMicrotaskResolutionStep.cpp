/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
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

#include "core/dom/custom/V0CustomElementMicrotaskResolutionStep.h"

#include "core/dom/Element.h"
#include "core/dom/custom/V0CustomElementRegistrationContext.h"

namespace blink {

V0CustomElementMicrotaskResolutionStep*
V0CustomElementMicrotaskResolutionStep::create(
    V0CustomElementRegistrationContext* context,
    Element* element,
    const V0CustomElementDescriptor& descriptor) {
  return new V0CustomElementMicrotaskResolutionStep(context, element,
                                                    descriptor);
}

V0CustomElementMicrotaskResolutionStep::V0CustomElementMicrotaskResolutionStep(
    V0CustomElementRegistrationContext* context,
    Element* element,
    const V0CustomElementDescriptor& descriptor)
    : m_context(context), m_element(element), m_descriptor(descriptor) {}

V0CustomElementMicrotaskResolutionStep::
    ~V0CustomElementMicrotaskResolutionStep() {}

V0CustomElementMicrotaskStep::Result
V0CustomElementMicrotaskResolutionStep::process() {
  m_context->resolve(m_element.get(), m_descriptor);
  return V0CustomElementMicrotaskStep::FinishedProcessing;
}

DEFINE_TRACE(V0CustomElementMicrotaskResolutionStep) {
  visitor->trace(m_context);
  visitor->trace(m_element);
  V0CustomElementMicrotaskStep::trace(visitor);
}

#if !defined(NDEBUG)
void V0CustomElementMicrotaskResolutionStep::show(unsigned indent) {
  fprintf(stderr, "%*sResolution: ", indent, "");
  m_element->outerHTML().show();
}
#endif

}  // namespace blink
