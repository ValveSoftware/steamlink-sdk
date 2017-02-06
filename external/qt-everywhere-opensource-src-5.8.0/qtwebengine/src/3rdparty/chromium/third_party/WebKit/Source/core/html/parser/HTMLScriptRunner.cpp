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

#include "core/html/parser/HTMLScriptRunner.h"

#include "bindings/core/v8/Microtask.h"
#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/V8PerIsolateData.h"
#include "core/dom/DocumentParserTiming.h"
#include "core/dom/Element.h"
#include "core/dom/IgnoreDestructiveWriteCountIncrementer.h"
#include "core/dom/ScriptLoader.h"
#include "core/events/Event.h"
#include "core/fetch/ScriptResource.h"
#include "core/frame/LocalFrame.h"
#include "core/html/parser/HTMLInputStream.h"
#include "core/html/parser/HTMLScriptRunnerHost.h"
#include "core/html/parser/NestingLevelIncrementer.h"
#include "platform/Histogram.h"
#include "platform/TraceEvent.h"
#include "platform/TracedValue.h"
#include "public/platform/Platform.h"
#include "public/platform/WebFrameScheduler.h"
#include <inttypes.h>
#include <memory>

namespace blink {

namespace {

// TODO(bmcquade): move this to a shared location if we find ourselves wanting
// to trace similar data elsewhere in the codebase.
std::unique_ptr<TracedValue> getTraceArgsForScriptElement(Element* element, const TextPosition& textPosition)
{
    std::unique_ptr<TracedValue> value = TracedValue::create();
    ScriptLoader* scriptLoader = toScriptLoaderIfPossible(element);
    if (scriptLoader && scriptLoader->resource())
        value->setString("url", scriptLoader->resource()->url().getString());
    if (element->ownerDocument() && element->ownerDocument()->frame())
        value->setString("frame", String::format("0x%" PRIx64, static_cast<uint64_t>(reinterpret_cast<intptr_t>(element->ownerDocument()->frame()))));
    if (textPosition.m_line.zeroBasedInt() > 0 || textPosition.m_column.zeroBasedInt() > 0) {
        value->setInteger("lineNumber", textPosition.m_line.oneBasedInt());
        value->setInteger("columnNumber", textPosition.m_column.oneBasedInt());
    }
    return value;
}

bool doExecuteScript(Element* scriptElement, const ScriptSourceCode& sourceCode, const TextPosition& textPosition)
{
    ScriptLoader* scriptLoader = toScriptLoaderIfPossible(scriptElement);
    ASSERT(scriptLoader);
    TRACE_EVENT_WITH_FLOW1("blink", "HTMLScriptRunner ExecuteScript", scriptElement, TRACE_EVENT_FLAG_FLOW_IN,
        "data", getTraceArgsForScriptElement(scriptElement, textPosition));
    return scriptLoader->executeScript(sourceCode);
}

void traceParserBlockingScript(const PendingScript* pendingScript, bool waitingForResources)
{
    // The HTML parser must yield before executing script in the following
    // cases:
    // * the script's execution is blocked on the completed load of the script
    //   resource
    //   (https://html.spec.whatwg.org/multipage/scripting.html#pending-parsing-blocking-script)
    // * the script's execution is blocked on the load of a style sheet or other
    //   resources that are blocking scripts
    //   (https://html.spec.whatwg.org/multipage/semantics.html#a-style-sheet-that-is-blocking-scripts)
    //
    // Both of these cases can introduce significant latency when loading a
    // web page, especially for users on slow connections, since the HTML parser
    // must yield until the blocking resources finish loading.
    //
    // We trace these parser yields here using flow events, so we can track
    // both when these yields occur, as well as how long the parser had
    // to yield. The connecting flow events are traced once the parser becomes
    // unblocked when the script actually executes, in doExecuteScript.
    Element* element = pendingScript->element();
    if (!element)
        return;
    TextPosition scriptStartPosition = pendingScript->startingPosition();
    if (!pendingScript->isReady()) {
        if (waitingForResources) {
            TRACE_EVENT_WITH_FLOW1("blink", "YieldParserForScriptLoadAndBlockingResources", element, TRACE_EVENT_FLAG_FLOW_OUT,
                "data", getTraceArgsForScriptElement(element, scriptStartPosition));
        } else {
            TRACE_EVENT_WITH_FLOW1("blink", "YieldParserForScriptLoad", element, TRACE_EVENT_FLAG_FLOW_OUT,
                "data", getTraceArgsForScriptElement(element, scriptStartPosition));
        }
    } else if (waitingForResources) {
        TRACE_EVENT_WITH_FLOW1("blink", "YieldParserForScriptBlockingResources", element, TRACE_EVENT_FLAG_FLOW_OUT,
            "data", getTraceArgsForScriptElement(element, scriptStartPosition));
    }
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

} // namespace

using namespace HTMLNames;

HTMLScriptRunner::HTMLScriptRunner(Document* document, HTMLScriptRunnerHost* host)
    : m_document(document)
    , m_host(host)
    , m_parserBlockingScript(PendingScript::create(nullptr, nullptr))
    , m_scriptNestingLevel(0)
    , m_hasScriptsWaitingForResources(false)
{
    ASSERT(m_host);
    ThreadState::current()->registerPreFinalizer(this);
}

HTMLScriptRunner::~HTMLScriptRunner()
{
    // Verify that detach() has been called.
    ASSERT(!m_document);
}

void HTMLScriptRunner::detach()
{
    if (!m_document)
        return;

    m_parserBlockingScript->stopWatchingForLoad();
    m_parserBlockingScript->releaseElementAndClear();

    while (!m_scriptsToExecuteAfterParsing.isEmpty()) {
        PendingScript* pendingScript = m_scriptsToExecuteAfterParsing.takeFirst();
        pendingScript->stopWatchingForLoad();
        pendingScript->releaseElementAndClear();
    }
    m_document = nullptr;
}

bool HTMLScriptRunner::isPendingScriptReady(const PendingScript* script)
{
    m_hasScriptsWaitingForResources = !m_document->isScriptExecutionReady();
    if (m_hasScriptsWaitingForResources)
        return false;
    return script->isReady();
}

void HTMLScriptRunner::executeParsingBlockingScript()
{
    ASSERT(m_document);
    ASSERT(!isExecutingScript());
    ASSERT(m_document->isScriptExecutionReady());
    ASSERT(isPendingScriptReady(m_parserBlockingScript.get()));

    InsertionPointRecord insertionPointRecord(m_host->inputStream());
    executePendingScriptAndDispatchEvent(m_parserBlockingScript.get(), ScriptStreamer::ParsingBlocking);
}

void HTMLScriptRunner::executePendingScriptAndDispatchEvent(PendingScript* pendingScript, ScriptStreamer::Type pendingScriptType)
{
    bool errorOccurred = false;
    ScriptSourceCode sourceCode = pendingScript->getSource(documentURLForScriptExecution(m_document), errorOccurred);

    // Stop watching loads before executeScript to prevent recursion if the script reloads itself.
    pendingScript->stopWatchingForLoad();

    if (!isExecutingScript()) {
        Microtask::performCheckpoint(V8PerIsolateData::mainThreadIsolate());
        if (pendingScriptType == ScriptStreamer::ParsingBlocking) {
            m_hasScriptsWaitingForResources = !m_document->isScriptExecutionReady();
            // The parser cannot be unblocked as a microtask requested another resource
            if (m_hasScriptsWaitingForResources)
                return;
        }
    }

    TextPosition scriptStartPosition = pendingScript->startingPosition();
    double scriptParserBlockingTime = pendingScript->parserBlockingLoadStartTime();
    // Clear the pending script before possible re-entrancy from executeScript()
    Element* element = pendingScript->releaseElementAndClear();
    if (ScriptLoader* scriptLoader = toScriptLoaderIfPossible(element)) {
        NestingLevelIncrementer nestingLevelIncrementer(m_scriptNestingLevel);
        IgnoreDestructiveWriteCountIncrementer ignoreDestructiveWriteCountIncrementer(m_document);
        if (errorOccurred) {
            TRACE_EVENT_WITH_FLOW1("blink", "HTMLScriptRunner ExecuteScriptFailed", element, TRACE_EVENT_FLAG_FLOW_IN,
                "data", getTraceArgsForScriptElement(element, scriptStartPosition));
            scriptLoader->dispatchErrorEvent();
        } else {
            ASSERT(isExecutingScript());
            if (scriptParserBlockingTime > 0.0) {
                DocumentParserTiming::from(*m_document).recordParserBlockedOnScriptLoadDuration(monotonicallyIncreasingTime() - scriptParserBlockingTime, scriptLoader->wasCreatedDuringDocumentWrite());
            }
            if (!doExecuteScript(element, sourceCode, scriptStartPosition)) {
                scriptLoader->dispatchErrorEvent();
            } else {
                element->dispatchEvent(Event::create(EventTypeNames::load));
            }
        }
    }

    ASSERT(!isExecutingScript());
}

void HTMLScriptRunner::stopWatchingResourceForLoad(Resource* resource)
{
    if (m_parserBlockingScript->resource() == resource) {
        m_parserBlockingScript->stopWatchingForLoad();
        m_parserBlockingScript->releaseElementAndClear();
        return;
    }
    for (auto& script : m_scriptsToExecuteAfterParsing) {
        if (script->resource() == resource) {
            script->stopWatchingForLoad();
            script->releaseElementAndClear();
            return;
        }
    }
}

void HTMLScriptRunner::notifyFinished(Resource* cachedResource)
{
    // Handle cancellations of parser-blocking script loads without
    // notifying the host (i.e., parser) if these were initiated by nested
    // document.write()s. The cancellation may have been triggered by
    // script execution to signal an abrupt stop (e.g., window.close().)
    //
    // The parser is unprepared to be told, and doesn't need to be.
    if (isExecutingScript() && cachedResource->wasCanceled()) {
        stopWatchingResourceForLoad(cachedResource);
        return;
    }
    m_host->notifyScriptLoaded(cachedResource);
}

// Implements the steps for 'An end tag whose tag name is "script"'
// http://whatwg.org/html#scriptEndTag
// Script handling lives outside the tree builder to keep each class simple.
void HTMLScriptRunner::execute(Element* scriptElement, const TextPosition& scriptStartPosition)
{
    ASSERT(scriptElement);
    TRACE_EVENT1("blink", "HTMLScriptRunner::execute",
        "data", getTraceArgsForScriptElement(scriptElement, scriptStartPosition));
    // FIXME: If scripting is disabled, always just return.

    bool hadPreloadScanner = m_host->hasPreloadScanner();

    // Try to execute the script given to us.
    runScript(scriptElement, scriptStartPosition);

    if (hasParserBlockingScript()) {
        if (isExecutingScript())
            return; // Unwind to the outermost HTMLScriptRunner::execute before continuing parsing.

        traceParserBlockingScript(m_parserBlockingScript.get(), !m_document->isScriptExecutionReady());
        m_parserBlockingScript->markParserBlockingLoadStartTime();

        // If preload scanner got created, it is missing the source after the current insertion point. Append it and scan.
        if (!hadPreloadScanner && m_host->hasPreloadScanner())
            m_host->appendCurrentInputStreamToPreloadScannerAndScan();
        executeParsingBlockingScripts();
    }
}

bool HTMLScriptRunner::hasParserBlockingScript() const
{
    return !!m_parserBlockingScript->element();
}

void HTMLScriptRunner::executeParsingBlockingScripts()
{
    while (hasParserBlockingScript() && isPendingScriptReady(m_parserBlockingScript.get()))
        executeParsingBlockingScript();
}

void HTMLScriptRunner::executeScriptsWaitingForLoad(Resource* resource)
{
    TRACE_EVENT0("blink", "HTMLScriptRunner::executeScriptsWaitingForLoad");
    ASSERT(!isExecutingScript());
    ASSERT(hasParserBlockingScript());
    ASSERT_UNUSED(resource, m_parserBlockingScript->resource() == resource);
    ASSERT(m_parserBlockingScript->isReady());
    executeParsingBlockingScripts();
}

void HTMLScriptRunner::executeScriptsWaitingForResources()
{
    TRACE_EVENT0("blink", "HTMLScriptRunner::executeScriptsWaitingForResources");
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
    TRACE_EVENT0("blink", "HTMLScriptRunner::executeScriptsWaitingForParsing");
    while (!m_scriptsToExecuteAfterParsing.isEmpty()) {
        ASSERT(!isExecutingScript());
        ASSERT(!hasParserBlockingScript());
        ASSERT(m_scriptsToExecuteAfterParsing.first()->resource());
        if (!m_scriptsToExecuteAfterParsing.first()->isReady()) {
            m_scriptsToExecuteAfterParsing.first()->watchForLoad(this);
            traceParserBlockingScript(m_scriptsToExecuteAfterParsing.first().get(), !m_document->isScriptExecutionReady());
            m_scriptsToExecuteAfterParsing.first()->markParserBlockingLoadStartTime();
            return false;
        }
        PendingScript* first = m_scriptsToExecuteAfterParsing.takeFirst();
        executePendingScriptAndDispatchEvent(first, ScriptStreamer::Deferred);
        // FIXME: What is this m_document check for?
        if (!m_document)
            return false;
    }
    return true;
}

void HTMLScriptRunner::requestParsingBlockingScript(Element* element)
{
    if (!requestPendingScript(m_parserBlockingScript.get(), element))
        return;

    ASSERT(m_parserBlockingScript->resource());

    // We only care about a load callback if resource is not already
    // in the cache. Callers will attempt to run the m_parserBlockingScript
    // if possible before returning control to the parser.
    if (!m_parserBlockingScript->isReady()) {
        if (m_document->frame()) {
            ScriptState* scriptState = ScriptState::forMainWorld(m_document->frame());
            if (scriptState)
                ScriptStreamer::startStreaming(m_parserBlockingScript.get(), ScriptStreamer::ParsingBlocking, m_document->frame()->settings(), scriptState, m_document->loadingTaskRunner());
        }

        m_parserBlockingScript->watchForLoad(this);
    }
}

void HTMLScriptRunner::requestDeferredScript(Element* element)
{
    PendingScript* pendingScript = PendingScript::create(nullptr, nullptr);
    if (!requestPendingScript(pendingScript, element))
        return;

    if (m_document->frame() && !pendingScript->isReady()) {
        ScriptState* scriptState = ScriptState::forMainWorld(m_document->frame());
        if (scriptState)
            ScriptStreamer::startStreaming(pendingScript, ScriptStreamer::Deferred, m_document->frame()->settings(), scriptState, m_document->loadingTaskRunner());
    }

    ASSERT(pendingScript->resource());
    m_scriptsToExecuteAfterParsing.append(pendingScript);
}

bool HTMLScriptRunner::requestPendingScript(PendingScript* pendingScript, Element* script) const
{
    ASSERT(!pendingScript->element());
    pendingScript->setElement(script);
    // This should correctly return 0 for empty or invalid srcValues.
    ScriptResource* resource = toScriptLoaderIfPossible(script)->resource();
    if (!resource) {
        DVLOG(1) << "Not implemented."; // Dispatch error event.
        return false;
    }
    pendingScript->setScriptResource(resource);
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
            Microtask::performCheckpoint(V8PerIsolateData::mainThreadIsolate());

        InsertionPointRecord insertionPointRecord(m_host->inputStream());
        NestingLevelIncrementer nestingLevelIncrementer(m_scriptNestingLevel);

        scriptLoader->prepareScript(scriptStartPosition);

        if (!scriptLoader->willBeParserExecuted())
            return;

        if (scriptLoader->willExecuteWhenDocumentFinishedParsing()) {
            requestDeferredScript(script);
        } else if (scriptLoader->readyToBeParserExecuted()) {
            if (m_scriptNestingLevel == 1) {
                m_parserBlockingScript->setElement(script);
                m_parserBlockingScript->setStartingPosition(scriptStartPosition);
            } else {
                ASSERT(m_scriptNestingLevel > 1);
                m_parserBlockingScript->releaseElementAndClear();
                ScriptSourceCode sourceCode(CompressibleString(script->textContent().impl()), documentURLForScriptExecution(m_document), scriptStartPosition);
                doExecuteScript(script, sourceCode, scriptStartPosition);
            }
        } else {
            requestParsingBlockingScript(script);
        }
    }
}

DEFINE_TRACE(HTMLScriptRunner)
{
    visitor->trace(m_document);
    visitor->trace(m_host);
    visitor->trace(m_parserBlockingScript);
    visitor->trace(m_scriptsToExecuteAfterParsing);
    ScriptResourceClient::trace(visitor);
}

} // namespace blink
