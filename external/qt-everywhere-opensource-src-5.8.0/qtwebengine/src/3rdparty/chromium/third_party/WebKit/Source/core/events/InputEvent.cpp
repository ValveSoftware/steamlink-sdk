// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/events/InputEvent.h"

#include "core/dom/Range.h"
#include "core/events/EventDispatcher.h"
#include "public/platform/WebEditingCommandType.h"

namespace blink {

namespace {

const struct {
    InputEvent::InputType inputType;
    const char* stringName;
} kInputTypeStringNameMap[] = {
    { InputEvent::InputType::None, "" },
    { InputEvent::InputType::InsertText, "insertText" },
    { InputEvent::InputType::ReplaceContent, "replaceContent" },
    { InputEvent::InputType::DeleteContent, "deleteContent" },
    { InputEvent::InputType::DeleteComposedCharacter, "deleteComposedCharacter" },
    { InputEvent::InputType::Undo, "undo" },
    { InputEvent::InputType::Redo, "redo" },
};

static_assert(arraysize(kInputTypeStringNameMap) == static_cast<size_t>(InputEvent::InputType::NumberOfInputTypes),
    "must handle all InputEvent::InputType");

String convertInputTypeToString(InputEvent::InputType inputType)
{
    const auto& it = std::begin(kInputTypeStringNameMap) + static_cast<size_t>(inputType);
    if (it >= std::begin(kInputTypeStringNameMap) && it < std::end(kInputTypeStringNameMap))
        return AtomicString(it->stringName);
    return emptyString();
}

InputEvent::InputType convertStringToInputType(const String& stringName)
{
    // TODO(chongz): Use binary search if the map goes larger.
    for (const auto& entry : kInputTypeStringNameMap) {
        if (stringName == entry.stringName)
            return entry.inputType;
    }
    return InputEvent::InputType::None;
}

} // anonymous namespace

InputEvent::InputEvent()
{
}

InputEvent::InputEvent(const AtomicString& type, const InputEventInit& initializer)
    : UIEvent(type, initializer)
{
    // TODO(ojan): We should find a way to prevent conversion like String->enum->String just in order to use initializer.
    // See InputEvent::createBeforeInput() for the first conversion.
    if (initializer.hasInputType())
        m_inputType = convertStringToInputType(initializer.inputType());
    if (initializer.hasData())
        m_data = initializer.data();
    if (initializer.hasIsComposing())
        m_isComposing = initializer.isComposing();
    if (initializer.hasRanges())
        m_ranges = initializer.ranges();
}

/* static */
InputEvent* InputEvent::createBeforeInput(InputType inputType, const String& data, EventCancelable cancelable, EventIsComposing isComposing, const RangeVector* ranges)
{
    InputEventInit inputEventInit;

    inputEventInit.setBubbles(true);
    inputEventInit.setCancelable(cancelable == IsCancelable);
    // TODO(ojan): We should find a way to prevent conversion like String->enum->String just in order to use initializer.
    // See InputEvent::InputEvent() for the second conversion.
    inputEventInit.setInputType(convertInputTypeToString(inputType));
    inputEventInit.setData(data);
    inputEventInit.setIsComposing(isComposing == IsComposing);
    if (ranges)
        inputEventInit.setRanges(*ranges);

    return InputEvent::create(EventTypeNames::beforeinput, inputEventInit);
}

/* static */
InputEvent* InputEvent::createInput(InputType inputType, const String& data, EventIsComposing isComposing, const RangeVector* ranges)
{
    InputEventInit inputEventInit;

    inputEventInit.setBubbles(true);
    inputEventInit.setCancelable(false);
    // TODO(ojan): We should find a way to prevent conversion like String->enum->String just in order to use initializer.
    // See InputEvent::InputEvent() for the second conversion.
    inputEventInit.setInputType(convertInputTypeToString(inputType));
    inputEventInit.setData(data);
    inputEventInit.setIsComposing(isComposing == IsComposing);
    if (ranges)
        inputEventInit.setRanges(*ranges);

    return InputEvent::create(EventTypeNames::input, inputEventInit);
}

String InputEvent::inputType() const
{
    return convertInputTypeToString(m_inputType);
}

StaticRangeVector InputEvent::getRanges() const
{
    StaticRangeVector staticRanges;
    for (const auto& range : m_ranges)
        staticRanges.append(StaticRange::create(range->ownerDocument(), range->startContainer(), range->startOffset(), range->endContainer(), range->endOffset()));
    return staticRanges;
}

bool InputEvent::isInputEvent() const
{
    return true;
}

// TODO(chongz): We should get rid of this |EventDispatchMediator| pattern and introduce
// simpler interface such as |beforeDispatchEvent()| and |afterDispatchEvent()| virtual methods.
EventDispatchMediator* InputEvent::createMediator()
{
    return InputEventDispatchMediator::create(this);
}

DEFINE_TRACE(InputEvent)
{
    UIEvent::trace(visitor);
    visitor->trace(m_ranges);
}

InputEventDispatchMediator* InputEventDispatchMediator::create(InputEvent* inputEvent)
{
    return new InputEventDispatchMediator(inputEvent);
}

InputEventDispatchMediator::InputEventDispatchMediator(InputEvent* inputEvent)
    : EventDispatchMediator(inputEvent)
{
}

InputEvent& InputEventDispatchMediator::event() const
{
    return toInputEvent(EventDispatchMediator::event());
}

DispatchEventResult InputEventDispatchMediator::dispatchEvent(EventDispatcher& dispatcher) const
{
    DispatchEventResult result = dispatcher.dispatch();
    // It's weird to hold and clear live |Range| objects internally, and only expose |StaticRange|
    // through |getRanges()|. However there is no better solutions due to the following issues:
    //   1. We don't want to expose live |Range| objects for the author to hold as it will slow down
    //      all DOM operations. So we just expose |StaticRange|.
    //   2. Event handlers in chain might modify DOM, which means we have to keep a copy of live
    //      |Range| internally and return snapshots.
    //   3. We don't want authors to hold live |Range| indefinitely by holding |InputEvent|, so we
    //      clear them after dispatch.
    // Authors should explicitly call |getRanges()|->|toRange()| if they want to keep a copy of |Range|.
    // See Editing TF meeting notes:
    // https://docs.google.com/document/d/1hCj6QX77NYIVY0RWrMHT1Yra6t8_Qu8PopaWLG0AM58/edit?usp=sharing
    event().m_ranges.clear();
    return result;
}

} // namespace blink
