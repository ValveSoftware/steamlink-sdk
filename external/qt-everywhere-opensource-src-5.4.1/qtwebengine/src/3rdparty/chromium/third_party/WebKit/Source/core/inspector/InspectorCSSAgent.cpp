/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/inspector/InspectorCSSAgent.h"

#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "core/CSSPropertyNames.h"
#include "core/InspectorTypeBuilder.h"
#include "core/StylePropertyShorthand.h"
#include "core/css/CSSComputedStyleDeclaration.h"
#include "core/css/CSSDefaultStyleSheets.h"
#include "core/css/CSSImportRule.h"
#include "core/css/CSSMediaRule.h"
#include "core/css/CSSRule.h"
#include "core/css/CSSRuleList.h"
#include "core/css/CSSStyleRule.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/MediaList.h"
#include "core/css/MediaQuery.h"
#include "core/css/MediaValues.h"
#include "core/css/StylePropertySet.h"
#include "core/css/StyleRule.h"
#include "core/css/StyleSheet.h"
#include "core/css/StyleSheetContents.h"
#include "core/css/StyleSheetList.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Node.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLHeadElement.h"
#include "core/html/VoidCallback.h"
#include "core/inspector/InspectorHistory.h"
#include "core/inspector/InspectorPageAgent.h"
#include "core/inspector/InspectorResourceAgent.h"
#include "core/inspector/InspectorResourceContentLoader.h"
#include "core/inspector/InspectorState.h"
#include "core/inspector/InstrumentingAgents.h"
#include "core/loader/DocumentLoader.h"
#include "core/page/Page.h"
#include "core/rendering/InlineTextBox.h"
#include "core/rendering/RenderObject.h"
#include "core/rendering/RenderText.h"
#include "core/rendering/RenderTextFragment.h"
#include "platform/fonts/Font.h"
#include "platform/fonts/GlyphBuffer.h"
#include "platform/fonts/WidthIterator.h"
#include "platform/text/TextRun.h"
#include "wtf/CurrentTime.h"
#include "wtf/text/CString.h"
#include "wtf/text/StringConcatenate.h"

namespace CSSAgentState {
static const char cssAgentEnabled[] = "cssAgentEnabled";
}

typedef WebCore::InspectorBackendDispatcher::CSSCommandHandler::EnableCallback EnableCallback;

namespace WebCore {

enum ForcePseudoClassFlags {
    PseudoNone = 0,
    PseudoHover = 1 << 0,
    PseudoFocus = 1 << 1,
    PseudoActive = 1 << 2,
    PseudoVisited = 1 << 3
};

static unsigned computePseudoClassMask(JSONArray* pseudoClassArray)
{
    DEFINE_STATIC_LOCAL(String, active, ("active"));
    DEFINE_STATIC_LOCAL(String, hover, ("hover"));
    DEFINE_STATIC_LOCAL(String, focus, ("focus"));
    DEFINE_STATIC_LOCAL(String, visited, ("visited"));
    if (!pseudoClassArray || !pseudoClassArray->length())
        return PseudoNone;

    unsigned result = PseudoNone;
    for (size_t i = 0; i < pseudoClassArray->length(); ++i) {
        RefPtr<JSONValue> pseudoClassValue = pseudoClassArray->get(i);
        String pseudoClass;
        bool success = pseudoClassValue->asString(&pseudoClass);
        if (!success)
            continue;
        if (pseudoClass == active)
            result |= PseudoActive;
        else if (pseudoClass == hover)
            result |= PseudoHover;
        else if (pseudoClass == focus)
            result |= PseudoFocus;
        else if (pseudoClass == visited)
            result |= PseudoVisited;
    }

    return result;
}

class InspectorCSSAgent::StyleSheetAction : public InspectorHistory::Action {
    WTF_MAKE_NONCOPYABLE(StyleSheetAction);
public:
    StyleSheetAction(const String& name)
        : InspectorHistory::Action(name)
    {
    }
};

class InspectorCSSAgent::InspectorResourceContentLoaderCallback FINAL : public VoidCallback {
public:
    InspectorResourceContentLoaderCallback(InspectorCSSAgent*, PassRefPtr<EnableCallback>);
    virtual void handleEvent() OVERRIDE;

private:
    InspectorCSSAgent* m_cssAgent;
    RefPtr<EnableCallback> m_callback;
};

InspectorCSSAgent::InspectorResourceContentLoaderCallback::InspectorResourceContentLoaderCallback(InspectorCSSAgent* cssAgent, PassRefPtr<EnableCallback> callback)
    : m_cssAgent(cssAgent)
    , m_callback(callback)
{
}

void InspectorCSSAgent::InspectorResourceContentLoaderCallback::handleEvent()
{
    // enable always succeeds.
    if (!m_callback->isActive())
        return;

    m_cssAgent->wasEnabled();
    m_callback->sendSuccess();
}

class InspectorCSSAgent::SetStyleSheetTextAction FINAL : public InspectorCSSAgent::StyleSheetAction {
    WTF_MAKE_NONCOPYABLE(SetStyleSheetTextAction);
public:
    SetStyleSheetTextAction(InspectorStyleSheetBase* styleSheet, const String& text)
        : InspectorCSSAgent::StyleSheetAction("SetStyleSheetText")
        , m_styleSheet(styleSheet)
        , m_text(text)
    {
    }

    virtual bool perform(ExceptionState& exceptionState) OVERRIDE
    {
        if (!m_styleSheet->getText(&m_oldText))
            return false;
        return redo(exceptionState);
    }

    virtual bool undo(ExceptionState& exceptionState) OVERRIDE
    {
        return m_styleSheet->setText(m_oldText, exceptionState);
    }

    virtual bool redo(ExceptionState& exceptionState) OVERRIDE
    {
        return m_styleSheet->setText(m_text, exceptionState);
    }

    virtual String mergeId() OVERRIDE
    {
        return String::format("SetStyleSheetText %s", m_styleSheet->id().utf8().data());
    }

    virtual void merge(PassRefPtrWillBeRawPtr<Action> action) OVERRIDE
    {
        ASSERT(action->mergeId() == mergeId());

        SetStyleSheetTextAction* other = static_cast<SetStyleSheetTextAction*>(action.get());
        m_text = other->m_text;
    }

private:
    RefPtr<InspectorStyleSheetBase> m_styleSheet;
    String m_text;
    String m_oldText;
};

class InspectorCSSAgent::SetPropertyTextAction FINAL : public InspectorCSSAgent::StyleSheetAction {
    WTF_MAKE_NONCOPYABLE(SetPropertyTextAction);
public:
    SetPropertyTextAction(InspectorStyleSheetBase* styleSheet, const InspectorCSSId& cssId, unsigned propertyIndex, const String& text, bool overwrite)
        : InspectorCSSAgent::StyleSheetAction("SetPropertyText")
        , m_styleSheet(styleSheet)
        , m_cssId(cssId)
        , m_propertyIndex(propertyIndex)
        , m_text(text)
        , m_overwrite(overwrite)
    {
    }

    virtual String toString() OVERRIDE
    {
        return mergeId() + ": " + m_oldStyleText + " -> " + m_text;
    }

    virtual bool perform(ExceptionState& exceptionState) OVERRIDE
    {
        return redo(exceptionState);
    }

    virtual bool undo(ExceptionState& exceptionState) OVERRIDE
    {
        String placeholder;
        return m_styleSheet->setStyleText(m_cssId, m_oldStyleText);
    }

