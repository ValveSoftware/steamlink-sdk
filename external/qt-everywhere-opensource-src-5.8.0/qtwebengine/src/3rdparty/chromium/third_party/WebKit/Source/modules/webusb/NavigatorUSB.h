// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NavigatorUSB_h
#define NavigatorUSB_h

#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"

namespace blink {

class Navigator;
class USB;

class NavigatorUSB final
    : public GarbageCollected<NavigatorUSB>
    , public Supplement<Navigator> {
    USING_GARBAGE_COLLECTED_MIXIN(NavigatorUSB);
public:
    // Gets, or creates, NavigatorUSB supplement on Navigator.
    // See platform/Supplementable.h
    static NavigatorUSB& from(Navigator&);

    static USB* usb(Navigator&);
    USB* usb();

    DECLARE_TRACE();

private:
    explicit NavigatorUSB(Navigator&);
    static const char* supplementName();

    Member<USB> m_usb;
};

} // namespace blink

#endif // NavigatorUSB_h
