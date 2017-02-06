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

#include "core/html/parser/HTMLDocumentParser.h"

#include "core/HTMLNames.h"
#include "core/css/MediaValuesCached.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/DocumentFragment.h"
#include "core/dom/Element.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLDocument.h"
#include "core/html/parser/AtomicHTMLToken.h"
#include "core/html/parser/BackgroundHTMLParser.h"
#include "core/html/parser/HTMLParserScheduler.h"
#include "core/html/parser/HTMLParserThread.h"
#include "core/html/parser/HTMLScriptRunner.h"
#include "core/html/parser/HTMLTreeBuilder.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/LinkLoader.h"
#include "core/loader/NavigationScheduler.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/Histogram.h"
#include "platform/SharedBuffer.h"
#include "platform/TraceEvent.h"
#include "platform/heap/Handle.h"
#include "public/platform/Platform.h"
#include "public/platform/WebFrameScheduler.h"
#include "public/platform/WebLoadingBehaviorFlag.h"
#include "public/platform/WebScheduler.h"
#include "public/platform/WebThread.h"
#include "wtf/PtrUtil.h"
#include "wtf/TemporaryChange.h"
#include <memory>

namespace blink {

using namespace HTMLNames;

// This is a direct transcription of step 4 from:
// http://www.whatwg.org/specs/web-apps/current-work/multipage/the-end.html#fragment-case
static HTMLTokenizer::State tokenizerStateForContextElement(Element* contextElement, bool reportErrors, const HTMLParserOptions& options)
{
    if (!contextElement)
        return HTMLTokenizer::DataState;

    const QualifiedName& contextTag = contextElement->tagQName();

    if (contextTag.matches(titleTag) || contextTag.matches(textareaTag))
        return HTMLTokenizer::RCDATAState;
    if (contextTag.matches(styleTag)
        || contextTag.matches(xmpTag)
        || contextTag.matches(iframeTag)
        || (contextTag.matches(noembedTag) && options.pluginsEnabled)
        || (contextTag.matches(noscriptTag) && options.scriptEnabled)
        || contextTag.matches(noframesTag))
        return reportErrors ? HTMLTokenizer::RAWTEXTState : HTMLTokenizer::PLAINTEXTState;
    if (contextTag.matches(scriptTag))
        return reportErrors ? HTMLTokenizer::ScriptDataState : HTMLTokenizer::PLAINTEXTState;
    if (contextTag.matches(plaintextTag))
        return HTMLTokenizer::PLAINTEXTState;
    return HTMLTokenizer::DataState;
}

HTMLDocumentParser::HTMLDocumentParser(HTMLDocument& document, ParserSynchronizationPolicy syncPolicy)
    : ScriptableDocumentParser(document)
    , m_options(&document)
    , m_token(syncPolicy == ForceSynchronousParsing ? wrapUnique(new HTMLToken) : nullptr)
    , m_tokenizer(syncPolicy == ForceSynchronousParsing ? HTMLTokenizer::create(m_options) : nullptr)
    , m_scriptRunner(HTMLScriptRunner::create(&document, this))
    , m_treeBuilder(HTMLTreeBuilder::create(this, &document, getParserContentPolicy(), m_options))
    , m_loadingTaskRunner(wrapUnique(document.loadingTaskRunner()->clone()))
    , m_parserScheduler(HTMLParserScheduler::create(this, m_loadingTaskRunner.get()))
    , m_xssAuditorDelegate(&document)
    , m_weakFactory(this)
    , m_preloader(HTMLResourcePreloader::create(document))
    , m_parsedChunkQueue(ParsedChunkQueue::create())
    , m_evaluator(DocumentWriteEvaluator::create(document))
    , m_shouldUseThreading(syncPolicy == AllowAsynchronousParsing)
    , m_endWasDelayed(false)
    , m_haveBackgroundParser(false)
    , m_tasksWereSuspended(false)
    , m_pumpSessionNestingLevel(0)
    , m_pumpSpeculationsSessionNestingLevel(0)
    , m_isParsingAtLineNumber(false)
    , m_triedLoadingLinkHeaders(false)
{
    ASSERT(shouldUseThreading() || (m_token && m_tokenizer));
}

// FIXME: Member variables should be grouped into self-initializing structs to
// minimize code duplication between these constructors.
HTMLDocumentParser::HTMLDocumentParser(DocumentFragment* fragment, Element* contextElement, ParserContentPolicy parserContentPolicy)
    : ScriptableDocumentParser(fragment->document(), parserContentPolicy)
    , m_options(&fragment->document())
    , m_token(wrapUnique(new HTMLToken))
    , m_tokenizer(HTMLTokenizer::create(m_options))
    , m_treeBuilder(HTMLTreeBuilder::create(this, fragment, contextElement, this->getParserContentPolicy(), m_options))
    , m_loadingTaskRunner(wrapUnique(fragment->document().loadingTaskRunner()->clone()))
    , m_xssAuditorDelegate(&fragment->document())
    , m_weakFactory(this)
    , m_shouldUseThreading(false)
    , m_endWasDelayed(false)
    , m_haveBackgroundParser(false)
    , m_tasksWereSuspended(false)
    , m_pumpSessionNestingLevel(0)
    , m_pumpSpeculationsSessionNestingLevel(0)
{
    bool reportErrors = false; // For now document fragment parsing never reports errors.
    m_tokenizer->setState(tokenizerStateForContextElement(contextElement, reportErrors, m_options));
    m_xssAuditor.initForFragment();
}

HTMLDocumentParser::~HTMLDocumentParser()
{
    // In Oilpan, HTMLDocumentParser can die together with Document, and
    // detach() is not called in this case.
    if (m_haveBackgroundParser)
        stopBackgroundParser();
}

DEFINE_TRACE(HTMLDocumentParser)
{
    visitor->trace(m_treeBuilder);
    visitor->trace(m_parserScheduler);
    visitor->trace(m_xssAuditorDelegate);
    visitor->trace(m_scriptRunner);
    visitor->trace(m_preloader);
    ScriptableDocumentParser::trace(visitor);
    HTMLScriptRunnerHost::trace(visitor);
}

void HTMLDocumentParser::detach()
{
    if (!isParsingFragment() && m_parsedChunkQueue.get() && m_parsedChunkQueue->peakPendingChunkCount()) {
        DEFINE_STATIC_LOCAL(CustomCountHistogram, peakPendingChunkHistogram, ("Parser.PeakPendingChunkCount", 1, 1000, 50));
        peakPendingChunkHistogram.count(m_parsedChunkQueue->peakPendingChunkCount());
        DEFINE_STATIC_LOCAL(CustomCountHistogram, peakPendingTokenHistogram, ("Parser.PeakPendingTokenCount", 1, 100000, 50));
        peakPendingTokenHistogram.count(m_parsedChunkQueue->peakPendingTokenCount());
    }

    if (m_haveBackgroundParser)
        stopBackgroundParser();
    DocumentParser::detach();
    if (m_scriptRunner)
        m_scriptRunner->detach();
    m_treeBuilder->detach();
    // FIXME: It seems wrong that we would have a preload scanner here.
    // Yet during fast/dom/HTMLScriptElement/script-load-events.html we do.
    m_preloadScanner.reset();
    m_insertionPreloadScanner.reset();
    if (m_parserScheduler) {
        m_parserScheduler->detach();
        m_parserScheduler.clear();
    }
    // Oilpan: It is important to clear m_token to deallocate backing memory of
    // HTMLToken::m_data and let the allocator reuse the memory for
    // HTMLToken::m_data of a next HTMLDocumentParser. We need to clear
    // m_tokenizer first because m_tokenizer has a raw pointer to m_token.
    m_tokenizer.reset();
    m_token.reset();
}

void HTMLDocumentParser::stopParsing()
{
    DocumentParser::stopParsing();
    if (m_parserScheduler) {
        m_parserScheduler->detach();
        m_parserScheduler.clear();
    }
    if (m_haveBackgroundParser)
        stopBackgroundParser();
}

// This kicks off "Once the user agent stops parsing" as described by:
// http://www.whatwg.org/specs/web-apps/current-work/multipage/the-end.html#the-end
void HTMLDocumentParser::prepareToStopParsing()
{
    // FIXME: It may not be correct to disable this for the background parser.
    // That means hasInsertionPoint() may not be correct in some cases.
    ASSERT(!hasInsertionPoint() || m_haveBackgroundParser);

    // NOTE: This pump should only ever emit buffered character tokens.
    if (m_tokenizer) {
        ASSERT(!m_haveBackgroundParser);
        pumpTokenizerIfPossible();
    }

    if (isStopped())
        return;

    DocumentParser::prepareToStopParsing();

    // We will not have a scriptRunner when parsing a DocumentFragment.
    if (m_scriptRunner)
        document()->setReadyState(Document::Interactive);

    // Setting the ready state above can fire mutation event and detach us
    // from underneath. In that case, just bail out.
    if (isDetached())
        return;

    attemptToRunDeferredScriptsAndEnd();
}

bool HTMLDocumentParser::isParsingFragment() const
{
    return m_treeBuilder->isParsingFragment();
}

void HTMLDocumentParser::pumpTokenizerIfPossible()
{
    if (isStopped() || isWaitingForScripts())
        return;

    pumpTokenizer();
}

bool HTMLDocumentParser::isScheduledForResume() const
{
    return m_parserScheduler && m_parserScheduler->isScheduledForResume();
}

// Used by HTMLParserScheduler
void HTMLDocumentParser::resumeParsingAfterYield()
{
    ASSERT(shouldUseThreading());
    ASSERT(m_haveBackgroundParser);

    pumpPendingSpeculations();
}

void HTMLDocumentParser::runScriptsForPausedTreeBuilder()
{
    ASSERT(scriptingContentIsAllowed(getParserContentPolicy()));

    TextPosition scriptStartPosition = TextPosition::belowRangePosition();
    Element* scriptElement = m_treeBuilder->takeScriptToProcess(scriptStartPosition);
    // We will not have a scriptRunner when parsing a DocumentFragment.
    if (m_scriptRunner)
        m_scriptRunner->execute(scriptElement, scriptStartPosition);
}

bool HTMLDocumentParser::canTakeNextToken()
{
    if (isStopped())
        return false;

    // If we're paused waiting for a script, we try to execute scripts before continuing.
    if (m_treeBuilder->hasParserBlockingScript())
        runScriptsForPausedTreeBuilder();
    if (isStopped() || isWaitingForScripts())
        return false;

    // FIXME: It's wrong for the HTMLDocumentParser to reach back to the
    //        LocalFrame, but this approach is how the old parser handled
    //        stopping when the page assigns window.location.  What really
    //        should happen is that assigning window.location causes the
    //        parser to stop parsing cleanly.  The problem is we're not
    //        perpared to do that at every point where we run JavaScript.
    if (!isParsingFragment()
        && document()->frame() && document()->frame()->navigationScheduler().locationChangePending())
        return false;

    return true;
}

void HTMLDocumentParser::notifyPendingParsedChunks()
{
    TRACE_EVENT0("blink", "HTMLDocumentParser::notifyPendingParsedChunks");
    ASSERT(m_parsedChunkQueue);

    Vector<std::unique_ptr<ParsedChunk>> pendingChunks;
    m_parsedChunkQueue->takeAll(pendingChunks);

    if (!isParsing())
        return;

    // ApplicationCache needs to be initialized before issuing preloads.
    // We suspend preload until HTMLHTMLElement is inserted and
    // ApplicationCache is initialized.
    if (!document()->documentElement()) {
        for (auto& chunk : pendingChunks) {
            for (auto& request : chunk->preloads)
                m_queuedPreloads.append(std::move(request));
            for (auto& index : chunk->likelyDocumentWriteScriptIndices) {
                const CompactHTMLToken& token = chunk->tokens->at(index);
                ASSERT(token.type() == HTMLToken::TokenType::Character);
                m_queuedDocumentWriteScripts.append(token.data());
            }
        }
    } else {
        // We can safely assume that there are no queued preloads request after
        // the document element is available, as we empty the queue immediately
        // after the document element is created in documentElementAvailable().
        ASSERT(m_queuedPreloads.isEmpty());
        ASSERT(m_queuedDocumentWriteScripts.isEmpty());
        // Loop through the chunks to generate preloads before any
        // document.write script evaluation takes place. Preloading these
        // scripts is valuable and comparably cheap, while evaluating JS can be
        // expensive.
        for (auto& chunk : pendingChunks) {
            m_preloader->takeAndPreload(chunk->preloads);
        }
        for (auto& chunk : pendingChunks) {
            for (auto& index : chunk->likelyDocumentWriteScriptIndices) {
                const CompactHTMLToken& token = chunk->tokens->at(index);
                ASSERT(token.type() == HTMLToken::TokenType::Character);
                evaluateAndPreloadScriptForDocumentWrite(token.data());
            }
        }
    }

    for (auto& chunk : pendingChunks)
        m_speculations.append(std::move(chunk));

    if (!isWaitingForScripts() && !isScheduledForResume()) {
        if (m_tasksWereSuspended)
            m_parserScheduler->forceResumeAfterYield();
        else
            m_parserScheduler->scheduleForResume();
    }
}

void HTMLDocumentParser::didReceiveEncodingDataFromBackgroundParser(const DocumentEncodingData& data)
{
    document()->setEncodingData(data);
}

void HTMLDocumentParser::validateSpeculations(std::unique_ptr<ParsedChunk> chunk)
{
    ASSERT(chunk);
    if (isWaitingForScripts()) {
        // We're waiting on a network script, just save the chunk, we'll get
        // a second validateSpeculations call after the script completes.
        // This call should have been made immediately after runScriptsForPausedTreeBuilder
        // which may have started a network load and left us waiting.
        ASSERT(!m_lastChunkBeforeScript);
        m_lastChunkBeforeScript = std::move(chunk);
        return;
    }

    ASSERT(!m_lastChunkBeforeScript);
    std::unique_ptr<HTMLTokenizer> tokenizer = std::move(m_tokenizer);
    std::unique_ptr<HTMLToken> token = std::move(m_token);

    if (!tokenizer) {
        // There must not have been any changes to the HTMLTokenizer state on
        // the main thread, which means the speculation buffer is correct.
        return;
    }

    // Currently we're only smart enough to reuse the speculation buffer if the tokenizer
    // both starts and ends in the DataState. That state is simplest because the HTMLToken
    // is always in the Uninitialized state. We should consider whether we can reuse the
    // speculation buffer in other states, but we'd likely need to do something more
    // sophisticated with the HTMLToken.
    if (chunk->tokenizerState == HTMLTokenizer::DataState
        && tokenizer->getState() == HTMLTokenizer::DataState
        && m_input.current().isEmpty()
        && chunk->treeBuilderState == HTMLTreeBuilderSimulator::stateFor(m_treeBuilder.get())) {
        ASSERT(token->isUninitialized());
        return;
    }

    discardSpeculationsAndResumeFrom(std::move(chunk), std::move(token), std::move(tokenizer));
}

void HTMLDocumentParser::discardSpeculationsAndResumeFrom(std::unique_ptr<ParsedChunk> lastChunkBeforeScript, std::unique_ptr<HTMLToken> token, std::unique_ptr<HTMLTokenizer> tokenizer)
{
    m_weakFactory.revokeAll();

    size_t discardedTokenCount = 0;
    for (const auto& speculation : m_speculations) {
        discardedTokenCount += speculation->tokens->size();
    }
    DEFINE_STATIC_LOCAL(CustomCountHistogram, discardedTokenCountHistogram, ("Parser.DiscardedTokenCount", 1, 100000, 50));
    discardedTokenCountHistogram.count(discardedTokenCount);

    m_speculations.clear();

    std::unique_ptr<BackgroundHTMLParser::Checkpoint> checkpoint = wrapUnique(new BackgroundHTMLParser::Checkpoint);
    checkpoint->parser = m_weakFactory.createWeakPtr();
    checkpoint->token = std::move(token);
    checkpoint->tokenizer = std::move(tokenizer);
    checkpoint->treeBuilderState = HTMLTreeBuilderSimulator::stateFor(m_treeBuilder.get());
    checkpoint->inputCheckpoint = lastChunkBeforeScript->inputCheckpoint;
    checkpoint->preloadScannerCheckpoint = lastChunkBeforeScript->preloadScannerCheckpoint;
    checkpoint->unparsedInput = m_input.current().toString().isolatedCopy();
    m_input.current().clear(); // FIXME: This should be passed in instead of cleared.

    ASSERT(checkpoint->unparsedInput.isSafeToSendToAnotherThread());
    postTaskToLookaheadParser(Asynchronous, &BackgroundHTMLParser::resumeFrom, m_backgroundParser, passed(std::move(checkpoint)));
}

size_t HTMLDocumentParser::processParsedChunkFromBackgroundParser(std::unique_ptr<ParsedChunk> popChunk)
{
    TRACE_EVENT_WITH_FLOW0("blink,loading", "HTMLDocumentParser::processParsedChunkFromBackgroundParser", popChunk.get(), TRACE_EVENT_FLAG_FLOW_IN);
    TemporaryChange<bool> hasLineNumber(m_isParsingAtLineNumber, true);

    ASSERT_WITH_SECURITY_IMPLICATION(m_pumpSpeculationsSessionNestingLevel == 1);
    ASSERT_WITH_SECURITY_IMPLICATION(!inPumpSession());
    ASSERT(!isParsingFragment());
    ASSERT(!isWaitingForScripts());
    ASSERT(!isStopped());
    ASSERT(shouldUseThreading());
    ASSERT(!m_tokenizer);
    ASSERT(!m_token);
    ASSERT(!m_lastChunkBeforeScript);

    std::unique_ptr<ParsedChunk> chunk(std::move(popChunk));
    std::unique_ptr<CompactHTMLTokenStream> tokens = std::move(chunk->tokens);
    size_t elementTokenCount = 0;

    postTaskToLookaheadParser(Asynchronous, &BackgroundHTMLParser::startedChunkWithCheckpoint, m_backgroundParser, chunk->inputCheckpoint);

    for (const auto& xssInfo : chunk->xssInfos) {
        m_textPosition = xssInfo->m_textPosition;
        m_xssAuditorDelegate.didBlockScript(*xssInfo);
        if (isStopped())
            break;
    }
    // XSSAuditorDelegate can detach the parser if it decides to block the entire current document.
    if (isDetached())
        return elementTokenCount;

    for (Vector<CompactHTMLToken>::const_iterator it = tokens->begin(); it != tokens->end(); ++it) {
        ASSERT(!isWaitingForScripts());

        if (!chunk->startingScript && (it->type() == HTMLToken::StartTag || it->type() == HTMLToken::EndTag))
            elementTokenCount++;

        if (document()->frame() && document()->frame()->navigationScheduler().locationChangePending()) {

            // To match main-thread parser behavior (which never checks locationChangePending on the EOF path)
            // we peek to see if this chunk has an EOF and process it anyway.
            if (tokens->last().type() == HTMLToken::EndOfFile) {
                ASSERT(m_speculations.isEmpty()); // There should never be any chunks after the EOF.
                prepareToStopParsing();
            }
            break;
        }

        m_textPosition = it->textPosition();

        constructTreeFromCompactHTMLToken(*it);

        if (isStopped())
            break;

        if (!m_triedLoadingLinkHeaders && document()->loader()) {
            String linkHeader = document()->loader()->response().httpHeaderField(HTTPNames::Link);
            if (!linkHeader.isEmpty()) {
                ASSERT(chunk);
                LinkLoader::loadLinksFromHeader(linkHeader, document()->loader()->response().url(),
                    document(), NetworkHintsInterfaceImpl(), LinkLoader::OnlyLoadResources, &(chunk->viewport));
                m_triedLoadingLinkHeaders = true;
            }
        }

        if (isWaitingForScripts()) {
            ASSERT(it + 1 == tokens->end()); // The </script> is assumed to be the last token of this bunch.
            runScriptsForPausedTreeBuilder();
            validateSpeculations(std::move(chunk));
            break;
        }

        if (it->type() == HTMLToken::EndOfFile) {
            ASSERT(it + 1 == tokens->end()); // The EOF is assumed to be the last token of this bunch.
            ASSERT(m_speculations.isEmpty()); // There should never be any chunks after the EOF.
            prepareToStopParsing();
            break;
        }

        ASSERT(!m_tokenizer);
        ASSERT(!m_token);
    }

    // Make sure all required pending text nodes are emitted before returning.
    // This leaves "script", "style" and "svg" nodes text nodes intact.
    if (!isStopped())
        m_treeBuilder->flush(FlushIfAtTextLimit);

    m_isParsingAtLineNumber = false;

    return elementTokenCount;
}

void HTMLDocumentParser::pumpPendingSpeculations()
{
    // If this assert fails, you need to call validateSpeculations to make sure
    // m_tokenizer and m_token don't have state that invalidates m_speculations.
    ASSERT(!m_tokenizer);
    ASSERT(!m_token);
    ASSERT(!m_lastChunkBeforeScript);
    ASSERT(!isWaitingForScripts());
    ASSERT(!isStopped());
    ASSERT(!isScheduledForResume());
    ASSERT(!inPumpSession());

    // FIXME: Here should never be reached when there is a blocking script,
    // but it happens in unknown scenarios. See https://crbug.com/440901
    if (isWaitingForScripts()) {
        m_parserScheduler->scheduleForResume();
        return;
    }

    // Do not allow pumping speculations in nested event loops.
    if (m_pumpSpeculationsSessionNestingLevel) {
        m_parserScheduler->scheduleForResume();
        return;
    }

    // FIXME: Pass in current input length.
    TRACE_EVENT_BEGIN1("devtools.timeline", "ParseHTML", "beginData", InspectorParseHtmlEvent::beginData(document(), lineNumber().zeroBasedInt()));

    SpeculationsPumpSession session(m_pumpSpeculationsSessionNestingLevel);
    while (!m_speculations.isEmpty()) {
        ASSERT(!isScheduledForResume());
        size_t elementTokenCount = processParsedChunkFromBackgroundParser(m_speculations.takeFirst());
        session.addedElementTokens(elementTokenCount);

        // Always check isParsing first as m_document may be null.
        // Surprisingly, isScheduledForResume() may be set here as a result of
        // processParsedChunkFromBackgroundParser running arbitrary javascript
        // which invokes nested event loops. (e.g. inspector breakpoints)
        if (!isParsing() || isWaitingForScripts() || isScheduledForResume())
            break;

        if (m_speculations.isEmpty() || m_parserScheduler->yieldIfNeeded(session, m_speculations.first()->startingScript))
            break;
    }

    TRACE_EVENT_END1("devtools.timeline", "ParseHTML", "endData", InspectorParseHtmlEvent::endData(lineNumber().zeroBasedInt() - 1));
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "UpdateCounters", TRACE_EVENT_SCOPE_THREAD, "data", InspectorUpdateCountersEvent::data());
}

