// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/locking_resource_releaser.h"
#include "ppapi/proxy/plugin_message_filter.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppapi_proxy_test.h"
#include "ppapi/proxy/talk_resource.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace proxy {

namespace {

template <class ResultType>
class MockCallbackBase {
 public:
  MockCallbackBase() : called_(false) {
  }

  bool called() {
    return called_;
  }

  ResultType result() {
    return result_;
  }

  void Reset() {
    called_ = false;
  }

  static void Callback(void* user_data, ResultType result) {
    MockCallbackBase* that = reinterpret_cast<MockCallbackBase*>(user_data);
    that->called_ = true;
    that->result_ = result;
  }

 private:
  bool called_;
  ResultType result_;
};

typedef MockCallbackBase<int32_t> MockCompletionCallback;
typedef MockCallbackBase<PP_TalkEvent> TalkEventCallback;

class TalkResourceTest : public PluginProxyTest {
 public:
  void SendReply(
      uint32_t id,
      const IPC::Message& reply,
      int32_t result) {
    IPC::Message msg;
    ResourceMessageCallParams params;
    ASSERT_TRUE(sink().GetFirstResourceCallMatching(id, &params, &msg));

    ResourceMessageReplyParams reply_params(params.pp_resource(),
                                            params.sequence());
    reply_params.set_result(result);
    PluginMessageFilter::DispatchResourceReplyForTest(reply_params, reply);
  }
};


}  // namespace

TEST_F(TalkResourceTest, GetPermission) {
  const PPB_Talk_Private_1_0* talk = thunk::GetPPB_Talk_Private_1_0_Thunk();
  LockingResourceReleaser res(talk->Create(pp_instance()));
  MockCompletionCallback callback;

  int32_t result = talk->GetPermission(
      res.get(),
      PP_MakeCompletionCallback(&MockCompletionCallback::Callback, &callback));
  ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);

  ResourceMessageCallParams params;
  IPC::Message msg;
  ASSERT_TRUE(sink().GetFirstResourceCallMatching(
      PpapiHostMsg_Talk_RequestPermission::ID, &params, &msg));

  ResourceMessageReplyParams reply_params(params.pp_resource(),
                                          params.sequence());
  reply_params.set_result(1);
  PluginMessageFilter::DispatchResourceReplyForTest(
      reply_params, PpapiPluginMsg_Talk_RequestPermissionReply());

  ASSERT_TRUE(callback.called());
  ASSERT_EQ(1, callback.result());
}

TEST_F(TalkResourceTest, RequestPermission) {
  const PPB_Talk_Private_2_0* talk = thunk::GetPPB_Talk_Private_2_0_Thunk();
  LockingResourceReleaser res(talk->Create(pp_instance()));
  MockCompletionCallback callback;

  int32_t result = talk->RequestPermission(
      res.get(),
      PP_TALKPERMISSION_REMOTING_CONTINUE,
      PP_MakeCompletionCallback(&MockCompletionCallback::Callback, &callback));
  ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);

  ResourceMessageCallParams params;
  IPC::Message msg;
  ASSERT_TRUE(sink().GetFirstResourceCallMatching(
      PpapiHostMsg_Talk_RequestPermission::ID, &params, &msg));

  ResourceMessageReplyParams reply_params(params.pp_resource(),
                                          params.sequence());
  reply_params.set_result(1);
  PluginMessageFilter::DispatchResourceReplyForTest(
      reply_params, PpapiPluginMsg_Talk_RequestPermissionReply());

  ASSERT_TRUE(callback.called());
  ASSERT_EQ(1, callback.result());
}

TEST_F(TalkResourceTest, StartStopRemoting) {
  const PPB_Talk_Private_2_0* talk = thunk::GetPPB_Talk_Private_2_0_Thunk();
  LockingResourceReleaser res(talk->Create(pp_instance()));
  MockCompletionCallback callback;
  TalkEventCallback event_callback;

  // Start
  int32_t result = talk->StartRemoting(
      res.get(),
      &TalkEventCallback::Callback,
      &event_callback,
      PP_MakeCompletionCallback(&MockCompletionCallback::Callback, &callback));
  ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);

  SendReply(PpapiHostMsg_Talk_StartRemoting::ID,
            PpapiPluginMsg_Talk_StartRemotingReply(),
            PP_OK);

  ASSERT_TRUE(callback.called());
  ASSERT_EQ(PP_OK, callback.result());

  // Receive an event
  ASSERT_FALSE(event_callback.called());
  ResourceMessageReplyParams notify_params(res.get(), 0);
  PluginMessageFilter::DispatchResourceReplyForTest(
      notify_params, PpapiPluginMsg_Talk_NotifyEvent(PP_TALKEVENT_ERROR));
  ASSERT_TRUE(event_callback.called());
  ASSERT_EQ(PP_TALKEVENT_ERROR, event_callback.result());

  // Stop
  callback.Reset();
  result = talk->StopRemoting(
      res.get(),
      PP_MakeCompletionCallback(&MockCompletionCallback::Callback, &callback));
  ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);

  SendReply(PpapiHostMsg_Talk_StopRemoting::ID,
           PpapiPluginMsg_Talk_StopRemotingReply(),
           PP_OK);

  ASSERT_TRUE(callback.called());
  ASSERT_EQ(PP_OK, callback.result());

  // Events should be discarded at this point
  event_callback.Reset();
  PluginMessageFilter::DispatchResourceReplyForTest(
      notify_params, PpapiPluginMsg_Talk_NotifyEvent(PP_TALKEVENT_ERROR));
  ASSERT_FALSE(event_callback.called());
}

}  // namespace proxy
}  // namespace ppapi
