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

#ifndef HTMLDocumentParser_h
#define HTMLDocumentParser_h

#include "bindings/core/v8/DocumentWriteEvaluator.h"
#include "core/dom/ParserContentPolicy.h"
#include "core/dom/ScriptableDocumentParser.h"
#include "core/fetch/ResourceClient.h"
#include "core/frame/UseCounter.h"
#include "core/html/parser/BackgroundHTMLInputStream.h"
#include "core/html/parser/CompactHTMLToken.h"
#include "core/html/parser/HTMLInputStream.h"
#include "core/html/parser/HTMLParserOptions.h"
#include "core/html/parser/HTMLPreloadScanner.h"
#include "core/html/parser/HTMLScriptRunnerHost.h"
#include "core/html/parser/HTMLSourceTracker.h"
#include "core/html/parser/HTMLToken.h"
#include "core/html/parser/HTMLTokenizer.h"
#include "core/html/parser/HTMLTreeBuilderSimulator.h"
#include "core/html/parser/ParserSynchronizationPolicy.h"
#include "core/html/parser/TextResourceDecoder.h"
#include "core/html/parser/XSSAuditor.h"
#include "core/html/parser/XSSAuditorDelegate.h"
#include "platform/text/SegmentedString.h"
#include "wtf/Deque.h"
#include "wtf/WeakPtr.h"
#include "wtf/text/TextPosition.h"
#include <memory>

namespace blink {

class BackgroundHTMLParser;
class CompactHTMLToken;
class Document;
class DocumentEncodingData;
class DocumentFragment;
class Element;
class HTMLDocument;
class HTMLParserScheduler;
class HTMLResourcePreloader;
class HTMLScriptRunner;
class HTMLTreeBuilder;
class ParsedChunkQueue;
class PumpSession;

class HTMLDocumentParser :  public ScriptableDocumentParser, private HTMLScriptRunnerHost {
    USING_GARBAGE_COLLECTED_MIXIN(HTMLDocumentParser);
public:
    static HTMLDocumentParser* create(HTMLDocument& document, ParserSynchronizationPolicy backgroundParsingPolicy)
    {
        return new HTMLDocumentParser(document, backgroundParsingPolicy);
    }
    ~HTMLDocumentParser() override;
    DECLARE_VIRTUAL_TRACE();

    // Exposed for HTMLParserScheduler
    void resumeParsingAfterYield();

    static void parseDocumentFragment(const String&, DocumentFragment*, Element* contextElement, ParserContentPolicy = AllowScriptingContent);

    HTMLTokenizer* tokenizer() const { return m_tokenizer.get(); }

    TextPosition textPosition() const final;
    bool isParsingAtLineNumber() const final;
    OrdinalNumber lineNumber() const final;

    void suspendScheduledTasks() final;
    void resumeScheduledTasks() final;

    struct ParsedChunk {
        USING_FAST_MALLOC(ParsedChunk);
    public:
        std::unique_ptr<CompactHTMLTokenStream> tokens;
        PreloadRequestStream preloads;
        ViewportDescriptionWrapper viewport;
        XSSInfoStream xssInfos;
        HTMLTokenizer::State tokenizerState;
        HTMLTreeBuilderSimulator::State treeBuilderState;
        HTMLInputCheckpoint inputCheckpoint;
        TokenPreloadScannerCheckpoint preloadScannerCheckpoint;
        bool startingScript;
        // Indices into |tokens|.
        Vector<int> likelyDocumentWriteScriptIndices;
    };
    void notifyPendingParsedChunks();
    void didReceiveEncodingDataFromBackgroundParser(const DocumentEncodingData&);

    void appendBytes(const char* bytes, size_t length) override;
    void flush() final;
    void setDecoder(std::unique_ptr<TextResourceDecoder>) final;

protected:
    void insert(const SegmentedString&) final;
    void append(const String&) override;
    void finish() final;

    HTMLDocumentParser(HTMLDocument&, ParserSynchronizationPolicy);
    HTMLDocumentParser(DocumentFragment*, Element* contextElement, ParserContentPolicy);

    HTMLTreeBuilder* treeBuilder() const { return m_treeBuilder.get(); }

    void forcePlaintextForTextDocument();

private:
    static HTMLDocumentParser* create(DocumentFragment* fragment, Element* contextElement, ParserContentPolicy parserContentPolicy)
    {
        return new HTMLDocumentParser(fragment, contextElement, parserContentPolicy);
    }

    // DocumentParser
    void detach() final;
    bool hasInsertionPoint() final;
    void prepareToStopParsing() final;
    void stopParsing() final;
    bool isWaitingForScripts() const final;
    bool isExecutingScript() const final;
    void executeScriptsWaitingForResources() final;
    void documentElementAvailable() override;