void HTMLDocumentParser::forcePlaintextForTextDocument()
{
    if (shouldUseThreading()) {
        // This method is called before any data is appended, so we have to start
        // the background parser ourselves.
        if (!m_haveBackgroundParser)
            startBackgroundParser();

        postTaskToLookaheadParser(Asynchronous, &BackgroundHTMLParser::forcePlaintextForTextDocument, m_backgroundParser);
    } else
        m_tokenizer->setState(HTMLTokenizer::PLAINTEXTState);
}

void HTMLDocumentParser::pumpTokenizer()
{
    ASSERT(!isStopped());
    ASSERT(m_tokenizer);
    ASSERT(m_token);

    PumpSession session(m_pumpSessionNestingLevel);

    // We tell the InspectorInstrumentation about every pump, even if we
    // end up pumping nothing.  It can filter out empty pumps itself.
    // FIXME: m_input.current().length() is only accurate if we
    // end up parsing the whole buffer in this pump.  We should pass how
    // much we parsed as part of didWriteHTML instead of willWriteHTML.
    TRACE_EVENT_BEGIN1("devtools.timeline", "ParseHTML", "beginData", InspectorParseHtmlEvent::beginData(document(), m_input.current().currentLine().zeroBasedInt()));

    if (!isParsingFragment())
        m_xssAuditor.init(document(), &m_xssAuditorDelegate);

    while (canTakeNextToken()) {
        if (m_xssAuditor.isEnabled())
            m_sourceTracker.start(m_input.current(), m_tokenizer.get(), token());

        if (!m_tokenizer->nextToken(m_input.current(), token()))
            break;

        if (m_xssAuditor.isEnabled()) {
            m_sourceTracker.end(m_input.current(), m_tokenizer.get(), token());

            // We do not XSS filter innerHTML, which means we (intentionally) fail
            // http/tests/security/xssAuditor/dom-write-innerHTML.html
            if (std::unique_ptr<XSSInfo> xssInfo = m_xssAuditor.filterToken(FilterTokenRequest(token(), m_sourceTracker, m_tokenizer->shouldAllowCDATA())))
                m_xssAuditorDelegate.didBlockScript(*xssInfo);
        }

        constructTreeFromHTMLToken();
        ASSERT(isStopped() || token().isUninitialized());
    }

    if (isStopped())
        return;

    // There should only be PendingText left since the tree-builder always flushes
    // the task queue before returning. In case that ever changes, crash.
    m_treeBuilder->flush(FlushAlways);
    RELEASE_ASSERT(!isStopped());

    if (isWaitingForScripts()) {
        ASSERT(m_tokenizer->getState() == HTMLTokenizer::DataState);

        ASSERT(m_preloader);
        // TODO(kouhei): m_preloader should be always available for synchronous parsing case,
        // adding paranoia if for speculative crash fix for crbug.com/465478
        if (m_preloader) {
            if (!m_preloadScanner) {
                m_preloadScanner = createPreloadScanner();
                m_preloadScanner->appendToEnd(m_input.current());
            }
            m_preloadScanner->scanAndPreload(m_preloader.get(), document()->validBaseElementURL(), nullptr);
        }
    }

    TRACE_EVENT_END1("devtools.timeline", "ParseHTML", "endData", InspectorParseHtmlEvent::endData(m_input.current().currentLine().zeroBasedInt() - 1));
}

