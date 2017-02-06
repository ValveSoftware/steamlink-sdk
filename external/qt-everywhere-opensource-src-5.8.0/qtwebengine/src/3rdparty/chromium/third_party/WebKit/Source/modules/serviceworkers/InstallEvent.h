// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InstallEvent_h
#define InstallEvent_h

#include "modules/EventModules.h"
#include "modules/ModulesExport.h"
#include "modules/serviceworkers/ExtendableEvent.h"
#include "modules/serviceworkers/ForeignFetchOptions.h"

namespace blink {

class USVStringOrUSVStringSequence;

class MODULES_EXPORT InstallEvent : public ExtendableEvent {
    DEFINE_WRAPPERTYPEINFO();

public:
    static InstallEvent* create();
    static InstallEvent* create(const AtomicString& type, const ExtendableEventInit&);
    static InstallEvent* create(const AtomicString& type, const ExtendableEventInit&, WaitUntilObserver*);

    ~InstallEvent() override;

    void registerForeignFetch(ExecutionContext*, const ForeignFetchOptions&, ExceptionState&);

    const AtomicString& interfaceName() const override;

protected:
    InstallEvent();
    InstallEvent(const AtomicString& type, const ExtendableEventInit&);
    InstallEvent(const AtomicString& type, const ExtendableEventInit&, WaitUntilObserver*);
};

} // namespace blink

#endif // InstallEvent_h
