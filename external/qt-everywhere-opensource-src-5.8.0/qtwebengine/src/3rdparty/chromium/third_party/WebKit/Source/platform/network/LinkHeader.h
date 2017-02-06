// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LinkHeader_h
#define LinkHeader_h

#include "platform/PlatformExport.h"
#include "wtf/Allocator.h"
#include "wtf/text/WTFString.h"

namespace blink {

class LinkHeader {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
public:
    const String& url() const { return m_url; }
    const String& rel() const { return m_rel; }
    const String& as() const { return m_as; }
    const String& mimeType() const { return m_mimeType; }
    const String& media() const { return m_media; }
    const String& crossOrigin() const { return m_crossOrigin; }
    bool valid() const { return m_isValid; }

    enum LinkParameterName {
        LinkParameterRel,
        LinkParameterAnchor,
        LinkParameterTitle,
        LinkParameterMedia,
        LinkParameterType,
        LinkParameterRev,
        LinkParameterHreflang,
        // Beyond this point, only link-extension parameters
        LinkParameterUnknown,
        LinkParameterCrossOrigin,
        LinkParameterAs,
    };

private:
    friend class LinkHeaderSet;

    template <typename Iterator>
    LinkHeader(Iterator begin, Iterator end);
    void setValue(LinkParameterName, const String& value);

    String m_url;
    String m_rel;
    String m_as;
    String m_mimeType;
    String m_media;
    String m_crossOrigin;
    bool m_isValid;
};

class PLATFORM_EXPORT LinkHeaderSet {
    STACK_ALLOCATED();

public:
    LinkHeaderSet(const String& header);

    Vector<LinkHeader>::const_iterator begin() const { return m_headerSet.begin(); }
    Vector<LinkHeader>::const_iterator end() const { return m_headerSet.end(); }
    LinkHeader& operator[](size_t i) { return m_headerSet[i]; }
    size_t size() { return m_headerSet.size(); }

private:
    Vector<LinkHeader> m_headerSet;
};

} // namespace blink

#endif