void HTMLDocumentParser::constructTreeFromHTMLToken()
{
    AtomicHTMLToken atomicToken(token());

    // We clear the m_token in case constructTreeFromAtomicToken
    // synchronously re-enters the parser. We don't clear the token immedately
    // for Character tokens because the AtomicHTMLToken avoids copying the
    // characters by keeping a pointer to the underlying buffer in the
    // HTMLToken. Fortunately, Character tokens can't cause us to re-enter
    // the parser.
    //
    // FIXME: Stop clearing the m_token once we start running the parser off
    // the main thread or once we stop allowing synchronous JavaScript
    // execution from parseAttribute.
    if (token().type() != HTMLToken::Character)
        token().clear();

    m_treeBuilder->constructTree(&atomicToken);

    // FIXME: constructTree may synchronously cause Document to be detached.
    if (!m_token)
        return;

    if (!token().isUninitialized()) {
        ASSERT(token().type() == HTMLToken::Character);
        token().clear();
    }
}

void HTMLDocumentParser::constructTreeFromCompactHTMLToken(const CompactHTMLToken& compactToken)
{
    AtomicHTMLToken token(compactToken);
    m_treeBuilder->constructTree(&token);
}

bool HTMLDocumentParser::hasInsertionPoint()
{
    // FIXME: The wasCreatedByScript() branch here might not be fully correct.
    //        Our model of the EOF character differs slightly from the one in
    //        the spec because our treatment is uniform between network-sourced
    //        and script-sourced input streams whereas the spec treats them
    //        differently.
    return m_input.hasInsertionPoint() || (wasCreatedByScript() && !m_input.haveSeenEndOfFile());
}

