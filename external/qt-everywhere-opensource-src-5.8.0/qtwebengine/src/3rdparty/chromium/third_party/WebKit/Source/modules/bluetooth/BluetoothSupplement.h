// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BluetoothSupplement_h
#define BluetoothSupplement_h

#include "core/frame/LocalFrame.h"

namespace blink {

class WebBluetooth;

// This class is attached to a LocalFrame in WebLocalFrameImpl::setCoreFrame, to
// pass WebFrameClient::bluetooth() (accessible by web/ only) to the Bluetooth
// code in modules/.
class BLINK_EXPORT BluetoothSupplement : public GarbageCollected<BluetoothSupplement>, public Supplement<LocalFrame> {
    WTF_MAKE_NONCOPYABLE(BluetoothSupplement);
    USING_GARBAGE_COLLECTED_MIXIN(BluetoothSupplement);

public:
    static const char* supplementName();

    static void provideTo(LocalFrame&, WebBluetooth*);

    // Returns the WebBluetooth attached to the frame.
    static WebBluetooth* from(LocalFrame*);

    // Returns the WebBluetooth attached to the frame if the frame exists.
    // Otherwise returns nullptr.
    static WebBluetooth* fromScriptState(ScriptState*);
    // Returns the WebBluetooth attached to the execution context.
    static WebBluetooth* fromExecutionContext(ExecutionContext*);

    DECLARE_VIRTUAL_TRACE();

private:
    explicit BluetoothSupplement(WebBluetooth*);

    WebBluetooth* m_bluetooth;
};

} // namespace blink

#endif // BluetoothRoutingId_h
