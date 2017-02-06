// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InputEvent_h
#define InputEvent_h

#include "core/dom/StaticRange.h"
#include "core/events/InputEventInit.h"
#include "core/events/UIEvent.h"

namespace blink {

class InputEvent final : public UIEvent {
    DEFINE_WRAPPERTYPEINFO();

public:
    static InputEvent* create()
    {
        return new InputEvent;
    }

    static InputEvent* create(const AtomicString& type, const InputEventInit& initializer)
    {
        return new InputEvent(type, initializer);
    }

    enum class InputType {
        None,
        InsertText,
        ReplaceContent,
        DeleteContent,
        DeleteComposedCharacter,
        Undo,
        Redo,

        // Add new input types immediately above this line.
        NumberOfInputTypes,
    };

    enum EventCancelable : bool {
        NotCancelable = false,
        IsCancelable = true,
    };

    enum EventIsComposing : bool {
        NotComposing = false,
        IsComposing = true,
    };

    static InputEvent* createBeforeInput(InputType, const String& data, EventCancelable, EventIsComposing, const RangeVector*);
    static InputEvent* createInput(InputType, const String& data, EventIsComposing, const RangeVector*);

    String inputType() const;
    const String& data() const { return m_data; }
    bool isComposing() const { return m_isComposing; }
    // Returns a copy of target ranges during event dispatch, and returns an empty
    // vector after dispatch.
    StaticRangeVector getRanges() const;

    bool isInputEvent() const override;

    EventDispatchMediator* createMediator() override;

    DECLARE_VIRTUAL_TRACE();

private:
    friend class InputEventDispatchMediator;
    InputEvent();
    InputEvent(const AtomicString&, const InputEventInit&);

    InputType m_inputType;
    String m_data;
    bool m_isComposing;
    // We have to stored |Range| internally and only expose |StaticRange|, please
    // see comments in |InputEventDispatchMediator::dispatchEvent()|.
    RangeVector m_ranges;
};

class InputEventDispatchMediator final : public EventDispatchMediator {
public:
    static InputEventDispatchMediator* create(InputEvent*);

private:
    explicit InputEventDispatchMediator(InputEvent*);
    InputEvent& event() const;
    DispatchEventResult dispatchEvent(EventDispatcher&) const override;
};

DEFINE_EVENT_TYPE_CASTS(InputEvent);

} // namespace blink

#endif // InputEvent_h
