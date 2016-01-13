// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PushEvent_h
#define PushEvent_h

#include "modules/EventModules.h"
#include "platform/heap/Handle.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

struct PushEventInit : public EventInit {
    PushEventInit();

    String data;
};

class PushEvent FINAL : public Event {
public:
    static PassRefPtrWillBeRawPtr<PushEvent> create()
    {
        return adoptRefWillBeNoop(new PushEvent);
    }
    static PassRefPtrWillBeRawPtr<PushEvent> create(const AtomicString& type, const String& data)
    {
        return adoptRefWillBeNoop(new PushEvent(type, data));
    }
    static PassRefPtrWillBeRawPtr<PushEvent> create(const AtomicString& type, const PushEventInit& initializer)
    {
        return adoptRefWillBeNoop(new PushEvent(type, initializer));
    }

    virtual ~PushEvent();

    virtual const AtomicString& interfaceName() const OVERRIDE;

    String data() const { return m_data; }

private:
    PushEvent();
    PushEvent(const AtomicString& type, const String& data);
    PushEvent(const AtomicString& type, const PushEventInit&);
    String m_data;
};

} // namespace WebCore

#endif // PushEvent_h
