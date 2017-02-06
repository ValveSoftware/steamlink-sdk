/*
 * Copyright (C) 2006, 2007 Rob Buis
 * Copyright (C) 2008 Apple, Inc. All rights reserved.
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

#include "core/dom/StyleElement.h"

#include "bindings/core/v8/ScriptController.h"
#include "core/css/MediaList.h"
#include "core/css/MediaQueryEvaluator.h"
#include "core/css/StyleSheetContents.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ScriptableDocumentParser.h"
#include "core/dom/StyleEngine.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/HTMLStyleElement.h"
#include "core/svg/SVGStyleElement.h"
#include "platform/TraceEvent.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

static bool isCSS(Element* element, const AtomicString& type)
{
    return type.isEmpty() || (element->isHTMLElement() ? equalIgnoringCase(type, "text/css") : (type == "text/css"));
}

StyleElement::StyleElement(Document* document, bool createdByParser)
    : m_createdByParser(createdByParser)
    , m_loading(false)
    , m_registeredAsCandidate(false)
    , m_startPosition(TextPosition::belowRangePosition())
{
    if (createdByParser && document && document->scriptableDocumentParser() && !document->isInDocumentWrite())
        m_startPosition = document->scriptableDocumentParser()->textPosition();
}

StyleElement::~StyleElement()
{
}

StyleElement::ProcessingResult StyleElement::processStyleSheet(Document& document, Element* element)
{
    TRACE_EVENT0("blink", "StyleElement::processStyleSheet");
    DCHECK(element);
    DCHECK(element->inShadowIncludingDocument());

    m_registeredAsCandidate = true;
    document.styleEngine().addStyleSheetCandidateNode(element);
    if (m_createdByParser)
        return ProcessingSuccessful;

    return process(element);
}

void StyleElement::insertedInto(Element* element, ContainerNode* insertionPoint)
{
    if (!insertionPoint->inShadowIncludingDocument() || !element->isInShadowTree())
        return;
    if (ShadowRoot* scope = element->containingShadowRoot())
        scope->registerScopedHTMLStyleChild();
}

void StyleElement::removedFrom(Element* element, ContainerNode* insertionPoint)
{
    if (!insertionPoint->inShadowIncludingDocument())
        return;

    ShadowRoot* shadowRoot = element->containingShadowRoot();
    if (!shadowRoot)
        shadowRoot = insertionPoint->containingShadowRoot();

    if (shadowRoot)
        shadowRoot->unregisterScopedHTMLStyleChild();

    Document& document = element->document();
    if (m_registeredAsCandidate) {
        document.styleEngine().removeStyleSheetCandidateNode(element, shadowRoot ? *toTreeScope(shadowRoot) : toTreeScope(document));
        m_registeredAsCandidate = false;
    }

    StyleSheet* removedSheet = m_sheet.get();

    if (m_sheet)
        clearSheet(element);
    if (removedSheet)
        document.styleEngine().setNeedsActiveStyleUpdate(removedSheet, AnalyzedStyleUpdate);
}

void StyleElement::clearDocumentData(Document& document, Element* element)
{
    if (m_sheet)
        m_sheet->clearOwnerNode();

    if (m_registeredAsCandidate) {
        DCHECK(element->inShadowIncludingDocument());
        document.styleEngine().removeStyleSheetCandidateNode(element, element->treeScope());
        m_registeredAsCandidate = false;
    }
}

StyleElement::ProcessingResult StyleElement::childrenChanged(Element* element)
{
    DCHECK(element);
    if (m_createdByParser)
        return ProcessingSuccessful;

    return process(element);
}

StyleElement::ProcessingResult StyleElement::finishParsingChildren(Element* element)
{
    DCHECK(element);
    ProcessingResult result = process(element);
    m_createdByParser = false;
    return result;
}

StyleElement::ProcessingResult StyleElement::process(Element* element)
{
    if (!element || !element->inShadowIncludingDocument())
        return ProcessingSuccessful;
    return createSheet(element, element->textFromChildren());
}

void StyleElement::clearSheet(Element* ownerElement)
{
    DCHECK(m_sheet);

    if (ownerElement && m_sheet->isLoading())
        ownerElement->document().styleEngine().removePendingSheet(ownerElement, m_styleEngineContext);

    m_sheet.release()->clearOwnerNode();
}

static bool shouldBypassMainWorldCSP(Element* element)
{
    // Main world CSP is bypassed within an isolated world.
    LocalFrame* frame = element->document().frame();
    if (frame && frame->script().shouldBypassMainWorldCSP())
        return true;

    // Main world CSP is bypassed for style elements in user agent shadow DOM.
    ShadowRoot* root = element->containingShadowRoot();
    if (root && root->type() == ShadowRootType::UserAgent)
        return true;

    return false;
}

StyleElement::ProcessingResult StyleElement::createSheet(Element* e, const String& text)
{
    DCHECK(e);
    DCHECK(e->inShadowIncludingDocument());
    Document& document = e->document();

    const ContentSecurityPolicy* csp = document.contentSecurityPolicy();
    bool passesContentSecurityPolicyChecks = shouldBypassMainWorldCSP(e)
        || csp->allowStyleWithHash(text, ContentSecurityPolicy::InlineType::Block)
        || csp->allowInlineStyle(e->document().url(), e->fastGetAttribute(HTMLNames::nonceAttr), m_startPosition.m_line, text);

    // Clearing the current sheet may remove the cache entry so create the new sheet first
    CSSStyleSheet* newSheet = nullptr;

    // If type is empty or CSS, this is a CSS style sheet.
    const AtomicString& type = this->type();
    if (isCSS(e, type) && passesContentSecurityPolicyChecks) {
        MediaQuerySet* mediaQueries = MediaQuerySet::create(media());

        MediaQueryEvaluator screenEval("screen", true);
        MediaQueryEvaluator printEval("print", true);
        if (screenEval.eval(mediaQueries) || printEval.eval(mediaQueries)) {
            m_loading = true;
            TextPosition startPosition = m_startPosition == TextPosition::belowRangePosition() ? TextPosition::minimumPosition() : m_startPosition;
            newSheet = document.styleEngine().createSheet(e, text, startPosition, m_styleEngineContext);
            newSheet->setMediaQueries(mediaQueries);
            m_loading = false;
        }
    }

    if (m_sheet)
        clearSheet(e);

    m_sheet = newSheet;
    if (m_sheet)
        m_sheet->contents()->checkLoaded();

    return passesContentSecurityPolicyChecks ? ProcessingSuccessful : ProcessingFatalError;
}

bool StyleElement::isLoading() const
{
    if (m_loading)
        return true;
    return m_sheet ? m_sheet->isLoading() : false;
}

bool StyleElement::sheetLoaded(Document& document)
{
    if (isLoading())
        return false;

    document.styleEngine().removePendingSheet(m_sheet->ownerNode(), m_styleEngineContext);
    return true;
}

void StyleElement::startLoadingDynamicSheet(Document& document)
{
    document.styleEngine().addPendingSheet(m_styleEngineContext);
}

DEFINE_TRACE(StyleElement)
{
    visitor->trace(m_sheet);
}

} // namespace blink
