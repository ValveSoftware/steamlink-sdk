/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "wtf/HashMap.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/PassRefPtr.h"
#include "wtf/PtrUtil.h"
#include "wtf/RefCounted.h"
#include "wtf/Vector.h"
#include <memory>

namespace WTF {

namespace {

using IntHashMap = HashMap<int, int>;

TEST(HashMapTest, IteratorComparison)
{
    IntHashMap map;
    map.add(1, 2);
    EXPECT_TRUE(map.begin() != map.end());
    EXPECT_FALSE(map.begin() == map.end());

    IntHashMap::const_iterator begin = map.begin();
    EXPECT_TRUE(begin == map.begin());
    EXPECT_TRUE(map.begin() == begin);
    EXPECT_TRUE(begin != map.end());
    EXPECT_TRUE(map.end() != begin);
    EXPECT_FALSE(begin != map.begin());
    EXPECT_FALSE(map.begin() != begin);
    EXPECT_FALSE(begin == map.end());
    EXPECT_FALSE(map.end() == begin);
}

struct TestDoubleHashTraits : HashTraits<double> {
    static const unsigned minimumTableSize = 8;
};

using DoubleHashMap = HashMap<double, int64_t, DefaultHash<double>::Hash, TestDoubleHashTraits>;

int bucketForKey(double key)
{
    return DefaultHash<double>::Hash::hash(key) & (TestDoubleHashTraits::minimumTableSize - 1);
}

TEST(HashMapTest, DoubleHashCollisions)
{
    // The "clobber" key here is one that ends up stealing the bucket that the -0 key
    // originally wants to be in. This makes the 0 and -0 keys collide and the test then
    // fails unless the FloatHash::equals() implementation can distinguish them.
    const double clobberKey = 6;
    const double zeroKey = 0;
    const double negativeZeroKey = -zeroKey;

    DoubleHashMap map;

    map.add(clobberKey, 1);
    map.add(zeroKey, 2);
    map.add(negativeZeroKey, 3);

    EXPECT_EQ(bucketForKey(clobberKey), bucketForKey(negativeZeroKey));
    EXPECT_EQ(1, map.get(clobberKey));
    EXPECT_EQ(2, map.get(zeroKey));
    EXPECT_EQ(3, map.get(negativeZeroKey));
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

using OwnPtrHashMap = HashMap<int, std::unique_ptr<DestructCounter>>;

TEST(HashMapTest, OwnPtrAsValue)
{
    int destructNumber = 0;
    OwnPtrHashMap map;
    map.add(1, wrapUnique(new DestructCounter(1, &destructNumber)));
    map.add(2, wrapUnique(new DestructCounter(2, &destructNumber)));

    DestructCounter* counter1 = map.get(1);
    EXPECT_EQ(1, counter1->get());
    DestructCounter* counter2 = map.get(2);
    EXPECT_EQ(2, counter2->get());
    EXPECT_EQ(0, destructNumber);

    for (OwnPtrHashMap::iterator iter = map.begin(); iter != map.end(); ++iter) {
        std::unique_ptr<DestructCounter>& ownCounter = iter->value;
        EXPECT_EQ(iter->key, ownCounter->get());
    }
    ASSERT_EQ(0, destructNumber);

    std::unique_ptr<DestructCounter> ownCounter1 = map.take(1);
    EXPECT_EQ(ownCounter1.get(), counter1);
    EXPECT_FALSE(map.contains(1));
    EXPECT_EQ(0, destructNumber);

    map.remove(2);
    EXPECT_FALSE(map.contains(2));
    EXPECT_EQ(0UL, map.size());
    EXPECT_EQ(1, destructNumber);

    ownCounter1.reset();
    EXPECT_EQ(2, destructNumber);
}

class DummyRefCounted : public RefCounted<DummyRefCounted> {
public:
    DummyRefCounted(bool& isDeleted) : m_isDeleted(isDeleted) { m_isDeleted = false; }
    ~DummyRefCounted()
    {
        ASSERT(!m_isDeleted);
        m_isDeleted = true;
    }

    void ref()
    {
        ASSERT(!m_isDeleted);
        WTF::RefCounted<DummyRefCounted>::ref();
        ++m_refInvokesCount;
    }

    void deref()
    {
        ASSERT(!m_isDeleted);
        WTF::RefCounted<DummyRefCounted>::deref();
    }

    static int m_refInvokesCount;

private:
    bool& m_isDeleted;
};

int DummyRefCounted::m_refInvokesCount = 0;

TEST(HashMapTest, RefPtrAsKey)
{
    bool isDeleted = false;
    DummyRefCounted::m_refInvokesCount = 0;
    RefPtr<DummyRefCounted> ptr = adoptRef(new DummyRefCounted(isDeleted));
    EXPECT_EQ(0, DummyRefCounted::m_refInvokesCount);
    HashMap<RefPtr<DummyRefCounted>, int> map;
    map.add(ptr, 1);
    // Referenced only once (to store a copy in the container).
    EXPECT_EQ(1, DummyRefCounted::m_refInvokesCount);
    EXPECT_EQ(1, map.get(ptr));

    DummyRefCounted* rawPtr = ptr.get();

    EXPECT_TRUE(map.contains(rawPtr));
    EXPECT_NE(map.end(), map.find(rawPtr));
    EXPECT_TRUE(map.contains(ptr));
    EXPECT_NE(map.end(), map.find(ptr));
    EXPECT_EQ(1, DummyRefCounted::m_refInvokesCount);

    ptr.clear();
    EXPECT_FALSE(isDeleted);

    map.remove(rawPtr);
    EXPECT_EQ(1, DummyRefCounted::m_refInvokesCount);
    EXPECT_TRUE(isDeleted);
    EXPECT_TRUE(map.isEmpty());
}

TEST(HashMaptest, RemoveAdd)
{
    DummyRefCounted::m_refInvokesCount = 0;
    bool isDeleted = false;

    typedef HashMap<int, RefPtr<DummyRefCounted>> Map;
    Map map;

    RefPtr<DummyRefCounted> ptr = adoptRef(new DummyRefCounted(isDeleted));
    EXPECT_EQ(0, DummyRefCounted::m_refInvokesCount);

    map.add(1, ptr);
    // Referenced only once (to store a copy in the container).
    EXPECT_EQ(1, DummyRefCounted::m_refInvokesCount);
    EXPECT_EQ(ptr, map.get(1));

    ptr.clear();
    EXPECT_FALSE(isDeleted);

    map.remove(1);
    EXPECT_EQ(1, DummyRefCounted::m_refInvokesCount);
    EXPECT_TRUE(isDeleted);
    EXPECT_TRUE(map.isEmpty());

    // Add and remove until the deleted slot is reused.
    for (int i = 1; i < 100; i++) {
        bool isDeleted2 = false;
        RefPtr<DummyRefCounted> ptr2 = adoptRef(new DummyRefCounted(isDeleted2));
        map.add(i, ptr2);
        EXPECT_FALSE(isDeleted2);
        ptr2.clear();
        EXPECT_FALSE(isDeleted2);
        map.remove(i);
        EXPECT_TRUE(isDeleted2);
    }
}

class SimpleClass {
public:
    explicit SimpleClass(int v) : m_v(v) { }
    int v() { return m_v; }

private:
    int m_v;
};
using IntSimpleMap = HashMap<int, std::unique_ptr<SimpleClass>>;

TEST(HashMapTest, AddResult)
{
    IntSimpleMap map;
    IntSimpleMap::AddResult result = map.add(1, nullptr);
    EXPECT_TRUE(result.isNewEntry);
    EXPECT_EQ(1, result.storedValue->key);
    EXPECT_EQ(0, result.storedValue->value.get());

    SimpleClass* simple1 = new SimpleClass(1);
    result.storedValue->value = wrapUnique(simple1);
    EXPECT_EQ(simple1, map.get(1));

    IntSimpleMap::AddResult result2 = map.add(1, wrapUnique(new SimpleClass(2)));
    EXPECT_FALSE(result2.isNewEntry);
    EXPECT_EQ(1, result.storedValue->key);
    EXPECT_EQ(1, result.storedValue->value->v());
    EXPECT_EQ(1, map.get(1)->v());
}

TEST(HashMapTest, AddResultVectorValue)
{
    using IntVectorMap = HashMap<int, Vector<int>>;
    IntVectorMap map;
    IntVectorMap::AddResult result = map.add(1, Vector<int>());
    EXPECT_TRUE(result.isNewEntry);
    EXPECT_EQ(1, result.storedValue->key);
    EXPECT_EQ(0u, result.storedValue->value.size());

    result.storedValue->value.append(11);
    EXPECT_EQ(1u, map.find(1)->value.size());
    EXPECT_EQ(11, map.find(1)->value.first());

    IntVectorMap::AddResult result2 = map.add(1, Vector<int>());
    EXPECT_FALSE(result2.isNewEntry);
    EXPECT_EQ(1, result.storedValue->key);
    EXPECT_EQ(1u, result.storedValue->value.size());
    EXPECT_EQ(11, result.storedValue->value.first());
    EXPECT_EQ(11, map.find(1)->value.first());
}

class InstanceCounter {
public:
    InstanceCounter() { ++counter; }
    InstanceCounter(const InstanceCounter& another) { ++counter; }
    ~InstanceCounter() { --counter; }
    static int counter;
};
int InstanceCounter::counter = 0;

TEST(HashMapTest, ValueTypeDestructed)
{
    InstanceCounter::counter = 0;
    HashMap<int, InstanceCounter> map;
    map.set(1, InstanceCounter());
    map.clear();
    EXPECT_EQ(0, InstanceCounter::counter);
}

class MoveOnly {
public:
    // kEmpty and kDeleted have special meanings when MoveOnly is used as the key of a hash table.
    enum {
        kEmpty = 0,
        kDeleted = -1,
        kMovedOut = -2
    };

    explicit MoveOnly(int value = kEmpty) : m_value(value) { }
    MoveOnly(MoveOnly&& other)
        : m_value(other.m_value)
    {
        other.m_value = kMovedOut;
    }
    MoveOnly& operator=(MoveOnly&& other)
    {
        m_value = other.m_value;
        other.m_value = kMovedOut;
        return *this;
    }

    int value() const { return m_value; }

private:
    MoveOnly(const MoveOnly&) = delete;
    MoveOnly& operator=(const MoveOnly&) = delete;

    int m_value;
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

TEST(HashMapTest, MoveOnlyValueType)
{
    using TheMap = HashMap<int, MoveOnly>;
    TheMap map;
    {
        TheMap::AddResult addResult = map.add(1, MoveOnly(10));
        EXPECT_TRUE(addResult.isNewEntry);
        EXPECT_EQ(1, addResult.storedValue->key);
        EXPECT_EQ(10, addResult.storedValue->value.value());
    }
    auto iter = map.find(1);
    ASSERT_TRUE(iter != map.end());
    EXPECT_EQ(1, iter->key);
    EXPECT_EQ(10, iter->value.value());

    iter = map.find(2);
    EXPECT_TRUE(iter == map.end());

    // Try to add more to trigger rehashing.
    for (int i = 2; i < 32; ++i) {
        TheMap::AddResult addResult = map.add(i, MoveOnly(i * 10));
        EXPECT_TRUE(addResult.isNewEntry);
        EXPECT_EQ(i, addResult.storedValue->key);
        EXPECT_EQ(i * 10, addResult.storedValue->value.value());
    }

    iter = map.find(1);
    ASSERT_TRUE(iter != map.end());
    EXPECT_EQ(1, iter->key);
    EXPECT_EQ(10, iter->value.value());

    iter = map.find(7);
    ASSERT_TRUE(iter != map.end());
    EXPECT_EQ(7, iter->key);
    EXPECT_EQ(70, iter->value.value());

    {
        TheMap::AddResult addResult = map.set(9, MoveOnly(999));
        EXPECT_FALSE(addResult.isNewEntry);
        EXPECT_EQ(9, addResult.storedValue->key);
        EXPECT_EQ(999, addResult.storedValue->value.value());
    }

    map.remove(11);
    iter = map.find(11);
    EXPECT_TRUE(iter == map.end());

    MoveOnly oneThirty(map.take(13));
    EXPECT_EQ(130, oneThirty.value());
    iter = map.find(13);
    EXPECT_TRUE(iter == map.end());

    map.clear();
}

TEST(HashMapTest, MoveOnlyKeyType)
{
    // The content of this test is similar to the test above, except that the types of key and value are swapped.
    using TheMap = HashMap<MoveOnly, int>;
    TheMap map;
    {
        TheMap::AddResult addResult = map.add(MoveOnly(1), 10);
        EXPECT_TRUE(addResult.isNewEntry);
        EXPECT_EQ(1, addResult.storedValue->key.value());
        EXPECT_EQ(10, addResult.storedValue->value);
    }
    auto iter = map.find(MoveOnly(1));
    ASSERT_TRUE(iter != map.end());
    EXPECT_EQ(1, iter->key.value());
    EXPECT_EQ(10, iter->value);

    iter = map.find(MoveOnly(2));
    EXPECT_TRUE(iter == map.end());

    for (int i = 2; i < 32; ++i) {
        TheMap::AddResult addResult = map.add(MoveOnly(i), i * 10);
        EXPECT_TRUE(addResult.isNewEntry);
        EXPECT_EQ(i, addResult.storedValue->key.value());
        EXPECT_EQ(i * 10, addResult.storedValue->value);
    }

    iter = map.find(MoveOnly(1));
    ASSERT_TRUE(iter != map.end());
    EXPECT_EQ(1, iter->key.value());
    EXPECT_EQ(10, iter->value);

    iter = map.find(MoveOnly(7));
    ASSERT_TRUE(iter != map.end());
    EXPECT_EQ(7, iter->key.value());
    EXPECT_EQ(70, iter->value);

    {
        TheMap::AddResult addResult = map.set(MoveOnly(9), 999);
        EXPECT_FALSE(addResult.isNewEntry);
        EXPECT_EQ(9, addResult.storedValue->key.value());
        EXPECT_EQ(999, addResult.storedValue->value);
    }

    map.remove(MoveOnly(11));
    iter = map.find(MoveOnly(11));
    EXPECT_TRUE(iter == map.end());

    int oneThirty = map.take(MoveOnly(13));
    EXPECT_EQ(130, oneThirty);
    iter = map.find(MoveOnly(13));
    EXPECT_TRUE(iter == map.end());

    map.clear();
}

class CountCopy final {
public:
    CountCopy() : m_counter(nullptr) { }
    explicit CountCopy(int& counter) : m_counter(&counter) { }
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

TEST(HashMapTest, MoveShouldNotMakeCopy)
{
    HashMap<int, CountCopy> map;
    int counter = 0;
    map.add(1, CountCopy(counter));

    HashMap<int, CountCopy> other(map);
    counter = 0;
    map = std::move(other);
    EXPECT_EQ(0, counter);

    counter = 0;
    HashMap<int, CountCopy> yetAnother(std::move(map));
    EXPECT_EQ(0, counter);
}

TEST(HashMapTest, UniquePtrAsKey)
{
    using Pointer = std::unique_ptr<int>;
    using Map = HashMap<Pointer, int>;
    Map map;
    int* onePointer = new int(1);
    {
        Map::AddResult addResult = map.add(Pointer(onePointer), 1);
        EXPECT_TRUE(addResult.isNewEntry);
        EXPECT_EQ(onePointer, addResult.storedValue->key.get());
        EXPECT_EQ(1, *addResult.storedValue->key);
        EXPECT_EQ(1, addResult.storedValue->value);
    }
    auto iter = map.find(onePointer);
    ASSERT_TRUE(iter != map.end());
    EXPECT_EQ(onePointer, iter->key.get());
    EXPECT_EQ(1, iter->value);

    Pointer nonexistent(new int(42));
    iter = map.find(nonexistent.get());
    EXPECT_TRUE(iter == map.end());

    // Insert more to cause a rehash.
    for (int i = 2; i < 32; ++i) {
        Map::AddResult addResult = map.add(Pointer(new int(i)), i);
        EXPECT_TRUE(addResult.isNewEntry);
        EXPECT_EQ(i, *addResult.storedValue->key);
        EXPECT_EQ(i, addResult.storedValue->value);
    }

    iter = map.find(onePointer);
    ASSERT_TRUE(iter != map.end());
    EXPECT_EQ(onePointer, iter->key.get());
    EXPECT_EQ(1, iter->value);

    EXPECT_EQ(1, map.take(onePointer));
    // From now on, |onePointer| is a dangling pointer.

    iter = map.find(onePointer);
    EXPECT_TRUE(iter == map.end());
}

TEST(HashMapTest, UniquePtrAsValue)
{
    using Pointer = std::unique_ptr<int>;
    using Map = HashMap<int, Pointer>;
    Map map;
    {
        Map::AddResult addResult = map.add(1, Pointer(new int(1)));
        EXPECT_TRUE(addResult.isNewEntry);
        EXPECT_EQ(1, addResult.storedValue->key);
        EXPECT_EQ(1, *addResult.storedValue->value);
    }
    auto iter = map.find(1);
    ASSERT_TRUE(iter != map.end());
    EXPECT_EQ(1, iter->key);
    EXPECT_EQ(1, *iter->value);

    int* onePointer = map.get(1);
    EXPECT_TRUE(onePointer);
    EXPECT_EQ(1, *onePointer);

    iter = map.find(42);
    EXPECT_TRUE(iter == map.end());

    for (int i = 2; i < 32; ++i) {
        Map::AddResult addResult = map.add(i, Pointer(new int(i)));
        EXPECT_TRUE(addResult.isNewEntry);
        EXPECT_EQ(i, addResult.storedValue->key);
        EXPECT_EQ(i, *addResult.storedValue->value);
    }

    iter = map.find(1);
    ASSERT_TRUE(iter != map.end());
    EXPECT_EQ(1, iter->key);
    EXPECT_EQ(1, *iter->value);

    Pointer one(map.take(1));
    ASSERT_TRUE(one);
    EXPECT_EQ(1, *one);

    Pointer empty(map.take(42));
    EXPECT_TRUE(!empty);

    iter = map.find(1);
    EXPECT_TRUE(iter == map.end());

    {
        Map::AddResult addResult = map.add(1, std::move(one));
        EXPECT_TRUE(addResult.isNewEntry);
        EXPECT_EQ(1, addResult.storedValue->key);
        EXPECT_EQ(1, *addResult.storedValue->value);
    }
}

TEST(HashMapTest, MoveOnlyPairKeyType)
{
    using Pair = std::pair<MoveOnly, int>;
    using TheMap = HashMap<Pair, int>;
    TheMap map;
    {
        TheMap::AddResult addResult = map.add(Pair(MoveOnly(1), -1), 10);
        EXPECT_TRUE(addResult.isNewEntry);
        EXPECT_EQ(1, addResult.storedValue->key.first.value());
        EXPECT_EQ(-1, addResult.storedValue->key.second);
        EXPECT_EQ(10, addResult.storedValue->value);
    }
    auto iter = map.find(Pair(MoveOnly(1), -1));
    ASSERT_TRUE(iter != map.end());
    EXPECT_EQ(1, iter->key.first.value());
    EXPECT_EQ(-1, iter->key.second);
    EXPECT_EQ(10, iter->value);

    iter = map.find(Pair(MoveOnly(1), 0));
    EXPECT_TRUE(iter == map.end());

    for (int i = 2; i < 32; ++i) {
        TheMap::AddResult addResult = map.add(Pair(MoveOnly(i), -i), i * 10);
        EXPECT_TRUE(addResult.isNewEntry);
        EXPECT_EQ(i, addResult.storedValue->key.first.value());
        EXPECT_EQ(-i, addResult.storedValue->key.second);
        EXPECT_EQ(i * 10, addResult.storedValue->value);
    }

    iter = map.find(Pair(MoveOnly(1), -1));
    ASSERT_TRUE(iter != map.end());
    EXPECT_EQ(1, iter->key.first.value());
    EXPECT_EQ(-1, iter->key.second);
    EXPECT_EQ(10, iter->value);

    iter = map.find(Pair(MoveOnly(7), -7));
    ASSERT_TRUE(iter != map.end());
    EXPECT_EQ(7, iter->key.first.value());
    EXPECT_EQ(-7, iter->key.second);
    EXPECT_EQ(70, iter->value);

    {
        TheMap::AddResult addResult = map.set(Pair(MoveOnly(9), -9), 999);
        EXPECT_FALSE(addResult.isNewEntry);
        EXPECT_EQ(9, addResult.storedValue->key.first.value());
        EXPECT_EQ(-9, addResult.storedValue->key.second);
        EXPECT_EQ(999, addResult.storedValue->value);
    }

    map.remove(Pair(MoveOnly(11), -11));
    iter = map.find(Pair(MoveOnly(11), -11));
    EXPECT_TRUE(iter == map.end());

    int oneThirty = map.take(Pair(MoveOnly(13), -13));
    EXPECT_EQ(130, oneThirty);
    iter = map.find(Pair(MoveOnly(13), -13));
    EXPECT_TRUE(iter == map.end());

    map.clear();
}

} // anonymous namespace

} // namespace WTF
