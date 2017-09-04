// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SourceListDirective_h
#define SourceListDirective_h

#include "core/CoreExport.h"
#include "core/frame/csp/CSPDirective.h"
#include "core/frame/csp/CSPSource.h"
#include "platform/Crypto.h"
#include "platform/network/ContentSecurityPolicyParsers.h"
#include "platform/network/ResourceRequest.h"
#include "wtf/HashSet.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ContentSecurityPolicy;
class KURL;

class CORE_EXPORT SourceListDirective final : public CSPDirective {
  WTF_MAKE_NONCOPYABLE(SourceListDirective);

 public:
  SourceListDirective(const String& name,
                      const String& value,
                      ContentSecurityPolicy*);
  DECLARE_TRACE();

  void parse(const UChar* begin, const UChar* end);

  bool matches(const KURL&,
               ResourceRequest::RedirectStatus =
                   ResourceRequest::RedirectStatus::NoRedirect) const;

  bool allows(const KURL&,
              ResourceRequest::RedirectStatus =
                  ResourceRequest::RedirectStatus::NoRedirect) const;
  bool allowInline() const;
  bool allowEval() const;
  bool allowDynamic() const;
  bool allowNonce(const String& nonce) const;
  bool allowHash(const CSPHashValue&) const;
  bool allowHashedAttributes() const;
  bool isHashOrNoncePresent() const;
  uint8_t hashAlgorithmsUsed() const;
  // The algorothm is described more extensively here:
  // https://w3c.github.io/webappsec-csp/embedded/#subsume-source-list
  bool subsumes(HeapVector<Member<SourceListDirective>>);

 private:
  FRIEND_TEST_ALL_PREFIXES(SourceListDirectiveTest, GetIntersectCSPSources);

  bool parseSource(const UChar* begin,
                   const UChar* end,
                   String& scheme,
                   String& host,
                   int& port,
                   String& path,
                   CSPSource::WildcardDisposition&,
                   CSPSource::WildcardDisposition&);
  bool parseScheme(const UChar* begin, const UChar* end, String& scheme);
  bool parseHost(const UChar* begin,
                 const UChar* end,
                 String& host,
                 CSPSource::WildcardDisposition&);
  bool parsePort(const UChar* begin,
                 const UChar* end,
                 int& port,
                 CSPSource::WildcardDisposition&);
  bool parsePath(const UChar* begin, const UChar* end, String& path);
  bool parseNonce(const UChar* begin, const UChar* end, String& nonce);
  bool parseHash(const UChar* begin,
                 const UChar* end,
                 DigestValue& hash,
                 ContentSecurityPolicyHashAlgorithm&);

  void addSourceSelf();
  void addSourceStar();
  void addSourceUnsafeInline();
  void addSourceUnsafeEval();
  void addSourceStrictDynamic();
  void addSourceUnsafeHashedAttributes();
  void addSourceNonce(const String& nonce);
  void addSourceHash(const ContentSecurityPolicyHashAlgorithm&,
                     const DigestValue& hash);

  bool hasSourceMatchInList(const KURL&, ResourceRequest::RedirectStatus) const;
  HeapVector<Member<CSPSource>> getIntersectCSPSources(
      HeapVector<Member<CSPSource>> other);

  Member<ContentSecurityPolicy> m_policy;
  HeapVector<Member<CSPSource>> m_list;
  String m_directiveName;
  bool m_allowSelf;
  bool m_allowStar;
  bool m_allowInline;
  bool m_allowEval;
  bool m_allowDynamic;
  bool m_allowHashedAttributes;
  HashSet<String> m_nonces;
  HashSet<CSPHashValue> m_hashes;
  uint8_t m_hashAlgorithmsUsed;
};

}  // namespace blink

#endif
