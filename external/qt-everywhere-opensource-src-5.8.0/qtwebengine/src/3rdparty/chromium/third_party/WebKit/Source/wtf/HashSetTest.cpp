/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#include "wtf/HashSet.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/PtrUtil.h"
#include "wtf/RefCounted.h"
#include <memory>

namespace WTF {

namespace {

template<unsigned size> void testReserveCapacity();
template<> void testReserveCapacity<0>() {}
template<unsigned size> void testReserveCapacity()
{
    HashSet<int> testSet;

    // Initial capacity is zero.
    EXPECT_EQ(0UL, testSet.capacity());

    testSet.reserveCapacityForSize(size);
    const unsigned initialCapacity = testSet.capacity();
    const unsigned minimumTableSize = HashTraits<int>::minimumTableSize;

    // reserveCapacityForSize should respect minimumTableSize.
    EXPECT_GE(initialCapacity, minimumTableSize);

    // Adding items up to size should never change the capacity.
    for (size_t i = 0; i < size; ++i) {
        testSet.add(i + 1); // Avoid adding '0'.
        EXPECT_EQ(initialCapacity, testSet.capacity());
    }

    // Adding items up to less than half the capacity should not change the capacity.
    unsigned capacityLimit = initialCapacity / 2 - 1;
    for (size_t i = size; i < capacityLimit; ++i) {
        testSet.add(i + 1);
        EXPECT_EQ(initialCapacity, testSet.capacity());
    }

    // Adding one more item increases the capacity.
    testSet.add(capacityLimit + 1);
    EXPECT_GT(testSet.capacity(), initialCapacity);

    testReserveCapacity<size-1>();
}

TEST(HashSetTest, ReserveCapacity)
{
    testReserveCapacity<128>();
}

struct Dummy {
    Dummy(bool& deleted) : deleted(deleted) { }

    ~Dummy()
    {
        deleted = true;
    }

    bool& deleted;
};

TEST(HashSetTest, HashSetOwnPtr)
{
    bool deleted1 = false, deleted2 = false;

    typedef HashSet<std::unique_ptr<Dummy>> OwnPtrSet;
    OwnPtrSet set;

    Dummy* ptr1 = new Dummy(deleted1);
    {
        // AddResult in a separate scope to avoid assertion hit,
        // since we modify the container further.
        HashSet<std::unique_ptr<Dummy>>::AddResult res1 = set.add(wrapUnique(ptr1));
        EXPECT_EQ(ptr1, res1.storedValue->get());
    }

    EXPECT_FALSE(deleted1);
    EXPECT_EQ(1UL, set.size());
    OwnPtrSet::iterator it1 = set.find(ptr1);
    EXPECT_NE(set.end(), it1);
    EXPECT_EQ(ptr1, (*it1).get());

    Dummy* ptr2 = new Dummy(deleted2);
    {
        HashSet<std::unique_ptr<Dummy>>::AddResult res2 = set.add(wrapUnique(ptr2));
        EXPECT_EQ(res2.storedValue->get(), ptr2);
    }

    EXPECT_FALSE(deleted2);
    EXPECT_EQ(2UL, set.size());
    OwnPtrSet::iterator it2 = set.find(ptr2);
    EXPECT_NE(set.end(), it2);
    EXPECT_EQ(ptr2, (*it2).get());

    set.remove(ptr1);
    EXPECT_TRUE(deleted1);

    set.clear();
    EXPECT_TRUE(deleted2);
    EXPECT_TRUE(set.isEmpty());

    deleted1 = false;
    deleted2 = false;
    {
        OwnPtrSet set;
        set.add(wrapUnique(new Dummy(deleted1)));
        set.add(wrapUnique(new Dummy(deleted2)));
    }
    EXPECT_TRUE(deleted1);
    EXPECT_TRUE(deleted2);

    deleted1 = false;
    deleted2 = false;
    std::unique_ptr<Dummy> ownPtr1;
    std::unique_ptr<Dummy> ownPtr2;
    ptr1 = new Dummy(deleted1);
    ptr2 = new Dummy(deleted2);
    {
        OwnPtrSet set;
        set.add(wrapUnique(ptr1));
        set.add(wrapUnique(ptr2));
        ownPtr1 = set.take(ptr1);
        EXPECT_EQ(1UL, set.size());
        ownPtr2 = set.takeAny();
        EXPECT_TRUE(set.isEmpty());
    }
    EXPECT_FALSE(deleted1);
    EXPECT_FALSE(deleted2);

    EXPECT_EQ(ptr1, ownPtr1.get());
    EXPECT_EQ(ptr2, ownPtr2.get());
}

class DummyRefCounted : public RefCounted<DummyRefCounted> {
public:
    DummyRefCounted(bool& isDeleted) : m_isDeleted(isDeleted) { m_isDeleted = false; }
    ~DummyRefCounted() { m_isDeleted = true; }