    virtual bool redo(ExceptionState& exceptionState) OVERRIDE
    {
        if (!m_styleSheet->getStyleText(m_cssId, &m_oldStyleText))
            return false;
        bool result = m_styleSheet->setPropertyText(m_cssId, m_propertyIndex, m_text, m_overwrite, exceptionState);
        return result;
    }

    virtual String mergeId() OVERRIDE
    {
        return String::format("SetPropertyText %s:%u:%s", m_styleSheet->id().utf8().data(), m_propertyIndex, m_overwrite ? "true" : "false");
    }

    virtual void merge(PassRefPtrWillBeRawPtr<Action> action) OVERRIDE
    {
        ASSERT(action->mergeId() == mergeId());

        SetPropertyTextAction* other = static_cast<SetPropertyTextAction*>(action.get());
        m_text = other->m_text;
    }

private:
    RefPtr<InspectorStyleSheetBase> m_styleSheet;
    InspectorCSSId m_cssId;
    unsigned m_propertyIndex;
    String m_text;
    String m_oldStyleText;
    bool m_overwrite;
};

class InspectorCSSAgent::SetRuleSelectorAction FINAL : public InspectorCSSAgent::StyleSheetAction {
    WTF_MAKE_NONCOPYABLE(SetRuleSelectorAction);
public:
    SetRuleSelectorAction(InspectorStyleSheet* styleSheet, const InspectorCSSId& cssId, const String& selector)
        : InspectorCSSAgent::StyleSheetAction("SetRuleSelector")
        , m_styleSheet(styleSheet)
        , m_cssId(cssId)
        , m_selector(selector)
    {
    }

    virtual bool perform(ExceptionState& exceptionState) OVERRIDE
    {
        m_oldSelector = m_styleSheet->ruleSelector(m_cssId, exceptionState);
        if (exceptionState.hadException())
            return false;
        return redo(exceptionState);
    }

    virtual bool undo(ExceptionState& exceptionState) OVERRIDE
    {
        return m_styleSheet->setRuleSelector(m_cssId, m_oldSelector, exceptionState);
    }

    virtual bool redo(ExceptionState& exceptionState) OVERRIDE
    {
        return m_styleSheet->setRuleSelector(m_cssId, m_selector, exceptionState);
    }

private:
    RefPtr<InspectorStyleSheet> m_styleSheet;
    InspectorCSSId m_cssId;
    String m_selector;
    String m_oldSelector;
};

class InspectorCSSAgent::AddRuleAction FINAL : public InspectorCSSAgent::StyleSheetAction {
    WTF_MAKE_NONCOPYABLE(AddRuleAction);
public:
    AddRuleAction(InspectorStyleSheet* styleSheet, const String& selector)
        : InspectorCSSAgent::StyleSheetAction("AddRule")
        , m_styleSheet(styleSheet)
        , m_selector(selector)
    {
    }

    virtual bool perform(ExceptionState& exceptionState) OVERRIDE
    {
        return redo(exceptionState);
    }

    virtual bool undo(ExceptionState& exceptionState) OVERRIDE
    {
        return m_styleSheet->deleteRule(m_newId, exceptionState);
    }

    virtual bool redo(ExceptionState& exceptionState) OVERRIDE
    {
        CSSStyleRule* cssStyleRule = m_styleSheet->addRule(m_selector, exceptionState);
        if (exceptionState.hadException())
            return false;
        m_newId = m_styleSheet->ruleId(cssStyleRule);
        return true;
    }

