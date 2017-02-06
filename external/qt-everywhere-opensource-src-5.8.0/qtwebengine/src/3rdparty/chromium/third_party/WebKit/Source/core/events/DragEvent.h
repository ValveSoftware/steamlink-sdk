// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DragEvent_h
#define DragEvent_h

#include "core/CoreExport.h"
#include "core/events/DragEventInit.h"
#include "core/events/MouseEvent.h"

namespace blink {

class DataTransfer;

class CORE_EXPORT DragEvent final : public MouseEvent {
    DEFINE_WRAPPERTYPEINFO();

public:
    static DragEvent* create()
    {
        return new DragEvent;
    }

    static DragEvent* create(DataTransfer* dataTransfer)
    {
        return new DragEvent(dataTransfer);
    }

    static DragEvent* create(const AtomicString& type, bool canBubble, bool cancelable, AbstractView*,
        int detail, int screenX, int screenY, int windowX, int windowY,
        int movementX, int movementY,
        PlatformEvent::Modifiers, short button, unsigned short buttons,
        EventTarget* relatedTarget,
        double platformTimeStamp, DataTransfer*,
        PlatformMouseEvent::SyntheticEventType = PlatformMouseEvent::RealOrIndistinguishable);

    static DragEvent* create(const AtomicString& type, const DragEventInit& initializer)
    {
        return new DragEvent(type, initializer);
    }

    DataTransfer* getDataTransfer() const override { return isDragEvent() ? m_dataTransfer.get() : nullptr; }

    bool isDragEvent() const override;
    bool isMouseEvent() const override;

    EventDispatchMediator* createMediator() override;

    DECLARE_VIRTUAL_TRACE();

private:
    DragEvent();
    DragEvent(DataTransfer*);
    DragEvent(const AtomicString& type, bool canBubble, bool cancelable, AbstractView*,
        int detail, int screenX, int screenY, int windowX, int windowY,
        int movementX, int movementY,
        PlatformEvent::Modifiers, short button, unsigned short buttons,
        EventTarget* relatedTarget,
        double platformTimeStamp, DataTransfer*,
        PlatformMouseEvent::SyntheticEventType);

    DragEvent(const AtomicString& type, const DragEventInit&);

    Member<DataTransfer> m_dataTransfer;
};

class DragEventDispatchMediator final : public EventDispatchMediator {
public:
    static DragEventDispatchMediator* create(DragEvent*);

private:
    explicit DragEventDispatchMediator(DragEvent*);
    DragEvent& event() const;
    DispatchEventResult dispatchEvent(EventDispatcher&) const override;
};

DEFINE_EVENT_TYPE_CASTS(DragEvent);

} // namespace blink

#endif // DragEvent_h
