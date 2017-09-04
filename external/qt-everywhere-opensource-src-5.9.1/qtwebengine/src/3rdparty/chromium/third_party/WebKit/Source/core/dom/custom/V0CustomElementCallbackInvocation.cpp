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

#include "core/dom/custom/V0CustomElementCallbackInvocation.h"

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/custom/V0CustomElementScheduler.h"

namespace blink {

class AttachedDetachedInvocation final
    : public V0CustomElementCallbackInvocation {
 public:
  AttachedDetachedInvocation(
      V0CustomElementLifecycleCallbacks*,
      V0CustomElementLifecycleCallbacks::CallbackType which);

 private:
  void dispatch(Element*) override;

  V0CustomElementLifecycleCallbacks::CallbackType m_which;
};

AttachedDetachedInvocation::AttachedDetachedInvocation(
    V0CustomElementLifecycleCallbacks* callbacks,
    V0CustomElementLifecycleCallbacks::CallbackType which)
    : V0CustomElementCallbackInvocation(callbacks), m_which(which) {
  DCHECK(m_which == V0CustomElementLifecycleCallbacks::AttachedCallback ||
         m_which == V0CustomElementLifecycleCallbacks::DetachedCallback);
}

void AttachedDetachedInvocation::dispatch(Element* element) {
  switch (m_which) {
    case V0CustomElementLifecycleCallbacks::AttachedCallback:
      callbacks()->attached(element);
      break;
    case V0CustomElementLifecycleCallbacks::DetachedCallback:
      callbacks()->detached(element);
      break;
    default:
      NOTREACHED();
  }
}

class AttributeChangedInvocation final
    : public V0CustomElementCallbackInvocation {
 public:
  AttributeChangedInvocation(V0CustomElementLifecycleCallbacks*,
                             const AtomicString& name,
                             const AtomicString& oldValue,
                             const AtomicString& newValue);

 private:
  void dispatch(Element*) override;

  AtomicString m_name;
  AtomicString m_oldValue;
  AtomicString m_newValue;
};

AttributeChangedInvocation::AttributeChangedInvocation(
    V0CustomElementLifecycleCallbacks* callbacks,
    const AtomicString& name,
    const AtomicString& oldValue,
    const AtomicString& newValue)
    : V0CustomElementCallbackInvocation(callbacks),
      m_name(name),
      m_oldValue(oldValue),
      m_newValue(newValue) {}

void AttributeChangedInvocation::dispatch(Element* element) {
  callbacks()->attributeChanged(element, m_name, m_oldValue, m_newValue);
}

class CreatedInvocation final : public V0CustomElementCallbackInvocation {
 public:
  explicit CreatedInvocation(V0CustomElementLifecycleCallbacks* callbacks)
      : V0CustomElementCallbackInvocation(callbacks) {}

 private:
  void dispatch(Element*) override;
  bool isCreatedCallback() const override { return true; }
};

void CreatedInvocation::dispatch(Element* element) {
  if (element->isConnected() && element->document().domWindow())
    V0CustomElementScheduler::scheduleCallback(
        callbacks(), element,
        V0CustomElementLifecycleCallbacks::AttachedCallback);
  callbacks()->created(element);
}

V0CustomElementCallbackInvocation*
V0CustomElementCallbackInvocation::createInvocation(
    V0CustomElementLifecycleCallbacks* callbacks,
    V0CustomElementLifecycleCallbacks::CallbackType which) {
  switch (which) {
    case V0CustomElementLifecycleCallbacks::CreatedCallback:
      return new CreatedInvocation(callbacks);

    case V0CustomElementLifecycleCallbacks::AttachedCallback:
    case V0CustomElementLifecycleCallbacks::DetachedCallback:
      return new AttachedDetachedInvocation(callbacks, which);
    default:
      NOTREACHED();
      return nullptr;
  }
}

V0CustomElementCallbackInvocation*
V0CustomElementCallbackInvocation::createAttributeChangedInvocation(
    V0CustomElementLifecycleCallbacks* callbacks,
    const AtomicString& name,
    const AtomicString& oldValue,
    const AtomicString& newValue) {
  return new AttributeChangedInvocation(callbacks, name, oldValue, newValue);
}

DEFINE_TRACE(V0CustomElementCallbackInvocation) {
  visitor->trace(m_callbacks);
  V0CustomElementProcessingStep::trace(visitor);
}

}  // namespace blink
