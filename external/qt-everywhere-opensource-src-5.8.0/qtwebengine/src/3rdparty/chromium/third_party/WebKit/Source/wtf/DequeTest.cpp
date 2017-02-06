/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "wtf/Deque.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/HashSet.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace WTF {

namespace {

TEST(DequeTest, Basic)
{
    Deque<int> intDeque;
    EXPECT_TRUE(intDeque.isEmpty());
    EXPECT_EQ(0ul, intDeque.size());
}

template <size_t inlineCapacity>
void checkNumberSequence(Deque<int, inlineCapacity>& deque, int from, int to, bool increment)
{
    auto it = increment ? deque.begin() : deque.end();
    size_t index = increment ? 0 : deque.size();
    int step = from < to ? 1 : -1;
    for (int i = from; i != to + step; i += step) {
        if (!increment) {
            --it;
            --index;
        }

        EXPECT_EQ(i, *it);
        EXPECT_EQ(i, deque[index]);

        if (increment) {
            ++it;
            ++index;
        }
    }
    EXPECT_EQ(increment ? deque.end() : deque.begin(), it);
    EXPECT_EQ(increment ? deque.size() : 0, index);
}

template <size_t inlineCapacity>
void checkNumberSequenceReverse(Deque<int, inlineCapacity>& deque, int from, int to, bool increment)
{
    auto it = increment ? deque.rbegin() : deque.rend();
    size_t index = increment ? 0 : deque.size();
    int step = from < to ? 1 : -1;
    for (int i = from; i != to + step; i += step) {
        if (!increment) {
            --it;
            --index;
        }

        EXPECT_EQ(i, *it);
        EXPECT_EQ(i, deque.at(deque.size() - 1 - index));

        if (increment) {
            ++it;
            ++index;
        }
    }
    EXPECT_EQ(increment ? deque.rend() : deque.rbegin(), it);
    EXPECT_EQ(increment ? deque.size() : 0, index);
}

template <size_t inlineCapacity>
void reverseTest()
{
    Deque<int, inlineCapacity> intDeque;
    intDeque.append(10);
    intDeque.append(11);
    intDeque.append(12);
    intDeque.append(13);

    checkNumberSequence(intDeque, 10, 13, true);
    checkNumberSequence(intDeque, 13, 10, false);
    checkNumberSequenceReverse(intDeque, 13, 10, true);
    checkNumberSequenceReverse(intDeque, 10, 13, false);

    intDeque.append(14);
    intDeque.append(15);
    EXPECT_EQ(10, intDeque.takeFirst());
    EXPECT_EQ(15, intDeque.takeLast());
    checkNumberSequence(intDeque, 11, 14, true);
    checkNumberSequence(intDeque, 14, 11, false);
    checkNumberSequenceReverse(intDeque, 14, 11, true);
    checkNumberSequenceReverse(intDeque, 11, 14, false);

    for (int i = 15; i < 200; ++i)
        intDeque.append(i);
    checkNumberSequence(intDeque, 11, 199, true);
    checkNumberSequence(intDeque, 199, 11, false);
    checkNumberSequenceReverse(intDeque, 199, 11, true);
    checkNumberSequenceReverse(intDeque, 11, 199, false);

    for (int i = 0; i < 180; ++i) {
        EXPECT_EQ(i + 11, intDeque[0]);
        EXPECT_EQ(i + 11, intDeque.takeFirst());
    }
    checkNumberSequence(intDeque, 191, 199, true);
    checkNumberSequence(intDeque, 199, 191, false);
    checkNumberSequenceReverse(intDeque, 199, 191, true);
    checkNumberSequenceReverse(intDeque, 191, 199, false);

    Deque<int, inlineCapacity> intDeque2;
    swap(intDeque, intDeque2);

    checkNumberSequence(intDeque2, 191, 199, true);
    checkNumberSequence(intDeque2, 199, 191, false);
    checkNumberSequenceReverse(intDeque2, 199, 191, true);
    checkNumberSequenceReverse(intDeque2, 191, 199, false);

    intDeque.swap(intDeque2);

    checkNumberSequence(intDeque, 191, 199, true);
    checkNumberSequence(intDeque, 199, 191, false);
    checkNumberSequenceReverse(intDeque, 199, 191, true);
    checkNumberSequenceReverse(intDeque, 191, 199, false);

    intDeque.swap(intDeque2);

    checkNumberSequence(intDeque2, 191, 199, true);
    checkNumberSequence(intDeque2, 199, 191, false);
    checkNumberSequenceReverse(intDeque2, 199, 191, true);
    checkNumberSequenceReverse(intDeque2, 191, 199, false);
}

TEST(DequeTest, Reverse)
{
    reverseTest<0>();
    reverseTest<2>();
}

class DestructCounter {
public:
    explicit DestructCounter(int i, int* destructNumber)
        : m_i(i)
        , m_destructNumber(destructNumber)
    { }

