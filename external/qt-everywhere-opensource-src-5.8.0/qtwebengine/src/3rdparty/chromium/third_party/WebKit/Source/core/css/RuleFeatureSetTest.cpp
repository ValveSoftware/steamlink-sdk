// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/RuleFeature.h"

#include "core/css/CSSSelectorList.h"
#include "core/css/RuleSet.h"
#include "core/css/StylePropertySet.h"
#include "core/css/StyleRule.h"
#include "core/css/invalidation/InvalidationSet.h"
#include "core/css/parser/CSSParser.h"
#include "core/dom/ElementTraversal.h"
#include "core/html/HTMLBodyElement.h"
#include "core/html/HTMLDocument.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLHtmlElement.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

// TODO(sof): consider making these part object helper abstractions
// available from platform/heap/.

template<typename T>
class HeapPartObject final : public GarbageCollectedFinalized<HeapPartObject<T>> {
public:
    static HeapPartObject* create()
    {
        return new HeapPartObject;
    }

    T* get() { return &m_part; }

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_part);
    }

private:
    HeapPartObject()
    {
    }

    T m_part;
};

template<typename T>
class PersistentPartObject final {
    DISALLOW_NEW();
public:
    PersistentPartObject()
        : m_part(HeapPartObject<T>::create())
    {
    }

    T* operator->() const { return (*m_part).get(); }

private:
    Persistent<HeapPartObject<T>> m_part;
};

class RuleFeatureSetTest : public ::testing::Test {
public:
    RuleFeatureSetTest()
    {
    }

    void SetUp()
    {
        m_document = HTMLDocument::create();
        HTMLHtmlElement* html = HTMLHtmlElement::create(*m_document);
        html->appendChild(HTMLBodyElement::create(*m_document));
        m_document->appendChild(html);

        m_document->body()->setInnerHTML("<b><i></i></b>", ASSERT_NO_EXCEPTION);
    }

    RuleFeatureSet::SelectorPreMatch collectFeatures(const String& selectorText)
    {
        CSSSelectorList selectorList = CSSParser::parseSelector(strictCSSParserContext(), nullptr, selectorText);

        StyleRule* styleRule = StyleRule::create(std::move(selectorList), MutableStylePropertySet::create(HTMLStandardMode));
        RuleData ruleData(styleRule, 0, 0, RuleHasNoSpecialState);
        return m_ruleFeatureSet->collectFeaturesFromRuleData(ruleData);
    }

    void collectInvalidationSetsForClass(InvalidationLists& invalidationLists, const AtomicString& className) const
    {
        Element* element = Traversal<HTMLElement>::firstChild(*Traversal<HTMLElement>::firstChild(*m_document->body()));
        m_ruleFeatureSet->collectInvalidationSetsForClass(invalidationLists, *element, className);
    }

    void collectInvalidationSetsForId(InvalidationLists& invalidationLists, const AtomicString& id) const
    {
        Element* element = Traversal<HTMLElement>::firstChild(*Traversal<HTMLElement>::firstChild(*m_document->body()));
        m_ruleFeatureSet->collectInvalidationSetsForId(invalidationLists, *element, id);
    }

    void collectInvalidationSetsForAttribute(InvalidationLists& invalidationLists, const QualifiedName& attributeName) const
    {
        Element* element = Traversal<HTMLElement>::firstChild(*Traversal<HTMLElement>::firstChild(*m_document->body()));
        m_ruleFeatureSet->collectInvalidationSetsForAttribute(invalidationLists, *element, attributeName);
    }

    void collectInvalidationSetsForPseudoClass(InvalidationLists& invalidationLists, CSSSelector::PseudoType pseudo) const
    {
        Element* element = Traversal<HTMLElement>::firstChild(*Traversal<HTMLElement>::firstChild(*m_document->body()));
        m_ruleFeatureSet->collectInvalidationSetsForPseudoClass(invalidationLists, *element, pseudo);
    }

