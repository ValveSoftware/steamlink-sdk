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

#include "wtf/ListHashSet.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/LinkedHashSet.h"
#include "wtf/PassRefPtr.h"
#include "wtf/PtrUtil.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include <memory>
#include <type_traits>

namespace WTF {

namespace {

template <typename Set>
class ListOrLinkedHashSetTest : public ::testing::Test { };

using SetTypes = ::testing::Types<ListHashSet<int>, ListHashSet<int, 1>, LinkedHashSet<int>>;
TYPED_TEST_CASE(ListOrLinkedHashSetTest, SetTypes);

TYPED_TEST(ListOrLinkedHashSetTest, RemoveFirst)
{
    using Set = TypeParam;
    Set list;
    list.add(-1);
    list.add(0);
    list.add(1);
    list.add(2);
    list.add(3);

    EXPECT_EQ(-1, list.first());
    EXPECT_EQ(3, list.last());

    list.removeFirst();
    EXPECT_EQ(0, list.first());

    list.removeLast();
    EXPECT_EQ(2, list.last());

    list.removeFirst();
    EXPECT_EQ(1, list.first());

    list.removeFirst();
    EXPECT_EQ(2, list.first());

    list.removeFirst();
    EXPECT_TRUE(list.isEmpty());
}

TYPED_TEST(ListOrLinkedHashSetTest, AppendOrMoveToLastNewItems)
{
    using Set = TypeParam;
    Set list;
    typename Set::AddResult result = list.appendOrMoveToLast(1);
    EXPECT_TRUE(result.isNewEntry);
    result = list.add(2);
    EXPECT_TRUE(result.isNewEntry);
    result = list.appendOrMoveToLast(3);
    EXPECT_TRUE(result.isNewEntry);

    EXPECT_EQ(list.size(), 3UL);

    // The list should be in order 1, 2, 3.
    typename Set::iterator iterator = list.begin();
    EXPECT_EQ(1, *iterator);
    ++iterator;
    EXPECT_EQ(2, *iterator);
    ++iterator;
    EXPECT_EQ(3, *iterator);
    ++iterator;
}

TYPED_TEST(ListOrLinkedHashSetTest, AppendOrMoveToLastWithDuplicates)
{
    using Set = TypeParam;
    Set list;

    // Add a single element twice.
    typename Set::AddResult result = list.add(1);
    EXPECT_TRUE(result.isNewEntry);
    result = list.appendOrMoveToLast(1);
    EXPECT_FALSE(result.isNewEntry);
    EXPECT_EQ(1UL, list.size());

    list.add(2);
    list.add(3);
    EXPECT_EQ(3UL, list.size());

    // Appending 2 move it to the end.
    EXPECT_EQ(3, list.last());
    result = list.appendOrMoveToLast(2);
    EXPECT_FALSE(result.isNewEntry);
    EXPECT_EQ(2, list.last());

    // Inverse the list by moving each element to end end.
    result = list.appendOrMoveToLast(3);
    EXPECT_FALSE(result.isNewEntry);
    result = list.appendOrMoveToLast(2);
    EXPECT_FALSE(result.isNewEntry);
    result = list.appendOrMoveToLast(1);
    EXPECT_FALSE(result.isNewEntry);
    EXPECT_EQ(3UL, list.size());

    typename Set::iterator iterator = list.begin();
    EXPECT_EQ(3, *iterator);
    ++iterator;
    EXPECT_EQ(2, *iterator);
    ++iterator;
    EXPECT_EQ(1, *iterator);
    ++iterator;
}

TYPED_TEST(ListOrLinkedHashSetTest, PrependOrMoveToFirstNewItems)
{
    using Set = TypeParam;
    Set list;
    typename Set::AddResult result = list.prependOrMoveToFirst(1);
    EXPECT_TRUE(result.isNewEntry);
    result = list.add(2);
    EXPECT_TRUE(result.isNewEntry);
    result = list.prependOrMoveToFirst(3);
    EXPECT_TRUE(result.isNewEntry);

    EXPECT_EQ(list.size(), 3UL);

    // The list should be in order 3, 1, 2.
    typename Set::iterator iterator = list.begin();
    EXPECT_EQ(3, *iterator);
    ++iterator;
    EXPECT_EQ(1, *iterator);
    ++iterator;
    EXPECT_EQ(2, *iterator);
    ++iterator;
}

TYPED_TEST(ListOrLinkedHashSetTest, PrependOrMoveToLastWithDuplicates)
{
    using Set = TypeParam;
    Set list;

    // Add a single element twice.
    typename Set::AddResult result = list.add(1);
    EXPECT_TRUE(result.isNewEntry);
    result = list.prependOrMoveToFirst(1);
    EXPECT_FALSE(result.isNewEntry);
    EXPECT_EQ(1UL, list.size());

    list.add(2);
    list.add(3);
    EXPECT_EQ(3UL, list.size());

    // Prepending 2 move it to the beginning.
    EXPECT_EQ(1, list.first());
    result = list.prependOrMoveToFirst(2);
    EXPECT_FALSE(result.isNewEntry);
    EXPECT_EQ(2, list.first());

    // Inverse the list by moving each element to the first position.
    result = list.prependOrMoveToFirst(1);
    EXPECT_FALSE(result.isNewEntry);
    result = list.prependOrMoveToFirst(2);
    EXPECT_FALSE(result.isNewEntry);
    result = list.prependOrMoveToFirst(3);
    EXPECT_FALSE(result.isNewEntry);
    EXPECT_EQ(3UL, list.size());

    typename Set::iterator iterator = list.begin();
    EXPECT_EQ(3, *iterator);
    ++iterator;
    EXPECT_EQ(2, *iterator);
    ++iterator;
    EXPECT_EQ(1, *iterator);
    ++iterator;
}

TYPED_TEST(ListOrLinkedHashSetTest, Find)
{
    using Set = TypeParam;
    Set set;
    set.add(-1);
    set.add(0);
    set.add(1);
    set.add(2);
    set.add(3);

    {
        const Set& ref = set;
        typename Set::const_iterator it = ref.find(2);
        EXPECT_EQ(2, *it);
        ++it;
        EXPECT_EQ(3, *it);
        --it;
        --it;
        EXPECT_EQ(1, *it);
    }
    {
        Set& ref = set;
        typename Set::iterator it = ref.find(2);
        EXPECT_EQ(2, *it);
        ++it;
        EXPECT_EQ(3, *it);
        --it;
        --it;
        EXPECT_EQ(1, *it);
    }
}

TYPED_TEST(ListOrLinkedHashSetTest, InsertBefore)
{
    using Set = TypeParam;
    bool canModifyWhileIterating = !std::is_same<Set, LinkedHashSet<int>>::value;
    Set set;
    set.add(-1);
    set.add(0);
    set.add(2);
    set.add(3);

    typename Set::iterator it = set.find(2);
    EXPECT_EQ(2, *it);
    set.insertBefore(it, 1);
    if (!canModifyWhileIterating)
        it = set.find(2);
    ++it;
    EXPECT_EQ(3, *it);
    EXPECT_EQ(5u, set.size());
    --it;
    --it;
    EXPECT_EQ(1, *it);
    if (canModifyWhileIterating) {
        set.remove(-1);
        set.remove(0);
        set.remove(2);
        set.remove(3);
        EXPECT_EQ(1u, set.size());
        EXPECT_EQ(1, *it);
        ++it;
        EXPECT_EQ(it, set.end());
        --it;
        EXPECT_EQ(1, *it);
        set.insertBefore(it, -1);
        set.insertBefore(it, 0);
        set.add(2);
        set.add(3);
    }
    set.insertBefore(2, 42);
    set.insertBefore(-1, 103);
    EXPECT_EQ(103, set.first());
    if (!canModifyWhileIterating)
        it = set.find(1);
    ++it;
    EXPECT_EQ(42, *it);
    EXPECT_EQ(7u, set.size());
}

TYPED_TEST(ListOrLinkedHashSetTest, AddReturnIterator)
{
    using Set = TypeParam;
    bool canModifyWhileIterating = !std::is_same<Set, LinkedHashSet<int>>::value;
    Set set;
    set.add(-1);
    set.add(0);
    set.add(1);
    set.add(2);

    typename Set::iterator it = set.addReturnIterator(3);
    EXPECT_EQ(3, *it);
    --it;
    EXPECT_EQ(2, *it);
    EXPECT_EQ(5u, set.size());
    --it;
    EXPECT_EQ(1, *it);
    --it;
    EXPECT_EQ(0, *it);
    it = set.addReturnIterator(4);
    if (canModifyWhileIterating) {
        set.remove(3);
        set.remove(2);
        set.remove(1);
        set.remove(0);
        set.remove(-1);
        EXPECT_EQ(1u, set.size());
    }
    EXPECT_EQ(4, *it);
    ++it;
    EXPECT_EQ(it, set.end());
    --it;
    EXPECT_EQ(4, *it);
    if (canModifyWhileIterating) {
        set.insertBefore(it, -1);
        set.insertBefore(it, 0);
        set.insertBefore(it, 1);
        set.insertBefore(it, 2);
        set.insertBefore(it, 3);
    }
    EXPECT_EQ(6u, set.size());
    it = set.addReturnIterator(5);
    EXPECT_EQ(7u, set.size());
    set.remove(it);
    EXPECT_EQ(6u, set.size());
    EXPECT_EQ(4, set.last());
}

TYPED_TEST(ListOrLinkedHashSetTest, Swap)
{
    using Set = TypeParam;
    int num = 10;
    Set set0;
    Set set1;
    Set set2;
    for (int i = 0; i < num; ++i) {
        set1.add(i + 1);
        set2.add(num - i);
    }

    typename Set::iterator it1 = set1.begin();
    typename Set::iterator it2 = set2.begin();
    for (int i = 0; i < num; ++i, ++it1, ++it2) {
        EXPECT_EQ(*it1, i + 1);
        EXPECT_EQ(*it2, num - i);
    }
    EXPECT_EQ(set0.begin(), set0.end());
    EXPECT_EQ(it1, set1.end());
    EXPECT_EQ(it2, set2.end());

    // Shift sets: 2->1, 1->0, 0->2
    set1.swap(set2); // Swap with non-empty sets.
    set0.swap(set2); // Swap with an empty set.

    it1 = set0.begin();
    it2 = set1.begin();
    for (int i = 0; i < num; ++i, ++it1, ++it2) {
        EXPECT_EQ(*it1, i + 1);
        EXPECT_EQ(*it2, num - i);
    }
    EXPECT_EQ(it1, set0.end());
    EXPECT_EQ(it2, set1.end());
    EXPECT_EQ(set2.begin(), set2.end());

    int removedIndex = num >> 1;
    set0.remove(removedIndex + 1);
    set1.remove(num - removedIndex);

    it1 = set0.begin();
    it2 = set1.begin();
    for (int i = 0; i < num; ++i, ++it1, ++it2) {
        if (i == removedIndex)
            ++i;
        EXPECT_EQ(*it1, i + 1);
        EXPECT_EQ(*it2, num - i);
    }
    EXPECT_EQ(it1, set0.end());
    EXPECT_EQ(it2, set1.end());
}

class DummyRefCounted : public RefCounted<DummyRefCounted> {
public:
    DummyRefCounted(bool& isDeleted) : m_isDeleted(isDeleted) { m_isDeleted = false; }
    ~DummyRefCounted() { m_isDeleted = true; }
    void ref()
    {
        WTF::RefCounted<DummyRefCounted>::ref();
        ++m_refInvokesCount;
    }

