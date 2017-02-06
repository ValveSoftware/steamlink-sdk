// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/FilteredComputedStylePropertyMap.h"

#include "core/css/CSSComputedStyleDeclaration.h"
#include "core/dom/Element.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/heap/Handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

class FilteredComputedStylePropertyMapTest : public ::testing::Test {
public:
    FilteredComputedStylePropertyMapTest()
        : m_page(DummyPageHolder::create())
    {
        m_declaration = CSSComputedStyleDeclaration::create(m_page->document().documentElement());
    }

    CSSComputedStyleDeclaration* declaration() const { return m_declaration.get(); }

private:
    std::unique_ptr<DummyPageHolder> m_page;
    Persistent<CSSComputedStyleDeclaration> m_declaration;
};

TEST_F(FilteredComputedStylePropertyMapTest, GetProperties)
{
    Vector<CSSPropertyID> nativeProperties({CSSPropertyColor, CSSPropertyAlignItems});
    Vector<CSSPropertyID> emptyNativeProperties;
    Vector<AtomicString> customProperties({"--foo", "--bar"});
    Vector<AtomicString> emptyCustomProperties;

    FilteredComputedStylePropertyMap* map = FilteredComputedStylePropertyMap::create(declaration(), nativeProperties, customProperties);
    EXPECT_TRUE(map->getProperties().contains("color"));
    EXPECT_TRUE(map->getProperties().contains("align-items"));
    EXPECT_TRUE(map->getProperties().contains("--foo"));
    EXPECT_TRUE(map->getProperties().contains("--bar"));

    map = FilteredComputedStylePropertyMap::create(declaration(), nativeProperties, emptyCustomProperties);
    EXPECT_TRUE(map->getProperties().contains("color"));
    EXPECT_TRUE(map->getProperties().contains("align-items"));

    map = FilteredComputedStylePropertyMap::create(declaration(), emptyNativeProperties, customProperties);
    EXPECT_TRUE(map->getProperties().contains("--foo"));
    EXPECT_TRUE(map->getProperties().contains("--bar"));
}

TEST_F(FilteredComputedStylePropertyMapTest, NativePropertyAccessors)
{
    Vector<CSSPropertyID> nativeProperties({CSSPropertyColor, CSSPropertyAlignItems});
    Vector<AtomicString> emptyCustomProperties;
    FilteredComputedStylePropertyMap* map = FilteredComputedStylePropertyMap::create(declaration(), nativeProperties, emptyCustomProperties);

    TrackExceptionState exceptionState;

    map->get("color", exceptionState);
    EXPECT_FALSE(exceptionState.hadException());

    map->has("color", exceptionState);
    EXPECT_FALSE(exceptionState.hadException());

    map->getAll("color", exceptionState);
    EXPECT_FALSE(exceptionState.hadException());

    map->get("align-contents", exceptionState);
    EXPECT_TRUE(exceptionState.hadException());
    exceptionState.clearException();

    map->has("align-contents", exceptionState);
    EXPECT_TRUE(exceptionState.hadException());
    exceptionState.clearException();

    map->getAll("align-contents", exceptionState);
    EXPECT_TRUE(exceptionState.hadException());
    exceptionState.clearException();
}

TEST_F(FilteredComputedStylePropertyMapTest, CustomPropertyAccessors)
{
    Vector<CSSPropertyID> emptyNativeProperties;
    Vector<AtomicString> customProperties({"--foo", "--bar"});
    FilteredComputedStylePropertyMap* map = FilteredComputedStylePropertyMap::create(declaration(), emptyNativeProperties, customProperties);

    TrackExceptionState exceptionState;

    map->get("--foo", exceptionState);
    EXPECT_FALSE(exceptionState.hadException());

    map->has("--foo", exceptionState);
    EXPECT_FALSE(exceptionState.hadException());

    map->getAll("--foo", exceptionState);
    EXPECT_FALSE(exceptionState.hadException());

    map->get("--quix", exceptionState);
    EXPECT_TRUE(exceptionState.hadException());
    exceptionState.clearException();

    map->has("--quix", exceptionState);
    EXPECT_TRUE(exceptionState.hadException());
    exceptionState.clearException();

    map->getAll("--quix", exceptionState);
    EXPECT_TRUE(exceptionState.hadException());
    exceptionState.clearException();
}

} // namespace blink