    void collectUniversalSiblingInvalidationSet(InvalidationLists& invalidationLists)
    {
        m_ruleFeatureSet->collectUniversalSiblingInvalidationSet(invalidationLists);
    }

    const HashSet<AtomicString>& classSet(const InvalidationSet& invalidationSet)
    {
        return invalidationSet.classSetForTesting();
    }

    const HashSet<AtomicString>& idSet(const InvalidationSet& invalidationSet)
    {
        return invalidationSet.idSetForTesting();
    }

    const HashSet<AtomicString>& tagNameSet(const InvalidationSet& invalidationSet)
    {
        return invalidationSet.tagNameSetForTesting();
    }

    const HashSet<AtomicString>& attributeSet(const InvalidationSet& invalidationSet)
    {
        return invalidationSet.attributeSetForTesting();
    }

    void expectNoInvalidation(InvalidationSetVector& invalidationSets)
    {
        EXPECT_EQ(0u, invalidationSets.size());
    }

    void expectSelfInvalidation(InvalidationSetVector& invalidationSets)
    {
        EXPECT_EQ(1u, invalidationSets.size());
        EXPECT_TRUE(invalidationSets[0]->invalidatesSelf());
    }

    void expectNoSelfInvalidation(InvalidationSetVector& invalidationSets)
    {
        EXPECT_EQ(1u, invalidationSets.size());
        EXPECT_FALSE(invalidationSets[0]->invalidatesSelf());
    }

    void expectClassInvalidation(const AtomicString& className, InvalidationSetVector& invalidationSets)
    {
        EXPECT_EQ(1u, invalidationSets.size());
        HashSet<AtomicString> classes = classSet(*invalidationSets[0]);
        EXPECT_EQ(1u, classes.size());
        EXPECT_TRUE(classes.contains(className));
    }

    void expectSiblingInvalidation(unsigned maxDirectAdjacentSelectors, const AtomicString& siblingName, InvalidationSetVector& invalidationSets)
    {
        EXPECT_EQ(1u, invalidationSets.size());
        const SiblingInvalidationSet& siblingInvalidationSet = toSiblingInvalidationSet(*invalidationSets[0]);
        HashSet<AtomicString> classes = classSet(siblingInvalidationSet);
        EXPECT_EQ(1u, classes.size());
        EXPECT_TRUE(classes.contains(siblingName));
        EXPECT_EQ(maxDirectAdjacentSelectors, siblingInvalidationSet.maxDirectAdjacentSelectors());
    }

    void expectSiblingDescendantInvalidation(unsigned maxDirectAdjacentSelectors, const AtomicString& siblingName, const AtomicString& descendantName, InvalidationSetVector& invalidationSets)
    {
        EXPECT_EQ(1u, invalidationSets.size());
        const SiblingInvalidationSet& siblingInvalidationSet = toSiblingInvalidationSet(*invalidationSets[0]);
        HashSet<AtomicString> classes = classSet(siblingInvalidationSet);
        EXPECT_EQ(1u, classes.size());
        EXPECT_TRUE(classes.contains(siblingName));
        EXPECT_EQ(maxDirectAdjacentSelectors, siblingInvalidationSet.maxDirectAdjacentSelectors());

        HashSet<AtomicString> descendantClasses = classSet(*siblingInvalidationSet.siblingDescendants());
        EXPECT_EQ(1u, descendantClasses.size());
        EXPECT_TRUE(descendantClasses.contains(descendantName));
    }

    void expectClassesInvalidation(const AtomicString& firstClassName, const AtomicString& secondClassName, InvalidationSetVector& invalidationSets)
    {
        EXPECT_EQ(1u, invalidationSets.size());
        HashSet<AtomicString> classes = classSet(*invalidationSets[0]);
        EXPECT_EQ(2u, classes.size());
        EXPECT_TRUE(classes.contains(firstClassName));
        EXPECT_TRUE(classes.contains(secondClassName));
    }