void HTMLDocumentParser::insert(const SegmentedString& source)
{
    if (isStopped())
        return;

    TRACE_EVENT1("blink", "HTMLDocumentParser::insert", "source_length", source.length());

    if (!m_tokenizer) {
        ASSERT(!inPumpSession());
        ASSERT(m_haveBackgroundParser || wasCreatedByScript());
        m_token = wrapUnique(new HTMLToken);
        m_tokenizer = HTMLTokenizer::create(m_options);
    }

    SegmentedString excludedLineNumberSource(source);
    excludedLineNumberSource.setExcludeLineNumbers();
    m_input.insertAtCurrentInsertionPoint(excludedLineNumberSource);
    pumpTokenizerIfPossible();

    if (isWaitingForScripts()) {
        // Check the document.write() output with a separate preload scanner as
        // the main scanner can't deal with insertions.
        if (!m_insertionPreloadScanner)
            m_insertionPreloadScanner = createPreloadScanner();
        m_insertionPreloadScanner->appendToEnd(source);
        m_insertionPreloadScanner->scanAndPreload(m_preloader.get(), document()->validBaseElementURL(), nullptr);
    }

    endIfDelayed();
}

void HTMLDocumentParser::startBackgroundParser()
{
    ASSERT(!isStopped());
    ASSERT(shouldUseThreading());
    ASSERT(!m_haveBackgroundParser);
    ASSERT(document());
    m_haveBackgroundParser = true;

    // Make sure that a resolver is set up, so that the correct viewport dimensions will be fed to the background parser and preload scanner.
    if (document()->loader())
        document()->ensureStyleResolver();

    RefPtr<WeakReference<BackgroundHTMLParser>> reference = WeakReference<BackgroundHTMLParser>::createUnbound();
    m_backgroundParser = WeakPtr<BackgroundHTMLParser>(reference);

    std::unique_ptr<BackgroundHTMLParser::Configuration> config = wrapUnique(new BackgroundHTMLParser::Configuration);
    config->options = m_options;
    config->parser = m_weakFactory.createWeakPtr();
    config->xssAuditor = wrapUnique(new XSSAuditor);
    config->xssAuditor->init(document(), &m_xssAuditorDelegate);

    config->decoder = takeDecoder();
    config->parsedChunkQueue = m_parsedChunkQueue.get();
    if (document()->settings()) {
        if (document()->settings()->backgroundHtmlParserOutstandingTokenLimit())
            config->outstandingTokenLimit = document()->settings()->backgroundHtmlParserOutstandingTokenLimit();
        if (document()->settings()->backgroundHtmlParserPendingTokenLimit())
            config->pendingTokenLimit = document()->settings()->backgroundHtmlParserPendingTokenLimit();
    }

    ASSERT(config->xssAuditor->isSafeToSendToAnotherThread());
    postTaskToLookaheadParser(
        Synchronous,
        &BackgroundHTMLParser::start,
        reference.release(),
        passed(std::move(config)),
        document()->url(),
        passed(CachedDocumentParameters::create(document())),
        MediaValuesCached::MediaValuesCachedData(*document()),
        passed(wrapUnique(m_loadingTaskRunner->clone())));
}

