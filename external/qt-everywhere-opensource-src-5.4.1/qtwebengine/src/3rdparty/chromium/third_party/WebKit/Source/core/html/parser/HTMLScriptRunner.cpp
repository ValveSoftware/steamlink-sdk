/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/html/parser/HTMLScriptRunner.h"

#include "bindings/v8/ScriptSourceCode.h"
#include "core/dom/Element.h"
#include "core/events/Event.h"
#include "core/dom/IgnoreDestructiveWriteCountIncrementer.h"
#include "core/dom/Microtask.h"
#include "core/dom/ScriptLoader.h"
#include "core/fetch/ScriptResource.h"
#include "core/frame/LocalFrame.h"
#include "core/html/parser/HTMLInputStream.h"
#include "core/html/parser/HTMLScriptRunnerHost.h"
#include "core/html/parser/NestingLevelIncrementer.h"
#include "platform/NotImplemented.h"

namespace WebCore {

using namespace HTMLNames;

HTMLScriptRunner::HTMLScriptRunner(Document* document, HTMLScriptRunnerHost* host)
    : m_document(document)
    , m_host(host)
    , m_scriptNestingLevel(0)
    , m_hasScriptsWaitingForResources(false)
{
    ASSERT(m_host);
}

HTMLScriptRunner::~HTMLScriptRunner()
{
#if ENABLE(OILPAN)
    // If the document is destructed without having explicitly
    // detached the parser (and this script runner object), perform
    // detach steps now. This will happen if the Document, the parser
    // and this script runner object are swept out in the same GC.
    detach();
#else
    // Verify that detach() has been called.
    ASSERT(!m_document);
#endif
}

void HTMLScriptRunner::detach()
{
    if (!m_document)
        return;

    if (m_parserBlockingScript.resource() && m_parserBlockingScript.watchingForLoad())
        stopWatchingForLoad(m_parserBlockingScript);

    while (!m_scriptsToExecuteAfterParsing.isEmpty()) {
        PendingScript pendingScript = m_scriptsToExecuteAfterParsing.takeFirst();
        if (pendingScript.resource() && pendingScript.watchingForLoad())
            stopWatchingForLoad(pendingScript);
    }
    m_document = nullptr;
}

static KURL documentURLForScriptExecution(Document* document)
{
    if (!document)
        return KURL();

    if (!document->frame()) {
        if (document->importsController())
            return document->url();
        return KURL();
    }

    // Use the URL of the currently active document for this frame.
    return document->frame()->document()->url();
}

inline PassRefPtrWillBeRawPtr<Event> createScriptLoadEvent()
{
    return Event::create(EventTypeNames::load);
}

ScriptSourceCode HTMLScriptRunner::sourceFromPendingScript(const PendingScript& script, bool& errorOccurred) const
{
    if (script.resource()) {
        errorOccurred = script.resource()->errorOccurred();
        ASSERT(script.resource()->isLoaded());
        return ScriptSourceCode(script.resource());
    }
    errorOccurred = false;
    return ScriptSourceCode(script.element()->textContent(), documentURLForScriptExecution(m_document), script.startingPosition());
}

bool HTMLScriptRunner::isPendingScriptReady(const PendingScript& script)
{
    m_hasScriptsWaitingForResources = !m_document->isScriptExecutionReady();
    if (m_hasScriptsWaitingForResources)
        return false;
    if (script.resource() && !script.resource()->isLoaded())
        return false;
    return true;
}

void HTMLScriptRunner::executeParsingBlockingScript()
{
    ASSERT(m_document);
    ASSERT(!isExecutingScript());
    ASSERT(m_document->isScriptExecutionReady());
    ASSERT(isPendingScriptReady(m_parserBlockingScript));

    InsertionPointRecord insertionPointRecord(m_host->inputStream());
    executePendingScriptAndDispatchEvent(m_parserBlockingScript, PendingScriptBlockingParser);
}

void HTMLScriptRunner::executePendingScriptAndDispatchEvent(PendingScript& pendingScript, PendingScriptType pendingScriptType)
{
    bool errorOccurred = false;
    ScriptSourceCode sourceCode = sourceFromPendingScript(pendingScript, errorOccurred);

    // Stop watching loads before executeScript to prevent recursion if the script reloads itself.
    if (pendingScript.resource() && pendingScript.watchingForLoad())
        stopWatchingForLoad(pendingScript);

    if (!isExecutingScript()) {
        Microtask::performCheckpoint();
        if (pendingScriptType == PendingScriptBlockingParser) {
            m_hasScriptsWaitingForResources = !m_document->isScriptExecutionReady();
            // The parser cannot be unblocked as a microtask requested another resource
            if (m_hasScriptsWaitingForResources)
                return;
        }
    }

    // Clear the pending script before possible rentrancy from executeScript()
    RefPtrWillBeRawPtr<Element> element = pendingScript.releaseElementAndClear();
    if (ScriptLoader* scriptLoader = toScriptLoaderIfPossible(element.get())) {
        NestingLevelIncrementer nestingLevelIncrementer(m_scriptNestingLevel);
        IgnoreDestructiveWriteCountIncrementer ignoreDestructiveWriteCountIncrementer(m_document);
        if (errorOccurred)
            scriptLoader->dispatchErrorEvent();
        else {
            ASSERT(isExecutingScript());
            scriptLoader->executeScript(sourceCode);
            element->dispatchEvent(createScriptLoadEvent());
        }
    }
    ASSERT(!isExecutingScript());
}

void HTMLScriptRunner::watchForLoad(PendingScript& pendingScript)
{
    ASSERT(!pendingScript.watchingForLoad());
    ASSERT(!pendingScript.resource()->isLoaded());
    // addClient() will call notifyFinished() if the load is complete.
    // Callers do not expect to be re-entered from this call, so they
    // should not become a client of an already-loaded Resource.
    pendingScript.resource()->addClient(this);
    pendingScript.setWatchingForLoad(true);
}

void HTMLScriptRunner::stopWatchingForLoad(PendingScript& pendingScript)
{
    ASSERT(pendingScript.watchingForLoad());
    pendingScript.resource()->removeClient(this);
    pendingScript.setWatchingForLoad(false);
}

void HTMLScriptRunner::notifyFinished(Resource* cachedResource)
{
    m_host->notifyScriptLoaded(cachedResource);
}

// Implements the steps for 'An end tag whose tag name is "script"'
// http://whatwg.org/html#scriptEndTag
// Script handling lives outside the tree builder to keep each class simple.
void HTMLScriptRunner::execute(PassRefPtrWillBeRawPtr<Element> scriptElement, const TextPosition& scriptStartPosition)
{
    ASSERT(scriptElement);
    // FIXME: If scripting is disabled, always just return.

    bool hadPreloadScanner = m_host->hasPreloadScanner();

    // Try to execute the script given to us.
    runScript(scriptElement.get(), scriptStartPosition);

    if (hasParserBlockingScript()) {
        if (isExecutingScript())
            return; // Unwind to the outermost HTMLScriptRunner::execute before continuing parsing.
        // If preload scanner got created, it is missing the source after the current insertion point. Append it and scan.
        if (!hadPreloadScanner && m_host->hasPreloadScanner())
            m_host->appendCurrentInputStreamToPreloadScannerAndScan();
        executeParsingBlockingScripts();
    }
}

bool HTMLScriptRunner::hasParserBlockingScript() const
{
    return !!m_parserBlockingScript.element();
}

void HTMLScriptRunner::executeParsingBlockingScripts()
{
    while (hasParserBlockingScript() && isPendingScriptReady(m_parserBlockingScript))
        executeParsingBlockingScript();
}

void HTMLScriptRunner::executeScriptsWaitingForLoad(Resource* resource)
{
    ASSERT(!isExecutingScript());
    ASSERT(hasParserBlockingScript());
    ASSERT_UNUSED(resource, m_parserBlockingScript.resource() == resource);
    ASSERT(m_parserBlockingScript.resource()->isLoaded());
    executeParsingBlockingScripts();
}

void HTMLScriptRunner::executeScriptsWaitingForResources()
{
    ASSERT(m_document);
    // Callers should check hasScriptsWaitingForResources() before calling
    // to prevent parser or script re-entry during </style> parsing.
    ASSERT(hasScriptsWaitingForResources());
    ASSERT(!isExecutingScript());
    ASSERT(m_document->isScriptExecutionReady());
    executeParsingBlockingScripts();
}

bool HTMLScriptRunner::executeScriptsWaitingForParsing()
{
    while (!m_scriptsToExecuteAfterParsing.isEmpty()) {
        ASSERT(!isExecutingScript());
        ASSERT(!hasParserBlockingScript());
        ASSERT(m_scriptsToExecuteAfterParsing.first().resource());
        if (!m_scriptsToExecuteAfterParsing.first().resource()->isLoaded()) {
            watchForLoad(m_scriptsToExecuteAfterParsing.first());
            return false;
        }
        PendingScript first = m_scriptsToExecuteAfterParsing.takeFirst();
        executePendingScriptAndDispatchEvent(first, PendingScriptDeferred);
        // FIXME: What is this m_document check for?
        if (!m_document)
            return false;
    }
    return true;
}

void HTMLScriptRunner::requestParsingBlockingScript(Element* element)
{
    if (!requestPendingScript(m_parserBlockingScript, element))
        return;

    ASSERT(m_parserBlockingScript.resource());

    // We only care about a load callback if resource is not already
    // in the cache. Callers will attempt to run the m_parserBlockingScript
    // if possible before returning control to the parser.
    if (!m_parserBlockingScript.resource()->isLoaded())
        watchForLoad(m_parserBlockingScript);
}

void HTMLScriptRunner::requestDeferredScript(Element* element)
{
    PendingScript pendingScript;
    if (!requestPendingScript(pendingScript, element))
        return;

    ASSERT(pendingScript.resource());
    m_scriptsToExecuteAfterParsing.append(pendingScript);
}

bool HTMLScriptRunner::requestPendingScript(PendingScript& pendingScript, Element* script) const
{
    ASSERT(!pendingScript.element());
    pendingScript.setElement(script);
    // This should correctly return 0 for empty or invalid srcValues.
    ScriptResource* resource = toScriptLoaderIfPossible(script)->resource().get();
    if (!resource) {
        notImplemented(); // Dispatch error event.
        return false;
    }
    pendingScript.setScriptResource(resource);
    return true;
}

// Implements the initial steps for 'An end tag whose tag name is "script"'
// http://whatwg.org/html#scriptEndTag
void HTMLScriptRunner::runScript(Element* script, const TextPosition& scriptStartPosition)
{
    ASSERT(m_document);
    ASSERT(!hasParserBlockingScript());
    {
        ScriptLoader* scriptLoader = toScriptLoaderIfPossible(script);

        // This contains both and ASSERTION and a null check since we should not
        // be getting into the case of a null script element, but seem to be from
        // time to time. The assertion is left in to help find those cases and
        // is being tracked by <https://bugs.webkit.org/show_bug.cgi?id=60559>.
        ASSERT(scriptLoader);
        if (!scriptLoader)
            return;

        ASSERT(scriptLoader->isParserInserted());

        if (!isExecutingScript())
            Microtask::performCheckpoint();

        InsertionPointRecord insertionPointRecord(m_host->inputStream());
        NestingLevelIncrementer nestingLevelIncrementer(m_scriptNestingLevel);

        scriptLoader->prepareScript(scriptStartPosition);

        if (!scriptLoader->willBeParserExecuted())
            return;

        if (scriptLoader->willExecuteWhenDocumentFinishedParsing()) {
            requestDeferredScript(script);
        } else if (scriptLoader->readyToBeParserExecuted()) {
            if (m_scriptNestingLevel == 1) {
                m_parserBlockingScript.setElement(script);
                m_parserBlockingScript.setStartingPosition(scriptStartPosition);
            } else {
                ScriptSourceCode sourceCode(script->textContent(), documentURLForScriptExecution(m_document), scriptStartPosition);
                scriptLoader->executeScript(sourceCode);
            }
        } else {
            requestParsingBlockingScript(script);
        }
    }
}

void HTMLScriptRunner::trace(Visitor* visitor)
{
    visitor->trace(m_document);
    visitor->trace(m_host);
    visitor->trace(m_parserBlockingScript);
    visitor->trace(m_scriptsToExecuteAfterParsing);
}

}
