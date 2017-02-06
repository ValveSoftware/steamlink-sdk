// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/timer/mock_timer.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/browser.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/cast_channel/cast_channel_api.h"
#include "extensions/browser/api/cast_channel/cast_socket.h"
#include "extensions/browser/api/cast_channel/cast_test_util.h"
#include "extensions/browser/api/cast_channel/logger.h"
#include "extensions/common/api/cast_channel.h"
#include "extensions/common/api/cast_channel/cast_channel.pb.h"
#include "extensions/common/switches.h"
#include "extensions/common/test_util.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "net/base/completion_callback.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/log/test_net_log.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gmock_mutant.h"

// TODO(mfoltz): Mock out the ApiResourceManager to resolve threading issues
// (crbug.com/398242) and simulate unloading of the extension.

namespace cast_channel = extensions::api::cast_channel;
using cast_channel::CastMessage;
using cast_channel::CastSocket;
using cast_channel::CastTransport;
using cast_channel::ChannelAuthType;
using cast_channel::ChannelError;
using cast_channel::CreateIPEndPointForTest;
using cast_channel::ErrorInfo;
using cast_channel::LastErrors;
using cast_channel::Logger;
using cast_channel::MessageInfo;
using cast_channel::MockCastSocket;
using cast_channel::MockCastTransport;
using cast_channel::ReadyState;
using extensions::Extension;

namespace utils = extension_function_test_utils;

using ::testing::_;
using ::testing::A;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::InSequence;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::ReturnPointee;
using ::testing::SaveArg;

namespace {

static void FillCastMessage(const std::string& message,
                            CastMessage* cast_message) {
  cast_message->set_namespace_("foo");
  cast_message->set_source_id("src");
  cast_message->set_destination_id("dest");
  cast_message->set_payload_utf8(message);
  cast_message->set_payload_type(CastMessage::STRING);
}

ACTION_TEMPLATE(InvokeCompletionCallback,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(result)) {
  ::std::tr1::get<k>(args).Run(result);
}

}  // namespace

