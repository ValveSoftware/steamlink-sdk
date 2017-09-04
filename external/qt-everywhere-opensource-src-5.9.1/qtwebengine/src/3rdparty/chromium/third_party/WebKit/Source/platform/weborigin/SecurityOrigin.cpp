/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/weborigin/SecurityOrigin.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/KnownPorts.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "platform/weborigin/URLSecurityOriginMap.h"
#include "url/url_canon.h"
#include "url/url_canon_ip.h"
#include "wtf/HexNumber.h"
#include "wtf/NotFound.h"
#include "wtf/PtrUtil.h"
#include "wtf/StdLibExtras.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/StringUTF8Adaptor.h"
#include <memory>

namespace blink {

const int InvalidPort = 0;
const int MaxAllowedPort = 65535;

static URLSecurityOriginMap* s_urlOriginMap = 0;

static SecurityOrigin* getOriginFromMap(const KURL& url) {
  if (s_urlOriginMap)
    return s_urlOriginMap->getOrigin(url);
  return nullptr;
}

bool SecurityOrigin::shouldUseInnerURL(const KURL& url) {
  // FIXME: Blob URLs don't have inner URLs. Their form is
  // "blob:<inner-origin>/<UUID>", so treating the part after "blob:" as a URL
  // is incorrect.
  if (url.protocolIs("blob"))
    return true;
  if (url.protocolIs("filesystem"))
    return true;
  return false;
}

// In general, extracting the inner URL varies by scheme. It just so happens
// that all the URL schemes we currently support that use inner URLs for their
// security origin can be parsed using this algorithm.
KURL SecurityOrigin::extractInnerURL(const KURL& url) {
  if (url.innerURL())
    return *url.innerURL();
  // FIXME: Update this callsite to use the innerURL member function when
  // we finish implementing it.
  return KURL(ParsedURLString, url.path());
}

void SecurityOrigin::setMap(URLSecurityOriginMap* map) {
  s_urlOriginMap = map;
}

static bool shouldTreatAsUniqueOrigin(const KURL& url) {
  if (!url.isValid())
    return true;

  // FIXME: Do we need to unwrap the URL further?
  KURL relevantURL;
  if (SecurityOrigin::shouldUseInnerURL(url)) {
    relevantURL = SecurityOrigin::extractInnerURL(url);
    if (!relevantURL.isValid())
      return true;
  } else {
    relevantURL = url;
  }

  // URLs with schemes that require an authority, but which don't have one,
  // will have failed the isValid() test; e.g. valid HTTP URLs must have a
  // host.
  ASSERT(!(
      (relevantURL.protocolIsInHTTPFamily() || relevantURL.protocolIs("ftp")) &&
      relevantURL.host().isEmpty()));

  if (SchemeRegistry::shouldTreatURLSchemeAsNoAccess(relevantURL.protocol()))
    return true;

  // This is the common case.
  return false;
}

SecurityOrigin::SecurityOrigin(const KURL& url)
    : m_protocol(url.protocol()),
      m_host(url.host()),
      m_port(url.port()),
      m_effectivePort(url.port() ? url.port()
                                 : defaultPortForProtocol(m_protocol)),
      m_isUnique(false),
      m_universalAccess(false),
      m_domainWasSetInDOM(false),
      m_blockLocalAccessFromLocalOrigin(false),
      m_isUniqueOriginPotentiallyTrustworthy(false) {
  if (m_protocol.isNull())
    m_protocol = emptyString();
  if (m_host.isNull())
    m_host = emptyString();

  // Suborigins are serialized into the host, so extract it if necessary.
  String suboriginName;
  if (deserializeSuboriginAndProtocolAndHost(m_protocol, m_host, suboriginName,
                                             m_protocol, m_host)) {
    if (!url.port())
      m_effectivePort = defaultPortForProtocol(m_protocol);

    m_suborigin.setName(suboriginName);
  }

  // document.domain starts as m_host, but can be set by the DOM.
  m_domain = m_host;

  if (isDefaultPortForProtocol(m_port, m_protocol))
    m_port = InvalidPort;

  // By default, only local SecurityOrigins can load local resources.
  m_canLoadLocalResources = isLocal() || equalIgnoringCase(m_protocol, "qrc");
}

SecurityOrigin::SecurityOrigin()
    : m_protocol(emptyString()),
      m_host(emptyString()),
      m_domain(emptyString()),
      m_port(InvalidPort),
      m_effectivePort(InvalidPort),
      m_isUnique(true),
      m_universalAccess(false),
      m_domainWasSetInDOM(false),
      m_canLoadLocalResources(false),
      m_blockLocalAccessFromLocalOrigin(false),
      m_isUniqueOriginPotentiallyTrustworthy(false) {}

SecurityOrigin::SecurityOrigin(const SecurityOrigin* other)
    : m_protocol(other->m_protocol.isolatedCopy()),
      m_host(other->m_host.isolatedCopy()),
      m_domain(other->m_domain.isolatedCopy()),
      m_suborigin(other->m_suborigin),
      m_port(other->m_port),
      m_effectivePort(other->m_effectivePort),
      m_isUnique(other->m_isUnique),
      m_universalAccess(other->m_universalAccess),
      m_domainWasSetInDOM(other->m_domainWasSetInDOM),
      m_canLoadLocalResources(other->m_canLoadLocalResources),
      m_blockLocalAccessFromLocalOrigin(
          other->m_blockLocalAccessFromLocalOrigin),
      m_isUniqueOriginPotentiallyTrustworthy(
          other->m_isUniqueOriginPotentiallyTrustworthy) {}

PassRefPtr<SecurityOrigin> SecurityOrigin::create(const KURL& url) {
  if (RefPtr<SecurityOrigin> origin = getOriginFromMap(url))
    return origin.release();

  if (shouldTreatAsUniqueOrigin(url)) {
    RefPtr<SecurityOrigin> origin = adoptRef(new SecurityOrigin());
    return origin.release();
  }

  if (shouldUseInnerURL(url))
    return adoptRef(new SecurityOrigin(extractInnerURL(url)));

  return adoptRef(new SecurityOrigin(url));
}

PassRefPtr<SecurityOrigin> SecurityOrigin::createUnique() {
  RefPtr<SecurityOrigin> origin = adoptRef(new SecurityOrigin());
  ASSERT(origin->isUnique());
  return origin.release();
}

PassRefPtr<SecurityOrigin> SecurityOrigin::isolatedCopy() const {
  return adoptRef(new SecurityOrigin(this));
}

void SecurityOrigin::setDomainFromDOM(const String& newDomain) {
  m_domainWasSetInDOM = true;
  m_domain = newDomain.lower();
}

bool SecurityOrigin::isSecure(const KURL& url) {
  if (SchemeRegistry::shouldTreatURLSchemeAsSecure(url.protocol()))
    return true;

  // URLs that wrap inner URLs are secure if those inner URLs are secure.
  if (shouldUseInnerURL(url) && SchemeRegistry::shouldTreatURLSchemeAsSecure(
                                    extractInnerURL(url).protocol()))
    return true;

  if (SecurityPolicy::isUrlWhiteListedTrustworthy(url))
    return true;

  return false;
}

bool SecurityOrigin::canAccess(const SecurityOrigin* other) const {
  if (m_universalAccess)
    return true;

  if (this == other)
    return true;

  if (isUnique() || other->isUnique())
    return false;

  // document.domain handling, as per
  // https://html.spec.whatwg.org/multipage/browsers.html#dom-document-domain:
  //
  // 1) Neither document has set document.domain. In this case, we insist
  //    that the scheme, host, and port of the URLs match.
  //
  // 2) Both documents have set document.domain. In this case, we insist
  //    that the documents have set document.domain to the same value and
  //    that the scheme of the URLs match. Ports do not need to match.
  bool canAccess = false;
  if (m_protocol == other->m_protocol) {
    if (!m_domainWasSetInDOM && !other->m_domainWasSetInDOM) {
      if (m_host == other->m_host && m_port == other->m_port)
        canAccess = true;
    } else if (m_domainWasSetInDOM && other->m_domainWasSetInDOM) {
      if (m_domain == other->m_domain)
        canAccess = true;
    }
  }

  if (canAccess && isLocal())
    canAccess = passesFileCheck(other);

  return canAccess;
}

bool SecurityOrigin::canAccessCheckSuborigins(
    const SecurityOrigin* other) const {
  if (hasSuborigin() != other->hasSuborigin())
    return false;

  if (hasSuborigin() && suborigin()->name() != other->suborigin()->name())
    return false;

  return canAccess(other);
}

bool SecurityOrigin::passesFileCheck(const SecurityOrigin* other) const {
  ASSERT(isLocal() && other->isLocal());

  return !m_blockLocalAccessFromLocalOrigin &&
         !other->m_blockLocalAccessFromLocalOrigin;
}

bool SecurityOrigin::canRequest(const KURL& url) const {
  if (m_universalAccess)
    return true;

  if (getOriginFromMap(url) == this)
    return true;

  if (isUnique())
    return false;

  RefPtr<SecurityOrigin> targetOrigin = SecurityOrigin::create(url);

  if (targetOrigin->isUnique())
    return false;

  // We call isSameSchemeHostPort here instead of canAccess because we want
  // to ignore document.domain effects.
  if (isSameSchemeHostPort(targetOrigin.get()))
    return true;

  if (SecurityPolicy::isAccessWhiteListed(this, targetOrigin.get()))
    return true;

  return false;
}

bool SecurityOrigin::canRequestNoSuborigin(const KURL& url) const {
  return !hasSuborigin() && canRequest(url);
}

bool SecurityOrigin::taintsCanvas(const KURL& url) const {
  if (canRequest(url))
    return false;

  // This function exists because we treat data URLs as having a unique origin,
  // contrary to the current (9/19/2009) draft of the HTML5 specification.
  // We still want to let folks paint data URLs onto untainted canvases, so
  // we special case data URLs below. If we change to match HTML5 w.r.t.
  // data URL security, then we can remove this function in favor of
  // !canRequest.
  if (url.protocolIsData())
    return false;

  return true;
}

bool SecurityOrigin::canDisplay(const KURL& url) const {
  if (m_universalAccess)
    return true;

  String protocol = url.protocol();
  if (SchemeRegistry::canDisplayOnlyIfCanRequest(protocol))
    return canRequest(url);

  if (SchemeRegistry::shouldTreatURLSchemeAsDisplayIsolated(protocol))
    return m_protocol == protocol ||
           SecurityPolicy::isAccessToURLWhiteListed(this, url);

  if (SchemeRegistry::shouldTreatURLSchemeAsLocal(protocol))
    return canLoadLocalResources() ||
           SecurityPolicy::isAccessToURLWhiteListed(this, url);

  return true;
}

bool SecurityOrigin::isPotentiallyTrustworthy() const {
  ASSERT(m_protocol != "data");
  if (isUnique())
    return m_isUniqueOriginPotentiallyTrustworthy;

  if (SchemeRegistry::shouldTreatURLSchemeAsSecure(m_protocol) || isLocal() ||
      isLocalhost())
    return true;

  if (SecurityPolicy::isOriginWhiteListedTrustworthy(*this))
    return true;

  return false;
}

// static
String SecurityOrigin::isPotentiallyTrustworthyErrorMessage() {
  return "Only secure origins are allowed (see: https://goo.gl/Y0ZkNV).";
}

void SecurityOrigin::grantLoadLocalResources() {
  // Granting privileges to some, but not all, documents in a SecurityOrigin
  // is a security hazard because the documents without the privilege can
  // obtain the privilege by injecting script into the documents that have
  // been granted the privilege.
  m_canLoadLocalResources = true;
}

void SecurityOrigin::grantUniversalAccess() {
  m_universalAccess = true;
}

void SecurityOrigin::addSuborigin(const Suborigin& suborigin) {
  ASSERT(RuntimeEnabledFeatures::suboriginsEnabled());
  // Changing suborigins midstream is bad. Very bad. It should not happen.
  // This is, in fact,  one of the very basic invariants that makes
  // suborigins an effective security tool.
  RELEASE_ASSERT(m_suborigin.name().isNull() ||
                 (m_suborigin.name() == suborigin.name()));
  m_suborigin.setTo(suborigin);
}

void SecurityOrigin::blockLocalAccessFromLocalOrigin() {
  ASSERT(isLocal());
  m_blockLocalAccessFromLocalOrigin = true;
}

bool SecurityOrigin::isLocal() const {
  return SchemeRegistry::shouldTreatURLSchemeAsLocal(m_protocol);
}

bool SecurityOrigin::isLocalhost() const {
  // Note: net::isLocalhost has looser checks which allow uppercase hosts, as
  // well as hosts like "a.localhost". The net code is also less optimized and
  // slower (mainly string and vector allocations).
  if (m_host == "localhost")
    return true;

  if (m_host == "[::1]")
    return true;

  // Test if m_host matches 127.0.0.1/8
  DCHECK(m_host.containsOnlyASCII());
  StringUTF8Adaptor utf8(m_host);
  Vector<uint8_t, 4> ipNumber;
  ipNumber.resize(4);

  int numComponents;
  url::Component hostComponent(0, utf8.length());
  url::CanonHostInfo::Family family = url::IPv4AddressToNumber(
      utf8.data(), hostComponent, &(ipNumber)[0], &numComponents);
  if (family != url::CanonHostInfo::IPV4)
    return false;
  return ipNumber[0] == 127;
}

String SecurityOrigin::toString() const {
  if (isUnique())
    return "null";
  if (isLocal() && m_blockLocalAccessFromLocalOrigin)
    return "null";
  return toRawString();
}

AtomicString SecurityOrigin::toAtomicString() const {
  if (isUnique())
    return AtomicString("null");
  if (isLocal() && m_blockLocalAccessFromLocalOrigin)
    return AtomicString("null");
  return toRawAtomicString();
}

String SecurityOrigin::toPhysicalOriginString() const {
  if (isUnique())
    return "null";
  if (isLocal() && m_blockLocalAccessFromLocalOrigin)
    return "null";
  return toRawStringIgnoreSuborigin();
}

String SecurityOrigin::toRawString() const {
  if (m_protocol == "file")
    return "file://";

  StringBuilder result;
  buildRawString(result, true);
  return result.toString();
}

String SecurityOrigin::toRawStringIgnoreSuborigin() const {
  if (m_protocol == "file")
    return "file://";

  StringBuilder result;
  buildRawString(result, false);
  return result.toString();
}

// Returns true if and only if a suborigin component was found. If false, no
// guarantees about the return value |suboriginName| are made.
bool SecurityOrigin::deserializeSuboriginAndProtocolAndHost(
    const String& oldProtocol,
    const String& oldHost,
    String& suboriginName,
    String& newProtocol,
    String& newHost) {
  String originalProtocol = oldProtocol;
  if (oldProtocol != "http-so" && oldProtocol != "https-so")
    return false;

  size_t protocolEnd = oldProtocol.reverseFind("-so");
  DCHECK_NE(protocolEnd, WTF::kNotFound);
  newProtocol = oldProtocol.substring(0, protocolEnd);

  size_t suboriginEnd = oldHost.find('.');
  // Suborigins cannot be empty.
  if (suboriginEnd == 0 || suboriginEnd == WTF::kNotFound) {
    newProtocol = originalProtocol;
    return false;
  }

  suboriginName = oldHost.substring(0, suboriginEnd);
  newHost = oldHost.substring(suboriginEnd + 1);

  return true;
}

AtomicString SecurityOrigin::toRawAtomicString() const {
  if (m_protocol == "file")
    return AtomicString("file://");

  StringBuilder result;
  buildRawString(result, true);
  return result.toAtomicString();
}

void SecurityOrigin::buildRawString(StringBuilder& builder,
                                    bool includeSuborigin) const {
  builder.append(m_protocol);
  if (includeSuborigin && hasSuborigin()) {
    builder.append("-so://");
    builder.append(m_suborigin.name());
    builder.append('.');
  } else {
    builder.append("://");
  }
  builder.append(m_host);

  if (m_port) {
    builder.append(':');
    builder.appendNumber(m_port);
  }
}

PassRefPtr<SecurityOrigin> SecurityOrigin::createFromString(
    const String& originString) {
  return SecurityOrigin::create(KURL(KURL(), originString));
}

PassRefPtr<SecurityOrigin> SecurityOrigin::create(const String& protocol,
                                                  const String& host,
                                                  int port) {
  if (port < 0 || port > MaxAllowedPort)
    return createUnique();

  DCHECK_EQ(host, decodeURLEscapeSequences(host));

  String portPart = port ? ":" + String::number(port) : String();
  return create(KURL(KURL(), protocol + "://" + host + portPart + "/"));
}

PassRefPtr<SecurityOrigin> SecurityOrigin::create(const String& protocol,
                                                  const String& host,
                                                  int port,
                                                  const String& suborigin) {
  RefPtr<SecurityOrigin> origin = create(protocol, host, port);
  if (!suborigin.isEmpty())
    origin->m_suborigin.setName(suborigin);
  return origin.release();
}

bool SecurityOrigin::isSameSchemeHostPort(const SecurityOrigin* other) const {
  if (this == other)
    return true;

  if (isUnique() || other->isUnique())
    return false;

  if (m_host != other->m_host)
    return false;

  if (m_protocol != other->m_protocol)
    return false;

  if (m_port != other->m_port)
    return false;

  if (isLocal() && !passesFileCheck(other))
    return false;

  return true;
}

bool SecurityOrigin::isSameSchemeHostPortAndSuborigin(
    const SecurityOrigin* other) const {
  bool sameSuborigins =
      (hasSuborigin() == other->hasSuborigin()) &&
      (!hasSuborigin() || (suborigin()->name() == other->suborigin()->name()));
  return isSameSchemeHostPort(other) && sameSuborigins;
}

const KURL& SecurityOrigin::urlWithUniqueSecurityOrigin() {
  ASSERT(isMainThread());
  DEFINE_STATIC_LOCAL(const KURL, uniqueSecurityOriginURL,
                      (ParsedURLString, "data:,"));
  return uniqueSecurityOriginURL;
}

std::unique_ptr<SecurityOrigin::PrivilegeData>
SecurityOrigin::createPrivilegeData() const {
  std::unique_ptr<PrivilegeData> privilegeData = wrapUnique(new PrivilegeData);
  privilegeData->m_universalAccess = m_universalAccess;
  privilegeData->m_canLoadLocalResources = m_canLoadLocalResources;
  privilegeData->m_blockLocalAccessFromLocalOrigin =
      m_blockLocalAccessFromLocalOrigin;
  return privilegeData;
}

void SecurityOrigin::transferPrivilegesFrom(
    std::unique_ptr<PrivilegeData> privilegeData) {
  m_universalAccess = privilegeData->m_universalAccess;
  m_canLoadLocalResources = privilegeData->m_canLoadLocalResources;
  m_blockLocalAccessFromLocalOrigin =
      privilegeData->m_blockLocalAccessFromLocalOrigin;
}

void SecurityOrigin::setUniqueOriginIsPotentiallyTrustworthy(
    bool isUniqueOriginPotentiallyTrustworthy) {
  ASSERT(!isUniqueOriginPotentiallyTrustworthy || isUnique());
  m_isUniqueOriginPotentiallyTrustworthy = isUniqueOriginPotentiallyTrustworthy;
}

String SecurityOrigin::canonicalizeHost(const String& host, bool* success) {
  url::Component outHost;
  url::RawCanonOutputT<char> canonOutput;
  if (host.is8Bit()) {
    StringUTF8Adaptor utf8(host);
    *success = url::CanonicalizeHost(
        utf8.data(), url::Component(0, utf8.length()), &canonOutput, &outHost);
  } else {
    *success = url::CanonicalizeHost(reinterpret_cast<const base::char16 *>(host.characters16()),
                                     url::Component(0, host.length()),
                                     &canonOutput, &outHost);
  }
  return String::fromUTF8(canonOutput.data(), canonOutput.length());
}

}  // namespace blink