    static int m_refInvokesCount;

private:
    bool& m_isDeleted;
};

int DummyRefCounted::m_refInvokesCount = 0;

template <typename Set>
class ListOrLinkedHashSetRefPtrTest : public ::testing::Test { };

using RefPtrSetTypes = ::testing::Types<ListHashSet<RefPtr<DummyRefCounted>>, ListHashSet<RefPtr<DummyRefCounted>, 1>, LinkedHashSet<RefPtr<DummyRefCounted>>>;
TYPED_TEST_CASE(ListOrLinkedHashSetRefPtrTest, RefPtrSetTypes);

TYPED_TEST(ListOrLinkedHashSetRefPtrTest, WithRefPtr)
{
    using Set = TypeParam;
    bool isDeleted = false;
    DummyRefCounted::m_refInvokesCount = 0;
    RefPtr<DummyRefCounted> ptr = adoptRef(new DummyRefCounted(isDeleted));
    EXPECT_EQ(0, DummyRefCounted::m_refInvokesCount);

    Set set;
    set.add(ptr);
    // Referenced only once (to store a copy in the container).
    EXPECT_EQ(1, DummyRefCounted::m_refInvokesCount);
    EXPECT_EQ(ptr, set.first());
    EXPECT_EQ(1, DummyRefCounted::m_refInvokesCount);

    DummyRefCounted* rawPtr = ptr.get();

    EXPECT_TRUE(set.contains(ptr));
    EXPECT_TRUE(set.contains(rawPtr));
    EXPECT_EQ(1, DummyRefCounted::m_refInvokesCount);

    ptr.clear();
    EXPECT_FALSE(isDeleted);
    EXPECT_EQ(1, DummyRefCounted::m_refInvokesCount);

    set.remove(rawPtr);
    EXPECT_TRUE(isDeleted);

    EXPECT_EQ(1, DummyRefCounted::m_refInvokesCount);
}

TYPED_TEST(ListOrLinkedHashSetRefPtrTest, ExerciseValuePeekInType)
{
    using Set = TypeParam;
    Set set;
    bool isDeleted = false;
    bool isDeleted2 = false;

    RefPtr<DummyRefCounted> ptr = adoptRef(new DummyRefCounted(isDeleted));
    RefPtr<DummyRefCounted> ptr2 = adoptRef(new DummyRefCounted(isDeleted2));

    typename Set::AddResult addResult = set.add(ptr);
    EXPECT_TRUE(addResult.isNewEntry);
    set.find(ptr);
    const Set& constSet(set);
    constSet.find(ptr);
    EXPECT_TRUE(set.contains(ptr));
    typename Set::iterator it = set.addReturnIterator(ptr);
    set.appendOrMoveToLast(ptr);
    set.prependOrMoveToFirst(ptr);
    set.insertBefore(ptr, ptr);
    set.insertBefore(it, ptr);
    EXPECT_EQ(1u, set.size());
    set.add(ptr2);
    ptr2.clear();
    set.remove(ptr);

    EXPECT_FALSE(isDeleted);
    ptr.clear();
    EXPECT_TRUE(isDeleted);

    EXPECT_FALSE(isDeleted2);
    set.removeFirst();
    EXPECT_TRUE(isDeleted2);

    EXPECT_EQ(0u, set.size());
}

struct Simple {
    Simple(int value) : m_value(value) { };
    int m_value;
};

struct Complicated {
    Complicated(int value) : m_simple(value)
    {
        s_objectsConstructed++;
    }

