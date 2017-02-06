// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/WebVector.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/StdLibExtras.h"
#include "wtf/Vector.h"

namespace blink {

TEST(WebVectorTest, Iterators)
{
    Vector<int> input;
    for (int i = 0; i < 5; ++i)
        input.append(i);

    WebVector<int> webVector(input);
    const WebVector<int>& constWebVector = webVector;
    Vector<int> output;

    ASSERT_EQ(input.size(), webVector.size());

    // Use begin()/end() iterators directly.
    for (WebVector<int>::iterator it = webVector.begin(); it != webVector.end(); ++it)
        output.append(*it);
    ASSERT_EQ(input.size(), output.size());
    for (size_t i = 0; i < input.size(); ++i)
        EXPECT_EQ(input[i], output[i]);

    // Use begin()/end() const_iterators directly.
    output.clear();
    for (WebVector<int>::const_iterator it = constWebVector.begin(); it != constWebVector.end(); ++it)
        output.append(*it);
    ASSERT_EQ(input.size(), output.size());
    for (size_t i = 0; i < input.size(); ++i)
        EXPECT_EQ(input[i], output[i]);

    // Use range-based for loop.
    output.clear();
    for (int x : webVector)
        output.append(x);
    ASSERT_EQ(input.size(), output.size());
    for (size_t i = 0; i < input.size(); ++i)
        EXPECT_EQ(input[i], output[i]);
}

TEST(WebVectorTest, IsEmpty)
{
    WebVector<int> vector;
    ASSERT_TRUE(vector.isEmpty());
    int value = 1;
    vector.assign(&value, 1);
    ASSERT_EQ(1u, vector.size());
    ASSERT_FALSE(vector.isEmpty());
}

TEST(WebVectorTest, Swap)
{
    const int firstData[] = {1, 2, 3, 4, 5};
    const int secondData[] = {6, 5, 8};
    const size_t kFirstDataLength = WTF_ARRAY_LENGTH(firstData);
    const size_t kSecondDataLength = WTF_ARRAY_LENGTH(secondData);

    WebVector<int> first(firstData, kFirstDataLength);
    WebVector<int> second(secondData, kSecondDataLength);
    ASSERT_EQ(kFirstDataLength, first.size());
    ASSERT_EQ(kSecondDataLength, second.size());
    first.swap(second);
    ASSERT_EQ(kSecondDataLength, first.size());
    ASSERT_EQ(kFirstDataLength, second.size());
    for (size_t i = 0; i < first.size(); ++i)
        EXPECT_EQ(secondData[i], first[i]);
    for (size_t i = 0; i < second.size(); ++i)
        EXPECT_EQ(firstData[i], second[i]);
}

TEST(WebVectorTest, CreateFromPointer)
{
    const int values[] = {1, 2, 3, 4, 5};

    WebVector<int> vector(values, 3);
    ASSERT_EQ(3u, vector.size());
    ASSERT_EQ(1, vector[0]);
    ASSERT_EQ(2, vector[1]);
    ASSERT_EQ(3, vector[2]);
}

TEST(WebVectorTest, CreateFromWtfVector)
{
    Vector<int> input;
    for (int i = 0; i < 5; ++i)
        input.append(i);

    WebVector<int> vector(input);
    ASSERT_EQ(input.size(), vector.size());
    for (size_t i = 0; i < vector.size(); ++i)
        EXPECT_EQ(input[i], vector[i]);

    WebVector<int> copy(input);
    ASSERT_EQ(input.size(), copy.size());
    for (size_t i = 0; i < copy.size(); ++i)
        EXPECT_EQ(input[i], copy[i]);

    WebVector<int> assigned;
    assigned = copy;
    ASSERT_EQ(input.size(), assigned.size());
    for (size_t i = 0; i < assigned.size(); ++i)
        EXPECT_EQ(input[i], assigned[i]);
}

TEST(WebVectorTest, CreateFromStdVector)
{
    std::vector<int> input;
    for (int i = 0; i < 5; ++i)
        input.push_back(i);

    WebVector<int> vector(input);
    ASSERT_EQ(input.size(), vector.size());
    for (size_t i = 0; i < vector.size(); ++i)
        EXPECT_EQ(input[i], vector[i]);

    WebVector<int> assigned;
    assigned = input;
    ASSERT_EQ(input.size(), assigned.size());
    for (size_t i = 0; i < assigned.size(); ++i)
        EXPECT_EQ(input[i], assigned[i]);
}


} // namespace blink
