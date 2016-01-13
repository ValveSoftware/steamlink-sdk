/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009 Torch Mobile, Inc. http://www.torchmobile.com/
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

#include "config.h"
#include "core/html/parser/HTMLPreloadScanner.h"

#include "core/HTMLNames.h"
#include "core/InputTypeNames.h"
#include "core/css/MediaList.h"
#include "core/css/MediaQueryEvaluator.h"
#include "core/css/MediaValues.h"
#include "core/css/parser/SizesAttributeParser.h"
#include "core/html/LinkRelAttribute.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/html/parser/HTMLSrcsetParser.h"
#include "core/html/parser/HTMLTokenizer.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/TraceEvent.h"
#include "wtf/MainThread.h"

namespace WebCore {

using namespace HTMLNames;

static bool match(const StringImpl* impl, const QualifiedName& qName)
{
    return impl == qName.localName().impl();
}

static bool match(const AtomicString& name, const QualifiedName& qName)
{
    ASSERT(isMainThread());
    return qName.localName() == name;
}

static bool match(const String& name, const QualifiedName& qName)
{
    return threadSafeMatch(name, qName);
}

static const StringImpl* tagImplFor(const HTMLToken::DataVector& data)
{
    AtomicString tagName(data);
    const StringImpl* result = tagName.impl();
    if (result->isStatic())
        return result;
    return 0;
}

static const StringImpl* tagImplFor(const String& tagName)
{
    const StringImpl* result = tagName.impl();
    if (result->isStatic())
        return result;
    return 0;
}

static String initiatorFor(const StringImpl* tagImpl)
{
    ASSERT(tagImpl);
    if (match(tagImpl, imgTag))
        return imgTag.localName();
    if (match(tagImpl, inputTag))
        return inputTag.localName();
    if (match(tagImpl, linkTag))
        return linkTag.localName();
    if (match(tagImpl, scriptTag))
        return scriptTag.localName();
    ASSERT_NOT_REACHED();
    return emptyString();
}

static bool mediaAttributeMatches(const MediaValues& mediaValues, const String& attributeValue)
{
    RefPtrWillBeRawPtr<MediaQuerySet> mediaQueries = MediaQuerySet::createOffMainThread(attributeValue);
    MediaQueryEvaluator mediaQueryEvaluator("screen", mediaValues);
    return mediaQueryEvaluator.eval(mediaQueries.get());
}

class TokenPreloadScanner::StartTagScanner {
public:
    StartTagScanner(const StringImpl* tagImpl, PassRefPtr<MediaValues> mediaValues)
        : m_tagImpl(tagImpl)
        , m_linkIsStyleSheet(false)
        , m_matchedMediaAttribute(true)
        , m_inputIsImage(false)
        , m_sourceSize(0)
        , m_sourceSizeSet(false)
        , m_isCORSEnabled(false)
        , m_allowCredentials(DoNotAllowStoredCredentials)
        , m_mediaValues(mediaValues)
    {
        if (match(m_tagImpl, imgTag)
            || match(m_tagImpl, sourceTag)) {
            if (RuntimeEnabledFeatures::pictureSizesEnabled())
                m_sourceSize = SizesAttributeParser::findEffectiveSize(String(), m_mediaValues);
            return;
        }
        if ( !match(m_tagImpl, inputTag)
            && !match(m_tagImpl, linkTag)
            && !match(m_tagImpl, scriptTag))
            m_tagImpl = 0;
    }

    enum URLReplacement {
        AllowURLReplacement,
        DisallowURLReplacement
    };

    void processAttributes(const HTMLToken::AttributeList& attributes)
    {
        ASSERT(isMainThread());
        if (!m_tagImpl)
            return;
        for (HTMLToken::AttributeList::const_iterator iter = attributes.begin(); iter != attributes.end(); ++iter) {
            AtomicString attributeName(iter->name);
            String attributeValue = StringImpl::create8BitIfPossible(iter->value);
            processAttribute(attributeName, attributeValue);
        }
    }

    void processAttributes(const Vector<CompactHTMLToken::Attribute>& attributes)
    {
        if (!m_tagImpl)
            return;
        for (Vector<CompactHTMLToken::Attribute>::const_iterator iter = attributes.begin(); iter != attributes.end(); ++iter)
            processAttribute(iter->name, iter->value);
    }

    void handlePictureSourceURL(String& sourceURL)
    {
        if (match(m_tagImpl, sourceTag) && m_matchedMediaAttribute && sourceURL.isEmpty())
            sourceURL = m_srcsetImageCandidate.toString();
        else if (match(m_tagImpl, imgTag) && !sourceURL.isEmpty())
            setUrlToLoad(sourceURL, AllowURLReplacement);
    }

