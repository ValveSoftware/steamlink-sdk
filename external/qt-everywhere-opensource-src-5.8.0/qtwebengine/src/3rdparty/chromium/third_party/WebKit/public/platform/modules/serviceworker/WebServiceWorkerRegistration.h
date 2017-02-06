// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebServiceWorkerRegistration_h
#define WebServiceWorkerRegistration_h

#include "public/platform/WebCallbacks.h"
#include "public/platform/WebURL.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerError.h"

namespace blink {

class WebServiceWorkerProvider;
class WebServiceWorkerRegistrationProxy;

// The interface of the registration representation in the embedder. The
// embedder implements this interface and passes its handle
// (WebServiceWorkerRegistration::Handle) to Blink. Blink accesses the
// implementation via the handle to update or unregister the registration.
class WebServiceWorkerRegistration {
public:
    virtual ~WebServiceWorkerRegistration() { }

    using WebServiceWorkerUpdateCallbacks = WebCallbacks<void, const WebServiceWorkerError&>;
    using WebServiceWorkerUnregistrationCallbacks = WebCallbacks<bool, const WebServiceWorkerError&>;

    // The handle interface that retains a reference to the implementation of
    // WebServiceWorkerRegistration in the embedder and is owned by
    // ServiceWorkerRegistration object in Blink. The embedder must keep the
    // registration representation while Blink is owning this handle.
    class Handle {
    public:
        virtual ~Handle() { }
        virtual WebServiceWorkerRegistration* registration() { return nullptr; }
    };

    virtual void setProxy(WebServiceWorkerRegistrationProxy*) { }
    virtual WebServiceWorkerRegistrationProxy* proxy() { return nullptr; }
    virtual void proxyStopped() { }

    virtual WebURL scope() const { return WebURL(); }
    virtual void update(WebServiceWorkerProvider*, WebServiceWorkerUpdateCallbacks*) { }
    virtual void unregister(WebServiceWorkerProvider*, WebServiceWorkerUnregistrationCallbacks*) { }
};

} // namespace blink

#endif // WebServiceWorkerRegistration_h
