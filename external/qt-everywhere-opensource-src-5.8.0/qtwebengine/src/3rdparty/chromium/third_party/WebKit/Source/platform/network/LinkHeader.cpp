// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/network/LinkHeader.h"

#include "base/strings/string_util.h"
#include "components/link_header_util/link_header_util.h"
#include "platform/ParsingUtilities.h"

namespace blink {

// Verify that the parameter is a link-extension which according to spec doesn't have to have a value.
static bool isExtensionParameter(LinkHeader::LinkParameterName name)
{
    return name >= LinkHeader::LinkParameterUnknown;
}

static LinkHeader::LinkParameterName parameterNameFromString(base::StringPiece name)
{
    if (base::EqualsCaseInsensitiveASCII(name, "rel"))
        return LinkHeader::LinkParameterRel;
    if (base::EqualsCaseInsensitiveASCII(name, "anchor"))
        return LinkHeader::LinkParameterAnchor;
    if (base::EqualsCaseInsensitiveASCII(name, "crossorigin"))
        return LinkHeader::LinkParameterCrossOrigin;
    if (base::EqualsCaseInsensitiveASCII(name, "title"))
        return LinkHeader::LinkParameterTitle;
    if (base::EqualsCaseInsensitiveASCII(name, "media"))
        return LinkHeader::LinkParameterMedia;
    if (base::EqualsCaseInsensitiveASCII(name, "type"))
        return LinkHeader::LinkParameterType;
    if (base::EqualsCaseInsensitiveASCII(name, "rev"))
        return LinkHeader::LinkParameterRev;
    if (base::EqualsCaseInsensitiveASCII(name, "hreflang"))
        return LinkHeader::LinkParameterHreflang;
    if (base::EqualsCaseInsensitiveASCII(name, "as"))
        return LinkHeader::LinkParameterAs;
    return LinkHeader::LinkParameterUnknown;
}

void LinkHeader::setValue(LinkParameterName name, const String& value)
{
    if (name == LinkParameterRel && !m_rel)
        m_rel = value.lower();
    else if (name == LinkParameterAnchor)
        m_isValid = false;
    else if (name == LinkParameterCrossOrigin)
        m_crossOrigin = value;
    else if (name == LinkParameterAs)
        m_as = value.lower();
    else if (name == LinkParameterType)
        m_mimeType = value.lower();
    else if (name == LinkParameterMedia)
        m_media = value.lower();
}

template <typename Iterator>
LinkHeader::LinkHeader(Iterator begin, Iterator end)
    : m_isValid(true)
{
    std::string url;
    std::unordered_map<std::string, base::Optional<std::string>> params;
    m_isValid = link_header_util::ParseLinkHeaderValue(begin, end, &url, &params);
    if (!m_isValid)
        return;

    m_url = String(&url[0], url.length());
    for (const auto& param : params) {
        LinkParameterName name = parameterNameFromString(param.first);
        if (!isExtensionParameter(name) && !param.second)
            m_isValid = false;
        std::string value = param.second.value_or("");
        setValue(name, String(&value[0], value.length()));
    }
}

LinkHeaderSet::LinkHeaderSet(const String& header)
{
    if (header.isNull())
        return;

    DCHECK(header.is8Bit()) << "Headers should always be 8 bit";
    std::string headerString(reinterpret_cast<const char*>(header.characters8()), header.length());
    for (const auto& value : link_header_util::SplitLinkHeader(headerString))
        m_headerSet.append(LinkHeader(value.first, value.second));
}

} // namespace blink