    // HTMLScriptRunnerHost
    void notifyScriptLoaded(Resource*) final;
    HTMLInputStream& inputStream() final { return m_input; }
    bool hasPreloadScanner() const final { return m_preloadScanner.get() && !shouldUseThreading(); }
    void appendCurrentInputStreamToPreloadScannerAndScan() final;

    void startBackgroundParser();
    void stopBackgroundParser();
    void validateSpeculations(std::unique_ptr<ParsedChunk> lastChunk);
    void discardSpeculationsAndResumeFrom(std::unique_ptr<ParsedChunk> lastChunk, std::unique_ptr<HTMLToken>, std::unique_ptr<HTMLTokenizer>);
    size_t processParsedChunkFromBackgroundParser(std::unique_ptr<ParsedChunk>);
    void pumpPendingSpeculations();

    bool canTakeNextToken();
    void pumpTokenizer();
    void pumpTokenizerIfPossible();
    void constructTreeFromHTMLToken();
    void constructTreeFromCompactHTMLToken(const CompactHTMLToken&);

    void runScriptsForPausedTreeBuilder();
    void resumeParsingAfterScriptExecution();

    void attemptToEnd();
    void endIfDelayed();
    void attemptToRunDeferredScriptsAndEnd();
    void end();

    bool shouldUseThreading() const { return m_shouldUseThreading; }

    bool isParsingFragment() const;
    bool isScheduledForResume() const;
    bool inPumpSession() const { return m_pumpSessionNestingLevel > 0; }
    bool shouldDelayEnd() const { return inPumpSession() || isWaitingForScripts() || isScheduledForResume() || isExecutingScript(); }

    std::unique_ptr<HTMLPreloadScanner> createPreloadScanner();

    int preloadInsertion(const SegmentedString& source);
    void evaluateAndPreloadScriptForDocumentWrite(const String& source);

    // Temporary enum for the ParseHTMLOnMainThread experiment. This is used to
    // annotate whether a given task should post a task or not on the main
    // thread if the lookahead parser is living on the main thread.
    enum LookaheadParserTaskSynchrony {
        Synchronous,
        Asynchronous,
    };

    // Setting |synchronyPolicy| to Synchronous will just call the function
    // with the given parameters. Note, this method is completely temporary
    // as we need to maintain both threading implementations until the
    // ParseHTMLOnMainThread experiment finishes.
    template <typename FunctionType, typename... Ps>
    void postTaskToLookaheadParser(LookaheadParserTaskSynchrony synchronyPolicy, FunctionType, Ps&&... parameters);

    HTMLToken& token() { return *m_token; }

    HTMLParserOptions m_options;
    HTMLInputStream m_input;

    std::unique_ptr<HTMLToken> m_token;
    std::unique_ptr<HTMLTokenizer> m_tokenizer;
    Member<HTMLScriptRunner> m_scriptRunner;
    Member<HTMLTreeBuilder> m_treeBuilder;
    std::unique_ptr<HTMLPreloadScanner> m_preloadScanner;
    std::unique_ptr<HTMLPreloadScanner> m_insertionPreloadScanner;
    std::unique_ptr<WebTaskRunner> m_loadingTaskRunner;
    Member<HTMLParserScheduler> m_parserScheduler;
    HTMLSourceTracker m_sourceTracker;
    TextPosition m_textPosition;
    XSSAuditor m_xssAuditor;
    XSSAuditorDelegate m_xssAuditorDelegate;

    // FIXME: m_lastChunkBeforeScript, m_tokenizer, m_token, and m_input should be combined into a single state object
    // so they can be set and cleared together and passed between threads together.
    std::unique_ptr<ParsedChunk> m_lastChunkBeforeScript;
    Deque<std::unique_ptr<ParsedChunk>> m_speculations;
    WeakPtrFactory<HTMLDocumentParser> m_weakFactory;
    WeakPtr<BackgroundHTMLParser> m_backgroundParser;
    Member<HTMLResourcePreloader> m_preloader;
    PreloadRequestStream m_queuedPreloads;
    Vector<String> m_queuedDocumentWriteScripts;
    RefPtr<ParsedChunkQueue> m_parsedChunkQueue;
    std::unique_ptr<DocumentWriteEvaluator> m_evaluator;

    bool m_shouldUseThreading;
    bool m_endWasDelayed;
    bool m_haveBackgroundParser;
    bool m_tasksWereSuspended;
    unsigned m_pumpSessionNestingLevel;
    unsigned m_pumpSpeculationsSessionNestingLevel;
    bool m_isParsingAtLineNumber;
    bool m_triedLoadingLinkHeaders;
};

} // namespace blink

#endif