    void expectIdInvalidation(const AtomicString& id, InvalidationSetVector& invalidationSets)
    {
        EXPECT_EQ(1u, invalidationSets.size());
        HashSet<AtomicString> ids = idSet(*invalidationSets[0]);
        EXPECT_EQ(1u, ids.size());
        EXPECT_TRUE(ids.contains(id));
    }

    void expectTagNameInvalidation(const AtomicString& tagName, InvalidationSetVector& invalidationSets)
    {
        EXPECT_EQ(1u, invalidationSets.size());
        HashSet<AtomicString> tagNames = tagNameSet(*invalidationSets[0]);
        EXPECT_EQ(1u, tagNames.size());
        EXPECT_TRUE(tagNames.contains(tagName));
    }

    void expectAttributeInvalidation(const AtomicString& attribute, InvalidationSetVector& invalidationSets)
    {
        EXPECT_EQ(1u, invalidationSets.size());
        HashSet<AtomicString> attributes = attributeSet(*invalidationSets[0]);
        EXPECT_EQ(1u, attributes.size());
        EXPECT_TRUE(attributes.contains(attribute));
    }

    void expectSiblingRuleCount(unsigned count)
    {
        EXPECT_EQ(count, m_ruleFeatureSet->siblingRules.size());
    }

    void expectUncommonAttributeRuleCount(unsigned count)
    {
        EXPECT_EQ(count, m_ruleFeatureSet->uncommonAttributeRules.size());
    }

private:
    PersistentPartObject<RuleFeatureSet> m_ruleFeatureSet;
    Persistent<Document> m_document;
};

TEST_F(RuleFeatureSetTest, interleavedDescendantSibling1)
{
    EXPECT_EQ(RuleFeatureSet::SelectorMayMatch, collectFeatures(".p"));

    InvalidationLists invalidationLists;
    collectInvalidationSetsForClass(invalidationLists, "p");
    expectSelfInvalidation(invalidationLists.descendants);
    expectNoInvalidation(invalidationLists.siblings);
}

TEST_F(RuleFeatureSetTest, interleavedDescendantSibling2)
{
    EXPECT_EQ(RuleFeatureSet::SelectorMayMatch, collectFeatures(".o + .p"));

    InvalidationLists invalidationLists;
    collectInvalidationSetsForClass(invalidationLists, "o");
    expectNoInvalidation(invalidationLists.descendants);
    expectSiblingInvalidation(1, "p", invalidationLists.siblings);
}

TEST_F(RuleFeatureSetTest, interleavedDescendantSibling3)
{
    EXPECT_EQ(RuleFeatureSet::SelectorMayMatch, collectFeatures(".m + .n .o + .p"));

    InvalidationLists invalidationLists;
    collectInvalidationSetsForClass(invalidationLists, "n");
    expectNoSelfInvalidation(invalidationLists.descendants);
    expectClassInvalidation("p", invalidationLists.descendants);
    expectNoInvalidation(invalidationLists.siblings);
}

TEST_F(RuleFeatureSetTest, interleavedDescendantSibling4)
{
    EXPECT_EQ(RuleFeatureSet::SelectorMayMatch, collectFeatures(".m + .n .o + .p"));

    InvalidationLists invalidationLists;
    collectInvalidationSetsForClass(invalidationLists, "m");
    expectNoInvalidation(invalidationLists.descendants);
    expectSiblingDescendantInvalidation(1, "n", "p", invalidationLists.siblings);
}

TEST_F(RuleFeatureSetTest, interleavedDescendantSibling5)
{
    EXPECT_EQ(RuleFeatureSet::SelectorMayMatch, collectFeatures(".l ~ .m + .n .o + .p"));

    InvalidationLists invalidationLists;
    collectInvalidationSetsForClass(invalidationLists, "l");
    expectNoInvalidation(invalidationLists.descendants);
    expectSiblingDescendantInvalidation(UINT_MAX, "n", "p", invalidationLists.siblings);
}

