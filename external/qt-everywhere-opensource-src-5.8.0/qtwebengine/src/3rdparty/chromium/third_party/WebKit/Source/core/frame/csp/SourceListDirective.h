// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SourceListDirective_h
#define SourceListDirective_h

#include "core/frame/csp/CSPDirective.h"
#include "core/frame/csp/CSPSourceList.h"
#include "platform/network/ContentSecurityPolicyParsers.h"
#include "platform/network/ResourceRequest.h"
#include "wtf/HashSet.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ContentSecurityPolicy;
class KURL;

class SourceListDirective final : public CSPDirective {
    WTF_MAKE_NONCOPYABLE(SourceListDirective);
public:
    SourceListDirective(const String& name, const String& value, ContentSecurityPolicy*);
    DECLARE_TRACE();

    bool allows(const KURL&, ResourceRequest::RedirectStatus) const;
    bool allowInline() const;
    bool allowEval() const;
    bool allowDynamic() const;
    bool allowNonce(const String& nonce) const;
    bool allowHash(const CSPHashValue&) const;
    bool allowHashedAttributes() const;
    bool isHashOrNoncePresent() const;
    uint8_t hashAlgorithmsUsed() const;

private:
    CSPSourceList m_sourceList;
};

} // namespace blink

#endif
