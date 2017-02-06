/*
 * Copyright (C) 2013 Google, Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef BackgroundHTMLParser_h
#define BackgroundHTMLParser_h

#include "core/dom/DocumentEncodingData.h"
#include "core/html/parser/BackgroundHTMLInputStream.h"
#include "core/html/parser/CompactHTMLToken.h"
#include "core/html/parser/HTMLParserOptions.h"
#include "core/html/parser/HTMLPreloadScanner.h"
#include "core/html/parser/HTMLSourceTracker.h"
#include "core/html/parser/HTMLTreeBuilderSimulator.h"
#include "core/html/parser/ParsedChunkQueue.h"
#include "core/html/parser/TextResourceDecoder.h"
#include "core/html/parser/XSSAuditorDelegate.h"
#include "wtf/WeakPtr.h"
#include <memory>

namespace blink {

class HTMLDocumentParser;
class XSSAuditor;
class WebTaskRunner;

class BackgroundHTMLParser {
    USING_FAST_MALLOC(BackgroundHTMLParser);
    WTF_MAKE_NONCOPYABLE(BackgroundHTMLParser);
public:
    struct Configuration {
        USING_FAST_MALLOC(Configuration);
    public:
        Configuration();
        HTMLParserOptions options;
        WeakPtr<HTMLDocumentParser> parser;
        std::unique_ptr<XSSAuditor> xssAuditor;
        std::unique_ptr<TextResourceDecoder> decoder;
        RefPtr<ParsedChunkQueue> parsedChunkQueue;
        // outstandingTokenLimit must be greater than or equal to
        // pendingTokenLimit
        size_t outstandingTokenLimit;
        size_t pendingTokenLimit;
    };

    static void start(PassRefPtr<WeakReference<BackgroundHTMLParser>>, std::unique_ptr<Configuration>, const KURL& documentURL, std::unique_ptr<CachedDocumentParameters>, const MediaValuesCached::MediaValuesCachedData&, std::unique_ptr<WebTaskRunner>);

    struct Checkpoint {
        USING_FAST_MALLOC(Checkpoint);
    public:
        WeakPtr<HTMLDocumentParser> parser;
        std::unique_ptr<HTMLToken> token;
        std::unique_ptr<HTMLTokenizer> tokenizer;
        HTMLTreeBuilderSimulator::State treeBuilderState;
        HTMLInputCheckpoint inputCheckpoint;
        TokenPreloadScannerCheckpoint preloadScannerCheckpoint;
        String unparsedInput;
    };

    void appendRawBytesFromMainThread(std::unique_ptr<Vector<char>>, double bytesReceivedTime);
    void setDecoder(std::unique_ptr<TextResourceDecoder>);
    void flush();
    void resumeFrom(std::unique_ptr<Checkpoint>);
    void startedChunkWithCheckpoint(HTMLInputCheckpoint);
    void finish();
    void stop();

    void forcePlaintextForTextDocument();

private:
    BackgroundHTMLParser(PassRefPtr<WeakReference<BackgroundHTMLParser>>, std::unique_ptr<Configuration>, const KURL& documentURL, std::unique_ptr<CachedDocumentParameters>, const MediaValuesCached::MediaValuesCachedData&, std::unique_ptr<WebTaskRunner>);
    ~BackgroundHTMLParser();

    void appendDecodedBytes(const String&);
    void markEndOfFile();
    void pumpTokenizer();
    void sendTokensToMainThread();
    void updateDocument(const String& decodedData);

    template <typename FunctionType, typename... Ps>
    void runOnMainThread(FunctionType, Ps&&...);

    WeakPtrFactory<BackgroundHTMLParser> m_weakFactory;
    BackgroundHTMLInputStream m_input;
    HTMLSourceTracker m_sourceTracker;
    std::unique_ptr<HTMLToken> m_token;
    std::unique_ptr<HTMLTokenizer> m_tokenizer;
    HTMLTreeBuilderSimulator m_treeBuilderSimulator;
    HTMLParserOptions m_options;
    const size_t m_outstandingTokenLimit;
    WeakPtr<HTMLDocumentParser> m_parser;

    std::unique_ptr<CompactHTMLTokenStream> m_pendingTokens;
    const size_t m_pendingTokenLimit;
    PreloadRequestStream m_pendingPreloads;
    // Indices into |m_pendingTokens|.
    Vector<int> m_likelyDocumentWriteScriptIndices;
    ViewportDescriptionWrapper m_viewportDescription;
    XSSInfoStream m_pendingXSSInfos;

    std::unique_ptr<XSSAuditor> m_xssAuditor;
    std::unique_ptr<TokenPreloadScanner> m_preloadScanner;
    std::unique_ptr<TextResourceDecoder> m_decoder;
    DocumentEncodingData m_lastSeenEncodingData;
    std::unique_ptr<WebTaskRunner> m_loadingTaskRunner;
    RefPtr<ParsedChunkQueue> m_parsedChunkQueue;

    bool m_startingScript;
    double m_lastBytesReceivedTime;
};

} // namespace blink

#endif