    PassOwnPtr<PreloadRequest> createPreloadRequest(const KURL& predictedBaseURL, const SegmentedString& source)
    {
        if (!shouldPreload() || !m_matchedMediaAttribute)
            return nullptr;

        TRACE_EVENT_INSTANT1("net", "PreloadRequest", "url", m_urlToLoad.ascii());
        TextPosition position = TextPosition(source.currentLine(), source.currentColumn());
        OwnPtr<PreloadRequest> request = PreloadRequest::create(initiatorFor(m_tagImpl), position, m_urlToLoad, predictedBaseURL, resourceType());
        if (isCORSEnabled())
            request->setCrossOriginEnabled(allowStoredCredentials());
        request->setCharset(charset());
        return request.release();
    }

private:
    template<typename NameType>
    void processScriptAttribute(const NameType& attributeName, const String& attributeValue)
    {
        // FIXME - Don't set crossorigin multiple times.
        if (match(attributeName, srcAttr))
            setUrlToLoad(attributeValue, DisallowURLReplacement);
        else if (match(attributeName, crossoriginAttr))
            setCrossOriginAllowed(attributeValue);
    }

    template<typename NameType>
    void processImgAttribute(const NameType& attributeName, const String& attributeValue)
    {
        if (match(attributeName, srcAttr) && m_imgSrcUrl.isNull()) {
            m_imgSrcUrl = attributeValue;
            setUrlToLoad(bestFitSourceForImageAttributes(m_mediaValues->devicePixelRatio(), m_sourceSize, attributeValue, m_srcsetImageCandidate), AllowURLReplacement);
        } else if (match(attributeName, crossoriginAttr)) {
            setCrossOriginAllowed(attributeValue);
        } else if (match(attributeName, srcsetAttr) && m_srcsetImageCandidate.isEmpty()) {
            m_srcsetAttributeValue = attributeValue;
            m_srcsetImageCandidate = bestFitSourceForSrcsetAttribute(m_mediaValues->devicePixelRatio(), m_sourceSize, attributeValue);
            setUrlToLoad(bestFitSourceForImageAttributes(m_mediaValues->devicePixelRatio(), m_sourceSize, m_imgSrcUrl, m_srcsetImageCandidate), AllowURLReplacement);
        } else if (RuntimeEnabledFeatures::pictureSizesEnabled() && match(attributeName, sizesAttr) && !m_sourceSizeSet) {
            m_sourceSize = SizesAttributeParser::findEffectiveSize(attributeValue, m_mediaValues);
            m_sourceSizeSet = true;
            if (!m_srcsetImageCandidate.isEmpty()) {
                m_srcsetImageCandidate = bestFitSourceForSrcsetAttribute(m_mediaValues->devicePixelRatio(), m_sourceSize, m_srcsetAttributeValue);
                setUrlToLoad(bestFitSourceForImageAttributes(m_mediaValues->devicePixelRatio(), m_sourceSize, m_imgSrcUrl, m_srcsetImageCandidate), AllowURLReplacement);
            }
        }
    }

    template<typename NameType>
    void processLinkAttribute(const NameType& attributeName, const String& attributeValue)
    {
        // FIXME - Don't set rel/media/crossorigin multiple times.
        if (match(attributeName, hrefAttr))
            setUrlToLoad(attributeValue, DisallowURLReplacement);
        else if (match(attributeName, relAttr))
            m_linkIsStyleSheet = relAttributeIsStyleSheet(attributeValue);
        else if (match(attributeName, mediaAttr))
            m_matchedMediaAttribute = mediaAttributeMatches(*m_mediaValues, attributeValue);
        else if (match(attributeName, crossoriginAttr))
            setCrossOriginAllowed(attributeValue);
    }

    template<typename NameType>
    void processInputAttribute(const NameType& attributeName, const String& attributeValue)
    {
        // FIXME - Don't set type multiple times.
        if (match(attributeName, srcAttr))
            setUrlToLoad(attributeValue, DisallowURLReplacement);
        else if (match(attributeName, typeAttr))
            m_inputIsImage = equalIgnoringCase(attributeValue, InputTypeNames::image);
    }

    template<typename NameType>
    void processSourceAttribute(const NameType& attributeName, const String& attributeValue)
    {
        if (!RuntimeEnabledFeatures::pictureEnabled())
            return;
        if (match(attributeName, srcsetAttr) && m_srcsetImageCandidate.isEmpty()) {
            m_srcsetAttributeValue = attributeValue;
            m_srcsetImageCandidate = bestFitSourceForSrcsetAttribute(m_mediaValues->devicePixelRatio(), m_sourceSize, attributeValue);
        } else if (match(attributeName, sizesAttr) && !m_sourceSizeSet) {
            m_sourceSize = SizesAttributeParser::findEffectiveSize(attributeValue, m_mediaValues);
            m_sourceSizeSet = true;
            if (!m_srcsetImageCandidate.isEmpty()) {
                m_srcsetImageCandidate = bestFitSourceForSrcsetAttribute(m_mediaValues->devicePixelRatio(), m_sourceSize, m_srcsetAttributeValue);
            }
        } else if (match(attributeName, mediaAttr)) {
            // FIXME - Don't match media multiple times.
            m_matchedMediaAttribute = mediaAttributeMatches(*m_mediaValues, attributeValue);
        }

    }

