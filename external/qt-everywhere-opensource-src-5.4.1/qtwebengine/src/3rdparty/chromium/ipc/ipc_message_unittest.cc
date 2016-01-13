// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_message.h"

#include <string.h>

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "ipc/ipc_message_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

// IPC messages for testing ----------------------------------------------------

#define IPC_MESSAGE_IMPL
#include "ipc/ipc_message_macros.h"

#define IPC_MESSAGE_START TestMsgStart

IPC_MESSAGE_CONTROL0(TestMsgClassEmpty)

IPC_MESSAGE_CONTROL1(TestMsgClassI, int)

IPC_SYNC_MESSAGE_CONTROL1_1(TestMsgClassIS, int, std::string)

namespace {

TEST(IPCMessageTest, ListValue) {
  base::ListValue input;
  input.Set(0, new base::FundamentalValue(42.42));
  input.Set(1, new base::StringValue("forty"));
  input.Set(2, base::Value::CreateNullValue());

  IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
  IPC::WriteParam(&msg, input);

  base::ListValue output;
  PickleIterator iter(msg);
  EXPECT_TRUE(IPC::ReadParam(&msg, &iter, &output));

  EXPECT_TRUE(input.Equals(&output));

  // Also test the corrupt case.
  IPC::Message bad_msg(1, 2, IPC::Message::PRIORITY_NORMAL);
  bad_msg.WriteInt(99);
  iter = PickleIterator(bad_msg);
  EXPECT_FALSE(IPC::ReadParam(&bad_msg, &iter, &output));
}

TEST(IPCMessageTest, DictionaryValue) {
  base::DictionaryValue input;
  input.Set("null", base::Value::CreateNullValue());
  input.Set("bool", new base::FundamentalValue(true));
  input.Set("int", new base::FundamentalValue(42));
  input.SetWithoutPathExpansion("int.with.dot", new base::FundamentalValue(43));

  scoped_ptr<base::DictionaryValue> subdict(new base::DictionaryValue());
  subdict->Set("str", new base::StringValue("forty two"));
  subdict->Set("bool", new base::FundamentalValue(false));

  scoped_ptr<base::ListValue> sublist(new base::ListValue());
  sublist->Set(0, new base::FundamentalValue(42.42));
  sublist->Set(1, new base::StringValue("forty"));
  sublist->Set(2, new base::StringValue("two"));
  subdict->Set("list", sublist.release());

  input.Set("dict", subdict.release());

  IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
  IPC::WriteParam(&msg, input);

  base::DictionaryValue output;
  PickleIterator iter(msg);
  EXPECT_TRUE(IPC::ReadParam(&msg, &iter, &output));

  EXPECT_TRUE(input.Equals(&output));

  // Also test the corrupt case.
  IPC::Message bad_msg(1, 2, IPC::Message::PRIORITY_NORMAL);
  bad_msg.WriteInt(99);
  iter = PickleIterator(bad_msg);
  EXPECT_FALSE(IPC::ReadParam(&bad_msg, &iter, &output));
}

class IPCMessageParameterTest : public testing::Test {
 public:
  IPCMessageParameterTest() : extra_param_("extra_param"), called_(false) {}

  bool OnMessageReceived(const IPC::Message& message) {
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(IPCMessageParameterTest, message,
                                     &extra_param_)
      IPC_MESSAGE_HANDLER(TestMsgClassEmpty, OnEmpty)
      IPC_MESSAGE_HANDLER(TestMsgClassI, OnInt)
      //IPC_MESSAGE_HANDLER(TestMsgClassIS, OnSync)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()

    return handled;
  }

  void OnEmpty(std::string* extra_param) {
    EXPECT_EQ(extra_param, &extra_param_);
    called_ = true;
  }

  void OnInt(std::string* extra_param, int foo) {
    EXPECT_EQ(extra_param, &extra_param_);
    EXPECT_EQ(foo, 42);
    called_ = true;
  }

  /* TODO: handle sync IPCs
    void OnSync(std::string* extra_param, int foo, std::string* out) {
    EXPECT_EQ(extra_param, &extra_param_);
    EXPECT_EQ(foo, 42);
    called_ = true;
    *out = std::string("out");
  }

  bool Send(IPC::Message* reply) {
    delete reply;
    return true;
  }*/

  std::string extra_param_;
  bool called_;
};

TEST_F(IPCMessageParameterTest, EmptyDispatcherWithParam) {
  TestMsgClassEmpty message;
  EXPECT_TRUE(OnMessageReceived(message));
  EXPECT_TRUE(called_);
}

TEST_F(IPCMessageParameterTest, OneIntegerWithParam) {
  TestMsgClassI message(42);
  EXPECT_TRUE(OnMessageReceived(message));
  EXPECT_TRUE(called_);
}

/* TODO: handle sync IPCs
TEST_F(IPCMessageParameterTest, Sync) {
  std::string output;
  TestMsgClassIS message(42, &output);
  EXPECT_TRUE(OnMessageReceived(message));
  EXPECT_TRUE(called_);
  EXPECT_EQ(output, std::string("out"));
}*/

}  // namespace
