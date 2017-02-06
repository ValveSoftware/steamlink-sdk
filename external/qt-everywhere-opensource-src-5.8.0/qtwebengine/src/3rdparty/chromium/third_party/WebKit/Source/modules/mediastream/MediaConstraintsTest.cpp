// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/mediastream/MediaConstraintsImpl.h"
#include "modules/mediastream/MediaTrackConstraints.h"
#include "public/platform/WebMediaConstraints.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

// The MediaTrackConstraintsTest group tests the types declared in
// WebKit/public/platform/WebMediaConstraints.h
TEST(MediaTrackConstraintsTest, LongConstraint)
{
    LongConstraint rangeConstraint(nullptr);
    rangeConstraint.setMin(5);
    rangeConstraint.setMax(6);
    EXPECT_TRUE(rangeConstraint.matches(5));
    EXPECT_TRUE(rangeConstraint.matches(6));
    EXPECT_FALSE(rangeConstraint.matches(4));
    EXPECT_FALSE(rangeConstraint.matches(7));
    LongConstraint exactConstraint(nullptr);
    exactConstraint.setExact(5);
    EXPECT_FALSE(exactConstraint.matches(4));
    EXPECT_TRUE(exactConstraint.matches(5));
    EXPECT_FALSE(exactConstraint.matches(6));
}

TEST(MediaTrackConstraintsTest, DoubleConstraint)
{
    DoubleConstraint rangeConstraint(nullptr);
    EXPECT_TRUE(rangeConstraint.isEmpty());
    rangeConstraint.setMin(5.0);
    rangeConstraint.setMax(6.5);
    EXPECT_FALSE(rangeConstraint.isEmpty());
    // Matching within epsilon
    EXPECT_TRUE(rangeConstraint.matches(5.0 - DoubleConstraint::kConstraintEpsilon / 2));
    EXPECT_TRUE(rangeConstraint.matches(6.5 + DoubleConstraint::kConstraintEpsilon / 2));
    DoubleConstraint exactConstraint(nullptr);
    exactConstraint.setExact(5.0);
    EXPECT_FALSE(rangeConstraint.isEmpty());
    EXPECT_FALSE(exactConstraint.matches(4.9));
    EXPECT_TRUE(exactConstraint.matches(5.0));
    EXPECT_TRUE(exactConstraint.matches(5.0 - DoubleConstraint::kConstraintEpsilon / 2));
    EXPECT_TRUE(exactConstraint.matches(5.0 + DoubleConstraint::kConstraintEpsilon / 2));
    EXPECT_FALSE(exactConstraint.matches(5.1));
}

TEST(MediaTrackConstraintsTest, BooleanConstraint)
{
    BooleanConstraint boolConstraint(nullptr);
    EXPECT_TRUE(boolConstraint.isEmpty());
    EXPECT_TRUE(boolConstraint.matches(false));
    EXPECT_TRUE(boolConstraint.matches(true));
    boolConstraint.setExact(false);
    EXPECT_FALSE(boolConstraint.isEmpty());
    EXPECT_FALSE(boolConstraint.matches(true));
    EXPECT_TRUE(boolConstraint.matches(false));
    boolConstraint.setExact(true);
    EXPECT_FALSE(boolConstraint.matches(false));
    EXPECT_TRUE(boolConstraint.matches(true));
}

TEST(MediaTrackConstraintsTest, ConstraintSetEmpty)
{
    WebMediaTrackConstraintSet theSet;
    EXPECT_TRUE(theSet.isEmpty());
    theSet.echoCancellation.setExact(false);
    EXPECT_FALSE(theSet.isEmpty());
}

TEST(MediaTrackConstraintsTest, ConstraintName)
{
    const char* theName = "name";
    BooleanConstraint boolConstraint(theName);
    EXPECT_EQ(theName, boolConstraint.name());
}

TEST(MediaTrackConstraintsTest, MandatoryChecks)
{
    WebMediaTrackConstraintSet theSet;
    std::string foundName;
    EXPECT_FALSE(theSet.hasMandatory());
    EXPECT_FALSE(theSet.hasMandatoryOutsideSet({ "width" }, foundName));
    EXPECT_FALSE(theSet.width.hasMandatory());
    theSet.width.setMax(240);
    EXPECT_TRUE(theSet.width.hasMandatory());
    EXPECT_TRUE(theSet.hasMandatory());
    EXPECT_FALSE(theSet.hasMandatoryOutsideSet({ "width" }, foundName));
    EXPECT_TRUE(theSet.hasMandatoryOutsideSet({ "height" }, foundName));
    EXPECT_EQ("width", foundName);
    theSet.googPayloadPadding.setExact(true);
    EXPECT_TRUE(theSet.hasMandatoryOutsideSet({ "width" }, foundName));
    EXPECT_EQ("googPayloadPadding", foundName);
}