TEST_F(RuleFeatureSetTest, interleavedDescendantSibling6)
{
    EXPECT_EQ(RuleFeatureSet::SelectorMayMatch, collectFeatures(".k > .l ~ .m + .n .o + .p"));

    InvalidationLists invalidationLists;
    collectInvalidationSetsForClass(invalidationLists, "k");
    expectClassInvalidation("p", invalidationLists.descendants);
    expectNoInvalidation(invalidationLists.siblings);
}


TEST_F(RuleFeatureSetTest, anySibling)
{
    EXPECT_EQ(RuleFeatureSet::SelectorMayMatch, collectFeatures(":-webkit-any(.q, .r) ~ .s .t"));

    InvalidationLists invalidationLists;
    collectInvalidationSetsForClass(invalidationLists, "q");
    expectNoInvalidation(invalidationLists.descendants);
    expectSiblingDescendantInvalidation(UINT_MAX, "s", "t", invalidationLists.siblings);
}

TEST_F(RuleFeatureSetTest, any)
{
    EXPECT_EQ(RuleFeatureSet::SelectorMayMatch, collectFeatures(":-webkit-any(.w, .x)"));

    InvalidationLists invalidationLists;
    collectInvalidationSetsForClass(invalidationLists, "w");
    expectSelfInvalidation(invalidationLists.descendants);
    expectNoInvalidation(invalidationLists.siblings);
}

TEST_F(RuleFeatureSetTest, siblingAny)
{
    EXPECT_EQ(RuleFeatureSet::SelectorMayMatch, collectFeatures(".v ~ :-webkit-any(.w, .x)"));

    InvalidationLists invalidationLists;
    collectInvalidationSetsForClass(invalidationLists, "v");
    expectNoInvalidation(invalidationLists.descendants);
    expectClassesInvalidation("w", "x", invalidationLists.siblings);
}

TEST_F(RuleFeatureSetTest, descendantSiblingAny)
{
    EXPECT_EQ(RuleFeatureSet::SelectorMayMatch, collectFeatures(".u .v ~ :-webkit-any(.w, .x)"));

    InvalidationLists invalidationLists;
    collectInvalidationSetsForClass(invalidationLists, "u");
    expectClassesInvalidation("w", "x", invalidationLists.descendants);
    expectNoInvalidation(invalidationLists.siblings);
}

TEST_F(RuleFeatureSetTest, id)
{
    EXPECT_EQ(RuleFeatureSet::SelectorMayMatch, collectFeatures("#a #b"));

    InvalidationLists invalidationLists;
    collectInvalidationSetsForId(invalidationLists, "a");
    expectIdInvalidation("b", invalidationLists.descendants);
}

TEST_F(RuleFeatureSetTest, attribute)
{
    EXPECT_EQ(RuleFeatureSet::SelectorMayMatch, collectFeatures("[c] [d]"));

    InvalidationLists invalidationLists;
    collectInvalidationSetsForAttribute(invalidationLists, QualifiedName("", "c", ""));
    expectAttributeInvalidation("d", invalidationLists.descendants);
}

TEST_F(RuleFeatureSetTest, pseudoClass)
{
    EXPECT_EQ(RuleFeatureSet::SelectorMayMatch, collectFeatures(":valid"));

    InvalidationLists invalidationLists;
    collectInvalidationSetsForPseudoClass(invalidationLists, CSSSelector::PseudoValid);
    expectSelfInvalidation(invalidationLists.descendants);
}

TEST_F(RuleFeatureSetTest, tagName)
{
    EXPECT_EQ(RuleFeatureSet::SelectorMayMatch, collectFeatures(":valid e"));

    InvalidationLists invalidationLists;
    collectInvalidationSetsForPseudoClass(invalidationLists, CSSSelector::PseudoValid);
    expectTagNameInvalidation("e", invalidationLists.descendants);
}

