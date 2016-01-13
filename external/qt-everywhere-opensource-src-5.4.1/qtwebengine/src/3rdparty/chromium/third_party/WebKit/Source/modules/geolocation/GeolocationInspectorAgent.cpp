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

#include "config.h"
#include "modules/geolocation/GeolocationInspectorAgent.h"

#include "modules/geolocation/GeolocationController.h"

namespace WebCore {

PassOwnPtr<GeolocationInspectorAgent> GeolocationInspectorAgent::create()
{
    return adoptPtr(new GeolocationInspectorAgent());
}

GeolocationInspectorAgent::~GeolocationInspectorAgent()
{
}

GeolocationInspectorAgent::GeolocationInspectorAgent()
    : InspectorBaseAgent<GeolocationInspectorAgent>("Geolocation")
    , m_geolocationOverridden(false)
{
}

void GeolocationInspectorAgent::setGeolocationOverride(ErrorString* error, const double* latitude, const double* longitude, const double* accuracy)
{
    GeolocationPosition* position = (*m_controllers.begin())->lastPosition();
    if (!m_geolocationOverridden && position)
        m_platformGeolocationPosition = position;

    m_geolocationOverridden = true;
    if (latitude && longitude && accuracy)
        m_geolocationPosition = GeolocationPosition::create(currentTime(), *latitude, *longitude, *accuracy);
    else
        m_geolocationPosition.clear();

    for (GeolocationControllers::iterator it = m_controllers.begin(); it != m_controllers.end(); ++it)
        (*it)->positionChanged(0); // Kick location update.
}

void GeolocationInspectorAgent::clearGeolocationOverride(ErrorString*)
{
    if (!m_geolocationOverridden)
        return;
    m_geolocationOverridden = false;
    m_geolocationPosition.clear();

    if (GeolocationPosition* platformPosition = m_platformGeolocationPosition.get()) {
        for (GeolocationControllers::iterator it = m_controllers.begin(); it != m_controllers.end(); ++it)
            (*it)->positionChanged(platformPosition);
    }
}

GeolocationPosition* GeolocationInspectorAgent::overrideGeolocationPosition(GeolocationPosition* position)
{
    if (m_geolocationOverridden) {
        if (position)
            m_platformGeolocationPosition = position;
        return m_geolocationPosition.get();
    }
    return position;
}

void GeolocationInspectorAgent::AddController(GeolocationController* controller)
{
    m_controllers.add(controller);
}

void GeolocationInspectorAgent::RemoveController(GeolocationController* controller)
{
    m_controllers.remove(controller);
}

} // namespace WebCore
