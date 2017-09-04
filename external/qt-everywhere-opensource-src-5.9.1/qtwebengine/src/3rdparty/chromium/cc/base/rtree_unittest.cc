// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/rtree.h"

#include <stddef.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

TEST(RTreeTest, NoOverlap) {
  std::vector<gfx::Rect> rects;
  for (int y = 0; y < 50; ++y) {
    for (int x = 0; x < 50; ++x) {
      rects.push_back(gfx::Rect(x, y, 1, 1));
    }
  }

  RTree rtree;
  rtree.Build(rects);

  std::vector<size_t> results;
  rtree.Search(gfx::Rect(0, 0, 50, 50), &results);
  ASSERT_EQ(2500u, results.size());
  for (size_t i = 0; i < 2500; ++i) {
    ASSERT_EQ(results[i], i);
  }

  results.clear();
  rtree.Search(gfx::Rect(0, 0, 50, 49), &results);
  ASSERT_EQ(2450u, results.size());
  for (size_t i = 0; i < 2450; ++i) {
    ASSERT_EQ(results[i], i);
  }

  results.clear();
  rtree.Search(gfx::Rect(5, 6, 1, 1), &results);
  ASSERT_EQ(1u, results.size());
  EXPECT_EQ(6u * 50 + 5u, results[0]);
}

TEST(RTreeTest, Overlap) {
  std::vector<gfx::Rect> rects;
  for (int h = 1; h <= 50; ++h) {
    for (int w = 1; w <= 50; ++w) {
      rects.push_back(gfx::Rect(0, 0, w, h));
    }
  }

  RTree rtree;
  rtree.Build(rects);

  std::vector<size_t> results;
  rtree.Search(gfx::Rect(0, 0, 1, 1), &results);
  ASSERT_EQ(2500u, results.size());
  for (size_t i = 0; i < 2500; ++i) {
    ASSERT_EQ(results[i], i);
  }

  results.clear();
  rtree.Search(gfx::Rect(0, 49, 1, 1), &results);
  ASSERT_EQ(50u, results.size());
  for (size_t i = 0; i < 50; ++i) {
    EXPECT_EQ(results[i], 2450u + i);
  }
}

TEST(RTreeTest, GetBoundsEmpty) {
  RTree rtree;
  ASSERT_EQ(gfx::Rect(), rtree.GetBounds());
}

TEST(RTreeTest, GetBoundsNonOverlapping) {
  std::vector<gfx::Rect> rects;
  rects.push_back(gfx::Rect(5, 6, 7, 8));
  rects.push_back(gfx::Rect(11, 12, 13, 14));

  RTree rtree;
  rtree.Build(rects);

  ASSERT_EQ(gfx::Rect(5, 6, 19, 20), rtree.GetBounds());
}

TEST(RTreeTest, GetBoundsOverlapping) {
  std::vector<gfx::Rect> rects;
  rects.push_back(gfx::Rect(0, 0, 10, 10));
  rects.push_back(gfx::Rect(5, 5, 5, 5));

  RTree rtree;
  rtree.Build(rects);

  ASSERT_EQ(gfx::Rect(0, 0, 10, 10), rtree.GetBounds());
}

}  // namespace cc