TEST_F(RuleFeatureSetTest, contentPseudo)
{
    EXPECT_EQ(RuleFeatureSet::SelectorMayMatch, collectFeatures(".a ::content .b"));
    EXPECT_EQ(RuleFeatureSet::SelectorMayMatch, collectFeatures(".a .c"));

    InvalidationLists invalidationLists;
    collectInvalidationSetsForClass(invalidationLists, "a");
    expectClassInvalidation("c", invalidationLists.descendants);

    EXPECT_EQ(RuleFeatureSet::SelectorMayMatch, collectFeatures(".a .b"));

    invalidationLists.descendants.clear();
    collectInvalidationSetsForClass(invalidationLists, "a");
    expectClassesInvalidation("b", "c", invalidationLists.descendants);
}

TEST_F(RuleFeatureSetTest, nonMatchingHost)
{
    EXPECT_EQ(RuleFeatureSet::SelectorNeverMatches, collectFeatures(".a:host"));
    EXPECT_EQ(RuleFeatureSet::SelectorNeverMatches, collectFeatures("*:host(.a)"));
    EXPECT_EQ(RuleFeatureSet::SelectorNeverMatches, collectFeatures("*:host .a"));
    EXPECT_EQ(RuleFeatureSet::SelectorNeverMatches, collectFeatures("div :host .a"));
    EXPECT_EQ(RuleFeatureSet::SelectorNeverMatches, collectFeatures(":host:hover .a"));

    InvalidationLists invalidationLists;
    collectInvalidationSetsForClass(invalidationLists, "a");
    expectNoInvalidation(invalidationLists.descendants);
}

TEST_F(RuleFeatureSetTest, nonMatchingHostContext)
{
    EXPECT_EQ(RuleFeatureSet::SelectorNeverMatches, collectFeatures(".a:host-context(*)"));
    EXPECT_EQ(RuleFeatureSet::SelectorNeverMatches, collectFeatures("*:host-context(.a)"));
    EXPECT_EQ(RuleFeatureSet::SelectorNeverMatches, collectFeatures("*:host-context(*) .a"));
    EXPECT_EQ(RuleFeatureSet::SelectorNeverMatches, collectFeatures("div :host-context(div) .a"));
    EXPECT_EQ(RuleFeatureSet::SelectorNeverMatches, collectFeatures(":host-context(div):hover .a"));

    InvalidationLists invalidationLists;
    collectInvalidationSetsForClass(invalidationLists, "a");
    expectNoInvalidation(invalidationLists.descendants);
}

TEST_F(RuleFeatureSetTest, siblingRulesBeforeContentPseudo)
{
    EXPECT_EQ(RuleFeatureSet::SelectorMayMatch, collectFeatures("a + b ::content .c"));
    expectSiblingRuleCount(0);
}

TEST_F(RuleFeatureSetTest, siblingRulesBeforeContentPseudo2)
{
    EXPECT_EQ(RuleFeatureSet::SelectorMayMatch, collectFeatures("a + ::content .b"));
    expectSiblingRuleCount(0);
}

TEST_F(RuleFeatureSetTest, siblingRulesAfterContentPseudo)
{
    EXPECT_EQ(RuleFeatureSet::SelectorMayMatch, collectFeatures(".a ::content .b + .c"));
    expectSiblingRuleCount(1);
}

TEST_F(RuleFeatureSetTest, siblingRulesNthBeforeContentPseudo)
{
    EXPECT_EQ(RuleFeatureSet::SelectorMayMatch, collectFeatures(":nth-child(2) ::content .a"));
    expectSiblingRuleCount(0);
}

TEST_F(RuleFeatureSetTest, siblingRulesNthAfterContentPseudo)
{
    EXPECT_EQ(RuleFeatureSet::SelectorMayMatch, collectFeatures(".a ::content :nth-child(2)"));
    expectSiblingRuleCount(1);
}

TEST_F(RuleFeatureSetTest, siblingRulesBeforeDeep)
{
    EXPECT_EQ(RuleFeatureSet::SelectorMayMatch, collectFeatures("a + b /deep/ .c"));
    expectSiblingRuleCount(1);
}

TEST_F(RuleFeatureSetTest, siblingRulesAfterDeep)
{
    EXPECT_EQ(RuleFeatureSet::SelectorMayMatch, collectFeatures(".a /deep/ .b + .c"));
    expectSiblingRuleCount(1);
}