    InspectorCSSId newRuleId() { return m_newId; }

private:
    RefPtr<InspectorStyleSheet> m_styleSheet;
    InspectorCSSId m_newId;
    String m_selector;
    String m_oldSelector;
};

// static
CSSStyleRule* InspectorCSSAgent::asCSSStyleRule(CSSRule* rule)
{
    if (!rule || rule->type() != CSSRule::STYLE_RULE)
        return 0;
    return toCSSStyleRule(rule);
}

InspectorCSSAgent::InspectorCSSAgent(InspectorDOMAgent* domAgent, InspectorPageAgent* pageAgent, InspectorResourceAgent* resourceAgent)
    : InspectorBaseAgent<InspectorCSSAgent>("CSS")
    , m_frontend(0)
    , m_domAgent(domAgent)
    , m_pageAgent(pageAgent)
    , m_resourceAgent(resourceAgent)
    , m_lastStyleSheetId(1)
    , m_styleSheetsPendingMutation(0)
    , m_styleDeclarationPendingMutation(false)
    , m_creatingViaInspectorStyleSheet(false)
    , m_isSettingStyleSheetText(false)
{
    m_domAgent->setDOMListener(this);
}

InspectorCSSAgent::~InspectorCSSAgent()
{
    ASSERT(!m_domAgent);
    reset();
}

void InspectorCSSAgent::setFrontend(InspectorFrontend* frontend)
{
    ASSERT(!m_frontend);
    m_frontend = frontend->css();
}

void InspectorCSSAgent::clearFrontend()
{
    ASSERT(m_frontend);
    ErrorString error;
    disable(&error);
    m_frontend = 0;
    resetNonPersistentData();
}

void InspectorCSSAgent::discardAgent()
{
    m_domAgent->setDOMListener(0);
    m_domAgent = 0;
}

void InspectorCSSAgent::restore()
{
    if (m_state->getBoolean(CSSAgentState::cssAgentEnabled))
        wasEnabled();
}

void InspectorCSSAgent::flushPendingFrontendMessages()
{
    if (!m_invalidatedDocuments.size())
        return;
    HashSet<Document*> invalidatedDocuments;
    m_invalidatedDocuments.swap(&invalidatedDocuments);
    for (HashSet<Document*>::iterator it = invalidatedDocuments.begin(); it != invalidatedDocuments.end(); ++it)
        updateActiveStyleSheets(*it, ExistingFrontendRefresh);
}

void InspectorCSSAgent::reset()
{
    m_idToInspectorStyleSheet.clear();
    m_idToInspectorStyleSheetForInlineStyle.clear();
    m_cssStyleSheetToInspectorStyleSheet.clear();
    m_documentToCSSStyleSheets.clear();
    m_invalidatedDocuments.clear();
    m_nodeToInspectorStyleSheet.clear();
    m_documentToViaInspectorStyleSheet.clear();
    resetNonPersistentData();
}

void InspectorCSSAgent::resetNonPersistentData()
{
    resetPseudoStates();
}

void InspectorCSSAgent::enable(ErrorString*, PassRefPtr<EnableCallback> prpCallback)
{
    m_state->setBoolean(CSSAgentState::cssAgentEnabled, true);
    if (!m_pageAgent->resourceContentLoader()) {
        wasEnabled();
        prpCallback->sendSuccess();
        return;
    }
    m_pageAgent->resourceContentLoader()->addListener(adoptPtr(new InspectorCSSAgent::InspectorResourceContentLoaderCallback(this, prpCallback)));
}

void InspectorCSSAgent::wasEnabled()
{
    if (!m_state->getBoolean(CSSAgentState::cssAgentEnabled)) {
        // We were disabled while fetching resources.
        return;
    }

    m_instrumentingAgents->setInspectorCSSAgent(this);
    Vector<Document*> documents = m_domAgent->documents();
    for (Vector<Document*>::iterator it = documents.begin(); it != documents.end(); ++it)
        updateActiveStyleSheets(*it, InitialFrontendLoad);
}

void InspectorCSSAgent::disable(ErrorString*)
{
    reset();
    m_instrumentingAgents->setInspectorCSSAgent(0);
    m_state->setBoolean(CSSAgentState::cssAgentEnabled, false);
}

void InspectorCSSAgent::didCommitLoadForMainFrame()
{
    reset();
}

void InspectorCSSAgent::mediaQueryResultChanged()
{
    flushPendingFrontendMessages();
    m_frontend->mediaQueryResultChanged();
}

void InspectorCSSAgent::willMutateRules()
{
    ++m_styleSheetsPendingMutation;
}

void InspectorCSSAgent::didMutateRules(CSSStyleSheet* styleSheet)
{
    --m_styleSheetsPendingMutation;
    ASSERT(m_styleSheetsPendingMutation >= 0);

    if (!styleSheetEditInProgress()) {
        Document* owner = styleSheet->ownerDocument();
        if (owner)
            owner->modifiedStyleSheet(styleSheet, FullStyleUpdate);
    }
}

void InspectorCSSAgent::willMutateStyle()
{
    m_styleDeclarationPendingMutation = true;
}

void InspectorCSSAgent::didMutateStyle(CSSStyleDeclaration* style, bool isInlineStyle)
{
    ASSERT(m_styleDeclarationPendingMutation);
    m_styleDeclarationPendingMutation = false;
    if (!styleSheetEditInProgress() && !isInlineStyle) {
        CSSStyleSheet* parentSheet = style->parentStyleSheet();
        Document* owner = parentSheet ? parentSheet->ownerDocument() : 0;
        if (owner)
            owner->modifiedStyleSheet(parentSheet, FullStyleUpdate);
    }
}

void InspectorCSSAgent::activeStyleSheetsUpdated(Document* document)
{
    if (styleSheetEditInProgress())
        return;
    m_invalidatedDocuments.add(document);
    if (m_creatingViaInspectorStyleSheet)
        flushPendingFrontendMessages();
}

void InspectorCSSAgent::updateActiveStyleSheets(Document* document, StyleSheetsUpdateType styleSheetsUpdateType)
{
    Vector<CSSStyleSheet*> newSheetsVector;
    InspectorCSSAgent::collectAllDocumentStyleSheets(document, newSheetsVector);
    setActiveStyleSheets(document, newSheetsVector, styleSheetsUpdateType);
}

void InspectorCSSAgent::setActiveStyleSheets(Document* document, const Vector<CSSStyleSheet*>& allSheetsVector, StyleSheetsUpdateType styleSheetsUpdateType)
{
    bool isInitialFrontendLoad = styleSheetsUpdateType == InitialFrontendLoad;

    HashSet<CSSStyleSheet*>* documentCSSStyleSheets = m_documentToCSSStyleSheets.get(document);
    if (!documentCSSStyleSheets) {
        documentCSSStyleSheets = new HashSet<CSSStyleSheet*>();
        OwnPtr<HashSet<CSSStyleSheet*> > documentCSSStyleSheetsPtr = adoptPtr(documentCSSStyleSheets);
        m_documentToCSSStyleSheets.set(document, documentCSSStyleSheetsPtr.release());
    }

    HashSet<CSSStyleSheet*> removedSheets(*documentCSSStyleSheets);
    Vector<CSSStyleSheet*> addedSheets;
    for (Vector<CSSStyleSheet*>::const_iterator it = allSheetsVector.begin(); it != allSheetsVector.end(); ++it) {
        CSSStyleSheet* cssStyleSheet = *it;
        if (removedSheets.contains(cssStyleSheet)) {
            removedSheets.remove(cssStyleSheet);
            if (isInitialFrontendLoad)
                addedSheets.append(cssStyleSheet);
        } else {
            addedSheets.append(cssStyleSheet);
        }
    }

    for (HashSet<CSSStyleSheet*>::iterator it = removedSheets.begin(); it != removedSheets.end(); ++it) {
        CSSStyleSheet* cssStyleSheet = *it;
        RefPtr<InspectorStyleSheet> inspectorStyleSheet = m_cssStyleSheetToInspectorStyleSheet.get(cssStyleSheet);
        ASSERT(inspectorStyleSheet);

        documentCSSStyleSheets->remove(cssStyleSheet);
        if (m_idToInspectorStyleSheet.contains(inspectorStyleSheet->id())) {
            String id = unbindStyleSheet(inspectorStyleSheet.get());
            if (m_frontend && !isInitialFrontendLoad)
                m_frontend->styleSheetRemoved(id);
        }
    }

    for (Vector<CSSStyleSheet*>::iterator it = addedSheets.begin(); it != addedSheets.end(); ++it) {
        CSSStyleSheet* cssStyleSheet = *it;
        bool isNew = isInitialFrontendLoad || !m_cssStyleSheetToInspectorStyleSheet.contains(cssStyleSheet);
        if (isNew) {
            InspectorStyleSheet* newStyleSheet = bindStyleSheet(cssStyleSheet);
            documentCSSStyleSheets->add(cssStyleSheet);
            if (m_frontend)
                m_frontend->styleSheetAdded(newStyleSheet->buildObjectForStyleSheetInfo());
        }
    }

    if (documentCSSStyleSheets->isEmpty())
        m_documentToCSSStyleSheets.remove(document);
}

void InspectorCSSAgent::documentDetached(Document* document)
{
    m_invalidatedDocuments.remove(document);
    setActiveStyleSheets(document, Vector<CSSStyleSheet*>(), ExistingFrontendRefresh);
}

bool InspectorCSSAgent::forcePseudoState(Element* element, CSSSelector::PseudoType pseudoType)
{
    if (m_nodeIdToForcedPseudoState.isEmpty())
        return false;

    int nodeId = m_domAgent->boundNodeId(element);
    if (!nodeId)
        return false;

    NodeIdToForcedPseudoState::iterator it = m_nodeIdToForcedPseudoState.find(nodeId);
    if (it == m_nodeIdToForcedPseudoState.end())
        return false;

    unsigned forcedPseudoState = it->value;
    switch (pseudoType) {
    case CSSSelector::PseudoActive:
        return forcedPseudoState & PseudoActive;
    case CSSSelector::PseudoFocus:
        return forcedPseudoState & PseudoFocus;
    case CSSSelector::PseudoHover:
        return forcedPseudoState & PseudoHover;
    case CSSSelector::PseudoVisited:
        return forcedPseudoState & PseudoVisited;
    default:
        return false;
    }
}

void InspectorCSSAgent::getMediaQueries(ErrorString* errorString, RefPtr<TypeBuilder::Array<TypeBuilder::CSS::CSSMedia> >& medias)
{
    medias = TypeBuilder::Array<TypeBuilder::CSS::CSSMedia>::create();
    for (IdToInspectorStyleSheet::iterator it = m_idToInspectorStyleSheet.begin(); it != m_idToInspectorStyleSheet.end(); ++it) {
        RefPtr<InspectorStyleSheet> styleSheet = it->value;
        collectMediaQueriesFromStyleSheet(styleSheet->pageStyleSheet(), medias.get());
        const CSSRuleVector& flatRules = styleSheet->flatRules();
        for (unsigned i = 0; i < flatRules.size(); ++i) {
            CSSRule* rule = flatRules.at(i).get();
            if (rule->type() == CSSRule::MEDIA_RULE)
                collectMediaQueriesFromRule(rule, medias.get());
        }
    }
}

void InspectorCSSAgent::getMatchedStylesForNode(ErrorString* errorString, int nodeId, const bool* includePseudo, const bool* includeInherited, RefPtr<TypeBuilder::Array<TypeBuilder::CSS::RuleMatch> >& matchedCSSRules, RefPtr<TypeBuilder::Array<TypeBuilder::CSS::PseudoIdMatches> >& pseudoIdMatches, RefPtr<TypeBuilder::Array<TypeBuilder::CSS::InheritedStyleEntry> >& inheritedEntries)
{
    Element* element = elementForId(errorString, nodeId);
    if (!element) {
        *errorString = "Node not found";
        return;
    }

    Element* originalElement = element;
    PseudoId elementPseudoId = element->pseudoId();
    if (elementPseudoId) {
        element = element->parentOrShadowHostElement();
        if (!element) {
            *errorString = "Pseudo element has no parent";
            return;
        }
    }

    Document* ownerDocument = element->ownerDocument();
    // A non-active document has no styles.
    if (!ownerDocument->isActive())
        return;

    // FIXME: It's really gross for the inspector to reach in and access StyleResolver
    // directly here. We need to provide the Inspector better APIs to get this information
    // without grabbing at internal style classes!

    // Matched rules.
    StyleResolver& styleResolver = ownerDocument->ensureStyleResolver();

    RefPtrWillBeRawPtr<CSSRuleList> matchedRules = styleResolver.pseudoCSSRulesForElement(element, elementPseudoId, StyleResolver::AllCSSRules);
    matchedCSSRules = buildArrayForMatchedRuleList(matchedRules.get(), originalElement);

    // Pseudo elements.
    if (!elementPseudoId && (!includePseudo || *includePseudo)) {
        RefPtr<TypeBuilder::Array<TypeBuilder::CSS::PseudoIdMatches> > pseudoElements = TypeBuilder::Array<TypeBuilder::CSS::PseudoIdMatches>::create();
        for (PseudoId pseudoId = FIRST_PUBLIC_PSEUDOID; pseudoId < AFTER_LAST_INTERNAL_PSEUDOID; pseudoId = static_cast<PseudoId>(pseudoId + 1)) {
            RefPtrWillBeRawPtr<CSSRuleList> matchedRules = styleResolver.pseudoCSSRulesForElement(element, pseudoId, StyleResolver::AllCSSRules);
            if (matchedRules && matchedRules->length()) {
                RefPtr<TypeBuilder::CSS::PseudoIdMatches> matches = TypeBuilder::CSS::PseudoIdMatches::create()
                    .setPseudoId(static_cast<int>(pseudoId))
                    .setMatches(buildArrayForMatchedRuleList(matchedRules.get(), element));
                pseudoElements->addItem(matches.release());
            }
        }

        pseudoIdMatches = pseudoElements.release();
    }

    // Inherited styles.
    if (!elementPseudoId && (!includeInherited || *includeInherited)) {
        RefPtr<TypeBuilder::Array<TypeBuilder::CSS::InheritedStyleEntry> > entries = TypeBuilder::Array<TypeBuilder::CSS::InheritedStyleEntry>::create();
        Element* parentElement = element->parentElement();
        while (parentElement) {
            StyleResolver& parentStyleResolver = parentElement->ownerDocument()->ensureStyleResolver();
            RefPtrWillBeRawPtr<CSSRuleList> parentMatchedRules = parentStyleResolver.cssRulesForElement(parentElement, StyleResolver::AllCSSRules);
            RefPtr<TypeBuilder::CSS::InheritedStyleEntry> entry = TypeBuilder::CSS::InheritedStyleEntry::create()
                .setMatchedCSSRules(buildArrayForMatchedRuleList(parentMatchedRules.get(), parentElement));
            if (parentElement->style() && parentElement->style()->length()) {
                InspectorStyleSheetForInlineStyle* styleSheet = asInspectorStyleSheet(parentElement);
                if (styleSheet)
                    entry->setInlineStyle(styleSheet->buildObjectForStyle(styleSheet->styleForId(InspectorCSSId(styleSheet->id(), 0))));
            }

            entries->addItem(entry.release());
            parentElement = parentElement->parentElement();
        }

        inheritedEntries = entries.release();
    }
}

void InspectorCSSAgent::getInlineStylesForNode(ErrorString* errorString, int nodeId, RefPtr<TypeBuilder::CSS::CSSStyle>& inlineStyle, RefPtr<TypeBuilder::CSS::CSSStyle>& attributesStyle)
{
    Element* element = elementForId(errorString, nodeId);
    if (!element)
        return;

    InspectorStyleSheetForInlineStyle* styleSheet = asInspectorStyleSheet(element);
    if (!styleSheet)
        return;

    inlineStyle = styleSheet->buildObjectForStyle(element->style());
    RefPtr<TypeBuilder::CSS::CSSStyle> attributes = buildObjectForAttributesStyle(element);
    attributesStyle = attributes ? attributes.release() : nullptr;
}

void InspectorCSSAgent::getComputedStyleForNode(ErrorString* errorString, int nodeId, RefPtr<TypeBuilder::Array<TypeBuilder::CSS::CSSComputedStyleProperty> >& style)
{
    Node* node = m_domAgent->assertNode(errorString, nodeId);
    if (!node)
        return;

    RefPtrWillBeRawPtr<CSSComputedStyleDeclaration> computedStyleInfo = CSSComputedStyleDeclaration::create(node, true);
    RefPtr<InspectorStyle> inspectorStyle = InspectorStyle::create(InspectorCSSId(), computedStyleInfo, 0);
    style = inspectorStyle->buildArrayForComputedStyle();
}

void InspectorCSSAgent::collectPlatformFontsForRenderer(RenderText* renderer, HashCountedSet<String>* fontStats)
{
    for (InlineTextBox* box = renderer->firstTextBox(); box; box = box->nextTextBox()) {
        RenderStyle* style = renderer->style(box->isFirstLineStyle());
        const Font& font = style->font();
        TextRun run = box->constructTextRunForInspector(style, font);
        WidthIterator it(&font, run, 0, false);
        GlyphBuffer glyphBuffer;
        it.advance(run.length(), &glyphBuffer);
        for (unsigned i = 0; i < glyphBuffer.size(); ++i) {
            String familyName = glyphBuffer.fontDataAt(i)->platformData().fontFamilyName();
            if (familyName.isNull())
                familyName = "";
            fontStats->add(familyName);
        }
    }
}

void InspectorCSSAgent::getPlatformFontsForNode(ErrorString* errorString, int nodeId,
    String* cssFamilyName, RefPtr<TypeBuilder::Array<TypeBuilder::CSS::PlatformFontUsage> >& platformFonts)
{
    Node* node = m_domAgent->assertNode(errorString, nodeId);
    if (!node)
        return;

    RefPtrWillBeRawPtr<CSSComputedStyleDeclaration> computedStyleInfo = CSSComputedStyleDeclaration::create(node, true);
    *cssFamilyName = computedStyleInfo->getPropertyValue(CSSPropertyFontFamily);

    Vector<Node*> textNodes;
    if (node->nodeType() == Node::TEXT_NODE) {
        if (node->renderer())
            textNodes.append(node);
    } else {
        for (Node* child = node->firstChild(); child; child = child->nextSibling()) {
            if (child->nodeType() == Node::TEXT_NODE && child->renderer())
                textNodes.append(child);
        }
    }

    HashCountedSet<String> fontStats;
    for (size_t i = 0; i < textNodes.size(); ++i) {
        RenderText* renderer = toRenderText(textNodes[i]->renderer());
        collectPlatformFontsForRenderer(renderer, &fontStats);
        if (renderer->isTextFragment()) {
            RenderTextFragment* textFragment = toRenderTextFragment(renderer);
            if (textFragment->firstLetter()) {
                RenderBoxModelObject* firstLetter = textFragment->firstLetter();
                for (RenderObject* current = firstLetter->slowFirstChild(); current; current = current->nextSibling()) {
                    if (current->isText())
                        collectPlatformFontsForRenderer(toRenderText(current), &fontStats);
                }
            }
        }
    }

    platformFonts = TypeBuilder::Array<TypeBuilder::CSS::PlatformFontUsage>::create();
    for (HashCountedSet<String>::iterator it = fontStats.begin(), end = fontStats.end(); it != end; ++it) {
        RefPtr<TypeBuilder::CSS::PlatformFontUsage> platformFont = TypeBuilder::CSS::PlatformFontUsage::create()
            .setFamilyName(it->key)
            .setGlyphCount(it->value);
        platformFonts->addItem(platformFont);
    }
}

void InspectorCSSAgent::getStyleSheetText(ErrorString* errorString, const String& styleSheetId, String* result)
{
    InspectorStyleSheetBase* inspectorStyleSheet = assertStyleSheetForId(errorString, styleSheetId);
    if (!inspectorStyleSheet)
        return;

    inspectorStyleSheet->getText(result);
}

void InspectorCSSAgent::setStyleSheetText(ErrorString* errorString, const String& styleSheetId, const String& text)
{
    InspectorStyleSheetBase* inspectorStyleSheet = assertStyleSheetForId(errorString, styleSheetId);
    if (!inspectorStyleSheet) {
        *errorString = "Style sheet with id " + styleSheetId + " not found";
        return;
    }

    TrackExceptionState exceptionState;
    m_domAgent->history()->perform(adoptRefWillBeNoop(new SetStyleSheetTextAction(inspectorStyleSheet, text)), exceptionState);
    *errorString = InspectorDOMAgent::toErrorString(exceptionState);
}

static bool extractRangeComponent(ErrorString* errorString, const RefPtr<JSONObject>& range, const String& component, unsigned& result)
{
    int parsedValue;
    if (!range->getNumber(component, &parsedValue) || parsedValue < 0) {
        *errorString = "range." + component + " must be a non-negative integer";
        return false;
    }
    result = parsedValue;
    return true;
}

static bool jsonRangeToSourceRange(ErrorString* errorString, InspectorStyleSheetBase* inspectorStyleSheet, const RefPtr<JSONObject>& range, SourceRange* sourceRange)
{
    unsigned startLineNumber;
    unsigned startColumn;
    unsigned endLineNumber;
    unsigned endColumn;
    if (!extractRangeComponent(errorString, range, "startLine", startLineNumber)
        || !extractRangeComponent(errorString, range, "startColumn", startColumn)
        || !extractRangeComponent(errorString, range, "endLine", endLineNumber)
        || !extractRangeComponent(errorString, range, "endColumn", endColumn))
        return false;

    unsigned startOffset;
    unsigned endOffset;
    bool success = inspectorStyleSheet->lineNumberAndColumnToOffset(startLineNumber, startColumn, &startOffset)
        && inspectorStyleSheet->lineNumberAndColumnToOffset(endLineNumber, endColumn, &endOffset);
    if (!success) {
        *errorString = "Specified range is out of bounds";
        return false;
    }

    if (startOffset > endOffset) {
        *errorString = "Range start must not succeed its end";
        return false;
    }
    sourceRange->start = startOffset;
    sourceRange->end = endOffset;
    return true;
}

void InspectorCSSAgent::setPropertyText(ErrorString* errorString, const String& styleSheetId, const RefPtr<JSONObject>& range, const String& text, RefPtr<TypeBuilder::CSS::CSSStyle>& result)
{
    InspectorStyleSheetBase* inspectorStyleSheet = assertStyleSheetForId(errorString, styleSheetId);
    if (!inspectorStyleSheet)
        return;
    SourceRange propertyRange;
    if (!jsonRangeToSourceRange(errorString, inspectorStyleSheet, range, &propertyRange))
        return;
    InspectorCSSId compoundId;
    unsigned propertyIndex;
    bool overwrite;
    if (!inspectorStyleSheet->findPropertyByRange(propertyRange, &compoundId, &propertyIndex, &overwrite)) {
        *errorString = "Source range didn't match any existing property source range nor any property insertion point";
        return;
    }

    TrackExceptionState exceptionState;
    bool success = m_domAgent->history()->perform(adoptRefWillBeNoop(new SetPropertyTextAction(inspectorStyleSheet, compoundId, propertyIndex, text, overwrite)), exceptionState);
    if (success)
        result = inspectorStyleSheet->buildObjectForStyle(inspectorStyleSheet->styleForId(compoundId));
    *errorString = InspectorDOMAgent::toErrorString(exceptionState);
}

void InspectorCSSAgent::setRuleSelector(ErrorString* errorString, const String& styleSheetId, const RefPtr<JSONObject>& range, const String& selector, RefPtr<TypeBuilder::CSS::CSSRule>& result)
{
    InspectorStyleSheet* inspectorStyleSheet = assertInspectorStyleSheetForId(errorString, styleSheetId);
    if (!inspectorStyleSheet)
        return;
    SourceRange selectorRange;
    if (!jsonRangeToSourceRange(errorString, inspectorStyleSheet, range, &selectorRange))
        return;
    InspectorCSSId compoundId;
    if (!inspectorStyleSheet->findRuleBySelectorRange(selectorRange, &compoundId)) {
        *errorString = "Source range didn't match any rule selector source range";
        return;
    }

    TrackExceptionState exceptionState;
    bool success = m_domAgent->history()->perform(adoptRefWillBeNoop(new SetRuleSelectorAction(inspectorStyleSheet, compoundId, selector)), exceptionState);
    if (success) {
        CSSStyleRule* rule = inspectorStyleSheet->ruleForId(compoundId);
        result = inspectorStyleSheet->buildObjectForRule(rule, buildMediaListChain(rule));
    }
    *errorString = InspectorDOMAgent::toErrorString(exceptionState);
}

void InspectorCSSAgent::createStyleSheet(ErrorString* errorString, const String& frameId, TypeBuilder::CSS::StyleSheetId* outStyleSheetId)
{
    LocalFrame* frame = m_pageAgent->frameForId(frameId);
    if (!frame) {
        *errorString = "Frame not found";
        return;
    }

    Document* document = frame->document();
    if (!document) {
        *errorString = "Frame does not have a document";
        return;
    }

    InspectorStyleSheet* inspectorStyleSheet = viaInspectorStyleSheet(document, true);
    if (!inspectorStyleSheet) {
        *errorString = "No target stylesheet found";
        return;
    }

    updateActiveStyleSheets(document, ExistingFrontendRefresh);

    *outStyleSheetId = inspectorStyleSheet->id();
}

void InspectorCSSAgent::addRule(ErrorString* errorString, const String& styleSheetId, const String& selector, RefPtr<TypeBuilder::CSS::CSSRule>& result)
{
    InspectorStyleSheet* inspectorStyleSheet = assertInspectorStyleSheetForId(errorString, styleSheetId);
    if (!inspectorStyleSheet)
        return;

    TrackExceptionState exceptionState;
    RefPtrWillBeRawPtr<AddRuleAction> action = adoptRefWillBeNoop(new AddRuleAction(inspectorStyleSheet, selector));
    bool success = m_domAgent->history()->perform(action, exceptionState);
    if (!success) {
        *errorString = InspectorDOMAgent::toErrorString(exceptionState);
        return;
    }

    InspectorCSSId ruleId = action->newRuleId();
    CSSStyleRule* rule = inspectorStyleSheet->ruleForId(ruleId);
    result = inspectorStyleSheet->buildObjectForRule(rule, buildMediaListChain(rule));
}

void InspectorCSSAgent::forcePseudoState(ErrorString* errorString, int nodeId, const RefPtr<JSONArray>& forcedPseudoClasses)
{
    Element* element = m_domAgent->assertElement(errorString, nodeId);
    if (!element)
        return;

    unsigned forcedPseudoState = computePseudoClassMask(forcedPseudoClasses.get());
    NodeIdToForcedPseudoState::iterator it = m_nodeIdToForcedPseudoState.find(nodeId);
    unsigned currentForcedPseudoState = it == m_nodeIdToForcedPseudoState.end() ? 0 : it->value;
    bool needStyleRecalc = forcedPseudoState != currentForcedPseudoState;
    if (!needStyleRecalc)
        return;

    if (forcedPseudoState)
        m_nodeIdToForcedPseudoState.set(nodeId, forcedPseudoState);
    else
        m_nodeIdToForcedPseudoState.remove(nodeId);
    element->ownerDocument()->setNeedsStyleRecalc(SubtreeStyleChange);
}

PassRefPtr<TypeBuilder::CSS::CSSMedia> InspectorCSSAgent::buildMediaObject(const MediaList* media, MediaListSource mediaListSource, const String& sourceURL, CSSStyleSheet* parentStyleSheet)
{
    // Make certain compilers happy by initializing |source| up-front.
    TypeBuilder::CSS::CSSMedia::Source::Enum source = TypeBuilder::CSS::CSSMedia::Source::InlineSheet;
    switch (mediaListSource) {
    case MediaListSourceMediaRule:
        source = TypeBuilder::CSS::CSSMedia::Source::MediaRule;
        break;
    case MediaListSourceImportRule:
        source = TypeBuilder::CSS::CSSMedia::Source::ImportRule;
        break;
    case MediaListSourceLinkedSheet:
        source = TypeBuilder::CSS::CSSMedia::Source::LinkedSheet;
        break;
    case MediaListSourceInlineSheet:
        source = TypeBuilder::CSS::CSSMedia::Source::InlineSheet;
        break;
    }

    const MediaQuerySet* queries = media->queries();
    const WillBeHeapVector<OwnPtrWillBeMember<MediaQuery> >& queryVector = queries->queryVector();
    bool hasMediaQueryItems = false;
    RefPtr<TypeBuilder::Array<TypeBuilder::Array<TypeBuilder::CSS::MediaQueryExpression> > > mediaListArray = TypeBuilder::Array<TypeBuilder::Array<TypeBuilder::CSS::MediaQueryExpression> >::create();
    for (size_t i = 0; i < queryVector.size(); ++i) {
        MediaQuery* query = queryVector.at(i).get();
        const ExpressionHeapVector& expressions = query->expressions();
        RefPtr<TypeBuilder::Array<TypeBuilder::CSS::MediaQueryExpression> > expressionArray = TypeBuilder::Array<TypeBuilder::CSS::MediaQueryExpression>::create();
        bool hasExpressionItems = false;
        for (size_t j = 0; j < expressions.size(); ++j) {
            MediaQueryExp* mediaQueryExp = expressions.at(j).get();
            MediaQueryExpValue expValue = mediaQueryExp->expValue();
            if (!expValue.isValue)
                continue;
            const char* valueName = CSSPrimitiveValue::unitTypeToString(expValue.unit);
            RefPtr<TypeBuilder::CSS::MediaQueryExpression> mediaQueryExpression = TypeBuilder::CSS::MediaQueryExpression::create()
                .setValue(expValue.value)
                .setUnit(String(valueName))
                .setFeature(mediaQueryExp->mediaFeature());
            RefPtr<MediaValues> mediaValues = MediaValues::createDynamicIfFrameExists(parentStyleSheet->ownerDocument()->frame());
            int computedLength;
            if (mediaValues->computeLength(expValue.value, expValue.unit, computedLength))
                mediaQueryExpression->setComputedLength(computedLength);

            expressionArray->addItem(mediaQueryExpression);
            hasExpressionItems = true;
        }
        if (hasExpressionItems) {
            mediaListArray->addItem(expressionArray);
            hasMediaQueryItems = true;
        }
    }

    RefPtr<TypeBuilder::CSS::CSSMedia> mediaObject = TypeBuilder::CSS::CSSMedia::create()
        .setText(media->mediaText())
        .setSource(source);
    if (hasMediaQueryItems)
        mediaObject->setMediaList(mediaListArray);

    if (parentStyleSheet && mediaListSource != MediaListSourceLinkedSheet) {
        if (InspectorStyleSheet* inspectorStyleSheet = m_cssStyleSheetToInspectorStyleSheet.get(parentStyleSheet))
            mediaObject->setParentStyleSheetId(inspectorStyleSheet->id());
    }

    if (!sourceURL.isEmpty()) {
        mediaObject->setSourceURL(sourceURL);

        CSSRule* parentRule = media->parentRule();
        if (!parentRule)
            return mediaObject.release();
        InspectorStyleSheet* inspectorStyleSheet = bindStyleSheet(parentRule->parentStyleSheet());
        RefPtr<TypeBuilder::CSS::SourceRange> mediaRange = inspectorStyleSheet->ruleHeaderSourceRange(parentRule);
        if (mediaRange)
            mediaObject->setRange(mediaRange);
    }
    return mediaObject.release();
}

bool InspectorCSSAgent::collectMediaQueriesFromStyleSheet(CSSStyleSheet* styleSheet, TypeBuilder::Array<TypeBuilder::CSS::CSSMedia>* mediaArray)
{
    bool addedItems = false;
    MediaList* mediaList = styleSheet->media();
    String sourceURL;
    if (mediaList && mediaList->length()) {
        Document* doc = styleSheet->ownerDocument();
        if (doc)
            sourceURL = doc->url();
        else if (!styleSheet->contents()->baseURL().isEmpty())
            sourceURL = styleSheet->contents()->baseURL();
        else
            sourceURL = "";
        mediaArray->addItem(buildMediaObject(mediaList, styleSheet->ownerNode() ? MediaListSourceLinkedSheet : MediaListSourceInlineSheet, sourceURL, styleSheet));
        addedItems = true;
    }
    return addedItems;
}

bool InspectorCSSAgent::collectMediaQueriesFromRule(CSSRule* rule, TypeBuilder::Array<TypeBuilder::CSS::CSSMedia>* mediaArray)
{
    MediaList* mediaList;
    String sourceURL;
    CSSStyleSheet* parentStyleSheet = 0;
    bool isMediaRule = true;
    bool addedItems = false;
    if (rule->type() == CSSRule::MEDIA_RULE) {
        CSSMediaRule* mediaRule = toCSSMediaRule(rule);
        mediaList = mediaRule->media();
        parentStyleSheet = mediaRule->parentStyleSheet();
    } else if (rule->type() == CSSRule::IMPORT_RULE) {
        CSSImportRule* importRule = toCSSImportRule(rule);
        mediaList = importRule->media();
        parentStyleSheet = importRule->parentStyleSheet();
        isMediaRule = false;
    } else {
        mediaList = 0;
    }

    if (parentStyleSheet) {
        sourceURL = parentStyleSheet->contents()->baseURL();
        if (sourceURL.isEmpty())
            sourceURL = InspectorDOMAgent::documentURLString(parentStyleSheet->ownerDocument());
    } else {
        sourceURL = "";
    }

    if (mediaList && mediaList->length()) {
        mediaArray->addItem(buildMediaObject(mediaList, isMediaRule ? MediaListSourceMediaRule : MediaListSourceImportRule, sourceURL, parentStyleSheet));
        addedItems = true;
    }
    return addedItems;
}

PassRefPtr<TypeBuilder::Array<TypeBuilder::CSS::CSSMedia> > InspectorCSSAgent::buildMediaListChain(CSSRule* rule)
{
    if (!rule)
        return nullptr;
    RefPtr<TypeBuilder::Array<TypeBuilder::CSS::CSSMedia> > mediaArray = TypeBuilder::Array<TypeBuilder::CSS::CSSMedia>::create();
    bool hasItems = false;
    CSSRule* parentRule = rule;
    while (parentRule) {
        hasItems = collectMediaQueriesFromRule(parentRule, mediaArray.get()) || hasItems;
        if (parentRule->parentRule()) {
            parentRule = parentRule->parentRule();
        } else {
            CSSStyleSheet* styleSheet = parentRule->parentStyleSheet();
            while (styleSheet) {
                hasItems = collectMediaQueriesFromStyleSheet(styleSheet, mediaArray.get()) || hasItems;
                parentRule = styleSheet->ownerRule();
                if (parentRule)
                    break;
                styleSheet = styleSheet->parentStyleSheet();
            }
        }
    }
    return hasItems ? mediaArray : nullptr;
}

InspectorStyleSheetForInlineStyle* InspectorCSSAgent::asInspectorStyleSheet(Element* element)
{
    NodeToInspectorStyleSheet::iterator it = m_nodeToInspectorStyleSheet.find(element);
    if (it != m_nodeToInspectorStyleSheet.end())
        return it->value.get();

    CSSStyleDeclaration* style = element->isStyledElement() ? element->style() : 0;
    if (!style)
        return 0;

    String newStyleSheetId = String::number(m_lastStyleSheetId++);
    RefPtr<InspectorStyleSheetForInlineStyle> inspectorStyleSheet = InspectorStyleSheetForInlineStyle::create(newStyleSheetId, element, this);
    m_idToInspectorStyleSheetForInlineStyle.set(newStyleSheetId, inspectorStyleSheet);
    m_nodeToInspectorStyleSheet.set(element, inspectorStyleSheet);
    return inspectorStyleSheet.get();
}

Element* InspectorCSSAgent::elementForId(ErrorString* errorString, int nodeId)
{
    Node* node = m_domAgent->nodeForId(nodeId);
    if (!node) {
        *errorString = "No node with given id found";
        return 0;
    }
    if (!node->isElementNode()) {
        *errorString = "Not an element node";
        return 0;
    }
    return toElement(node);
}

// static
void InspectorCSSAgent::collectAllDocumentStyleSheets(Document* document, Vector<CSSStyleSheet*>& result)
{
    const WillBeHeapVector<RefPtrWillBeMember<CSSStyleSheet> > activeStyleSheets = document->styleEngine()->activeStyleSheetsForInspector();
    for (WillBeHeapVector<RefPtrWillBeMember<CSSStyleSheet> >::const_iterator it = activeStyleSheets.begin(); it != activeStyleSheets.end(); ++it) {
        CSSStyleSheet* styleSheet = it->get();
        InspectorCSSAgent::collectStyleSheets(styleSheet, result);
    }
}

// static
void InspectorCSSAgent::collectStyleSheets(CSSStyleSheet* styleSheet, Vector<CSSStyleSheet*>& result)
{
    result.append(styleSheet);
    for (unsigned i = 0, size = styleSheet->length(); i < size; ++i) {
        CSSRule* rule = styleSheet->item(i);
        if (rule->type() == CSSRule::IMPORT_RULE) {
            CSSStyleSheet* importedStyleSheet = toCSSImportRule(rule)->styleSheet();
            if (importedStyleSheet)
                InspectorCSSAgent::collectStyleSheets(importedStyleSheet, result);
        }
    }
}

InspectorStyleSheet* InspectorCSSAgent::bindStyleSheet(CSSStyleSheet* styleSheet)
{
    RefPtr<InspectorStyleSheet> inspectorStyleSheet = m_cssStyleSheetToInspectorStyleSheet.get(styleSheet);
    if (!inspectorStyleSheet) {
        String id = String::number(m_lastStyleSheetId++);
        Document* document = styleSheet->ownerDocument();
        inspectorStyleSheet = InspectorStyleSheet::create(m_pageAgent, m_resourceAgent, id, styleSheet, detectOrigin(styleSheet, document), InspectorDOMAgent::documentURLString(document), this);
        m_idToInspectorStyleSheet.set(id, inspectorStyleSheet);
        m_cssStyleSheetToInspectorStyleSheet.set(styleSheet, inspectorStyleSheet);
        if (m_creatingViaInspectorStyleSheet)
            m_documentToViaInspectorStyleSheet.add(document, inspectorStyleSheet);
    }
    return inspectorStyleSheet.get();
}

String InspectorCSSAgent::unbindStyleSheet(InspectorStyleSheet* inspectorStyleSheet)
{
    String id = inspectorStyleSheet->id();
    m_idToInspectorStyleSheet.remove(id);
    if (inspectorStyleSheet->pageStyleSheet())
        m_cssStyleSheetToInspectorStyleSheet.remove(inspectorStyleSheet->pageStyleSheet());
    return id;
}

InspectorStyleSheet* InspectorCSSAgent::viaInspectorStyleSheet(Document* document, bool createIfAbsent)
{
    if (!document) {
        ASSERT(!createIfAbsent);
        return 0;
    }

    if (!document->isHTMLDocument() && !document->isSVGDocument())
        return 0;

    RefPtr<InspectorStyleSheet> inspectorStyleSheet = m_documentToViaInspectorStyleSheet.get(document);
    if (inspectorStyleSheet || !createIfAbsent)
        return inspectorStyleSheet.get();

    TrackExceptionState exceptionState;
    RefPtrWillBeRawPtr<Element> styleElement = document->createElement("style", exceptionState);
    if (!exceptionState.hadException())
        styleElement->setAttribute("type", "text/css", exceptionState);
    if (!exceptionState.hadException()) {
        ContainerNode* targetNode;
        // HEAD is absent in ImageDocuments, for example.
        if (document->head())
            targetNode = document->head();
        else if (document->body())
            targetNode = document->body();
        else
            return 0;

        InlineStyleOverrideScope overrideScope(document);
        m_creatingViaInspectorStyleSheet = true;
        targetNode->appendChild(styleElement, exceptionState);
        // At this point the added stylesheet will get bound through the updateActiveStyleSheets() invocation.
        // We just need to pick the respective InspectorStyleSheet from m_documentToViaInspectorStyleSheet.
        m_creatingViaInspectorStyleSheet = false;
    }
    if (exceptionState.hadException())
        return 0;

    return m_documentToViaInspectorStyleSheet.get(document);
}

InspectorStyleSheet* InspectorCSSAgent::assertInspectorStyleSheetForId(ErrorString* errorString, const String& styleSheetId)
{
    IdToInspectorStyleSheet::iterator it = m_idToInspectorStyleSheet.find(styleSheetId);
    if (it == m_idToInspectorStyleSheet.end()) {
        *errorString = "No style sheet with given id found";
        return 0;
    }
    return it->value.get();
}

InspectorStyleSheetBase* InspectorCSSAgent::assertStyleSheetForId(ErrorString* errorString, const String& styleSheetId)
{
    String placeholder;
    InspectorStyleSheetBase* result = assertInspectorStyleSheetForId(&placeholder, styleSheetId);
    if (result)
        return result;
    IdToInspectorStyleSheetForInlineStyle::iterator it = m_idToInspectorStyleSheetForInlineStyle.find(styleSheetId);
    if (it == m_idToInspectorStyleSheetForInlineStyle.end()) {
        *errorString = "No style sheet with given id found";
        return 0;
    }
    return it->value.get();
}

TypeBuilder::CSS::StyleSheetOrigin::Enum InspectorCSSAgent::detectOrigin(CSSStyleSheet* pageStyleSheet, Document* ownerDocument)
{
    if (m_creatingViaInspectorStyleSheet)
        return TypeBuilder::CSS::StyleSheetOrigin::Inspector;

    TypeBuilder::CSS::StyleSheetOrigin::Enum origin = TypeBuilder::CSS::StyleSheetOrigin::Regular;
    if (pageStyleSheet && !pageStyleSheet->ownerNode() && pageStyleSheet->href().isEmpty())
        origin = TypeBuilder::CSS::StyleSheetOrigin::User_agent;
    else if (pageStyleSheet && pageStyleSheet->ownerNode() && pageStyleSheet->ownerNode()->isDocumentNode())
        origin = TypeBuilder::CSS::StyleSheetOrigin::User;
    else {
        InspectorStyleSheet* viaInspectorStyleSheetForOwner = viaInspectorStyleSheet(ownerDocument, false);
        if (viaInspectorStyleSheetForOwner && pageStyleSheet == viaInspectorStyleSheetForOwner->pageStyleSheet())
            origin = TypeBuilder::CSS::StyleSheetOrigin::Inspector;
    }
    return origin;
}

PassRefPtr<TypeBuilder::CSS::CSSRule> InspectorCSSAgent::buildObjectForRule(CSSStyleRule* rule)
{
    if (!rule)
        return nullptr;

    // CSSRules returned by StyleResolver::pseudoCSSRulesForElement lack parent pointers if they are coming from
    // user agent stylesheets. To work around this issue, we use CSSOM wrapper created by inspector.
    if (!rule->parentStyleSheet()) {
        if (!m_inspectorUserAgentStyleSheet)
            m_inspectorUserAgentStyleSheet = CSSStyleSheet::create(CSSDefaultStyleSheets::instance().defaultStyleSheet());
        rule->setParentStyleSheet(m_inspectorUserAgentStyleSheet.get());
    }
    return bindStyleSheet(rule->parentStyleSheet())->buildObjectForRule(rule, buildMediaListChain(rule));
}

static inline bool matchesPseudoElement(const CSSSelector* selector, PseudoId elementPseudoId)
{
    // According to http://www.w3.org/TR/css3-selectors/#pseudo-elements, "Only one pseudo-element may appear per selector."
    // As such, check the last selector in the tag history.
    for (; !selector->isLastInTagHistory(); ++selector) { }
    PseudoId selectorPseudoId = selector->matchesPseudoElement() ? CSSSelector::pseudoId(selector->pseudoType()) : NOPSEUDO;

    // FIXME: This only covers the case of matching pseudo-element selectors against PseudoElements.
    // We should come up with a solution for matching pseudo-element selectors against ordinary Elements, too.
    return selectorPseudoId == elementPseudoId;
}

PassRefPtr<TypeBuilder::Array<TypeBuilder::CSS::RuleMatch> > InspectorCSSAgent::buildArrayForMatchedRuleList(CSSRuleList* ruleList, Element* element)
{
    RefPtr<TypeBuilder::Array<TypeBuilder::CSS::RuleMatch> > result = TypeBuilder::Array<TypeBuilder::CSS::RuleMatch>::create();
    if (!ruleList)
        return result.release();

    for (unsigned i = 0, size = ruleList->length(); i < size; ++i) {
        CSSStyleRule* rule = asCSSStyleRule(ruleList->item(i));
        RefPtr<TypeBuilder::CSS::CSSRule> ruleObject = buildObjectForRule(rule);
        if (!ruleObject)
            continue;
        RefPtr<TypeBuilder::Array<int> > matchingSelectors = TypeBuilder::Array<int>::create();
        const CSSSelectorList& selectorList = rule->styleRule()->selectorList();
        long index = 0;
        PseudoId elementPseudoId = element->pseudoId();
        for (const CSSSelector* selector = selectorList.first(); selector; selector = CSSSelectorList::next(*selector)) {
            const CSSSelector* firstTagHistorySelector = selector;
            bool matched = false;
            if (elementPseudoId)
                matched = matchesPseudoElement(selector, elementPseudoId); // Modifies |selector|.
            matched |= element->matches(firstTagHistorySelector->selectorText(), IGNORE_EXCEPTION);
            if (matched)
                matchingSelectors->addItem(index);
            ++index;
        }
        RefPtr<TypeBuilder::CSS::RuleMatch> match = TypeBuilder::CSS::RuleMatch::create()
            .setRule(ruleObject.release())
            .setMatchingSelectors(matchingSelectors.release());
        result->addItem(match);
    }

    return result;
}

PassRefPtr<TypeBuilder::CSS::CSSStyle> InspectorCSSAgent::buildObjectForAttributesStyle(Element* element)
{
    if (!element->isStyledElement())
        return nullptr;

    // FIXME: Ugliness below.
    StylePropertySet* attributeStyle = const_cast<StylePropertySet*>(element->presentationAttributeStyle());
    if (!attributeStyle)
        return nullptr;

    MutableStylePropertySet* mutableAttributeStyle = toMutableStylePropertySet(attributeStyle);

    RefPtr<InspectorStyle> inspectorStyle = InspectorStyle::create(InspectorCSSId(), mutableAttributeStyle->ensureCSSStyleDeclaration(), 0);
    return inspectorStyle->buildObjectForStyle();
}

void InspectorCSSAgent::didRemoveDocument(Document* document)
{
    if (document)
        m_documentToViaInspectorStyleSheet.remove(document);
}

void InspectorCSSAgent::didRemoveDOMNode(Node* node)
{
    if (!node)
        return;

    int nodeId = m_domAgent->boundNodeId(node);
    if (nodeId)
        m_nodeIdToForcedPseudoState.remove(nodeId);

    NodeToInspectorStyleSheet::iterator it = m_nodeToInspectorStyleSheet.find(node);
    if (it == m_nodeToInspectorStyleSheet.end())
        return;

    m_idToInspectorStyleSheetForInlineStyle.remove(it->value->id());
    m_nodeToInspectorStyleSheet.remove(node);
}

void InspectorCSSAgent::didModifyDOMAttr(Element* element)
{
    if (!element)
        return;

    NodeToInspectorStyleSheet::iterator it = m_nodeToInspectorStyleSheet.find(element);
    if (it == m_nodeToInspectorStyleSheet.end())
        return;

    it->value->didModifyElementAttribute();
}

void InspectorCSSAgent::styleSheetChanged(InspectorStyleSheetBase* styleSheet)
{
    flushPendingFrontendMessages();
    m_frontend->styleSheetChanged(styleSheet->id());
}

void InspectorCSSAgent::willReparseStyleSheet()
{
    ASSERT(!m_isSettingStyleSheetText);
    m_isSettingStyleSheetText = true;
}

void InspectorCSSAgent::didReparseStyleSheet()
{
    ASSERT(m_isSettingStyleSheetText);
    m_isSettingStyleSheetText = false;
}

void InspectorCSSAgent::resetPseudoStates()
{
    HashSet<Document*> documentsToChange;
    for (NodeIdToForcedPseudoState::iterator it = m_nodeIdToForcedPseudoState.begin(), end = m_nodeIdToForcedPseudoState.end(); it != end; ++it) {
        Element* element = toElement(m_domAgent->nodeForId(it->key));
        if (element && element->ownerDocument())
            documentsToChange.add(element->ownerDocument());
    }

    m_nodeIdToForcedPseudoState.clear();
    for (HashSet<Document*>::iterator it = documentsToChange.begin(), end = documentsToChange.end(); it != end; ++it)
        (*it)->setNeedsStyleRecalc(SubtreeStyleChange);
}

} // namespace WebCore