    Complicated(const Complicated& other) : m_simple(other.m_simple)
    {
        s_objectsConstructed++;
    }

    Simple m_simple;
    static int s_objectsConstructed;

private:
    Complicated();
};

int Complicated::s_objectsConstructed = 0;

struct ComplicatedHashFunctions {
    static unsigned hash(const Complicated& key) { return key.m_simple.m_value; }
    static bool equal(const Complicated& a, const Complicated& b) { return a.m_simple.m_value == b.m_simple.m_value; }
};
struct ComplexityTranslator {
    static unsigned hash(const Simple& key) { return key.m_value; }
    static bool equal(const Complicated& a, const Simple& b) { return a.m_simple.m_value == b.m_value; }
};

template <typename Set>
class ListOrLinkedHashSetTranslatorTest : public ::testing::Test { };

using TranslatorSetTypes = ::testing::Types<
    ListHashSet<Complicated, 256, ComplicatedHashFunctions>,
    ListHashSet<Complicated, 1, ComplicatedHashFunctions>,
    LinkedHashSet<Complicated, ComplicatedHashFunctions>>;
TYPED_TEST_CASE(ListOrLinkedHashSetTranslatorTest, TranslatorSetTypes);

TYPED_TEST(ListOrLinkedHashSetTranslatorTest, ComplexityTranslator)
{
    using Set = TypeParam;
    Set set;
    set.add(Complicated(42));
    int baseLine = Complicated::s_objectsConstructed;

    typename Set::iterator it = set.template find<ComplexityTranslator>(Simple(42));
    EXPECT_NE(it, set.end());
    EXPECT_EQ(baseLine, Complicated::s_objectsConstructed);

    it = set.template find<ComplexityTranslator>(Simple(103));
    EXPECT_EQ(it, set.end());
    EXPECT_EQ(baseLine, Complicated::s_objectsConstructed);

    const Set& constSet(set);

    typename Set::const_iterator constIterator = constSet.template find<ComplexityTranslator>(Simple(42));
    EXPECT_NE(constIterator, constSet.end());
    EXPECT_EQ(baseLine, Complicated::s_objectsConstructed);

    constIterator = constSet.template find<ComplexityTranslator>(Simple(103));
    EXPECT_EQ(constIterator, constSet.end());
    EXPECT_EQ(baseLine, Complicated::s_objectsConstructed);
}

struct Dummy {
    Dummy(bool& deleted) : deleted(deleted) { }

