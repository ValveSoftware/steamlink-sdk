/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2010 Google Inc. All Rights Reserved.
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

#ifndef HTMLPreloadScanner_h
#define HTMLPreloadScanner_h

#include "core/CoreExport.h"
#include "core/css/MediaValuesCached.h"
#include "core/dom/ViewportDescription.h"
#include "core/html/parser/CSSPreloadScanner.h"
#include "core/html/parser/CompactHTMLToken.h"
#include "core/html/parser/HTMLToken.h"
#include "platform/text/SegmentedString.h"
#include "wtf/PtrUtil.h"
#include "wtf/Vector.h"
#include <memory>

namespace blink {

typedef size_t TokenPreloadScannerCheckpoint;

class HTMLParserOptions;
class HTMLTokenizer;
class SegmentedString;

struct ViewportDescriptionWrapper {
    ViewportDescription description;
    bool set;
    ViewportDescriptionWrapper()
        : set(false)
    {
    }
};

struct CORE_EXPORT CachedDocumentParameters {
    USING_FAST_MALLOC(CachedDocumentParameters);
public:
    static std::unique_ptr<CachedDocumentParameters> create(Document* document)
    {
        return wrapUnique(new CachedDocumentParameters(document));
    }

    bool doHtmlPreloadScanning;
    bool doDocumentWritePreloadScanning;
    Length defaultViewportMinWidth;
    bool viewportMetaZeroValuesQuirk;
    bool viewportMetaEnabled;
    ReferrerPolicy referrerPolicy;

private:
    explicit CachedDocumentParameters(Document*);
};

class TokenPreloadScanner {
    WTF_MAKE_NONCOPYABLE(TokenPreloadScanner); USING_FAST_MALLOC(TokenPreloadScanner);
public:
    TokenPreloadScanner(const KURL& documentURL, std::unique_ptr<CachedDocumentParameters>, const MediaValuesCached::MediaValuesCachedData&);
    ~TokenPreloadScanner();

    void scan(const HTMLToken&, const SegmentedString&, PreloadRequestStream& requests, ViewportDescriptionWrapper*);
    void scan(const CompactHTMLToken&, const SegmentedString&, PreloadRequestStream& requests, ViewportDescriptionWrapper*, bool* likelyDocumentWriteScript);

    void setPredictedBaseElementURL(const KURL& url) { m_predictedBaseElementURL = url; }

    // A TokenPreloadScannerCheckpoint is valid until the next call to rewindTo,
    // at which point all outstanding checkpoints are invalidated.
    TokenPreloadScannerCheckpoint createCheckpoint();
    void rewindTo(TokenPreloadScannerCheckpoint);

private:
    class StartTagScanner;

    bool shouldEvaluateForDocumentWrite(const String& source);
    bool shouldEvaluateForDocumentWrite(const HTMLToken::DataVector& source) { return false; }

    template <typename Token>
    inline void scanCommon(const Token&, const SegmentedString&, PreloadRequestStream& requests, ViewportDescriptionWrapper*, bool* likelyDocumentWriteScript);

    template<typename Token>
    void updatePredictedBaseURL(const Token&);

    struct Checkpoint {
        Checkpoint(const KURL& predictedBaseElementURL, bool inStyle, bool inScript, bool isAppCacheEnabled, bool isCSPEnabled, size_t templateCount)
            : predictedBaseElementURL(predictedBaseElementURL)
            , inStyle(inStyle)
            , inScript(inScript)
            , isAppCacheEnabled(isAppCacheEnabled)
            , isCSPEnabled(isCSPEnabled)
            , templateCount(templateCount)
        {
        }

        KURL predictedBaseElementURL;
        bool inStyle;
        bool inScript;
        bool isAppCacheEnabled;
        bool isCSPEnabled;
        size_t templateCount;
    };

    struct PictureData {
        PictureData()
            : sourceSize(0.0)
            , sourceSizeSet(false)
            , picked(false)
        {
        }
        String sourceURL;
        float sourceSize;
        bool sourceSizeSet;
        bool picked;
    };

    CSSPreloadScanner m_cssScanner;
    const KURL m_documentURL;
    KURL m_predictedBaseElementURL;
    bool m_inStyle;
    bool m_inPicture;
    bool m_inScript;
    bool m_isAppCacheEnabled;
    bool m_isCSPEnabled;
    PictureData m_pictureData;
    size_t m_templateCount;
    std::unique_ptr<CachedDocumentParameters> m_documentParameters;
    Persistent<MediaValuesCached> m_mediaValues;
    ClientHintsPreferences m_clientHintsPreferences;

    bool m_didRewind = false;

    Vector<Checkpoint> m_checkpoints;
};

class CORE_EXPORT HTMLPreloadScanner {
    WTF_MAKE_NONCOPYABLE(HTMLPreloadScanner); USING_FAST_MALLOC(HTMLPreloadScanner);
public:
    static std::unique_ptr<HTMLPreloadScanner> create(const HTMLParserOptions& options, const KURL& documentURL, std::unique_ptr<CachedDocumentParameters> documentParameters, const MediaValuesCached::MediaValuesCachedData& mediaValuesCachedData)
    {
        return wrapUnique(new HTMLPreloadScanner(options, documentURL, std::move(documentParameters), mediaValuesCachedData));
    }


    ~HTMLPreloadScanner();

    void appendToEnd(const SegmentedString&);
    void scanAndPreload(ResourcePreloader*, const KURL& documentBaseElementURL, ViewportDescriptionWrapper*);

private:
    HTMLPreloadScanner(const HTMLParserOptions&, const KURL& documentURL, std::unique_ptr<CachedDocumentParameters>, const MediaValuesCached::MediaValuesCachedData&);

    TokenPreloadScanner m_scanner;
    SegmentedString m_source;
    HTMLToken m_token;
    std::unique_ptr<HTMLTokenizer> m_tokenizer;
};

} // namespace blink

#endif
