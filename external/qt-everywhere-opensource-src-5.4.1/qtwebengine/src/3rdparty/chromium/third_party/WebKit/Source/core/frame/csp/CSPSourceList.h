// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSPSourceList_h
#define CSPSourceList_h

#include "core/frame/csp/CSPSource.h"
#include "platform/Crypto.h"
#include "platform/network/ContentSecurityPolicyParsers.h"
#include "wtf/HashSet.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class ContentSecurityPolicy;
class KURL;

class CSPSourceList {
    WTF_MAKE_NONCOPYABLE(CSPSourceList);
public:
    CSPSourceList(ContentSecurityPolicy*, const String& directiveName);

    void parse(const UChar* begin, const UChar* end);

    bool matches(const KURL&) const;
    bool allowInline() const;
    bool allowEval() const;
    bool allowNonce(const String&) const;
    bool allowHash(const CSPHashValue&) const;
    uint8_t hashAlgorithmsUsed() const;

    bool isHashOrNoncePresent() const;

private:
    bool parseSource(const UChar* begin, const UChar* end, String& scheme, String& host, int& port, String& path, bool& hostHasWildcard, bool& portHasWildcard);
    bool parseScheme(const UChar* begin, const UChar* end, String& scheme);
    bool parseHost(const UChar* begin, const UChar* end, String& host, bool& hostHasWildcard);
    bool parsePort(const UChar* begin, const UChar* end, int& port, bool& portHasWildcard);
    bool parsePath(const UChar* begin, const UChar* end, String& path);
    bool parseNonce(const UChar* begin, const UChar* end, String& nonce);
    bool parseHash(const UChar* begin, const UChar* end, DigestValue& hash, ContentSecurityPolicyHashAlgorithm&);

    void addSourceSelf();
    void addSourceStar();
    void addSourceUnsafeInline();
    void addSourceUnsafeEval();
    void addSourceNonce(const String& nonce);
    void addSourceHash(const ContentSecurityPolicyHashAlgorithm&, const DigestValue& hash);

    ContentSecurityPolicy* m_policy;
    Vector<CSPSource> m_list;
    String m_directiveName;
    bool m_allowStar;
    bool m_allowInline;
    bool m_allowEval;
    HashSet<String> m_nonces;
    HashSet<CSPHashValue> m_hashes;
    uint8_t m_hashAlgorithmsUsed;
};


} // namespace WebCore

#endif
