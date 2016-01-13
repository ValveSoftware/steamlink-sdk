// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InitModules_h
#define InitModules_h

#include "core/Init.h"
#include "platform/heap/Handle.h"

namespace WebCore {

class Event;
class ExceptionState;

class ModulesInitializer : public CoreInitializer {
public:
    virtual void registerEventFactory() OVERRIDE;
    virtual void initEventNames() OVERRIDE;
    virtual void initEventTargetNames() OVERRIDE;
};

PassRefPtrWillBeRawPtr<Event> createEventModules(const String& eventType, ExceptionState&);

} // namespace WebCore

#endif // InitModules_h
