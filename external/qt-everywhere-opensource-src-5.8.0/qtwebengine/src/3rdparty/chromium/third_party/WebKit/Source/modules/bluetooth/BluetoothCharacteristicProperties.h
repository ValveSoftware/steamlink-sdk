// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BluetoothCharacteristicProperties_h
#define BluetoothCharacteristicProperties_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

// Each BluetoothRemoteGATTCharacteristic exposes its characteristic properties
// through a BluetoothCharacteristicProperties object. These properties express
// what operations are valid on the characteristic.
class BluetoothCharacteristicProperties final
    : public GarbageCollected<BluetoothCharacteristicProperties>
    , public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    static BluetoothCharacteristicProperties* create(uint32_t properties);

    bool broadcast() const;
    bool read() const;
    bool writeWithoutResponse() const;
    bool write() const;
    bool notify() const;
    bool indicate() const;
    bool authenticatedSignedWrites() const;
    bool reliableWrite() const;
    bool writableAuxiliaries() const;

    DEFINE_INLINE_TRACE() { }

private:
    explicit BluetoothCharacteristicProperties(uint32_t properties);

    enum Property {
        None = 0,
        Broadcast = 1 << 0,
        Read = 1 << 1,
        WriteWithoutResponse = 1 << 2,
        Write = 1 << 3,
        Notify = 1 << 4,
        Indicate = 1 << 5,
        AuthenticatedSignedWrites = 1 << 6,
        ExtendedProperties = 1 << 7, // Not used in class.
        ReliableWrite = 1 << 8,
        WritableAuxiliaries = 1 << 9,
    };

    uint32_t properties;
};

} // namespace blink

#endif // GamepadButton_h
