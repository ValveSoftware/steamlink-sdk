/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "core/dom/custom/V0CustomElementUpgradeCandidateMap.h"

#include "core/dom/Element.h"

namespace blink {

V0CustomElementUpgradeCandidateMap* V0CustomElementUpgradeCandidateMap::create()
{
    return new V0CustomElementUpgradeCandidateMap();
}

V0CustomElementUpgradeCandidateMap::~V0CustomElementUpgradeCandidateMap()
{
}

void V0CustomElementUpgradeCandidateMap::add(const V0CustomElementDescriptor& descriptor, Element* element)
{
    observe(element);

    UpgradeCandidateMap::AddResult result = m_upgradeCandidates.add(element, descriptor);
    ASSERT_UNUSED(result, result.isNewEntry);

    UnresolvedDefinitionMap::iterator it = m_unresolvedDefinitions.find(descriptor);
    ElementSet* elements;
    if (it == m_unresolvedDefinitions.end())
        elements = m_unresolvedDefinitions.add(descriptor, new ElementSet()).storedValue->value.get();
    else
        elements = it->value.get();
    elements->add(element);
}

void V0CustomElementUpgradeCandidateMap::elementWasDestroyed(Element* element)
{
    V0CustomElementObserver::elementWasDestroyed(element);
    UpgradeCandidateMap::iterator candidate = m_upgradeCandidates.find(element);
    ASSERT_WITH_SECURITY_IMPLICATION(candidate != m_upgradeCandidates.end());

    UnresolvedDefinitionMap::iterator elements = m_unresolvedDefinitions.find(candidate->value);
    ASSERT_WITH_SECURITY_IMPLICATION(elements != m_unresolvedDefinitions.end());
    elements->value->remove(element);
    m_upgradeCandidates.remove(candidate);
}

V0CustomElementUpgradeCandidateMap::ElementSet* V0CustomElementUpgradeCandidateMap::takeUpgradeCandidatesFor(const V0CustomElementDescriptor& descriptor)
{
    ElementSet* candidates = m_unresolvedDefinitions.take(descriptor);

    if (!candidates)
        return nullptr;

    for (const auto& candidate : *candidates) {
        unobserve(candidate);
        m_upgradeCandidates.remove(candidate);
    }
    return candidates;
}

DEFINE_TRACE(V0CustomElementUpgradeCandidateMap)
{
    visitor->trace(m_upgradeCandidates);
    visitor->trace(m_unresolvedDefinitions);
    V0CustomElementObserver::trace(visitor);
}

} // namespace blink
