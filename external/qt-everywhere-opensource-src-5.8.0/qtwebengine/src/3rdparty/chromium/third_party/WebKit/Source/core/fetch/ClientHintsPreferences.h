// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ClientHintsPreferences_h
#define ClientHintsPreferences_h

#include "core/CoreExport.h"
#include "wtf/Allocator.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ResourceFetcher;

class CORE_EXPORT ClientHintsPreferences {
    DISALLOW_NEW();
public:
    ClientHintsPreferences();

    void updateFrom(const ClientHintsPreferences&);
    void updateFromAcceptClientHintsHeader(const String& headerValue, ResourceFetcher*);

    bool shouldSendDPR() const { return m_shouldSendDPR; }
    void setShouldSendDPR(bool should) { m_shouldSendDPR = should; }

    bool shouldSendResourceWidth() const { return m_shouldSendResourceWidth; }
    void setShouldSendResourceWidth(bool should) { m_shouldSendResourceWidth = should; }

    bool shouldSendViewportWidth() const { return m_shouldSendViewportWidth; }
    void setShouldSendViewportWidth(bool should) { m_shouldSendViewportWidth = should; }

private:
    bool m_shouldSendDPR;
    bool m_shouldSendResourceWidth;
    bool m_shouldSendViewportWidth;
};

} // namespace blink

#endif