class CastChannelAPITest : public ExtensionApiTest {
 public:
  CastChannelAPITest() : ip_endpoint_(CreateIPEndPointForTest()) {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        extensions::switches::kWhitelistedExtensionID,
        cast_channel::kTestExtensionId);
  }

  void SetUpMockCastSocket() {
    extensions::CastChannelAPI* api = GetApi();
    timeout_timer_ = new base::MockTimer(true, false);
    api->SetPingTimeoutTimerForTest(base::WrapUnique(timeout_timer_));

    net::IPEndPoint ip_endpoint(net::IPAddress(192, 168, 1, 1), 8009);
    mock_cast_socket_ = new MockCastSocket;
    // Transfers ownership of the socket.
    api->SetSocketForTest(base::WrapUnique<CastSocket>(mock_cast_socket_));
    ON_CALL(*mock_cast_socket_, set_id(_))
        .WillByDefault(SaveArg<0>(&channel_id_));
    ON_CALL(*mock_cast_socket_, id())
        .WillByDefault(ReturnPointee(&channel_id_));
    ON_CALL(*mock_cast_socket_, ip_endpoint())
        .WillByDefault(ReturnRef(ip_endpoint_));
    ON_CALL(*mock_cast_socket_, channel_auth())
        .WillByDefault(Return(cast_channel::CHANNEL_AUTH_TYPE_SSL));
    ON_CALL(*mock_cast_socket_, keep_alive()).WillByDefault(Return(false));
  }

  void SetUpOpenSendClose() {
    SetUpMockCastSocket();
    EXPECT_CALL(*mock_cast_socket_, error_state())
        .WillRepeatedly(Return(cast_channel::CHANNEL_ERROR_NONE));
    {
      InSequence sequence;
      EXPECT_CALL(*mock_cast_socket_, ConnectRawPtr(_, _))
          .WillOnce(
              InvokeCompletionCallback<1>(cast_channel::CHANNEL_ERROR_NONE));
      EXPECT_CALL(*mock_cast_socket_, ready_state())
          .WillOnce(Return(cast_channel::READY_STATE_OPEN));
      EXPECT_CALL(*mock_cast_socket_->mock_transport(),
                  SendMessage(A<const CastMessage&>(), _))
          .WillOnce(InvokeCompletionCallback<1>(net::OK));
      EXPECT_CALL(*mock_cast_socket_, ready_state())
          .WillOnce(Return(cast_channel::READY_STATE_OPEN));
      EXPECT_CALL(*mock_cast_socket_, Close(_))
          .WillOnce(InvokeCompletionCallback<0>(net::OK));
      EXPECT_CALL(*mock_cast_socket_, ready_state())
          .WillOnce(Return(cast_channel::READY_STATE_CLOSED));
    }
  }

  void SetUpOpenPingTimeout() {
    SetUpMockCastSocket();
    EXPECT_CALL(*mock_cast_socket_, error_state())
        .WillRepeatedly(Return(cast_channel::CHANNEL_ERROR_NONE));
    EXPECT_CALL(*mock_cast_socket_, keep_alive()).WillRepeatedly(Return(true));
    {
      InSequence sequence;
      EXPECT_CALL(*mock_cast_socket_, ConnectRawPtr(_, _))
          .WillOnce(DoAll(
              SaveArg<0>(&message_delegate_),
              InvokeCompletionCallback<1>(cast_channel::CHANNEL_ERROR_NONE)));
      EXPECT_CALL(*mock_cast_socket_, ready_state())
          .WillOnce(Return(cast_channel::READY_STATE_OPEN))
          .RetiresOnSaturation();
      EXPECT_CALL(*mock_cast_socket_, ready_state())
          .WillOnce(Return(cast_channel::READY_STATE_CLOSED));
    }
  }

  extensions::CastChannelAPI* GetApi() {
    return extensions::CastChannelAPI::Get(profile());
  }

  // Logs some bogus error details and calls the OnError handler.
  void DoCallOnError(extensions::CastChannelAPI* api) {
    api->GetLogger()->LogSocketEventWithRv(mock_cast_socket_->id(),
                                           cast_channel::proto::SOCKET_WRITE,
                                           net::ERR_FAILED);
    message_delegate_->OnError(cast_channel::CHANNEL_ERROR_CONNECT_ERROR);
  }

 protected:
  void CallOnMessage(const std::string& message) {
    content::BrowserThread::PostTask(
        content::BrowserThread::IO,
        FROM_HERE,
        base::Bind(&CastChannelAPITest::DoCallOnMessage, this,
                   GetApi(), mock_cast_socket_, message));
  }

  void DoCallOnMessage(extensions::CastChannelAPI* api,
                       MockCastSocket* cast_socket,
                       const std::string& message) {
    CastMessage cast_message;
    FillCastMessage(message, &cast_message);
    message_delegate_->OnMessage(cast_message);
  }

  // Starts the read delegate on the IO thread.
  void StartDelegate() {
    CHECK(message_delegate_);
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&cast_channel::CastTransport::Delegate::Start,
                   base::Unretained(message_delegate_)));
  }

  // Fires a timer on the IO thread.
  void FireTimeout() {
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&base::MockTimer::Fire, base::Unretained(timeout_timer_)));
  }

  extensions::CastChannelOpenFunction* CreateOpenFunction(
        scoped_refptr<Extension> extension) {
    extensions::CastChannelOpenFunction* cast_channel_open_function =
      new extensions::CastChannelOpenFunction;
    cast_channel_open_function->set_extension(extension.get());
    return cast_channel_open_function;
  }

  extensions::CastChannelSendFunction* CreateSendFunction(
        scoped_refptr<Extension> extension) {
    extensions::CastChannelSendFunction* cast_channel_send_function =
      new extensions::CastChannelSendFunction;
    cast_channel_send_function->set_extension(extension.get());
    return cast_channel_send_function;
  }

  extensions::CastChannelSetAuthorityKeysFunction*
  CreateSetAuthorityKeysFunction(scoped_refptr<Extension> extension) {
    extensions::CastChannelSetAuthorityKeysFunction*
        cast_channel_set_authority_keys_function =
            new extensions::CastChannelSetAuthorityKeysFunction;
    cast_channel_set_authority_keys_function->set_extension(extension.get());
    return cast_channel_set_authority_keys_function;
  }

  MockCastSocket* mock_cast_socket_;
  base::MockTimer* timeout_timer_;
  net::IPEndPoint ip_endpoint_;
  LastErrors last_errors_;
  CastTransport::Delegate* message_delegate_;
  net::TestNetLog capturing_net_log_;
  int channel_id_;
};

