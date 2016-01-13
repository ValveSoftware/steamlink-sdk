/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nikolas Zimmermann <zimmermann@kde.org>
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

#include "config.h"
#include "core/dom/ScriptLoader.h"

#include "bindings/v8/ScriptController.h"
#include "bindings/v8/ScriptSourceCode.h"
#include "core/HTMLNames.h"
#include "core/SVGNames.h"
#include "core/dom/Document.h"
#include "core/events/Event.h"
#include "core/dom/IgnoreDestructiveWriteCountIncrementer.h"
#include "core/dom/ScriptLoaderClient.h"
#include "core/dom/ScriptRunner.h"
#include "core/dom/ScriptableDocumentParser.h"
#include "core/dom/Text.h"
#include "core/fetch/FetchRequest.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/fetch/ScriptResource.h"
#include "core/html/HTMLScriptElement.h"
#include "core/html/imports/HTMLImport.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/svg/SVGScriptElement.h"
#include "platform/MIMETypeRegistry.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "wtf/StdLibExtras.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/StringHash.h"

namespace WebCore {

ScriptLoader::ScriptLoader(Element* element, bool parserInserted, bool alreadyStarted)
    : m_element(element)
    , m_resource(0)
    , m_startLineNumber(WTF::OrdinalNumber::beforeFirst())
    , m_parserInserted(parserInserted)
    , m_isExternalScript(false)
    , m_alreadyStarted(alreadyStarted)
    , m_haveFiredLoad(false)
    , m_willBeParserExecuted(false)
    , m_readyToBeParserExecuted(false)
    , m_willExecuteWhenDocumentFinishedParsing(false)
    , m_forceAsync(!parserInserted)
    , m_willExecuteInOrder(false)
{
    ASSERT(m_element);
    if (parserInserted && element->document().scriptableDocumentParser() && !element->document().isInDocumentWrite())
        m_startLineNumber = element->document().scriptableDocumentParser()->lineNumber();
}

ScriptLoader::~ScriptLoader()
{
    stopLoadRequest();
}

void ScriptLoader::didNotifySubtreeInsertionsToDocument()
{
    if (!m_parserInserted)
        prepareScript(); // FIXME: Provide a real starting line number here.
}

void ScriptLoader::childrenChanged()
{
    if (!m_parserInserted && m_element->inDocument())
        prepareScript(); // FIXME: Provide a real starting line number here.
}

void ScriptLoader::handleSourceAttribute(const String& sourceUrl)
{
    if (ignoresLoadRequest() || sourceUrl.isEmpty())
        return;

    prepareScript(); // FIXME: Provide a real starting line number here.
}

void ScriptLoader::handleAsyncAttribute()
{
    m_forceAsync = false;
}

// Helper function
static bool isLegacySupportedJavaScriptLanguage(const String& language)
{
    // Mozilla 1.8 accepts javascript1.0 - javascript1.7, but WinIE 7 accepts only javascript1.1 - javascript1.3.
    // Mozilla 1.8 and WinIE 7 both accept javascript and livescript.
    // WinIE 7 accepts ecmascript and jscript, but Mozilla 1.8 doesn't.
    // Neither Mozilla 1.8 nor WinIE 7 accept leading or trailing whitespace.
    // We want to accept all the values that either of these browsers accept, but not other values.

    // FIXME: This function is not HTML5 compliant. These belong in the MIME registry as "text/javascript<version>" entries.
    typedef HashSet<String, CaseFoldingHash> LanguageSet;
    DEFINE_STATIC_LOCAL(LanguageSet, languages, ());
    if (languages.isEmpty()) {
        languages.add("javascript");
        languages.add("javascript1.0");
        languages.add("javascript1.1");
        languages.add("javascript1.2");
        languages.add("javascript1.3");
        languages.add("javascript1.4");
        languages.add("javascript1.5");
        languages.add("javascript1.6");
        languages.add("javascript1.7");
        languages.add("livescript");
        languages.add("ecmascript");
        languages.add("jscript");
    }

    return languages.contains(language);
}

void ScriptLoader::dispatchErrorEvent()
{
    m_element->dispatchEvent(Event::create(EventTypeNames::error));
}

void ScriptLoader::dispatchLoadEvent()
{
    if (ScriptLoaderClient* client = this->client())
        client->dispatchLoadEvent();
    setHaveFiredLoadEvent(true);
}

bool ScriptLoader::isScriptTypeSupported(LegacyTypeSupport supportLegacyTypes) const
{
    // FIXME: isLegacySupportedJavaScriptLanguage() is not valid HTML5. It is used here to maintain backwards compatibility with existing layout tests. The specific violations are:
    // - Allowing type=javascript. type= should only support MIME types, such as text/javascript.
    // - Allowing a different set of languages for language= and type=. language= supports Javascript 1.1 and 1.4-1.6, but type= does not.

    String type = client()->typeAttributeValue();
    String language = client()->languageAttributeValue();
    if (type.isEmpty() && language.isEmpty())
        return true; // Assume text/javascript.
    if (type.isEmpty()) {
        type = "text/" + language.lower();
        if (MIMETypeRegistry::isSupportedJavaScriptMIMEType(type) || isLegacySupportedJavaScriptLanguage(language))
            return true;
    } else if (MIMETypeRegistry::isSupportedJavaScriptMIMEType(type.stripWhiteSpace()) || (supportLegacyTypes == AllowLegacyTypeInTypeAttribute && isLegacySupportedJavaScriptLanguage(type))) {
        return true;
    }

    return false;
}

// http://dev.w3.org/html5/spec/Overview.html#prepare-a-script
bool ScriptLoader::prepareScript(const TextPosition& scriptStartPosition, LegacyTypeSupport supportLegacyTypes)
{
    if (m_alreadyStarted)
        return false;

    ScriptLoaderClient* client = this->client();

    bool wasParserInserted;
    if (m_parserInserted) {
        wasParserInserted = true;
        m_parserInserted = false;
    } else {
        wasParserInserted = false;
    }

    if (wasParserInserted && !client->asyncAttributeValue())
        m_forceAsync = true;

    // FIXME: HTML5 spec says we should check that all children are either comments or empty text nodes.
    if (!client->hasSourceAttribute() && !m_element->firstChild())
        return false;

    if (!m_element->inDocument())
        return false;

    if (!isScriptTypeSupported(supportLegacyTypes))
        return false;

    if (wasParserInserted) {
        m_parserInserted = true;
        m_forceAsync = false;
    }

    m_alreadyStarted = true;

    // FIXME: If script is parser inserted, verify it's still in the original document.
    Document& elementDocument = m_element->document();
    Document* contextDocument = elementDocument.contextDocument().get();

    if (!contextDocument || !contextDocument->allowExecutingScripts(m_element))
        return false;

    if (!isScriptForEventSupported())
        return false;

    if (!client->charsetAttributeValue().isEmpty())
        m_characterEncoding = client->charsetAttributeValue();
    else
        m_characterEncoding = elementDocument.charset();

    if (client->hasSourceAttribute()) {
        if (!fetchScript(client->sourceAttributeValue()))
            return false;
    }

    if (client->hasSourceAttribute() && client->deferAttributeValue() && m_parserInserted && !client->asyncAttributeValue()) {
        m_willExecuteWhenDocumentFinishedParsing = true;
        m_willBeParserExecuted = true;
    } else if (client->hasSourceAttribute() && m_parserInserted && !client->asyncAttributeValue()) {
        m_willBeParserExecuted = true;
    } else if (!client->hasSourceAttribute() && m_parserInserted && !elementDocument.isRenderingReady()) {
        m_willBeParserExecuted = true;
        m_readyToBeParserExecuted = true;
    } else if (client->hasSourceAttribute() && !client->asyncAttributeValue() && !m_forceAsync) {
        m_willExecuteInOrder = true;
        contextDocument->scriptRunner()->queueScriptForExecution(this, m_resource, ScriptRunner::IN_ORDER_EXECUTION);
        m_resource->addClient(this);
    } else if (client->hasSourceAttribute()) {
        contextDocument->scriptRunner()->queueScriptForExecution(this, m_resource, ScriptRunner::ASYNC_EXECUTION);
        m_resource->addClient(this);
    } else {
        // Reset line numbering for nested writes.
        TextPosition position = elementDocument.isInDocumentWrite() ? TextPosition() : scriptStartPosition;
        KURL scriptURL = (!elementDocument.isInDocumentWrite() && m_parserInserted) ? elementDocument.url() : KURL();
        executeScript(ScriptSourceCode(scriptContent(), scriptURL, position));
    }

    return true;
}

bool ScriptLoader::fetchScript(const String& sourceUrl)
{
    ASSERT(m_element);

    RefPtrWillBeRawPtr<Document> elementDocument(m_element->document());
    if (!m_element->inDocument() || m_element->document() != elementDocument)
        return false;

    ASSERT(!m_resource);
    if (!stripLeadingAndTrailingHTMLSpaces(sourceUrl).isEmpty()) {
        FetchRequest request(ResourceRequest(elementDocument->completeURL(sourceUrl)), m_element->localName());

        AtomicString crossOriginMode = m_element->fastGetAttribute(HTMLNames::crossoriginAttr);
        if (!crossOriginMode.isNull())
            request.setCrossOriginAccessControl(elementDocument->securityOrigin(), crossOriginMode);
        request.setCharset(scriptCharset());

        bool isValidScriptNonce = elementDocument->contentSecurityPolicy()->allowScriptNonce(m_element->fastGetAttribute(HTMLNames::nonceAttr));
        if (isValidScriptNonce)
            request.setContentSecurityCheck(DoNotCheckContentSecurityPolicy);

        m_resource = elementDocument->fetcher()->fetchScript(request);
        m_isExternalScript = true;
    }

    if (m_resource)
        return true;

    dispatchErrorEvent();
    return false;
}

bool isHTMLScriptLoader(Element* element)
{
    ASSERT(element);
    return isHTMLScriptElement(*element);
}

bool isSVGScriptLoader(Element* element)
{
    ASSERT(element);
    return isSVGScriptElement(*element);
}

void ScriptLoader::executeScript(const ScriptSourceCode& sourceCode)
{
    ASSERT(m_alreadyStarted);

    if (sourceCode.isEmpty())
        return;

    RefPtrWillBeRawPtr<Document> elementDocument(m_element->document());
    RefPtrWillBeRawPtr<Document> contextDocument = elementDocument->contextDocument().get();
    if (!contextDocument)
        return;

    LocalFrame* frame = contextDocument->frame();

    bool shouldBypassMainWorldContentSecurityPolicy = (frame && frame->script().shouldBypassMainWorldContentSecurityPolicy()) || elementDocument->contentSecurityPolicy()->allowScriptNonce(m_element->fastGetAttribute(HTMLNames::nonceAttr)) || elementDocument->contentSecurityPolicy()->allowScriptHash(sourceCode.source());

    if (!m_isExternalScript && (!shouldBypassMainWorldContentSecurityPolicy && !elementDocument->contentSecurityPolicy()->allowInlineScript(elementDocument->url(), m_startLineNumber)))
        return;

    if (m_isExternalScript) {
        ScriptResource* resource = m_resource ? m_resource.get() : sourceCode.resource();
        if (resource && !resource->mimeTypeAllowedByNosniff()) {
            contextDocument->addConsoleMessage(SecurityMessageSource, ErrorMessageLevel, "Refused to execute script from '" + resource->url().elidedString() + "' because its MIME type ('" + resource->mimeType() + "') is not executable, and strict MIME type checking is enabled.");
            return;
        }
    }

    if (frame) {
        const bool isImportedScript = contextDocument != elementDocument;
        // http://www.whatwg.org/specs/web-apps/current-work/#execute-the-script-block step 2.3
        // with additional support for HTML imports.
        IgnoreDestructiveWriteCountIncrementer ignoreDestructiveWriteCountIncrementer(m_isExternalScript || isImportedScript ? contextDocument.get() : 0);

        if (isHTMLScriptLoader(m_element))
            contextDocument->pushCurrentScript(toHTMLScriptElement(m_element));

        AccessControlStatus corsCheck = NotSharableCrossOrigin;
        if (!m_isExternalScript || (sourceCode.resource() && sourceCode.resource()->passesAccessControlCheck(m_element->document().securityOrigin())))
            corsCheck = SharableCrossOrigin;

        // Create a script from the script element node, using the script
        // block's source and the script block's type.
        // Note: This is where the script is compiled and actually executed.
        frame->script().executeScriptInMainWorld(sourceCode, corsCheck);

        if (isHTMLScriptLoader(m_element)) {
            ASSERT(contextDocument->currentScript() == m_element);
            contextDocument->popCurrentScript();
        }
    }
}

void ScriptLoader::stopLoadRequest()
{
    if (m_resource) {
        if (!m_willBeParserExecuted)
            m_resource->removeClient(this);
        m_resource = 0;
    }
}

void ScriptLoader::execute(ScriptResource* resource)
{
    ASSERT(!m_willBeParserExecuted);
    ASSERT(resource);
    if (resource->errorOccurred()) {
        dispatchErrorEvent();
    } else if (!resource->wasCanceled()) {
        executeScript(ScriptSourceCode(resource));
        dispatchLoadEvent();
    }
    resource->removeClient(this);
}

void ScriptLoader::notifyFinished(Resource* resource)
{
    ASSERT(!m_willBeParserExecuted);

    RefPtrWillBeRawPtr<Document> elementDocument(m_element->document());
    RefPtrWillBeRawPtr<Document> contextDocument = elementDocument->contextDocument().get();
    if (!contextDocument)
        return;

    // Resource possibly invokes this notifyFinished() more than
    // once because ScriptLoader doesn't unsubscribe itself from
    // Resource here and does it in execute() instead.
    // We use m_resource to check if this function is already called.
    ASSERT_UNUSED(resource, resource == m_resource);
    if (!m_resource)
        return;
    if (m_resource->errorOccurred()) {
        dispatchErrorEvent();
        contextDocument->scriptRunner()->notifyScriptLoadError(this, m_willExecuteInOrder ? ScriptRunner::IN_ORDER_EXECUTION : ScriptRunner::ASYNC_EXECUTION);
        return;
    }
    if (m_willExecuteInOrder)
        contextDocument->scriptRunner()->notifyScriptReady(this, ScriptRunner::IN_ORDER_EXECUTION);
    else
        contextDocument->scriptRunner()->notifyScriptReady(this, ScriptRunner::ASYNC_EXECUTION);

    m_resource = 0;
}

bool ScriptLoader::ignoresLoadRequest() const
{
    return m_alreadyStarted || m_isExternalScript || m_parserInserted || !element() || !element()->inDocument();
}

bool ScriptLoader::isScriptForEventSupported() const
{
    String eventAttribute = client()->eventAttributeValue();
    String forAttribute = client()->forAttributeValue();
    if (!eventAttribute.isEmpty() && !forAttribute.isEmpty()) {
        forAttribute = forAttribute.stripWhiteSpace();
        if (!equalIgnoringCase(forAttribute, "window"))
            return false;

        eventAttribute = eventAttribute.stripWhiteSpace();
        if (!equalIgnoringCase(eventAttribute, "onload") && !equalIgnoringCase(eventAttribute, "onload()"))
            return false;
    }
    return true;
}

String ScriptLoader::scriptContent() const
{
    return m_element->textFromChildren();
}

ScriptLoaderClient* ScriptLoader::client() const
{
    if (isHTMLScriptLoader(m_element))
        return toHTMLScriptElement(m_element);

    if (isSVGScriptLoader(m_element))
        return toSVGScriptElement(m_element);

    ASSERT_NOT_REACHED();
    return 0;
}

ScriptLoader* toScriptLoaderIfPossible(Element* element)
{
    if (isHTMLScriptLoader(element))
        return toHTMLScriptElement(element)->loader();

    if (isSVGScriptLoader(element))
        return toSVGScriptElement(element)->loader();

    return 0;
}

}
