// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HostsUsingFeatures_h
#define HostsUsingFeatures_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "wtf/HashMap.h"
#include "wtf/Vector.h"
#include "wtf/text/StringHash.h"
#include "wtf/text/WTFString.h"

namespace blink {

class Document;
class EventTarget;
class ScriptState;

class CORE_EXPORT HostsUsingFeatures {
    DISALLOW_NEW();
public:
    ~HostsUsingFeatures();

    // Features for RAPPOR. Do not reorder or remove!
    enum class Feature {
        ElementCreateShadowRoot,
        DocumentRegisterElement,
        EventPath,
        DeviceMotionInsecureHost,
        DeviceOrientationInsecureHost,
        FullscreenInsecureHost,
        GeolocationInsecureHost,
        GetUserMediaInsecureHost,
        GetUserMediaSecureHost,
        ElementAttachShadow,
        ApplicationCacheManifestSelectInsecureHost,
        ApplicationCacheAPIInsecureHost,
        RTCPeerConnectionAudio,
        RTCPeerConnectionVideo,
        RTCPeerConnectionDataChannel,

        NumberOfFeatures // This must be the last item.
    };

    static void countAnyWorld(Document&, Feature);
    static void countMainWorldOnly(const ScriptState*, Document&, Feature);
    static void countHostOrIsolatedWorldHumanReadableName(const ScriptState*, EventTarget&, Feature);

    void documentDetached(Document&);
    void updateMeasurementsAndClear();

    class CORE_EXPORT Value {
        DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
    public:
        Value();

        bool isEmpty() const { return !m_countBits; }
        void clear() { m_countBits = 0; }

        void count(Feature);
        bool get(Feature feature) const { return m_countBits & (1 << static_cast<unsigned>(feature)); }

        void aggregate(Value);
        void recordHostToRappor(const String& host);
        void recordNameToRappor(const String& name);
        void recordETLDPlus1ToRappor(const KURL&);

    private:
        unsigned m_countBits : static_cast<unsigned>(Feature::NumberOfFeatures);
    };

    void countName(Feature, const String&);
    HashMap<String, Value>& valueByName() { return m_valueByName; }
    void clear();

private:
    void recordHostToRappor();
    void recordNamesToRappor();
    void recordETLDPlus1ToRappor();

    Vector<std::pair<KURL, HostsUsingFeatures::Value>, 1> m_urlAndValues;
    HashMap<String, HostsUsingFeatures::Value> m_valueByName;
};

} // namespace blink

#endif // HostsUsingFeatures_h
