// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BluetoothRemoteGATTServer_h
#define BluetoothRemoteGATTServer_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/modules/v8/StringOrUnsignedLong.h"
#include "modules/bluetooth/BluetoothDevice.h"
#include "platform/heap/Heap.h"
#include "third_party/WebKit/public/platform/modules/bluetooth/web_bluetooth.mojom-blink.h"
#include "wtf/text/WTFString.h"

namespace blink {

class BluetoothDevice;
class ScriptPromise;
class ScriptPromiseResolver;
class ScriptState;

// BluetoothRemoteGATTServer provides a way to interact with a connected
// bluetooth peripheral.
class BluetoothRemoteGATTServer final
    : public GarbageCollected<BluetoothRemoteGATTServer>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  BluetoothRemoteGATTServer(BluetoothDevice*);

  static BluetoothRemoteGATTServer* create(BluetoothDevice*);

  void setConnected(bool connected) { m_connected = connected; }

  // The Active Algorithms set is maintained so that disconnection, i.e.
  // disconnect() method or the device disconnecting by itself, can be detected
  // by algorithms. They check via RemoveFromActiveAlgorithms that their
  // resolvers is still in the set of active algorithms.
  //
  // Adds |resolver| to the set of Active Algorithms. CHECK-fails if
  // |resolver| was already added.
  void AddToActiveAlgorithms(ScriptPromiseResolver*);
  // Removes |resolver| from the set of Active Algorithms if it was in the set
  // and returns true, false otherwise.
  bool RemoveFromActiveAlgorithms(ScriptPromiseResolver*);
  // Removes all ScriptPromiseResolvers from the set of Active Algorithms.
  void ClearActiveAlgorithms() { m_activeAlgorithms.clear(); }

  // Interface required by Garbage Collectoin:
  DECLARE_VIRTUAL_TRACE();

  // IDL exposed interface:
  BluetoothDevice* device() { return m_device; }
  bool connected() { return m_connected; }
  ScriptPromise connect(ScriptState*);
  void disconnect(ScriptState*);
  ScriptPromise getPrimaryService(ScriptState*,
                                  const StringOrUnsignedLong& service,
                                  ExceptionState&);
  ScriptPromise getPrimaryServices(ScriptState*,
                                   const StringOrUnsignedLong& service,
                                   ExceptionState&);
  ScriptPromise getPrimaryServices(ScriptState*, ExceptionState&);

 private:
  ScriptPromise getPrimaryServicesImpl(
      ScriptState*,
      mojom::blink::WebBluetoothGATTQueryQuantity,
      String serviceUUID = String());

  // Contains a ScriptPromiseResolver corresponding to each active algorithm
  // using this server’s connection.
  HeapHashSet<Member<ScriptPromiseResolver>> m_activeAlgorithms;

  Member<BluetoothDevice> m_device;
  bool m_connected;
};

}  // namespace blink

#endif  // BluetoothDevice_h
