/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2006, 2007, 2012 Apple Inc. All rights reserved.
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

#include "core/css/CSSStyleSheet.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8PerIsolateData.h"
#include "core/HTMLNames.h"
#include "core/SVGNames.h"
#include "core/css/CSSImportRule.h"
#include "core/css/CSSRuleList.h"
#include "core/css/MediaList.h"
#include "core/css/StyleRule.h"
#include "core/css/StyleSheetContents.h"
#include "core/css/parser/CSSParser.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/Node.h"
#include "core/dom/StyleEngine.h"
#include "core/frame/Deprecation.h"
#include "core/html/HTMLStyleElement.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/svg/SVGStyleElement.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

class StyleSheetCSSRuleList final : public CSSRuleList {
public:
    static StyleSheetCSSRuleList* create(CSSStyleSheet* sheet)
    {
        return new StyleSheetCSSRuleList(sheet);
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_styleSheet);
        CSSRuleList::trace(visitor);
    }

private:
    StyleSheetCSSRuleList(CSSStyleSheet* sheet) : m_styleSheet(sheet) { }

    unsigned length() const override { return m_styleSheet->length(); }
    CSSRule* item(unsigned index) const override { return m_styleSheet->item(index); }

    CSSStyleSheet* styleSheet() const override { return m_styleSheet; }

    Member<CSSStyleSheet> m_styleSheet;
};

#if ENABLE(ASSERT)
static bool isAcceptableCSSStyleSheetParent(Node* parentNode)
{
    // Only these nodes can be parents of StyleSheets, and they need to call
    // clearOwnerNode() when moved out of document. Note that destructor of
    // the nodes don't call clearOwnerNode() with Oilpan.
    return !parentNode
        || parentNode->isDocumentNode()
        || isHTMLLinkElement(*parentNode)
        || isHTMLStyleElement(*parentNode)
        || isSVGStyleElement(*parentNode)
        || parentNode->getNodeType() == Node::PROCESSING_INSTRUCTION_NODE;
}
#endif

CSSStyleSheet* CSSStyleSheet::create(StyleSheetContents* sheet, CSSImportRule* ownerRule)
{
    return new CSSStyleSheet(sheet, ownerRule);
}

CSSStyleSheet* CSSStyleSheet::create(StyleSheetContents* sheet, Node* ownerNode)
{
    return new CSSStyleSheet(sheet, ownerNode, false, TextPosition::minimumPosition());
}

CSSStyleSheet* CSSStyleSheet::createInline(StyleSheetContents* sheet, Node* ownerNode, const TextPosition& startPosition)
{
    ASSERT(sheet);
    return new CSSStyleSheet(sheet, ownerNode, true, startPosition);
}

CSSStyleSheet* CSSStyleSheet::createInline(Node* ownerNode, const KURL& baseURL, const TextPosition& startPosition, const String& encoding)
{
    CSSParserContext parserContext(ownerNode->document(), nullptr, baseURL, encoding);
    StyleSheetContents* sheet = StyleSheetContents::create(baseURL.getString(), parserContext);
    return new CSSStyleSheet(sheet, ownerNode, true, startPosition);
}

CSSStyleSheet::CSSStyleSheet(StyleSheetContents* contents, CSSImportRule* ownerRule)
    : m_contents(contents)
    , m_isInlineStylesheet(false)
    , m_isDisabled(false)
    , m_ownerNode(nullptr)
    , m_ownerRule(ownerRule)
    , m_startPosition(TextPosition::minimumPosition())
    , m_loadCompleted(false)
{
    m_contents->registerClient(this);
}

CSSStyleSheet::CSSStyleSheet(StyleSheetContents* contents, Node* ownerNode, bool isInlineStylesheet, const TextPosition& startPosition)
    : m_contents(contents)
    , m_isInlineStylesheet(isInlineStylesheet)
    , m_isDisabled(false)
    , m_ownerNode(ownerNode)
    , m_ownerRule(nullptr)
    , m_startPosition(startPosition)
    , m_loadCompleted(false)
{
    ASSERT(isAcceptableCSSStyleSheetParent(ownerNode));
    m_contents->registerClient(this);
}

CSSStyleSheet::~CSSStyleSheet()
{
}

#if ENABLE(ASSERT)

static bool isStyleElement(const Node* node)
{
    return node && (isHTMLStyleElement(node) || isSVGStyleElement(node));
}

#endif // ENABLE(ASSERT)

void CSSStyleSheet::willMutateRules()
{
    // If we are the only client it is safe to mutate.
    if (m_contents->clientSize() <= 1 && !m_contents->isReferencedFromResource()) {
        m_contents->clearRuleSet();
        if (Document* document = ownerDocument())
            m_contents->removeSheetFromCache(document);
        m_contents->setMutable();
        return;
    }
    // Only cacheable stylesheets should have multiple clients.
    ASSERT((isStyleElement(ownerNode()) && m_contents->isCacheableForStyleElement())
        || m_contents->isCacheableForResource());

    // Copy-on-write.
    m_contents->unregisterClient(this);
    m_contents = m_contents->copy();
    m_contents->registerClient(this);

    m_contents->setMutable();

    // Any existing CSSOM wrappers need to be connected to the copied child rules.
    reattachChildRuleCSSOMWrappers();
}