    ~Dummy()
    {
        deleted = true;
    }

    bool& deleted;
};

TEST(ListHashSetTest, WithOwnPtr)
{
    bool deleted1 = false, deleted2 = false;

    typedef ListHashSet<std::unique_ptr<Dummy>> OwnPtrSet;
    OwnPtrSet set;

    Dummy* ptr1 = new Dummy(deleted1);
    {
        // AddResult in a separate scope to avoid assertion hit,
        // since we modify the container further.
        OwnPtrSet::AddResult res1 = set.add(wrapUnique(ptr1));
        EXPECT_EQ(res1.storedValue->get(), ptr1);
    }

    EXPECT_FALSE(deleted1);
    EXPECT_EQ(1UL, set.size());
    OwnPtrSet::iterator it1 = set.find(ptr1);
    EXPECT_NE(set.end(), it1);
    EXPECT_EQ(ptr1, (*it1).get());

    Dummy* ptr2 = new Dummy(deleted2);
    {
        OwnPtrSet::AddResult res2 = set.add(wrapUnique(ptr2));
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
        ownPtr1 = set.takeFirst();
        EXPECT_EQ(1UL, set.size());
        ownPtr2 = set.take(ptr2);
        EXPECT_TRUE(set.isEmpty());
    }
    EXPECT_FALSE(deleted1);
    EXPECT_FALSE(deleted2);

    EXPECT_EQ(ptr1, ownPtr1.get());
    EXPECT_EQ(ptr2, ownPtr2.get());
}

class CountCopy final {
public:
    static int* const kDeletedValue;

