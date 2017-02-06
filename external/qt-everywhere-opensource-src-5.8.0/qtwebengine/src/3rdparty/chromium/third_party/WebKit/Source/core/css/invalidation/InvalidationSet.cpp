/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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

#include "core/css/invalidation/InvalidationSet.h"

#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Element.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "platform/TracedValue.h"
#include "wtf/Compiler.h"
#include "wtf/PtrUtil.h"
#include "wtf/text/StringBuilder.h"
#include <memory>

namespace blink {

static const unsigned char* s_tracingEnabled = nullptr;

#define TRACE_STYLE_INVALIDATOR_INVALIDATION_SELECTORPART_IF_ENABLED(element, reason, invalidationSet, singleSelectorPart) \
    if (UNLIKELY(*s_tracingEnabled)) \
        TRACE_STYLE_INVALIDATOR_INVALIDATION_SELECTORPART(element, reason, invalidationSet, singleSelectorPart);

void InvalidationSet::cacheTracingFlag()
{
    s_tracingEnabled = TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(TRACE_DISABLED_BY_DEFAULT("devtools.timeline.invalidationTracking"));
}

InvalidationSet::InvalidationSet(InvalidationType type)
    : m_type(type)
    , m_allDescendantsMightBeInvalid(false)
    , m_invalidatesSelf(false)
    , m_customPseudoInvalid(false)
    , m_treeBoundaryCrossing(false)
    , m_insertionPointCrossing(false)
    , m_invalidatesSlotted(false)
{
}

bool InvalidationSet::invalidatesElement(Element& element) const
{
    if (m_allDescendantsMightBeInvalid)
        return true;

    if (m_tagNames && m_tagNames->contains(element.tagQName().localName())) {
        TRACE_STYLE_INVALIDATOR_INVALIDATION_SELECTORPART_IF_ENABLED(element, InvalidationSetMatchedTagName, *this, element.tagQName().localName());
        return true;
    }

    if (element.hasID() && m_ids && m_ids->contains(element.idForStyleResolution())) {
        TRACE_STYLE_INVALIDATOR_INVALIDATION_SELECTORPART_IF_ENABLED(element, InvalidationSetMatchedId, *this, element.idForStyleResolution());
        return true;
    }

    if (element.hasClass() && m_classes) {
        const SpaceSplitString& classNames = element.classNames();
        for (const auto& className : *m_classes) {
            if (classNames.contains(className)) {
                TRACE_STYLE_INVALIDATOR_INVALIDATION_SELECTORPART_IF_ENABLED(element, InvalidationSetMatchedClass, *this, className);
                return true;
            }
        }
    }

    if (element.hasAttributes() && m_attributes) {
        for (const auto& attribute : *m_attributes) {
            if (element.hasAttribute(attribute)) {
                TRACE_STYLE_INVALIDATOR_INVALIDATION_SELECTORPART_IF_ENABLED(element, InvalidationSetMatchedAttribute, *this, attribute);
                return true;
            }
        }
    }

    return false;
}

void InvalidationSet::combine(const InvalidationSet& other)
{
    ASSERT(type() == other.type());
    if (type() == InvalidateSiblings) {
        SiblingInvalidationSet& siblings = toSiblingInvalidationSet(*this);
        const SiblingInvalidationSet& otherSiblings = toSiblingInvalidationSet(other);

        siblings.updateMaxDirectAdjacentSelectors(otherSiblings.maxDirectAdjacentSelectors());
        if (otherSiblings.siblingDescendants())
            siblings.ensureSiblingDescendants().combine(*otherSiblings.siblingDescendants());
        if (otherSiblings.descendants())
            siblings.ensureDescendants().combine(*otherSiblings.descendants());
    }

    if (other.invalidatesSelf())
        setInvalidatesSelf();

    // No longer bother combining data structures, since the whole subtree is deemed invalid.
    if (wholeSubtreeInvalid())
        return;

    if (other.wholeSubtreeInvalid()) {
        setWholeSubtreeInvalid();
        return;
    }

    if (other.customPseudoInvalid())
        setCustomPseudoInvalid();

    if (other.treeBoundaryCrossing())
        setTreeBoundaryCrossing();

    if (other.insertionPointCrossing())
        setInsertionPointCrossing();

    if (other.invalidatesSlotted())
        setInvalidatesSlotted();

    if (other.m_classes) {
        for (const auto& className : *other.m_classes)
            addClass(className);
    }

    if (other.m_ids) {
        for (const auto& id : *other.m_ids)
            addId(id);
    }

    if (other.m_tagNames) {
        for (const auto& tagName : *other.m_tagNames)
            addTagName(tagName);
    }

    if (other.m_attributes) {
        for (const auto& attribute : *other.m_attributes)
            addAttribute(attribute);
    }
}

void InvalidationSet::destroy()
{
    if (isDescendantInvalidationSet())
        delete toDescendantInvalidationSet(this);
    else
        delete toSiblingInvalidationSet(this);
}

HashSet<AtomicString>& InvalidationSet::ensureClassSet()
{
    if (!m_classes)
        m_classes = wrapUnique(new HashSet<AtomicString>);
    return *m_classes;
}

HashSet<AtomicString>& InvalidationSet::ensureIdSet()
{
    if (!m_ids)
        m_ids = wrapUnique(new HashSet<AtomicString>);
    return *m_ids;
}

HashSet<AtomicString>& InvalidationSet::ensureTagNameSet()
{
    if (!m_tagNames)
        m_tagNames = wrapUnique(new HashSet<AtomicString>);
    return *m_tagNames;
}

HashSet<AtomicString>& InvalidationSet::ensureAttributeSet()
{
    if (!m_attributes)
        m_attributes = wrapUnique(new HashSet<AtomicString>);
    return *m_attributes;
}

void InvalidationSet::addClass(const AtomicString& className)
{
    if (wholeSubtreeInvalid())
        return;
    ensureClassSet().add(className);
}

void InvalidationSet::addId(const AtomicString& id)
{
    if (wholeSubtreeInvalid())
        return;
    ensureIdSet().add(id);
}

void InvalidationSet::addTagName(const AtomicString& tagName)
{
    if (wholeSubtreeInvalid())
        return;
    ensureTagNameSet().add(tagName);
}

void InvalidationSet::addAttribute(const AtomicString& attribute)
{
    if (wholeSubtreeInvalid())
        return;
    ensureAttributeSet().add(attribute);
}

void InvalidationSet::setWholeSubtreeInvalid()
{
    if (m_allDescendantsMightBeInvalid)
        return;

    m_allDescendantsMightBeInvalid = true;
    m_customPseudoInvalid = false;
    m_treeBoundaryCrossing = false;
    m_insertionPointCrossing = false;
    m_invalidatesSlotted = false;
    m_classes = nullptr;
    m_ids = nullptr;
    m_tagNames = nullptr;
    m_attributes = nullptr;
}

void InvalidationSet::toTracedValue(TracedValue* value) const
{
    value->beginDictionary();

    value->setString("id", descendantInvalidationSetToIdString(*this));

    if (m_allDescendantsMightBeInvalid)
        value->setBoolean("allDescendantsMightBeInvalid", true);
    if (m_customPseudoInvalid)
        value->setBoolean("customPseudoInvalid", true);
    if (m_treeBoundaryCrossing)
        value->setBoolean("treeBoundaryCrossing", true);
    if (m_insertionPointCrossing)
        value->setBoolean("insertionPointCrossing", true);
    if (m_invalidatesSlotted)
        value->setBoolean("invalidatesSlotted", true);

    if (m_ids) {
        value->beginArray("ids");
        for (const auto& id : *m_ids)
            value->pushString(id);
        value->endArray();
    }

    if (m_classes) {
        value->beginArray("classes");
        for (const auto& className : *m_classes)
            value->pushString(className);
        value->endArray();
    }

    if (m_tagNames) {
        value->beginArray("tagNames");
        for (const auto& tagName : *m_tagNames)
            value->pushString(tagName);
        value->endArray();
    }

    if (m_attributes) {
        value->beginArray("attributes");
        for (const auto& attribute : *m_attributes)
            value->pushString(attribute);
        value->endArray();
    }

    value->endDictionary();
}

#ifndef NDEBUG
void InvalidationSet::show() const
{
    std::unique_ptr<TracedValue> value = TracedValue::create();
    value->beginArray("InvalidationSet");
    toTracedValue(value.get());
    value->endArray();
    fprintf(stderr, "%s\n", value->toString().ascii().data());
}
#endif // NDEBUG

SiblingInvalidationSet::SiblingInvalidationSet(PassRefPtr<DescendantInvalidationSet> descendants)
    : InvalidationSet(InvalidateSiblings)
    , m_maxDirectAdjacentSelectors(1)
    , m_descendantInvalidationSet(descendants)
{
}

DescendantInvalidationSet& SiblingInvalidationSet::ensureSiblingDescendants()
{
    if (!m_siblingDescendantInvalidationSet)
        m_siblingDescendantInvalidationSet = DescendantInvalidationSet::create();
    return *m_siblingDescendantInvalidationSet;
}

DescendantInvalidationSet& SiblingInvalidationSet::ensureDescendants()
{
    if (!m_descendantInvalidationSet)
        m_descendantInvalidationSet = DescendantInvalidationSet::create();
    return *m_descendantInvalidationSet;
}

} // namespace blink
