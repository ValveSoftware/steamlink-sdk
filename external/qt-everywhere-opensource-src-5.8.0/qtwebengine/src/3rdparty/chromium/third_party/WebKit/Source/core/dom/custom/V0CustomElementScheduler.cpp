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

#include "core/dom/custom/V0CustomElementScheduler.h"

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/custom/V0CustomElementCallbackInvocation.h"
#include "core/dom/custom/V0CustomElementLifecycleCallbacks.h"
#include "core/dom/custom/V0CustomElementMicrotaskDispatcher.h"
#include "core/dom/custom/V0CustomElementMicrotaskImportStep.h"
#include "core/dom/custom/V0CustomElementMicrotaskResolutionStep.h"
#include "core/dom/custom/V0CustomElementMicrotaskRunQueue.h"
#include "core/dom/custom/V0CustomElementProcessingStack.h"
#include "core/dom/custom/V0CustomElementRegistrationContext.h"
#include "core/dom/custom/V0CustomElementSyncMicrotaskQueue.h"
#include "core/html/imports/HTMLImportChild.h"
#include "core/html/imports/HTMLImportsController.h"

namespace blink {

// FIXME: Consider moving the element's callback queue to ElementRareData.
typedef HeapHashMap<Member<Element>, Member<V0CustomElementCallbackQueue>> ElementCallbackQueueMap;

static ElementCallbackQueueMap& callbackQueues()
{
    DEFINE_STATIC_LOCAL(ElementCallbackQueueMap, map, (new ElementCallbackQueueMap));
    return map;
}

static V0CustomElementCallbackQueue& ensureCallbackQueue(Element* element)
{
    ElementCallbackQueueMap::ValueType* it = callbackQueues().add(element, nullptr).storedValue;
    if (!it->value)
        it->value = V0CustomElementCallbackQueue::create(element);
    return *it->value.get();
}

// Finds or creates the callback queue for element.
static V0CustomElementCallbackQueue& scheduleCallbackQueue(Element* element)
{
    V0CustomElementCallbackQueue& callbackQueue = ensureCallbackQueue(element);
    if (callbackQueue.inCreatedCallback()) {
        // Don't move it. Authors use the createdCallback like a
        // constructor. By not moving it, the createdCallback
        // completes before any other callbacks are entered for this
        // element.
        return callbackQueue;
    }

    if (V0CustomElementProcessingStack::inCallbackDeliveryScope()) {
        // The processing stack is active.
        V0CustomElementProcessingStack::instance().enqueue(&callbackQueue);
        return callbackQueue;
    }

    V0CustomElementMicrotaskDispatcher::instance().enqueue(&callbackQueue);
    return callbackQueue;
}

void V0CustomElementScheduler::scheduleCallback(V0CustomElementLifecycleCallbacks* callbacks, Element* element, V0CustomElementLifecycleCallbacks::CallbackType type)
{
    DCHECK(type != V0CustomElementLifecycleCallbacks::AttributeChangedCallback);

    if (!callbacks->hasCallback(type))
        return;

    V0CustomElementCallbackQueue& queue = scheduleCallbackQueue(element);
    queue.append(V0CustomElementCallbackInvocation::createInvocation(callbacks, type));
}

void V0CustomElementScheduler::scheduleAttributeChangedCallback(V0CustomElementLifecycleCallbacks* callbacks, Element* element, const AtomicString& name, const AtomicString& oldValue, const AtomicString& newValue)
{
    if (!callbacks->hasCallback(V0CustomElementLifecycleCallbacks::AttributeChangedCallback))
        return;

    V0CustomElementCallbackQueue& queue = scheduleCallbackQueue(element);
    queue.append(V0CustomElementCallbackInvocation::createAttributeChangedInvocation(callbacks, name, oldValue, newValue));
}

void V0CustomElementScheduler::resolveOrScheduleResolution(V0CustomElementRegistrationContext* context, Element* element, const V0CustomElementDescriptor& descriptor)
{
    if (V0CustomElementProcessingStack::inCallbackDeliveryScope()) {
        context->resolve(element, descriptor);
        return;
    }

    Document& document = element->document();
    V0CustomElementMicrotaskResolutionStep* step = V0CustomElementMicrotaskResolutionStep::create(context, element, descriptor);
    enqueueMicrotaskStep(document, step);
}

V0CustomElementMicrotaskImportStep* V0CustomElementScheduler::scheduleImport(HTMLImportChild* import)
{
    DCHECK(!import->hasFinishedLoading());
    DCHECK(import->parent());

    // Ownership of the new step is transferred to the parent
    // processing step, or the base queue.
    V0CustomElementMicrotaskImportStep* step = V0CustomElementMicrotaskImportStep::create(import);
    V0CustomElementMicrotaskImportStep* rawStep = step;
    enqueueMicrotaskStep(*(import->parent()->document()), step, import->isSync());
    return rawStep;
}

void V0CustomElementScheduler::enqueueMicrotaskStep(Document& document, V0CustomElementMicrotaskStep* step, bool importIsSync)
{
    Document& master = document.importsController() ? *(document.importsController()->master()) : document;
    master.customElementMicrotaskRunQueue()->enqueue(document.importLoader(), step, importIsSync);
}


void V0CustomElementScheduler::callbackDispatcherDidFinish()
{
    if (V0CustomElementMicrotaskDispatcher::instance().elementQueueIsEmpty())
        callbackQueues().clear();
}

void V0CustomElementScheduler::microtaskDispatcherDidFinish()
{
    DCHECK(!V0CustomElementProcessingStack::inCallbackDeliveryScope());
    callbackQueues().clear();
}

} // namespace blink
