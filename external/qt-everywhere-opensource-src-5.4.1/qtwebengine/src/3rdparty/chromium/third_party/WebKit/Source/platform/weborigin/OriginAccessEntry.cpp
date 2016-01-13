/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "platform/weborigin/OriginAccessEntry.h"

#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/Platform.h"
#include "public/platform/WebPublicSuffixList.h"

namespace WebCore {

OriginAccessEntry::OriginAccessEntry(const String& protocol, const String& host, SubdomainSetting subdomainSetting, IPAddressSetting ipAddressSetting)
    : m_protocol(protocol.lower())
    , m_host(host.lower())
    , m_subdomainSettings(subdomainSetting)
    , m_ipAddressSettings(ipAddressSetting)
    , m_hostIsPublicSuffix(false)
{
    ASSERT(subdomainSetting == AllowSubdomains || subdomainSetting == DisallowSubdomains);

    // Assume that any host that ends with a digit is trying to be an IP address.
    m_hostIsIPAddress = !m_host.isEmpty() && isASCIIDigit(m_host[m_host.length() - 1]);

    // Look for top-level domains, either with or without an additional dot.
    if (!m_hostIsIPAddress) {
        blink::WebPublicSuffixList* suffixList = blink::Platform::current()->publicSuffixList();
        if (suffixList && m_host.length() <= suffixList->getPublicSuffixLength(m_host) + 1)
            m_hostIsPublicSuffix = true;
    }
}

OriginAccessEntry::MatchResult OriginAccessEntry::matchesOrigin(const SecurityOrigin& origin) const
{
    ASSERT(origin.host() == origin.host().lower());
    ASSERT(origin.protocol() == origin.protocol().lower());

    if (m_protocol != origin.protocol())
        return DoesNotMatchOrigin;

    // Special case: Include subdomains and empty host means "all hosts, including ip addresses".
    if (m_subdomainSettings == AllowSubdomains && m_host.isEmpty())
        return MatchesOrigin;

    // Exact match.
    if (m_host == origin.host())
        return MatchesOrigin;

    // Otherwise we can only match if we're matching subdomains.
    if (m_subdomainSettings == DisallowSubdomains)
        return DoesNotMatchOrigin;

    // Don't try to do subdomain matching on IP addresses (except for testing).
    if (m_hostIsIPAddress && m_ipAddressSettings == TreatIPAddressAsIPAddress)
        return DoesNotMatchOrigin;

    // Match subdomains.
    if (origin.host().length() <= m_host.length() || origin.host()[origin.host().length() - m_host.length() - 1] != '.' || !origin.host().endsWith(m_host))
        return DoesNotMatchOrigin;

    if (m_hostIsPublicSuffix)
        return MatchesOriginButIsPublicSuffix;

    return MatchesOrigin;
}

} // namespace WebCore