    ~DestructCounter() { ++(*m_destructNumber); }
    int get() const { return m_i; }

private:
    int m_i;
    int* m_destructNumber;
};

template <typename OwnPtrDeque>
void ownPtrTest()
{
    int destructNumber = 0;
    OwnPtrDeque deque;
    deque.append(wrapUnique(new DestructCounter(0, &destructNumber)));
    deque.append(wrapUnique(new DestructCounter(1, &destructNumber)));
    EXPECT_EQ(2u, deque.size());

    std::unique_ptr<DestructCounter>& counter0 = deque.first();
    EXPECT_EQ(0, counter0->get());
    int counter1 = deque.last()->get();
    EXPECT_EQ(1, counter1);
    EXPECT_EQ(0, destructNumber);

    size_t index = 0;
    for (auto iter = deque.begin(); iter != deque.end(); ++iter) {
        std::unique_ptr<DestructCounter>& refCounter = *iter;
        EXPECT_EQ(index, static_cast<size_t>(refCounter->get()));
        EXPECT_EQ(index, static_cast<size_t>((*refCounter).get()));
        index++;
    }
    EXPECT_EQ(0, destructNumber);

    auto it = deque.begin();
    for (index = 0; index < deque.size(); ++index) {
        std::unique_ptr<DestructCounter>& refCounter = *it;
        EXPECT_EQ(index, static_cast<size_t>(refCounter->get()));
        index++;
        ++it;
    }
    EXPECT_EQ(0, destructNumber);

    EXPECT_EQ(0, deque.first()->get());
    deque.removeFirst();
    EXPECT_EQ(1, deque.first()->get());
    EXPECT_EQ(1u, deque.size());
    EXPECT_EQ(1, destructNumber);

    std::unique_ptr<DestructCounter> ownCounter1 = std::move(deque.first());
    deque.removeFirst();
    EXPECT_EQ(counter1, ownCounter1->get());
    EXPECT_EQ(0u, deque.size());
    EXPECT_EQ(1, destructNumber);

    ownCounter1.reset();
    EXPECT_EQ(2, destructNumber);

    size_t count = 1025;
    destructNumber = 0;
    for (size_t i = 0; i < count; ++i)
        deque.prepend(wrapUnique(new DestructCounter(i, &destructNumber)));

    // Deque relocation must not destruct std::unique_ptr element.
    EXPECT_EQ(0, destructNumber);
    EXPECT_EQ(count, deque.size());

    OwnPtrDeque copyDeque;
    deque.swap(copyDeque);
    EXPECT_EQ(0, destructNumber);
    EXPECT_EQ(count, copyDeque.size());
    EXPECT_EQ(0u, deque.size());

    copyDeque.clear();
    EXPECT_EQ(count, static_cast<size_t>(destructNumber));
}

TEST(DequeTest, OwnPtr)
{
    ownPtrTest<Deque<std::unique_ptr<DestructCounter>>>();
    ownPtrTest<Deque<std::unique_ptr<DestructCounter>, 2>>();
}

class MoveOnly {
public:
    explicit MoveOnly(int i = 0)
        : m_i(i)
    { }

