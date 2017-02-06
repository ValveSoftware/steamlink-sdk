// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_pressure/memory_pressure_listener.h"

#include "base/lazy_instance.h"
#include "base/observer_list_threadsafe.h"
#include "base/trace_event/trace_event.h"

namespace memory_pressure {

namespace {

// ObserverListThreadSafe is RefCountedThreadSafe, this traits is needed
// to ensure the LazyInstance will hold a reference to it.
struct LeakyLazyObserverListTraits
    : base::internal::LeakyLazyInstanceTraits<
          base::ObserverListThreadSafe<MemoryPressureListener>> {
  static base::ObserverListThreadSafe<MemoryPressureListener>* New(
      void* instance) {
    base::ObserverListThreadSafe<MemoryPressureListener>* ret =
        base::internal::LeakyLazyInstanceTraits<base::ObserverListThreadSafe<
            MemoryPressureListener>>::New(instance);
    // Leaky.
    ret->AddRef();
    return ret;
  }
};

base::LazyInstance<base::ObserverListThreadSafe<MemoryPressureListener>,
                   LeakyLazyObserverListTraits> g_observers =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

MemoryPressureListener::MemoryPressureListener(
    const MemoryPressureListener::MemoryPressureCallback& callback)
    : callback_(callback) {
  g_observers.Get().AddObserver(this);
}

MemoryPressureListener::~MemoryPressureListener() {
  g_observers.Get().RemoveObserver(this);
}

void MemoryPressureListener::Notify(MemoryPressureLevel memory_pressure_level) {
  callback_.Run(memory_pressure_level);
}

// static
void MemoryPressureListener::NotifyMemoryPressure(
    MemoryPressureLevel memory_pressure_level) {
  DCHECK_NE(memory_pressure_level, MEMORY_PRESSURE_LEVEL_NONE);
  TRACE_EVENT1("memory", "MemoryPressureListener::NotifyMemoryPressure",
               "level", memory_pressure_level);
  g_observers.Get().Notify(FROM_HERE, &MemoryPressureListener::Notify,
                           memory_pressure_level);
}

}  // namespace memory_pressure