TEST_F(RuleFeatureSetTest, siblingRulesBeforeShadow)
{
    EXPECT_EQ(RuleFeatureSet::SelectorMayMatch, collectFeatures(".a + .b::shadow .c"));
    expectSiblingRuleCount(1);
}

TEST_F(RuleFeatureSetTest, siblingRulesAfterShadow)
{
    EXPECT_EQ(RuleFeatureSet::SelectorMayMatch, collectFeatures(".a ::shadow .b + .c"));
    expectSiblingRuleCount(1);
}

TEST_F(RuleFeatureSetTest, siblingRulesBeforeSlotted)
{
    EXPECT_EQ(RuleFeatureSet::SelectorMayMatch, collectFeatures(".a + ::slotted(.b)"));
    expectSiblingRuleCount(0);
}

TEST_F(RuleFeatureSetTest, siblingRulesBeforeHost)
{
    EXPECT_EQ(RuleFeatureSet::SelectorNeverMatches, collectFeatures(".a + :host(.b)"));
    expectSiblingRuleCount(0);
}

TEST_F(RuleFeatureSetTest, siblingRulesBeforeHostContext)
{
    EXPECT_EQ(RuleFeatureSet::SelectorNeverMatches, collectFeatures(".a + :host-context(.b)"));
    expectSiblingRuleCount(0);
}

TEST_F(RuleFeatureSetTest, uncommonAttributeRulesAfterContentPseudo)
{
    EXPECT_EQ(RuleFeatureSet::SelectorMayMatch, collectFeatures("div ::content [attr]"));
    expectUncommonAttributeRuleCount(1);
}

TEST_F(RuleFeatureSetTest, uncommonAttributeRulesBeforeContentPseudo)
{
    EXPECT_EQ(RuleFeatureSet::SelectorMayMatch, collectFeatures("[attr] ::content div"));
    expectUncommonAttributeRuleCount(0);
}

TEST_F(RuleFeatureSetTest, uncommonAttributeRulesSlotted)
{
    EXPECT_EQ(RuleFeatureSet::SelectorMayMatch, collectFeatures("::slotted([attr])"));
    expectUncommonAttributeRuleCount(1);
}

TEST_F(RuleFeatureSetTest, uncommonAttributeRulesBeforeSlotted)
{
    EXPECT_EQ(RuleFeatureSet::SelectorMayMatch, collectFeatures("[attr]::slotted(*)"));
    expectUncommonAttributeRuleCount(0);
}

TEST_F(RuleFeatureSetTest, uncommonAttributeRulesHost)
{
    EXPECT_EQ(RuleFeatureSet::SelectorMayMatch, collectFeatures(":host([attr])"));
    expectUncommonAttributeRuleCount(1);
}

TEST_F(RuleFeatureSetTest, uncommonAttributeRulesBeforeHost)
{
    EXPECT_EQ(RuleFeatureSet::SelectorNeverMatches, collectFeatures("[attr] :host"));
    expectUncommonAttributeRuleCount(0);
}

TEST_F(RuleFeatureSetTest, uncommonAttributeRulesAfterHost)
{
    EXPECT_EQ(RuleFeatureSet::SelectorNeverMatches, collectFeatures(":host[attr]"));
    expectUncommonAttributeRuleCount(0);
}

TEST_F(RuleFeatureSetTest, uncommonAttributeRulesAfterHost2)
{
    EXPECT_EQ(RuleFeatureSet::SelectorMayMatch, collectFeatures(":host [attr]"));
    expectUncommonAttributeRuleCount(1);
}

TEST_F(RuleFeatureSetTest, uncommonAttributeRulesHostBeforePseudo)
{
    EXPECT_EQ(RuleFeatureSet::SelectorMayMatch, collectFeatures(":host([attr])::before"));
    expectUncommonAttributeRuleCount(1);
}

TEST_F(RuleFeatureSetTest, uncommonAttributeRulesHostContext)
{
    EXPECT_EQ(RuleFeatureSet::SelectorMayMatch, collectFeatures(":host-context([attr])"));
    expectUncommonAttributeRuleCount(1);
}

