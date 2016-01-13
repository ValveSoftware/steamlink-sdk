// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_writer.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(JSONWriterTest, BasicTypes) {
  std::string output_js;

  // Test null.
  Value* root = Value::CreateNullValue();
  EXPECT_TRUE(JSONWriter::Write(root, &output_js));
  EXPECT_EQ("null", output_js);
  delete root;

  // Test empty dict.
  root = new DictionaryValue;
  EXPECT_TRUE(JSONWriter::Write(root, &output_js));
  EXPECT_EQ("{}", output_js);
  delete root;

  // Test empty list.
  root = new ListValue;
  EXPECT_TRUE(JSONWriter::Write(root, &output_js));
  EXPECT_EQ("[]", output_js);
  delete root;

  // Test integer values.
  root = new FundamentalValue(42);
  EXPECT_TRUE(JSONWriter::Write(root, &output_js));
  EXPECT_EQ("42", output_js);
  delete root;

  // Test boolean values.
  root = new FundamentalValue(true);
  EXPECT_TRUE(JSONWriter::Write(root, &output_js));
  EXPECT_EQ("true", output_js);
  delete root;

  // Test Real values should always have a decimal or an 'e'.
  root = new FundamentalValue(1.0);
  EXPECT_TRUE(JSONWriter::Write(root, &output_js));
  EXPECT_EQ("1.0", output_js);
  delete root;

  // Test Real values in the the range (-1, 1) must have leading zeros
  root = new FundamentalValue(0.2);
  EXPECT_TRUE(JSONWriter::Write(root, &output_js));
  EXPECT_EQ("0.2", output_js);
  delete root;

  // Test Real values in the the range (-1, 1) must have leading zeros
  root = new FundamentalValue(-0.8);
  EXPECT_TRUE(JSONWriter::Write(root, &output_js));
  EXPECT_EQ("-0.8", output_js);
  delete root;

  // Test String values.
  root = new StringValue("foo");
  EXPECT_TRUE(JSONWriter::Write(root, &output_js));
  EXPECT_EQ("\"foo\"", output_js);
  delete root;
}


TEST(JSONWriterTest, NestedTypes) {
  std::string output_js;

  // Writer unittests like empty list/dict nesting,
  // list list nesting, etc.
  DictionaryValue root_dict;
  ListValue* list = new ListValue;
  root_dict.Set("list", list);
  DictionaryValue* inner_dict = new DictionaryValue;
  list->Append(inner_dict);
  inner_dict->SetInteger("inner int", 10);
  ListValue* inner_list = new ListValue;
  list->Append(inner_list);
  list->Append(new FundamentalValue(true));

  // Test the pretty-printer.
  EXPECT_TRUE(JSONWriter::Write(&root_dict, &output_js));
  EXPECT_EQ("{\"list\":[{\"inner int\":10},[],true]}", output_js);
  EXPECT_TRUE(JSONWriter::WriteWithOptions(&root_dict,
                                           JSONWriter::OPTIONS_PRETTY_PRINT,
                                           &output_js));

  // The pretty-printer uses a different newline style on Windows than on
  // other platforms.
#if defined(OS_WIN)
#define JSON_NEWLINE "\r\n"
#else
#define JSON_NEWLINE "\n"
#endif
  EXPECT_EQ("{" JSON_NEWLINE
            "   \"list\": [ {" JSON_NEWLINE
            "      \"inner int\": 10" JSON_NEWLINE
            "   }, [  ], true ]" JSON_NEWLINE
            "}" JSON_NEWLINE,
            output_js);
#undef JSON_NEWLINE
}

TEST(JSONWriterTest, KeysWithPeriods) {
  std::string output_js;

  DictionaryValue period_dict;
  period_dict.SetWithoutPathExpansion("a.b", new FundamentalValue(3));
  period_dict.SetWithoutPathExpansion("c", new FundamentalValue(2));
  DictionaryValue* period_dict2 = new DictionaryValue;
  period_dict2->SetWithoutPathExpansion("g.h.i.j", new FundamentalValue(1));
  period_dict.SetWithoutPathExpansion("d.e.f", period_dict2);
  EXPECT_TRUE(JSONWriter::Write(&period_dict, &output_js));
  EXPECT_EQ("{\"a.b\":3,\"c\":2,\"d.e.f\":{\"g.h.i.j\":1}}", output_js);

  DictionaryValue period_dict3;
  period_dict3.Set("a.b", new FundamentalValue(2));
  period_dict3.SetWithoutPathExpansion("a.b", new FundamentalValue(1));
  EXPECT_TRUE(JSONWriter::Write(&period_dict3, &output_js));
  EXPECT_EQ("{\"a\":{\"b\":2},\"a.b\":1}", output_js);
}

TEST(JSONWriterTest, BinaryValues) {
  std::string output_js;

  // Binary values should return errors unless suppressed via the
  // OPTIONS_OMIT_BINARY_VALUES flag.
  Value* root = BinaryValue::CreateWithCopiedBuffer("asdf", 4);
  EXPECT_FALSE(JSONWriter::Write(root, &output_js));
  EXPECT_TRUE(JSONWriter::WriteWithOptions(
      root, JSONWriter::OPTIONS_OMIT_BINARY_VALUES, &output_js));
  EXPECT_TRUE(output_js.empty());
  delete root;

  ListValue binary_list;
  binary_list.Append(BinaryValue::CreateWithCopiedBuffer("asdf", 4));
  binary_list.Append(new FundamentalValue(5));
  binary_list.Append(BinaryValue::CreateWithCopiedBuffer("asdf", 4));
  binary_list.Append(new FundamentalValue(2));
  binary_list.Append(BinaryValue::CreateWithCopiedBuffer("asdf", 4));
  EXPECT_FALSE(JSONWriter::Write(&binary_list, &output_js));
  EXPECT_TRUE(JSONWriter::WriteWithOptions(
      &binary_list, JSONWriter::OPTIONS_OMIT_BINARY_VALUES, &output_js));
  EXPECT_EQ("[5,2]", output_js);

  DictionaryValue binary_dict;
  binary_dict.Set("a", BinaryValue::CreateWithCopiedBuffer("asdf", 4));
  binary_dict.Set("b", new FundamentalValue(5));
  binary_dict.Set("c", BinaryValue::CreateWithCopiedBuffer("asdf", 4));
  binary_dict.Set("d", new FundamentalValue(2));
  binary_dict.Set("e", BinaryValue::CreateWithCopiedBuffer("asdf", 4));
  EXPECT_FALSE(JSONWriter::Write(&binary_dict, &output_js));
  EXPECT_TRUE(JSONWriter::WriteWithOptions(
      &binary_dict, JSONWriter::OPTIONS_OMIT_BINARY_VALUES, &output_js));
  EXPECT_EQ("{\"b\":5,\"d\":2}", output_js);
}

TEST(JSONWriterTest, DoublesAsInts) {
  std::string output_js;

  // Test allowing a double with no fractional part to be written as an integer.
  FundamentalValue double_value(1e10);
  EXPECT_TRUE(JSONWriter::WriteWithOptions(
      &double_value,
      JSONWriter::OPTIONS_OMIT_DOUBLE_TYPE_PRESERVATION,
      &output_js));
  EXPECT_EQ("10000000000", output_js);
}

}  // namespace base
