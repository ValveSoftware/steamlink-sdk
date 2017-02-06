// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/SmallCapsIterator.h"

#include "platform/Logging.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <string>

namespace blink {

struct TestRun {
    std::string text;
    SmallCapsIterator::SmallCapsBehavior code;
};

struct ExpectedRun {
    unsigned limit;
    SmallCapsIterator::SmallCapsBehavior smallCapsBehavior;

    ExpectedRun(unsigned theLimit, SmallCapsIterator::SmallCapsBehavior theSmallCapsBehavior)
        : limit(theLimit)
        , smallCapsBehavior(theSmallCapsBehavior)
    {
    }
};

class SmallCapsIteratorTest : public testing::Test {
protected:
    void CheckRuns(const Vector<TestRun>& runs)
    {
        String text(emptyString16Bit());
        Vector<ExpectedRun> expect;
        for (auto& run : runs) {
            text.append(String::fromUTF8(run.text.c_str()));
            expect.append(ExpectedRun(text.length(), run.code));
        }
        SmallCapsIterator smallCapsIterator(text.characters16(), text.length());
        VerifyRuns(&smallCapsIterator, expect);
    }

    void VerifyRuns(SmallCapsIterator* smallCapsIterator,
        const Vector<ExpectedRun>& expect)
    {
        unsigned limit;
        SmallCapsIterator::SmallCapsBehavior smallCapsBehavior;
        unsigned long runCount = 0;
        while (smallCapsIterator->consume(&limit, &smallCapsBehavior)) {
            ASSERT_LT(runCount, expect.size());
            ASSERT_EQ(expect[runCount].limit, limit);
            ASSERT_EQ(expect[runCount].smallCapsBehavior, smallCapsBehavior);
            ++runCount;
        }
        ASSERT_EQ(expect.size(), runCount);
    }
};

// Some of our compilers cannot initialize a vector from an array yet.
#define DECLARE_RUNSVECTOR(...)                     \
    static const TestRun runsArray[] = __VA_ARGS__; \
    Vector<TestRun> runs; \
    runs.append(runsArray, sizeof(runsArray) / sizeof(*runsArray));

#define CHECK_RUNS(...)              \
    DECLARE_RUNSVECTOR(__VA_ARGS__); \
    CheckRuns(runs);


TEST_F(SmallCapsIteratorTest, Empty)
{
    String empty(emptyString16Bit());
    SmallCapsIterator smallCapsIterator(empty.characters16(), empty.length());
    unsigned limit = 0;
    SmallCapsIterator::SmallCapsBehavior smallCapsBehavior = SmallCapsIterator::SmallCapsInvalid;
    ASSERT(!smallCapsIterator.consume(&limit, &smallCapsBehavior));
    ASSERT_EQ(limit, 0u);
    ASSERT_EQ(smallCapsBehavior, SmallCapsIterator::SmallCapsInvalid);
}

TEST_F(SmallCapsIteratorTest, UppercaseA)
{
    CHECK_RUNS({ { "A", SmallCapsIterator::SmallCapsSameCase } });
}

TEST_F(SmallCapsIteratorTest, LowercaseA)
{
    CHECK_RUNS({ { "a", SmallCapsIterator::SmallCapsUppercaseNeeded } });
}

TEST_F(SmallCapsIteratorTest, UppercaseLowercaseA)
{
    CHECK_RUNS({ { "A", SmallCapsIterator::SmallCapsSameCase },
        { "a", SmallCapsIterator::SmallCapsUppercaseNeeded } });
}

TEST_F(SmallCapsIteratorTest, UppercasePunctuationMixed)
{
    CHECK_RUNS({ { "AAA??", SmallCapsIterator::SmallCapsSameCase } });
}

TEST_F(SmallCapsIteratorTest, LowercasePunctuationMixed)
{
    CHECK_RUNS({ { "aaa", SmallCapsIterator::SmallCapsUppercaseNeeded },
        { "===", SmallCapsIterator::SmallCapsSameCase } });
}

TEST_F(SmallCapsIteratorTest, LowercasePunctuationInterleaved)
{
    CHECK_RUNS({ { "aaa", SmallCapsIterator::SmallCapsUppercaseNeeded },
        { "===", SmallCapsIterator::SmallCapsSameCase },
        { "bbb", SmallCapsIterator::SmallCapsUppercaseNeeded } });
}

TEST_F(SmallCapsIteratorTest, Japanese)
{
    CHECK_RUNS({ { "ほへと", SmallCapsIterator::SmallCapsSameCase } });
}

TEST_F(SmallCapsIteratorTest, Armenian)
{
    CHECK_RUNS({ { "աբգդ", SmallCapsIterator::SmallCapsUppercaseNeeded },
        { "ԵԶԷԸ", SmallCapsIterator::SmallCapsSameCase } });
}

TEST_F(SmallCapsIteratorTest, CombiningCharacterSequence)
{
    CHECK_RUNS({ { "èü", SmallCapsIterator::SmallCapsUppercaseNeeded } });
}

} // namespace blink
