// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/custom_data_helper.h"

#include <utility>

#include "base/pickle.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace ui {

namespace {

void PrepareEmptyTestData(Pickle* pickle) {
  std::map<base::string16, base::string16> data;
  WriteCustomDataToPickle(data, pickle);
}

void PrepareTestData(Pickle* pickle) {
  std::map<base::string16, base::string16> data;
  data.insert(std::make_pair(ASCIIToUTF16("abc"), base::string16()));
  data.insert(std::make_pair(ASCIIToUTF16("de"), ASCIIToUTF16("1")));
  data.insert(std::make_pair(ASCIIToUTF16("f"), ASCIIToUTF16("23")));
  WriteCustomDataToPickle(data, pickle);
}

TEST(CustomDataHelperTest, EmptyReadTypes) {
  Pickle pickle;
  PrepareEmptyTestData(&pickle);

  std::vector<base::string16> types;
  ReadCustomDataTypes(pickle.data(), pickle.size(), &types);
  EXPECT_EQ(0u, types.size());
}

TEST(CustomDataHelperTest, EmptyReadSingleType) {
  Pickle pickle;
  PrepareEmptyTestData(&pickle);

  base::string16 result;
  ReadCustomDataForType(pickle.data(),
                        pickle.size(),
                        ASCIIToUTF16("f"),
                        &result);
  EXPECT_EQ(base::string16(), result);
}

TEST(CustomDataHelperTest, EmptyReadMap) {
  Pickle pickle;
  PrepareEmptyTestData(&pickle);

  std::map<base::string16, base::string16> result;
  ReadCustomDataIntoMap(pickle.data(), pickle.size(), &result);
  EXPECT_EQ(0u, result.size());
}

TEST(CustomDataHelperTest, ReadTypes) {
  Pickle pickle;
  PrepareTestData(&pickle);

  std::vector<base::string16> types;
  ReadCustomDataTypes(pickle.data(), pickle.size(), &types);

  std::vector<base::string16> expected;
  expected.push_back(ASCIIToUTF16("abc"));
  expected.push_back(ASCIIToUTF16("de"));
  expected.push_back(ASCIIToUTF16("f"));
  EXPECT_EQ(expected, types);
}

TEST(CustomDataHelperTest, ReadSingleType) {
  Pickle pickle;
  PrepareTestData(&pickle);

  base::string16 result;
  ReadCustomDataForType(pickle.data(),
                        pickle.size(),
                        ASCIIToUTF16("abc"),
                        &result);
  EXPECT_EQ(base::string16(), result);

  ReadCustomDataForType(pickle.data(),
                        pickle.size(),
                        ASCIIToUTF16("de"),
                        &result);
  EXPECT_EQ(ASCIIToUTF16("1"), result);

  ReadCustomDataForType(pickle.data(),
                        pickle.size(),
                        ASCIIToUTF16("f"),
                        &result);
  EXPECT_EQ(ASCIIToUTF16("23"), result);
}

TEST(CustomDataHelperTest, ReadMap) {
  Pickle pickle;
  PrepareTestData(&pickle);

  std::map<base::string16, base::string16> result;
  ReadCustomDataIntoMap(pickle.data(), pickle.size(), &result);

  std::map<base::string16, base::string16> expected;
  expected.insert(std::make_pair(ASCIIToUTF16("abc"), base::string16()));
  expected.insert(std::make_pair(ASCIIToUTF16("de"), ASCIIToUTF16("1")));
  expected.insert(std::make_pair(ASCIIToUTF16("f"), ASCIIToUTF16("23")));
  EXPECT_EQ(expected, result);
}

TEST(CustomDataHelperTest, BadReadTypes) {
  // ReadCustomDataTypes makes the additional guarantee that the contents of the
  // result vector will not change if the input is malformed.
  std::vector<base::string16> expected;
  expected.push_back(ASCIIToUTF16("abc"));
  expected.push_back(ASCIIToUTF16("de"));
  expected.push_back(ASCIIToUTF16("f"));

  Pickle malformed;
  malformed.WriteUInt64(1000);
  malformed.WriteString16(ASCIIToUTF16("hello"));
  malformed.WriteString16(ASCIIToUTF16("world"));
  std::vector<base::string16> actual(expected);
  ReadCustomDataTypes(malformed.data(), malformed.size(), &actual);
  EXPECT_EQ(expected, actual);

  Pickle malformed2;
  malformed2.WriteUInt64(1);
  malformed2.WriteString16(ASCIIToUTF16("hello"));
  std::vector<base::string16> actual2(expected);
  ReadCustomDataTypes(malformed2.data(), malformed2.size(), &actual2);
  EXPECT_EQ(expected, actual2);
}

TEST(CustomDataHelperTest, BadPickle) {
  base::string16 result_data;
  std::map<base::string16, base::string16> result_map;

  Pickle malformed;
  malformed.WriteUInt64(1000);
  malformed.WriteString16(ASCIIToUTF16("hello"));
  malformed.WriteString16(ASCIIToUTF16("world"));

  ReadCustomDataForType(malformed.data(),
                        malformed.size(),
                        ASCIIToUTF16("f"),
                        &result_data);
  ReadCustomDataIntoMap(malformed.data(), malformed.size(), &result_map);
  EXPECT_EQ(0u, result_data.size());
  EXPECT_EQ(0u, result_map.size());

  Pickle malformed2;
  malformed2.WriteUInt64(1);
  malformed2.WriteString16(ASCIIToUTF16("hello"));

  ReadCustomDataForType(malformed2.data(),
                        malformed2.size(),
                        ASCIIToUTF16("f"),
                        &result_data);
  ReadCustomDataIntoMap(malformed2.data(), malformed2.size(), &result_map);
  EXPECT_EQ(0u, result_data.size());
  EXPECT_EQ(0u, result_map.size());
}

}  // namespace

}  // namespace ui
