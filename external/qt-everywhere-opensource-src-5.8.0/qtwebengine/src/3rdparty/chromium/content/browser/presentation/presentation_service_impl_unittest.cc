// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/presentation/presentation_service_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/browser/presentation_service_delegate.h"
#include "content/public/browser/presentation_session.h"
#include "content/public/common/presentation_constants.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/bindings/string.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::InvokeWithoutArgs;
using ::testing::Mock;
using ::testing::Return;
using ::testing::SaveArg;

namespace content {

namespace {

// Matches mojo structs.
MATCHER_P(Equals, expected, "") {
  return expected.Equals(arg);
}

const char *const kPresentationId = "presentationId";
const char *const kPresentationUrl = "http://foo.com/index.html";

bool ArePresentationSessionMessagesEqual(
    const blink::mojom::SessionMessage* expected,
    const blink::mojom::SessionMessage* actual) {
  return expected->type == actual->type &&
         expected->message == actual->message &&
         expected->data.Equals(actual->data);
}

void DoNothing(blink::mojom::PresentationSessionInfoPtr info,
               blink::mojom::PresentationErrorPtr error) {}

}  // namespace

class MockPresentationServiceDelegate : public PresentationServiceDelegate {
 public:
  MOCK_METHOD3(AddObserver,
      void(int render_process_id,
           int render_frame_id,
           PresentationServiceDelegate::Observer* observer));
  MOCK_METHOD2(RemoveObserver,
      void(int render_process_id, int render_frame_id));

  bool AddScreenAvailabilityListener(
      int render_process_id,
      int routing_id,
      PresentationScreenAvailabilityListener* listener) override {
    if (!screen_availability_listening_supported_)
      listener->OnScreenAvailabilityNotSupported();

    return AddScreenAvailabilityListener();
  }
  MOCK_METHOD0(AddScreenAvailabilityListener, bool());

  MOCK_METHOD3(RemoveScreenAvailabilityListener,
      void(
          int render_process_id,
          int routing_id,
          PresentationScreenAvailabilityListener* listener));
  MOCK_METHOD2(Reset,
      void(
          int render_process_id,
          int routing_id));
  MOCK_METHOD4(SetDefaultPresentationUrl,
               void(int render_process_id,
                    int routing_id,
                    const std::string& default_presentation_url,
                    const PresentationSessionStartedCallback& callback));
  MOCK_METHOD5(StartSession,
               void(int render_process_id,
                    int render_frame_id,
                    const std::string& presentation_url,
                    const PresentationSessionStartedCallback& success_cb,
                    const PresentationSessionErrorCallback& error_cb));
  MOCK_METHOD6(JoinSession,
               void(int render_process_id,
                    int render_frame_id,
                    const std::string& presentation_url,
                    const std::string& presentation_id,
                    const PresentationSessionStartedCallback& success_cb,
                    const PresentationSessionErrorCallback& error_cb));
  MOCK_METHOD3(CloseConnection,
               void(int render_process_id,
                    int render_frame_id,
                    const std::string& presentation_id));
  MOCK_METHOD3(Terminate,
               void(int render_process_id,
                    int render_frame_id,
                    const std::string& presentation_id));
  MOCK_METHOD4(ListenForSessionMessages,
               void(int render_process_id,
                    int render_frame_id,
                    const content::PresentationSessionInfo& session,
                    const PresentationSessionMessageCallback& message_cb));
  MOCK_METHOD5(SendMessageRawPtr,
               void(int render_process_id,
                    int render_frame_id,
                    const content::PresentationSessionInfo& session,
                    PresentationSessionMessage* message_request,
                    const SendMessageCallback& send_message_cb));
  void SendMessage(int render_process_id,
                   int render_frame_id,
                   const content::PresentationSessionInfo& session,
                   std::unique_ptr<PresentationSessionMessage> message_request,
                   const SendMessageCallback& send_message_cb) override {
    SendMessageRawPtr(render_process_id, render_frame_id, session,
                      message_request.release(), send_message_cb);
  }
  MOCK_METHOD4(ListenForConnectionStateChange,
               void(int render_process_id,
                    int render_frame_id,
                    const content::PresentationSessionInfo& connection,
                    const content::PresentationConnectionStateChangedCallback&
                        state_changed_cb));

  void set_screen_availability_listening_supported(bool value) {
    screen_availability_listening_supported_ = value;
  }