    explicit CountCopy(int* counter = nullptr) : m_counter(counter) { }
    CountCopy(const CountCopy& other) : m_counter(other.m_counter)
    {
        if (m_counter && m_counter != kDeletedValue)
            ++*m_counter;
    }
    const int* counter() const { return m_counter; }

private:
    int* m_counter;
};

int* const CountCopy::kDeletedValue = reinterpret_cast<int*>(static_cast<uintptr_t>(-1));

struct CountCopyHashTraits : public GenericHashTraits<CountCopy> {
    static bool isEmptyValue(const CountCopy& value) { return !value.counter(); }
    static void constructDeletedValue(CountCopy& slot, bool) { slot = CountCopy(CountCopy::kDeletedValue); }
    static bool isDeletedValue(const CountCopy& value) { return value.counter() == CountCopy::kDeletedValue; }
};

struct CountCopyHash : public PtrHash<const int> {
    static unsigned hash(const CountCopy& value) { return PtrHash<const int>::hash(value.counter()); }
    static bool equal(const CountCopy& left, const CountCopy& right)
    {
        return PtrHash<const int>::equal(left.counter(), right.counter());
    }
};

} // anonymous namespace

template <>
struct HashTraits<CountCopy> : public CountCopyHashTraits { };

template <>
struct DefaultHash<CountCopy> {
    using Hash = CountCopyHash;
};

namespace {

template <typename Set>
class ListOrLinkedHashSetCountCopyTest : public ::testing::Test { };

using CountCopySetTypes = ::testing::Types<ListHashSet<CountCopy>, ListHashSet<CountCopy, 1>, LinkedHashSet<CountCopy>>;
TYPED_TEST_CASE(ListOrLinkedHashSetCountCopyTest, CountCopySetTypes);

TYPED_TEST(ListOrLinkedHashSetCountCopyTest, MoveConstructionShouldNotMakeCopy)
{
    using Set = TypeParam;
    Set set;
    int counter = 0;
    set.add(CountCopy(&counter));

    counter = 0;
    Set other(std::move(set));
    EXPECT_EQ(0, counter);
}

TYPED_TEST(ListOrLinkedHashSetCountCopyTest, MoveAssignmentShouldNotMakeACopy)
{
    using Set = TypeParam;
    Set set;
    int counter = 0;
    set.add(CountCopy(&counter));

    Set other(set);
    counter = 0;
    set = std::move(other);
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

struct MoveOnlyHash {
    static unsigned hash(const MoveOnly& value) { return DefaultHash<int>::Hash::hash(value.value()); }
    static bool equal(const MoveOnly& left, const MoveOnly& right)
    {
        return DefaultHash<int>::Hash::equal(left.value(), right.value());
    }
};

} // anonymous namespace

template <>
struct DefaultHash<MoveOnly> {
    using Hash = MoveOnlyHash;
};

namespace {

template <typename Set>
class ListOrLinkedHashSetMoveOnlyTest : public ::testing::Test { };

using MoveOnlySetTypes = ::testing::Types<ListHashSet<MoveOnly>, ListHashSet<MoveOnly, 1>, LinkedHashSet<MoveOnly>>;
TYPED_TEST_CASE(ListOrLinkedHashSetMoveOnlyTest, MoveOnlySetTypes);

TYPED_TEST(ListOrLinkedHashSetMoveOnlyTest, MoveOnlyValue)
{
    using Set = TypeParam;
    using AddResult = typename Set::AddResult;
    Set set;
    {
        AddResult addResult = set.add(MoveOnly(1, 1));
        EXPECT_TRUE(addResult.isNewEntry);
        EXPECT_EQ(1, addResult.storedValue->value());
        EXPECT_EQ(1, addResult.storedValue->id());
    }
    {
        AddResult addResult = set.add(MoveOnly(1, 111));
        EXPECT_FALSE(addResult.isNewEntry);
        EXPECT_EQ(1, addResult.storedValue->value());
        EXPECT_EQ(1, addResult.storedValue->id());
    }
    auto iter = set.find(MoveOnly(1));
    ASSERT_TRUE(iter != set.end());
    EXPECT_EQ(1, iter->value());
    EXPECT_EQ(1, iter->id());

    iter = set.find(MoveOnly(2));
    EXPECT_TRUE(iter == set.end());

    // ListHashSet and LinkedHashSet have several flavors of add().
    iter = set.addReturnIterator(MoveOnly(2, 2));
    EXPECT_EQ(2, iter->value());
    EXPECT_EQ(2, iter->id());

    iter = set.addReturnIterator(MoveOnly(2, 222));
    EXPECT_EQ(2, iter->value());
    EXPECT_EQ(2, iter->id());

    {
        AddResult addResult = set.appendOrMoveToLast(MoveOnly(3, 3));
        EXPECT_TRUE(addResult.isNewEntry);
        EXPECT_EQ(3, addResult.storedValue->value());
        EXPECT_EQ(3, addResult.storedValue->id());
    }
    {
        AddResult addResult = set.prependOrMoveToFirst(MoveOnly(4, 4));
        EXPECT_TRUE(addResult.isNewEntry);
        EXPECT_EQ(4, addResult.storedValue->value());
        EXPECT_EQ(4, addResult.storedValue->id());
    }
    {
        AddResult addResult = set.insertBefore(MoveOnly(4), MoveOnly(5, 5));
        EXPECT_TRUE(addResult.isNewEntry);
        EXPECT_EQ(5, addResult.storedValue->value());
        EXPECT_EQ(5, addResult.storedValue->id());
    }
    {
        iter = set.find(MoveOnly(5));
        ASSERT_TRUE(iter != set.end());
        AddResult addResult = set.insertBefore(iter, MoveOnly(6, 6));
        EXPECT_TRUE(addResult.isNewEntry);
        EXPECT_EQ(6, addResult.storedValue->value());
        EXPECT_EQ(6, addResult.storedValue->id());
    }

    // ... but they don't have any pass-out (like take()) methods.

    set.remove(MoveOnly(3));
    set.clear();
}


} // anonymous namespace

} // namespace WTF