    MoveOnly(MoveOnly&& other)
        : m_i(other.m_i)
    {
        other.m_i = 0;
    }

    MoveOnly& operator=(MoveOnly&& other)
    {
        if (this != &other) {
            m_i = other.m_i;
            other.m_i = 0;
        }
        return *this;
    }

    int value() const { return m_i; }

private:
    WTF_MAKE_NONCOPYABLE(MoveOnly);
    int m_i;
};

TEST(DequeTest, MoveOnlyType)
{
    Deque<MoveOnly> deque;
    deque.append(MoveOnly(1));
    deque.append(MoveOnly(2));
    EXPECT_EQ(2u, deque.size());

    ASSERT_EQ(1, deque.first().value());
    ASSERT_EQ(2, deque.last().value());

    MoveOnly oldFirst = deque.takeFirst();
    ASSERT_EQ(1, oldFirst.value());
    EXPECT_EQ(1u, deque.size());

    Deque<MoveOnly> otherDeque;
    deque.swap(otherDeque);
    EXPECT_EQ(1u, otherDeque.size());
    EXPECT_EQ(0u, deque.size());
}

// WrappedInt class will fail if it was memmoved or memcpyed.
HashSet<void*> constructedWrappedInts;

class WrappedInt {
public:
    WrappedInt(int i = 0)
        : m_originalThisPtr(this)
        , m_i(i)
    {
        constructedWrappedInts.add(this);
    }

    WrappedInt(const WrappedInt& other)
        : m_originalThisPtr(this)
        , m_i(other.m_i)
    {
        constructedWrappedInts.add(this);
    }

    WrappedInt& operator=(const WrappedInt& other)
    {
        m_i = other.m_i;
        return *this;
    }

    ~WrappedInt()
    {
        EXPECT_EQ(m_originalThisPtr, this);
        EXPECT_TRUE(constructedWrappedInts.contains(this));
        constructedWrappedInts.remove(this);
    }

    int get() const { return m_i; }

private:
    void* m_originalThisPtr;
    int m_i;
};

template <size_t inlineCapacity>
void swapWithOrWithoutInlineCapacity()
{
    Deque<WrappedInt, inlineCapacity> dequeA;
    dequeA.append(WrappedInt(1));
    Deque<WrappedInt, inlineCapacity> dequeB;
    dequeB.append(WrappedInt(2));

    ASSERT_EQ(dequeA.size(), dequeB.size());
    dequeA.swap(dequeB);

    ASSERT_EQ(1u, dequeA.size());
    EXPECT_EQ(2, dequeA.first().get());
    ASSERT_EQ(1u, dequeB.size());
    EXPECT_EQ(1, dequeB.first().get());

    dequeA.append(WrappedInt(3));

    ASSERT_GT(dequeA.size(), dequeB.size());
    dequeA.swap(dequeB);

    ASSERT_EQ(1u, dequeA.size());
    EXPECT_EQ(1, dequeA.first().get());
    ASSERT_EQ(2u, dequeB.size());
    EXPECT_EQ(2, dequeB.first().get());

    ASSERT_LT(dequeA.size(), dequeB.size());
    dequeA.swap(dequeB);

    ASSERT_EQ(2u, dequeA.size());
    EXPECT_EQ(2, dequeA.first().get());
    ASSERT_EQ(1u, dequeB.size());
    EXPECT_EQ(1, dequeB.first().get());

    dequeA.append(WrappedInt(4));
    dequeA.swap(dequeB);

    ASSERT_EQ(1u, dequeA.size());
    EXPECT_EQ(1, dequeA.first().get());
    ASSERT_EQ(3u, dequeB.size());
    EXPECT_EQ(2, dequeB.first().get());

    dequeB.swap(dequeA);
}

TEST(DequeTest, SwapWithOrWithoutInlineCapacity)
{
    swapWithOrWithoutInlineCapacity<0>();
    swapWithOrWithoutInlineCapacity<2>();
}

class LivenessCounter {
public:
    void ref() { s_live++; }
    void deref() { s_live--; }