 private:
  bool screen_availability_listening_supported_ = true;
};

class MockPresentationServiceClient
    : public blink::mojom::PresentationServiceClient {
 public:
  MOCK_METHOD2(OnScreenAvailabilityUpdated,
      void(const mojo::String& url, bool available));
  void OnConnectionStateChanged(
      blink::mojom::PresentationSessionInfoPtr connection,
      blink::mojom::PresentationConnectionState new_state) override {
    OnConnectionStateChanged(*connection, new_state);
  }
  MOCK_METHOD2(OnConnectionStateChanged,
               void(const blink::mojom::PresentationSessionInfo& connection,
                    blink::mojom::PresentationConnectionState new_state));

  void OnConnectionClosed(
      blink::mojom::PresentationSessionInfoPtr connection,
      blink::mojom::PresentationConnectionCloseReason reason,
      const mojo::String& message) override {
    OnConnectionClosed(*connection, reason, message);
  }
  MOCK_METHOD3(OnConnectionClosed,
               void(const blink::mojom::PresentationSessionInfo& connection,
                    blink::mojom::PresentationConnectionCloseReason reason,
                    const mojo::String& message));

  MOCK_METHOD1(OnScreenAvailabilityNotSupported, void(const mojo::String& url));

  void OnSessionMessagesReceived(
      blink::mojom::PresentationSessionInfoPtr session_info,
      mojo::Array<blink::mojom::SessionMessagePtr> messages) override {
    messages_received_ = std::move(messages);
    MessagesReceived();
  }
  MOCK_METHOD0(MessagesReceived, void());

  void OnDefaultSessionStarted(
      blink::mojom::PresentationSessionInfoPtr session_info) override {
    OnDefaultSessionStarted(*session_info);
  }
  MOCK_METHOD1(OnDefaultSessionStarted,
               void(const blink::mojom::PresentationSessionInfo& session_info));

  mojo::Array<blink::mojom::SessionMessagePtr> messages_received_;
};

class PresentationServiceImplTest : public RenderViewHostImplTestHarness {
 public:
  PresentationServiceImplTest() {}

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();

    auto request = mojo::GetProxy(&service_ptr_);
    EXPECT_CALL(mock_delegate_, AddObserver(_, _, _)).Times(1);
    TestRenderFrameHost* render_frame_host = contents()->GetMainFrame();
    render_frame_host->InitializeRenderFrameIfNeeded();
    service_impl_.reset(new PresentationServiceImpl(
        render_frame_host, contents(), &mock_delegate_));
    service_impl_->Bind(std::move(request));

    blink::mojom::PresentationServiceClientPtr client_ptr;
    client_binding_.reset(
        new mojo::Binding<blink::mojom::PresentationServiceClient>(
            &mock_client_, mojo::GetProxy(&client_ptr)));
    service_impl_->SetClient(std::move(client_ptr));
  }

  void TearDown() override {
    service_ptr_.reset();
    if (service_impl_.get()) {
      EXPECT_CALL(mock_delegate_, RemoveObserver(_, _)).Times(1);
      service_impl_.reset();
    }
    RenderViewHostImplTestHarness::TearDown();
  }

  void ListenForScreenAvailabilityAndWait(
      const mojo::String& url, bool delegate_success) {
    base::RunLoop run_loop;
    // This will call to |service_impl_| via mojo. Process the message
    // using RunLoop.
    // The callback shouldn't be invoked since there is no availability
    // result yet.
    EXPECT_CALL(mock_delegate_, AddScreenAvailabilityListener())
        .WillOnce(DoAll(
            InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit),
            Return(delegate_success)));
    service_ptr_->ListenForScreenAvailability(url);
    run_loop.Run();

    EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_delegate_));
  }

  void RunLoopFor(base::TimeDelta duration) {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), duration);
    run_loop.Run();
  }

  void SaveQuitClosureAndRunLoop() {
    base::RunLoop run_loop;
    run_loop_quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
    run_loop_quit_closure_.Reset();
  }

  void SimulateScreenAvailabilityChangeAndWait(
      const std::string& url, bool available) {
    auto listener_it = service_impl_->screen_availability_listeners_.find(url);
    ASSERT_TRUE(listener_it->second);

    base::RunLoop run_loop;
    EXPECT_CALL(mock_client_,
                OnScreenAvailabilityUpdated(mojo::String(url), available))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    listener_it->second->OnScreenAvailabilityChanged(available);
    run_loop.Run();
  }

  void ExpectReset() {
    EXPECT_CALL(mock_delegate_, Reset(_, _)).Times(1);
  }

  void ExpectCleanState() {
    EXPECT_TRUE(service_impl_->default_presentation_url_.empty());
    EXPECT_EQ(
        service_impl_->screen_availability_listeners_.find(kPresentationUrl),
        service_impl_->screen_availability_listeners_.end());
    EXPECT_FALSE(service_impl_->on_session_messages_callback_.get());
  }

  void ExpectNewSessionCallbackSuccess(
      blink::mojom::PresentationSessionInfoPtr info,
      blink::mojom::PresentationErrorPtr error) {
    EXPECT_FALSE(info.is_null());
    EXPECT_TRUE(error.is_null());
    if (!run_loop_quit_closure_.is_null())
      run_loop_quit_closure_.Run();
  }

  void ExpectNewSessionCallbackError(
      blink::mojom::PresentationSessionInfoPtr info,
      blink::mojom::PresentationErrorPtr error) {
    EXPECT_TRUE(info.is_null());
    EXPECT_FALSE(error.is_null());
    if (!run_loop_quit_closure_.is_null())
      run_loop_quit_closure_.Run();
  }

  void ExpectSessionMessages(
      const mojo::Array<blink::mojom::SessionMessagePtr>& expected_msgs,
      const mojo::Array<blink::mojom::SessionMessagePtr>& actual_msgs) {
    EXPECT_EQ(expected_msgs.size(), actual_msgs.size());
    for (size_t i = 0; i < actual_msgs.size(); ++i) {
      EXPECT_TRUE(ArePresentationSessionMessagesEqual(expected_msgs[i].get(),
                                                      actual_msgs[i].get()));
    }
  }

  void ExpectSendSessionMessageCallback(bool success) {
    EXPECT_TRUE(success);
    EXPECT_FALSE(service_impl_->send_message_callback_);
    if (!run_loop_quit_closure_.is_null())
      run_loop_quit_closure_.Run();
  }

  void RunListenForSessionMessages(const std::string& text_msg,
                                   const std::vector<uint8_t>& binary_data,
                                   bool pass_ownership) {
    mojo::Array<blink::mojom::SessionMessagePtr> expected_msgs(2);
    expected_msgs[0] = blink::mojom::SessionMessage::New();
    expected_msgs[0]->type = blink::mojom::PresentationMessageType::TEXT;
    expected_msgs[0]->message = text_msg;
    expected_msgs[1] = blink::mojom::SessionMessage::New();
    expected_msgs[1]->type =
        blink::mojom::PresentationMessageType::ARRAY_BUFFER;
    expected_msgs[1]->data = mojo::Array<uint8_t>::From(binary_data);

    blink::mojom::PresentationSessionInfoPtr session(
        blink::mojom::PresentationSessionInfo::New());
    session->url = kPresentationUrl;
    session->id = kPresentationId;

    PresentationSessionMessageCallback message_cb;
    {
    base::RunLoop run_loop;
    EXPECT_CALL(mock_delegate_, ListenForSessionMessages(_, _, _, _))
        .WillOnce(DoAll(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit),
                        SaveArg<3>(&message_cb)));
    service_ptr_->ListenForSessionMessages(session.Clone());
    run_loop.Run();
    }

    ScopedVector<PresentationSessionMessage> messages;
    std::unique_ptr<content::PresentationSessionMessage> message;
    message.reset(
        new content::PresentationSessionMessage(PresentationMessageType::TEXT));
    message->message = text_msg;
    messages.push_back(std::move(message));
    message.reset(new content::PresentationSessionMessage(
        PresentationMessageType::ARRAY_BUFFER));
    message->data.reset(new std::vector<uint8_t>(binary_data));
    messages.push_back(std::move(message));

    std::vector<blink::mojom::SessionMessagePtr> actual_msgs;
    {
      base::RunLoop run_loop;
      EXPECT_CALL(mock_client_, MessagesReceived())
          .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
      message_cb.Run(std::move(messages), pass_ownership);
      run_loop.Run();
    }
    ExpectSessionMessages(expected_msgs, mock_client_.messages_received_);
  }

  MockPresentationServiceDelegate mock_delegate_;

  std::unique_ptr<PresentationServiceImpl> service_impl_;
  mojo::InterfacePtr<blink::mojom::PresentationService> service_ptr_;

  MockPresentationServiceClient mock_client_;
  std::unique_ptr<mojo::Binding<blink::mojom::PresentationServiceClient>>
      client_binding_;

  base::Closure run_loop_quit_closure_;
};

