// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/UseCounter.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class UseCounterTest : public ::testing::Test {
protected:
    bool hasRecordedMeasurement(const UseCounter& useCounter, UseCounter::Feature feature)
    {
        return useCounter.hasRecordedMeasurement(feature);
    }

    void recordMeasurement(UseCounter& useCounter, UseCounter::Feature feature)
    {
        useCounter.recordMeasurement(feature);
    }

    void setUseCounterMuted(UseCounter& useCounter, bool muteCount)
    {
        UseCounter::m_muteCount = muteCount;
    }
};

TEST_F(UseCounterTest, RecordingMeasurements)
{
    UseCounter useCounter;
    for (unsigned feature = 0; feature < UseCounter::NumberOfFeatures; feature++) {
        if (feature != UseCounter::Feature::PageDestruction) {
            EXPECT_FALSE(hasRecordedMeasurement(useCounter, static_cast<UseCounter::Feature>(feature)));
            recordMeasurement(useCounter, static_cast<UseCounter::Feature>(feature));
            EXPECT_TRUE(hasRecordedMeasurement(useCounter, static_cast<UseCounter::Feature>(feature)));
        }
    }
}

TEST_F(UseCounterTest, MultipleMeasurements)
{
    UseCounter useCounter;
    for (unsigned feature = 0; feature < UseCounter::NumberOfFeatures; feature++) {
        if (feature != UseCounter::Feature::PageDestruction) {
            recordMeasurement(useCounter, static_cast<UseCounter::Feature>(feature));
            recordMeasurement(useCounter, static_cast<UseCounter::Feature>(feature));
            EXPECT_TRUE(hasRecordedMeasurement(useCounter, static_cast<UseCounter::Feature>(feature)));
        }
    }
}

TEST_F(UseCounterTest, InspectorDisablesMeasurement)
{
    UseCounter useCounter;

    // The specific feature we use here isn't important.
    UseCounter::Feature feature = UseCounter::Feature::SVGSMILElementInDocument;

    EXPECT_FALSE(hasRecordedMeasurement(useCounter, feature));

    UseCounter::muteForInspector();
    recordMeasurement(useCounter, feature);
    EXPECT_FALSE(hasRecordedMeasurement(useCounter, feature));

    UseCounter::muteForInspector();
    recordMeasurement(useCounter, feature);
    EXPECT_FALSE(hasRecordedMeasurement(useCounter, feature));

    UseCounter::unmuteForInspector();
    recordMeasurement(useCounter, feature);
    EXPECT_FALSE(hasRecordedMeasurement(useCounter, feature));

    UseCounter::unmuteForInspector();
    recordMeasurement(useCounter, feature);
    EXPECT_TRUE(hasRecordedMeasurement(useCounter, feature));
}

} // namespace blink