    template<typename NameType>
    void processAttribute(const NameType& attributeName, const String& attributeValue)
    {
        if (match(attributeName, charsetAttr))
            m_charset = attributeValue;

        if (match(m_tagImpl, scriptTag))
            processScriptAttribute(attributeName, attributeValue);
        else if (match(m_tagImpl, imgTag))
            processImgAttribute(attributeName, attributeValue);
        else if (match(m_tagImpl, linkTag))
            processLinkAttribute(attributeName, attributeValue);
        else if (match(m_tagImpl, inputTag))
            processInputAttribute(attributeName, attributeValue);
        else if (match(m_tagImpl, sourceTag))
            processSourceAttribute(attributeName, attributeValue);
    }

    static bool relAttributeIsStyleSheet(const String& attributeValue)
    {
        LinkRelAttribute rel(attributeValue);
        return rel.isStyleSheet() && !rel.isAlternate() && rel.iconType() == InvalidIcon && !rel.isDNSPrefetch();
    }

    void setUrlToLoad(const String& value, URLReplacement replacement)
    {
        // We only respect the first src/href, per HTML5:
        // http://www.whatwg.org/specs/web-apps/current-work/multipage/tokenization.html#attribute-name-state
        if (replacement == DisallowURLReplacement && !m_urlToLoad.isEmpty())
            return;
        String url = stripLeadingAndTrailingHTMLSpaces(value);
        if (url.isEmpty())
            return;
        m_urlToLoad = url;
    }

    const String& charset() const
    {
        // FIXME: Its not clear that this if is needed, the loader probably ignores charset for image requests anyway.
        if (match(m_tagImpl, imgTag))
            return emptyString();
        return m_charset;
    }

    Resource::Type resourceType() const
    {
        if (match(m_tagImpl, scriptTag))
            return Resource::Script;
        if (match(m_tagImpl, imgTag) || (match(m_tagImpl, inputTag) && m_inputIsImage))
            return Resource::Image;
        if (match(m_tagImpl, linkTag) && m_linkIsStyleSheet)
            return Resource::CSSStyleSheet;
        ASSERT_NOT_REACHED();
        return Resource::Raw;
    }

    bool shouldPreload() const
    {
        if (m_urlToLoad.isEmpty())
            return false;
        if (match(m_tagImpl, linkTag) && !m_linkIsStyleSheet)
            return false;
        if (match(m_tagImpl, inputTag) && !m_inputIsImage)
            return false;
        return true;
    }

    bool isCORSEnabled() const
    {
        return m_isCORSEnabled;
    }

    StoredCredentials allowStoredCredentials() const
    {
        return m_allowCredentials;
    }

    void setCrossOriginAllowed(const String& corsSetting)
    {
        m_isCORSEnabled = true;
        if (!corsSetting.isNull() && equalIgnoringCase(stripLeadingAndTrailingHTMLSpaces(corsSetting), "use-credentials"))
            m_allowCredentials = AllowStoredCredentials;
        else
            m_allowCredentials = DoNotAllowStoredCredentials;
    }