TEST_F(PresentationServiceImplTest, ListenForScreenAvailability) {
  ListenForScreenAvailabilityAndWait(kPresentationUrl, true);

  SimulateScreenAvailabilityChangeAndWait(kPresentationUrl, true);
  SimulateScreenAvailabilityChangeAndWait(kPresentationUrl, false);
  SimulateScreenAvailabilityChangeAndWait(kPresentationUrl, true);
}

TEST_F(PresentationServiceImplTest, Reset) {
  ListenForScreenAvailabilityAndWait(kPresentationUrl, true);

  ExpectReset();
  service_impl_->Reset();
  ExpectCleanState();
}

TEST_F(PresentationServiceImplTest, DidNavigateThisFrame) {
  ListenForScreenAvailabilityAndWait(kPresentationUrl, true);

  ExpectReset();
  service_impl_->DidNavigateAnyFrame(
      contents()->GetMainFrame(),
      content::LoadCommittedDetails(),
      content::FrameNavigateParams());
  ExpectCleanState();
}

TEST_F(PresentationServiceImplTest, DidNavigateOtherFrame) {
  ListenForScreenAvailabilityAndWait(kPresentationUrl, true);

  // TODO(imcheng): How to get a different RenderFrameHost?
  service_impl_->DidNavigateAnyFrame(
      nullptr,
      content::LoadCommittedDetails(),
      content::FrameNavigateParams());

  // Availability is reported and callback is invoked since it was not
  // removed.
  SimulateScreenAvailabilityChangeAndWait(kPresentationUrl, true);
}

TEST_F(PresentationServiceImplTest, ThisRenderFrameDeleted) {
  ListenForScreenAvailabilityAndWait(kPresentationUrl, true);

  ExpectReset();

  // Since the frame matched the service, |service_impl_| will be deleted.
  PresentationServiceImpl* service = service_impl_.release();
  EXPECT_CALL(mock_delegate_, RemoveObserver(_, _)).Times(1);
  service->RenderFrameDeleted(contents()->GetMainFrame());
}

TEST_F(PresentationServiceImplTest, OtherRenderFrameDeleted) {
  ListenForScreenAvailabilityAndWait(kPresentationUrl, true);

  // TODO(imcheng): How to get a different RenderFrameHost?
  service_impl_->RenderFrameDeleted(nullptr);

  // Availability is reported and callback should be invoked since listener
  // has not been deleted.
  SimulateScreenAvailabilityChangeAndWait(kPresentationUrl, true);
}

TEST_F(PresentationServiceImplTest, DelegateFails) {
  ListenForScreenAvailabilityAndWait(kPresentationUrl, false);
  ASSERT_EQ(
      service_impl_->screen_availability_listeners_.find(kPresentationUrl),
      service_impl_->screen_availability_listeners_.end());
}

TEST_F(PresentationServiceImplTest, SetDefaultPresentationUrl) {
  std::string url1("http://fooUrl");
  EXPECT_CALL(mock_delegate_, SetDefaultPresentationUrl(_, _, Eq(url1), _))
      .Times(1);
  service_impl_->SetDefaultPresentationURL(url1);
  EXPECT_EQ(url1, service_impl_->default_presentation_url_);

  std::string url2("http://barUrl");
  // Sets different DPU.
  content::PresentationSessionStartedCallback callback;
  EXPECT_CALL(mock_delegate_, SetDefaultPresentationUrl(_, _, Eq(url2), _))
      .WillOnce(SaveArg<3>(&callback));
  service_impl_->SetDefaultPresentationURL(url2);
  EXPECT_EQ(url2, service_impl_->default_presentation_url_);

  blink::mojom::PresentationSessionInfo session_info;
  session_info.url = url2;
  session_info.id = kPresentationId;
  base::RunLoop run_loop;
  EXPECT_CALL(mock_client_, OnDefaultSessionStarted(Equals(session_info)))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  callback.Run(content::PresentationSessionInfo(url2, kPresentationId));
  run_loop.Run();
}

