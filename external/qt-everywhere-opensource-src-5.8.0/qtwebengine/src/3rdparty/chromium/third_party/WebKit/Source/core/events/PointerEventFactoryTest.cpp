// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/events/PointerEventFactory.h"

#include "core/frame/FrameView.h"
#include "core/page/Page.h"
#include "public/platform/WebPointerProperties.h"
#include <climits>
#include <gtest/gtest.h>

namespace blink {

class PointerEventFactoryTest : public ::testing::Test {
protected:
    void SetUp() override;
    PointerEvent* createAndCheckTouchCancel(
        WebPointerProperties::PointerType, int rawId,
        int uniqueId, bool isPrimary);
    PointerEvent* createAndCheckTouchEvent(
        WebPointerProperties::PointerType, int rawId,
        int uniqueId, bool isPrimary,
        PlatformTouchPoint::TouchState = PlatformTouchPoint::TouchPressed);
    PointerEvent* createAndCheckMouseEvent(
        WebPointerProperties::PointerType, int rawId,
        int uniqueId, bool isPrimary,
        PlatformEvent::Modifiers = PlatformEvent::NoModifiers);
    void createAndCheckPointerTransitionEvent(
        PointerEvent*,
        const AtomicString&);

    PointerEventFactory m_pointerEventFactory;
    unsigned m_expectedMouseId;
    unsigned m_mappedIdStart;

    class PlatformTouchPointBuilder : public PlatformTouchPoint {
    public:
        PlatformTouchPointBuilder(WebPointerProperties::PointerType, int,
            PlatformTouchPoint::TouchState);
    };

