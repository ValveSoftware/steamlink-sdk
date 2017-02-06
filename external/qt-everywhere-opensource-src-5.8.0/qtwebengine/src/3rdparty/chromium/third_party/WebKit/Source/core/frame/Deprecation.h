// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Deprecation_h
#define Deprecation_h

#include "core/CSSPropertyNames.h"
#include "core/CoreExport.h"
#include "core/frame/UseCounter.h"
#include "wtf/BitVector.h"
#include "wtf/Noncopyable.h"

namespace blink {

class LocalFrame;

class CORE_EXPORT Deprecation {
    DISALLOW_NEW();
    WTF_MAKE_NONCOPYABLE(Deprecation);
public:
    Deprecation();
    ~Deprecation();

    static void warnOnDeprecatedProperties(const LocalFrame*, CSSPropertyID unresolvedProperty);
    void clearSuppression();

    // "countDeprecation" sets the bit for this feature to 1, and sends a deprecation
    // warning to the console. Repeated calls are ignored.
    //
    // Be considerate to developers' consoles: features should only send
    // deprecation warnings when we're actively interested in removing them from
    // the platform.
    //
    // For shared workers and service workers, the ExecutionContext* overload
    // doesn't count the usage but only sends a console warning.
    static void countDeprecation(const LocalFrame*, UseCounter::Feature);
    static void countDeprecation(ExecutionContext*, UseCounter::Feature);
    static void countDeprecation(const Document&, UseCounter::Feature);
    // Use countDeprecationIfNotPrivateScript() instead of countDeprecation()
    // if you don't want to count metrics in private scripts. You should use
    // countDeprecationIfNotPrivateScript() in a binding layer.
    static void countDeprecationIfNotPrivateScript(v8::Isolate*, ExecutionContext*, UseCounter::Feature);
    // Count only features if they're being used in an iframe which does not
    // have script access into the top level document.
    static void countDeprecationCrossOriginIframe(const LocalFrame*, UseCounter::Feature);
    static void countDeprecationCrossOriginIframe(const Document&, UseCounter::Feature);
    static String deprecationMessage(UseCounter::Feature);

protected:
    void suppress(CSSPropertyID unresolvedProperty);
    bool isSuppressed(CSSPropertyID unresolvedProperty);
    // CSSPropertyIDs that aren't deprecated return an empty string.
    static String deprecationMessage(CSSPropertyID unresolvedProperty);

    BitVector m_cssPropertyDeprecationBits;
};

} // namespace blink

#endif // Deprecation_h
