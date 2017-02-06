// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EventModulesFactory_h
#define EventModulesFactory_h

#include "core/events/EventFactory.h"
#include "platform/heap/Handle.h"
#include "wtf/PassRefPtr.h"
#include "wtf/PtrUtil.h"
#include "wtf/text/AtomicString.h"
#include <memory>

namespace blink {

class Event;

class EventModulesFactory final : public EventFactoryBase {
public:
    static std::unique_ptr<EventModulesFactory> create()
    {
        return wrapUnique(new EventModulesFactory());
    }

    Event* create(ExecutionContext*, const String& eventType) override;
};

} // namespace blink

#endif