    class PlatformMouseEventBuilder : public PlatformMouseEvent {
    public:
        PlatformMouseEventBuilder(WebPointerProperties::PointerType, int,
            PlatformEvent::Modifiers);
    };
};

void PointerEventFactoryTest::SetUp()
{
    m_expectedMouseId = 1;
    m_mappedIdStart = 2;
}

PointerEventFactoryTest::PlatformTouchPointBuilder::PlatformTouchPointBuilder(
    WebPointerProperties::PointerType pointerType, int id,
    PlatformTouchPoint::TouchState state)
{
    m_pointerProperties.id = id;
    m_pointerProperties.pointerType = pointerType;
    m_pointerProperties.force = 1.0;
    m_state = state;
}

PointerEventFactoryTest::PlatformMouseEventBuilder::PlatformMouseEventBuilder(
    WebPointerProperties::PointerType pointerType, int id,
    PlatformEvent::Modifiers modifiers)
{
    m_pointerProperties.pointerType = pointerType;
    m_pointerProperties.id = id;
    m_modifiers = modifiers;
}

PointerEvent* PointerEventFactoryTest::createAndCheckTouchCancel(
    WebPointerProperties::PointerType pointerType, int rawId,
    int uniqueId, bool isPrimary)
{
    PointerEvent* pointerEvent = m_pointerEventFactory.createPointerCancelEvent(
        uniqueId, pointerType);
    EXPECT_EQ(uniqueId, pointerEvent->pointerId());
    EXPECT_EQ(isPrimary, pointerEvent->isPrimary());
    return pointerEvent;
}

void PointerEventFactoryTest::createAndCheckPointerTransitionEvent(
    PointerEvent* pointerEvent,
    const AtomicString& type)
{
    PointerEvent* clonePointerEvent = m_pointerEventFactory.
        createPointerBoundaryEvent(pointerEvent, type, nullptr);
    EXPECT_EQ(clonePointerEvent->pointerType(), pointerEvent->pointerType());
    EXPECT_EQ(clonePointerEvent->pointerId(), pointerEvent->pointerId());
    EXPECT_EQ(clonePointerEvent->isPrimary(), pointerEvent->isPrimary());
    EXPECT_EQ(clonePointerEvent->type(), type);
}

PointerEvent* PointerEventFactoryTest::createAndCheckTouchEvent(
    WebPointerProperties::PointerType pointerType,
    int rawId, int uniqueId, bool isPrimary,
    PlatformTouchPoint::TouchState state)
{
    PointerEvent* pointerEvent = m_pointerEventFactory.create(
        EventTypeNames::pointerdown, PointerEventFactoryTest::PlatformTouchPointBuilder(pointerType, rawId, state), PlatformEvent::NoModifiers, FloatSize(), FloatPoint(), nullptr);
    EXPECT_EQ(uniqueId, pointerEvent->pointerId());
    EXPECT_EQ(isPrimary, pointerEvent->isPrimary());
    return pointerEvent;
}

PointerEvent* PointerEventFactoryTest::createAndCheckMouseEvent(
    WebPointerProperties::PointerType pointerType,
    int rawId, int uniqueId, bool isPrimary,
    PlatformEvent::Modifiers modifiers)
{
    PointerEvent* pointerEvent = m_pointerEventFactory.create(
        EventTypeNames::mousedown, PlatformMouseEventBuilder(pointerType, rawId, modifiers), nullptr, nullptr);
    EXPECT_EQ(uniqueId, pointerEvent->pointerId());
    EXPECT_EQ(isPrimary, pointerEvent->isPrimary());
    return pointerEvent;
}

TEST_F(PointerEventFactoryTest, MousePointer)
{
    EXPECT_TRUE(m_pointerEventFactory.isActive(m_expectedMouseId));
    EXPECT_FALSE(m_pointerEventFactory.isActiveButtonsState(m_expectedMouseId));

    PointerEvent* pointerEvent1 = createAndCheckMouseEvent(WebPointerProperties::PointerType::Mouse, 0, m_expectedMouseId, true);
    PointerEvent* pointerEvent2 = createAndCheckMouseEvent(WebPointerProperties::PointerType::Mouse, 0, m_expectedMouseId, true, PlatformEvent::LeftButtonDown);

    createAndCheckPointerTransitionEvent(pointerEvent1, EventTypeNames::pointerout);

    EXPECT_TRUE(m_pointerEventFactory.isActive(m_expectedMouseId));
    EXPECT_TRUE(m_pointerEventFactory.isActiveButtonsState(m_expectedMouseId));

    m_pointerEventFactory.remove(pointerEvent1->pointerId());

    EXPECT_TRUE(m_pointerEventFactory.isActive(m_expectedMouseId));
    EXPECT_TRUE(m_pointerEventFactory.isActiveButtonsState(m_expectedMouseId));

    createAndCheckMouseEvent(WebPointerProperties::PointerType::Mouse, 0, m_expectedMouseId, true);

    EXPECT_TRUE(m_pointerEventFactory.isActive(m_expectedMouseId));
    EXPECT_FALSE(m_pointerEventFactory.isActiveButtonsState(m_expectedMouseId));

    m_pointerEventFactory.remove(pointerEvent1->pointerId());
    m_pointerEventFactory.remove(pointerEvent2->pointerId());

    createAndCheckMouseEvent(WebPointerProperties::PointerType::Mouse, 1, m_expectedMouseId, true);
    createAndCheckMouseEvent(WebPointerProperties::PointerType::Mouse, 20, m_expectedMouseId, true);

}

TEST_F(PointerEventFactoryTest, TouchPointerPrimaryRemovedWhileAnotherIsThere)
{
    PointerEvent* pointerEvent1 = createAndCheckTouchEvent(WebPointerProperties::PointerType::Touch, 0, m_mappedIdStart, true);
    createAndCheckTouchEvent(WebPointerProperties::PointerType::Touch, 1, m_mappedIdStart+1, false);

    m_pointerEventFactory.remove(pointerEvent1->pointerId());

    createAndCheckTouchEvent(WebPointerProperties::PointerType::Touch, 2, m_mappedIdStart+2, false);
    createAndCheckTouchEvent(WebPointerProperties::PointerType::Touch, 1, m_mappedIdStart+1, false);
}

TEST_F(PointerEventFactoryTest, TouchPointerReleasedAndPressedAgain)
{
    EXPECT_FALSE(m_pointerEventFactory.isActive(m_mappedIdStart));
    EXPECT_FALSE(m_pointerEventFactory.isActive(m_mappedIdStart+1));
    EXPECT_FALSE(m_pointerEventFactory.isActiveButtonsState(m_mappedIdStart));
    EXPECT_FALSE(m_pointerEventFactory.isActiveButtonsState(m_mappedIdStart+1));

    PointerEvent* pointerEvent1 = createAndCheckTouchEvent(WebPointerProperties::PointerType::Touch, 0, m_mappedIdStart, true);
    PointerEvent* pointerEvent2 = createAndCheckTouchEvent(WebPointerProperties::PointerType::Touch, 1, m_mappedIdStart+1, false);

    createAndCheckPointerTransitionEvent(pointerEvent1, EventTypeNames::pointerleave);
    createAndCheckPointerTransitionEvent(pointerEvent2, EventTypeNames::pointerenter);

    EXPECT_TRUE(m_pointerEventFactory.isActive(m_mappedIdStart));
    EXPECT_TRUE(m_pointerEventFactory.isActive(m_mappedIdStart+1));
    EXPECT_TRUE(m_pointerEventFactory.isActiveButtonsState(m_mappedIdStart));
    EXPECT_TRUE(m_pointerEventFactory.isActiveButtonsState(m_mappedIdStart+1));

    m_pointerEventFactory.remove(pointerEvent1->pointerId());
    m_pointerEventFactory.remove(pointerEvent2->pointerId());

    EXPECT_FALSE(m_pointerEventFactory.isActive(m_mappedIdStart));
    EXPECT_FALSE(m_pointerEventFactory.isActive(m_mappedIdStart+1));
    EXPECT_FALSE(m_pointerEventFactory.isActiveButtonsState(m_mappedIdStart));
    EXPECT_FALSE(m_pointerEventFactory.isActiveButtonsState(m_mappedIdStart+1));

    createAndCheckTouchEvent(WebPointerProperties::PointerType::Touch, 1, m_mappedIdStart+2, true);
    createAndCheckTouchEvent(WebPointerProperties::PointerType::Touch, 0, m_mappedIdStart+3, false);

    m_pointerEventFactory.clear();

    createAndCheckTouchEvent(WebPointerProperties::PointerType::Touch, 10, m_mappedIdStart, true);
}

TEST_F(PointerEventFactoryTest, TouchAndDrag)
{
    EXPECT_FALSE(m_pointerEventFactory.isActive(m_mappedIdStart));
    EXPECT_FALSE(m_pointerEventFactory.isActiveButtonsState(m_mappedIdStart));

    PointerEvent* pointerEvent1 = createAndCheckTouchEvent(WebPointerProperties::PointerType::Touch, 0, m_mappedIdStart, true);
    PointerEvent* pointerEvent2 = createAndCheckTouchEvent(WebPointerProperties::PointerType::Touch, 0, m_mappedIdStart, true);

    EXPECT_TRUE(m_pointerEventFactory.isActive(m_mappedIdStart));
    EXPECT_TRUE(m_pointerEventFactory.isActiveButtonsState(m_mappedIdStart));

    createAndCheckTouchEvent(WebPointerProperties::PointerType::Touch, 0, m_mappedIdStart, true, PlatformTouchPoint::TouchReleased);

    EXPECT_TRUE(m_pointerEventFactory.isActive(m_mappedIdStart));
    EXPECT_FALSE(m_pointerEventFactory.isActiveButtonsState(m_mappedIdStart));

    m_pointerEventFactory.remove(pointerEvent1->pointerId());
    m_pointerEventFactory.remove(pointerEvent2->pointerId());

    EXPECT_FALSE(m_pointerEventFactory.isActive(m_mappedIdStart));
    EXPECT_FALSE(m_pointerEventFactory.isActiveButtonsState(m_mappedIdStart));


    EXPECT_FALSE(m_pointerEventFactory.isActive(m_mappedIdStart+1));
    EXPECT_FALSE(m_pointerEventFactory.isActiveButtonsState(m_mappedIdStart+1));

    createAndCheckTouchEvent(WebPointerProperties::PointerType::Touch, 0, m_mappedIdStart+1, true);
    createAndCheckTouchEvent(WebPointerProperties::PointerType::Touch, 0, m_mappedIdStart+1, true);

    // Remove an obsolete (i.e. already removed) pointer event which should have no effect
    m_pointerEventFactory.remove(pointerEvent1->pointerId());

    EXPECT_TRUE(m_pointerEventFactory.isActive(m_mappedIdStart+1));
    EXPECT_TRUE(m_pointerEventFactory.isActiveButtonsState(m_mappedIdStart+1));

    createAndCheckTouchEvent(WebPointerProperties::PointerType::Touch, 0, m_mappedIdStart+1, true);
    createAndCheckTouchCancel(WebPointerProperties::PointerType::Touch, 0, m_mappedIdStart+1, true);

    EXPECT_TRUE(m_pointerEventFactory.isActive(m_mappedIdStart+1));
    EXPECT_FALSE(m_pointerEventFactory.isActiveButtonsState(m_mappedIdStart+1));

    createAndCheckTouchEvent(WebPointerProperties::PointerType::Touch, 0, m_mappedIdStart+1, true);

    EXPECT_TRUE(m_pointerEventFactory.isActive(m_mappedIdStart+1));
    EXPECT_TRUE(m_pointerEventFactory.isActiveButtonsState(m_mappedIdStart+1));
}

TEST_F(PointerEventFactoryTest, MouseAndTouchAndPen)
{
    createAndCheckMouseEvent(WebPointerProperties::PointerType::Mouse, 0, m_expectedMouseId, true);
    PointerEvent* pointerEvent1 = createAndCheckTouchEvent(WebPointerProperties::PointerType::Touch, 0, m_mappedIdStart, true);
    createAndCheckTouchEvent(WebPointerProperties::PointerType::Pen, 0, m_mappedIdStart+1, true);

    PointerEvent* pointerEvent2 = createAndCheckTouchEvent(WebPointerProperties::PointerType::Touch, 1, m_mappedIdStart+2, false);
    PointerEvent* pointerEvent3 = createAndCheckTouchEvent(WebPointerProperties::PointerType::Touch, 2, m_mappedIdStart+3, false);
    createAndCheckTouchEvent(WebPointerProperties::PointerType::Pen, 0, m_mappedIdStart+1, true);
    createAndCheckTouchEvent(WebPointerProperties::PointerType::Pen, 47213, m_mappedIdStart+4, false);

    m_pointerEventFactory.remove(pointerEvent1->pointerId());
    m_pointerEventFactory.remove(pointerEvent2->pointerId());
    m_pointerEventFactory.remove(pointerEvent3->pointerId());

    createAndCheckTouchEvent(WebPointerProperties::PointerType::Touch, 100, m_mappedIdStart+5, true);

    m_pointerEventFactory.clear();

    createAndCheckMouseEvent(WebPointerProperties::PointerType::Mouse, 0, m_expectedMouseId, true);
    createAndCheckTouchEvent(WebPointerProperties::PointerType::Touch, 0, m_mappedIdStart, true);
    createAndCheckTouchEvent(WebPointerProperties::PointerType::Pen, 0, m_mappedIdStart+1, true);
}

TEST_F(PointerEventFactoryTest, PenAsTouchAndMouseEvent)
{
    PointerEvent* pointerEvent1 = createAndCheckMouseEvent(WebPointerProperties::PointerType::Pen, 0, m_mappedIdStart, true);
    createAndCheckMouseEvent(WebPointerProperties::PointerType::Pen, 1, m_mappedIdStart+1, false);
    createAndCheckMouseEvent(WebPointerProperties::PointerType::Pen, 2, m_mappedIdStart+2, false);
    createAndCheckMouseEvent(WebPointerProperties::PointerType::Pen, 0, m_mappedIdStart, true);
    createAndCheckMouseEvent(WebPointerProperties::PointerType::Pen, 1, m_mappedIdStart+1, false);
    createAndCheckTouchEvent(WebPointerProperties::PointerType::Pen, 0, m_mappedIdStart, true);
    createAndCheckTouchEvent(WebPointerProperties::PointerType::Pen, 1, m_mappedIdStart+1, false);

    m_pointerEventFactory.remove(pointerEvent1->pointerId());

    createAndCheckTouchEvent(WebPointerProperties::PointerType::Pen, 0, m_mappedIdStart+3, false);
    createAndCheckMouseEvent(WebPointerProperties::PointerType::Pen, 0, m_mappedIdStart+3, false);
    createAndCheckTouchCancel(WebPointerProperties::PointerType::Pen, 0, m_mappedIdStart+3, false);

    m_pointerEventFactory.clear();

    createAndCheckTouchEvent(WebPointerProperties::PointerType::Pen, 1, m_mappedIdStart, true);
    createAndCheckTouchEvent(WebPointerProperties::PointerType::Pen, 0, m_mappedIdStart+1, false);
    createAndCheckMouseEvent(WebPointerProperties::PointerType::Pen, 1, m_mappedIdStart, true);
    createAndCheckMouseEvent(WebPointerProperties::PointerType::Pen, 0, m_mappedIdStart+1, false);
    createAndCheckTouchCancel(WebPointerProperties::PointerType::Pen, 1, m_mappedIdStart, true);
    createAndCheckTouchCancel(WebPointerProperties::PointerType::Pen, 0, m_mappedIdStart+1, false);
}


TEST_F(PointerEventFactoryTest, OutOfRange)
{
    PointerEvent* pointerEvent1 = createAndCheckMouseEvent(WebPointerProperties::PointerType::Unknown, 0, m_mappedIdStart, true);
    createAndCheckMouseEvent(WebPointerProperties::PointerType::Unknown, 1, m_mappedIdStart+1, false);
    createAndCheckMouseEvent(WebPointerProperties::PointerType::Unknown, 2, m_mappedIdStart+2, false);
    createAndCheckTouchEvent(WebPointerProperties::PointerType::Unknown, 0, m_mappedIdStart, true);
    createAndCheckTouchEvent(WebPointerProperties::PointerType::Unknown, 3, m_mappedIdStart+3, false);
    createAndCheckMouseEvent(WebPointerProperties::PointerType::Unknown, 2, m_mappedIdStart+2, false);
    createAndCheckTouchCancel(WebPointerProperties::PointerType::Unknown, 3, m_mappedIdStart+3, false);

    m_pointerEventFactory.remove(pointerEvent1->pointerId());

    createAndCheckTouchEvent(WebPointerProperties::PointerType::Unknown, 0, m_mappedIdStart+4, false);
    createAndCheckTouchEvent(WebPointerProperties::PointerType::Unknown, INT_MAX, m_mappedIdStart+5, false);

    m_pointerEventFactory.clear();

    for (int i = 0; i < 100; ++i) {
        createAndCheckMouseEvent(WebPointerProperties::PointerType::Touch, i, m_mappedIdStart+i, i == 0);
    }

    for (int i = 0; i < 100; ++i) {
        createAndCheckTouchEvent(WebPointerProperties::PointerType::Mouse, i, m_expectedMouseId, true);
    }
    createAndCheckTouchCancel(WebPointerProperties::PointerType::Mouse, 0, m_expectedMouseId, true);
}

} // namespace blink
