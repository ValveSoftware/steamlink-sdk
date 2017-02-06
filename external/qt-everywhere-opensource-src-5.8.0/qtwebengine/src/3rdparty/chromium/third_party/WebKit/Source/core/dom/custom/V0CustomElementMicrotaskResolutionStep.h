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

#ifndef V0CustomElementMicrotaskResolutionStep_h
#define V0CustomElementMicrotaskResolutionStep_h

#include "core/dom/custom/V0CustomElementDescriptor.h"
#include "core/dom/custom/V0CustomElementMicrotaskStep.h"
#include "platform/heap/Handle.h"

namespace blink {

class V0CustomElementRegistrationContext;
class Element;

class V0CustomElementMicrotaskResolutionStep final : public V0CustomElementMicrotaskStep {
public:
    static V0CustomElementMicrotaskResolutionStep* create(V0CustomElementRegistrationContext*, Element*, const V0CustomElementDescriptor&);

    ~V0CustomElementMicrotaskResolutionStep() override;

    DECLARE_VIRTUAL_TRACE();

private:
    V0CustomElementMicrotaskResolutionStep(V0CustomElementRegistrationContext*, Element*, const V0CustomElementDescriptor&);

    Result process() override;

#if !defined(NDEBUG)
    void show(unsigned indent) override;
#endif

    Member<V0CustomElementRegistrationContext> m_context;
    Member<Element> m_element;
    V0CustomElementDescriptor m_descriptor;
};

} // namespace blink

#endif // V0CustomElementMicrotaskResolutionStep_h
