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

#ifndef CSSPreloadScanner_h
#define CSSPreloadScanner_h

#include "core/fetch/CSSStyleSheetResource.h"
#include "core/fetch/ResourceOwner.h"
#include "core/fetch/StyleSheetResourceClient.h"
#include "core/html/parser/HTMLToken.h"
#include "core/html/parser/PreloadRequest.h"
#include "platform/heap/Handle.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

class SegmentedString;
class HTMLResourcePreloader;

class CSSPreloadScanner {
  DISALLOW_NEW();
  WTF_MAKE_NONCOPYABLE(CSSPreloadScanner);

 public:
  CSSPreloadScanner();
  ~CSSPreloadScanner();

  void reset();

  void scan(const HTMLToken::DataVector&,
            const SegmentedString&,
            PreloadRequestStream&,
            const KURL&);
  void scan(const String&,
            const SegmentedString&,
            PreloadRequestStream&,
            const KURL&);

  void setReferrerPolicy(const ReferrerPolicy);

 private:
  enum State {
    Initial,
    MaybeComment,
    Comment,
    MaybeCommentEnd,
    RuleStart,
    Rule,
    AfterRule,
    RuleValue,
    AfterRuleValue,
    DoneParsingImportRules,
  };

  template <typename Char>
  void scanCommon(const Char* begin,
                  const Char* end,
                  const SegmentedString&,
                  PreloadRequestStream&,
                  const KURL&);

  inline void tokenize(UChar, const SegmentedString&);
  void emitRule(const SegmentedString&);

  State m_state = Initial;
  StringBuilder m_rule;
  StringBuilder m_ruleValue;

  ReferrerPolicy m_referrerPolicy = ReferrerPolicyDefault;

  // Below members only non-null during scan()
  PreloadRequestStream* m_requests = nullptr;
  const KURL* m_predictedBaseElementURL = nullptr;
};

// Each CSSPreloaderResourceClient keeps track of a single CSS resource, and
// drives a CSSPreloadScanner as raw data arrives for it. This lets us preload
// @import tags before parsing.
class CORE_EXPORT CSSPreloaderResourceClient
    : public GarbageCollectedFinalized<CSSPreloaderResourceClient>,
      public StyleSheetResourceClient {
  USING_GARBAGE_COLLECTED_MIXIN(CSSPreloaderResourceClient);

 public:
  CSSPreloaderResourceClient(Resource*, HTMLResourcePreloader*);
  ~CSSPreloaderResourceClient();
  void setCSSStyleSheet(const String& href,
                        const KURL& baseURL,
                        const String& charset,
                        const CSSStyleSheetResource*) override;
  void didAppendFirstData(const CSSStyleSheetResource*) override;
  String debugName() const override { return "CSSPreloaderResourceClient"; }

  DECLARE_TRACE();

 protected:
  // Protected for tests, which don't want to initialize a fully featured
  // DocumentLoader.
  virtual void fetchPreloads(PreloadRequestStream& preloads);

 private:
  void scanCSS(const CSSStyleSheetResource*);
  void clearResource();

  enum PreloadPolicy {
    ScanOnly,
    ScanAndPreload,
  };

  const PreloadPolicy m_policy;
  WeakMember<HTMLResourcePreloader> m_preloader;
  WeakMember<CSSStyleSheetResource> m_resource;
};

}  // namespace blink

#endif
