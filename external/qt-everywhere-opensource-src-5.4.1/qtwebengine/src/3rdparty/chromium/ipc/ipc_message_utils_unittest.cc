// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_message_utils.h"

#include "base/files/file_path.h"
#include "ipc/ipc_message.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace IPC {
namespace {

// Tests nesting of messages as parameters to other messages.
TEST(IPCMessageUtilsTest, NestedMessages) {
  int32 nested_routing = 12;
  uint32 nested_type = 78;
  int nested_content = 456789;
  Message::PriorityValue nested_priority = Message::PRIORITY_HIGH;
  Message nested_msg(nested_routing, nested_type, nested_priority);
  nested_msg.set_sync();
  ParamTraits<int>::Write(&nested_msg, nested_content);

  // Outer message contains the nested one as its parameter.
  int32 outer_routing = 91;
  uint32 outer_type = 88;
  Message::PriorityValue outer_priority = Message::PRIORITY_NORMAL;
  Message outer_msg(outer_routing, outer_type, outer_priority);
  ParamTraits<Message>::Write(&outer_msg, nested_msg);

  // Read back the nested message.
  PickleIterator iter(outer_msg);
  IPC::Message result_msg;
  ASSERT_TRUE(ParamTraits<Message>::Read(&outer_msg, &iter, &result_msg));

  // Verify nested message headers.
  EXPECT_EQ(nested_msg.routing_id(), result_msg.routing_id());
  EXPECT_EQ(nested_msg.type(), result_msg.type());
  EXPECT_EQ(nested_msg.priority(), result_msg.priority());
  EXPECT_EQ(nested_msg.flags(), result_msg.flags());

  // Verify nested message content
  PickleIterator nested_iter(nested_msg);
  int result_content = 0;
  ASSERT_TRUE(ParamTraits<int>::Read(&nested_msg, &nested_iter,
                                     &result_content));
  EXPECT_EQ(nested_content, result_content);

  // Try reading past the ends for both messages and make sure it fails.
  IPC::Message dummy;
  ASSERT_FALSE(ParamTraits<Message>::Read(&outer_msg, &iter, &dummy));
  ASSERT_FALSE(ParamTraits<int>::Read(&nested_msg, &nested_iter,
                                      &result_content));
}

// Tests that detection of various bad parameters is working correctly.
TEST(IPCMessageUtilsTest, ParameterValidation) {
  base::FilePath::StringType ok_string(FILE_PATH_LITERAL("hello"), 5);
  base::FilePath::StringType bad_string(FILE_PATH_LITERAL("hel\0o"), 5);

  // Change this if ParamTraits<FilePath>::Write() changes.
  IPC::Message message;
  ParamTraits<base::FilePath::StringType>::Write(&message, ok_string);
  ParamTraits<base::FilePath::StringType>::Write(&message, bad_string);

  PickleIterator iter(message);
  base::FilePath ok_path;
  base::FilePath bad_path;
  ASSERT_TRUE(ParamTraits<base::FilePath>::Read(&message, &iter, &ok_path));
  ASSERT_FALSE(ParamTraits<base::FilePath>::Read(&message, &iter, &bad_path));
}

}  // namespace
}  // namespace IPC