void HTMLDocumentParser::stopBackgroundParser()
{
    ASSERT(shouldUseThreading());
    ASSERT(m_haveBackgroundParser);
    m_haveBackgroundParser = false;

    // Make this sync, as lsan triggers on some unittests if the task runner is
    // used. Note that these lifetimes will be much more concrete if
    // ParseHTMLOnMainThread lands (the lookahead parser will be a member).
    postTaskToLookaheadParser(Synchronous, &BackgroundHTMLParser::stop, m_backgroundParser);
    m_weakFactory.revokeAll();
}

void HTMLDocumentParser::append(const String& inputSource)
{
    if (isStopped())
        return;

    // We should never reach this point if we're using a parser thread,
    // as appendBytes() will directly ship the data to the thread.
    ASSERT(!shouldUseThreading());

    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("blink.debug"), "HTMLDocumentParser::append", "size", inputSource.length());
    const SegmentedString source(inputSource);

    if (m_preloadScanner) {
        if (m_input.current().isEmpty() && !isWaitingForScripts()) {
            // We have parsed until the end of the current input and so are now moving ahead of the preload scanner.
            // Clear the scanner so we know to scan starting from the current input point if we block again.
            m_preloadScanner.reset();
        } else {
            m_preloadScanner->appendToEnd(source);
            if (isWaitingForScripts())
                m_preloadScanner->scanAndPreload(m_preloader.get(), document()->validBaseElementURL(), nullptr);
        }
    }

    m_input.appendToEnd(source);

    if (inPumpSession()) {
        // We've gotten data off the network in a nested write.
        // We don't want to consume any more of the input stream now.  Do
        // not worry.  We'll consume this data in a less-nested write().
        return;
    }

    pumpTokenizerIfPossible();

    endIfDelayed();
}

