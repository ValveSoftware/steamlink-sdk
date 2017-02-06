// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/testing/PrivateScriptTest.h"

#include "bindings/core/v8/PrivateScriptRunner.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "bindings/core/v8/V8PrivateScriptTest.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

// PrivateScriptTest.js is available only in debug builds.
#ifndef NDEBUG
namespace blink {

namespace {

TEST(PrivateScriptTestTest, invokePrivateScriptMethodFromCPP)
{
    V8TestingScope scope;
    PrivateScriptTest* privateScriptTest = PrivateScriptTest::create(&scope.document());
    bool success;
    int result;
    success = V8PrivateScriptTest::PrivateScript::addIntegerForPrivateScriptOnlyMethod(&scope.frame(), privateScriptTest, 100, 200, &result);
    EXPECT_TRUE(success);
    EXPECT_EQ(result, 300);
}

TEST(PrivateScriptTestTest, invokePrivateScriptAttributeFromCPP)
{
    V8TestingScope scope;
    PrivateScriptTest* privateScriptTest = PrivateScriptTest::create(&scope.document());
    bool success;
    String result;
    success = V8PrivateScriptTest::PrivateScript::stringAttributeForPrivateScriptOnlyAttributeGetter(&scope.frame(), privateScriptTest, &result);
    EXPECT_TRUE(success);
    EXPECT_EQ(result, "yyy");
    success = V8PrivateScriptTest::PrivateScript::stringAttributeForPrivateScriptOnlyAttributeSetter(&scope.frame(), privateScriptTest, "foo");
    EXPECT_TRUE(success);
    success = V8PrivateScriptTest::PrivateScript::stringAttributeForPrivateScriptOnlyAttributeGetter(&scope.frame(), privateScriptTest, &result);
    EXPECT_TRUE(success);
    EXPECT_EQ(result, "foo");
}

} // namespace

} // namespace blink
#endif
