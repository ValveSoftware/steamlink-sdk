// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/RelList.h"

#include "core/dom/Document.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "wtf/HashMap.h"

namespace blink {

using namespace HTMLNames;


RelList::RelList(Element* element) : DOMTokenList(nullptr), m_element(element) { }

unsigned RelList::length() const
{
    return !m_element->fastGetAttribute(relAttr).isEmpty() ? m_relValues.size() : 0;
}

const AtomicString RelList::item(unsigned index) const
{
    if (index >= length())
        return AtomicString();
    return m_relValues[index];
}

bool RelList::containsInternal(const AtomicString& token) const
{
    return !m_element->fastGetAttribute(relAttr).isEmpty() && m_relValues.contains(token);
}

void RelList::setRelValues(const AtomicString& value)
{
    m_relValues.set(value, SpaceSplitString::ShouldNotFoldCase);
}

static RelList::SupportedTokens& supportedTokens()
{
    DEFINE_STATIC_LOCAL(RelList::SupportedTokens, supportedValuesMap, ());
    if (supportedValuesMap.isEmpty()) {
        supportedValuesMap.add("preload");
        supportedValuesMap.add("preconnect");
        supportedValuesMap.add("dns-prefetch");
        supportedValuesMap.add("stylesheet");
        supportedValuesMap.add("import");
        supportedValuesMap.add("icon");
        supportedValuesMap.add("alternate");
        supportedValuesMap.add("prefetch");
        supportedValuesMap.add("prerender");
        supportedValuesMap.add("next");
        supportedValuesMap.add("manifest");
        supportedValuesMap.add("apple-touch-icon");
        supportedValuesMap.add("apple-touch-icon-precomposed");
        if (RuntimeEnabledFeatures::linkServiceWorkerEnabled())
            supportedValuesMap.add("serviceworker");
    }

    return supportedValuesMap;
}

bool RelList::validateTokenValue(const AtomicString& tokenValue, ExceptionState&) const
{
    return supportedTokens().contains(tokenValue);
}

DEFINE_TRACE(RelList)
{
    visitor->trace(m_element);
    DOMTokenList::trace(visitor);
}

} // namespace blink
