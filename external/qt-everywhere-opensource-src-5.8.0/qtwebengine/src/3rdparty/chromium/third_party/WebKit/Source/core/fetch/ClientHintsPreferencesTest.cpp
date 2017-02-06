// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fetch/ClientHintsPreferences.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class ClientHintsPreferencesTest : public ::testing::Test {
};

TEST_F(ClientHintsPreferencesTest, Basic)
{
    struct TestCase {
        const char* headerValue;
        bool expectationResourceWidth;
        bool expectationDPR;
        bool expectationViewportWidth;
    } cases[] = {
        {"width, dpr, viewportWidth", true, true, false},
        {"WiDtH, dPr,     viewport-width", true, true, true},
        {"WIDTH, DPR, VIWEPROT-Width", true, true, false},
        {"VIewporT-Width, wutwut, width", true, false, true},
        {"dprw", false, false, false},
        {"DPRW", false, false, false},
    };

    for (const auto& testCase : cases) {
        ClientHintsPreferences preferences;
        const char* value = testCase.headerValue;

        preferences.updateFromAcceptClientHintsHeader(value, nullptr);
        EXPECT_EQ(testCase.expectationResourceWidth, preferences.shouldSendResourceWidth());
        EXPECT_EQ(testCase.expectationDPR, preferences.shouldSendDPR());
        EXPECT_EQ(testCase.expectationViewportWidth, preferences.shouldSendViewportWidth());
    }
}

} // namespace blink
