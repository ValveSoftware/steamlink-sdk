// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BlinkGCInterruptor_h
#define BlinkGCInterruptor_h

#include "platform/PlatformExport.h"
#include "wtf/Allocator.h"

namespace blink {

// If attached thread enters long running loop that can call back
// into Blink and leaving and reentering safepoint at every
// transition between this loop and Blink is deemed too expensive
// then instead of marking this loop as a GC safepoint thread
// can provide an interruptor object which would allow GC
// to temporarily interrupt and pause this long running loop at
// an arbitrary moment creating a safepoint for a GC.
class PLATFORM_EXPORT BlinkGCInterruptor {
    USING_FAST_MALLOC(BlinkGCInterruptor);
public:
    virtual ~BlinkGCInterruptor() { }

    // Request the interruptor to interrupt the thread and
    // call onInterrupted on that thread once interruption
    // succeeds.
    virtual void requestInterrupt() = 0;

protected:
    // This method is called on the interrupted thread to
    // create a safepoint for a GC.
    void onInterrupted();
};

} // namespace blink

#endif
