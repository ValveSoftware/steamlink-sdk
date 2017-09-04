// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/csp/SourceListDirective.h"

#include "core/frame/csp/CSPSource.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "platform/network/ContentSecurityPolicyParsers.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "wtf/HashSet.h"
#include "wtf/text/Base64.h"
#include "wtf/text/ParsingUtilities.h"
#include "wtf/text/StringToNumber.h"
#include "wtf/text/WTFString.h"

namespace blink {

SourceListDirective::SourceListDirective(const String& name,
                                         const String& value,
                                         ContentSecurityPolicy* policy)
    : CSPDirective(name, value, policy),
      m_policy(policy),
      m_directiveName(name),
      m_allowSelf(false),
      m_allowStar(false),
      m_allowInline(false),
      m_allowEval(false),
      m_allowDynamic(false),
      m_allowHashedAttributes(false),
      m_hashAlgorithmsUsed(0) {
  Vector<UChar> characters;
  value.appendTo(characters);
  parse(characters.data(), characters.data() + characters.size());
}

static bool isSourceListNone(const UChar* begin, const UChar* end) {
  skipWhile<UChar, isASCIISpace>(begin, end);

  const UChar* position = begin;
  skipWhile<UChar, isSourceCharacter>(position, end);
  if (!equalIgnoringCase("'none'", StringView(begin, position - begin)))
    return false;

  skipWhile<UChar, isASCIISpace>(position, end);
  if (position != end)
    return false;

  return true;
}

bool SourceListDirective::allows(
    const KURL& url,
    ResourceRequest::RedirectStatus redirectStatus) const {
  // Wildcards match network schemes ('http', 'https', 'ftp', 'ws', 'wss'), and
  // the scheme of the protected resource:
  // https://w3c.github.io/webappsec-csp/#match-url-to-source-expression. Other
  // schemes, including custom schemes, must be explicitly listed in a source
  // list.
  if (m_allowStar) {
    if (url.protocolIsInHTTPFamily() || url.protocolIs("ftp") ||
        url.protocolIs("ws") || url.protocolIs("wss") ||
        m_policy->protocolMatchesSelf(url))
      return true;

    return hasSourceMatchInList(url, redirectStatus);
  }

  KURL effectiveURL =
      m_policy->selfMatchesInnerURL() && SecurityOrigin::shouldUseInnerURL(url)
          ? SecurityOrigin::extractInnerURL(url)
          : url;

  if (m_allowSelf && m_policy->urlMatchesSelf(effectiveURL))
    return true;

  return hasSourceMatchInList(effectiveURL, redirectStatus);
}

bool SourceListDirective::allowInline() const {
  return m_allowInline;
}

bool SourceListDirective::allowEval() const {
  return m_allowEval;
}

bool SourceListDirective::allowDynamic() const {
  return m_allowDynamic;
}

bool SourceListDirective::allowNonce(const String& nonce) const {
  String nonceStripped = nonce.stripWhiteSpace();
  return !nonceStripped.isNull() && m_nonces.contains(nonceStripped);
}

bool SourceListDirective::allowHash(const CSPHashValue& hashValue) const {
  return m_hashes.contains(hashValue);
}

bool SourceListDirective::allowHashedAttributes() const {
  return m_allowHashedAttributes;
}

uint8_t SourceListDirective::hashAlgorithmsUsed() const {
  return m_hashAlgorithmsUsed;
}

bool SourceListDirective::isHashOrNoncePresent() const {
  return !m_nonces.isEmpty() ||
         m_hashAlgorithmsUsed != ContentSecurityPolicyHashAlgorithmNone;
}

// source-list       = *WSP [ source *( 1*WSP source ) *WSP ]
//                   / *WSP "'none'" *WSP
//
void SourceListDirective::parse(const UChar* begin, const UChar* end) {
  // We represent 'none' as an empty m_list.
  if (isSourceListNone(begin, end))
    return;

  const UChar* position = begin;
  while (position < end) {
    skipWhile<UChar, isASCIISpace>(position, end);
    if (position == end)
      return;

    const UChar* beginSource = position;
    skipWhile<UChar, isSourceCharacter>(position, end);

    String scheme, host, path;
    int port = 0;
    CSPSource::WildcardDisposition hostWildcard = CSPSource::NoWildcard;
    CSPSource::WildcardDisposition portWildcard = CSPSource::NoWildcard;

    if (parseSource(beginSource, position, scheme, host, port, path,
                    hostWildcard, portWildcard)) {
      // Wildcard hosts and keyword sources ('self', 'unsafe-inline',
      // etc.) aren't stored in m_list, but as attributes on the source
      // list itself.
      if (scheme.isEmpty() && host.isEmpty())
        continue;
      if (m_policy->isDirectiveName(host))
        m_policy->reportDirectiveAsSourceExpression(m_directiveName, host);
      m_list.append(new CSPSource(m_policy, scheme, host, port, path,
                                  hostWildcard, portWildcard));
    } else {
      m_policy->reportInvalidSourceExpression(
          m_directiveName, String(beginSource, position - beginSource));
    }

    DCHECK(position == end || isASCIISpace(*position));
  }
}

// source            = scheme ":"
//                   / ( [ scheme "://" ] host [ port ] [ path ] )
//                   / "'self'"
bool SourceListDirective::parseSource(
    const UChar* begin,
    const UChar* end,
    String& scheme,
    String& host,
    int& port,
    String& path,
    CSPSource::WildcardDisposition& hostWildcard,
    CSPSource::WildcardDisposition& portWildcard) {
  if (begin == end)
    return false;

  StringView token(begin, end - begin);

  if (equalIgnoringCase("'none'", token))
    return false;

  if (end - begin == 1 && *begin == '*') {
    addSourceStar();
    return true;
  }

  if (equalIgnoringCase("'self'", token)) {
    addSourceSelf();
    return true;
  }

  if (equalIgnoringCase("'unsafe-inline'", token)) {
    addSourceUnsafeInline();
    return true;
  }

  if (equalIgnoringCase("'unsafe-eval'", token)) {
    addSourceUnsafeEval();
    return true;
  }

  if (equalIgnoringCase("'strict-dynamic'", token)) {
    addSourceStrictDynamic();
    return true;
  }

  if (equalIgnoringCase("'unsafe-hashed-attributes'", token)) {
    addSourceUnsafeHashedAttributes();
    return true;
  }

  String nonce;
  if (!parseNonce(begin, end, nonce))
    return false;

  if (!nonce.isNull()) {
    addSourceNonce(nonce);
    return true;
  }

  DigestValue hash;
  ContentSecurityPolicyHashAlgorithm algorithm =
      ContentSecurityPolicyHashAlgorithmNone;
  if (!parseHash(begin, end, hash, algorithm))
    return false;

  if (hash.size() > 0) {
    addSourceHash(algorithm, hash);
    return true;
  }

  const UChar* position = begin;
  const UChar* beginHost = begin;
  const UChar* beginPath = end;
  const UChar* beginPort = 0;

  skipWhile<UChar, isNotColonOrSlash>(position, end);

  if (position == end) {
    // host
    //     ^
    return parseHost(beginHost, position, host, hostWildcard);
  }

  if (position < end && *position == '/') {
    // host/path || host/ || /
    //     ^            ^    ^
    return parseHost(beginHost, position, host, hostWildcard) &&
           parsePath(position, end, path);
  }

  if (position < end && *position == ':') {
    if (end - position == 1) {
      // scheme:
      //       ^
      return parseScheme(begin, position, scheme);
    }

    if (position[1] == '/') {
      // scheme://host || scheme://
      //       ^                ^
      if (!parseScheme(begin, position, scheme) ||
          !skipExactly<UChar>(position, end, ':') ||
          !skipExactly<UChar>(position, end, '/') ||
          !skipExactly<UChar>(position, end, '/'))
        return false;
      if (position == end)
        return false;
      beginHost = position;
      skipWhile<UChar, isNotColonOrSlash>(position, end);
    }

    if (position < end && *position == ':') {
      // host:port || scheme://host:port
      //     ^                     ^
      beginPort = position;
      skipUntil<UChar>(position, end, '/');
    }
  }

  if (position < end && *position == '/') {
    // scheme://host/path || scheme://host:port/path
    //              ^                          ^
    if (position == beginHost)
      return false;
    beginPath = position;
  }

  if (!parseHost(beginHost, beginPort ? beginPort : beginPath, host,
                 hostWildcard))
    return false;

  if (beginPort) {
    if (!parsePort(beginPort, beginPath, port, portWildcard))
      return false;
  } else {
    port = 0;
  }

  if (beginPath != end) {
    if (!parsePath(beginPath, end, path))
      return false;
  }

  return true;
}

// nonce-source      = "'nonce-" nonce-value "'"
// nonce-value        = 1*( ALPHA / DIGIT / "+" / "/" / "=" )
//
bool SourceListDirective::parseNonce(const UChar* begin,
                                     const UChar* end,
                                     String& nonce) {
  size_t nonceLength = end - begin;
  StringView prefix("'nonce-");

  // TODO(esprehn): Should be StringView(begin, nonceLength).startsWith(prefix).
  if (nonceLength <= prefix.length() ||
      !equalIgnoringCase(prefix, StringView(begin, prefix.length())))
    return true;

  const UChar* position = begin + prefix.length();
  const UChar* nonceBegin = position;

  DCHECK(position < end);
  skipWhile<UChar, isNonceCharacter>(position, end);
  DCHECK(nonceBegin <= position);

  if (position + 1 != end || *position != '\'' || position == nonceBegin)
    return false;

  nonce = String(nonceBegin, position - nonceBegin);
  return true;
}

// hash-source       = "'" hash-algorithm "-" hash-value "'"
// hash-algorithm    = "sha1" / "sha256" / "sha384" / "sha512"
// hash-value        = 1*( ALPHA / DIGIT / "+" / "/" / "=" )
//
bool SourceListDirective::parseHash(
    const UChar* begin,
    const UChar* end,
    DigestValue& hash,
    ContentSecurityPolicyHashAlgorithm& hashAlgorithm) {
  // Any additions or subtractions from this struct should also modify the
  // respective entries in the kAlgorithmMap array in checkDigest().
  static const struct {
    const char* prefix;
    ContentSecurityPolicyHashAlgorithm type;
  } kSupportedPrefixes[] = {
      // FIXME: Drop support for SHA-1. It's not in the spec.
      {"'sha1-", ContentSecurityPolicyHashAlgorithmSha1},
      {"'sha256-", ContentSecurityPolicyHashAlgorithmSha256},
      {"'sha384-", ContentSecurityPolicyHashAlgorithmSha384},
      {"'sha512-", ContentSecurityPolicyHashAlgorithmSha512},
      {"'sha-256-", ContentSecurityPolicyHashAlgorithmSha256},
      {"'sha-384-", ContentSecurityPolicyHashAlgorithmSha384},
      {"'sha-512-", ContentSecurityPolicyHashAlgorithmSha512}};

  StringView prefix;
  hashAlgorithm = ContentSecurityPolicyHashAlgorithmNone;
  size_t hashLength = end - begin;

  for (const auto& algorithm : kSupportedPrefixes) {
    prefix = algorithm.prefix;
    // TODO(esprehn): Should be StringView(begin, end -
    // begin).startsWith(prefix).
    if (hashLength > prefix.length() &&
        equalIgnoringCase(prefix, StringView(begin, prefix.length()))) {
      hashAlgorithm = algorithm.type;
      break;
    }
  }

  if (hashAlgorithm == ContentSecurityPolicyHashAlgorithmNone)
    return true;

  const UChar* position = begin + prefix.length();
  const UChar* hashBegin = position;

  DCHECK(position < end);
  skipWhile<UChar, isBase64EncodedCharacter>(position, end);
  DCHECK(hashBegin <= position);

  // Base64 encodings may end with exactly one or two '=' characters
  if (position < end)
    skipExactly<UChar>(position, position + 1, '=');
  if (position < end)
    skipExactly<UChar>(position, position + 1, '=');

  if (position + 1 != end || *position != '\'' || position == hashBegin)
    return false;

  Vector<char> hashVector;
  // We accept base64url-encoded data here by normalizing it to base64.
  base64Decode(normalizeToBase64(String(hashBegin, position - hashBegin)),
               hashVector);
  if (hashVector.size() > kMaxDigestSize)
    return false;
  hash.append(reinterpret_cast<uint8_t*>(hashVector.data()), hashVector.size());
  return true;
}

//                     ; <scheme> production from RFC 3986
// scheme      = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
//
bool SourceListDirective::parseScheme(const UChar* begin,
                                      const UChar* end,
                                      String& scheme) {
  DCHECK(begin <= end);
  DCHECK(scheme.isEmpty());

  if (begin == end)
    return false;

  const UChar* position = begin;

  if (!skipExactly<UChar, isASCIIAlpha>(position, end))
    return false;

  skipWhile<UChar, isSchemeContinuationCharacter>(position, end);

  if (position != end)
    return false;

  scheme = String(begin, end - begin);
  return true;
}

// host              = [ "*." ] 1*host-char *( "." 1*host-char )
//                   / "*"
// host-char         = ALPHA / DIGIT / "-"
//
bool SourceListDirective::parseHost(
    const UChar* begin,
    const UChar* end,
    String& host,
    CSPSource::WildcardDisposition& hostWildcard) {
  DCHECK(begin <= end);
  DCHECK(host.isEmpty());
  DCHECK(hostWildcard == CSPSource::NoWildcard);

  if (begin == end)
    return false;

  const UChar* position = begin;

  if (skipExactly<UChar>(position, end, '*')) {
    hostWildcard = CSPSource::HasWildcard;

    if (position == end)
      return true;

    if (!skipExactly<UChar>(position, end, '.'))
      return false;
  }

  const UChar* hostBegin = position;

  while (position < end) {
    if (!skipExactly<UChar, isHostCharacter>(position, end))
      return false;

    skipWhile<UChar, isHostCharacter>(position, end);

    if (position < end && !skipExactly<UChar>(position, end, '.'))
      return false;
  }

  DCHECK(position == end);
  host = String(hostBegin, end - hostBegin);
  return true;
}

bool SourceListDirective::parsePath(const UChar* begin,
                                    const UChar* end,
                                    String& path) {
  DCHECK(begin <= end);
  DCHECK(path.isEmpty());

  const UChar* position = begin;
  skipWhile<UChar, isPathComponentCharacter>(position, end);
  // path/to/file.js?query=string || path/to/file.js#anchor
  //                ^                               ^
  if (position < end) {
    m_policy->reportInvalidPathCharacter(m_directiveName,
                                         String(begin, end - begin), *position);
  }

  path = decodeURLEscapeSequences(String(begin, position - begin));

  DCHECK(position <= end);
  DCHECK(position == end || (*position == '#' || *position == '?'));
  return true;
}

// port              = ":" ( 1*DIGIT / "*" )
//
bool SourceListDirective::parsePort(
    const UChar* begin,
    const UChar* end,
    int& port,
    CSPSource::WildcardDisposition& portWildcard) {
  DCHECK(begin <= end);
  DCHECK(!port);
  DCHECK(portWildcard == CSPSource::NoWildcard);

  if (!skipExactly<UChar>(begin, end, ':'))
    NOTREACHED();

  if (begin == end)
    return false;

  if (end - begin == 1 && *begin == '*') {
    port = 0;
    portWildcard = CSPSource::HasWildcard;
    return true;
  }

  const UChar* position = begin;
  skipWhile<UChar, isASCIIDigit>(position, end);

  if (position != end)
    return false;

  bool ok;
  port = charactersToIntStrict(begin, end - begin, &ok);
  return ok;
}

void SourceListDirective::addSourceSelf() {
  m_allowSelf = true;
}

void SourceListDirective::addSourceStar() {
  m_allowStar = true;
}

void SourceListDirective::addSourceUnsafeInline() {
  m_allowInline = true;
}

void SourceListDirective::addSourceUnsafeEval() {
  m_allowEval = true;
}

void SourceListDirective::addSourceStrictDynamic() {
  m_allowDynamic = true;
}

void SourceListDirective::addSourceUnsafeHashedAttributes() {
  m_allowHashedAttributes = true;
}

void SourceListDirective::addSourceNonce(const String& nonce) {
  m_nonces.add(nonce);
}

void SourceListDirective::addSourceHash(
    const ContentSecurityPolicyHashAlgorithm& algorithm,
    const DigestValue& hash) {
  m_hashes.add(CSPHashValue(algorithm, hash));
  m_hashAlgorithmsUsed |= algorithm;
}

bool SourceListDirective::hasSourceMatchInList(
    const KURL& url,
    ResourceRequest::RedirectStatus redirectStatus) const {
  for (size_t i = 0; i < m_list.size(); ++i) {
    if (m_list[i]->matches(url, redirectStatus))
      return true;
  }

  return false;
}

bool SourceListDirective::subsumes(
    HeapVector<Member<SourceListDirective>> other) {
  // TODO(amalika): Handle here special keywords.
  if (!m_list.size() || !other.size())
    return !m_list.size();

  HeapVector<Member<CSPSource>> normalizedA = other[0]->m_list;
  for (size_t i = 1; i < other.size(); i++) {
    normalizedA = other[i]->getIntersectCSPSources(normalizedA);
  }

  return CSPSource::firstSubsumesSecond(m_list, normalizedA);
}

HeapVector<Member<CSPSource>> SourceListDirective::getIntersectCSPSources(
    HeapVector<Member<CSPSource>> otherVector) {
  HeapVector<Member<CSPSource>> normalized;
  for (const auto& aCspSource : m_list) {
    Member<CSPSource> matchedCspSource(nullptr);
    for (const auto& bCspSource : otherVector) {
      if ((matchedCspSource = bCspSource->intersect(aCspSource)))
        break;
    }
    if (matchedCspSource)
      normalized.append(matchedCspSource);
  }
  return normalized;
}

DEFINE_TRACE(SourceListDirective) {
  visitor->trace(m_policy);
  visitor->trace(m_list);
  CSPDirective::trace(visitor);
}

}  // namespace blink
