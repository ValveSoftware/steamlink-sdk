// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/weborigin/SchemeRegistry.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

const char kTestScheme[] = "test-scheme";
const char kTestSchemeUppercase[] = "TEST-SCHEME";
const char kTestScheme2[] = "test-scheme-2";

class SchemeRegistryTest : public ::testing::Test {
    void TearDown() override
    {
        SchemeRegistry::removeURLSchemeRegisteredAsBypassingContentSecurityPolicy(kTestScheme);
    }
};

TEST_F(SchemeRegistryTest, NoCSPBypass)
{
    EXPECT_FALSE(SchemeRegistry::schemeShouldBypassContentSecurityPolicy(kTestScheme));
    EXPECT_FALSE(SchemeRegistry::schemeShouldBypassContentSecurityPolicy(kTestSchemeUppercase));
    EXPECT_FALSE(SchemeRegistry::schemeShouldBypassContentSecurityPolicy(kTestScheme, SchemeRegistry::PolicyAreaImage));
}

TEST_F(SchemeRegistryTest, FullCSPBypass)
{
    SchemeRegistry::registerURLSchemeAsBypassingContentSecurityPolicy(kTestScheme);
    EXPECT_TRUE(SchemeRegistry::schemeShouldBypassContentSecurityPolicy(kTestScheme));
    EXPECT_TRUE(SchemeRegistry::schemeShouldBypassContentSecurityPolicy(kTestSchemeUppercase));
    EXPECT_TRUE(SchemeRegistry::schemeShouldBypassContentSecurityPolicy(kTestScheme, SchemeRegistry::PolicyAreaImage));
    EXPECT_FALSE(SchemeRegistry::schemeShouldBypassContentSecurityPolicy(kTestScheme2));
    EXPECT_FALSE(SchemeRegistry::schemeShouldBypassContentSecurityPolicy(kTestScheme2, SchemeRegistry::PolicyAreaImage));
}

TEST_F(SchemeRegistryTest, PartialCSPBypass)
{
    SchemeRegistry::registerURLSchemeAsBypassingContentSecurityPolicy(kTestScheme, SchemeRegistry::PolicyAreaImage);
    EXPECT_FALSE(SchemeRegistry::schemeShouldBypassContentSecurityPolicy(kTestScheme));
    EXPECT_FALSE(SchemeRegistry::schemeShouldBypassContentSecurityPolicy(kTestSchemeUppercase));
    EXPECT_TRUE(SchemeRegistry::schemeShouldBypassContentSecurityPolicy(kTestScheme, SchemeRegistry::PolicyAreaImage));
    EXPECT_TRUE(SchemeRegistry::schemeShouldBypassContentSecurityPolicy(kTestSchemeUppercase, SchemeRegistry::PolicyAreaImage));
    EXPECT_FALSE(SchemeRegistry::schemeShouldBypassContentSecurityPolicy(kTestScheme, SchemeRegistry::PolicyAreaStyle));
    EXPECT_FALSE(SchemeRegistry::schemeShouldBypassContentSecurityPolicy(kTestSchemeUppercase, SchemeRegistry::PolicyAreaStyle));
    EXPECT_FALSE(SchemeRegistry::schemeShouldBypassContentSecurityPolicy(kTestScheme2, SchemeRegistry::PolicyAreaImage));
}

TEST_F(SchemeRegistryTest, BypassSecureContextCheck)
{
    const char* scheme1 = "http";
    const char* scheme2 = "https";
    const char* scheme3 = "random-scheme";
    const char* scheme4 = "RANDOM-SCHEME";

    EXPECT_FALSE(SchemeRegistry::schemeShouldBypassSecureContextCheck(scheme1));
    EXPECT_FALSE(SchemeRegistry::schemeShouldBypassSecureContextCheck(scheme2));
    EXPECT_FALSE(SchemeRegistry::schemeShouldBypassSecureContextCheck(scheme3));
    EXPECT_FALSE(SchemeRegistry::schemeShouldBypassSecureContextCheck(scheme4));

    SchemeRegistry::registerURLSchemeBypassingSecureContextCheck("random-scheme");

    EXPECT_FALSE(SchemeRegistry::schemeShouldBypassSecureContextCheck(scheme1));
    EXPECT_FALSE(SchemeRegistry::schemeShouldBypassSecureContextCheck(scheme2));
    EXPECT_TRUE(SchemeRegistry::schemeShouldBypassSecureContextCheck(scheme3));
    EXPECT_TRUE(SchemeRegistry::schemeShouldBypassSecureContextCheck(scheme4));
}

} // namespace
} // namespace blink
