// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/csp/CSPSource.h"

#include "core/frame/UseCounter.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/KnownPorts.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "wtf/text/WTFString.h"

namespace blink {

CSPSource::CSPSource(ContentSecurityPolicy* policy, const String& scheme, const String& host, int port, const String& path, WildcardDisposition hostWildcard, WildcardDisposition portWildcard)
    : m_policy(policy)
    , m_scheme(scheme)
    , m_host(host)
    , m_port(port)
    , m_path(path)
    , m_hostWildcard(hostWildcard)
    , m_portWildcard(portWildcard)
{
}

bool CSPSource::matches(const KURL& url, ResourceRequest::RedirectStatus redirectStatus) const
{
    if (!schemeMatches(url))
        return false;
    if (isSchemeOnly())
        return true;
    bool pathsMatch = (redirectStatus == RedirectStatus::FollowedRedirect) || pathMatches(url);
    return hostMatches(url) && portMatches(url) && pathsMatch;
}

bool CSPSource::schemeMatches(const KURL& url) const
{
    if (m_scheme.isEmpty())
        return m_policy->protocolMatchesSelf(url);
    if (equalIgnoringCase(m_scheme, "http"))
        return equalIgnoringCase(url.protocol(), "http") || equalIgnoringCase(url.protocol(), "https");
    if (equalIgnoringCase(m_scheme, "ws"))
        return equalIgnoringCase(url.protocol(), "ws") || equalIgnoringCase(url.protocol(), "wss");
    return equalIgnoringCase(url.protocol(), m_scheme);
}

bool CSPSource::hostMatches(const KURL& url) const
{
    const String& host = url.host();
    Document* document = m_policy->document();
    bool match;

    bool equalHosts = equalIgnoringCase(host, m_host);
    if (m_hostWildcard == HasWildcard) {
        match = host.endsWith("." + m_host, TextCaseInsensitive);

        // Chrome used to, incorrectly, match *.x.y to x.y. This was fixed, but
        // the following count measures when a match fails that would have
        // passed the old, incorrect style, in case a lot of sites were
        // relying on that behavior.
        if (document && equalHosts)
            UseCounter::count(*document, UseCounter::CSPSourceWildcardWouldMatchExactHost);
    } else {
        match = equalHosts;
    }

    return match;
}

bool CSPSource::pathMatches(const KURL& url) const
{
    if (m_path.isEmpty())
        return true;

    String path = decodeURLEscapeSequences(url.path());

    if (m_path.endsWith("/"))
        return path.startsWith(m_path);

    return path == m_path;
}

bool CSPSource::portMatches(const KURL& url) const
{
    if (m_portWildcard == HasWildcard)
        return true;

    int port = url.port();

    if (port == m_port)
        return true;

    if (m_port == 80 && (port == 443 || (port == 0 && defaultPortForProtocol(url.protocol()) == 443)))
        return true;

    if (!port)
        return isDefaultPortForProtocol(m_port, url.protocol());

    if (!m_port)
        return isDefaultPortForProtocol(port, url.protocol());

    return false;
}

bool CSPSource::isSchemeOnly() const
{
    return m_host.isEmpty();
}

DEFINE_TRACE(CSPSource)
{
    visitor->trace(m_policy);
}

} // namespace blink
