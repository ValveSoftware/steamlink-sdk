// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ptr_util.h"
#include "content/browser/service_manager/merge_dictionary.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

using MergeDictionaryTest = testing::Test;

TEST(MergeDictionaryTest, Merge) {
  // |source| & |target| have three properties, "string", "list" and
  // "dictionary", which are then merged.
  base::DictionaryValue target;
  target.SetString("string", "Hello, ");
  std::unique_ptr<base::DictionaryValue> dict_value_original(
      base::MakeUnique<base::DictionaryValue>());
  dict_value_original->SetString("key1", "original");
  dict_value_original->SetString("key3", "original");
  target.Set("dictionary", std::move(dict_value_original));
  std::unique_ptr<base::ListValue> list(base::MakeUnique<base::ListValue>());
  list->AppendString("A");
  list->AppendString("B");
  target.Set("list", std::move(list));

  base::DictionaryValue source;
  source.SetString("string", "World!");
  std::unique_ptr<base::DictionaryValue> dict_value_replacement(
      base::MakeUnique<base::DictionaryValue>());
  dict_value_replacement->SetString("key1", "new");
  dict_value_replacement->SetString("key2", "new");
  source.Set("dictionary", std::move(dict_value_replacement));
  list = base::MakeUnique<base::ListValue>();
  list->AppendString("C");
  source.Set("list", std::move(list));

  MergeDictionary(&target, &source);

  // Simple string value should have been clobbered.
  std::string out_string;
  EXPECT_TRUE(target.GetString("string", &out_string));
  EXPECT_EQ(out_string, "World!");

  // Dictionary should have been merged, with key1 being clobbered, key2 added
  // and key3 preserved.
  base::DictionaryValue* out_dictionary = nullptr;
  EXPECT_TRUE(target.GetDictionary("dictionary", &out_dictionary));
  EXPECT_EQ(3u, out_dictionary->size());
  std::string value1, value2, value3;
  EXPECT_TRUE(out_dictionary->GetString("key1", &value1));
  EXPECT_TRUE(out_dictionary->GetString("key2", &value2));
  EXPECT_TRUE(out_dictionary->GetString("key3", &value3));
  EXPECT_EQ(value1, "new");
  EXPECT_EQ(value2, "new");
  EXPECT_EQ(value3, "original");

  // List should have been merged, with the items from source appended to the
  // items from target.
  base::ListValue* out_list = nullptr;
  EXPECT_TRUE(target.GetList("list", &out_list));
  EXPECT_EQ(3u, out_list->GetSize());
  std::string a, b, c;
  EXPECT_TRUE(out_list->GetString(0, &a));
  EXPECT_TRUE(out_list->GetString(1, &b));
  EXPECT_TRUE(out_list->GetString(2, &c));
  EXPECT_EQ("A", a);
  EXPECT_EQ("B", b);
  EXPECT_EQ("C", c);
}

}  // namespace content
