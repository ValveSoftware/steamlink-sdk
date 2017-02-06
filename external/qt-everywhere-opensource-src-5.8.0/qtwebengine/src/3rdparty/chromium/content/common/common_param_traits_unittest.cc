// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/common_param_traits.h"

#include <stddef.h>
#include <string.h>

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/values.h"
#include "content/public/common/content_constants.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_utils.h"
#include "net/base/host_port_pair.h"
#include "printing/backend/print_backend.h"
#include "printing/page_range.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/ipc/gfx_param_traits.h"
#include "ui/gfx/ipc/skia/gfx_skia_param_traits.h"

// Tests std::pair serialization
TEST(IPCMessageTest, Pair) {
  typedef std::pair<std::string, std::string> TestPair;

  TestPair input("foo", "bar");
  IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
  IPC::ParamTraits<TestPair>::Write(&msg, input);

  TestPair output;
  base::PickleIterator iter(msg);
  EXPECT_TRUE(IPC::ParamTraits<TestPair>::Read(&msg, &iter, &output));
  EXPECT_EQ(output.first, "foo");
  EXPECT_EQ(output.second, "bar");
}

// Tests bitmap serialization.
TEST(IPCMessageTest, Bitmap) {
  SkBitmap bitmap;

  bitmap.allocN32Pixels(10, 5);
  memset(bitmap.getPixels(), 'A', bitmap.getSize());

  IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
  IPC::ParamTraits<SkBitmap>::Write(&msg, bitmap);

  SkBitmap output;
  base::PickleIterator iter(msg);
  EXPECT_TRUE(IPC::ParamTraits<SkBitmap>::Read(&msg, &iter, &output));

  EXPECT_EQ(bitmap.colorType(), output.colorType());
  EXPECT_EQ(bitmap.width(), output.width());
  EXPECT_EQ(bitmap.height(), output.height());
  EXPECT_EQ(bitmap.rowBytes(), output.rowBytes());
  EXPECT_EQ(bitmap.getSize(), output.getSize());
  EXPECT_EQ(memcmp(bitmap.getPixels(), output.getPixels(), bitmap.getSize()),
            0);

  // Also test the corrupt case.
  IPC::Message bad_msg(1, 2, IPC::Message::PRIORITY_NORMAL);
  // Copy the first message block over to |bad_msg|.
  const char* fixed_data;
  int fixed_data_size;
  iter = base::PickleIterator(msg);
  EXPECT_TRUE(iter.ReadData(&fixed_data, &fixed_data_size));
  bad_msg.WriteData(fixed_data, fixed_data_size);
  // Add some bogus pixel data.
  const size_t bogus_pixels_size = bitmap.getSize() * 2;
  std::unique_ptr<char[]> bogus_pixels(new char[bogus_pixels_size]);
  memset(bogus_pixels.get(), 'B', bogus_pixels_size);
  bad_msg.WriteData(bogus_pixels.get(), bogus_pixels_size);
  // Make sure we don't read out the bitmap!
  SkBitmap bad_output;
  iter = base::PickleIterator(bad_msg);
  EXPECT_FALSE(IPC::ParamTraits<SkBitmap>::Read(&bad_msg, &iter, &bad_output));
}

TEST(IPCMessageTest, ListValue) {
  base::ListValue input;
  input.Set(0, new base::FundamentalValue(42.42));
  input.Set(1, new base::StringValue("forty"));
  input.Set(2, base::Value::CreateNullValue());

  IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
  IPC::WriteParam(&msg, input);

  base::ListValue output;
  base::PickleIterator iter(msg);
  EXPECT_TRUE(IPC::ReadParam(&msg, &iter, &output));

  EXPECT_TRUE(input.Equals(&output));

  // Also test the corrupt case.
  IPC::Message bad_msg(1, 2, IPC::Message::PRIORITY_NORMAL);
  bad_msg.WriteInt(99);
  iter = base::PickleIterator(bad_msg);
  EXPECT_FALSE(IPC::ReadParam(&bad_msg, &iter, &output));
}

TEST(IPCMessageTest, DictionaryValue) {
  base::DictionaryValue input;
  input.Set("null", base::Value::CreateNullValue());
  input.Set("bool", new base::FundamentalValue(true));
  input.Set("int", new base::FundamentalValue(42));

  std::unique_ptr<base::DictionaryValue> subdict(new base::DictionaryValue());
  subdict->Set("str", new base::StringValue("forty two"));
  subdict->Set("bool", new base::FundamentalValue(false));

  std::unique_ptr<base::ListValue> sublist(new base::ListValue());
  sublist->Set(0, new base::FundamentalValue(42.42));
  sublist->Set(1, new base::StringValue("forty"));
  sublist->Set(2, new base::StringValue("two"));
  subdict->Set("list", sublist.release());

  input.Set("dict", subdict.release());

  IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
  IPC::WriteParam(&msg, input);

  base::DictionaryValue output;
  base::PickleIterator iter(msg);
  EXPECT_TRUE(IPC::ReadParam(&msg, &iter, &output));

  EXPECT_TRUE(input.Equals(&output));

  // Also test the corrupt case.
  IPC::Message bad_msg(1, 2, IPC::Message::PRIORITY_NORMAL);
  bad_msg.WriteInt(99);
  iter = base::PickleIterator(bad_msg);
  EXPECT_FALSE(IPC::ReadParam(&bad_msg, &iter, &output));
}

// Tests net::HostPortPair serialization
TEST(IPCMessageTest, HostPortPair) {
  net::HostPortPair input("host.com", 12345);

  IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
  IPC::ParamTraits<net::HostPortPair>::Write(&msg, input);

  net::HostPortPair output;
  base::PickleIterator iter(msg);
  EXPECT_TRUE(IPC::ParamTraits<net::HostPortPair>::Read(&msg, &iter, &output));
  EXPECT_EQ(input.host(), output.host());
  EXPECT_EQ(input.port(), output.port());
}