    void ref()
    {
        WTF::RefCounted<DummyRefCounted>::ref();
        ++s_refInvokesCount;
    }

    static int s_refInvokesCount;

private:
    bool& m_isDeleted;
};

int DummyRefCounted::s_refInvokesCount = 0;

TEST(HashSetTest, HashSetRefPtr)
{
    bool isDeleted = false;
    RefPtr<DummyRefCounted> ptr = adoptRef(new DummyRefCounted(isDeleted));
    EXPECT_EQ(0, DummyRefCounted::s_refInvokesCount);
    HashSet<RefPtr<DummyRefCounted>> set;
    set.add(ptr);
    // Referenced only once (to store a copy in the container).
    EXPECT_EQ(1, DummyRefCounted::s_refInvokesCount);

    DummyRefCounted* rawPtr = ptr.get();

    EXPECT_TRUE(set.contains(rawPtr));
    EXPECT_NE(set.end(), set.find(rawPtr));
    EXPECT_TRUE(set.contains(ptr));
    EXPECT_NE(set.end(), set.find(ptr));

    ptr.clear();
    EXPECT_FALSE(isDeleted);

    set.remove(rawPtr);
    EXPECT_TRUE(isDeleted);
    EXPECT_TRUE(set.isEmpty());
    EXPECT_EQ(1, DummyRefCounted::s_refInvokesCount);
}

class CountCopy final {
public:
    static int* const kDeletedValue;

    explicit CountCopy(int* counter = nullptr) : m_counter(counter) { }
    CountCopy(const CountCopy& other)
        : m_counter(other.m_counter)
    {
        if (m_counter && m_counter != kDeletedValue)
            ++*m_counter;
    }
    CountCopy& operator=(const CountCopy& other)
    {
        m_counter = other.m_counter;
        if (m_counter && m_counter != kDeletedValue)
            ++*m_counter;
        return *this;
    }
    const int* counter() const { return m_counter; }

private:
    int* m_counter;
};

int* const CountCopy::kDeletedValue = reinterpret_cast<int*>(static_cast<uintptr_t>(-1));

struct CountCopyHashTraits : public GenericHashTraits<CountCopy> {
    static const bool emptyValueIsZero = false;
    static const bool hasIsEmptyValueFunction = true;
    static bool isEmptyValue(const CountCopy& value) { return !value.counter(); }
    static void constructDeletedValue(CountCopy& slot, bool) { slot = CountCopy(CountCopy::kDeletedValue); }
    static bool isDeletedValue(const CountCopy& value) { return value.counter() == CountCopy::kDeletedValue; }
};

struct CountCopyHash : public PtrHash<const int*> {
    static unsigned hash(const CountCopy& value) { return PtrHash<const int>::hash(value.counter()); }
    static bool equal(const CountCopy& left, const CountCopy& right)
    {
        return PtrHash<const int>::equal(left.counter(), right.counter());
    }
    static const bool safeToCompareToEmptyOrDeleted = true;
};

} // anonymous namespace

template <>
struct HashTraits<CountCopy> : public CountCopyHashTraits { };

template <>
struct DefaultHash<CountCopy> {
    using Hash = CountCopyHash;
};

namespace {

TEST(HashSetTest, MoveShouldNotMakeCopy)
{
    HashSet<CountCopy> set;
    int counter = 0;
    set.add(CountCopy(&counter));

    HashSet<CountCopy> other(set);
    counter = 0;
    set = std::move(other);
    EXPECT_EQ(0, counter);

    counter = 0;
    HashSet<CountCopy> yetAnother(std::move(set));
    EXPECT_EQ(0, counter);
}

class MoveOnly {
public:
    // kEmpty and kDeleted have special meanings when MoveOnly is used as the key of a hash table.
    enum {
        kEmpty = 0,
        kDeleted = -1,
        kMovedOut = -2
    };