TEST_F(PresentationServiceImplTest, ListenForConnectionStateChange) {
  content::PresentationSessionInfo connection(kPresentationUrl,
                                              kPresentationId);
  content::PresentationConnectionStateChangedCallback state_changed_cb;
  EXPECT_CALL(mock_delegate_, ListenForConnectionStateChange(_, _, _, _))
      .WillOnce(SaveArg<3>(&state_changed_cb));
  service_impl_->ListenForConnectionStateChange(connection);

  // Trigger state change. It should be propagated back up to |mock_client_|.
  blink::mojom::PresentationSessionInfo presentation_connection;
  presentation_connection.url = kPresentationUrl;
  presentation_connection.id = kPresentationId;
  {
    base::RunLoop run_loop;
    EXPECT_CALL(mock_client_,
                OnConnectionStateChanged(
                    Equals(presentation_connection),
                    blink::mojom::PresentationConnectionState::TERMINATED))
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    state_changed_cb.Run(PresentationConnectionStateChangeInfo(
        PRESENTATION_CONNECTION_STATE_TERMINATED));
    run_loop.Run();
  }
}

TEST_F(PresentationServiceImplTest, ListenForConnectionClose) {
  content::PresentationSessionInfo connection(kPresentationUrl,
                                              kPresentationId);
  content::PresentationConnectionStateChangedCallback state_changed_cb;
  EXPECT_CALL(mock_delegate_, ListenForConnectionStateChange(_, _, _, _))
      .WillOnce(SaveArg<3>(&state_changed_cb));
  service_impl_->ListenForConnectionStateChange(connection);

  // Trigger connection close. It should be propagated back up to
  // |mock_client_|.
  blink::mojom::PresentationSessionInfo presentation_connection;
  presentation_connection.url = kPresentationUrl;
  presentation_connection.id = kPresentationId;
  {
    base::RunLoop run_loop;
    PresentationConnectionStateChangeInfo closed_info(
        PRESENTATION_CONNECTION_STATE_CLOSED);
    closed_info.close_reason = PRESENTATION_CONNECTION_CLOSE_REASON_WENT_AWAY;
    closed_info.message = "Foo";

    EXPECT_CALL(mock_client_,
                OnConnectionClosed(
                    Equals(presentation_connection),
                    blink::mojom::PresentationConnectionCloseReason::WENT_AWAY,
                    mojo::String("Foo")))
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    state_changed_cb.Run(closed_info);
    run_loop.Run();
  }
}

TEST_F(PresentationServiceImplTest, SetSameDefaultPresentationUrl) {
  EXPECT_CALL(mock_delegate_,
              SetDefaultPresentationUrl(_, _, Eq(kPresentationUrl), _))
      .Times(1);
  service_impl_->SetDefaultPresentationURL(kPresentationUrl);
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_delegate_));
  EXPECT_EQ(kPresentationUrl, service_impl_->default_presentation_url_);

  // Same URL as before; no-ops.
  service_impl_->SetDefaultPresentationURL(kPresentationUrl);
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_delegate_));
  EXPECT_EQ(kPresentationUrl, service_impl_->default_presentation_url_);
}

TEST_F(PresentationServiceImplTest, StartSessionSuccess) {
  service_ptr_->StartSession(
      kPresentationUrl,
      base::Bind(
          &PresentationServiceImplTest::ExpectNewSessionCallbackSuccess,
          base::Unretained(this)));
  base::RunLoop run_loop;
  base::Callback<void(const PresentationSessionInfo&)> success_cb;
  EXPECT_CALL(mock_delegate_, StartSession(_, _, Eq(kPresentationUrl), _, _))
      .WillOnce(DoAll(
            InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit),
            SaveArg<3>(&success_cb)));
  run_loop.Run();

  EXPECT_CALL(mock_delegate_, ListenForConnectionStateChange(_, _, _, _))
      .Times(1);
  success_cb.Run(PresentationSessionInfo(kPresentationUrl, kPresentationId));
  SaveQuitClosureAndRunLoop();
}

TEST_F(PresentationServiceImplTest, StartSessionError) {
  service_ptr_->StartSession(
      kPresentationUrl,
      base::Bind(
          &PresentationServiceImplTest::ExpectNewSessionCallbackError,
          base::Unretained(this)));
  base::RunLoop run_loop;
  base::Callback<void(const PresentationError&)> error_cb;
  EXPECT_CALL(mock_delegate_, StartSession(_, _, Eq(kPresentationUrl), _, _))
      .WillOnce(DoAll(
            InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit),
            SaveArg<4>(&error_cb)));
  run_loop.Run();
  error_cb.Run(PresentationError(PRESENTATION_ERROR_UNKNOWN, "Error message"));
  SaveQuitClosureAndRunLoop();
}

