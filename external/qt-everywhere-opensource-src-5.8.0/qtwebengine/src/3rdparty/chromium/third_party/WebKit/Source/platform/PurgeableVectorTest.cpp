/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/PurgeableVector.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/Vector.h"
#include <algorithm>
#include <cstdlib>

namespace blink {

const size_t kTestSize = 32 * 1024;

class PurgeableVectorTestWithDiscardableMemory : public testing::TestWithParam<PurgeableVector::PurgeableOption> {
};

TEST_P(PurgeableVectorTestWithDiscardableMemory, grow)
{
    PurgeableVector purgeableVector(GetParam());
    purgeableVector.grow(kTestSize);
    ASSERT_EQ(kTestSize, purgeableVector.size());
    // Make sure the underlying buffer was actually (re)allocated.
    memset(purgeableVector.data(), 0, purgeableVector.size());
}

TEST_P(PurgeableVectorTestWithDiscardableMemory, clear)
{
    Vector<char> testData(kTestSize);
    std::generate(testData.begin(), testData.end(), &std::rand);

    PurgeableVector purgeableVector(GetParam());
    purgeableVector.append(testData.data(), testData.size());
    EXPECT_EQ(testData.size(), purgeableVector.size());

    purgeableVector.clear();
    EXPECT_EQ(0U, purgeableVector.size());
    EXPECT_EQ(0, purgeableVector.data());
}

TEST_P(PurgeableVectorTestWithDiscardableMemory, clearDoesNotResetLockCounter)
{
    PurgeableVector purgeableVector(GetParam());
    purgeableVector.clear();
    EXPECT_TRUE(purgeableVector.isLocked());
    purgeableVector.unlock();
    EXPECT_FALSE(purgeableVector.isLocked());
}

TEST_P(PurgeableVectorTestWithDiscardableMemory, reserveCapacityDoesNotChangeSize)
{
    PurgeableVector purgeableVector(GetParam());
    EXPECT_EQ(0U, purgeableVector.size());
    purgeableVector.reserveCapacity(kTestSize);
    EXPECT_EQ(0U, purgeableVector.size());
}

TEST_P(PurgeableVectorTestWithDiscardableMemory, multipleAppends)
{
    Vector<char> testData(kTestSize);
    std::generate(testData.begin(), testData.end(), &std::rand);

    PurgeableVector purgeableVector(GetParam());
    // Force an allocation.
    const char kSmallString[] = "hello";
    purgeableVector.append(kSmallString, sizeof(kSmallString));
    const char* const data = purgeableVector.data();

    // Append all the testing data in 4 iterations. The |data| pointer should
    // have been changed at the end of the unit test due to reallocations.
    const size_t kIterationCount = 4;
    ASSERT_EQ(0U, testData.size() % kIterationCount);
    for (size_t i = 0; i < kIterationCount; ++i) {
        const char* const testDataStart = testData.data() + i * (testData.size() / kIterationCount);
        purgeableVector.append(testDataStart, testData.size() / kIterationCount);
        ASSERT_EQ((i + 1) * testData.size() / kIterationCount, purgeableVector.size() - sizeof(kSmallString));
    }

    ASSERT_EQ(sizeof(kSmallString) + testData.size(), purgeableVector.size());
    EXPECT_NE(data, purgeableVector.data());
    EXPECT_EQ(0, memcmp(purgeableVector.data() + sizeof(kSmallString), testData.data(), testData.size()));
}

TEST_P(PurgeableVectorTestWithDiscardableMemory, multipleAppendsAfterReserveCapacity)
{
    Vector<char> testData(kTestSize);
    std::generate(testData.begin(), testData.end(), &std::rand);

    PurgeableVector purgeableVector(GetParam());
    purgeableVector.reserveCapacity(testData.size());
    const char* const data = purgeableVector.data();

    // The |data| pointer should be unchanged at the end of the unit test
    // meaning that there should not have been any reallocation.
    const size_t kIterationCount = 4;
    ASSERT_EQ(0U, testData.size() % kIterationCount);
    for (size_t i = 0; i < kIterationCount; ++i) {
        const char* const testDataStart = testData.data() + i * (testData.size() / kIterationCount);
        purgeableVector.append(testDataStart, testData.size() / kIterationCount);
        ASSERT_EQ((i + 1) * testData.size() / kIterationCount, purgeableVector.size());
    }

    ASSERT_EQ(testData.size(), purgeableVector.size());
    EXPECT_EQ(data, purgeableVector.data());
    EXPECT_EQ(0, memcmp(purgeableVector.data(), testData.data(), testData.size()));
}

TEST_P(PurgeableVectorTestWithDiscardableMemory, reserveCapacityUsesExactCapacityWhenVectorIsEmpty)
{
    Vector<char> testData(kTestSize);
    std::generate(testData.begin(), testData.end(), &std::rand);

    PurgeableVector purgeableVector(GetParam());
    purgeableVector.reserveCapacity(kTestSize);
    const char* const data = purgeableVector.data();

    purgeableVector.append(testData.data(), testData.size());
    EXPECT_EQ(data, purgeableVector.data());
    EXPECT_EQ(0, memcmp(purgeableVector.data(), testData.data(), testData.size()));

    // This test is not reliable if the PurgeableVector uses a plain WTF::Vector
    // for storage, as it does if discardable memory is not supported; the vectors
    // capacity will always be expanded to fill the PartitionAlloc bucket.
    if (GetParam() == PurgeableVector::Purgeable) {
        // Appending one extra byte should cause a reallocation since the first
        // allocation happened while the purgeable vector was empty. This behavior
        // helps us guarantee that there is no memory waste on very small vectors
        // (which SharedBuffer requires).
        purgeableVector.append(testData.data(), 1);
        EXPECT_NE(data, purgeableVector.data());
    }
}

TEST_P(PurgeableVectorTestWithDiscardableMemory, appendReservesCapacityIfNeeded)
{
    Vector<char> testData(kTestSize);
    std::generate(testData.begin(), testData.end(), &std::rand);

    PurgeableVector purgeableVector(GetParam());
    // No reserveCapacity().
    ASSERT_FALSE(purgeableVector.data());

    purgeableVector.append(testData.data(), testData.size());
    ASSERT_EQ(testData.size(), purgeableVector.size());
    ASSERT_EQ(0, memcmp(purgeableVector.data(), testData.data(), testData.size()));
}

TEST_P(PurgeableVectorTestWithDiscardableMemory, adopt)
{
    Vector<char> testData(kTestSize);
    std::generate(testData.begin(), testData.end(), &std::rand);
    const Vector<char> testDataCopy(testData);
    const char* const testDataPtr = testData.data();

    PurgeableVector purgeableVector(GetParam());
    purgeableVector.adopt(testData);
    EXPECT_TRUE(testData.isEmpty());
    EXPECT_EQ(kTestSize, purgeableVector.size());
    ASSERT_EQ(0, memcmp(purgeableVector.data(), testDataCopy.data(), testDataCopy.size()));

    if (GetParam() == PurgeableVector::Purgeable) {
        // An extra discardable memory allocation + memcpy() should have happened.
        EXPECT_NE(testDataPtr, purgeableVector.data());
    } else {
        // Vector::swap() should have been used.
        EXPECT_EQ(testDataPtr, purgeableVector.data());
    }
}

TEST_P(PurgeableVectorTestWithDiscardableMemory, adoptEmptyVector)
{
    Vector<char> testData;
    PurgeableVector purgeableVector(GetParam());
    purgeableVector.adopt(testData);
}

TEST(PurgeableVectorTest, adoptDiscardsPreviousData)
{
    Vector<char> testData;
    std::generate(testData.begin(), testData.end(), &std::rand);

    PurgeableVector purgeableVector(PurgeableVector::NotPurgeable);
    static const char smallString[] = "hello";
    purgeableVector.append(smallString, sizeof(smallString));
    ASSERT_EQ(0, memcmp(purgeableVector.data(), smallString, sizeof(smallString)));

    purgeableVector.adopt(testData);
    EXPECT_EQ(testData.size(), purgeableVector.size());
    ASSERT_EQ(0, memcmp(purgeableVector.data(), testData.data(), testData.size()));
}

TEST(PurgeableVectorTest, unlockWithoutHintAtConstruction)
{
    Vector<char> testData(30000);
    std::generate(testData.begin(), testData.end(), &std::rand);

    unsigned length = testData.size();
    PurgeableVector purgeableVector(PurgeableVector::NotPurgeable);
    purgeableVector.append(testData.data(), length);
    ASSERT_EQ(length, purgeableVector.size());
    const char* data = purgeableVector.data();

    purgeableVector.unlock();

    // Note that the purgeable vector must be locked before calling data().
    const bool wasPurged = !purgeableVector.lock();

    // The implementation of purgeable memory used for testing always purges data upon unlock().
    EXPECT_TRUE(wasPurged);

    // The data should have been moved from the heap-allocated vector to a purgeable buffer.
    ASSERT_NE(data, purgeableVector.data());

    if (!wasPurged)
        ASSERT_EQ(0, memcmp(purgeableVector.data(), testData.data(), length));
}

TEST(PurgeableVectorTest, unlockOnEmptyPurgeableVector)
{
    PurgeableVector purgeableVector;
    ASSERT_EQ(0U, purgeableVector.size());
    purgeableVector.unlock();
    ASSERT_FALSE(purgeableVector.isLocked());
}

TEST(PurgeableVectorTest, unlockOnPurgeableVectorWithPurgeableHint)
{
    Vector<char> testData(kTestSize);
    std::generate(testData.begin(), testData.end(), &std::rand);

    PurgeableVector purgeableVector;
    purgeableVector.append(testData.data(), kTestSize);
    const char* const data = purgeableVector.data();

    // unlock() should happen in place, i.e. without causing any reallocation.
    // Note that the instance must be locked when data() is called.
    purgeableVector.unlock();
    EXPECT_FALSE(purgeableVector.isLocked());
    purgeableVector.lock();
    EXPECT_TRUE(purgeableVector.isLocked());
    EXPECT_EQ(data, purgeableVector.data());
}

TEST(PurgeableVectorTest, lockingUsesACounter)
{
    Vector<char> testData(kTestSize);
    std::generate(testData.begin(), testData.end(), &std::rand);

    PurgeableVector purgeableVector(PurgeableVector::NotPurgeable);
    purgeableVector.append(testData.data(), testData.size());
    ASSERT_EQ(testData.size(), purgeableVector.size());

    ASSERT_TRUE(purgeableVector.isLocked()); // PurgeableVector is locked at creation.
    ASSERT_TRUE(purgeableVector.lock()); // Add an extra lock.
    ASSERT_TRUE(purgeableVector.isLocked());

    purgeableVector.unlock();
    ASSERT_TRUE(purgeableVector.isLocked());

    purgeableVector.unlock();
    ASSERT_FALSE(purgeableVector.isLocked());

    if (purgeableVector.lock())
        ASSERT_EQ(0, memcmp(purgeableVector.data(), testData.data(), testData.size()));
}

// Instantiates all the unit tests using the PurgeableVectorTestWithDiscardableMemory
// fixture both with and without using discardable memory support.
INSTANTIATE_TEST_CASE_P(, PurgeableVectorTestWithDiscardableMemory,
    ::testing::Values(PurgeableVector::NotPurgeable, PurgeableVector::Purgeable));

} // namespace blink