void HTMLDocumentParser::end()
{
    ASSERT(!isDetached());
    ASSERT(!isScheduledForResume());

    if (m_haveBackgroundParser)
        stopBackgroundParser();

    // Informs the the rest of WebCore that parsing is really finished (and deletes this).
    m_treeBuilder->finished();

    DocumentParser::stopParsing();
}

void HTMLDocumentParser::attemptToRunDeferredScriptsAndEnd()
{
    ASSERT(isStopping());
    // FIXME: It may not be correct to disable this for the background parser.
    // That means hasInsertionPoint() may not be correct in some cases.
    ASSERT(!hasInsertionPoint() || m_haveBackgroundParser);
    if (m_scriptRunner && !m_scriptRunner->executeScriptsWaitingForParsing())
        return;
    end();
}

void HTMLDocumentParser::attemptToEnd()
{
    // finish() indicates we will not receive any more data. If we are waiting on
    // an external script to load, we can't finish parsing quite yet.

    if (shouldDelayEnd()) {
        m_endWasDelayed = true;
        return;
    }
    prepareToStopParsing();
}

void HTMLDocumentParser::endIfDelayed()
{
    // If we've already been detached, don't bother ending.
    if (isDetached())
        return;

    if (!m_endWasDelayed || shouldDelayEnd())
        return;

    m_endWasDelayed = false;
    prepareToStopParsing();
}