TEST_F(PresentationServiceImplTest, JoinSessionSuccess) {
  service_ptr_->JoinSession(
      kPresentationUrl,
      kPresentationId,
      base::Bind(
          &PresentationServiceImplTest::ExpectNewSessionCallbackSuccess,
          base::Unretained(this)));
  base::RunLoop run_loop;
  base::Callback<void(const PresentationSessionInfo&)> success_cb;
  EXPECT_CALL(mock_delegate_, JoinSession(
      _, _, Eq(kPresentationUrl), Eq(kPresentationId), _, _))
      .WillOnce(DoAll(
            InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit),
            SaveArg<4>(&success_cb)));
  run_loop.Run();

  EXPECT_CALL(mock_delegate_, ListenForConnectionStateChange(_, _, _, _))
      .Times(1);
  success_cb.Run(PresentationSessionInfo(kPresentationUrl, kPresentationId));
  SaveQuitClosureAndRunLoop();
}

TEST_F(PresentationServiceImplTest, JoinSessionError) {
  service_ptr_->JoinSession(
      kPresentationUrl,
      kPresentationId,
      base::Bind(
          &PresentationServiceImplTest::ExpectNewSessionCallbackError,
          base::Unretained(this)));
  base::RunLoop run_loop;
  base::Callback<void(const PresentationError&)> error_cb;
  EXPECT_CALL(mock_delegate_, JoinSession(
      _, _, Eq(kPresentationUrl), Eq(kPresentationId), _, _))
      .WillOnce(DoAll(
            InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit),
            SaveArg<5>(&error_cb)));
  run_loop.Run();
  error_cb.Run(PresentationError(PRESENTATION_ERROR_UNKNOWN, "Error message"));
  SaveQuitClosureAndRunLoop();
}

TEST_F(PresentationServiceImplTest, CloseConnection) {
  service_ptr_->CloseConnection(kPresentationUrl, kPresentationId);
  base::RunLoop run_loop;
  EXPECT_CALL(mock_delegate_, CloseConnection(_, _, Eq(kPresentationId)))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  run_loop.Run();
}

TEST_F(PresentationServiceImplTest, Terminate) {
  service_ptr_->Terminate(kPresentationUrl, kPresentationId);
  base::RunLoop run_loop;
  EXPECT_CALL(mock_delegate_, Terminate(_, _, Eq(kPresentationId)))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  run_loop.Run();
}

TEST_F(PresentationServiceImplTest, ListenForSessionMessagesPassed) {
  std::string text_msg("123");
  std::vector<uint8_t> binary_data(3, '\1');
  RunListenForSessionMessages(text_msg, binary_data, true);
}

TEST_F(PresentationServiceImplTest, ListenForSessionMessagesCopied) {
  std::string text_msg("123");
  std::vector<uint8_t> binary_data(3, '\1');
  RunListenForSessionMessages(text_msg, binary_data, false);
}

TEST_F(PresentationServiceImplTest, ListenForSessionMessagesWithEmptyMsg) {
  std::string text_msg("");
  std::vector<uint8_t> binary_data;
  RunListenForSessionMessages(text_msg, binary_data, false);
}

TEST_F(PresentationServiceImplTest, StartSessionInProgress) {
  std::string presentation_url1("http://fooUrl");
  std::string presentation_url2("http://barUrl");
  EXPECT_CALL(mock_delegate_, StartSession(_, _, Eq(presentation_url1), _, _))
      .Times(1);
  service_ptr_->StartSession(presentation_url1,
                             base::Bind(&DoNothing));

  // This request should fail immediately, since there is already a StartSession
  // in progress.
  service_ptr_->StartSession(
      presentation_url2,
      base::Bind(
          &PresentationServiceImplTest::ExpectNewSessionCallbackError,
          base::Unretained(this)));
  SaveQuitClosureAndRunLoop();
}

