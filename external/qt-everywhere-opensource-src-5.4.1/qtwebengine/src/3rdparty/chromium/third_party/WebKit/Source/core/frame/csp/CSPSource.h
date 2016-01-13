// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSPSource_h
#define CSPSource_h

#include "wtf/text/WTFString.h"

namespace WebCore {

class ContentSecurityPolicy;
class KURL;

class CSPSource {
public:
    CSPSource(ContentSecurityPolicy*, const String& scheme, const String& host, int port, const String& path, bool hostHasWildcard, bool portHasWildcard);
    bool matches(const KURL&) const;

private:
    bool schemeMatches(const KURL&) const;
    bool hostMatches(const KURL&) const;
    bool pathMatches(const KURL&) const;
    bool portMatches(const KURL&) const;
    bool isSchemeOnly() const;

    ContentSecurityPolicy* m_policy;
    String m_scheme;
    String m_host;
    int m_port;
    String m_path;

    bool m_hostHasWildcard;
    bool m_portHasWildcard;
};

} // namespace

#endif