void HTMLDocumentParser::finish()
{
    // FIXME: We should ASSERT(!m_parserStopped) here, since it does not
    // makes sense to call any methods on DocumentParser once it's been stopped.
    // However, FrameLoader::stop calls DocumentParser::finish unconditionally.

    flush();
    if (isDetached())
        return;

    // Empty documents never got an append() call, and thus have never started
    // a background parser. In those cases, we ignore shouldUseThreading()
    // and fall through to the non-threading case.
    if (m_haveBackgroundParser) {
        if (!m_input.haveSeenEndOfFile())
            m_input.closeWithoutMarkingEndOfFile();
        postTaskToLookaheadParser(Asynchronous, &BackgroundHTMLParser::finish, m_backgroundParser);
        return;
    }

    if (!m_tokenizer) {
        ASSERT(!m_token);
        // We're finishing before receiving any data. Rather than booting up
        // the background parser just to spin it down, we finish parsing
        // synchronously.
        m_token = wrapUnique(new HTMLToken);
        m_tokenizer = HTMLTokenizer::create(m_options);
    }

    // We're not going to get any more data off the network, so we tell the
    // input stream we've reached the end of file. finish() can be called more
    // than once, if the first time does not call end().
    if (!m_input.haveSeenEndOfFile())
        m_input.markEndOfFile();

    attemptToEnd();
}

bool HTMLDocumentParser::isExecutingScript() const
{
    if (!m_scriptRunner)
        return false;
    return m_scriptRunner->isExecutingScript();
}

bool HTMLDocumentParser::isParsingAtLineNumber() const
{
    return m_isParsingAtLineNumber && ScriptableDocumentParser::isParsingAtLineNumber();
}

OrdinalNumber HTMLDocumentParser::lineNumber() const
{
    if (m_haveBackgroundParser)
        return m_textPosition.m_line;

    return m_input.current().currentLine();
}

TextPosition HTMLDocumentParser::textPosition() const
{
    if (m_haveBackgroundParser)
        return m_textPosition;

    const SegmentedString& currentString = m_input.current();
    OrdinalNumber line = currentString.currentLine();
    OrdinalNumber column = currentString.currentColumn();

    return TextPosition(line, column);
}

bool HTMLDocumentParser::isWaitingForScripts() const
{
    // When the TreeBuilder encounters a </script> tag, it returns to the HTMLDocumentParser
    // where the script is transfered from the treebuilder to the script runner.
    // The script runner will hold the script until its loaded and run. During
    // any of this time, we want to count ourselves as "waiting for a script" and thus
    // run the preload scanner, as well as delay completion of parsing.
    bool treeBuilderHasBlockingScript = m_treeBuilder->hasParserBlockingScript();
    bool scriptRunnerHasBlockingScript = m_scriptRunner && m_scriptRunner->hasParserBlockingScript();
    // Since the parser is paused while a script runner has a blocking script, it should
    // never be possible to end up with both objects holding a blocking script.
    ASSERT(!(treeBuilderHasBlockingScript && scriptRunnerHasBlockingScript));
    // If either object has a blocking script, the parser should be paused.
    return treeBuilderHasBlockingScript || scriptRunnerHasBlockingScript;
}

void HTMLDocumentParser::resumeParsingAfterScriptExecution()
{
    ASSERT(!isExecutingScript());
    ASSERT(!isWaitingForScripts());

    if (m_haveBackgroundParser) {
        validateSpeculations(std::move(m_lastChunkBeforeScript));
        ASSERT(!m_lastChunkBeforeScript);
        pumpPendingSpeculations();
        return;
    }

    m_insertionPreloadScanner.reset();
    pumpTokenizerIfPossible();
    endIfDelayed();
}

void HTMLDocumentParser::appendCurrentInputStreamToPreloadScannerAndScan()
{
    ASSERT(m_preloadScanner);
    m_preloadScanner->appendToEnd(m_input.current());
    m_preloadScanner->scanAndPreload(m_preloader.get(), document()->validBaseElementURL(), nullptr);
}

void HTMLDocumentParser::notifyScriptLoaded(Resource* cachedResource)
{
    ASSERT(m_scriptRunner);
    ASSERT(!isExecutingScript());

    if (isStopped()) {
        return;
    }

    if (isStopping()) {
        attemptToRunDeferredScriptsAndEnd();
        return;
    }

    m_scriptRunner->executeScriptsWaitingForLoad(cachedResource);
    if (!isWaitingForScripts())
        resumeParsingAfterScriptExecution();
}

void HTMLDocumentParser::executeScriptsWaitingForResources()
{
    // Document only calls this when the Document owns the DocumentParser
    // so this will not be called in the DocumentFragment case.
    ASSERT(m_scriptRunner);
    // Ignore calls unless we have a script blocking the parser waiting on a
    // stylesheet load.  Otherwise we are currently parsing and this
    // is a re-entrant call from encountering a </ style> tag.
    if (!m_scriptRunner->hasScriptsWaitingForResources())
        return;
    m_scriptRunner->executeScriptsWaitingForResources();
    if (!isWaitingForScripts())
        resumeParsingAfterScriptExecution();
}

void HTMLDocumentParser::parseDocumentFragment(const String& source, DocumentFragment* fragment, Element* contextElement, ParserContentPolicy parserContentPolicy)
{
    HTMLDocumentParser* parser = HTMLDocumentParser::create(fragment, contextElement, parserContentPolicy);
    parser->append(source);
    parser->finish();
    parser->detach(); // Allows ~DocumentParser to assert it was detached before destruction.
}

void HTMLDocumentParser::suspendScheduledTasks()
{
    ASSERT(!m_tasksWereSuspended);
    m_tasksWereSuspended = true;
    if (m_parserScheduler)
        m_parserScheduler->suspend();
}

void HTMLDocumentParser::resumeScheduledTasks()
{
    ASSERT(m_tasksWereSuspended);
    m_tasksWereSuspended = false;
    if (m_parserScheduler)
        m_parserScheduler->resume();
}

