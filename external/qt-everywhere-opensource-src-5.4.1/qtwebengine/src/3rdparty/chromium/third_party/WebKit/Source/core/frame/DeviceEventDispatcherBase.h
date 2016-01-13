// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DeviceEventDispatcherBase_h
#define DeviceEventDispatcherBase_h

#include "wtf/Vector.h"

namespace WebCore {
class DeviceEventControllerBase;

class DeviceEventDispatcherBase {
public:
    void addController(DeviceEventControllerBase*);
    void removeController(DeviceEventControllerBase*);

protected:
    DeviceEventDispatcherBase();
    virtual ~DeviceEventDispatcherBase();

    void notifyControllers();

    virtual void startListening() = 0;
    virtual void stopListening() = 0;

private:
    void purgeControllers();

    Vector<DeviceEventControllerBase*> m_controllers;
    bool m_needsPurge;
    bool m_isDispatching;
};

} // namespace WebCore

#endif // DeviceEventDispatcherBase_h
