// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power/origin_power_map.h"

#include <stddef.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace power {

TEST(OriginPowerMapTest, StartEmpty) {
  OriginPowerMap origin_power_map;
  EXPECT_EQ(size_t(0), origin_power_map.GetPercentOriginMap().size());
}

TEST(OriginPowerMapTest, AddOneOriginNotInMap) {
  OriginPowerMap origin_power_map;
  GURL url("http://www.google.com");
  EXPECT_EQ(0, origin_power_map.GetPowerForOrigin(url));
  origin_power_map.AddPowerForOrigin(url, 10);
  EXPECT_EQ(size_t(1), origin_power_map.GetPercentOriginMap().size());
  EXPECT_EQ(100, origin_power_map.GetPowerForOrigin(url));
}

TEST(OriginPowerMapTest, AddMultiplesOrigins) {
  OriginPowerMap origin_power_map;
  GURL url1("http://www.google.com");
  EXPECT_EQ(0, origin_power_map.GetPowerForOrigin(url1));
  origin_power_map.AddPowerForOrigin(url1, 10);
  EXPECT_EQ(size_t(1), origin_power_map.GetPercentOriginMap().size());
  EXPECT_EQ(100, origin_power_map.GetPowerForOrigin(url1));

  GURL url2("http://www.example.com");
  origin_power_map.AddPowerForOrigin(url2, 30);
  EXPECT_EQ(25, origin_power_map.GetPowerForOrigin(url1));
  EXPECT_EQ(75, origin_power_map.GetPowerForOrigin(url2));
  origin_power_map.AddPowerForOrigin(url2, 10);
  EXPECT_EQ(20, origin_power_map.GetPowerForOrigin(url1));
  EXPECT_EQ(80, origin_power_map.GetPowerForOrigin(url2));

  GURL url3("https://www.google.com");
  origin_power_map.AddPowerForOrigin(url3, 50);
  EXPECT_EQ(10, origin_power_map.GetPowerForOrigin(url1));
  EXPECT_EQ(40, origin_power_map.GetPowerForOrigin(url2));
  EXPECT_EQ(50, origin_power_map.GetPowerForOrigin(url3));
}

TEST(OriginPowerMapTest, PercentOriginMap) {
  OriginPowerMap origin_power_map;
  GURL url1("http://www.google.com");
  GURL url2("http://www.example.com");
  origin_power_map.AddPowerForOrigin(url1, 10);
  origin_power_map.AddPowerForOrigin(url2, 40);
  OriginPowerMap::PercentOriginMap origin_map =
      origin_power_map.GetPercentOriginMap();
  EXPECT_EQ(20, origin_map.find(url1)->second);
  EXPECT_EQ(80, origin_map.find(url2)->second);
}

TEST(OriginPowerMapTest, EmptyPercentOriginMapWhenZeroConsumed) {
  OriginPowerMap origin_power_map;
  EXPECT_EQ(size_t(0), origin_power_map.GetPercentOriginMap().size());
  GURL url("http://www.google.com");
  origin_power_map.AddPowerForOrigin(url, 0);
  EXPECT_EQ(size_t(0), origin_power_map.GetPercentOriginMap().size());
}

}  // namespace power
