// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/MediaQueryMatcher.h"

#include "core/MediaTypeNames.h"
#include "core/css/MediaList.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

TEST(MediaQueryMatcherTest, LostFrame)
{
    std::unique_ptr<DummyPageHolder> pageHolder = DummyPageHolder::create(IntSize(500, 500));
    MediaQueryMatcher* matcher = MediaQueryMatcher::create(pageHolder->document());
    MediaQuerySet* querySet = MediaQuerySet::create(MediaTypeNames::all);
    ASSERT_TRUE(matcher->evaluate(querySet));

    matcher->documentDetached();
    ASSERT_FALSE(matcher->evaluate(querySet));
}

} // namespace blink
