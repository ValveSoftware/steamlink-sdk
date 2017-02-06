// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/ContiguousContainer.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/TypeTraits.h"

namespace blink {
namespace {

struct Point2D {
    Point2D() : Point2D(0, 0) { }
    Point2D(int x, int y) : x(x), y(y) { }
    int x, y;
};

struct Point3D : public Point2D {
    Point3D() : Point3D(0, 0, 0) { }
    Point3D(int x, int y, int z) : Point2D(x, y), z(z) { }
    int z;
};

// Maximum size of a subclass of Point2D.
static const size_t kMaxPointSize = sizeof(Point3D);

// Alignment for Point2D and its subclasses.
static const size_t kPointAlignment = sizeof(int);

// How many elements to use for tests with "plenty" of elements.
static const unsigned kNumElements = 150;

TEST(ContiguousContainerTest, SimpleStructs)
{
    ContiguousContainer<Point2D, kPointAlignment> list(kMaxPointSize);
    list.allocateAndConstruct<Point2D>(1, 2);
    list.allocateAndConstruct<Point3D>(3, 4, 5);
    list.allocateAndConstruct<Point2D>(6, 7);

    ASSERT_EQ(3u, list.size());
    EXPECT_EQ(1, list[0].x);
    EXPECT_EQ(2, list[0].y);
    EXPECT_EQ(3, list[1].x);
    EXPECT_EQ(4, list[1].y);
    EXPECT_EQ(5, static_cast<Point3D&>(list[1]).z);
    EXPECT_EQ(6, list[2].x);
    EXPECT_EQ(7, list[2].y);
}

TEST(ContiguousContainerTest, AllocateLots)
{
    ContiguousContainer<Point2D, kPointAlignment> list(kMaxPointSize);
    for (int i = 0; i < (int)kNumElements; i++) {
        list.allocateAndConstruct<Point2D>(i, i);
        list.allocateAndConstruct<Point2D>(i, i);
        list.removeLast();
    }
    ASSERT_EQ(kNumElements, list.size());
    for (int i = 0; i < (int)kNumElements; i++) {
        ASSERT_EQ(i, list[i].x);
        ASSERT_EQ(i, list[i].y);
    }
}

class MockDestructible {
public:
    ~MockDestructible() { destruct(); }
    MOCK_METHOD0(destruct, void());
};

TEST(ContiguousContainerTest, DestructorCalled)
{
    ContiguousContainer<MockDestructible> list(sizeof(MockDestructible));
    auto& destructible = list.allocateAndConstruct<MockDestructible>();
    EXPECT_EQ(&destructible, &list.first());
    EXPECT_CALL(destructible, destruct());
}

TEST(ContiguousContainerTest, DestructorCalledOnceWhenClear)
{
    ContiguousContainer<MockDestructible> list(sizeof(MockDestructible));
    auto& destructible = list.allocateAndConstruct<MockDestructible>();
    EXPECT_EQ(&destructible, &list.first());

    testing::MockFunction<void()> separator;
    {
        testing::InSequence s;
        EXPECT_CALL(destructible, destruct());
        EXPECT_CALL(separator, Call());
        EXPECT_CALL(destructible, destruct()).Times(0);
    }

    list.clear();
    separator.Call();
}

TEST(ContiguousContainerTest, DestructorCalledOnceWhenRemoveLast)
{
    ContiguousContainer<MockDestructible> list(sizeof(MockDestructible));
    auto& destructible = list.allocateAndConstruct<MockDestructible>();
    EXPECT_EQ(&destructible, &list.first());

    testing::MockFunction<void()> separator;
    {
        testing::InSequence s;
        EXPECT_CALL(destructible, destruct());
        EXPECT_CALL(separator, Call());
        EXPECT_CALL(destructible, destruct()).Times(0);
    }

    list.removeLast();
    separator.Call();
}

TEST(ContiguousContainerTest, DestructorCalledWithMultipleRemoveLastCalls)
{
    // This container only requests space for one, but the implementation is
    // free to use more space if the allocator provides it.
    ContiguousContainer<MockDestructible> list(sizeof(MockDestructible), 1 *  sizeof(MockDestructible));
    testing::MockFunction<void()> separator;

    // We should be okay to allocate and remove a single one, like before.
    list.allocateAndConstruct<MockDestructible>();
    EXPECT_EQ(1u, list.size());
    {
        testing::InSequence s;
        EXPECT_CALL(list[0], destruct());
        EXPECT_CALL(separator, Call());
        EXPECT_CALL(list[0], destruct()).Times(0);
    }
    list.removeLast();
    separator.Call();
    EXPECT_EQ(0u, list.size());

    testing::Mock::VerifyAndClearExpectations(&separator);

    // We should also be okay to allocate and remove multiple.
    list.allocateAndConstruct<MockDestructible>();
    list.allocateAndConstruct<MockDestructible>();
    list.allocateAndConstruct<MockDestructible>();
    list.allocateAndConstruct<MockDestructible>();
    list.allocateAndConstruct<MockDestructible>();
    list.allocateAndConstruct<MockDestructible>();
    EXPECT_EQ(6u, list.size());
    {
        // The last three should be destroyed by removeLast.
        testing::InSequence s;
        EXPECT_CALL(list[5], destruct());
        EXPECT_CALL(separator, Call());
        EXPECT_CALL(list[5], destruct()).Times(0);
        EXPECT_CALL(list[4], destruct());
        EXPECT_CALL(separator, Call());
        EXPECT_CALL(list[4], destruct()).Times(0);
        EXPECT_CALL(list[3], destruct());
        EXPECT_CALL(separator, Call());
        EXPECT_CALL(list[3], destruct()).Times(0);
    }
    list.removeLast();
    separator.Call();
    list.removeLast();
    separator.Call();
    list.removeLast();
    separator.Call();
    EXPECT_EQ(3u, list.size());

    // The remaining ones are destroyed when the test finishes.
    EXPECT_CALL(list[2], destruct());
    EXPECT_CALL(list[1], destruct());
    EXPECT_CALL(list[0], destruct());
}

TEST(ContiguousContainerTest, InsertionAndIndexedAccess)
{
    ContiguousContainer<Point2D, kPointAlignment> list(kMaxPointSize);

    auto& point1 = list.allocateAndConstruct<Point2D>();
    auto& point2 = list.allocateAndConstruct<Point2D>();
    auto& point3 = list.allocateAndConstruct<Point2D>();

    EXPECT_EQ(3u, list.size());
    EXPECT_EQ(&point1, &list.first());
    EXPECT_EQ(&point3, &list.last());
    EXPECT_EQ(&point1, &list[0]);
    EXPECT_EQ(&point2, &list[1]);
    EXPECT_EQ(&point3, &list[2]);
}

TEST(ContiguousContainerTest, InsertionAndClear)
{
    ContiguousContainer<Point2D, kPointAlignment> list(kMaxPointSize);
    EXPECT_TRUE(list.isEmpty());
    EXPECT_EQ(0u, list.size());

    list.allocateAndConstruct<Point2D>();
    EXPECT_FALSE(list.isEmpty());
    EXPECT_EQ(1u, list.size());

    list.clear();
    EXPECT_TRUE(list.isEmpty());
    EXPECT_EQ(0u, list.size());

    list.allocateAndConstruct<Point2D>();
    EXPECT_FALSE(list.isEmpty());
    EXPECT_EQ(1u, list.size());
}

TEST(ContiguousContainerTest, ElementAddressesAreStable)
{
    ContiguousContainer<Point2D, kPointAlignment> list(kMaxPointSize);
    Vector<Point2D*> pointers;
    for (int i = 0; i < (int)kNumElements; i++)
        pointers.append(&list.allocateAndConstruct<Point2D>());
    EXPECT_EQ(kNumElements, list.size());
    EXPECT_EQ(kNumElements, pointers.size());

    auto listIt = list.begin();
    auto vectorIt = pointers.begin();
    for (; listIt != list.end(); ++listIt, ++vectorIt)
        EXPECT_EQ(&*listIt, *vectorIt);
}

TEST(ContiguousContainerTest, ForwardIteration)
{
    ContiguousContainer<Point2D, kPointAlignment> list(kMaxPointSize);
    for (int i = 0; i < (int)kNumElements; i++)
        list.allocateAndConstruct<Point2D>(i, i);
    unsigned count = 0;
    for (Point2D& point : list) {
        EXPECT_EQ((int)count, point.x);
        count++;
    }
    EXPECT_EQ(kNumElements, count);

    static_assert(std::is_same<decltype(*list.begin()), Point2D&>::value,
        "Non-const iteration should produce non-const references.");
}

TEST(ContiguousContainerTest, ConstForwardIteration)
{
    ContiguousContainer<Point2D, kPointAlignment> list(kMaxPointSize);
    for (int i = 0; i < (int)kNumElements; i++)
        list.allocateAndConstruct<Point2D>(i, i);

    const auto& constList = list;
    unsigned count = 0;
    for (const Point2D& point : constList) {
        EXPECT_EQ((int)count, point.x);
        count++;
    }
    EXPECT_EQ(kNumElements, count);

    static_assert(std::is_same<decltype(*constList.begin()), const Point2D&>::value,
        "Const iteration should produce const references.");
}

TEST(ContiguousContainerTest, ReverseIteration)
{
    ContiguousContainer<Point2D, kPointAlignment> list(kMaxPointSize);
    for (int i = 0; i < (int)kNumElements; i++)
        list.allocateAndConstruct<Point2D>(i, i);

    unsigned count = 0;
    for (auto it = list.rbegin(); it != list.rend(); ++it) {
        EXPECT_EQ((int)(kNumElements - 1 - count), it->x);
        count++;
    }
    EXPECT_EQ(kNumElements, count);

    static_assert(std::is_same<decltype(*list.rbegin()), Point2D&>::value,
        "Non-const iteration should produce non-const references.");
}

// Checks that the latter list has pointers to the elements of the former.
template <typename It1, typename It2>
bool EqualPointers(It1 it1, const It1& end1, It2 it2)
{
    for (; it1 != end1; ++it1, ++it2) {
        if (&*it1 != *it2)
            return false;
    }
    return true;
}

TEST(ContiguousContainerTest, IterationAfterRemoveLast)
{
    struct SmallStruct {
        char dummy[16];
    };
    ContiguousContainer<SmallStruct> list(sizeof(SmallStruct), 1 * sizeof(SmallStruct));
    Vector<SmallStruct*> pointers;

    // Utilities which keep these two lists in sync and check that their
    // iteration order matches.
    auto push = [&list, &pointers]()
    {
        pointers.append(&list.allocateAndConstruct<SmallStruct>());
    };
    auto pop = [&list, &pointers]()
    {
        pointers.removeLast();
        list.removeLast();
    };
    auto check_equal = [&list, &pointers]()
    {
        // They should be of the same size, and compare equal with all four
        // kinds of iteration.
        const auto& constList = list;
        const auto& constPointers = pointers;
        ASSERT_EQ(list.size(), pointers.size());
        ASSERT_TRUE(EqualPointers(list.begin(), list.end(), pointers.begin()));
        ASSERT_TRUE(EqualPointers(constList.begin(), constList.end(), constPointers.begin()));
        ASSERT_TRUE(EqualPointers(list.rbegin(), list.rend(), pointers.rbegin()));
        ASSERT_TRUE(EqualPointers(constList.rbegin(), constList.rend(), constPointers.rbegin()));
    };

    // Note that the allocations that actually happen may not match the
    // idealized descriptions here, since the implementation takes advantage of
    // space available in the underlying allocator.
    check_equal(); // Initially empty.
    push();
    check_equal(); // One full inner list.
    push();
    check_equal(); // One full, one partially full.
    push();
    push();
    check_equal(); // Two full, one partially full.
    pop();
    check_equal(); // Two full, one empty.
    pop();
    check_equal(); // One full, one partially full, one empty.
    pop();
    check_equal(); // One full, one empty.
    push();
    pop();
    pop();
    ASSERT_TRUE(list.isEmpty());
    check_equal(); // Empty.
}

TEST(ContiguousContainerTest, AppendByMovingSameList)
{
    ContiguousContainer<Point2D, kPointAlignment> list(kMaxPointSize);
    list.allocateAndConstruct<Point3D>(1, 2, 3);

    // Moves the Point3D to the end, and default-constructs a Point2D in its
    // place.
    list.appendByMoving(list.first(), sizeof(Point3D));
    EXPECT_EQ(1, list.last().x);
    EXPECT_EQ(2, list.last().y);
    EXPECT_EQ(3, static_cast<const Point3D&>(list.last()).z);
    EXPECT_EQ(2u, list.size());

    // Moves that Point2D to the end, and default-constructs another in its
    // place.
    list.first().x = 4;
    list.appendByMoving(list.first(), sizeof(Point2D));
    EXPECT_EQ(4, list.last().x);
    EXPECT_EQ(3u, list.size());
}

TEST(ContiguousContainerTest, AppendByMovingDoesNotDestruct)
{
    // GMock mock objects (e.g. MockDestructible) aren't guaranteed to be safe
    // to memcpy (which is required for appendByMoving).
    class DestructionNotifier {
    public:
        DestructionNotifier(bool* flag = nullptr) : m_flag(flag) { }
        ~DestructionNotifier() { if (m_flag) *m_flag = true; }
    private:
        bool* m_flag;
    };

    bool destroyed = false;
    ContiguousContainer<DestructionNotifier> list1(sizeof(DestructionNotifier));
    list1.allocateAndConstruct<DestructionNotifier>(&destroyed);
    {
        // Make sure destructor isn't called during appendByMoving.
        ContiguousContainer<DestructionNotifier> list2(sizeof(DestructionNotifier));
        list2.appendByMoving(list1.last(), sizeof(DestructionNotifier));
        EXPECT_FALSE(destroyed);
    }
    // But it should be destroyed when list2 is.
    EXPECT_TRUE(destroyed);
}

TEST(ContiguousContainerTest, AppendByMovingReturnsMovedPointer)
{
    ContiguousContainer<Point2D, kPointAlignment> list1(kMaxPointSize);
    ContiguousContainer<Point2D, kPointAlignment> list2(kMaxPointSize);

    Point2D& point = list1.allocateAndConstruct<Point2D>();
    Point2D& movedPoint1 = list2.appendByMoving(point, sizeof(Point2D));
    EXPECT_EQ(&movedPoint1, &list2.last());

    Point2D& movedPoint2 = list1.appendByMoving(movedPoint1, sizeof(Point2D));
    EXPECT_EQ(&movedPoint2, &list1.last());
    EXPECT_NE(&movedPoint1, &movedPoint2);
}

TEST(ContiguousContainerTest, AppendByMovingReplacesSourceWithNewElement)
{
    ContiguousContainer<Point2D, kPointAlignment> list1(kMaxPointSize);
    ContiguousContainer<Point2D, kPointAlignment> list2(kMaxPointSize);

    list1.allocateAndConstruct<Point2D>(1, 2);
    EXPECT_EQ(1, list1.first().x);
    EXPECT_EQ(2, list1.first().y);

    list2.appendByMoving(list1.first(), sizeof(Point2D));
    EXPECT_EQ(0, list1.first().x);
    EXPECT_EQ(0, list1.first().y);
    EXPECT_EQ(1, list2.first().x);
    EXPECT_EQ(2, list2.first().y);

    EXPECT_EQ(1u, list1.size());
    EXPECT_EQ(1u, list2.size());
}

TEST(ContiguousContainerTest, AppendByMovingElementsOfDifferentSizes)
{
    ContiguousContainer<Point2D, kPointAlignment> list(kMaxPointSize);
    list.allocateAndConstruct<Point3D>(1, 2, 3);
    list.allocateAndConstruct<Point2D>(4, 5);

    EXPECT_EQ(1, list[0].x);
    EXPECT_EQ(2, list[0].y);
    EXPECT_EQ(3, static_cast<const Point3D&>(list[0]).z);
    EXPECT_EQ(4, list[1].x);
    EXPECT_EQ(5, list[1].y);

    // Test that moving the first element actually moves the entire object, not
    // just the base element.
    list.appendByMoving(list[0], sizeof(Point3D));
    EXPECT_EQ(1, list[2].x);
    EXPECT_EQ(2, list[2].y);
    EXPECT_EQ(3, static_cast<const Point3D&>(list[2]).z);
    EXPECT_EQ(4, list[1].x);
    EXPECT_EQ(5, list[1].y);

    list.appendByMoving(list[1], sizeof(Point2D));
    EXPECT_EQ(1, list[2].x);
    EXPECT_EQ(2, list[2].y);
    EXPECT_EQ(3, static_cast<const Point3D&>(list[2]).z);
    EXPECT_EQ(4, list[3].x);
    EXPECT_EQ(5, list[3].y);
}

TEST(ContiguousContainerTest, Swap)
{
    ContiguousContainer<Point2D, kPointAlignment> list1(kMaxPointSize);
    list1.allocateAndConstruct<Point2D>(1, 2);
    ContiguousContainer<Point2D, kPointAlignment> list2(kMaxPointSize);
    list2.allocateAndConstruct<Point2D>(3, 4);
    list2.allocateAndConstruct<Point2D>(5, 6);

    EXPECT_EQ(1u, list1.size());
    EXPECT_EQ(1, list1[0].x);
    EXPECT_EQ(2, list1[0].y);
    EXPECT_EQ(2u, list2.size());
    EXPECT_EQ(3, list2[0].x);
    EXPECT_EQ(4, list2[0].y);
    EXPECT_EQ(5, list2[1].x);
    EXPECT_EQ(6, list2[1].y);

    list2.swap(list1);

    EXPECT_EQ(1u, list2.size());
    EXPECT_EQ(1, list2[0].x);
    EXPECT_EQ(2, list2[0].y);
    EXPECT_EQ(2u, list1.size());
    EXPECT_EQ(3, list1[0].x);
    EXPECT_EQ(4, list1[0].y);
    EXPECT_EQ(5, list1[1].x);
    EXPECT_EQ(6, list1[1].y);
}

TEST(ContiguousContainerTest, CapacityInBytes)
{
    const int iterations = 500;
    const size_t initialCapacity = 10 * kMaxPointSize;
    const size_t upperBoundOnMinCapacity = initialCapacity;

    // At time of writing, removing elements from the end can cause up to 7x the
    // memory required to be consumed, in the worst case, since we can have up to
    // two trailing inner lists that are empty (for 2*size + 4*size in unused
    // memory, due to the exponential growth strategy).
    // Unfortunately, this captures behaviour of the underlying allocator as
    // well as this container, so we're pretty loose here. This constant may
    // need to be adjusted.
    const size_t maxWasteFactor = 8;

    ContiguousContainer<Point2D, kPointAlignment> list(kMaxPointSize, initialCapacity);

    // The capacity should grow with the list.
    for (int i = 0; i < iterations; i++) {
        size_t capacity = list.capacityInBytes();
        ASSERT_GE(capacity, list.size() * sizeof(Point2D));
        ASSERT_LE(capacity,
            std::max(list.size() * sizeof(Point2D), upperBoundOnMinCapacity) * maxWasteFactor);
        list.allocateAndConstruct<Point2D>();
    }

    // The capacity should shrink with the list.
    for (int i = 0; i < iterations; i++) {
        size_t capacity = list.capacityInBytes();
        ASSERT_GE(capacity, list.size() * sizeof(Point2D));
        ASSERT_LE(capacity,
            std::max(list.size() * sizeof(Point2D), upperBoundOnMinCapacity) * maxWasteFactor);
        list.removeLast();
    }
}

TEST(ContiguousContainerTest, CapacityInBytesAfterClear)
{
    // Clearing should restore the capacity of the container to the same as a
    // newly allocated one (without reserved capacity requested).
    ContiguousContainer<Point2D, kPointAlignment> list(kMaxPointSize);
    size_t emptyCapacity = list.capacityInBytes();
    list.allocateAndConstruct<Point2D>();
    list.allocateAndConstruct<Point2D>();
    list.clear();
    EXPECT_EQ(emptyCapacity, list.capacityInBytes());
}

} // namespace
} // namespace blink