TEST_F(RuleFeatureSetTest, uncommonAttributeRulesBeforeHostContext)
{
    EXPECT_EQ(RuleFeatureSet::SelectorNeverMatches, collectFeatures("[attr] :host-context(div)"));
    expectUncommonAttributeRuleCount(0);
}

TEST_F(RuleFeatureSetTest, uncommonAttributeRulesBeforeHostContext2)
{
    EXPECT_EQ(RuleFeatureSet::SelectorNeverMatches, collectFeatures("[attr]:host-context(div)"));
    expectUncommonAttributeRuleCount(0);
}

TEST_F(RuleFeatureSetTest, uncommonAttributeRulesAfterHostContext)
{
    EXPECT_EQ(RuleFeatureSet::SelectorNeverMatches, collectFeatures(":host-context(*)[attr]"));
    expectUncommonAttributeRuleCount(0);
}

TEST_F(RuleFeatureSetTest, uncommonAttributeRulesAfterHostContext2)
{
    EXPECT_EQ(RuleFeatureSet::SelectorMayMatch, collectFeatures(":host-context(*) [attr]"));
    expectUncommonAttributeRuleCount(1);
}

TEST_F(RuleFeatureSetTest, uncommonAttributeRulesAfterHostContextBeforePseudo)
{
    EXPECT_EQ(RuleFeatureSet::SelectorMayMatch, collectFeatures(":host-context([attr])::before"));
    expectUncommonAttributeRuleCount(1);
}

TEST_F(RuleFeatureSetTest, universalSiblingInvalidationDirectAdjacent)
{
    EXPECT_EQ(RuleFeatureSet::SelectorMayMatch, collectFeatures("* + .a"));

    InvalidationLists invalidationLists;
    collectUniversalSiblingInvalidationSet(invalidationLists);

    expectSiblingInvalidation(1, "a", invalidationLists.siblings);
    expectSelfInvalidation(invalidationLists.siblings);
}

TEST_F(RuleFeatureSetTest, universalSiblingInvalidationMultipleDirectAdjacent)
{
    EXPECT_EQ(RuleFeatureSet::SelectorMayMatch, collectFeatures("* + .a + .b"));

    InvalidationLists invalidationLists;
    collectUniversalSiblingInvalidationSet(invalidationLists);

    expectSiblingInvalidation(2, "b", invalidationLists.siblings);
    expectSelfInvalidation(invalidationLists.siblings);
}

TEST_F(RuleFeatureSetTest, universalSiblingInvalidationDirectAdjacentDescendant)
{
    EXPECT_EQ(RuleFeatureSet::SelectorMayMatch, collectFeatures("* + .a .b"));

    InvalidationLists invalidationLists;
    collectUniversalSiblingInvalidationSet(invalidationLists);

    expectSiblingDescendantInvalidation(1, "a", "b", invalidationLists.siblings);
    expectNoSelfInvalidation(invalidationLists.siblings);
}

TEST_F(RuleFeatureSetTest, universalSiblingInvalidationIndirectAdjacent)
{
    EXPECT_EQ(RuleFeatureSet::SelectorMayMatch, collectFeatures("* ~ .a"));

    InvalidationLists invalidationLists;
    collectUniversalSiblingInvalidationSet(invalidationLists);

    expectSiblingInvalidation(UINT_MAX, "a", invalidationLists.siblings);
    expectSelfInvalidation(invalidationLists.siblings);
}

TEST_F(RuleFeatureSetTest, universalSiblingInvalidationMultipleIndirectAdjacent)
{
    EXPECT_EQ(RuleFeatureSet::SelectorMayMatch, collectFeatures("* ~ .a ~ .b"));

    InvalidationLists invalidationLists;
    collectUniversalSiblingInvalidationSet(invalidationLists);

    expectSiblingInvalidation(UINT_MAX, "b", invalidationLists.siblings);
    expectSelfInvalidation(invalidationLists.siblings);
}