void CSSStyleSheet::didMutateRules()
{
    ASSERT(m_contents->isMutable());
    ASSERT(m_contents->clientSize() <= 1);

    didMutate(PartialRuleUpdate);
}

void CSSStyleSheet::didMutate(StyleSheetUpdateType updateType)
{
    Document* owner = ownerDocument();
    if (!owner)
        return;

    // Need FullStyleUpdate when insertRule or deleteRule,
    // because StyleSheetCollection::analyzeStyleSheetChange cannot detect partial rule update.
    StyleResolverUpdateMode updateMode = updateType != PartialRuleUpdate ? AnalyzedStyleUpdate : FullStyleUpdate;
    owner->styleEngine().setNeedsActiveStyleUpdate(this, updateMode);
}

void CSSStyleSheet::reattachChildRuleCSSOMWrappers()
{
    for (unsigned i = 0; i < m_childRuleCSSOMWrappers.size(); ++i) {
        if (!m_childRuleCSSOMWrappers[i])
            continue;
        m_childRuleCSSOMWrappers[i]->reattach(m_contents->ruleAt(i));
    }
}

void CSSStyleSheet::setDisabled(bool disabled)
{
    if (disabled == m_isDisabled)
        return;
    m_isDisabled = disabled;

    didMutate();
}

void CSSStyleSheet::setMediaQueries(MediaQuerySet* mediaQueries)
{
    m_mediaQueries = mediaQueries;
    if (m_mediaCSSOMWrapper && m_mediaQueries)
        m_mediaCSSOMWrapper->reattach(m_mediaQueries.get());

}

unsigned CSSStyleSheet::length() const
{
    return m_contents->ruleCount();
}

CSSRule* CSSStyleSheet::item(unsigned index)
{
    unsigned ruleCount = length();
    if (index >= ruleCount)
        return nullptr;

    if (m_childRuleCSSOMWrappers.isEmpty())
        m_childRuleCSSOMWrappers.grow(ruleCount);
    ASSERT(m_childRuleCSSOMWrappers.size() == ruleCount);

    Member<CSSRule>& cssRule = m_childRuleCSSOMWrappers[index];
    if (!cssRule)
        cssRule = m_contents->ruleAt(index)->createCSSOMWrapper(this);
    return cssRule.get();
}

void CSSStyleSheet::clearOwnerNode()
{
    didMutate(EntireStyleSheetUpdate);
    if (m_ownerNode)
        m_contents->unregisterClient(this);
    m_ownerNode = nullptr;
}

bool CSSStyleSheet::canAccessRules() const
{
    if (m_isInlineStylesheet)
        return true;
    KURL baseURL = m_contents->baseURL();
    if (baseURL.isEmpty())
        return true;
    Document* document = ownerDocument();
    if (!document)
        return true;
    if (document->getSecurityOrigin()->canRequestNoSuborigin(baseURL))
        return true;
    if (m_allowRuleAccessFromOrigin && document->getSecurityOrigin()->canAccessCheckSuborigins(m_allowRuleAccessFromOrigin.get()))
        return true;
    return false;
}

CSSRuleList* CSSStyleSheet::rules()
{
    return cssRules();
}

unsigned CSSStyleSheet::insertRule(const String& ruleString, unsigned index, ExceptionState& exceptionState)
{
    ASSERT(m_childRuleCSSOMWrappers.isEmpty() || m_childRuleCSSOMWrappers.size() == m_contents->ruleCount());

    if (index > length()) {
        exceptionState.throwDOMException(IndexSizeError, "The index provided (" + String::number(index) + ") is larger than the maximum index (" + String::number(length()) + ").");
        return 0;
    }
    CSSParserContext context(m_contents->parserContext(), UseCounter::getFrom(this));
    StyleRuleBase* rule = CSSParser::parseRule(context, m_contents.get(), ruleString);

    if (!rule) {
        exceptionState.throwDOMException(SyntaxError, "Failed to parse the rule '" + ruleString + "'.");
        return 0;
    }
    RuleMutationScope mutationScope(this);

    bool success = m_contents->wrapperInsertRule(rule, index);
    if (!success) {
        if (rule->isNamespaceRule())
            exceptionState.throwDOMException(InvalidStateError, "Failed to insert the rule");
        else
            exceptionState.throwDOMException(HierarchyRequestError, "Failed to insert the rule.");
        return 0;
    }
    if (!m_childRuleCSSOMWrappers.isEmpty())
        m_childRuleCSSOMWrappers.insert(index, Member<CSSRule>(nullptr));

    return index;
}

