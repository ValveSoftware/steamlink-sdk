// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power/origin_power_map.h"

#include <stddef.h>

#include "base/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

bool HostFilter(const std::string& host, const GURL& url) {
  return url.host() == host;
}

}  // namespace

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

TEST(OriginPowerMapTest, ClearOriginMap) {
  GURL url1("https://www.google.com");
  GURL url2("https://www.chrome.com");
  GURL url3("https://www.example.com");
  GURL url4("http://www.chrome.com");
  GURL url5("http://www.example.com");

  // Add all 5 URLs to the map.
  OriginPowerMap origin_power_map;
  origin_power_map.AddPowerForOrigin(url1, 50);
  origin_power_map.AddPowerForOrigin(url2, 20);
  origin_power_map.AddPowerForOrigin(url3, 15);
  origin_power_map.AddPowerForOrigin(url4, 5);
  origin_power_map.AddPowerForOrigin(url5, 10);
  EXPECT_DOUBLE_EQ(50, origin_power_map.GetPowerForOrigin(url1));
  EXPECT_DOUBLE_EQ(20, origin_power_map.GetPowerForOrigin(url2));
  EXPECT_DOUBLE_EQ(15, origin_power_map.GetPowerForOrigin(url3));
  EXPECT_DOUBLE_EQ(5, origin_power_map.GetPowerForOrigin(url4));
  EXPECT_DOUBLE_EQ(10, origin_power_map.GetPowerForOrigin(url5));

  // Delete |url1|.
  origin_power_map.ClearOriginMap(base::Bind(
      static_cast<bool (*)(const GURL&, const GURL&)>(operator==), url1));
  EXPECT_DOUBLE_EQ(0, origin_power_map.GetPowerForOrigin(url1));
  EXPECT_DOUBLE_EQ(40, origin_power_map.GetPowerForOrigin(url2));
  EXPECT_DOUBLE_EQ(30, origin_power_map.GetPowerForOrigin(url3));
  EXPECT_DOUBLE_EQ(10, origin_power_map.GetPowerForOrigin(url4));
  EXPECT_DOUBLE_EQ(20, origin_power_map.GetPowerForOrigin(url5));

  // Delete every URL with the host "www.chrome.com", i.e. |url2| and |url4|.
  origin_power_map.ClearOriginMap(base::Bind(&HostFilter, "www.chrome.com"));
  EXPECT_DOUBLE_EQ(0, origin_power_map.GetPowerForOrigin(url1));
  EXPECT_DOUBLE_EQ(0, origin_power_map.GetPowerForOrigin(url2));
  EXPECT_DOUBLE_EQ(60, origin_power_map.GetPowerForOrigin(url3));
  EXPECT_DOUBLE_EQ(0, origin_power_map.GetPowerForOrigin(url4));
  EXPECT_DOUBLE_EQ(40, origin_power_map.GetPowerForOrigin(url5));

  // Delete every URL with the host "www.example.org". There should be none.
  origin_power_map.ClearOriginMap(base::Bind(&HostFilter, "www.example.org"));
  EXPECT_DOUBLE_EQ(0, origin_power_map.GetPowerForOrigin(url1));
  EXPECT_DOUBLE_EQ(0, origin_power_map.GetPowerForOrigin(url2));
  EXPECT_DOUBLE_EQ(60, origin_power_map.GetPowerForOrigin(url3));
  EXPECT_DOUBLE_EQ(0, origin_power_map.GetPowerForOrigin(url4));
  EXPECT_DOUBLE_EQ(40, origin_power_map.GetPowerForOrigin(url5));

  // Null callback means complete deletion.
  origin_power_map.ClearOriginMap(base::Callback<bool(const GURL&)>());
  EXPECT_DOUBLE_EQ(0, origin_power_map.GetPowerForOrigin(url1));
  EXPECT_DOUBLE_EQ(0, origin_power_map.GetPowerForOrigin(url2));
  EXPECT_DOUBLE_EQ(0, origin_power_map.GetPowerForOrigin(url3));
  EXPECT_DOUBLE_EQ(0, origin_power_map.GetPowerForOrigin(url4));
  EXPECT_DOUBLE_EQ(0, origin_power_map.GetPowerForOrigin(url5));
}

}  // namespace power
