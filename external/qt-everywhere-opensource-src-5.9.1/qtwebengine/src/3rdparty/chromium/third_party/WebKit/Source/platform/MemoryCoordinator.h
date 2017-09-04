// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MemoryCoordinator_h
#define MemoryCoordinator_h

#include "platform/PlatformExport.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebMemoryPressureLevel.h"
#include "public/platform/WebMemoryState.h"
#include "wtf/Noncopyable.h"

namespace blink {

class PLATFORM_EXPORT MemoryCoordinatorClient : public GarbageCollectedMixin {
 public:
  virtual ~MemoryCoordinatorClient() {}

  // TODO(bashi): Deprecating. Remove this when MemoryPressureListener is
  // gone.
  virtual void onMemoryPressure(WebMemoryPressureLevel) {}

  virtual void onMemoryStateChange(MemoryState) {}
};

// MemoryCoordinator listens to some events which could be opportunities
// for reducing memory consumption and notifies its clients.
class PLATFORM_EXPORT MemoryCoordinator final
    : public GarbageCollected<MemoryCoordinator> {
  WTF_MAKE_NONCOPYABLE(MemoryCoordinator);

 public:
  static MemoryCoordinator& instance();

  // Whether the device Blink runs on is a low-end device.
  // Can be overridden in layout tests via internals.
  static bool isLowEndDevice();

  // Caches whether this device is a low-end device in a static member.
  // instance() is not used as it's a heap allocated object - meaning it's not
  // thread-safe as well as might break tests counting the heap size.
  static void initialize();

  void registerClient(MemoryCoordinatorClient*);
  void unregisterClient(MemoryCoordinatorClient*);

  // TODO(bashi): Deprecating. Remove this when MemoryPressureListener is
  // gone.
  void onMemoryPressure(WebMemoryPressureLevel);

  void onMemoryStateChange(MemoryState);

  DECLARE_TRACE();

 private:
  friend class Internals;

  static void setIsLowEndDeviceForTesting(bool);

  MemoryCoordinator();

  void clearMemory();

  static bool s_isLowEndDevice;

  HeapHashSet<WeakMember<MemoryCoordinatorClient>> m_clients;
};

}  // namespace blink

#endif  // MemoryCoordinator_h