unsigned CSSStyleSheet::insertRule(const String& rule, ExceptionState& exceptionState)
{
    Deprecation::countDeprecation(currentExecutionContext(V8PerIsolateData::mainThreadIsolate()), UseCounter::CSSStyleSheetInsertRuleOptionalArg);
    return insertRule(rule, 0, exceptionState);
}

void CSSStyleSheet::deleteRule(unsigned index, ExceptionState& exceptionState)
{
    ASSERT(m_childRuleCSSOMWrappers.isEmpty() || m_childRuleCSSOMWrappers.size() == m_contents->ruleCount());

    if (index >= length()) {
        exceptionState.throwDOMException(IndexSizeError, "The index provided (" + String::number(index) + ") is larger than the maximum index (" + String::number(length() - 1) + ").");
        return;
    }
    RuleMutationScope mutationScope(this);

    bool success = m_contents->wrapperDeleteRule(index);
    if (!success) {
        exceptionState.throwDOMException(InvalidStateError, "Failed to delete rule");
        return;
    }

    if (!m_childRuleCSSOMWrappers.isEmpty()) {
        if (m_childRuleCSSOMWrappers[index])
            m_childRuleCSSOMWrappers[index]->setParentStyleSheet(0);
        m_childRuleCSSOMWrappers.remove(index);
    }
}

int CSSStyleSheet::addRule(const String& selector, const String& style, int index, ExceptionState& exceptionState)
{
    StringBuilder text;
    text.append(selector);
    text.append(" { ");
    text.append(style);
    if (!style.isEmpty())
        text.append(' ');
    text.append('}');
    insertRule(text.toString(), index, exceptionState);

    // As per Microsoft documentation, always return -1.
    return -1;
}

int CSSStyleSheet::addRule(const String& selector, const String& style, ExceptionState& exceptionState)
{
    return addRule(selector, style, length(), exceptionState);
}


CSSRuleList* CSSStyleSheet::cssRules()
{
    if (!canAccessRules())
        return nullptr;
    if (!m_ruleListCSSOMWrapper)
        m_ruleListCSSOMWrapper = StyleSheetCSSRuleList::create(this);
    return m_ruleListCSSOMWrapper.get();
}

String CSSStyleSheet::href() const
{
    return m_contents->originalURL();
}

KURL CSSStyleSheet::baseURL() const
{
    return m_contents->baseURL();
}

bool CSSStyleSheet::isLoading() const
{
    return m_contents->isLoading();
}

MediaList* CSSStyleSheet::media() const
{
    if (!m_mediaQueries)
        return nullptr;

    if (!m_mediaCSSOMWrapper)
        m_mediaCSSOMWrapper = MediaList::create(m_mediaQueries.get(), const_cast<CSSStyleSheet*>(this));
    return m_mediaCSSOMWrapper.get();
}

CSSStyleSheet* CSSStyleSheet::parentStyleSheet() const
{
    return m_ownerRule ? m_ownerRule->parentStyleSheet() : nullptr;
}

Document* CSSStyleSheet::ownerDocument() const
{
    const CSSStyleSheet* root = this;
    while (root->parentStyleSheet())
        root = root->parentStyleSheet();
    return root->ownerNode() ? &root->ownerNode()->document() : nullptr;
}

void CSSStyleSheet::setAllowRuleAccessFromOrigin(PassRefPtr<SecurityOrigin> allowedOrigin)
{
    m_allowRuleAccessFromOrigin = allowedOrigin;
}

bool CSSStyleSheet::sheetLoaded()
{
    ASSERT(m_ownerNode);
    setLoadCompleted(m_ownerNode->sheetLoaded());
    return m_loadCompleted;
}

void CSSStyleSheet::startLoadingDynamicSheet()
{
    setLoadCompleted(false);
    m_ownerNode->startLoadingDynamicSheet();
}

void CSSStyleSheet::setLoadCompleted(bool completed)
{
    if (completed == m_loadCompleted)
        return;

    m_loadCompleted = completed;

    if (completed)
        m_contents->clientLoadCompleted(this);
    else
        m_contents->clientLoadStarted(this);
}

void CSSStyleSheet::setText(const String& text)
{
    m_childRuleCSSOMWrappers.clear();

    CSSStyleSheet::RuleMutationScope mutationScope(this);
    m_contents->clearRules();
    m_contents->parseString(text);
}

DEFINE_TRACE(CSSStyleSheet)
{
    visitor->trace(m_contents);
    visitor->trace(m_mediaQueries);
    visitor->trace(m_ownerNode);
    visitor->trace(m_ownerRule);
    visitor->trace(m_mediaCSSOMWrapper);
    visitor->trace(m_childRuleCSSOMWrappers);
    visitor->trace(m_ruleListCSSOMWrapper);
    StyleSheet::trace(visitor);
}

} // namespace blink
