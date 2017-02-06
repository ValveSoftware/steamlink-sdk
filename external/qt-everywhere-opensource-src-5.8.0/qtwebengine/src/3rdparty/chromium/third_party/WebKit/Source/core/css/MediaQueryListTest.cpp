// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/MediaQueryList.h"

#include "core/css/MediaList.h"
#include "core/css/MediaQueryListListener.h"
#include "core/css/MediaQueryMatcher.h"
#include "core/dom/Document.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

class TestListener : public MediaQueryListListener {
public:
    void notifyMediaQueryChanged() override { }
};

} // anonymous namespace

TEST(MediaQueryListTest, CrashInStop)
{
    Document* document = Document::create();
    MediaQueryList* list = MediaQueryList::create(document, MediaQueryMatcher::create(*document), MediaQuerySet::create());
    list->addListener(new TestListener());
    list->stop();
    // This test passes if it's not crashed.
}

} // namespace blink
