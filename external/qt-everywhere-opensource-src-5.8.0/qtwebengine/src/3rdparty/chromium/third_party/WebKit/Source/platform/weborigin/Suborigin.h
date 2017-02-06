// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Suborigin_h
#define Suborigin_h

#include "platform/PlatformExport.h"
#include "wtf/Noncopyable.h"
#include "wtf/text/WTFString.h"

namespace blink {

class PLATFORM_EXPORT Suborigin {
public:
    enum class SuboriginPolicyOptions : unsigned {
        None = 0,
        UnsafePostMessageSend = 1 << 0,
        UnsafePostMessageReceive = 1 << 1,
        UnsafeCookies = 1 << 2
    };

    Suborigin();
    explicit Suborigin(const Suborigin*);

    void setTo(const Suborigin&);
    String name() const { return m_name; }
    void setName(const String& name) { m_name = name; }
    void addPolicyOption(SuboriginPolicyOptions);
    bool policyContains(SuboriginPolicyOptions) const;
    void clear();
    // For testing
    unsigned optionsMask() const { return m_optionsMask; }

private:
    String m_name;
    unsigned m_optionsMask;
};

} // namespace blink

#endif // Suborigin_h
