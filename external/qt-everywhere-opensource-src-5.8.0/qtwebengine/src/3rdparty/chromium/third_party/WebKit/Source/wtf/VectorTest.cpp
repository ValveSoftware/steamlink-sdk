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

#include "wtf/Vector.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/HashSet.h"
#include "wtf/PtrUtil.h"
#include "wtf/text/WTFString.h"
#include <memory>

namespace WTF {

namespace {

TEST(VectorTest, Basic)
{
    Vector<int> intVector;
    EXPECT_TRUE(intVector.isEmpty());
    EXPECT_EQ(0ul, intVector.size());
    EXPECT_EQ(0ul, intVector.capacity());
}

TEST(VectorTest, Reverse)
{
    Vector<int> intVector;
    intVector.append(10);
    intVector.append(11);
    intVector.append(12);
    intVector.append(13);
    intVector.reverse();

    EXPECT_EQ(13, intVector[0]);
    EXPECT_EQ(12, intVector[1]);
    EXPECT_EQ(11, intVector[2]);
    EXPECT_EQ(10, intVector[3]);

    intVector.append(9);
    intVector.reverse();

    EXPECT_EQ(9, intVector[0]);
    EXPECT_EQ(10, intVector[1]);
    EXPECT_EQ(11, intVector[2]);
    EXPECT_EQ(12, intVector[3]);
    EXPECT_EQ(13, intVector[4]);
}

TEST(VectorTest, Remove)
{
    Vector<int> intVector;
    intVector.append(0);
    intVector.append(1);
    intVector.append(2);
    intVector.append(3);

    EXPECT_EQ(4u, intVector.size());
    EXPECT_EQ(0, intVector[0]);
    EXPECT_EQ(1, intVector[1]);
    EXPECT_EQ(2, intVector[2]);
    EXPECT_EQ(3, intVector[3]);

    intVector.remove(2, 0);
    EXPECT_EQ(4u, intVector.size());
    EXPECT_EQ(2, intVector[2]);

    intVector.remove(2, 1);
    EXPECT_EQ(3u, intVector.size());
    EXPECT_EQ(3, intVector[2]);

    intVector.remove(0, 0);
    EXPECT_EQ(3u, intVector.size());
    EXPECT_EQ(0, intVector[0]);

    intVector.remove(0);
    EXPECT_EQ(2u, intVector.size());
    EXPECT_EQ(1, intVector[0]);
}

TEST(VectorTest, Iterator)
{
    Vector<int> intVector;
    intVector.append(10);
    intVector.append(11);
    intVector.append(12);
    intVector.append(13);

    Vector<int>::iterator it = intVector.begin();
    Vector<int>::iterator end = intVector.end();
    EXPECT_TRUE(end != it);

    EXPECT_EQ(10, *it);
    ++it;
    EXPECT_EQ(11, *it);
    ++it;
    EXPECT_EQ(12, *it);
    ++it;
    EXPECT_EQ(13, *it);
    ++it;

    EXPECT_TRUE(end == it);
}

TEST(VectorTest, ReverseIterator)
{
    Vector<int> intVector;
    intVector.append(10);
    intVector.append(11);
    intVector.append(12);
    intVector.append(13);

    Vector<int>::reverse_iterator it = intVector.rbegin();
    Vector<int>::reverse_iterator end = intVector.rend();
    EXPECT_TRUE(end != it);

    EXPECT_EQ(13, *it);
    ++it;
    EXPECT_EQ(12, *it);
    ++it;
    EXPECT_EQ(11, *it);
    ++it;
    EXPECT_EQ(10, *it);
    ++it;

    EXPECT_TRUE(end == it);
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

typedef WTF::Vector<std::unique_ptr<DestructCounter>> OwnPtrVector;

TEST(VectorTest, OwnPtr)
{
    int destructNumber = 0;
    OwnPtrVector vector;
    vector.append(wrapUnique(new DestructCounter(0, &destructNumber)));
    vector.append(wrapUnique(new DestructCounter(1, &destructNumber)));
    EXPECT_EQ(2u, vector.size());

    std::unique_ptr<DestructCounter>& counter0 = vector.first();
    ASSERT_EQ(0, counter0->get());
    int counter1 = vector.last()->get();
    ASSERT_EQ(1, counter1);
    ASSERT_EQ(0, destructNumber);

    size_t index = 0;
    for (OwnPtrVector::iterator iter = vector.begin(); iter != vector.end(); ++iter) {
        std::unique_ptr<DestructCounter>* refCounter = iter;
        EXPECT_EQ(index, static_cast<size_t>(refCounter->get()->get()));
        EXPECT_EQ(index, static_cast<size_t>((*refCounter)->get()));
        index++;
    }
    EXPECT_EQ(0, destructNumber);

    for (index = 0; index < vector.size(); index++) {
        std::unique_ptr<DestructCounter>& refCounter = vector[index];
        EXPECT_EQ(index, static_cast<size_t>(refCounter->get()));
    }
    EXPECT_EQ(0, destructNumber);

    EXPECT_EQ(0, vector[0]->get());
    EXPECT_EQ(1, vector[1]->get());
    vector.remove(0);
    EXPECT_EQ(1, vector[0]->get());
    EXPECT_EQ(1u, vector.size());
    EXPECT_EQ(1, destructNumber);

    std::unique_ptr<DestructCounter> ownCounter1 = std::move(vector[0]);
    vector.remove(0);
    ASSERT_EQ(counter1, ownCounter1->get());
    ASSERT_EQ(0u, vector.size());
    ASSERT_EQ(1, destructNumber);

    ownCounter1.reset();
    EXPECT_EQ(2, destructNumber);

    size_t count = 1025;
    destructNumber = 0;
    for (size_t i = 0; i < count; i++)
        vector.prepend(wrapUnique(new DestructCounter(i, &destructNumber)));

    // Vector relocation must not destruct std::unique_ptr element.
    EXPECT_EQ(0, destructNumber);
    EXPECT_EQ(count, vector.size());

    OwnPtrVector copyVector;
    vector.swap(copyVector);
    EXPECT_EQ(0, destructNumber);
    EXPECT_EQ(count, copyVector.size());
    EXPECT_EQ(0u, vector.size());

    copyVector.clear();
    EXPECT_EQ(count, static_cast<size_t>(destructNumber));
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

TEST(VectorTest, MoveOnlyType)
{
    WTF::Vector<MoveOnly> vector;
    vector.append(MoveOnly(1));
    vector.append(MoveOnly(2));
    EXPECT_EQ(2u, vector.size());

    ASSERT_EQ(1, vector.first().value());
    ASSERT_EQ(2, vector.last().value());

    vector.remove(0);
    EXPECT_EQ(2, vector[0].value());
    EXPECT_EQ(1u, vector.size());

    MoveOnly moveOnly(std::move(vector[0]));
    vector.remove(0);
    ASSERT_EQ(2, moveOnly.value());
    ASSERT_EQ(0u, vector.size());

    size_t count = vector.capacity() + 1;
    for (size_t i = 0; i < count; i++)
        vector.append(MoveOnly(i + 1)); // +1 to distinguish from default-constructed.

    // Reallocation did not affect the vector's content.
    EXPECT_EQ(count, vector.size());
    for (size_t i = 0; i < vector.size(); i++)
        EXPECT_EQ(static_cast<int>(i + 1), vector[i].value());

    WTF::Vector<MoveOnly> otherVector;
    vector.swap(otherVector);
    EXPECT_EQ(count, otherVector.size());
    EXPECT_EQ(0u, vector.size());

    vector = std::move(otherVector);
    EXPECT_EQ(count, vector.size());
}

// WrappedInt class will fail if it was memmoved or memcpyed.
static HashSet<void*> constructedWrappedInts;
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

TEST(VectorTest, SwapWithInlineCapacity)
{
    const size_t inlineCapacity = 2;
    Vector<WrappedInt, inlineCapacity> vectorA;
    vectorA.append(WrappedInt(1));
    Vector<WrappedInt, inlineCapacity> vectorB;
    vectorB.append(WrappedInt(2));

    EXPECT_EQ(vectorA.size(), vectorB.size());
    vectorA.swap(vectorB);

    EXPECT_EQ(1u, vectorA.size());
    EXPECT_EQ(2, vectorA.at(0).get());
    EXPECT_EQ(1u, vectorB.size());
    EXPECT_EQ(1, vectorB.at(0).get());

    vectorA.append(WrappedInt(3));

    EXPECT_GT(vectorA.size(), vectorB.size());
    vectorA.swap(vectorB);

    EXPECT_EQ(1u, vectorA.size());
    EXPECT_EQ(1, vectorA.at(0).get());
    EXPECT_EQ(2u, vectorB.size());
    EXPECT_EQ(2, vectorB.at(0).get());
    EXPECT_EQ(3, vectorB.at(1).get());

    EXPECT_LT(vectorA.size(), vectorB.size());
    vectorA.swap(vectorB);

    EXPECT_EQ(2u, vectorA.size());
    EXPECT_EQ(2, vectorA.at(0).get());
    EXPECT_EQ(3, vectorA.at(1).get());
    EXPECT_EQ(1u, vectorB.size());
    EXPECT_EQ(1, vectorB.at(0).get());

    vectorA.append(WrappedInt(4));
    EXPECT_GT(vectorA.size(), inlineCapacity);
    vectorA.swap(vectorB);

    EXPECT_EQ(1u, vectorA.size());
    EXPECT_EQ(1, vectorA.at(0).get());
    EXPECT_EQ(3u, vectorB.size());
    EXPECT_EQ(2, vectorB.at(0).get());
    EXPECT_EQ(3, vectorB.at(1).get());
    EXPECT_EQ(4, vectorB.at(2).get());

    vectorB.swap(vectorA);
}

#if defined(ANNOTATE_CONTIGUOUS_CONTAINER)
TEST(VectorTest, ContainerAnnotations)
{
    Vector<int> vectorA;
    vectorA.append(10);
    vectorA.reserveCapacity(32);

    volatile int* intPointerA = vectorA.data();
    EXPECT_DEATH(intPointerA[1] = 11, "container-overflow");
    vectorA.append(11);
    intPointerA[1] = 11;
    EXPECT_DEATH(intPointerA[2] = 12, "container-overflow");
    EXPECT_DEATH((void)intPointerA[2], "container-overflow");
    vectorA.shrinkToFit();
    vectorA.reserveCapacity(16);
    intPointerA = vectorA.data();
    EXPECT_DEATH((void)intPointerA[2], "container-overflow");

    Vector<int> vectorB(vectorA);
    vectorB.reserveCapacity(16);
    volatile int* intPointerB = vectorB.data();
    EXPECT_DEATH((void)intPointerB[2], "container-overflow");

    Vector<int> vectorC((Vector<int>(vectorA)));
    volatile int* intPointerC = vectorC.data();
    EXPECT_DEATH((void)intPointerC[2], "container-overflow");
    vectorC.append(13);
    vectorC.swap(vectorB);

    volatile int* intPointerB2 = vectorB.data();
    volatile int* intPointerC2 = vectorC.data();
    intPointerB2[2] = 13;
    EXPECT_DEATH((void)intPointerB2[3], "container-overflow");
    EXPECT_DEATH((void)intPointerC2[2], "container-overflow");

    vectorB = vectorC;
    volatile int* intPointerB3 = vectorB.data();
    EXPECT_DEATH((void)intPointerB3[2], "container-overflow");
}
#endif // defined(ANNOTATE_CONTIGUOUS_CONTAINER)

class Comparable {
};
bool operator==(const Comparable& a, const Comparable& b) { return true; }

template<typename T> void compare()
{
    EXPECT_TRUE(Vector<T>() == Vector<T>());
    EXPECT_FALSE(Vector<T>(1) == Vector<T>(0));
    EXPECT_FALSE(Vector<T>() == Vector<T>(1));
    EXPECT_TRUE(Vector<T>(1) == Vector<T>(1));

    Vector<T, 1> vectorWithInlineCapacity;
    EXPECT_TRUE(vectorWithInlineCapacity == Vector<T>());
    EXPECT_FALSE(vectorWithInlineCapacity == Vector<T>(1));
}

TEST(VectorTest, Compare)
{
    compare<int>();
    compare<Comparable>();
    compare<WTF::String>();
}

TEST(VectorTest, AppendFirst)
{
    Vector<WTF::String> vector;
    vector.append("string");
    // Test passes if it does not crash (reallocation did not make
    // the input reference stale).
    size_t limit = vector.capacity() + 1;
    for (size_t i = 0; i < limit; i++)
        vector.append(vector.first());

    limit = vector.capacity() + 1;
    for (size_t i = 0; i < limit; i++)
        vector.append(const_cast<const WTF::String&>(vector.first()));
}

// The test below is for the following issue:
//
// https://bugs.chromium.org/p/chromium/issues/detail?id=592767
//
// where deleted copy assignment operator made canMoveWithMemcpy true because of the implementation of
// IsTriviallyMoveAssignable<T>.

class MojoMoveOnlyType final {
public:
    MojoMoveOnlyType();
    MojoMoveOnlyType(MojoMoveOnlyType&&);
    MojoMoveOnlyType& operator=(MojoMoveOnlyType&&);
    ~MojoMoveOnlyType();

private:
    MojoMoveOnlyType(const MojoMoveOnlyType&) = delete;
    void operator=(const MojoMoveOnlyType&) = delete;
};

static_assert(!IsTriviallyMoveAssignable<MojoMoveOnlyType>::value, "MojoMoveOnlyType isn't trivially move assignable.");
static_assert(!IsTriviallyCopyAssignable<MojoMoveOnlyType>::value, "MojoMoveOnlyType isn't trivially copy assignable.");

static_assert(!VectorTraits<MojoMoveOnlyType>::canMoveWithMemcpy, "MojoMoveOnlyType can't be moved with memcpy.");
static_assert(!VectorTraits<MojoMoveOnlyType>::canCopyWithMemcpy, "MojoMoveOnlyType can't be copied with memcpy.");

class LivenessCounter {
public:
    void ref() { s_live++; }
    void deref() { s_live--; }

    static unsigned s_live;
};

unsigned LivenessCounter::s_live = 0;

class VectorWithDifferingInlineCapacityTest : public ::testing::TestWithParam<size_t> { };

template <size_t inlineCapacity>
void testDestructorAndConstructorCallsWhenSwappingWithInlineCapacity()
{
    LivenessCounter::s_live = 0;
    LivenessCounter counter;
    EXPECT_EQ(0u, LivenessCounter::s_live);

    Vector<RefPtr<LivenessCounter>, inlineCapacity> vector;
    Vector<RefPtr<LivenessCounter>, inlineCapacity> vector2;
    vector.append(&counter);
    vector2.append(&counter);
    EXPECT_EQ(2u, LivenessCounter::s_live);

    for (unsigned i = 0; i < 13; i++) {
        for (unsigned j = 0; j < 13; j++) {
            vector.clear();
            vector2.clear();
            EXPECT_EQ(0u, LivenessCounter::s_live);

            for (unsigned k = 0; k < j; k++)
                vector.append(&counter);
            EXPECT_EQ(j, LivenessCounter::s_live);
            EXPECT_EQ(j, vector.size());

            for (unsigned k = 0; k < i; k++)
                vector2.append(&counter);
            EXPECT_EQ(i + j, LivenessCounter::s_live);
            EXPECT_EQ(i, vector2.size());

            vector.swap(vector2);
            EXPECT_EQ(i + j, LivenessCounter::s_live);
            EXPECT_EQ(i, vector.size());
            EXPECT_EQ(j, vector2.size());

            unsigned size = vector.size();
            unsigned size2 = vector2.size();

            for (unsigned k = 0; k < 5; k++) {
                vector.swap(vector2);
                std::swap(size, size2);
                EXPECT_EQ(i + j, LivenessCounter::s_live);
                EXPECT_EQ(size, vector.size());
                EXPECT_EQ(size2, vector2.size());

                vector2.append(&counter);
                vector2.remove(0);
            }
        }
    }

}

TEST(VectorTest, SwapWithConstructorsAndDestructors)
{
    testDestructorAndConstructorCallsWhenSwappingWithInlineCapacity<0>();
    testDestructorAndConstructorCallsWhenSwappingWithInlineCapacity<2>();
    testDestructorAndConstructorCallsWhenSwappingWithInlineCapacity<10>();
}

template <size_t inlineCapacity>
void testValuesMovedAndSwappedWithInlineCapacity()
{
    Vector<unsigned, inlineCapacity> vector;
    Vector<unsigned, inlineCapacity> vector2;

    for (unsigned size = 0; size < 13; size++) {
        for (unsigned size2 = 0; size2 < 13; size2++) {
            vector.clear();
            vector2.clear();
            for (unsigned i = 0; i < size; i++)
                vector.append(i);
            for (unsigned i = 0; i < size2; i++)
                vector2.append(i + 42);
            EXPECT_EQ(size, vector.size());
            EXPECT_EQ(size2, vector2.size());
            vector.swap(vector2);
            for (unsigned i = 0; i < size; i++)
                EXPECT_EQ(i, vector2[i]);
            for (unsigned i = 0; i < size2; i++)
                EXPECT_EQ(i + 42, vector[i]);
        }
    }
}

TEST(VectorTest, ValuesMovedAndSwappedWithInlineCapacity)
{
    testValuesMovedAndSwappedWithInlineCapacity<0>();
    testValuesMovedAndSwappedWithInlineCapacity<2>();
    testValuesMovedAndSwappedWithInlineCapacity<10>();
}

TEST(VectorTest, UniquePtr)
{
    using Pointer = std::unique_ptr<int>;
    Vector<Pointer> vector;
    vector.append(Pointer(new int(1)));
    vector.reserveCapacity(2);
    vector.uncheckedAppend(Pointer(new int(2)));
    vector.insert(2, Pointer(new int(3)));
    vector.prepend(Pointer(new int(0)));

    ASSERT_EQ(4u, vector.size());
    EXPECT_EQ(0, *vector[0]);
    EXPECT_EQ(1, *vector[1]);
    EXPECT_EQ(2, *vector[2]);
    EXPECT_EQ(3, *vector[3]);

    vector.shrink(3);
    EXPECT_EQ(3u, vector.size());
    vector.grow(4);
    ASSERT_EQ(4u, vector.size());
    EXPECT_TRUE(!vector[3]);
    vector.remove(3);
    vector[0] = Pointer(new int(-1));
    ASSERT_EQ(3u, vector.size());
    EXPECT_EQ(-1, *vector[0]);
}

bool isOneTwoThree(const Vector<int>& vector)
{
    return vector.size() == 3 && vector[0] == 1 && vector[1] == 2 && vector[2] == 3;
}

Vector<int> returnOneTwoThree()
{
    return {1, 2, 3};
}

TEST(VectorTest, InitializerList)
{
    Vector<int> empty({});
    EXPECT_TRUE(empty.isEmpty());

    Vector<int> one({1});
    ASSERT_EQ(1u, one.size());
    EXPECT_EQ(1, one[0]);

    Vector<int> oneTwoThree({1, 2, 3});
    ASSERT_EQ(3u, oneTwoThree.size());
    EXPECT_EQ(1, oneTwoThree[0]);
    EXPECT_EQ(2, oneTwoThree[1]);
    EXPECT_EQ(3, oneTwoThree[2]);

    // Put some jank so we can check if the assignments later can clear them.
    empty.append(9999);
    one.append(9999);
    oneTwoThree.append(9999);

    empty = {};
    EXPECT_TRUE(empty.isEmpty());

    one = {1};
    ASSERT_EQ(1u, one.size());
    EXPECT_EQ(1, one[0]);

    oneTwoThree = {1, 2, 3};
    ASSERT_EQ(3u, oneTwoThree.size());
    EXPECT_EQ(1, oneTwoThree[0]);
    EXPECT_EQ(2, oneTwoThree[1]);
    EXPECT_EQ(3, oneTwoThree[2]);

    // Other ways of construction: as a function parameter and in a return statement.
    EXPECT_TRUE(isOneTwoThree({1, 2, 3}));
    EXPECT_TRUE(isOneTwoThree(returnOneTwoThree()));

    // The tests below correspond to the cases in the "if" branch in operator=(std::initializer_list<T>).

    // Shrinking.
    Vector<int, 1> vector1(3); // capacity = 3.
    vector1 = {1, 2};
    ASSERT_EQ(2u, vector1.size());
    EXPECT_EQ(1, vector1[0]);
    EXPECT_EQ(2, vector1[1]);

    // Expanding.
    Vector<int, 1> vector2(3);
    vector2 = {1, 2, 3, 4};
    ASSERT_EQ(4u, vector2.size());
    EXPECT_EQ(1, vector2[0]);
    EXPECT_EQ(2, vector2[1]);
    EXPECT_EQ(3, vector2[2]);
    EXPECT_EQ(4, vector2[3]);

    // Exact match.
    Vector<int, 1> vector3(3);
    vector3 = {1, 2, 3};
    ASSERT_EQ(3u, vector3.size());
    EXPECT_EQ(1, vector3[0]);
    EXPECT_EQ(2, vector3[1]);
    EXPECT_EQ(3, vector3[2]);
}

static_assert(VectorTraits<int>::canCopyWithMemcpy, "int should be copied with memcopy.");
static_assert(VectorTraits<char>::canCopyWithMemcpy, "char should be copied with memcpy.");
static_assert(VectorTraits<LChar>::canCopyWithMemcpy, "LChar should be copied with memcpy.");
static_assert(VectorTraits<UChar>::canCopyWithMemcpy, "UChar should be copied with memcpy.");

class UnknownType;
static_assert(VectorTraits<UnknownType*>::canCopyWithMemcpy, "Pointers should be copied with memcpy.");

} // anonymous namespace

} // namespace WTF
