// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/custom/CustomElementDescriptor.h"

#include "core/dom/custom/CustomElementDescriptorHash.h"
#include "core/dom/custom/CustomElementTestHelpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/HashSet.h"
#include "wtf/text/AtomicString.h"

namespace blink {

class Element;

TEST(CustomElementDescriptorTest, equal)
{
    CustomElementDescriptor myTypeExtension("my-button", "button");
    CustomElementDescriptor again("my-button", "button");
    EXPECT_TRUE(myTypeExtension == again)
        << "two descriptors with the same name and local name should be equal";
}

TEST(CustomElementDescriptorTest, notEqual)
{
    CustomElementDescriptor myTypeExtension("my-button", "button");
    CustomElementDescriptor collidingNewType("my-button", "my-button");
    EXPECT_FALSE(myTypeExtension == collidingNewType)
        << "type extension should not be equal to a non-type extension";
}

TEST(CustomElementDescriptorTest, hashable)
{
    HashSet<CustomElementDescriptor> descriptors;
    descriptors.add(CustomElementDescriptor("foo-bar", "foo-bar"));
    EXPECT_TRUE(
        descriptors.contains(CustomElementDescriptor("foo-bar", "foo-bar")))
        << "the identical descriptor should be found in the hash set";
    EXPECT_FALSE(descriptors.contains(CustomElementDescriptor(
        "bad-poetry",
        "blockquote")))
        << "an unrelated descriptor should not be found in the hash set";
}

TEST(CustomElementDescriptorTest, matches_autonomous)
{
    CustomElementDescriptor descriptor("a-b", "a-b");
    Element* element = CreateElement("a-b");
    EXPECT_TRUE(descriptor.matches(*element));
}

TEST(CustomElementDescriptorTest,
    matches_autonomous_shouldNotMatchCustomizedBuiltInElement)
{
    CustomElementDescriptor descriptor("a-b", "a-b");
    Element* element = CreateElement("futuretag").withIsAttribute("a-b");
    EXPECT_FALSE(descriptor.matches(*element));
}

TEST(CustomElementDescriptorTest, matches_customizedBuiltIn)
{
    CustomElementDescriptor descriptor("a-b", "button");
    Element* element = CreateElement("button").withIsAttribute("a-b");
    EXPECT_TRUE(descriptor.matches(*element));
}

TEST(CustomElementDescriptorTest,
    matches_customizedBuiltIn_shouldNotMatchAutonomousElement)
{
    CustomElementDescriptor descriptor("a-b", "button");
    Element* element = CreateElement("a-b");
    EXPECT_FALSE(descriptor.matches(*element));
}

TEST(CustomElementDescriptorTest, matches_elementNotInHTMLNamespaceDoesNotMatch)
{
    CustomElementDescriptor descriptor("a-b", "a-b");
    Element* element = CreateElement("a-b").inNamespace("data:text/plain,foo");
    EXPECT_FALSE(descriptor.matches(*element));
}

} // namespace blink