TEST(MediaTrackConstraintsTest, SetToString)
{
    WebMediaTrackConstraintSet theSet;
    EXPECT_EQ("", theSet.toString());
    theSet.width.setMax(240);
    EXPECT_EQ("width: {max: 240}", theSet.toString().utf8());
    theSet.echoCancellation.setIdeal(true);
    EXPECT_EQ("width: {max: 240}, echoCancellation: {ideal: true}", theSet.toString().utf8());
}

TEST(MediaTrackConstraintsTest, ConstraintsToString)
{
    WebMediaConstraints theConstraints;
    WebMediaTrackConstraintSet basic;
    WebVector<WebMediaTrackConstraintSet> advanced(static_cast<size_t>(1));
    basic.width.setMax(240);
    advanced[0].echoCancellation.setExact(true);
    theConstraints.initialize(basic, advanced);
    EXPECT_EQ("{width: {max: 240}, advanced: [{echoCancellation: {exact: true}}]}", theConstraints.toString().utf8());

    WebMediaConstraints nullConstraints;
    EXPECT_EQ("", nullConstraints.toString().utf8());
}

TEST(MediaTrackConstraintsTest, ConvertWebConstraintsBasic)
{
    WebMediaConstraints input;
    MediaTrackConstraints output;

    MediaConstraintsImpl::convertConstraints(input, output);
}

TEST(MediaTrackConstraintsTest, ConvertWebSingleStringConstraint)
{
    WebMediaConstraints input;
    MediaTrackConstraints output;

    WebMediaTrackConstraintSet basic;
    WebVector<WebMediaTrackConstraintSet> advanced;

    basic.facingMode.setIdeal(WebVector<WebString>(&"foo", 1));
    input.initialize(basic, advanced);
    MediaConstraintsImpl::convertConstraints(input, output);
    ASSERT_TRUE(output.hasFacingMode());
    ASSERT_TRUE(output.facingMode().isString());
    EXPECT_EQ("foo", output.facingMode().getAsString());
}

TEST(MediaTrackConstraintsTest, ConvertWebDoubleStringConstraint)
{
    WebMediaConstraints input;
    MediaTrackConstraints output;

    WebVector<WebString> buffer(static_cast<size_t>(2u));
    buffer[0] = "foo";
    buffer[1] = "bar";

    WebMediaTrackConstraintSet basic;
    std::vector<WebMediaTrackConstraintSet> advanced;
    basic.facingMode.setIdeal(buffer);
    input.initialize(basic, advanced);
    MediaConstraintsImpl::convertConstraints(input, output);
    ASSERT_TRUE(output.hasFacingMode());
    ASSERT_TRUE(output.facingMode().isStringSequence());
    auto outBuffer = output.facingMode().getAsStringSequence();
    EXPECT_EQ("foo", outBuffer[0]);
    EXPECT_EQ("bar", outBuffer[1]);
}

TEST(MediaTrackConstraintsTest, ConvertBlinkStringConstraint)
{
    MediaTrackConstraints input;
    WebMediaConstraints output;
    StringOrStringSequenceOrConstrainDOMStringParameters parameter;
    parameter.setString("foo");
    input.setFacingMode(parameter);
    output = MediaConstraintsImpl::convertConstraintsToWeb(input);
    ASSERT_TRUE(output.basic().facingMode.hasIdeal());
    ASSERT_EQ(1U, output.basic().facingMode.ideal().size());
    ASSERT_EQ("foo", output.basic().facingMode.ideal()[0]);
}

TEST(MediaTrackConstraintsTest, ConvertBlinkComplexStringConstraint)
{
    MediaTrackConstraints input;
    WebMediaConstraints output;
    StringOrStringSequenceOrConstrainDOMStringParameters parameter;
    ConstrainDOMStringParameters subparameter;
    StringOrStringSequence innerString;
    innerString.setString("foo");
    subparameter.setIdeal(innerString);
    parameter.setConstrainDOMStringParameters(subparameter);
    input.setFacingMode(parameter);
    output = MediaConstraintsImpl::convertConstraintsToWeb(input);
    ASSERT_TRUE(output.basic().facingMode.hasIdeal());
    ASSERT_EQ(1U, output.basic().facingMode.ideal().size());
    ASSERT_EQ("foo", output.basic().facingMode.ideal()[0]);

    // Convert this back, and see that it appears as a single string.
    MediaTrackConstraints recycled;
    MediaConstraintsImpl::convertConstraints(output, recycled);
    ASSERT_TRUE(recycled.hasFacingMode());
    ASSERT_TRUE(recycled.facingMode().isString());
    ASSERT_EQ("foo", recycled.facingMode().getAsString());
}

} // namespace blink
