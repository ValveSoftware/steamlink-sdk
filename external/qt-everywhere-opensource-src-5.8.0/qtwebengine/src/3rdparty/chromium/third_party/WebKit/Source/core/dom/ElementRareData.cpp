/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "core/dom/ElementRareData.h"

#include "core/css/cssom/InlineStylePropertyMap.h"
#include "core/dom/CompositorProxiedPropertySet.h"
#include "core/style/ComputedStyle.h"

namespace blink {

struct SameSizeAsElementRareData : NodeRareData {
    short indices[1];
    LayoutSize sizeForResizing;
    IntSize scrollOffset;
    void* pointers[10];
    Member<void*> persistentMember[3];
};

CSSStyleDeclaration& ElementRareData::ensureInlineCSSStyleDeclaration(Element* ownerElement)
{
    if (!m_cssomWrapper)
        m_cssomWrapper = new InlineCSSStyleDeclaration(ownerElement);
    return *m_cssomWrapper;
}

InlineStylePropertyMap& ElementRareData::ensureInlineStylePropertyMap(Element* ownerElement)
{
    if (!m_cssomMapWrapper) {
        m_cssomMapWrapper = new InlineStylePropertyMap(ownerElement);
    }
    return *m_cssomMapWrapper;
}

AttrNodeList& ElementRareData::ensureAttrNodeList()
{
    if (!m_attrNodeList)
        m_attrNodeList = new AttrNodeList;
    return *m_attrNodeList;
}

DEFINE_TRACE_AFTER_DISPATCH(ElementRareData)
{
    visitor->trace(m_dataset);
    visitor->trace(m_classList);
    visitor->trace(m_shadow);
    visitor->trace(m_attributeMap);
    visitor->trace(m_attrNodeList);
    visitor->trace(m_elementAnimations);
    visitor->trace(m_cssomWrapper);
    visitor->trace(m_cssomMapWrapper);
    visitor->trace(m_pseudoElementData);
    visitor->trace(m_customElementDefinition);
    visitor->trace(m_intersectionObserverData);
    NodeRareData::traceAfterDispatch(visitor);
}

DEFINE_TRACE_WRAPPERS_AFTER_DISPATCH(ElementRareData)
{
    if (m_attrNodeList.get()) {
        for (auto& attr : *m_attrNodeList) {
            visitor->traceWrappers(attr);
        }
    }
    visitor->traceWrappers(m_shadow);
    visitor->traceWrappers(m_attributeMap);
    visitor->traceWrappers(m_dataset);
    visitor->traceWrappers(m_classList);
    visitor->traceWrappers(m_intersectionObserverData);
    NodeRareData::traceWrappersAfterDispatch(visitor);
}

static_assert(sizeof(ElementRareData) == sizeof(SameSizeAsElementRareData), "ElementRareData should stay small");

} // namespace blink