    static unsigned s_live;
};

unsigned LivenessCounter::s_live = 0;

// Filter a few numbers out to improve the running speed of the tests. The
// test has nested loops, and removing even numbers from 4 and up from the
// loops makes it run 10 times faster.
bool interestingNumber(int i)
{
    return i < 4 || (i & 1);
}

template<size_t inlineCapacity>
void testDestructorAndConstructorCallsWhenSwappingWithInlineCapacity()
{
    LivenessCounter::s_live = 0;
    LivenessCounter counter;
    EXPECT_EQ(0u, LivenessCounter::s_live);

    Deque<RefPtr<LivenessCounter>, inlineCapacity> deque;
    Deque<RefPtr<LivenessCounter>, inlineCapacity> deque2;
    deque.append(&counter);
    deque2.append(&counter);
    EXPECT_EQ(2u, LivenessCounter::s_live);

    // Add various numbers of elements to deques, then remove various numbers
    // of elements from the head. This creates in-use ranges in the backing
    // that sometimes wrap around the end of the buffer, testing various ways
    // in which the in-use ranges of the inline buffers can overlap when we
    // call swap().
    for (unsigned i = 0; i < 12; i++) {
        if (!interestingNumber(i))
            continue;
        for (unsigned j = i; j < 12; j++) {
            if (!interestingNumber(j))
                continue;
            deque.clear();
            deque2.clear();
            EXPECT_EQ(0u, LivenessCounter::s_live);
            for (unsigned k = 0; k < j; k++)
                deque.append(&counter);
            EXPECT_EQ(j, LivenessCounter::s_live);
            EXPECT_EQ(j, deque.size());
            for (unsigned k = 0; k < i; k++)
                deque.removeFirst();

            EXPECT_EQ(j - i, LivenessCounter::s_live);
            EXPECT_EQ(j - i, deque.size());
            deque.swap(deque2);
            EXPECT_EQ(j - i, LivenessCounter::s_live);
            EXPECT_EQ(0u, deque.size());
            EXPECT_EQ(j - i, deque2.size());
            deque.swap(deque2);
            EXPECT_EQ(j - i, LivenessCounter::s_live);

            deque2.append(&counter);
            deque2.append(&counter);
            deque2.append(&counter);

            for (unsigned k = 0; k < 12; k++) {
                EXPECT_EQ(3 + j - i, LivenessCounter::s_live);
                EXPECT_EQ(j - i, deque.size());
                EXPECT_EQ(3u, deque2.size());
                deque.swap(deque2);
                EXPECT_EQ(3 + j - i, LivenessCounter::s_live);
                EXPECT_EQ(j - i, deque2.size());
                EXPECT_EQ(3u, deque.size());
                deque.swap(deque2);
                EXPECT_EQ(3 + j - i, LivenessCounter::s_live);
                EXPECT_EQ(j - i, deque.size());
                EXPECT_EQ(3u, deque2.size());

                deque2.removeFirst();
                deque2.append(&counter);
            }
        }
    }

}

TEST(DequeTest, SwapWithConstructorsAndDestructors)
{
    testDestructorAndConstructorCallsWhenSwappingWithInlineCapacity<0>();
    testDestructorAndConstructorCallsWhenSwappingWithInlineCapacity<4>();
    testDestructorAndConstructorCallsWhenSwappingWithInlineCapacity<9>();
}

template<size_t inlineCapacity>
void testValuesMovedAndSwappedWithInlineCapacity()
{
    Deque<unsigned, inlineCapacity> deque;
    Deque<unsigned, inlineCapacity> deque2;

    // Add various numbers of elements to deques, then remove various numbers
    // of elements from the head. This creates in-use ranges in the backing
    // that sometimes wrap around the end of the buffer, testing various ways
    // in which the in-use ranges of the inline buffers can overlap when we
    // call swap().
    for (unsigned pad = 0; pad < 12; pad++) {
        if (!interestingNumber(pad))
            continue;
        for (unsigned pad2 = 0; pad2 < 12; pad2++) {
            if (!interestingNumber(pad2))
                continue;
            for (unsigned size = 0; size < 12; size++) {
                if (!interestingNumber(size))
                    continue;
                for (unsigned size2 = 0; size2 < 12; size2++) {
                    if (!interestingNumber(size2))
                        continue;
                    deque.clear();
                    deque2.clear();
                    for (unsigned i = 0; i < pad; i++)
                        deque.append(103);
                    for (unsigned i = 0; i < pad2; i++)
                        deque2.append(888);
                    for (unsigned i = 0; i < size; i++)
                        deque.append(i);
                    for (unsigned i = 0; i < size2; i++)
                        deque2.append(i + 42);
                    for (unsigned i = 0; i < pad; i++)
                        EXPECT_EQ(103u, deque.takeFirst());
                    for (unsigned i = 0; i < pad2; i++)
                        EXPECT_EQ(888u, deque2.takeFirst());
                    EXPECT_EQ(size, deque.size());
                    EXPECT_EQ(size2, deque2.size());
                    deque.swap(deque2);
                    for (unsigned i = 0; i < size; i++)
                        EXPECT_EQ(i, deque2.takeFirst());
                    for (unsigned i = 0; i < size2; i++)
                        EXPECT_EQ(i + 42, deque.takeFirst());
                }
            }
        }
    }
}

TEST(DequeTest, ValuesMovedAndSwappedWithInlineCapacity)
{
    testValuesMovedAndSwappedWithInlineCapacity<0>();
    testValuesMovedAndSwappedWithInlineCapacity<4>();
    testValuesMovedAndSwappedWithInlineCapacity<9>();
}

TEST(DequeTest, UniquePtr)
{
    using Pointer = std::unique_ptr<int>;
    Deque<Pointer> deque;
    deque.append(Pointer(new int(1)));
    deque.append(Pointer(new int(2)));
    deque.prepend(Pointer(new int(-1)));
    deque.prepend(Pointer(new int(-2)));
    ASSERT_EQ(4u, deque.size());
    EXPECT_EQ(-2, *deque[0]);
    EXPECT_EQ(-1, *deque[1]);
    EXPECT_EQ(1, *deque[2]);
    EXPECT_EQ(2, *deque[3]);

    Pointer first(deque.takeFirst());
    EXPECT_EQ(-2, *first);
    Pointer last(deque.takeLast());
    EXPECT_EQ(2, *last);

    EXPECT_EQ(2u, deque.size());
    deque.removeFirst();
    deque.removeLast();
    EXPECT_EQ(0u, deque.size());

    deque.append(Pointer(new int(42)));
    deque[0] = Pointer(new int(24));
    ASSERT_EQ(1u, deque.size());
    EXPECT_EQ(24, *deque[0]);

    deque.clear();
}

class CountCopy final {
public:
    explicit CountCopy(int* counter = nullptr) : m_counter(counter) { }
    CountCopy(const CountCopy& other)
        : m_counter(other.m_counter)
    {
        if (m_counter)
            ++*m_counter;
    }
    CountCopy& operator=(const CountCopy& other)
    {
        m_counter = other.m_counter;
        if (m_counter)
            ++*m_counter;
        return *this;
    }

private:
    int* m_counter;
};

TEST(DequeTest, MoveShouldNotMakeCopy)
{
    // Because data in inline buffer may be swapped or moved individually, we force the creation of out-of-line buffer
    // so we can make sure there's no element-wise copy/move.
    Deque<CountCopy, 1> deque;
    int counter = 0;
    deque.append(CountCopy(&counter));
    deque.append(CountCopy(&counter));

    Deque<CountCopy, 1> other(deque);
    counter = 0;
    deque = std::move(other); // Move assignment.
    EXPECT_EQ(0, counter);

    counter = 0;
    Deque<CountCopy, 1> yetAnother(std::move(deque)); // Move construction.
    EXPECT_EQ(0, counter);
}

} // anonymous namespace

} // namespace WTF