void HTMLDocumentParser::appendBytes(const char* data, size_t length)
{
    if (!length || isStopped())
        return;

    if (shouldUseThreading()) {
        double bytesReceivedTime = monotonicallyIncreasingTimeMS();
        if (!m_haveBackgroundParser)
            startBackgroundParser();

        std::unique_ptr<Vector<char>> buffer = wrapUnique(new Vector<char>(length));
        memcpy(buffer->data(), data, length);
        TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("blink.debug"), "HTMLDocumentParser::appendBytes", "size", (unsigned)length);

        LookaheadParserTaskSynchrony policy = document()->settings() && document()->settings()->parseHTMLOnMainThreadSyncTokenize() ? Synchronous : Asynchronous;
        postTaskToLookaheadParser(policy, &BackgroundHTMLParser::appendRawBytesFromMainThread, m_backgroundParser, passed(std::move(buffer)), bytesReceivedTime);
        return;
    }

    DecodedDataDocumentParser::appendBytes(data, length);
}

void HTMLDocumentParser::flush()
{
    // If we've got no decoder, we never received any data.
    if (isDetached() || needsDecoder())
        return;

    if (shouldUseThreading()) {
        // In some cases, flush() is called without any invocation of
        // appendBytes. Fallback to synchronous parsing in that case.
        if (!m_haveBackgroundParser) {
            m_shouldUseThreading = false;
            m_token = wrapUnique(new HTMLToken);
            m_tokenizer = HTMLTokenizer::create(m_options);
            DecodedDataDocumentParser::flush();
            return;
        }

        postTaskToLookaheadParser(Asynchronous, &BackgroundHTMLParser::flush, m_backgroundParser);
    } else {
        DecodedDataDocumentParser::flush();
    }
}

void HTMLDocumentParser::setDecoder(std::unique_ptr<TextResourceDecoder> decoder)
{
    ASSERT(decoder);
    DecodedDataDocumentParser::setDecoder(std::move(decoder));

    if (m_haveBackgroundParser)
        postTaskToLookaheadParser(Asynchronous, &BackgroundHTMLParser::setDecoder, m_backgroundParser, passed(takeDecoder()));
}

void HTMLDocumentParser::documentElementAvailable()
{
    TRACE_EVENT0("blink,loader", "HTMLDocumentParser::documentElementAvailable");
    DCHECK(document()->documentElement());
    if (!m_queuedPreloads.isEmpty())
        m_preloader->takeAndPreload(m_queuedPreloads);

    for (const String& scriptSource : m_queuedDocumentWriteScripts) {
        evaluateAndPreloadScriptForDocumentWrite(scriptSource);
    }

    m_queuedDocumentWriteScripts.clear();
}

std::unique_ptr<HTMLPreloadScanner> HTMLDocumentParser::createPreloadScanner()
{
    return HTMLPreloadScanner::create(
        m_options,
        document()->url(),
        CachedDocumentParameters::create(document()),
        MediaValuesCached::MediaValuesCachedData(*document()));
}

void HTMLDocumentParser::evaluateAndPreloadScriptForDocumentWrite(const String& source)
{
    if (!m_evaluator->shouldEvaluate(source))
        return;
    document()->loader()->didObserveLoadingBehavior(WebLoadingBehaviorFlag::WebLoadingBehaviorDocumentWriteEvaluator);
    if (!RuntimeEnabledFeatures::documentWriteEvaluatorEnabled())
        return;
    TRACE_EVENT0("blink", "HTMLDocumentParser::evaluateAndPreloadScriptForDocumentWrite");

    double initializeStartTime = monotonicallyIncreasingTimeMS();
    bool neededInitialization = m_evaluator->ensureEvaluationContext();
    double initializationDuration = monotonicallyIncreasingTimeMS() - initializeStartTime;

    double startTime = monotonicallyIncreasingTimeMS();
    String writtenSource = m_evaluator->evaluateAndEmitWrittenSource(source);
    double duration = monotonicallyIncreasingTimeMS() - startTime;

    int currentPreloadCount = document()->loader()->fetcher()->countPreloads();
    std::unique_ptr<HTMLPreloadScanner> scanner = createPreloadScanner();
    scanner->appendToEnd(SegmentedString(writtenSource));
    scanner->scanAndPreload(m_preloader.get(), document()->validBaseElementURL(), nullptr);
    int numPreloads = document()->loader()->fetcher()->countPreloads() - currentPreloadCount;

    TRACE_EVENT_INSTANT2("blink", "HTMLDocumentParser::evaluateAndPreloadScriptForDocumentWrite.data", TRACE_EVENT_SCOPE_THREAD, "numPreloads", numPreloads, "scriptLength", source.length());

    if (neededInitialization) {
        DEFINE_STATIC_LOCAL(CustomCountHistogram, initializeHistograms, ("PreloadScanner.DocumentWrite.InitializationTime", 1, 10000, 50));
        initializeHistograms.count(initializationDuration);
    }

    if (numPreloads) {
        DEFINE_STATIC_LOCAL(CustomCountHistogram, successHistogram, ("PreloadScanner.DocumentWrite.ExecutionTime.Success", 1, 10000, 50));
        successHistogram.count(duration);
    } else {
        DEFINE_STATIC_LOCAL(CustomCountHistogram, failureHistogram, ("PreloadScanner.DocumentWrite.ExecutionTime.Failure", 1, 10000, 50));
        failureHistogram.count(duration);
    }
}

template <typename FunctionType, typename... Ps>
void HTMLDocumentParser::postTaskToLookaheadParser(LookaheadParserTaskSynchrony synchronyPolicy, FunctionType function, Ps&&... parameters)
{
    if (!RuntimeEnabledFeatures::parseHTMLOnMainThreadEnabled()) {
        HTMLParserThread::shared()->postTask(crossThreadBind(function, std::forward<Ps>(parameters)...));
        return;
    }

    // Some messages to the lookahead parser should be synchronous. Otherwise,
    // just post to the loading task runner.
    switch (synchronyPolicy) {
    case Synchronous:
        (*WTF::bind(function, std::forward<Ps>(parameters)...))();
        return;
    case Asynchronous:
        m_loadingTaskRunner->postTask(BLINK_FROM_HERE, WTF::bind(function, std::forward<Ps>(parameters)...));
        return;
    }
    NOTREACHED();
}

} // namespace blink