TEST_F(PresentationServiceImplTest, SendStringMessage) {
  std::string message("Test presentation session message");

  blink::mojom::PresentationSessionInfoPtr session(
      blink::mojom::PresentationSessionInfo::New());
  session->url = kPresentationUrl;
  session->id = kPresentationId;
  blink::mojom::SessionMessagePtr message_request(
      blink::mojom::SessionMessage::New());
  message_request->type = blink::mojom::PresentationMessageType::TEXT;
  message_request->message = message;
  service_ptr_->SendSessionMessage(
      std::move(session), std::move(message_request),
      base::Bind(&PresentationServiceImplTest::ExpectSendSessionMessageCallback,
                 base::Unretained(this)));

  base::RunLoop run_loop;
  base::Callback<void(bool)> send_message_cb;
  PresentationSessionMessage* test_message = nullptr;
  EXPECT_CALL(mock_delegate_, SendMessageRawPtr(_, _, _, _, _))
      .WillOnce(DoAll(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit),
                      SaveArg<3>(&test_message), SaveArg<4>(&send_message_cb)));
  run_loop.Run();

  // Make sure |test_message| gets deleted.
  std::unique_ptr<PresentationSessionMessage> scoped_test_message(test_message);
  EXPECT_TRUE(test_message);
  EXPECT_FALSE(test_message->is_binary());
  EXPECT_LE(test_message->message.size(), kMaxPresentationSessionMessageSize);
  EXPECT_EQ(message, test_message->message);
  ASSERT_FALSE(test_message->data);
  send_message_cb.Run(true);
  SaveQuitClosureAndRunLoop();
}

TEST_F(PresentationServiceImplTest, SendArrayBuffer) {
  // Test Array buffer data.
  const uint8_t buffer[] = {0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48};
  std::vector<uint8_t> data;
  data.assign(buffer, buffer + sizeof(buffer));

  blink::mojom::PresentationSessionInfoPtr session(
      blink::mojom::PresentationSessionInfo::New());
  session->url = kPresentationUrl;
  session->id = kPresentationId;
  blink::mojom::SessionMessagePtr message_request(
      blink::mojom::SessionMessage::New());
  message_request->type = blink::mojom::PresentationMessageType::ARRAY_BUFFER;
  message_request->data = mojo::Array<uint8_t>::From(data);
  service_ptr_->SendSessionMessage(
      std::move(session), std::move(message_request),
      base::Bind(&PresentationServiceImplTest::ExpectSendSessionMessageCallback,
                 base::Unretained(this)));

  base::RunLoop run_loop;
  base::Callback<void(bool)> send_message_cb;
  PresentationSessionMessage* test_message = nullptr;
  EXPECT_CALL(mock_delegate_, SendMessageRawPtr(_, _, _, _, _))
      .WillOnce(DoAll(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit),
                      SaveArg<3>(&test_message), SaveArg<4>(&send_message_cb)));
  run_loop.Run();

  // Make sure |test_message| gets deleted.
  std::unique_ptr<PresentationSessionMessage> scoped_test_message(test_message);
  EXPECT_TRUE(test_message);
  EXPECT_TRUE(test_message->is_binary());
  EXPECT_EQ(PresentationMessageType::ARRAY_BUFFER, test_message->type);
  EXPECT_TRUE(test_message->message.empty());
  ASSERT_TRUE(test_message->data);
  EXPECT_EQ(data.size(), test_message->data->size());
  EXPECT_LE(test_message->data->size(), kMaxPresentationSessionMessageSize);
  EXPECT_EQ(0, memcmp(buffer, &(*test_message->data)[0], sizeof(buffer)));
  send_message_cb.Run(true);
  SaveQuitClosureAndRunLoop();
}