ACTION_P2(InvokeDelegateOnError, api_test, api) {
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&CastChannelAPITest::DoCallOnError, base::Unretained(api_test),
                 base::Unretained(api)));
}

// TODO(kmarshall): Win Dbg has a workaround that makes RunExtensionSubtest
// always return true without actually running the test. Remove when fixed.
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_TestOpenSendClose DISABLED_TestOpenSendClose
#else
#define MAYBE_TestOpenSendClose TestOpenSendClose
#endif
// Test loading extension, opening a channel with ConnectInfo, adding a
// listener, writing, reading, and closing.
IN_PROC_BROWSER_TEST_F(CastChannelAPITest, MAYBE_TestOpenSendClose) {
  SetUpOpenSendClose();

  EXPECT_TRUE(RunExtensionSubtest("cast_channel/api",
                                  "test_open_send_close.html"));
}

// TODO(kmarshall): Win Dbg has a workaround that makes RunExtensionSubtest
// always return true without actually running the test. Remove when fixed.
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_TestPingTimeout DISABLED_TestPingTimeout
#else
#define MAYBE_TestPingTimeout TestPingTimeout
#endif
// Verify that timeout events are propagated through the API layer.
// (SSL, non-verified).
IN_PROC_BROWSER_TEST_F(CastChannelAPITest, MAYBE_TestPingTimeout) {
  SetUpOpenPingTimeout();

  ExtensionTestMessageListener channel_opened("channel_opened_ssl", false);
  ExtensionTestMessageListener timeout("timeout_ssl", false);
  EXPECT_TRUE(
      RunExtensionSubtest("cast_channel/api", "test_open_timeout.html"));
  EXPECT_TRUE(channel_opened.WaitUntilSatisfied());
  StartDelegate();
  FireTimeout();
  EXPECT_TRUE(timeout.WaitUntilSatisfied());
}

// TODO(kmarshall): Win Dbg has a workaround that makes RunExtensionSubtest
// always return true without actually running the test. Remove when fixed.
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_TestPingTimeoutSslVerified DISABLED_TestPingTimeoutSslVerified
#else
#define MAYBE_TestPingTimeoutSslVerified TestPingTimeoutSslVerified
#endif
// Verify that timeout events are propagated through the API layer.
// (SSL, verified).
IN_PROC_BROWSER_TEST_F(CastChannelAPITest, MAYBE_TestPingTimeoutSslVerified) {
  SetUpOpenPingTimeout();

  ExtensionTestMessageListener channel_opened("channel_opened_ssl_verified",
                                              false);
  ExtensionTestMessageListener timeout("timeout_ssl_verified", false);
  EXPECT_TRUE(RunExtensionSubtest("cast_channel/api",
                                  "test_open_timeout_verified.html"));
  EXPECT_TRUE(channel_opened.WaitUntilSatisfied());
  StartDelegate();
  FireTimeout();
  EXPECT_TRUE(timeout.WaitUntilSatisfied());
}

