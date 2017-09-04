/*
 * Copyright (C) 2008, 2010 Apple Inc. All Rights Reserved.
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

#include "core/html/parser/CSSPreloadScanner.h"

#include "core/dom/Document.h"
#include "core/fetch/CSSStyleSheetResource.h"
#include "core/fetch/FetchInitiatorTypeNames.h"
#include "core/frame/Settings.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/html/parser/HTMLResourcePreloader.h"
#include "core/loader/DocumentLoader.h"
#include "platform/Histogram.h"
#include "platform/text/SegmentedString.h"
#include <memory>

namespace blink {

CSSPreloadScanner::CSSPreloadScanner() {}

CSSPreloadScanner::~CSSPreloadScanner() {}

void CSSPreloadScanner::reset() {
  m_state = Initial;
  m_rule.clear();
  m_ruleValue.clear();
}

template <typename Char>
void CSSPreloadScanner::scanCommon(const Char* begin,
                                   const Char* end,
                                   const SegmentedString& source,
                                   PreloadRequestStream& requests,
                                   const KURL& predictedBaseElementURL) {
  m_requests = &requests;
  m_predictedBaseElementURL = &predictedBaseElementURL;

  for (const Char* it = begin; it != end && m_state != DoneParsingImportRules;
       ++it)
    tokenize(*it, source);

  m_requests = nullptr;
  m_predictedBaseElementURL = nullptr;
}

void CSSPreloadScanner::scan(const HTMLToken::DataVector& data,
                             const SegmentedString& source,
                             PreloadRequestStream& requests,
                             const KURL& predictedBaseElementURL) {
  scanCommon(data.data(), data.data() + data.size(), source, requests,
             predictedBaseElementURL);
}

void CSSPreloadScanner::scan(const String& tagName,
                             const SegmentedString& source,
                             PreloadRequestStream& requests,
                             const KURL& predictedBaseElementURL) {
  if (tagName.is8Bit()) {
    const LChar* begin = tagName.characters8();
    scanCommon(begin, begin + tagName.length(), source, requests,
               predictedBaseElementURL);
    return;
  }
  const UChar* begin = tagName.characters16();
  scanCommon(begin, begin + tagName.length(), source, requests,
             predictedBaseElementURL);
}

void CSSPreloadScanner::setReferrerPolicy(const ReferrerPolicy policy) {
  m_referrerPolicy = policy;
}

inline void CSSPreloadScanner::tokenize(UChar c,
                                        const SegmentedString& source) {
  // We are just interested in @import rules, no need for real tokenization here
  // Searching for other types of resources is probably low payoff.
  switch (m_state) {
    case Initial:
      if (isHTMLSpace<UChar>(c))
        break;
      if (c == '@')
        m_state = RuleStart;
      else if (c == '/')
        m_state = MaybeComment;
      else
        m_state = DoneParsingImportRules;
      break;
    case MaybeComment:
      if (c == '*')
        m_state = Comment;
      else
        m_state = Initial;
      break;
    case Comment:
      if (c == '*')
        m_state = MaybeCommentEnd;
      break;
    case MaybeCommentEnd:
      if (c == '*')
        break;
      if (c == '/')
        m_state = Initial;
      else
        m_state = Comment;
      break;
    case RuleStart:
      if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
        m_rule.clear();
        m_ruleValue.clear();
        m_rule.append(c);
        m_state = Rule;
      } else
        m_state = Initial;
      break;
    case Rule:
      if (isHTMLSpace<UChar>(c))
        m_state = AfterRule;
      else if (c == ';')
        m_state = Initial;
      else
        m_rule.append(c);
      break;
    case AfterRule:
      if (isHTMLSpace<UChar>(c))
        break;
      if (c == ';')
        m_state = Initial;
      else if (c == '{')
        m_state = DoneParsingImportRules;
      else {
        m_state = RuleValue;
        m_ruleValue.append(c);
      }
      break;
    case RuleValue:
      if (isHTMLSpace<UChar>(c))
        m_state = AfterRuleValue;
      else if (c == ';')
        emitRule(source);
      else
        m_ruleValue.append(c);
      break;
    case AfterRuleValue:
      if (isHTMLSpace<UChar>(c))
        break;
      if (c == ';')
        emitRule(source);
      else if (c == '{')
        m_state = DoneParsingImportRules;
      else {
        // FIXME: media rules
        m_state = Initial;
      }
      break;
    case DoneParsingImportRules:
      ASSERT_NOT_REACHED();
      break;
  }
}

static String parseCSSStringOrURL(const String& string) {
  size_t offset = 0;
  size_t reducedLength = string.length();

  while (reducedLength && isHTMLSpace<UChar>(string[offset])) {
    ++offset;
    --reducedLength;
  }
  while (reducedLength &&
         isHTMLSpace<UChar>(string[offset + reducedLength - 1]))
    --reducedLength;

  if (reducedLength >= 5 && (string[offset] == 'u' || string[offset] == 'U') &&
      (string[offset + 1] == 'r' || string[offset + 1] == 'R') &&
      (string[offset + 2] == 'l' || string[offset + 2] == 'L') &&
      string[offset + 3] == '(' && string[offset + reducedLength - 1] == ')') {
    offset += 4;
    reducedLength -= 5;
  }

  while (reducedLength && isHTMLSpace<UChar>(string[offset])) {
    ++offset;
    --reducedLength;
  }
  while (reducedLength &&
         isHTMLSpace<UChar>(string[offset + reducedLength - 1]))
    --reducedLength;

  if (reducedLength < 2 ||
      string[offset] != string[offset + reducedLength - 1] ||
      !(string[offset] == '\'' || string[offset] == '"'))
    return String();
  offset++;
  reducedLength -= 2;

  while (reducedLength && isHTMLSpace<UChar>(string[offset])) {
    ++offset;
    --reducedLength;
  }
  while (reducedLength &&
         isHTMLSpace<UChar>(string[offset + reducedLength - 1]))
    --reducedLength;

  return string.substring(offset, reducedLength);
}

void CSSPreloadScanner::emitRule(const SegmentedString& source) {
  if (equalIgnoringCase(m_rule, "import")) {
    String url = parseCSSStringOrURL(m_ruleValue.toString());
    TextPosition position =
        TextPosition(source.currentLine(), source.currentColumn());
    auto request = PreloadRequest::createIfNeeded(
        FetchInitiatorTypeNames::css, position, url, *m_predictedBaseElementURL,
        Resource::CSSStyleSheet, m_referrerPolicy);
    if (request) {
      // FIXME: Should this be including the charset in the preload request?
      m_requests->append(std::move(request));
    }
    m_state = Initial;
  } else if (equalIgnoringCase(m_rule, "charset"))
    m_state = Initial;
  else
    m_state = DoneParsingImportRules;
  m_rule.clear();
  m_ruleValue.clear();
}

CSSPreloaderResourceClient::CSSPreloaderResourceClient(
    Resource* resource,
    HTMLResourcePreloader* preloader)
    : m_policy(preloader->document()->settings()->cssExternalScannerPreload()
                   ? ScanAndPreload
                   : ScanOnly),
      m_preloader(preloader),
      m_resource(toCSSStyleSheetResource(resource)) {
  m_resource->addClient(this);
}

CSSPreloaderResourceClient::~CSSPreloaderResourceClient() {}

void CSSPreloaderResourceClient::setCSSStyleSheet(
    const String& href,
    const KURL& baseURL,
    const String& charset,
    const CSSStyleSheetResource*) {
  clearResource();
}

// Only attach for one appendData call, as that's where most imports will likely
// be (according to spec).
void CSSPreloaderResourceClient::didAppendFirstData(
    const CSSStyleSheetResource* resource) {
  if (m_preloader)
    scanCSS(resource);
  clearResource();
}

void CSSPreloaderResourceClient::scanCSS(
    const CSSStyleSheetResource* resource) {
  DCHECK(m_preloader);

  // Early abort if there is no document loader. Do this early to ensure that
  // scan histograms and preload histograms do not count different quantities.
  if (!m_preloader->document()->loader())
    return;

  // Passing an empty SegmentedString here results in PreloadRequest with no
  // file/line information.
  // TODO(csharrison): If this becomes an issue the CSSPreloadScanner may be
  // augmented to take care of this case without performing an additional
  // copy.
  double startTime = monotonicallyIncreasingTimeMS();
  const String& chunk = resource->sheetText();
  if (chunk.isNull())
    return;
  CSSPreloadScanner cssPreloadScanner;
  PreloadRequestStream preloads;
  cssPreloadScanner.scan(chunk, SegmentedString(), preloads,
                         resource->response().url());
  DEFINE_STATIC_LOCAL(CustomCountHistogram, cssScanTimeHistogram,
                      ("PreloadScanner.ExternalCSS.ScanTime", 1, 1000000, 50));
  cssScanTimeHistogram.count((monotonicallyIncreasingTimeMS() - startTime) *
                             1000);
  fetchPreloads(preloads);
}

void CSSPreloaderResourceClient::fetchPreloads(PreloadRequestStream& preloads) {
  if (preloads.size()) {
    m_preloader->document()->loader()->didObserveLoadingBehavior(
        WebLoadingBehaviorFlag::WebLoadingBehaviorCSSPreloadFound);
  }

  if (m_policy == ScanAndPreload) {
    int currentPreloadCount = m_preloader->countPreloads();
    m_preloader->takeAndPreload(preloads);
    DEFINE_STATIC_LOCAL(
        CustomCountHistogram, cssImportHistogram,
        ("PreloadScanner.ExternalCSS.PreloadCount", 1, 100, 50));
    cssImportHistogram.count(m_preloader->countPreloads() -
                             currentPreloadCount);
  }
}

void CSSPreloaderResourceClient::clearResource() {
  if (m_resource)
    m_resource->removeClient(this);
  m_resource.clear();
}

DEFINE_TRACE(CSSPreloaderResourceClient) {
  visitor->trace(m_preloader);
  visitor->trace(m_resource);
  StyleSheetResourceClient::trace(visitor);
}

}  // namespace blink
