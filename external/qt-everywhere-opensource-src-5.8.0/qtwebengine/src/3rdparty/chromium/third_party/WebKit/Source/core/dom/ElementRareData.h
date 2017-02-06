/*
 * Copyright (C) 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2008 David Smith <catfish.man@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef ElementRareData_h
#define ElementRareData_h

#include "core/animation/ElementAnimations.h"
#include "core/css/cssom/InlineStylePropertyMap.h"
#include "core/dom/Attr.h"
#include "core/dom/CompositorProxiedPropertySet.h"
#include "core/dom/DatasetDOMStringMap.h"
#include "core/dom/NamedNodeMap.h"
#include "core/dom/NodeIntersectionObserverData.h"
#include "core/dom/NodeRareData.h"
#include "core/dom/PseudoElement.h"
#include "core/dom/PseudoElementData.h"
#include "core/dom/custom/V0CustomElementDefinition.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/html/ClassList.h"
#include "core/style/StyleInheritedData.h"
#include "platform/heap/Handle.h"
#include "wtf/HashSet.h"
#include <memory>

namespace blink {

class HTMLElement;
class CompositorProxiedPropertySet;

class ElementRareData : public NodeRareData {
public:
    static ElementRareData* create(LayoutObject* layoutObject)
    {
        return new ElementRareData(layoutObject);
    }

    ~ElementRareData();

    void setPseudoElement(PseudoId, PseudoElement*);
    PseudoElement* pseudoElement(PseudoId) const;

    short tabIndex() const { return m_tabindex; }

    void setTabIndexExplicitly(short index)
    {
        m_tabindex = index;
        setElementFlag(TabIndexWasSetExplicitly, true);
    }

    void clearTabIndexExplicitly()
    {
        m_tabindex = 0;
        clearElementFlag(TabIndexWasSetExplicitly);
    }

    CSSStyleDeclaration& ensureInlineCSSStyleDeclaration(Element* ownerElement);
    InlineStylePropertyMap& ensureInlineStylePropertyMap(Element* ownerElement);

    InlineStylePropertyMap* inlineStylePropertyMap() { return m_cssomMapWrapper.get(); }

    void clearShadow() { m_shadow = nullptr; }
    ElementShadow* shadow() const { return m_shadow.get(); }
    ElementShadow& ensureShadow()
    {
        if (!m_shadow)
            m_shadow = ElementShadow::create();
        return *m_shadow;
    }

    NamedNodeMap* attributeMap() const { return m_attributeMap.get(); }
    void setAttributeMap(NamedNodeMap* attributeMap) { m_attributeMap = attributeMap; }

    ComputedStyle* ensureComputedStyle() const { return m_computedStyle.get(); }
    void setComputedStyle(PassRefPtr<ComputedStyle> computedStyle) { m_computedStyle = computedStyle; }
    void clearComputedStyle() { m_computedStyle = nullptr; }

    ClassList* classList() const { return m_classList.get(); }
    void setClassList(ClassList* classList) { m_classList = classList; }
    void clearClassListValueForQuirksMode()
    {
        if (!m_classList)
            return;
        m_classList->clearValueForQuirksMode();
    }

    DatasetDOMStringMap* dataset() const { return m_dataset.get(); }
    void setDataset(DatasetDOMStringMap* dataset) { m_dataset = dataset; }

    LayoutSize minimumSizeForResizing() const { return m_minimumSizeForResizing; }
    void setMinimumSizeForResizing(LayoutSize size) { m_minimumSizeForResizing = size; }

    IntSize savedLayerScrollOffset() const { return m_savedLayerScrollOffset; }
    void setSavedLayerScrollOffset(IntSize size) { m_savedLayerScrollOffset = size; }

    ElementAnimations* elementAnimations() { return m_elementAnimations.get(); }
    void setElementAnimations(ElementAnimations* elementAnimations)
    {
        m_elementAnimations = elementAnimations;
    }

    bool hasPseudoElements() const;
    void clearPseudoElements();

    void incrementCompositorProxiedProperties(uint32_t properties);
    void decrementCompositorProxiedProperties(uint32_t properties);
    CompositorProxiedPropertySet* proxiedPropertyCounts() const { return m_proxiedProperties.get(); }

    void setCustomElementDefinition(V0CustomElementDefinition* definition) { m_customElementDefinition = definition; }
    V0CustomElementDefinition* customElementDefinition() const { return m_customElementDefinition.get(); }

    AttrNodeList& ensureAttrNodeList();
    AttrNodeList* attrNodeList() { return m_attrNodeList.get(); }
    void removeAttrNodeList() { m_attrNodeList.clear(); }

    NodeIntersectionObserverData* intersectionObserverData() const { return m_intersectionObserverData.get(); }
    NodeIntersectionObserverData& ensureIntersectionObserverData()
    {
        if (!m_intersectionObserverData)
            m_intersectionObserverData = new NodeIntersectionObserverData();
        return *m_intersectionObserverData;
    }

    DECLARE_TRACE_AFTER_DISPATCH();

    DECLARE_TRACE_WRAPPERS_AFTER_DISPATCH();

private:
    CompositorProxiedPropertySet& ensureCompositorProxiedPropertySet();
    void clearCompositorProxiedPropertySet() { m_proxiedProperties = nullptr; }

    short m_tabindex;

    LayoutSize m_minimumSizeForResizing;
    IntSize m_savedLayerScrollOffset;

    Member<DatasetDOMStringMap> m_dataset;
    Member<ElementShadow> m_shadow;
    Member<ClassList> m_classList;
    Member<NamedNodeMap> m_attributeMap;
    Member<AttrNodeList> m_attrNodeList;
    Member<InlineCSSStyleDeclaration> m_cssomWrapper;
    Member<InlineStylePropertyMap> m_cssomMapWrapper;
    std::unique_ptr<CompositorProxiedPropertySet> m_proxiedProperties;

    Member<ElementAnimations> m_elementAnimations;
    Member<NodeIntersectionObserverData> m_intersectionObserverData;

    RefPtr<ComputedStyle> m_computedStyle;
    Member<V0CustomElementDefinition> m_customElementDefinition;

    Member<PseudoElementData> m_pseudoElementData;

    explicit ElementRareData(LayoutObject*);
};

inline LayoutSize defaultMinimumSizeForResizing()
{
    return LayoutSize(LayoutUnit::max(), LayoutUnit::max());
}

inline ElementRareData::ElementRareData(LayoutObject* layoutObject)
    : NodeRareData(layoutObject)
    , m_tabindex(0)
    , m_minimumSizeForResizing(defaultMinimumSizeForResizing())
    , m_classList(nullptr)
{
    m_isElementRareData = true;
}

inline ElementRareData::~ElementRareData()
{
    DCHECK(!m_pseudoElementData);
}

inline bool ElementRareData::hasPseudoElements() const
{
    return (m_pseudoElementData && m_pseudoElementData->hasPseudoElements());
}

inline void ElementRareData::clearPseudoElements()
{
    if (m_pseudoElementData) {
        m_pseudoElementData->clearPseudoElements();
        m_pseudoElementData.clear();
    }
}

inline void ElementRareData::setPseudoElement(PseudoId pseudoId, PseudoElement* element)
{
    if (!m_pseudoElementData)
        m_pseudoElementData = PseudoElementData::create();
    m_pseudoElementData->setPseudoElement(pseudoId, element);
}

inline PseudoElement* ElementRareData::pseudoElement(PseudoId pseudoId) const
{
    if (!m_pseudoElementData)
        return nullptr;
    return m_pseudoElementData->pseudoElement(pseudoId);
}

inline void ElementRareData::incrementCompositorProxiedProperties(uint32_t properties)
{
    ensureCompositorProxiedPropertySet().increment(properties);
}

inline void ElementRareData::decrementCompositorProxiedProperties(uint32_t properties)
{
    m_proxiedProperties->decrement(properties);
    if (m_proxiedProperties->isEmpty())
        clearCompositorProxiedPropertySet();
}

inline CompositorProxiedPropertySet& ElementRareData::ensureCompositorProxiedPropertySet()
{
    if (!m_proxiedProperties)
        m_proxiedProperties = CompositorProxiedPropertySet::create();
    return *m_proxiedProperties;
}

} // namespace blink

#endif // ElementRareData_h