TEST_F(RuleFeatureSetTest, universalSiblingInvalidationIndirectAdjacentDescendant)
{
    EXPECT_EQ(RuleFeatureSet::SelectorMayMatch, collectFeatures("* ~ .a .b"));

    InvalidationLists invalidationLists;
    collectUniversalSiblingInvalidationSet(invalidationLists);

    expectSiblingDescendantInvalidation(UINT_MAX, "a", "b", invalidationLists.siblings);
    expectNoSelfInvalidation(invalidationLists.siblings);
}

TEST_F(RuleFeatureSetTest, universalSiblingInvalidationNot)
{
    EXPECT_EQ(RuleFeatureSet::SelectorMayMatch, collectFeatures(":not(.a) + .b"));

    InvalidationLists invalidationLists;
    collectUniversalSiblingInvalidationSet(invalidationLists);

    expectSiblingInvalidation(1, "b", invalidationLists.siblings);
    expectSelfInvalidation(invalidationLists.siblings);
}

TEST_F(RuleFeatureSetTest, nonUniversalSiblingInvalidationNot)
{
    EXPECT_EQ(RuleFeatureSet::SelectorMayMatch, collectFeatures("#x:not(.a) + .b"));

    InvalidationLists invalidationLists;
    collectUniversalSiblingInvalidationSet(invalidationLists);

    expectNoInvalidation(invalidationLists.siblings);
}

TEST_F(RuleFeatureSetTest, universalSiblingInvalidationAny)
{
    EXPECT_EQ(RuleFeatureSet::SelectorMayMatch, collectFeatures(":-webkit-any(.a) + .b"));

    InvalidationLists invalidationLists;
    collectUniversalSiblingInvalidationSet(invalidationLists);

    expectSiblingInvalidation(1, "b", invalidationLists.siblings);
    expectSelfInvalidation(invalidationLists.siblings);
}

TEST_F(RuleFeatureSetTest, nonUniversalSiblingInvalidationAny)
{
    EXPECT_EQ(RuleFeatureSet::SelectorMayMatch, collectFeatures("#x:-webkit-any(.a) + .b"));

    InvalidationLists invalidationLists;
    collectUniversalSiblingInvalidationSet(invalidationLists);

    expectNoInvalidation(invalidationLists.siblings);
}

TEST_F(RuleFeatureSetTest, universalSiblingInvalidationType)
{
    EXPECT_EQ(RuleFeatureSet::SelectorMayMatch, collectFeatures("div + .a"));

    InvalidationLists invalidationLists;
    collectUniversalSiblingInvalidationSet(invalidationLists);

    expectSiblingInvalidation(1, "a", invalidationLists.siblings);
    expectSelfInvalidation(invalidationLists.siblings);
}

TEST_F(RuleFeatureSetTest, nonUniversalSiblingInvalidationType)
{
    EXPECT_EQ(RuleFeatureSet::SelectorMayMatch, collectFeatures("div#x + .a"));

    InvalidationLists invalidationLists;
    collectUniversalSiblingInvalidationSet(invalidationLists);

    expectNoInvalidation(invalidationLists.siblings);
}

TEST_F(RuleFeatureSetTest, universalSiblingInvalidationLink)
{
    EXPECT_EQ(RuleFeatureSet::SelectorMayMatch, collectFeatures(":link + .a"));

    InvalidationLists invalidationLists;
    collectUniversalSiblingInvalidationSet(invalidationLists);

    expectSiblingInvalidation(1, "a", invalidationLists.siblings);
    expectSelfInvalidation(invalidationLists.siblings);
}

TEST_F(RuleFeatureSetTest, nonUniversalSiblingInvalidationLink)
{
    EXPECT_EQ(RuleFeatureSet::SelectorMayMatch, collectFeatures("#x:link + .a"));

    InvalidationLists invalidationLists;
    collectUniversalSiblingInvalidationSet(invalidationLists);

    expectNoInvalidation(invalidationLists.siblings);
}

} // namespace blink