// TODO(kmarshall): Win Dbg has a workaround that makes RunExtensionSubtest
// always return true without actually running the test. Remove when fixed.
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_TestOpenReceiveClose DISABLED_TestOpenReceiveClose
#else
#define MAYBE_TestOpenReceiveClose TestOpenReceiveClose
#endif
// Test loading extension, opening a channel, adding a listener,
// writing, reading, and closing.
IN_PROC_BROWSER_TEST_F(CastChannelAPITest, MAYBE_TestOpenReceiveClose) {
  SetUpMockCastSocket();
  EXPECT_CALL(*mock_cast_socket_, error_state())
      .WillRepeatedly(Return(cast_channel::CHANNEL_ERROR_NONE));

  {
    InSequence sequence;
    EXPECT_CALL(*mock_cast_socket_, ConnectRawPtr(NotNull(), _))
        .WillOnce(DoAll(
            SaveArg<0>(&message_delegate_),
            InvokeCompletionCallback<1>(cast_channel::CHANNEL_ERROR_NONE)));
    EXPECT_CALL(*mock_cast_socket_, ready_state())
        .Times(3)
        .WillRepeatedly(Return(cast_channel::READY_STATE_OPEN));
    EXPECT_CALL(*mock_cast_socket_, Close(_))
        .WillOnce(InvokeCompletionCallback<0>(net::OK));
    EXPECT_CALL(*mock_cast_socket_, ready_state())
        .WillOnce(Return(cast_channel::READY_STATE_CLOSED));
  }

  EXPECT_TRUE(RunExtensionSubtest("cast_channel/api",
                                  "test_open_receive_close.html"));

  extensions::ResultCatcher catcher;
  CallOnMessage("some-message");
  CallOnMessage("some-message");
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// TODO(imcheng): Win Dbg has a workaround that makes RunExtensionSubtest
// always return true without actually running the test. Remove when fixed.
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_TestGetLogs DISABLED_TestGetLogs
#else
#define MAYBE_TestGetLogs TestGetLogs
#endif
// Test loading extension, execute a open-send-close sequence, then get logs.
IN_PROC_BROWSER_TEST_F(CastChannelAPITest, MAYBE_TestGetLogs) {
  SetUpOpenSendClose();

  EXPECT_TRUE(RunExtensionSubtest("cast_channel/api", "test_get_logs.html"));
}

// TODO(kmarshall): Win Dbg has a workaround that makes RunExtensionSubtest
// always return true without actually running the test. Remove when fixed.
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_TestOpenError DISABLED_TestOpenError
#else
#define MAYBE_TestOpenError TestOpenError
#endif
// Test the case when socket open results in an error.
IN_PROC_BROWSER_TEST_F(CastChannelAPITest, MAYBE_TestOpenError) {
  SetUpMockCastSocket();

  EXPECT_CALL(*mock_cast_socket_, ConnectRawPtr(NotNull(), _))
      .WillOnce(DoAll(SaveArg<0>(&message_delegate_),
                      InvokeDelegateOnError(this, GetApi()),
                      InvokeCompletionCallback<1>(
                          cast_channel::CHANNEL_ERROR_CONNECT_ERROR)));
  EXPECT_CALL(*mock_cast_socket_, error_state())
      .WillRepeatedly(Return(cast_channel::CHANNEL_ERROR_CONNECT_ERROR));
  EXPECT_CALL(*mock_cast_socket_, ready_state())
      .WillRepeatedly(Return(cast_channel::READY_STATE_CLOSED));
  EXPECT_CALL(*mock_cast_socket_, Close(_))
      .WillOnce(InvokeCompletionCallback<0>(net::OK));

  EXPECT_TRUE(RunExtensionSubtest("cast_channel/api",
                                  "test_open_error.html"));
}

IN_PROC_BROWSER_TEST_F(CastChannelAPITest, TestOpenInvalidConnectInfo) {
  scoped_refptr<Extension> empty_extension =
      extensions::test_util::CreateEmptyExtension();
  scoped_refptr<extensions::CastChannelOpenFunction> cast_channel_open_function;

  // Invalid IP address
  cast_channel_open_function = CreateOpenFunction(empty_extension);
  std::string error = utils::RunFunctionAndReturnError(
      cast_channel_open_function.get(),
      "[{\"ipAddress\": \"invalid_ip\", \"port\": 8009, \"auth\": \"ssl\"}]",
      browser());
  EXPECT_EQ(error, "Invalid connect_info (invalid IP address)");

  // Invalid port
  cast_channel_open_function = CreateOpenFunction(empty_extension);
  error = utils::RunFunctionAndReturnError(
      cast_channel_open_function.get(),
      "[{\"ipAddress\": \"127.0.0.1\", \"port\": -200, \"auth\": \"ssl\"}]",
      browser());
  EXPECT_EQ(error, "Invalid connect_info (invalid port)");
}

IN_PROC_BROWSER_TEST_F(CastChannelAPITest, TestSendInvalidMessageInfo) {
  scoped_refptr<Extension> empty_extension(
      extensions::test_util::CreateEmptyExtension());
  scoped_refptr<extensions::CastChannelSendFunction> cast_channel_send_function;

  // Numbers are not supported
  cast_channel_send_function = CreateSendFunction(empty_extension);
  std::string error(utils::RunFunctionAndReturnError(
      cast_channel_send_function.get(),
      "[{\"channelId\": 1, "
      "\"keepAlive\": true, "
      "\"audioOnly\": false, "
      "\"connectInfo\": "
      "{\"ipAddress\": \"127.0.0.1\", \"port\": 8009, "
      "\"auth\": \"ssl\"}, \"readyState\": \"open\"}, "
      "{\"namespace_\": \"foo\", \"sourceId\": \"src\", "
      "\"destinationId\": \"dest\", \"data\": 1235}]",
      browser()));
  EXPECT_EQ(error, "Invalid type of message_info.data");

  // Missing namespace_
  cast_channel_send_function = CreateSendFunction(empty_extension);
  error = utils::RunFunctionAndReturnError(
      cast_channel_send_function.get(),
      "[{\"channelId\": 1, "
      "\"keepAlive\": true, "
      "\"audioOnly\": false, "
      "\"connectInfo\": "
      "{\"ipAddress\": \"127.0.0.1\", \"port\": 8009, "
      "\"auth\": \"ssl\"}, \"readyState\": \"open\"}, "
      "{\"namespace_\": \"\", \"sourceId\": \"src\", "
      "\"destinationId\": \"dest\", \"data\": \"data\"}]",
      browser());
  EXPECT_EQ(error, "message_info.namespace_ is required");

  // Missing source_id
  cast_channel_send_function = CreateSendFunction(empty_extension);
  error = utils::RunFunctionAndReturnError(
      cast_channel_send_function.get(),
      "[{\"channelId\": 1, "
      "\"keepAlive\": true, "
      "\"audioOnly\": false, "
      "\"connectInfo\": "
      "{\"ipAddress\": \"127.0.0.1\", \"port\": 8009, "
      "\"auth\": \"ssl\"}, \"readyState\": \"open\"}, "
      "{\"namespace_\": \"foo\", \"sourceId\": \"\", "
      "\"destinationId\": \"dest\", \"data\": \"data\"}]",
      browser());
  EXPECT_EQ(error, "message_info.source_id is required");

  // Missing destination_id
  cast_channel_send_function = CreateSendFunction(empty_extension);
  error = utils::RunFunctionAndReturnError(
      cast_channel_send_function.get(),
      "[{\"channelId\": 1, "
      "\"keepAlive\": true, "
      "\"audioOnly\": false, "
      "\"connectInfo\": "
      "{\"ipAddress\": \"127.0.0.1\", \"port\": 8009, "
      "\"auth\": \"ssl\"}, \"readyState\": \"open\"}, "
      "{\"namespace_\": \"foo\", \"sourceId\": \"src\", "
      "\"destinationId\": \"\", \"data\": \"data\"}]",
      browser());
  EXPECT_EQ(error, "message_info.destination_id is required");
}

IN_PROC_BROWSER_TEST_F(CastChannelAPITest, TestSetAuthorityKeysInvalid) {
  scoped_refptr<Extension> empty_extension(
      extensions::test_util::CreateEmptyExtension());
  scoped_refptr<extensions::CastChannelSetAuthorityKeysFunction>
      cast_channel_set_authority_keys_function;
  // TODO(eroman): crbug.com/601171: Delete this test once the API has
  // been removed. The API is deprecated and will trivially return
  // success. So this is just testing that it succeeds for all inputs
  // (even invalid ones).
  std::string errorResult = "";

  cast_channel_set_authority_keys_function =
      CreateSetAuthorityKeysFunction(empty_extension);
  std::string error = utils::RunFunctionAndReturnError(
      cast_channel_set_authority_keys_function.get(),
      "[\"\", \"signature\"]",
      browser());
  EXPECT_EQ(error, errorResult);

  cast_channel_set_authority_keys_function =
      CreateSetAuthorityKeysFunction(empty_extension);
  error = utils::RunFunctionAndReturnError(
      cast_channel_set_authority_keys_function.get(),
      "[\"keys\", \"\"]",
      browser());
  EXPECT_EQ(error, errorResult);

  std::string keys =
      "CrMCCiBSnZzWf+XraY5w3SbX2PEmWfHm5SNIv2pc9xbhP0EOcxKOAjCCAQoCggEBALwigL"
      "2A9johADuudl41fz3DZFxVlIY0LwWHKM33aYwXs1CnuIL638dDLdZ+q6BvtxNygKRHFcEg"
      "mVDN7BRiCVukmM3SQbY2Tv/oLjIwSoGoQqNsmzNuyrL1U2bgJ1OGGoUepzk/SneO+1RmZv"
      "tYVMBeOcf1UAYL4IrUzuFqVR+LFwDmaaMn5gglaTwSnY0FLNYuojHetFJQ1iBJ3nGg+a0g"
      "QBLx3SXr1ea4NvTWj3/KQ9zXEFvmP1GKhbPz//YDLcsjT5ytGOeTBYysUpr3TOmZer5ufk"
      "0K48YcqZP6OqWRXRy9ZuvMYNyGdMrP+JIcmH1X+mFHnquAt+RIgCqSxRsCAwEAAQ==";
  std::string signature =
      "chCUHZKkykcwU8HzU+hm027fUTBL0dqPMtrzppwExQwK9+"
      "XlmCjJswfce2sUUfhR1OL1tyW4hWFwu4JnuQCJ+CvmSmAh2bzRpnuSKzBfgvIDjNOAGUs7"
      "ADaNSSWPLxp+6ko++2Dn4S9HpOt8N1v6gMWqj3Ru5IqFSQPZSvGH2ois6uE50CFayPcjQE"
      "OVZt41noQdFd15RmKTvocoCC5tHNlaikeQ52yi0IScOlad1B1lMhoplW3rWophQaqxMumr"
      "OcHIZ+Y+p858x5f8Pny/kuqUClmFh9B/vF07NsUHwoSL9tA5t5jCY3L5iUc/v7o3oFcW/T"
      "gojKkX2Kg7KQ86QA==";

  cast_channel_set_authority_keys_function =
      CreateSetAuthorityKeysFunction(empty_extension);
  error = utils::RunFunctionAndReturnError(
      cast_channel_set_authority_keys_function.get(),
      "[\"" + keys + "\", \"signature\"]",
      browser());
  EXPECT_EQ(error, errorResult);

  cast_channel_set_authority_keys_function =
      CreateSetAuthorityKeysFunction(empty_extension);
  error = utils::RunFunctionAndReturnError(
      cast_channel_set_authority_keys_function.get(),
      "[\"keys\", \"" + signature + "\"]",
      browser());
  EXPECT_EQ(error, errorResult);

  cast_channel_set_authority_keys_function =
      CreateSetAuthorityKeysFunction(empty_extension);
  error = utils::RunFunctionAndReturnError(
      cast_channel_set_authority_keys_function.get(),
      "[\"" + keys + "\", \"" + signature + "\"]",
      browser());
  EXPECT_EQ(error, errorResult);
}

IN_PROC_BROWSER_TEST_F(CastChannelAPITest, TestSetAuthorityKeysValid) {
  scoped_refptr<Extension> empty_extension(
      extensions::test_util::CreateEmptyExtension());
  scoped_refptr<extensions::CastChannelSetAuthorityKeysFunction>
      cast_channel_set_authority_keys_function;

  cast_channel_set_authority_keys_function =
      CreateSetAuthorityKeysFunction(empty_extension);
  std::string keys =
      "CrMCCiBSnZzWf+XraY5w3SbX2PEmWfHm5SNIv2pc9xbhP0EOcxKOAjCCAQoCggEBALwigL"
      "2A9johADuudl41fz3DZFxVlIY0LwWHKM33aYwXs1CnuIL638dDLdZ+q6BvtxNygKRHFcEg"
      "mVDN7BRiCVukmM3SQbY2Tv/oLjIwSoGoQqNsmzNuyrL1U2bgJ1OGGoUepzk/SneO+1RmZv"
      "tYVMBeOcf1UAYL4IrUzuFqVR+LFwDmaaMn5gglaTwSnY0FLNYuojHetFJQ1iBJ3nGg+a0g"
      "QBLx3SXr1ea4NvTWj3/KQ9zXEFvmP1GKhbPz//YDLcsjT5ytGOeTBYysUpr3TOmZer5ufk"
      "0K48YcqZP6OqWRXRy9ZuvMYNyGdMrP+JIcmH1X+mFHnquAt+RIgCqSxRsCAwEAAQqzAgog"
      "okjC6FTmVqVt6CMfHuF1b9vkB/n+1GUNYMxay2URxyASjgIwggEKAoIBAQCwDl4HOt+kX2"
      "j3Icdk27Z27+6Lk/j2G4jhk7cX8BUeflJVdzwCjXtKbNO91sGccsizFc8RwfVGxNUgR/sw"
      "9ORhDGjwXqs3jpvhvIHDcIp41oM0MpwZYuvknO3jZGxBHZzSi0hMI5CVs+dS6gVXzGCzuh"
      "TkugA55EZVdM5ajnpnI9poCvrEhB60xaGianMfbsguL5qeqLEO/Yemj009SwXVNVp0TbyO"
      "gkSW9LWVYE6l3yc9QVwHo7Q1WrOe8gUkys0xWg0mTNTT/VDhNOlMgVgwssd63YGJptQ6OI"
      "QDtzSedz//eAdbmcGyHzVWbjo8DCXhV/aKfknAzIMRNeeRbS5lAgMBAAE=";
  std::string signature =
      "o83oku3jP+xjTysNBalqp/ZfJRPLt8R+IUhZMepbARFSRVizLoeFW5XyUwe6lQaC+PFFQH"
      "SZeGZyeeGRpwCJ/lef0xh6SWJlVMWNTk5+z0U84GQdizJP/CTCeHpIwMobN+kyDajgOyfD"
      "DLhktc6LHmSlFGG6J7B8W67oziS8ZFEdrcT9WSXFrjLVyURHjvidZD5iFtuImI6k9R9OoX"
      "LR6SyAwpjdrL+vlHMk3Gol6KQ98YpF0ghHnN3/FFW4ibvIwjmRbp+tUV3h8TRcCOjlXVGp"
      "bzPtNRRlTqfv7Rxm5YXkZMLmJJMZiTs5+o8FMRMTQZT4hRR3DQ+A/jofViyTGA==";

  std::string args = "[\"" + keys + "\", \"" + signature + "\"]";
  std::string error = utils::RunFunctionAndReturnError(
      cast_channel_set_authority_keys_function.get(), args, browser());
  EXPECT_EQ(error, std::string());
}

// TODO(vadimgo): Win Dbg has a workaround that makes RunExtensionSubtest
// always return true without actually running the test. Remove when fixed.
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_TestSetAuthorityKeys DISABLED_TestSetAuthorityKeys
#else
#define MAYBE_TestSetAuthorityKeys TestSetAuthorityKeys
#endif
// Test loading extension, opening a channel with ConnectInfo, adding a
// listener, writing, reading, and closing.
IN_PROC_BROWSER_TEST_F(CastChannelAPITest, MAYBE_TestSetAuthorityKeys) {
  EXPECT_TRUE(
      RunExtensionSubtest("cast_channel/api", "test_authority_keys.html"));
}