    explicit MoveOnly(int value = kEmpty, int id = 0) : m_value(value), m_id(id) { }
    MoveOnly(MoveOnly&& other)
        : m_value(other.m_value)
        , m_id(other.m_id)
    {
        other.m_value = kMovedOut;
        other.m_id = 0;
    }
    MoveOnly& operator=(MoveOnly&& other)
    {
        m_value = other.m_value;
        m_id = other.m_id;
        other.m_value = kMovedOut;
        other.m_id = 0;
        return *this;
    }

    int value() const { return m_value; }
    // id() is used for distinguishing MoveOnlys with the same value().
    int id() const { return m_id; }

private:
    MoveOnly(const MoveOnly&) = delete;
    MoveOnly& operator=(const MoveOnly&) = delete;

    int m_value;
    int m_id;
};

struct MoveOnlyHashTraits : public GenericHashTraits<MoveOnly> {
    // This is actually true, but we pretend that it's false to disable the optimization.
    static const bool emptyValueIsZero = false;

    static const bool hasIsEmptyValueFunction = true;
    static bool isEmptyValue(const MoveOnly& value) { return value.value() == MoveOnly::kEmpty; }
    static void constructDeletedValue(MoveOnly& slot, bool) { slot = MoveOnly(MoveOnly::kDeleted); }
    static bool isDeletedValue(const MoveOnly& value) { return value.value() == MoveOnly::kDeleted; }
};

struct MoveOnlyHash {
    static unsigned hash(const MoveOnly& value) { return DefaultHash<int>::Hash::hash(value.value()); }
    static bool equal(const MoveOnly& left, const MoveOnly& right)
    {
        return DefaultHash<int>::Hash::equal(left.value(), right.value());
    }
    static const bool safeToCompareToEmptyOrDeleted = true;
};

} // anonymous namespace

template <>
struct HashTraits<MoveOnly> : public MoveOnlyHashTraits { };

template <>
struct DefaultHash<MoveOnly> {
    using Hash = MoveOnlyHash;
};

namespace {

TEST(HashSetTest, MoveOnlyValue)
{
    using TheSet = HashSet<MoveOnly>;
    TheSet set;
    {
        TheSet::AddResult addResult = set.add(MoveOnly(1, 1));
        EXPECT_TRUE(addResult.isNewEntry);
        EXPECT_EQ(1, addResult.storedValue->value());
        EXPECT_EQ(1, addResult.storedValue->id());
    }
    auto iter = set.find(MoveOnly(1));
    ASSERT_TRUE(iter != set.end());
    EXPECT_EQ(1, iter->value());

    iter = set.find(MoveOnly(2));
    EXPECT_TRUE(iter == set.end());

    for (int i = 2; i < 32; ++i) {
        TheSet::AddResult addResult = set.add(MoveOnly(i, i));
        EXPECT_TRUE(addResult.isNewEntry);
        EXPECT_EQ(i, addResult.storedValue->value());
        EXPECT_EQ(i, addResult.storedValue->id());
    }

    iter = set.find(MoveOnly(1));
    ASSERT_TRUE(iter != set.end());
    EXPECT_EQ(1, iter->value());
    EXPECT_EQ(1, iter->id());

    iter = set.find(MoveOnly(7));
    ASSERT_TRUE(iter != set.end());
    EXPECT_EQ(7, iter->value());
    EXPECT_EQ(7, iter->id());

    {
        TheSet::AddResult addResult = set.add(MoveOnly(7, 777)); // With different ID for identification.
        EXPECT_FALSE(addResult.isNewEntry);
        EXPECT_EQ(7, addResult.storedValue->value());
        EXPECT_EQ(7, addResult.storedValue->id());
    }

    set.remove(MoveOnly(11));
    iter = set.find(MoveOnly(11));
    EXPECT_TRUE(iter == set.end());

    MoveOnly thirteen(set.take(MoveOnly(13)));
    EXPECT_EQ(13, thirteen.value());
    EXPECT_EQ(13, thirteen.id());
    iter = set.find(MoveOnly(13));
    EXPECT_TRUE(iter == set.end());

    set.clear();
}

TEST(HashSetTest, UniquePtr)
{
    using Pointer = std::unique_ptr<int>;
    using Set = HashSet<Pointer>;
    Set set;
    int* onePointer = new int(1);
    {
        Set::AddResult addResult = set.add(Pointer(onePointer));
        EXPECT_TRUE(addResult.isNewEntry);
        EXPECT_EQ(onePointer, addResult.storedValue->get());
        EXPECT_EQ(1, **addResult.storedValue);
    }
    auto iter = set.find(onePointer);
    ASSERT_TRUE(iter != set.end());
    EXPECT_EQ(onePointer, iter->get());

    Pointer nonexistent(new int(42));
    iter = set.find(nonexistent.get());
    EXPECT_TRUE(iter == set.end());

    // Insert more to cause a rehash.
    for (int i = 2; i < 32; ++i) {
        Set::AddResult addResult = set.add(Pointer(new int(i)));
        EXPECT_TRUE(addResult.isNewEntry);
        EXPECT_EQ(i, **addResult.storedValue);
    }

    iter = set.find(onePointer);
    ASSERT_TRUE(iter != set.end());
    EXPECT_EQ(onePointer, iter->get());

    Pointer one(set.take(onePointer));
    ASSERT_TRUE(one);
    EXPECT_EQ(onePointer, one.get());

    Pointer empty(set.take(nonexistent.get()));
    EXPECT_TRUE(!empty);

    iter = set.find(onePointer);
    EXPECT_TRUE(iter == set.end());

    // Re-insert to the deleted slot.
    {
        Set::AddResult addResult = set.add(std::move(one));
        EXPECT_TRUE(addResult.isNewEntry);
        EXPECT_EQ(onePointer, addResult.storedValue->get());
        EXPECT_EQ(1, **addResult.storedValue);
    }
}

bool isOneTwoThree(const HashSet<int>& set)
{
    return set.size() == 3 && set.contains(1) && set.contains(2) && set.contains(3);
}

HashSet<int> returnOneTwoThree()
{
    return {1, 2, 3};
}

TEST(HashSetTest, InitializerList)
{
    HashSet<int> empty({});
    EXPECT_TRUE(empty.isEmpty());

    HashSet<int> one({1});
    EXPECT_EQ(1u, one.size());
    EXPECT_TRUE(one.contains(1));

    HashSet<int> oneTwoThree({1, 2, 3});
    EXPECT_EQ(3u, oneTwoThree.size());
    EXPECT_TRUE(oneTwoThree.contains(1));
    EXPECT_TRUE(oneTwoThree.contains(2));
    EXPECT_TRUE(oneTwoThree.contains(3));

    // Put some jank so we can check if the assignments later can clear them.
    empty.add(9999);
    one.add(9999);
    oneTwoThree.add(9999);

    empty = {};
    EXPECT_TRUE(empty.isEmpty());

    one = {1};
    EXPECT_EQ(1u, one.size());
    EXPECT_TRUE(one.contains(1));

    oneTwoThree = {1, 2, 3};
    EXPECT_EQ(3u, oneTwoThree.size());
    EXPECT_TRUE(oneTwoThree.contains(1));
    EXPECT_TRUE(oneTwoThree.contains(2));
    EXPECT_TRUE(oneTwoThree.contains(3));

    oneTwoThree = {3, 1, 1, 2, 1, 1, 3};
    EXPECT_EQ(3u, oneTwoThree.size());
    EXPECT_TRUE(oneTwoThree.contains(1));
    EXPECT_TRUE(oneTwoThree.contains(2));
    EXPECT_TRUE(oneTwoThree.contains(3));

    // Other ways of construction: as a function parameter and in a return statement.
    EXPECT_TRUE(isOneTwoThree({1, 2, 3}));
    EXPECT_TRUE(isOneTwoThree(returnOneTwoThree()));
}

} // anonymous namespace

} // namespace WTF
