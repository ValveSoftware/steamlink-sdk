/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
 */

#include "core/css/RuleSet.h"

#include "core/HTMLNames.h"
#include "core/css/CSSFontSelector.h"
#include "core/css/CSSSelector.h"
#include "core/css/CSSSelectorList.h"
#include "core/css/SelectorFilter.h"
#include "core/css/StyleRuleImport.h"
#include "core/css/StyleSheetContents.h"
#include "core/html/track/TextTrackCue.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/TraceEvent.h"
#include "platform/heap/HeapTerminatedArrayBuilder.h"
#include "platform/weborigin/SecurityOrigin.h"

#include "wtf/TerminatedArrayBuilder.h"

namespace blink {

using namespace HTMLNames;

// -----------------------------------------------------------------

static bool containsUncommonAttributeSelector(const CSSSelector&);

static inline bool selectorListContainsUncommonAttributeSelector(const CSSSelector* selector)
{
    const CSSSelectorList* selectorList = selector->selectorList();
    if (!selectorList)
        return false;
    for (const CSSSelector* selector = selectorList->first(); selector; selector = CSSSelectorList::next(*selector)) {
        if (containsUncommonAttributeSelector(*selector))
            return true;
    }
    return false;
}

static inline bool isCommonAttributeSelectorAttribute(const QualifiedName& attribute)
{
    // These are explicitly tested for equality in canShareStyleWithElement.
    return attribute == typeAttr || attribute == readonlyAttr;
}

static bool containsUncommonAttributeSelector(const CSSSelector& selector)
{
    const CSSSelector* current = &selector;
    for (; current; current = current->tagHistory()) {
        // Allow certain common attributes (used in the default style) in the selectors that match the current element.
        if (current->isAttributeSelector() && !isCommonAttributeSelectorAttribute(current->attribute()))
            return true;
        if (selectorListContainsUncommonAttributeSelector(current))
            return true;
        if (current->relationIsAffectedByPseudoContent() || current->getPseudoType() == CSSSelector::PseudoSlotted)
            return false;
        if (current->relation() != CSSSelector::SubSelector) {
            current = current->tagHistory();
            break;
        }
    }

    for (; current; current = current->tagHistory()) {
        if (current->isAttributeSelector())
            return true;
        if (selectorListContainsUncommonAttributeSelector(current))
            return true;
    }
    return false;
}

static inline PropertyWhitelistType determinePropertyWhitelistType(const AddRuleFlags addRuleFlags, const CSSSelector& selector)
{
    for (const CSSSelector* component = &selector; component; component = component->tagHistory()) {
        if (component->getPseudoType() == CSSSelector::PseudoCue || (component->match() == CSSSelector::PseudoElement && component->value() == TextTrackCue::cueShadowPseudoId()))
            return PropertyWhitelistCue;
        if (component->getPseudoType() == CSSSelector::PseudoFirstLetter)
            return PropertyWhitelistFirstLetter;
    }
    return PropertyWhitelistNone;
}

RuleData::RuleData(StyleRule* rule, unsigned selectorIndex, unsigned position, AddRuleFlags addRuleFlags)
    : m_rule(rule)
    , m_selectorIndex(selectorIndex)
    , m_isLastInArray(false)
    , m_position(position)
    , m_specificity(selector().specificity())
    , m_containsUncommonAttributeSelector(blink::containsUncommonAttributeSelector(selector()))
    , m_linkMatchType(selector().computeLinkMatchType())
    , m_hasDocumentSecurityOrigin(addRuleFlags & RuleHasDocumentSecurityOrigin)
    , m_propertyWhitelist(determinePropertyWhitelistType(addRuleFlags, selector()))
{
    SelectorFilter::collectIdentifierHashes(selector(), m_descendantSelectorIdentifierHashes, maximumIdentifierCount);
}

void RuleSet::addToRuleSet(const AtomicString& key, PendingRuleMap& map, const RuleData& ruleData)
{
    Member<HeapLinkedStack<RuleData>>& rules = map.add(key, nullptr).storedValue->value;
    if (!rules)
        rules = new HeapLinkedStack<RuleData>;
    rules->push(ruleData);
}

static void extractValuesforSelector(const CSSSelector* selector, AtomicString& id, AtomicString& className, AtomicString& customPseudoElementName, AtomicString& tagName)
{
    switch (selector->match()) {
    case CSSSelector::Id:
        id = selector->value();
        break;
    case CSSSelector::Class:
        className = selector->value();
        break;
    case CSSSelector::Tag:
        if (selector->tagQName().localName() != starAtom)
            tagName = selector->tagQName().localName();
        break;
    default:
        break;
    }
    if (selector->getPseudoType() == CSSSelector::PseudoWebKitCustomElement || selector->getPseudoType() == CSSSelector::PseudoBlinkInternalElement)
        customPseudoElementName = selector->value();
}

bool RuleSet::findBestRuleSetAndAdd(const CSSSelector& component, RuleData& ruleData)
{
    AtomicString id;
    AtomicString className;
    AtomicString customPseudoElementName;
    AtomicString tagName;

#ifndef NDEBUG
    m_allRules.append(ruleData);
#endif

    const CSSSelector* it = &component;
    for (; it && it->relation() == CSSSelector::SubSelector; it = it->tagHistory())
        extractValuesforSelector(it, id, className, customPseudoElementName, tagName);
    if (it)
        extractValuesforSelector(it, id, className, customPseudoElementName, tagName);

    // Prefer rule sets in order of most likely to apply infrequently.
    if (!id.isEmpty()) {
        addToRuleSet(id, ensurePendingRules()->idRules, ruleData);
        return true;
    }
    if (!className.isEmpty()) {
        addToRuleSet(className, ensurePendingRules()->classRules, ruleData);
        return true;
    }
    if (!customPseudoElementName.isEmpty()) {
        // Custom pseudos come before ids and classes in the order of tagHistory, and have a relation of
        // ShadowPseudo between them. Therefore we should never be a situation where extractValuesforSelector
        // finsd id and className in addition to custom pseudo.
        ASSERT(id.isEmpty() && className.isEmpty());
        addToRuleSet(customPseudoElementName, ensurePendingRules()->shadowPseudoElementRules, ruleData);
        return true;
    }

    switch (component.getPseudoType()) {
    case CSSSelector::PseudoCue:
        m_cuePseudoRules.append(ruleData);
        return true;
    case CSSSelector::PseudoLink:
    case CSSSelector::PseudoVisited:
    case CSSSelector::PseudoAnyLink:
        m_linkPseudoClassRules.append(ruleData);
        return true;
    case CSSSelector::PseudoFocus:
        m_focusPseudoClassRules.append(ruleData);
        return true;
    default:
        break;
    }

    if (!tagName.isEmpty()) {
        addToRuleSet(tagName, ensurePendingRules()->tagRules, ruleData);
        return true;
    }

    if (component.isHostPseudoClass()) {
        m_shadowHostRules.append(ruleData);
        return true;
    }

    return false;
}

void RuleSet::addRule(StyleRule* rule, unsigned selectorIndex, AddRuleFlags addRuleFlags)
{
    RuleData ruleData(rule, selectorIndex, m_ruleCount++, addRuleFlags);
    if (m_features.collectFeaturesFromRuleData(ruleData) == RuleFeatureSet::SelectorNeverMatches)
        return;

    if (!findBestRuleSetAndAdd(ruleData.selector(), ruleData)) {
        // If we didn't find a specialized map to stick it in, file under universal rules.
        m_universalRules.append(ruleData);
    }
}

void RuleSet::addPageRule(StyleRulePage* rule)
{
    ensurePendingRules(); // So that m_pageRules.shrinkToFit() gets called.
    m_pageRules.append(rule);
}

void RuleSet::addViewportRule(StyleRuleViewport* rule)
{
    ensurePendingRules(); // So that m_viewportRules.shrinkToFit() gets called.
    m_viewportRules.append(rule);
}

void RuleSet::addFontFaceRule(StyleRuleFontFace* rule)
{
    ensurePendingRules(); // So that m_fontFaceRules.shrinkToFit() gets called.
    m_fontFaceRules.append(rule);
}

void RuleSet::addKeyframesRule(StyleRuleKeyframes* rule)
{
    ensurePendingRules(); // So that m_keyframesRules.shrinkToFit() gets called.
    m_keyframesRules.append(rule);
}

void RuleSet::addChildRules(const HeapVector<Member<StyleRuleBase>>& rules, const MediaQueryEvaluator& medium, AddRuleFlags addRuleFlags)
{
    for (unsigned i = 0; i < rules.size(); ++i) {
        StyleRuleBase* rule = rules[i].get();

        if (rule->isStyleRule()) {
            StyleRule* styleRule = toStyleRule(rule);

            const CSSSelectorList& selectorList = styleRule->selectorList();
            for (size_t selectorIndex = 0; selectorIndex != kNotFound; selectorIndex = selectorList.indexOfNextSelectorAfter(selectorIndex)) {
                if (selectorList.selectorUsesDeepCombinatorOrShadowPseudo(selectorIndex)) {
                    m_deepCombinatorOrShadowPseudoRules.append(MinimalRuleData(styleRule, selectorIndex, addRuleFlags));
                } else if (selectorList.selectorHasContentPseudo(selectorIndex)) {
                    m_contentPseudoElementRules.append(MinimalRuleData(styleRule, selectorIndex, addRuleFlags));
                } else if (selectorList.selectorHasSlottedPseudo(selectorIndex)) {
                    m_slottedPseudoElementRules.append(MinimalRuleData(styleRule, selectorIndex, addRuleFlags));
                } else {
                    addRule(styleRule, selectorIndex, addRuleFlags);
                }
            }
        } else if (rule->isPageRule()) {
            addPageRule(toStyleRulePage(rule));
        } else if (rule->isMediaRule()) {
            StyleRuleMedia* mediaRule = toStyleRuleMedia(rule);
            if ((!mediaRule->mediaQueries() || medium.eval(mediaRule->mediaQueries(), &m_viewportDependentMediaQueryResults, &m_deviceDependentMediaQueryResults)))
                addChildRules(mediaRule->childRules(), medium, addRuleFlags);
        } else if (rule->isFontFaceRule()) {
            addFontFaceRule(toStyleRuleFontFace(rule));
        } else if (rule->isKeyframesRule()) {
            addKeyframesRule(toStyleRuleKeyframes(rule));
        } else if (rule->isViewportRule()) {
            addViewportRule(toStyleRuleViewport(rule));
        } else if (rule->isSupportsRule() && toStyleRuleSupports(rule)->conditionIsSupported()) {
            addChildRules(toStyleRuleSupports(rule)->childRules(), medium, addRuleFlags);
        }
    }
}

void RuleSet::addRulesFromSheet(StyleSheetContents* sheet, const MediaQueryEvaluator& medium, AddRuleFlags addRuleFlags)
{
    TRACE_EVENT0("blink", "RuleSet::addRulesFromSheet");

    ASSERT(sheet);

    const HeapVector<Member<StyleRuleImport>>& importRules = sheet->importRules();
    for (unsigned i = 0; i < importRules.size(); ++i) {
        StyleRuleImport* importRule = importRules[i].get();
        if (importRule->styleSheet() && (!importRule->mediaQueries() || medium.eval(importRule->mediaQueries(), &m_viewportDependentMediaQueryResults, &m_deviceDependentMediaQueryResults)))
            addRulesFromSheet(importRule->styleSheet(), medium, addRuleFlags);
    }

    addChildRules(sheet->childRules(), medium, addRuleFlags);
}

void RuleSet::addStyleRule(StyleRule* rule, AddRuleFlags addRuleFlags)
{
    for (size_t selectorIndex = 0; selectorIndex != kNotFound; selectorIndex = rule->selectorList().indexOfNextSelectorAfter(selectorIndex))
        addRule(rule, selectorIndex, addRuleFlags);
}

void RuleSet::compactPendingRules(PendingRuleMap& pendingMap, CompactRuleMap& compactMap)
{
    for (auto& item : pendingMap) {
        HeapLinkedStack<RuleData>* pendingRules = item.value.release();
        CompactRuleMap::ValueType* compactRules = compactMap.add(item.key, nullptr).storedValue;

        HeapTerminatedArrayBuilder<RuleData> builder(compactRules->value.release());
        builder.grow(pendingRules->size());
        while (!pendingRules->isEmpty()) {
            builder.append(pendingRules->peek());
            pendingRules->pop();
        }

        compactRules->value = builder.release();
    }
}

void RuleSet::compactRules()
{
    ASSERT(m_pendingRules);
    PendingRuleMaps* pendingRules = m_pendingRules.release();
    compactPendingRules(pendingRules->idRules, m_idRules);
    compactPendingRules(pendingRules->classRules, m_classRules);
    compactPendingRules(pendingRules->tagRules, m_tagRules);
    compactPendingRules(pendingRules->shadowPseudoElementRules, m_shadowPseudoElementRules);
    m_linkPseudoClassRules.shrinkToFit();
    m_cuePseudoRules.shrinkToFit();
    m_focusPseudoClassRules.shrinkToFit();
    m_universalRules.shrinkToFit();
    m_shadowHostRules.shrinkToFit();
    m_pageRules.shrinkToFit();
    m_viewportRules.shrinkToFit();
    m_fontFaceRules.shrinkToFit();
    m_keyframesRules.shrinkToFit();
    m_deepCombinatorOrShadowPseudoRules.shrinkToFit();
    m_contentPseudoElementRules.shrinkToFit();
    m_slottedPseudoElementRules.shrinkToFit();
}

DEFINE_TRACE(MinimalRuleData)
{
    visitor->trace(m_rule);
}

DEFINE_TRACE(RuleData)
{
    visitor->trace(m_rule);
}

DEFINE_TRACE(RuleSet::PendingRuleMaps)
{
    visitor->trace(idRules);
    visitor->trace(classRules);
    visitor->trace(tagRules);
    visitor->trace(shadowPseudoElementRules);
}

DEFINE_TRACE(RuleSet)
{
    visitor->trace(m_idRules);
    visitor->trace(m_classRules);
    visitor->trace(m_tagRules);
    visitor->trace(m_shadowPseudoElementRules);
    visitor->trace(m_linkPseudoClassRules);
    visitor->trace(m_cuePseudoRules);
    visitor->trace(m_focusPseudoClassRules);
    visitor->trace(m_universalRules);
    visitor->trace(m_shadowHostRules);
    visitor->trace(m_features);
    visitor->trace(m_pageRules);
    visitor->trace(m_viewportRules);
    visitor->trace(m_fontFaceRules);
    visitor->trace(m_keyframesRules);
    visitor->trace(m_deepCombinatorOrShadowPseudoRules);
    visitor->trace(m_contentPseudoElementRules);
    visitor->trace(m_slottedPseudoElementRules);
    visitor->trace(m_viewportDependentMediaQueryResults);
    visitor->trace(m_deviceDependentMediaQueryResults);
    visitor->trace(m_pendingRules);
#ifndef NDEBUG
    visitor->trace(m_allRules);
#endif
}

#ifndef NDEBUG
void RuleSet::show() const
{
    for (const auto& rule: m_allRules)
        rule.selector().show();
}
#endif

} // namespace blink
