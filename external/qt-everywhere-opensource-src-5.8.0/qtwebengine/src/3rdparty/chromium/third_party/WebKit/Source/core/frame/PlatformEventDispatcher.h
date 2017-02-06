// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PlatformEventDispatcher_h
#define PlatformEventDispatcher_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "wtf/Vector.h"

namespace blink {
class PlatformEventController;

class CORE_EXPORT PlatformEventDispatcher : public GarbageCollectedMixin {
public:
    void addController(PlatformEventController*);
    void removeController(PlatformEventController*);

    DECLARE_VIRTUAL_TRACE();

protected:
    PlatformEventDispatcher();

    void notifyControllers();

    virtual void startListening() = 0;
    virtual void stopListening() = 0;

private:
    void purgeControllers();

    HeapHashSet<WeakMember<PlatformEventController>> m_controllers;
    bool m_isDispatching;
    bool m_isListening;
};

} // namespace blink

#endif // PlatformEventDispatcher_h
