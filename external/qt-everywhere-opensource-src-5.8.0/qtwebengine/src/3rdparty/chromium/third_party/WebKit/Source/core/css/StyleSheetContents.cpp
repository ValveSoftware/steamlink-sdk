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

#include "core/css/StyleSheetContents.h"

#include "core/css/CSSStyleSheet.h"
#include "core/css/StylePropertySet.h"
#include "core/css/StyleRule.h"
#include "core/css/StyleRuleImport.h"
#include "core/css/StyleRuleNamespace.h"
#include "core/css/parser/CSSParser.h"
#include "core/dom/Document.h"
#include "core/dom/Node.h"
#include "core/dom/StyleEngine.h"
#include "core/fetch/CSSStyleSheetResource.h"
#include "core/frame/UseCounter.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "platform/TraceEvent.h"
#include "platform/weborigin/SecurityOrigin.h"

namespace blink {

// Rough size estimate for the memory cache.
unsigned StyleSheetContents::estimatedSizeInBytes() const
{
    // Note that this does not take into account size of the strings hanging from various objects.
    // The assumption is that nearly all of of them are atomic and would exist anyway.
    unsigned size = sizeof(*this);

    // FIXME: This ignores the children of media rules.
    // Most rules are StyleRules.
    size += ruleCount() * StyleRule::averageSizeInBytes();

    for (unsigned i = 0; i < m_importRules.size(); ++i) {
        if (StyleSheetContents* sheet = m_importRules[i]->styleSheet())
            size += sheet->estimatedSizeInBytes();
    }
    return size;
}

StyleSheetContents::StyleSheetContents(StyleRuleImport* ownerRule, const String& originalURL, const CSSParserContext& context)
    : m_ownerRule(ownerRule)
    , m_originalURL(originalURL)
    , m_defaultNamespace(starAtom)
    , m_hasSyntacticallyValidCSSHeader(true)
    , m_didLoadErrorOccur(false)
    , m_isMutable(false)
    , m_hasFontFaceRule(false)
    , m_hasMediaQueries(false)
    , m_hasSingleOwnerDocument(true)
    , m_parserContext(context)
{
}

StyleSheetContents::StyleSheetContents(const StyleSheetContents& o)
    : m_ownerRule(nullptr)
    , m_originalURL(o.m_originalURL)
    , m_importRules(o.m_importRules.size())
    , m_namespaceRules(o.m_namespaceRules.size())
    , m_childRules(o.m_childRules.size())
    , m_namespaces(o.m_namespaces)
    , m_defaultNamespace(o.m_defaultNamespace)
    , m_hasSyntacticallyValidCSSHeader(o.m_hasSyntacticallyValidCSSHeader)
    , m_didLoadErrorOccur(false)
    , m_isMutable(false)
    , m_hasFontFaceRule(o.m_hasFontFaceRule)
    , m_hasMediaQueries(o.m_hasMediaQueries)
    , m_hasSingleOwnerDocument(true)
    , m_parserContext(o.m_parserContext)
{
    // FIXME: Copy import rules.
    ASSERT(o.m_importRules.isEmpty());

    for (unsigned i = 0; i < m_childRules.size(); ++i)
        m_childRules[i] = o.m_childRules[i]->copy();
}

StyleSheetContents::~StyleSheetContents()
{
}

void StyleSheetContents::setHasSyntacticallyValidCSSHeader(bool isValidCss)
{
    if (!isValidCss) {
        if (Document* document = clientSingleOwnerDocument())
            removeSheetFromCache(document);
    }
    m_hasSyntacticallyValidCSSHeader = isValidCss;
}

bool StyleSheetContents::isCacheableForResource() const
{
    // This would require dealing with multiple clients for load callbacks.
    if (!loadCompleted())
        return false;
    // FIXME: StyleSheets with media queries can't be cached because their RuleSet
    // is processed differently based off the media queries, which might resolve
    // differently depending on the context of the parent CSSStyleSheet (e.g.
    // if they are in differently sized iframes). Once RuleSets are media query
    // agnostic, we can restore sharing of StyleSheetContents with medea queries.
    if (m_hasMediaQueries)
        return false;
    // FIXME: Support copying import rules.
    if (!m_importRules.isEmpty())
        return false;
    // FIXME: Support cached stylesheets in import rules.
    if (m_ownerRule)
        return false;
    if (m_didLoadErrorOccur)
        return false;
    // It is not the original sheet anymore.
    if (m_isMutable)
        return false;
    // If the header is valid we are not going to need to check the SecurityOrigin.
    // FIXME: Valid mime type avoids the check too.
    if (!m_hasSyntacticallyValidCSSHeader)
        return false;
    return true;
}

bool StyleSheetContents::isCacheableForStyleElement() const
{
    // FIXME: Support copying import rules.
    if (!importRules().isEmpty())
        return false;
    // Until import rules are supported in cached sheets it's not possible for loading to fail.
    DCHECK(!didLoadErrorOccur());
    // It is not the original sheet anymore.
    if (isMutable())
        return false;
    if (!hasSyntacticallyValidCSSHeader())
        return false;
    return true;
}


void StyleSheetContents::parserAppendRule(StyleRuleBase* rule)
{
    if (rule->isImportRule()) {
        // Parser enforces that @import rules come before anything else
        ASSERT(m_childRules.isEmpty());
        StyleRuleImport* importRule = toStyleRuleImport(rule);
        if (importRule->mediaQueries())
            setHasMediaQueries();
        m_importRules.append(importRule);
        m_importRules.last()->setParentStyleSheet(this);
        m_importRules.last()->requestStyleSheet();
        return;
    }

    if (rule->isNamespaceRule()) {
        // Parser enforces that @namespace rules come before all rules other than
        // import/charset rules
        ASSERT(m_childRules.isEmpty());
        StyleRuleNamespace& namespaceRule = toStyleRuleNamespace(*rule);
        parserAddNamespace(namespaceRule.prefix(), namespaceRule.uri());
        m_namespaceRules.append(&namespaceRule);
        return;
    }

    m_childRules.append(rule);
}

void StyleSheetContents::setHasMediaQueries()
{
    m_hasMediaQueries = true;
    if (parentStyleSheet())
        parentStyleSheet()->setHasMediaQueries();
}

StyleRuleBase* StyleSheetContents::ruleAt(unsigned index) const
{
    ASSERT_WITH_SECURITY_IMPLICATION(index < ruleCount());

    if (index < m_importRules.size())
        return m_importRules[index].get();

    index -= m_importRules.size();

    if (index < m_namespaceRules.size())
        return m_namespaceRules[index].get();

    index -= m_namespaceRules.size();

    return m_childRules[index].get();
}

unsigned StyleSheetContents::ruleCount() const
{
    return m_importRules.size() + m_namespaceRules.size() + m_childRules.size();
}

void StyleSheetContents::clearRules()
{
    for (unsigned i = 0; i < m_importRules.size(); ++i) {
        ASSERT(m_importRules.at(i)->parentStyleSheet() == this);
        m_importRules[i]->clearParentStyleSheet();
    }
    m_importRules.clear();
    m_namespaceRules.clear();
    m_childRules.clear();
}

bool StyleSheetContents::wrapperInsertRule(StyleRuleBase* rule, unsigned index)
{
    ASSERT(m_isMutable);
    ASSERT_WITH_SECURITY_IMPLICATION(index <= ruleCount());

    if (index < m_importRules.size() || (index == m_importRules.size() && rule->isImportRule())) {
        // Inserting non-import rule before @import is not allowed.
        if (!rule->isImportRule())
            return false;

        StyleRuleImport* importRule = toStyleRuleImport(rule);
        if (importRule->mediaQueries())
            setHasMediaQueries();

        m_importRules.insert(index, importRule);
        m_importRules[index]->setParentStyleSheet(this);
        m_importRules[index]->requestStyleSheet();
        // FIXME: Stylesheet doesn't actually change meaningfully before the imported sheets are loaded.
        return true;
    }
    // Inserting @import rule after a non-import rule is not allowed.
    if (rule->isImportRule())
        return false;

    index -= m_importRules.size();

    if (index < m_namespaceRules.size() || (index == m_namespaceRules.size() && rule->isNamespaceRule())) {
        // Inserting non-namespace rules other than import rule before @namespace is not allowed.
        if (!rule->isNamespaceRule())
            return false;
        // Inserting @namespace rule when rules other than import/namespace/charset are present is not allowed.
        if (!m_childRules.isEmpty())
            return false;

        StyleRuleNamespace* namespaceRule = toStyleRuleNamespace(rule);
        m_namespaceRules.insert(index, namespaceRule);
        // For now to be compatible with IE and Firefox if namespace rule with same prefix is added
        // irrespective of adding the rule at any index, last added rule's value is considered.
        // TODO (ramya.v@samsung.com): As per spec last valid rule should be considered,
        // which means if namespace rule is added in the middle of existing namespace rules,
        // rule which comes later in rule list with same prefix needs to be considered.
        parserAddNamespace(namespaceRule->prefix(), namespaceRule->uri());
        return true;
    }

    if (rule->isNamespaceRule())
        return false;

    index -= m_namespaceRules.size();

    m_childRules.insert(index, rule);
    return true;
}

bool StyleSheetContents::wrapperDeleteRule(unsigned index)
{
    ASSERT(m_isMutable);
    ASSERT_WITH_SECURITY_IMPLICATION(index < ruleCount());

    if (index < m_importRules.size()) {
        m_importRules[index]->clearParentStyleSheet();
        if (m_importRules[index]->isFontFaceRule())
            notifyRemoveFontFaceRule(toStyleRuleFontFace(m_importRules[index].get()));
        m_importRules.remove(index);
        return true;
    }
    index -= m_importRules.size();

    if (index < m_namespaceRules.size()) {
        if (!m_childRules.isEmpty())
            return false;
        m_namespaceRules.remove(index);
        return true;
    }
    index -= m_namespaceRules.size();

    if (m_childRules[index]->isFontFaceRule())
        notifyRemoveFontFaceRule(toStyleRuleFontFace(m_childRules[index].get()));
    m_childRules.remove(index);
    return true;
}

void StyleSheetContents::parserAddNamespace(const AtomicString& prefix, const AtomicString& uri)
{
    ASSERT(!uri.isNull());
    if (prefix.isNull()) {
        m_defaultNamespace = uri;
        return;
    }
    PrefixNamespaceURIMap::AddResult result = m_namespaces.add(prefix, uri);
    if (result.isNewEntry)
        return;
    result.storedValue->value = uri;
}

const AtomicString& StyleSheetContents::namespaceURIFromPrefix(const AtomicString& prefix)
{
    return m_namespaces.get(prefix);
}

void StyleSheetContents::parseAuthorStyleSheet(const CSSStyleSheetResource* cachedStyleSheet, const SecurityOrigin* securityOrigin)
{
    TRACE_EVENT1("blink,devtools.timeline", "ParseAuthorStyleSheet", "data", InspectorParseAuthorStyleSheetEvent::data(cachedStyleSheet));

    bool isSameOriginRequest = securityOrigin && securityOrigin->canRequest(baseURL());

    // When the response was fetched via the Service Worker, the original URL may not be same as the base URL.
    // TODO(horo): When we will use the original URL as the base URL, we can remove this check. crbug.com/553535
    if (cachedStyleSheet->response().wasFetchedViaServiceWorker()) {
        const KURL originalURL(cachedStyleSheet->response().originalURLViaServiceWorker());
        // |originalURL| is empty when the response is created in the SW.
        if (!originalURL.isEmpty() && !securityOrigin->canRequest(originalURL))
            isSameOriginRequest = false;
    }

    CSSStyleSheetResource::MIMETypeCheck mimeTypeCheck = isQuirksModeBehavior(m_parserContext.mode()) && isSameOriginRequest ? CSSStyleSheetResource::MIMETypeCheck::Lax : CSSStyleSheetResource::MIMETypeCheck::Strict;
    String sheetText = cachedStyleSheet->sheetText(mimeTypeCheck);

    const ResourceResponse& response = cachedStyleSheet->response();
    m_sourceMapURL = response.httpHeaderField(HTTPNames::SourceMap);
    if (m_sourceMapURL.isEmpty()) {
        // Try to get deprecated header.
        m_sourceMapURL = response.httpHeaderField(HTTPNames::X_SourceMap);
    }

    CSSParserContext context(parserContext(), UseCounter::getFrom(this));
    CSSParser::parseSheet(context, this, sheetText);
}

void StyleSheetContents::parseString(const String& sheetText)
{
    parseStringAtPosition(sheetText, TextPosition::minimumPosition());
}

void StyleSheetContents::parseStringAtPosition(const String& sheetText, const TextPosition& startPosition)
{
    CSSParserContext context(parserContext(), UseCounter::getFrom(this));
    CSSParser::parseSheet(context, this, sheetText);
}

bool StyleSheetContents::isLoading() const
{
    for (unsigned i = 0; i < m_importRules.size(); ++i) {
        if (m_importRules[i]->isLoading())
            return true;
    }
    return false;
}

bool StyleSheetContents::loadCompleted() const
{
    StyleSheetContents* parentSheet = parentStyleSheet();
    if (parentSheet)
        return parentSheet->loadCompleted();

    StyleSheetContents* root = rootStyleSheet();
    return root->m_loadingClients.isEmpty();
}

void StyleSheetContents::checkLoaded()
{
    if (isLoading())
        return;

    StyleSheetContents* parentSheet = parentStyleSheet();
    if (parentSheet) {
        parentSheet->checkLoaded();
        return;
    }

    ASSERT(this == rootStyleSheet());
    if (m_loadingClients.isEmpty())
        return;

    // Avoid |CSSSStyleSheet| and |ownerNode| being deleted by scripts that run via
    // ScriptableDocumentParser::executeScriptsWaitingForResources(). Also protect
    // the |CSSStyleSheet| from being deleted during iteration via the |sheetLoaded|
    // method.
    //
    // When a sheet is loaded it is moved from the set of loading clients
    // to the set of completed clients. We therefore need the copy in order to
    // not modify the set while iterating it.
    HeapVector<Member<CSSStyleSheet>> loadingClients;
    copyToVector(m_loadingClients, loadingClients);

    for (unsigned i = 0; i < loadingClients.size(); ++i) {
        if (loadingClients[i]->loadCompleted())
            continue;

        // sheetLoaded might be invoked after its owner node is removed from document.
        if (Node* ownerNode = loadingClients[i]->ownerNode()) {
            if (loadingClients[i]->sheetLoaded())
                ownerNode->notifyLoadedSheetAndAllCriticalSubresources(m_didLoadErrorOccur ? Node::ErrorOccurredLoadingSubresource : Node::NoErrorLoadingSubresource);
        }
    }
}

void StyleSheetContents::notifyLoadedSheet(const CSSStyleSheetResource* sheet)
{
    ASSERT(sheet);
    m_didLoadErrorOccur |= sheet->errorOccurred();
    // updateLayoutIgnorePendingStyleSheets can cause us to create the RuleSet on this
    // sheet before its imports have loaded. So clear the RuleSet when the imports
    // load since the import's subrules are flattened into its parent sheet's RuleSet.
    clearRuleSet();
}

void StyleSheetContents::startLoadingDynamicSheet()
{
    StyleSheetContents* root = rootStyleSheet();
    for (const auto& client : root->m_loadingClients)
        client->startLoadingDynamicSheet();
    // Copy the completed clients to a vector for iteration.
    // startLoadingDynamicSheet will move the style sheet from the
    // completed state to the loading state which modifies the set of
    // completed clients. We therefore need the copy in order to not
    // modify the set of completed clients while iterating it.
    HeapVector<Member<CSSStyleSheet>> completedClients;
    copyToVector(root->m_completedClients, completedClients);
    for (unsigned i = 0; i < completedClients.size(); ++i)
        completedClients[i]->startLoadingDynamicSheet();
}

StyleSheetContents* StyleSheetContents::rootStyleSheet() const
{
    const StyleSheetContents* root = this;
    while (root->parentStyleSheet())
        root = root->parentStyleSheet();
    return const_cast<StyleSheetContents*>(root);
}

bool StyleSheetContents::hasSingleOwnerNode() const
{
    return rootStyleSheet()->hasOneClient();
}

Node* StyleSheetContents::singleOwnerNode() const
{
    StyleSheetContents* root = rootStyleSheet();
    if (!root->hasOneClient())
        return nullptr;
    if (root->m_loadingClients.size())
        return (*root->m_loadingClients.begin())->ownerNode();
    return (*root->m_completedClients.begin())->ownerNode();
}

Document* StyleSheetContents::singleOwnerDocument() const
{
    StyleSheetContents* root = rootStyleSheet();
    return root->clientSingleOwnerDocument();
}

static bool childRulesHaveFailedOrCanceledSubresources(const HeapVector<Member<StyleRuleBase>>& rules)
{
    for (unsigned i = 0; i < rules.size(); ++i) {
        const StyleRuleBase* rule = rules[i].get();
        switch (rule->type()) {
        case StyleRuleBase::Style:
            if (toStyleRule(rule)->properties().hasFailedOrCanceledSubresources())
                return true;
            break;
        case StyleRuleBase::FontFace:
            if (toStyleRuleFontFace(rule)->properties().hasFailedOrCanceledSubresources())
                return true;
            break;
        case StyleRuleBase::Media:
            if (childRulesHaveFailedOrCanceledSubresources(toStyleRuleMedia(rule)->childRules()))
                return true;
            break;
        case StyleRuleBase::Charset:
        case StyleRuleBase::Import:
        case StyleRuleBase::Namespace:
            ASSERT_NOT_REACHED();
        case StyleRuleBase::Page:
        case StyleRuleBase::Keyframes:
        case StyleRuleBase::Keyframe:
        case StyleRuleBase::Supports:
        case StyleRuleBase::Viewport:
            break;
        }
    }
    return false;
}

bool StyleSheetContents::hasFailedOrCanceledSubresources() const
{
    ASSERT(isCacheableForResource());
    return childRulesHaveFailedOrCanceledSubresources(m_childRules);
}

Document* StyleSheetContents::clientSingleOwnerDocument() const
{
    if (!m_hasSingleOwnerDocument || clientSize() <= 0)
        return nullptr;

    if (m_loadingClients.size())
        return (*m_loadingClients.begin())->ownerDocument();
    return (*m_completedClients.begin())->ownerDocument();
}

StyleSheetContents* StyleSheetContents::parentStyleSheet() const
{
    return m_ownerRule ? m_ownerRule->parentStyleSheet() : nullptr;
}

void StyleSheetContents::registerClient(CSSStyleSheet* sheet)
{
    ASSERT(!m_loadingClients.contains(sheet) && !m_completedClients.contains(sheet));

    // InspectorCSSAgent::buildObjectForRule creates CSSStyleSheet without any owner node.
    if (!sheet->ownerDocument())
        return;

    if (Document* document = clientSingleOwnerDocument()) {
        if (sheet->ownerDocument() != document)
            m_hasSingleOwnerDocument = false;
    }
    m_loadingClients.add(sheet);
}

void StyleSheetContents::unregisterClient(CSSStyleSheet* sheet)
{
    m_loadingClients.remove(sheet);
    m_completedClients.remove(sheet);

    if (!sheet->ownerDocument() || !m_loadingClients.isEmpty() || !m_completedClients.isEmpty())
        return;

    if (m_hasSingleOwnerDocument)
        removeSheetFromCache(sheet->ownerDocument());
    m_hasSingleOwnerDocument = true;
}

void StyleSheetContents::clientLoadCompleted(CSSStyleSheet* sheet)
{
    ASSERT(m_loadingClients.contains(sheet) || !sheet->ownerDocument());
    m_loadingClients.remove(sheet);
    // In m_ownerNode->sheetLoaded, the CSSStyleSheet might be detached.
    // (i.e. clearOwnerNode was invoked.)
    // In this case, we don't need to add the stylesheet to completed clients.
    if (!sheet->ownerDocument())
        return;
    m_completedClients.add(sheet);
}

void StyleSheetContents::clientLoadStarted(CSSStyleSheet* sheet)
{
    ASSERT(m_completedClients.contains(sheet));
    m_completedClients.remove(sheet);
    m_loadingClients.add(sheet);
}

void StyleSheetContents::removeSheetFromCache(Document* document)
{
    ASSERT(document);
    document->styleEngine().removeSheet(this);
}

void StyleSheetContents::setReferencedFromResource(CSSStyleSheetResource* resource)
{
    DCHECK(resource);
    DCHECK(!isReferencedFromResource());
    DCHECK(isCacheableForResource());
    m_referencedFromResource = resource;
}

void StyleSheetContents::clearReferencedFromResource()
{
    DCHECK(isReferencedFromResource());
    DCHECK(isCacheableForResource());
    m_referencedFromResource = nullptr;
}

RuleSet& StyleSheetContents::ensureRuleSet(const MediaQueryEvaluator& medium, AddRuleFlags addRuleFlags)
{
    if (!m_ruleSet) {
        m_ruleSet = RuleSet::create();
        m_ruleSet->addRulesFromSheet(this, medium, addRuleFlags);
    }
    return *m_ruleSet.get();
}

static void clearResolvers(HeapHashSet<WeakMember<CSSStyleSheet>>& clients)
{
    for (const auto& sheet : clients) {
        if (Document* document = sheet->ownerDocument())
            document->styleEngine().clearResolver();
    }
}

void StyleSheetContents::clearRuleSet()
{
    if (StyleSheetContents* parentSheet = parentStyleSheet())
        parentSheet->clearRuleSet();

    // Don't want to clear the StyleResolver if the RuleSet hasn't been created
    // since we only clear the StyleResolver so that it's members are properly
    // updated in ScopedStyleResolver::addRulesFromSheet.
    if (!m_ruleSet)
        return;

    // Clearing the ruleSet means we need to recreate the styleResolver data structures.
    // See the StyleResolver calls in ScopedStyleResolver::addRulesFromSheet.
    clearResolvers(m_loadingClients);
    clearResolvers(m_completedClients);
    m_ruleSet.clear();
}

static void removeFontFaceRules(HeapHashSet<WeakMember<CSSStyleSheet>>& clients, const StyleRuleFontFace* fontFaceRule)
{
    for (const auto& sheet : clients) {
        if (Node* ownerNode = sheet->ownerNode())
            ownerNode->document().styleEngine().removeFontFaceRules(HeapVector<Member<const StyleRuleFontFace>>(1, fontFaceRule));
    }
}

void StyleSheetContents::notifyRemoveFontFaceRule(const StyleRuleFontFace* fontFaceRule)
{
    StyleSheetContents* root = rootStyleSheet();
    removeFontFaceRules(root->m_loadingClients, fontFaceRule);
    removeFontFaceRules(root->m_completedClients, fontFaceRule);
}

static void findFontFaceRulesFromRules(const HeapVector<Member<StyleRuleBase>>& rules, HeapVector<Member<const StyleRuleFontFace>>& fontFaceRules)
{
    for (unsigned i = 0; i < rules.size(); ++i) {
        StyleRuleBase* rule = rules[i].get();

        if (rule->isFontFaceRule()) {
            fontFaceRules.append(toStyleRuleFontFace(rule));
        } else if (rule->isMediaRule()) {
            StyleRuleMedia* mediaRule = toStyleRuleMedia(rule);
            // We cannot know whether the media rule matches or not, but
            // for safety, remove @font-face in the media rule (if exists).
            findFontFaceRulesFromRules(mediaRule->childRules(), fontFaceRules);
        }
    }
}

void StyleSheetContents::findFontFaceRules(HeapVector<Member<const StyleRuleFontFace>>& fontFaceRules)
{
    for (unsigned i = 0; i < m_importRules.size(); ++i) {
        if (!m_importRules[i]->styleSheet())
            continue;
        m_importRules[i]->styleSheet()->findFontFaceRules(fontFaceRules);
    }

    findFontFaceRulesFromRules(childRules(), fontFaceRules);
}

DEFINE_TRACE(StyleSheetContents)
{
    visitor->trace(m_ownerRule);
    visitor->trace(m_importRules);
    visitor->trace(m_namespaceRules);
    visitor->trace(m_childRules);
    visitor->trace(m_loadingClients);
    visitor->trace(m_completedClients);
    visitor->trace(m_ruleSet);
    visitor->trace(m_referencedFromResource);
}

} // namespace blink
