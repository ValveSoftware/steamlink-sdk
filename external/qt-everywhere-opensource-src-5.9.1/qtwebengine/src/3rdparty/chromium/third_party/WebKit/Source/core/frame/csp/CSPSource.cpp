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

CSPSource::CSPSource(ContentSecurityPolicy* policy,
                     const String& scheme,
                     const String& host,
                     int port,
                     const String& path,
                     WildcardDisposition hostWildcard,
                     WildcardDisposition portWildcard)
    : m_policy(policy),
      m_scheme(scheme.lower()),
      m_host(host),
      m_port(port),
      m_path(path),
      m_hostWildcard(hostWildcard),
      m_portWildcard(portWildcard) {}

bool CSPSource::matches(const KURL& url,
                        ResourceRequest::RedirectStatus redirectStatus) const {
  bool schemesMatch = m_scheme.isEmpty() ? m_policy->protocolMatchesSelf(url)
                                         : schemeMatches(url.protocol());
  if (!schemesMatch)
    return false;
  if (isSchemeOnly())
    return true;
  bool pathsMatch = (redirectStatus == RedirectStatus::FollowedRedirect) ||
                    pathMatches(url.path());
  return hostMatches(url.host()) && portMatches(url.port(), url.protocol()) &&
         pathsMatch;
}

bool CSPSource::schemeMatches(const String& protocol) const {
  DCHECK_EQ(protocol, protocol.lower());
  if (m_scheme == "http")
    return protocol == "http" || protocol == "https";
  if (m_scheme == "ws")
    return protocol == "ws" || protocol == "wss";
  return protocol == m_scheme;
}

bool CSPSource::hostMatches(const String& host) const {
  Document* document = m_policy->document();
  bool match;

  bool equalHosts = m_host == host;
  if (m_hostWildcard == HasWildcard) {
    match = host.endsWith(String("." + m_host), TextCaseInsensitive);

    // Chrome used to, incorrectly, match *.x.y to x.y. This was fixed, but
    // the following count measures when a match fails that would have
    // passed the old, incorrect style, in case a lot of sites were
    // relying on that behavior.
    if (document && equalHosts)
      UseCounter::count(*document,
                        UseCounter::CSPSourceWildcardWouldMatchExactHost);
  } else {
    match = equalHosts;
  }

  return match;
}

bool CSPSource::pathMatches(const String& urlPath) const {
  if (m_path.isEmpty())
    return true;

  String path = decodeURLEscapeSequences(urlPath);

  if (m_path.endsWith("/"))
    return path.startsWith(m_path);

  return path == m_path;
}

bool CSPSource::portMatches(int port, const String& protocol) const {
  if (m_portWildcard == HasWildcard)
    return true;

  if (port == m_port)
    return true;

  if (m_port == 80 &&
      (port == 443 || (port == 0 && defaultPortForProtocol(protocol) == 443)))
    return true;

  if (!port)
    return isDefaultPortForProtocol(m_port, protocol);

  if (!m_port)
    return isDefaultPortForProtocol(port, protocol);

  return false;
}

bool CSPSource::subsumes(CSPSource* other) {
  if (!schemeMatches(other->m_scheme))
    return false;

  if (other->isSchemeOnly() || isSchemeOnly())
    return isSchemeOnly();

  if ((m_hostWildcard == NoWildcard && other->m_hostWildcard == HasWildcard) ||
      (m_portWildcard == NoWildcard && other->m_portWildcard == HasWildcard)) {
    return false;
  }

  bool hostSubsumes = (m_host == other->m_host || hostMatches(other->m_host));
  bool portSubsumes = (m_portWildcard == HasWildcard) ||
                      portMatches(other->m_port, other->m_scheme);
  bool pathSubsumes = pathMatches(other->m_path);
  return hostSubsumes && portSubsumes && pathSubsumes;
}

bool CSPSource::isSimilar(CSPSource* other) {
  bool schemesMatch =
      schemeMatches(other->m_scheme) || other->schemeMatches(m_scheme);
  if (!schemesMatch || isSchemeOnly() || other->isSchemeOnly())
    return schemesMatch;
  bool hostsMatch = (m_host == other->m_host) || hostMatches(other->m_host) ||
                    other->hostMatches(m_host);
  bool portsMatch = (other->m_portWildcard == HasWildcard) ||
                    portMatches(other->m_port, other->m_scheme);
  bool pathsMatch = pathMatches(other->m_path) || other->pathMatches(m_path);
  if (hostsMatch && portsMatch && pathsMatch)
    return true;

  return false;
}

CSPSource* CSPSource::intersect(CSPSource* other) {
  if (!isSimilar(other))
    return nullptr;

  String scheme = other->schemeMatches(m_scheme) ? m_scheme : other->m_scheme;
  String host = m_hostWildcard == NoWildcard ? m_host : other->m_host;
  String path = other->pathMatches(m_path) ? m_path : other->m_path;
  int port = (other->m_portWildcard == HasWildcard || !other->m_port)
                 ? m_port
                 : other->m_port;
  WildcardDisposition hostWildcard =
      (m_hostWildcard == HasWildcard) ? other->m_hostWildcard : m_hostWildcard;
  WildcardDisposition portWildcard =
      (m_portWildcard == HasWildcard) ? other->m_portWildcard : m_portWildcard;
  return new CSPSource(m_policy, scheme, host, port, path, hostWildcard,
                       portWildcard);
}

bool CSPSource::isSchemeOnly() const {
  return m_host.isEmpty();
}

bool CSPSource::firstSubsumesSecond(HeapVector<Member<CSPSource>> listA,
                                    HeapVector<Member<CSPSource>> listB) {
  // Empty vector of CSPSources has an effect of 'none'.
  if (!listA.size() || !listB.size())
    return !listB.size();

  // Walk through all the items in |listB|, ensuring that each is subsumed by at
  // least one item in |listA|. If any item in |listB| is not subsumed, return
  // false.
  for (const auto& sourceB : listB) {
    bool foundMatch = false;
    for (const auto& sourceA : listA) {
      if ((foundMatch = sourceA->subsumes(sourceB)))
        break;
    }
    if (!foundMatch)
      return false;
  }
  return true;
}

DEFINE_TRACE(CSPSource) {
  visitor->trace(m_policy);
}

}  // namespace blink