    const StringImpl* m_tagImpl;
    String m_urlToLoad;
    ImageCandidate m_srcsetImageCandidate;
    String m_charset;
    bool m_linkIsStyleSheet;
    bool m_matchedMediaAttribute;
    bool m_inputIsImage;
    String m_imgSrcUrl;
    String m_srcsetAttributeValue;
    unsigned m_sourceSize;
    bool m_sourceSizeSet;
    bool m_isCORSEnabled;
    StoredCredentials m_allowCredentials;
    RefPtr<MediaValues> m_mediaValues;
};

TokenPreloadScanner::TokenPreloadScanner(const KURL& documentURL, PassRefPtr<MediaValues> mediaValues)
    : m_documentURL(documentURL)
    , m_inStyle(false)
    , m_inPicture(false)
    , m_templateCount(0)
    , m_mediaValues(mediaValues)
{
}

TokenPreloadScanner::~TokenPreloadScanner()
{
}

TokenPreloadScannerCheckpoint TokenPreloadScanner::createCheckpoint()
{
    TokenPreloadScannerCheckpoint checkpoint = m_checkpoints.size();
    m_checkpoints.append(Checkpoint(m_predictedBaseElementURL, m_inStyle, m_templateCount));
    return checkpoint;
}

void TokenPreloadScanner::rewindTo(TokenPreloadScannerCheckpoint checkpointIndex)
{
    ASSERT(checkpointIndex < m_checkpoints.size()); // If this ASSERT fires, checkpointIndex is invalid.
    const Checkpoint& checkpoint = m_checkpoints[checkpointIndex];
    m_predictedBaseElementURL = checkpoint.predictedBaseElementURL;
    m_inStyle = checkpoint.inStyle;
    m_templateCount = checkpoint.templateCount;
    m_cssScanner.reset();
    m_checkpoints.clear();
}

void TokenPreloadScanner::scan(const HTMLToken& token, const SegmentedString& source, PreloadRequestStream& requests)
{
    scanCommon(token, source, requests);
}

void TokenPreloadScanner::scan(const CompactHTMLToken& token, const SegmentedString& source, PreloadRequestStream& requests)
{
    scanCommon(token, source, requests);
}

template<typename Token>
void TokenPreloadScanner::scanCommon(const Token& token, const SegmentedString& source, PreloadRequestStream& requests)
{
    switch (token.type()) {
    case HTMLToken::Character: {
        if (!m_inStyle)
            return;
        m_cssScanner.scan(token.data(), source, requests);
        return;
    }
    case HTMLToken::EndTag: {
        const StringImpl* tagImpl = tagImplFor(token.data());
        if (match(tagImpl, templateTag)) {
            if (m_templateCount)
                --m_templateCount;
            return;
        }
        if (match(tagImpl, styleTag)) {
            if (m_inStyle)
                m_cssScanner.reset();
            m_inStyle = false;
            return;
        }
        if (match(tagImpl, pictureTag))
            m_inPicture = false;
        return;
    }
    case HTMLToken::StartTag: {
        if (m_templateCount)
            return;
        const StringImpl* tagImpl = tagImplFor(token.data());
        if (match(tagImpl, templateTag)) {
            ++m_templateCount;
            return;
        }
        if (match(tagImpl, styleTag)) {
            m_inStyle = true;
            return;
        }
        if (match(tagImpl, baseTag)) {
            // The first <base> element is the one that wins.
            if (!m_predictedBaseElementURL.isEmpty())
                return;
            updatePredictedBaseURL(token);
            return;
        }
        if (RuntimeEnabledFeatures::pictureEnabled() && (match(tagImpl, pictureTag))) {
            m_inPicture = true;
            m_pictureSourceURL = String();
            return;
        }

        StartTagScanner scanner(tagImpl, m_mediaValues);
        scanner.processAttributes(token.attributes());
        if (m_inPicture)
            scanner.handlePictureSourceURL(m_pictureSourceURL);
        OwnPtr<PreloadRequest> request = scanner.createPreloadRequest(m_predictedBaseElementURL, source);
        if (request)
            requests.append(request.release());
        return;
    }
    default: {
        return;
    }
    }
}

template<typename Token>
void TokenPreloadScanner::updatePredictedBaseURL(const Token& token)
{
    ASSERT(m_predictedBaseElementURL.isEmpty());
    if (const typename Token::Attribute* hrefAttribute = token.getAttributeItem(hrefAttr))
        m_predictedBaseElementURL = KURL(m_documentURL, stripLeadingAndTrailingHTMLSpaces(hrefAttribute->value)).copy();
}

HTMLPreloadScanner::HTMLPreloadScanner(const HTMLParserOptions& options, const KURL& documentURL, PassRefPtr<MediaValues> mediaValues)
    : m_scanner(documentURL, mediaValues)
    , m_tokenizer(HTMLTokenizer::create(options))
{
}

HTMLPreloadScanner::~HTMLPreloadScanner()
{
}

void HTMLPreloadScanner::appendToEnd(const SegmentedString& source)
{
    m_source.append(source);
}

void HTMLPreloadScanner::scan(HTMLResourcePreloader* preloader, const KURL& startingBaseElementURL)
{
    ASSERT(isMainThread()); // HTMLTokenizer::updateStateFor only works on the main thread.

    TRACE_EVENT1("webkit", "HTMLPreloadScanner::scan", "source_length", m_source.length());

    // When we start scanning, our best prediction of the baseElementURL is the real one!
    if (!startingBaseElementURL.isEmpty())
        m_scanner.setPredictedBaseElementURL(startingBaseElementURL);

    PreloadRequestStream requests;

    while (m_tokenizer->nextToken(m_source, m_token)) {
        if (m_token.type() == HTMLToken::StartTag)
            m_tokenizer->updateStateFor(attemptStaticStringCreation(m_token.name(), Likely8Bit));
        m_scanner.scan(m_token, m_source, requests);
        m_token.clear();
    }

    preloader->takeAndPreload(requests);
}

}
