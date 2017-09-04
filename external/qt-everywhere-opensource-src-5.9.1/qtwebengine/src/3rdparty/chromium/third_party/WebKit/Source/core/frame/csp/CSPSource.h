// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSPSource_h
#define CSPSource_h

#include "core/CoreExport.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "platform/heap/Handle.h"
#include "platform/network/ResourceRequest.h"
#include "wtf/Allocator.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ContentSecurityPolicy;
class KURL;

class CORE_EXPORT CSPSource : public GarbageCollectedFinalized<CSPSource> {
 public:
  enum WildcardDisposition { HasWildcard, NoWildcard };

  CSPSource(ContentSecurityPolicy*,
            const String& scheme,
            const String& host,
            int port,
            const String& path,
            WildcardDisposition hostWildcard,
            WildcardDisposition portWildcard);
  bool matches(const KURL&,
               ResourceRequest::RedirectStatus =
                   ResourceRequest::RedirectStatus::NoRedirect) const;

  // Returns true if this CSPSource subsumes the other, as defined by the
  // algorithm at https://w3c.github.io/webappsec-csp/embedded/#subsume-policy
  bool subsumes(CSPSource*);
  // Retrieve the most restrictive information from the two CSPSources if
  // isSimilar is true for the two. Otherwise, return nullptr.
  CSPSource* intersect(CSPSource*);
  // Returns true if the first list subsumes the second, as defined by the
  // algorithm at
  // https://w3c.github.io/webappsec-csp/embedded/#subsume-source-list
  static bool firstSubsumesSecond(HeapVector<Member<CSPSource>>,
                                  HeapVector<Member<CSPSource>>);

  DECLARE_TRACE();

 private:
  FRIEND_TEST_ALL_PREFIXES(CSPSourceTest, IsSimilar);
  FRIEND_TEST_ALL_PREFIXES(SourceListDirectiveTest, GetIntersectCSPSources);

  bool schemeMatches(const String&) const;
  bool hostMatches(const String&) const;
  bool pathMatches(const String&) const;
  // Protocol is necessary to determine default port if it is zero.
  bool portMatches(int port, const String& protocol) const;
  bool isSchemeOnly() const;
  bool isSimilar(CSPSource* other);

  Member<ContentSecurityPolicy> m_policy;
  String m_scheme;
  String m_host;
  int m_port;
  String m_path;

  WildcardDisposition m_hostWildcard;
  WildcardDisposition m_portWildcard;
};

}  // namespace blink

#endif
