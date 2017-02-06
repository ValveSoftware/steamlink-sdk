/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "core/dom/MutationObserver.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/Microtask.h"
#include "core/dom/MutationCallback.h"
#include "core/dom/MutationObserverInit.h"
#include "core/dom/MutationObserverRegistration.h"
#include "core/dom/MutationRecord.h"
#include "core/dom/Node.h"
#include "core/inspector/InspectorInstrumentation.h"
#include <algorithm>

namespace blink {

static unsigned s_observerPriority = 0;

struct MutationObserver::ObserverLessThan {
    bool operator()(const Member<MutationObserver>& lhs, const Member<MutationObserver>& rhs)
    {
        return lhs->m_priority < rhs->m_priority;
    }
};

MutationObserver* MutationObserver::create(MutationCallback* callback)
{
    DCHECK(isMainThread());
    return new MutationObserver(callback);
}

MutationObserver::MutationObserver(MutationCallback* callback)
    : m_callback(callback)
    , m_priority(s_observerPriority++)
{
}

MutationObserver::~MutationObserver()
{
    cancelInspectorAsyncTasks();
}

void MutationObserver::observe(Node* node, const MutationObserverInit& observerInit, ExceptionState& exceptionState)
{
    DCHECK(node);

    MutationObserverOptions options = 0;

    if (observerInit.hasAttributeOldValue() && observerInit.attributeOldValue())
        options |= AttributeOldValue;

    HashSet<AtomicString> attributeFilter;
    if (observerInit.hasAttributeFilter()) {
        const Vector<String>& sequence = observerInit.attributeFilter();
        for (unsigned i = 0; i < sequence.size(); ++i)
            attributeFilter.add(AtomicString(sequence[i]));
        options |= AttributeFilter;
    }

    bool attributes = observerInit.hasAttributes() && observerInit.attributes();
    if (attributes || (!observerInit.hasAttributes() && (observerInit.hasAttributeOldValue() || observerInit.hasAttributeFilter())))
        options |= Attributes;

    if (observerInit.hasCharacterDataOldValue() && observerInit.characterDataOldValue())
        options |= CharacterDataOldValue;

    bool characterData = observerInit.hasCharacterData() && observerInit.characterData();
    if (characterData || (!observerInit.hasCharacterData() && observerInit.hasCharacterDataOldValue()))
        options |= CharacterData;

    if (observerInit.childList())
        options |= ChildList;

    if (observerInit.subtree())
        options |= Subtree;

    if (!(options & Attributes)) {
        if (options & AttributeOldValue) {
            exceptionState.throwTypeError("The options object may only set 'attributeOldValue' to true when 'attributes' is true or not present.");
            return;
        }
        if (options & AttributeFilter) {
            exceptionState.throwTypeError("The options object may only set 'attributeFilter' when 'attributes' is true or not present.");
            return;
        }
    }
    if (!((options & CharacterData) || !(options & CharacterDataOldValue))) {
        exceptionState.throwTypeError("The options object may only set 'characterDataOldValue' to true when 'characterData' is true or not present.");
        return;
    }

    if (!(options & (Attributes | CharacterData | ChildList))) {
        exceptionState.throwTypeError("The options object must set at least one of 'attributes', 'characterData', or 'childList' to true.");
        return;
    }

    node->registerMutationObserver(*this, options, attributeFilter);
}

MutationRecordVector MutationObserver::takeRecords()
{
    MutationRecordVector records;
    cancelInspectorAsyncTasks();
    records.swap(m_records);
    return records;
}

void MutationObserver::disconnect()
{
    cancelInspectorAsyncTasks();
    m_records.clear();
    MutationObserverRegistrationSet registrations(m_registrations);
    for (auto& registration : registrations) {
        // The registration may be already unregistered while iteration.
        // Only call unregister if it is still in the original set.
        if (m_registrations.contains(registration))
            registration->unregister();
    }
    DCHECK(m_registrations.isEmpty());
}

void MutationObserver::observationStarted(MutationObserverRegistration* registration)
{
    DCHECK(!m_registrations.contains(registration));
    m_registrations.add(registration);
}

void MutationObserver::observationEnded(MutationObserverRegistration* registration)
{
    DCHECK(m_registrations.contains(registration));
    m_registrations.remove(registration);
}

static MutationObserverSet& activeMutationObservers()
{
    DEFINE_STATIC_LOCAL(MutationObserverSet, activeObservers, (new MutationObserverSet));
    return activeObservers;
}

static MutationObserverSet& suspendedMutationObservers()
{
    DEFINE_STATIC_LOCAL(MutationObserverSet, suspendedObservers, (new MutationObserverSet));
    return suspendedObservers;
}

static void activateObserver(MutationObserver* observer)
{
    if (activeMutationObservers().isEmpty())
        Microtask::enqueueMicrotask(WTF::bind(&MutationObserver::deliverMutations));

    activeMutationObservers().add(observer);
}

void MutationObserver::enqueueMutationRecord(MutationRecord* mutation)
{
    DCHECK(isMainThread());
    m_records.append(mutation);
    activateObserver(this);
    InspectorInstrumentation::asyncTaskScheduled(m_callback->getExecutionContext(), mutation->type(), mutation);
}

void MutationObserver::setHasTransientRegistration()
{
    DCHECK(isMainThread());
    activateObserver(this);
}

HeapHashSet<Member<Node>> MutationObserver::getObservedNodes() const
{
    HeapHashSet<Member<Node>> observedNodes;
    for (const auto& registration : m_registrations)
        registration->addRegistrationNodesToSet(observedNodes);
    return observedNodes;
}

bool MutationObserver::shouldBeSuspended() const
{
    return m_callback->getExecutionContext() && m_callback->getExecutionContext()->activeDOMObjectsAreSuspended();
}

void MutationObserver::cancelInspectorAsyncTasks()
{
    for (auto& record : m_records)
        InspectorInstrumentation::asyncTaskCanceled(m_callback->getExecutionContext(), record);
}

void MutationObserver::deliver()
{
    DCHECK(!shouldBeSuspended());

    // Calling clearTransientRegistrations() can modify m_registrations, so it's necessary
    // to make a copy of the transient registrations before operating on them.
    HeapVector<Member<MutationObserverRegistration>, 1> transientRegistrations;
    for (auto& registration : m_registrations) {
        if (registration->hasTransientRegistrations())
            transientRegistrations.append(registration);
    }
    for (size_t i = 0; i < transientRegistrations.size(); ++i)
        transientRegistrations[i]->clearTransientRegistrations();

    if (m_records.isEmpty())
        return;

    MutationRecordVector records;
    records.swap(m_records);

    // Report the first (earliest) stack as the async cause.
    InspectorInstrumentation::AsyncTask asyncTask(m_callback->getExecutionContext(), records.first());
    m_callback->call(records, this);
}

void MutationObserver::resumeSuspendedObservers()
{
    DCHECK(isMainThread());
    if (suspendedMutationObservers().isEmpty())
        return;

    MutationObserverVector suspended;
    copyToVector(suspendedMutationObservers(), suspended);
    for (size_t i = 0; i < suspended.size(); ++i) {
        if (!suspended[i]->shouldBeSuspended()) {
            suspendedMutationObservers().remove(suspended[i]);
            activateObserver(suspended[i]);
        }
    }
}

void MutationObserver::deliverMutations()
{
    DCHECK(isMainThread());
    MutationObserverVector observers;
    copyToVector(activeMutationObservers(), observers);
    activeMutationObservers().clear();
    std::sort(observers.begin(), observers.end(), ObserverLessThan());
    for (size_t i = 0; i < observers.size(); ++i) {
        if (observers[i]->shouldBeSuspended())
            suspendedMutationObservers().add(observers[i]);
        else
            observers[i]->deliver();
    }
}

DEFINE_TRACE(MutationObserver)
{
    visitor->trace(m_callback);
    visitor->trace(m_records);
    visitor->trace(m_registrations);
    visitor->trace(m_callback);
}

} // namespace blink
