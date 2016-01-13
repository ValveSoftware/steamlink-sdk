// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DeviceEventControllerBase_h
#define DeviceEventControllerBase_h

#include "core/page/PageLifecycleObserver.h"
#include "platform/Timer.h"

namespace WebCore {

// Base controller class for registering controllers with a dispatcher.
// It watches page visibility and calls stopUpdating when page is not visible.
// It provides a didUpdateData() callback method which is called when new data
// it available.
class DeviceEventControllerBase : public PageLifecycleObserver {
public:
    void startUpdating();
    void stopUpdating();

    // This is called when new data becomes available.
    virtual void didUpdateData() = 0;

protected:
    explicit DeviceEventControllerBase(Page*);
    virtual ~DeviceEventControllerBase();

    virtual void registerWithDispatcher() = 0;
    virtual void unregisterWithDispatcher() = 0;

    // When true initiates a one-shot didUpdateData() when startUpdating() is called.
    virtual bool hasLastData() = 0;

    bool m_hasEventListener;

private:
    // Inherited from PageLifecycleObserver.
    virtual void pageVisibilityChanged() OVERRIDE FINAL;

    void oneShotCallback(Timer<DeviceEventControllerBase>*);

    bool m_isActive;
    Timer<DeviceEventControllerBase> m_timer;
};

} // namespace WebCore

#endif // DeviceEventControllerBase_h
