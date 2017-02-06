// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InputDeviceCapabilities_h
#define InputDeviceCapabilities_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
#include "core/input/InputDeviceCapabilitiesInit.h"

namespace blink {

class CORE_EXPORT InputDeviceCapabilities final : public GarbageCollected<InputDeviceCapabilities>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    // This return a static local InputDeviceCapabilities pointer which has firesTouchEvents set to be true.
    static InputDeviceCapabilities* firesTouchEventsSourceCapabilities();

    // This return a static local InputDeviceCapabilities pointer which has firesTouchEvents set to be false.
    static InputDeviceCapabilities* doesntFireTouchEventsSourceCapabilities();

    static InputDeviceCapabilities* create(bool firesTouchEvents)
    {
        return new InputDeviceCapabilities(firesTouchEvents);
    }

    static InputDeviceCapabilities* create(
        const InputDeviceCapabilitiesInit& initializer)
    {
        return new InputDeviceCapabilities(initializer);
    }

    bool firesTouchEvents() const { return m_firesTouchEvents; }

    DEFINE_INLINE_TRACE() { }

private:
    InputDeviceCapabilities(bool firesTouchEvents);
    InputDeviceCapabilities(const InputDeviceCapabilitiesInit&);

    // Whether this device dispatches touch events. This mainly lets developers
    // avoid handling both touch and mouse events dispatched for a single user
    // action.
    bool m_firesTouchEvents;
};

} // namespace blink

#endif // InputDeviceCapabilities_h
