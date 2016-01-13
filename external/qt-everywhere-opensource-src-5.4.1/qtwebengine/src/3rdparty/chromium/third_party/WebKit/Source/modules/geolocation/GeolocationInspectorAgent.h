/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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

#ifndef GeolocationInspectorAgent_h
#define GeolocationInspectorAgent_h


#include "core/inspector/InspectorBaseAgent.h"
#include "modules/geolocation/GeolocationPosition.h"
#include "wtf/HashSet.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class GeolocationController;

typedef String ErrorString;

class GeolocationInspectorAgent FINAL : public InspectorBaseAgent<GeolocationInspectorAgent>, public InspectorBackendDispatcher::GeolocationCommandHandler {
    WTF_MAKE_NONCOPYABLE(GeolocationInspectorAgent);
public:
    static PassOwnPtr<GeolocationInspectorAgent> create();
    virtual ~GeolocationInspectorAgent();

    // Protocol methods.
    virtual void setGeolocationOverride(ErrorString*, const double*, const double*, const double*) OVERRIDE;
    virtual void clearGeolocationOverride(ErrorString*) OVERRIDE;

    // Instrumentation method.
    GeolocationPosition* overrideGeolocationPosition(GeolocationPosition*);

    void AddController(GeolocationController*);
    void RemoveController(GeolocationController*);

private:
    GeolocationInspectorAgent();
    typedef WillBeHeapHashSet<RawPtrWillBeMember<GeolocationController> > GeolocationControllers;
    WillBePersistentHeapHashSet<RawPtrWillBeMember<GeolocationController> > m_controllers;
    bool m_geolocationOverridden;
    Persistent<GeolocationPosition> m_geolocationPosition;
    Persistent<GeolocationPosition> m_platformGeolocationPosition;
};


} // namespace WebCore


#endif // !defined(GeolocationInspectorAgent_h)