TEST_F(PresentationServiceImplTest, SendArrayBufferWithExceedingLimit) {
  // Create buffer with size exceeding the limit.
  // Use same size as in content::kMaxPresentationSessionMessageSize.
  const size_t kMaxBufferSizeInBytes = 64 * 1024;  // 64 KB.
  uint8_t buffer[kMaxBufferSizeInBytes + 1];
  memset(buffer, 0, kMaxBufferSizeInBytes+1);
  std::vector<uint8_t> data;
  data.assign(buffer, buffer + sizeof(buffer));

  blink::mojom::PresentationSessionInfoPtr session(
      blink::mojom::PresentationSessionInfo::New());
  session->url = kPresentationUrl;
  session->id = kPresentationId;
  blink::mojom::SessionMessagePtr message_request(
      blink::mojom::SessionMessage::New());
  message_request->type = blink::mojom::PresentationMessageType::ARRAY_BUFFER;
  message_request->data = mojo::Array<uint8_t>::From(data);
  service_ptr_->SendSessionMessage(
      std::move(session), std::move(message_request),
      base::Bind(&PresentationServiceImplTest::ExpectSendSessionMessageCallback,
                 base::Unretained(this)));

  base::RunLoop run_loop;
  base::Callback<void(bool)> send_message_cb;
  PresentationSessionMessage* test_message = nullptr;
  EXPECT_CALL(mock_delegate_, SendMessageRawPtr(_, _, _, _, _))
      .WillOnce(DoAll(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit),
                      SaveArg<3>(&test_message), SaveArg<4>(&send_message_cb)));
  run_loop.Run();

  EXPECT_FALSE(test_message);
  send_message_cb.Run(true);
  SaveQuitClosureAndRunLoop();
}

TEST_F(PresentationServiceImplTest, SendBlobData) {
  const uint8_t buffer[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
  std::vector<uint8_t> data;
  data.assign(buffer, buffer + sizeof(buffer));

  blink::mojom::PresentationSessionInfoPtr session(
      blink::mojom::PresentationSessionInfo::New());
  session->url = kPresentationUrl;
  session->id = kPresentationId;
  blink::mojom::SessionMessagePtr message_request(
      blink::mojom::SessionMessage::New());
  message_request->type = blink::mojom::PresentationMessageType::BLOB;
  message_request->data = mojo::Array<uint8_t>::From(data);
  service_ptr_->SendSessionMessage(
      std::move(session), std::move(message_request),
      base::Bind(&PresentationServiceImplTest::ExpectSendSessionMessageCallback,
                 base::Unretained(this)));

  base::RunLoop run_loop;
  base::Callback<void(bool)> send_message_cb;
  PresentationSessionMessage* test_message = nullptr;
  EXPECT_CALL(mock_delegate_, SendMessageRawPtr(_, _, _, _, _))
      .WillOnce(DoAll(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit),
                      SaveArg<3>(&test_message), SaveArg<4>(&send_message_cb)));
  run_loop.Run();

  // Make sure |test_message| gets deleted.
  std::unique_ptr<PresentationSessionMessage> scoped_test_message(test_message);
  EXPECT_TRUE(test_message);
  EXPECT_TRUE(test_message->is_binary());
  EXPECT_EQ(PresentationMessageType::BLOB, test_message->type);
  EXPECT_TRUE(test_message->message.empty());
  ASSERT_TRUE(test_message->data);
  EXPECT_EQ(data.size(), test_message->data->size());
  EXPECT_LE(test_message->data->size(), kMaxPresentationSessionMessageSize);
  EXPECT_EQ(0, memcmp(buffer, &(*test_message->data)[0], sizeof(buffer)));
  send_message_cb.Run(true);
  SaveQuitClosureAndRunLoop();
}

TEST_F(PresentationServiceImplTest, MaxPendingJoinSessionRequests) {
  const char* presentation_url = "http://fooUrl%d";
  const char* presentation_id = "presentationId%d";
  int num_requests = PresentationServiceImpl::kMaxNumQueuedSessionRequests;
  int i = 0;
  EXPECT_CALL(mock_delegate_, JoinSession(_, _, _, _, _, _))
      .Times(num_requests);
  for (; i < num_requests; ++i) {
    service_ptr_->JoinSession(
        base::StringPrintf(presentation_url, i),
        base::StringPrintf(presentation_id, i),
        base::Bind(&DoNothing));
  }

  // Exceeded maximum queue size, should invoke mojo callback with error.
  service_ptr_->JoinSession(
        base::StringPrintf(presentation_url, i),
        base::StringPrintf(presentation_id, i),
        base::Bind(
            &PresentationServiceImplTest::ExpectNewSessionCallbackError,
            base::Unretained(this)));
  SaveQuitClosureAndRunLoop();
}

TEST_F(PresentationServiceImplTest, ScreenAvailabilityNotSupported) {
  mock_delegate_.set_screen_availability_listening_supported(false);
  base::RunLoop run_loop;
  EXPECT_CALL(mock_client_,
              OnScreenAvailabilityNotSupported(Eq(kPresentationUrl)))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  ListenForScreenAvailabilityAndWait(kPresentationUrl, false);
  run_loop.Run();
}

}  // namespace content
