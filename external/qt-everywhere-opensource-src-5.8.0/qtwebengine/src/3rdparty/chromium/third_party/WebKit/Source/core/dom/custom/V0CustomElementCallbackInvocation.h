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

#ifndef V0CustomElementCallbackInvocation_h
#define V0CustomElementCallbackInvocation_h

#include "core/dom/custom/V0CustomElementLifecycleCallbacks.h"
#include "core/dom/custom/V0CustomElementProcessingStep.h"
#include "wtf/text/AtomicString.h"

namespace blink {

class V0CustomElementCallbackInvocation : public V0CustomElementProcessingStep {
    WTF_MAKE_NONCOPYABLE(V0CustomElementCallbackInvocation);
public:
    static V0CustomElementCallbackInvocation* createInvocation(V0CustomElementLifecycleCallbacks*, V0CustomElementLifecycleCallbacks::CallbackType);
    static V0CustomElementCallbackInvocation* createAttributeChangedInvocation(V0CustomElementLifecycleCallbacks*, const AtomicString& name, const AtomicString& oldValue, const AtomicString& newValue);

protected:
    V0CustomElementCallbackInvocation(V0CustomElementLifecycleCallbacks* callbacks)
        : m_callbacks(callbacks)
    {
    }

    V0CustomElementLifecycleCallbacks* callbacks() { return m_callbacks.get(); }

    DECLARE_VIRTUAL_TRACE();

private:
    Member<V0CustomElementLifecycleCallbacks> m_callbacks;
};

} // namespace blink

#endif // V0CustomElementCallbackInvocation_h
