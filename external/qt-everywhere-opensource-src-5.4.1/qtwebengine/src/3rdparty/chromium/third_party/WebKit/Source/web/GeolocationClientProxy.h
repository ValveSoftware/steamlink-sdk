/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef GeolocationClientProxy_h
#define GeolocationClientProxy_h

#include "modules/geolocation/GeolocationClient.h"
#include "platform/heap/Handle.h"
#include "public/web/WebGeolocationController.h"

namespace WebCore {
class GeolocationPosition;
}

namespace blink {
class WebGeolocationClient;

class GeolocationClientProxy FINAL : public WebCore::GeolocationClient {
public:
    GeolocationClientProxy(WebGeolocationClient* client);
    virtual ~GeolocationClientProxy();
    void setController(WebCore::GeolocationController *controller);
    virtual void geolocationDestroyed() OVERRIDE;
    virtual void startUpdating() OVERRIDE;
    virtual void stopUpdating() OVERRIDE;
    virtual void setEnableHighAccuracy(bool) OVERRIDE;
    virtual WebCore::GeolocationPosition* lastPosition() OVERRIDE;

    virtual void requestPermission(WebCore::Geolocation*) OVERRIDE;
    virtual void cancelPermissionRequest(WebCore::Geolocation*) OVERRIDE;

private:
    WebGeolocationClient* m_client;
    WebCore::Persistent<WebCore::GeolocationPosition> m_lastPosition;
};

} // namespace blink

#endif // GeolocationClientProxy_h
