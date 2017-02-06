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

#include "core/dom/ScriptLoader.h"

#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/ScriptSourceCode.h"
#include "core/HTMLNames.h"
#include "core/SVGNames.h"
#include "core/dom/Document.h"
#include "core/dom/IgnoreDestructiveWriteCountIncrementer.h"
#include "core/dom/ScriptLoaderClient.h"
#include "core/dom/ScriptRunner.h"
#include "core/dom/ScriptableDocumentParser.h"
#include "core/dom/Text.h"
#include "core/events/Event.h"
#include "core/fetch/AccessControlStatus.h"
#include "core/fetch/FetchRequest.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/fetch/ScriptResource.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/SubresourceIntegrity.h"
#include "core/frame/UseCounter.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/CrossOriginAttribute.h"
#include "core/html/HTMLScriptElement.h"
#include "core/html/imports/HTMLImport.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/svg/SVGScriptElement.h"
#include "platform/MIMETypeRegistry.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/WebFrameScheduler.h"
#include "wtf/StdLibExtras.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/StringHash.h"

namespace blink {

ScriptLoader::ScriptLoader(Element* element, bool parserInserted, bool alreadyStarted, bool createdDuringDocumentWrite)
    : m_element(element)
    , m_startLineNumber(WTF::OrdinalNumber::beforeFirst())
    , m_parserInserted(parserInserted)
    , m_isExternalScript(false)
    , m_alreadyStarted(alreadyStarted)
    , m_haveFiredLoad(false)
    , m_willBeParserExecuted(false)
    , m_readyToBeParserExecuted(false)
    , m_willExecuteInOrder(false)
    , m_willExecuteWhenDocumentFinishedParsing(false)
    , m_forceAsync(!parserInserted)
    , m_createdDuringDocumentWrite(createdDuringDocumentWrite)
{
    DCHECK(m_element);
    if (parserInserted && element->document().scriptableDocumentParser() && !element->document().isInDocumentWrite())
        m_startLineNumber = element->document().scriptableDocumentParser()->lineNumber();
}

ScriptLoader::~ScriptLoader()
{
}

DEFINE_TRACE(ScriptLoader)
{
    visitor->trace(m_element);
    visitor->trace(m_resource);
    visitor->trace(m_pendingScript);
    ScriptResourceClient::trace(visitor);
}

void ScriptLoader::didNotifySubtreeInsertionsToDocument()
{
    if (!m_parserInserted)
        prepareScript(); // FIXME: Provide a real starting line number here.
}

void ScriptLoader::childrenChanged()
{
    if (!m_parserInserted && m_element->inShadowIncludingDocument())
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

void ScriptLoader::detach()
{
    if (!m_pendingScript)
        return;
    m_pendingScript->dispose();
    m_pendingScript = nullptr;
}

// Helper function. Must take a lowercase language as input.
static bool isLegacySupportedJavaScriptLanguage(const String& language)
{
    // Mozilla 1.8 accepts javascript1.0 - javascript1.7, but WinIE 7 accepts only javascript1.1 - javascript1.3.
    // Mozilla 1.8 and WinIE 7 both accept javascript and livescript.
    // WinIE 7 accepts ecmascript and jscript, but Mozilla 1.8 doesn't.
    // Neither Mozilla 1.8 nor WinIE 7 accept leading or trailing whitespace.
    // We want to accept all the values that either of these browsers accept, but not other values.

    // FIXME: This function is not HTML5 compliant. These belong in the MIME registry as "text/javascript<version>" entries.
    DCHECK_EQ(language, language.lower());
    return language == "javascript"
        || language == "javascript1.0"
        || language == "javascript1.1"
        || language == "javascript1.2"
        || language == "javascript1.3"
        || language == "javascript1.4"
        || language == "javascript1.5"
        || language == "javascript1.6"
        || language == "javascript1.7"
        || language == "livescript"
        || language == "ecmascript"
        || language == "jscript";
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

bool ScriptLoader::isValidScriptTypeAndLanguage(const String& type, const String& language, LegacyTypeSupport supportLegacyTypes)
{
    // FIXME: isLegacySupportedJavaScriptLanguage() is not valid HTML5. It is used here to maintain backwards compatibility with existing layout tests. The specific violations are:
    // - Allowing type=javascript. type= should only support MIME types, such as text/javascript.
    // - Allowing a different set of languages for language= and type=. language= supports Javascript 1.1 and 1.4-1.6, but type= does not.
    if (type.isEmpty()) {
        String lowerLanguage = language.lower();
        return language.isEmpty() // assume text/javascript.
            || MIMETypeRegistry::isSupportedJavaScriptMIMEType("text/" + lowerLanguage)
            || isLegacySupportedJavaScriptLanguage(lowerLanguage);
    } else if (RuntimeEnabledFeatures::moduleScriptsEnabled() && type == "module") {
        return true;
    } else if (MIMETypeRegistry::isSupportedJavaScriptMIMEType(type.stripWhiteSpace()) || (supportLegacyTypes == AllowLegacyTypeInTypeAttribute && isLegacySupportedJavaScriptLanguage(type.lower()))) {
        return true;
    }

    return false;
}

bool ScriptLoader::isScriptTypeSupported(LegacyTypeSupport supportLegacyTypes) const
{
    return isValidScriptTypeAndLanguage(client()->typeAttributeValue(), client()->languageAttributeValue(), supportLegacyTypes);
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
    if (!client->hasSourceAttribute() && !m_element->hasChildren())
        return false;

    if (!m_element->inShadowIncludingDocument())
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
    Document* contextDocument = elementDocument.contextDocument();

    if (!contextDocument || !contextDocument->allowExecutingScripts(m_element))
        return false;

    if (!isScriptForEventSupported())
        return false;

    if (!client->charsetAttributeValue().isEmpty())
        m_characterEncoding = client->charsetAttributeValue();
    else
        m_characterEncoding = elementDocument.characterSet();

    if (client->hasSourceAttribute()) {
        FetchRequest::DeferOption defer = FetchRequest::NoDefer;
        if (!m_parserInserted || client->asyncAttributeValue() || client->deferAttributeValue())
            defer = FetchRequest::LazyLoad;
        if (!fetchScript(client->sourceAttributeValue(), defer))
            return false;
    }

    if (client->hasSourceAttribute() && client->deferAttributeValue() && m_parserInserted && !client->asyncAttributeValue()) {
        m_willExecuteWhenDocumentFinishedParsing = true;
        m_willBeParserExecuted = true;
    } else if (client->hasSourceAttribute() && m_parserInserted && !client->asyncAttributeValue()) {
        m_willBeParserExecuted = true;
    } else if (!client->hasSourceAttribute() && m_parserInserted && !elementDocument.isScriptExecutionReady()) {
        m_willBeParserExecuted = true;
        m_readyToBeParserExecuted = true;
    } else if (client->hasSourceAttribute() && !client->asyncAttributeValue() && !m_forceAsync) {
        m_pendingScript = PendingScript::create(m_element, m_resource.get());
        m_willExecuteInOrder = true;
        contextDocument->scriptRunner()->queueScriptForExecution(this, ScriptRunner::IN_ORDER_EXECUTION);
        // Note that watchForLoad can immediately call notifyFinished.
        m_pendingScript->watchForLoad(this);
    } else if (client->hasSourceAttribute()) {
        m_pendingScript = PendingScript::create(m_element, m_resource.get());
        LocalFrame* frame = m_element->document().frame();
        if (frame) {
            ScriptState* scriptState = ScriptState::forMainWorld(frame);
            if (scriptState)
                ScriptStreamer::startStreaming(m_pendingScript.get(), ScriptStreamer::Async, frame->settings(), scriptState, frame->frameScheduler()->loadingTaskRunner());
        }
        contextDocument->scriptRunner()->queueScriptForExecution(this, ScriptRunner::ASYNC_EXECUTION);
        // Note that watchForLoad can immediately call notifyFinished.
        m_pendingScript->watchForLoad(this);
    } else {
        // Reset line numbering for nested writes.
        TextPosition position = elementDocument.isInDocumentWrite() ? TextPosition() : scriptStartPosition;
        KURL scriptURL = (!elementDocument.isInDocumentWrite() && m_parserInserted) ? elementDocument.url() : KURL();
        if (!executeScript(ScriptSourceCode(scriptContent(), scriptURL, position))) {
            dispatchErrorEvent();
            return false;
        }
    }

    return true;
}

bool ScriptLoader::fetchScript(const String& sourceUrl, FetchRequest::DeferOption defer)
{
    DCHECK(m_element);

    Document* elementDocument = &(m_element->document());
    if (!m_element->inShadowIncludingDocument() || m_element->document() != elementDocument)
        return false;

    DCHECK(!m_resource);
    if (!stripLeadingAndTrailingHTMLSpaces(sourceUrl).isEmpty()) {
        FetchRequest request(ResourceRequest(elementDocument->completeURL(sourceUrl)), m_element->localName());

        CrossOriginAttributeValue crossOrigin = crossOriginAttributeValue(m_element->fastGetAttribute(HTMLNames::crossoriginAttr));
        if (crossOrigin != CrossOriginAttributeNotSet)
            request.setCrossOriginAccessControl(elementDocument->getSecurityOrigin(), crossOrigin);
        request.setCharset(scriptCharset());

        // Skip fetch-related CSP checks if dynamically injected script is whitelisted and this script is not parser-inserted.
        bool scriptPassesCSPDynamic = (!isParserInserted() && elementDocument->contentSecurityPolicy()->allowDynamic());

        request.setContentSecurityPolicyNonce(m_element->fastGetAttribute(HTMLNames::nonceAttr));

        if (scriptPassesCSPDynamic) {
            UseCounter::count(elementDocument->frame(), UseCounter::ScriptPassesCSPDynamic);
            request.setContentSecurityCheck(DoNotCheckContentSecurityPolicy);
        }
        request.setDefer(defer);

        String integrityAttr = m_element->fastGetAttribute(HTMLNames::integrityAttr);
        if (!integrityAttr.isEmpty()) {
            IntegrityMetadataSet metadataSet;
            SubresourceIntegrity::parseIntegrityAttribute(integrityAttr, metadataSet, elementDocument);
            request.setIntegrityMetadata(metadataSet);
        }

        m_resource = ScriptResource::fetch(request, elementDocument->fetcher());

        m_isExternalScript = true;
    }

    if (m_resource)
        return true;

    dispatchErrorEvent();
    return false;
}

bool isHTMLScriptLoader(Element* element)
{
    DCHECK(element);
    return isHTMLScriptElement(*element);
}

bool isSVGScriptLoader(Element* element)
{
    DCHECK(element);
    return isSVGScriptElement(*element);
}

void ScriptLoader::logScriptMimetype(ScriptResource* resource, LocalFrame* frame, String mimetype)
{
    String lowerMimetype = mimetype.lower();
    bool text = lowerMimetype.startsWith("text/");
    bool application = lowerMimetype.startsWith("application/");
    bool expectedJs = MIMETypeRegistry::isSupportedJavaScriptMIMEType(lowerMimetype) || (text && isLegacySupportedJavaScriptLanguage(lowerMimetype.substring(5)));
    bool sameOrigin = m_element->document().getSecurityOrigin()->canRequest(m_resource->url());
    if (expectedJs) {
        return;
    }
    UseCounter::Feature feature = sameOrigin ? (text ? UseCounter::SameOriginTextScript : application ? UseCounter::SameOriginApplicationScript : UseCounter::SameOriginOtherScript) : (text ? UseCounter::CrossOriginTextScript : application ? UseCounter::CrossOriginApplicationScript : UseCounter::CrossOriginOtherScript);
    UseCounter::count(frame, feature);
}

bool ScriptLoader::executeScript(const ScriptSourceCode& sourceCode, double* compilationFinishTime)
{
    DCHECK(m_alreadyStarted);

    if (sourceCode.isEmpty())
        return true;

    Document* elementDocument = &(m_element->document());
    Document* contextDocument = elementDocument->contextDocument();
    if (!contextDocument)
        return true;

    LocalFrame* frame = contextDocument->frame();

    const ContentSecurityPolicy* csp = elementDocument->contentSecurityPolicy();
    bool shouldBypassMainWorldCSP = (frame && frame->script().shouldBypassMainWorldCSP())
        || csp->allowScriptWithHash(sourceCode.source().toString(), ContentSecurityPolicy::InlineType::Block)
        || (!isParserInserted() && csp->allowDynamic());

    if (!m_isExternalScript && (!shouldBypassMainWorldCSP && !csp->allowInlineScript(elementDocument->url(), m_element->fastGetAttribute(HTMLNames::nonceAttr), m_startLineNumber, sourceCode.source().toString()))) {
        return false;
    }

    if (m_isExternalScript) {
        ScriptResource* resource = m_resource ? m_resource.get() : sourceCode.resource();
        if (resource) {
            if (!resource->mimeTypeAllowedByNosniff()) {
                contextDocument->addConsoleMessage(ConsoleMessage::create(SecurityMessageSource, ErrorMessageLevel, "Refused to execute script from '" + resource->url().elidedString() + "' because its MIME type ('" + resource->httpContentType() + "') is not executable, and strict MIME type checking is enabled."));
                return false;
            }

            String mimetype = resource->httpContentType();
            if (mimetype.startsWith("image/")) {
                contextDocument->addConsoleMessage(ConsoleMessage::create(SecurityMessageSource, ErrorMessageLevel, "Refused to execute script from '" + resource->url().elidedString() + "' because its MIME type ('" + mimetype + "') is not executable."));
                UseCounter::count(frame, UseCounter::BlockedSniffingImageToScript);
                return false;
            }

            logScriptMimetype(resource, frame, mimetype);
        }
    }

    // FIXME: Can this be moved earlier in the function?
    // Why are we ever attempting to execute scripts without a frame?
    if (!frame)
        return true;

    AccessControlStatus accessControlStatus = NotSharableCrossOrigin;
    if (!m_isExternalScript) {
        accessControlStatus = SharableCrossOrigin;
    } else if (sourceCode.resource()) {
        if (sourceCode.resource()->response().wasFetchedViaServiceWorker()) {
            if (sourceCode.resource()->response().serviceWorkerResponseType() == WebServiceWorkerResponseTypeOpaque)
                accessControlStatus = OpaqueResource;
            else
                accessControlStatus = SharableCrossOrigin;
        } else if (sourceCode.resource()->passesAccessControlCheck(m_element->document().getSecurityOrigin())) {
            accessControlStatus = SharableCrossOrigin;
        }
    }

    const bool isImportedScript = contextDocument != elementDocument;
    // http://www.whatwg.org/specs/web-apps/current-work/#execute-the-script-block step 2.3
    // with additional support for HTML imports.
    IgnoreDestructiveWriteCountIncrementer ignoreDestructiveWriteCountIncrementer(m_isExternalScript || isImportedScript ? contextDocument : 0);

    if (isHTMLScriptLoader(m_element) || isSVGScriptLoader(m_element))
        contextDocument->pushCurrentScript(m_element);

    // Create a script from the script element node, using the script
    // block's source and the script block's type.
    // Note: This is where the script is compiled and actually executed.
    frame->script().executeScriptInMainWorld(sourceCode, accessControlStatus, compilationFinishTime);

    if (isHTMLScriptLoader(m_element) || isSVGScriptLoader(m_element)) {
        DCHECK(contextDocument->currentScript() == m_element);
        contextDocument->popCurrentScript();
    }

    return true;
}

void ScriptLoader::execute()
{
    DCHECK(!m_willBeParserExecuted);
    DCHECK(m_pendingScript->resource());
    bool errorOccurred = false;
    ScriptSourceCode source = m_pendingScript->getSource(KURL(), errorOccurred);
    Element* element = m_pendingScript->releaseElementAndClear();
    ALLOW_UNUSED_LOCAL(element);
    if (errorOccurred) {
        dispatchErrorEvent();
    } else if (!m_resource->wasCanceled()) {
        if (executeScript(source))
            dispatchLoadEvent();
        else
            dispatchErrorEvent();
    }
    m_resource = nullptr;
}

void ScriptLoader::notifyFinished(Resource* resource)
{
    DCHECK(!m_willBeParserExecuted);

    Document* contextDocument = m_element->document().contextDocument();
    if (!contextDocument) {
        detach();
        return;
    }

    ASSERT_UNUSED(resource, resource == m_resource);

    ScriptRunner::ExecutionType runOrder = m_willExecuteInOrder ? ScriptRunner::IN_ORDER_EXECUTION : ScriptRunner::ASYNC_EXECUTION;
    if (m_resource->errorOccurred()) {
        contextDocument->scriptRunner()->notifyScriptLoadError(this, runOrder);
        detach();
        dispatchErrorEvent();
        return;
    }
    contextDocument->scriptRunner()->notifyScriptReady(this, runOrder);
    m_pendingScript->stopWatchingForLoad();
}

bool ScriptLoader::ignoresLoadRequest() const
{
    return m_alreadyStarted || m_isExternalScript || m_parserInserted || !element() || !element()->inShadowIncludingDocument();
}

bool ScriptLoader::isScriptForEventSupported() const
{
    String eventAttribute = client()->eventAttributeValue();
    String forAttribute = client()->forAttributeValue();
    if (eventAttribute.isNull() || forAttribute.isNull())
        return true;

    forAttribute = forAttribute.stripWhiteSpace();
    if (!equalIgnoringCase(forAttribute, "window"))
        return false;
    eventAttribute = eventAttribute.stripWhiteSpace();
    return equalIgnoringCase(eventAttribute, "onload") || equalIgnoringCase(eventAttribute, "onload()");
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

} // namespace blink
