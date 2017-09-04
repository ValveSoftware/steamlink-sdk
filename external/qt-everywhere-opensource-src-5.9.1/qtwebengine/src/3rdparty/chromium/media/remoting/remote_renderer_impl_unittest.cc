// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/remote_renderer_impl.h"

#include "base/run_loop.h"
#include "media/base/pipeline_status.h"
#include "media/base/test_helpers.h"
#include "media/remoting/fake_remoting_controller.h"
#include "media/remoting/fake_remoting_demuxer_stream_provider.h"
#include "media/remoting/remoting_renderer_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::Return;

namespace media {

namespace {

PipelineMetadata DefaultMetadata() {
  PipelineMetadata data;
  data.has_audio = true;
  data.has_video = true;
  data.video_decoder_config = TestVideoConfig::Normal();
  return data;
}

class RendererClientImpl : public RendererClient {
 public:
  RendererClientImpl() {
    ON_CALL(*this, OnStatisticsUpdate(_))
        .WillByDefault(
            Invoke(this, &RendererClientImpl::DelegateOnStatisticsUpdate));
    ON_CALL(*this, OnPipelineStatus(_))
        .WillByDefault(
            Invoke(this, &RendererClientImpl::DelegateOnPipelineStatus));
    ON_CALL(*this, OnBufferingStateChange(_))
        .WillByDefault(
            Invoke(this, &RendererClientImpl::DelegateOnBufferingStateChange));
    ON_CALL(*this, OnVideoNaturalSizeChange(_))
        .WillByDefault(Invoke(
            this, &RendererClientImpl::DelegateOnVideoNaturalSizeChange));
    ON_CALL(*this, OnVideoOpacityChange(_))
        .WillByDefault(
            Invoke(this, &RendererClientImpl::DelegateOnVideoOpacityChange));
    ON_CALL(*this, OnDurationChange(_))
        .WillByDefault(
            Invoke(this, &RendererClientImpl::DelegateOnDurationChange));
  }
  ~RendererClientImpl() {}

  // RendererClient implementation.
  void OnError(PipelineStatus status) override {}
  void OnEnded() override {}
  MOCK_METHOD1(OnStatisticsUpdate, void(const PipelineStatistics& stats));
  MOCK_METHOD1(OnBufferingStateChange, void(BufferingState state));
  void OnWaitingForDecryptionKey() override {}
  MOCK_METHOD1(OnVideoNaturalSizeChange, void(const gfx::Size& size));
  MOCK_METHOD1(OnVideoOpacityChange, void(bool opaque));
  MOCK_METHOD1(OnDurationChange, void(base::TimeDelta duration));

  void DelegateOnStatisticsUpdate(const PipelineStatistics& stats) {
    stats_ = stats;
  }
  void DelegateOnBufferingStateChange(BufferingState state) { state_ = state; }
  void DelegateOnVideoNaturalSizeChange(const gfx::Size& size) { size_ = size; }
  void DelegateOnVideoOpacityChange(bool opaque) { opaque_ = opaque; }
  void DelegateOnDurationChange(base::TimeDelta duration) {
    duration_ = duration;
  }

  MOCK_METHOD1(OnPipelineStatus, void(PipelineStatus status));
  void DelegateOnPipelineStatus(PipelineStatus status) {
    VLOG(2) << "OnPipelineStatus status:" << status;
    status_ = status;
  }
  MOCK_METHOD0(OnFlushCallback, void());

  PipelineStatus status() const { return status_; }
  PipelineStatistics stats() const { return stats_; }
  BufferingState state() const { return state_; }
  gfx::Size size() const { return size_; }
  bool opaque() const { return opaque_; }
  base::TimeDelta duration() const { return duration_; }

 private:
  PipelineStatus status_ = PIPELINE_OK;
  BufferingState state_ = BUFFERING_HAVE_NOTHING;
  gfx::Size size_;
  bool opaque_ = false;
  base::TimeDelta duration_;
  PipelineStatistics stats_;

  DISALLOW_COPY_AND_ASSIGN(RendererClientImpl);
};

}  // namespace

class RemoteRendererImplTest : public testing::Test {
 public:
  RemoteRendererImplTest()
      : receiver_renderer_handle_(10),
        receiver_audio_demuxer_callback_handle_(11),
        receiver_video_demuxer_callback_handle_(12),
        sender_client_handle_(remoting::kInvalidHandle),
        sender_renderer_callback_handle_(remoting::kInvalidHandle),
        sender_audio_demuxer_handle_(remoting::kInvalidHandle),
        sender_video_demuxer_handle_(remoting::kInvalidHandle),
        received_audio_ds_init_cb_(false),
        received_video_ds_init_cb_(false),
        renderer_initialized_(false) {}
  ~RemoteRendererImplTest() override = default;

  // Use this function to mimic receiver to handle RPC message for renderer
  // initialization,
  void RpcMessageResponseBot(std::unique_ptr<std::vector<uint8_t>> message) {
    std::unique_ptr<remoting::pb::RpcMessage> rpc(
        new remoting::pb::RpcMessage());
    ASSERT_TRUE(rpc->ParseFromArray(message->data(), message->size()));
    switch (rpc->proc()) {
      case remoting::pb::RpcMessage::RPC_ACQUIRE_RENDERER: {
        // Issues RPC_ACQUIRE_RENDERER_DONE RPC message.
        std::unique_ptr<remoting::pb::RpcMessage> acquire_done(
            new remoting::pb::RpcMessage());
        acquire_done->set_handle(rpc->integer_value());
        acquire_done->set_proc(
            remoting::pb::RpcMessage::RPC_ACQUIRE_RENDERER_DONE);
        acquire_done->set_integer_value(receiver_renderer_handle_);
        remoting_renderer_controller_->GetRpcBroker()->ProcessMessageFromRemote(
            std::move(acquire_done));
      } break;
      case remoting::pb::RpcMessage::RPC_R_INITIALIZE: {
        EXPECT_EQ(rpc->handle(), receiver_renderer_handle_);
        sender_renderer_callback_handle_ =
            rpc->renderer_initialize_rpc().callback_handle();
        sender_client_handle_ = rpc->renderer_initialize_rpc().client_handle();
        sender_audio_demuxer_handle_ =
            rpc->renderer_initialize_rpc().audio_demuxer_handle();
        sender_video_demuxer_handle_ =
            rpc->renderer_initialize_rpc().video_demuxer_handle();

        // Issues audio RPC_DS_INITIALIZE RPC message.
        if (sender_audio_demuxer_handle_ != remoting::kInvalidHandle) {
          std::unique_ptr<remoting::pb::RpcMessage> ds_init(
              new remoting::pb::RpcMessage());
          ds_init->set_handle(sender_audio_demuxer_handle_);
          ds_init->set_proc(remoting::pb::RpcMessage::RPC_DS_INITIALIZE);
          ds_init->set_integer_value(receiver_audio_demuxer_callback_handle_);
          remoting_renderer_controller_->GetRpcBroker()
              ->ProcessMessageFromRemote(std::move(ds_init));
        }
        if (sender_video_demuxer_handle_ != remoting::kInvalidHandle) {
          std::unique_ptr<remoting::pb::RpcMessage> ds_init(
              new remoting::pb::RpcMessage());
          ds_init->set_handle(sender_video_demuxer_handle_);
          ds_init->set_proc(remoting::pb::RpcMessage::RPC_DS_INITIALIZE);
          ds_init->set_integer_value(receiver_video_demuxer_callback_handle_);
          remoting_renderer_controller_->GetRpcBroker()
              ->ProcessMessageFromRemote(std::move(ds_init));
        }
      } break;
      case remoting::pb::RpcMessage::RPC_DS_INITIALIZE_CALLBACK: {
        if (rpc->handle() == receiver_audio_demuxer_callback_handle_)
          received_audio_ds_init_cb_ = true;
        if (rpc->handle() == receiver_video_demuxer_callback_handle_)
          received_video_ds_init_cb_ = true;

        // Issues RPC_R_INITIALIZE_CALLBACK RPC message when receiving
        // RPC_DS_INITIALIZE_CALLBACK on available streams.
        if (received_audio_ds_init_cb_ ==
                (sender_audio_demuxer_handle_ != remoting::kInvalidHandle) &&
            received_video_ds_init_cb_ ==
                (sender_video_demuxer_handle_ != remoting::kInvalidHandle)) {
          std::unique_ptr<remoting::pb::RpcMessage> init_cb(
              new remoting::pb::RpcMessage());
          init_cb->set_handle(sender_renderer_callback_handle_);
          init_cb->set_proc(
              remoting::pb::RpcMessage::RPC_R_INITIALIZE_CALLBACK);
          init_cb->set_boolean_value(true);
          remoting_renderer_controller_->GetRpcBroker()
              ->ProcessMessageFromRemote(std::move(init_cb));
          renderer_initialized_ = true;
        }

      } break;
      case remoting::pb::RpcMessage::RPC_R_FLUSHUNTIL: {
        // Issues RPC_R_FLUSHUNTIL_CALLBACK RPC message.
        std::unique_ptr<remoting::pb::RpcMessage> flush_cb(
            new remoting::pb::RpcMessage());
        flush_cb->set_handle(rpc->renderer_flushuntil_rpc().callback_handle());
        flush_cb->set_proc(remoting::pb::RpcMessage::RPC_R_FLUSHUNTIL_CALLBACK);
        remoting_renderer_controller_->GetRpcBroker()->ProcessMessageFromRemote(
            std::move(flush_cb));

      } break;

      default:
        NOTREACHED();
    }
    RunPendingTasks();
  }

  // Callback from RpcBroker when sending message to remote sink.
  void OnSendMessageToSink(std::unique_ptr<std::vector<uint8_t>> message) {
    std::unique_ptr<remoting::pb::RpcMessage> rpc(
        new remoting::pb::RpcMessage());
    ASSERT_TRUE(rpc->ParseFromArray(message->data(), message->size()));
    received_rpc_.push_back(std::move(rpc));
  }

 protected:
  void InitializeRenderer() {
    // Register media::RendererClient implementation.
    render_client_.reset(new RendererClientImpl());
    demuxer_stream_provider_.reset(new FakeRemotingDemuxerStreamProvider());
    EXPECT_CALL(*render_client_, OnPipelineStatus(_)).Times(1);
    DCHECK(remote_renderer_impl_);
    // Redirect RPC message for simulate receiver scenario
    remoting_renderer_controller_->GetRpcBroker()->SetMessageCallbackForTesting(
        base::Bind(&RemoteRendererImplTest::RpcMessageResponseBot,
                   base::Unretained(this)));
    RunPendingTasks();
    remote_renderer_impl_->Initialize(
        demuxer_stream_provider_.get(), render_client_.get(),
        base::Bind(&RendererClientImpl::OnPipelineStatus,
                   base::Unretained(render_client_.get())));
    RunPendingTasks();
    // Redirect RPC message back to save for later check.
    remoting_renderer_controller_->GetRpcBroker()->SetMessageCallbackForTesting(
        base::Bind(&RemoteRendererImplTest::OnSendMessageToSink,
                   base::Unretained(this)));
    RunPendingTasks();
  }

  bool IsRendererInitialized() const {
    return remote_renderer_impl_->state_ == RemoteRendererImpl::STATE_PLAYING;
  }

  void OnReceivedRpc(std::unique_ptr<remoting::pb::RpcMessage> message) {
    remote_renderer_impl_->OnReceivedRpc(std::move(message));
  }

  void SetUp() override {
    // Creates RemotingRendererController.
    remoting_renderer_controller_ =
        base::MakeUnique<RemotingRendererController>(
            CreateRemotingSourceImpl(false));
    remoting_renderer_controller_->OnMetadataChanged(DefaultMetadata());

    // Redirect RPC message to RemoteRendererImplTest::OnSendMessageToSink().
    remoting_renderer_controller_->GetRpcBroker()->SetMessageCallbackForTesting(
        base::Bind(&RemoteRendererImplTest::OnSendMessageToSink,
                   base::Unretained(this)));

    // Creates RemoteRendererImpl.
    remote_renderer_impl_.reset(new RemoteRendererImpl(
        base::ThreadTaskRunnerHandle::Get(),
        remoting_renderer_controller_->GetWeakPtr(), nullptr));
    RunPendingTasks();
  }

  RemoteRendererImpl::State state() const {
    return remote_renderer_impl_->state_;
  }

  void RunPendingTasks() { base::RunLoop().RunUntilIdle(); }

  // Gets first available RpcMessage with specific |proc|.
  const remoting::pb::RpcMessage* PeekRpcMessage(int proc) const {
    for (auto& s : received_rpc_) {
      if (proc == s->proc())
        return s.get();
    }
    return nullptr;
  }
  int ReceivedRpcMessageCount() const { return received_rpc_.size(); }
  void ResetReceivedRpcMessage() { received_rpc_.clear(); }

  void ValidateCurrentTime(base::TimeDelta current,
                           base::TimeDelta current_max) const {
    ASSERT_EQ(remote_renderer_impl_->current_media_time_, current);
    ASSERT_EQ(remote_renderer_impl_->current_max_time_, current_max);
  }

  std::unique_ptr<RemotingRendererController> remoting_renderer_controller_;
  std::unique_ptr<RendererClientImpl> render_client_;
  std::unique_ptr<FakeRemotingDemuxerStreamProvider> demuxer_stream_provider_;
  std::unique_ptr<RemoteRendererImpl> remote_renderer_impl_;
  base::MessageLoop message_loop_;

  // RPC handles.
  const int receiver_renderer_handle_;
  const int receiver_audio_demuxer_callback_handle_;
  const int receiver_video_demuxer_callback_handle_;
  int sender_client_handle_;
  int sender_renderer_callback_handle_;
  int sender_audio_demuxer_handle_;
  int sender_video_demuxer_handle_;

  // flag to indicate if RPC_DS_INITIALIZE_CALLBACK RPC messages are received.
  bool received_audio_ds_init_cb_;
  bool received_video_ds_init_cb_;

  // Check if |remote_renderer_impl_| is initialized successfully or not.
  bool renderer_initialized_;

  // vector to store received RPC message with proc value
  std::vector<std::unique_ptr<remoting::pb::RpcMessage>> received_rpc_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RemoteRendererImplTest);
};

TEST_F(RemoteRendererImplTest, Initialize) {
  InitializeRenderer();
  RunPendingTasks();

  ASSERT_TRUE(IsRendererInitialized());
  ASSERT_EQ(render_client_->status(), PIPELINE_OK);
}

TEST_F(RemoteRendererImplTest, Flush) {
  // Initialize Renderer.
  InitializeRenderer();
  RunPendingTasks();
  ASSERT_TRUE(IsRendererInitialized());
  ASSERT_EQ(render_client_->status(), PIPELINE_OK);

  // Flush Renderer.
  // Redirect RPC message for simulate receiver scenario
  remoting_renderer_controller_->GetRpcBroker()->SetMessageCallbackForTesting(
      base::Bind(&RemoteRendererImplTest::RpcMessageResponseBot,
                 base::Unretained(this)));
  RunPendingTasks();
  EXPECT_CALL(*render_client_, OnFlushCallback()).Times(1);
  remote_renderer_impl_->Flush(
      base::Bind(&RendererClientImpl::OnFlushCallback,
                 base::Unretained(render_client_.get())));
  RunPendingTasks();
}

TEST_F(RemoteRendererImplTest, StartPlayingFrom) {
  // Initialize Renderer
  InitializeRenderer();
  RunPendingTasks();
  ASSERT_TRUE(IsRendererInitialized());
  ASSERT_EQ(render_client_->status(), PIPELINE_OK);

  // StartPlaying from
  base::TimeDelta seek = base::TimeDelta::FromMicroseconds(100);
  remote_renderer_impl_->StartPlayingFrom(seek);
  RunPendingTasks();

  // Checks if it sends out RPC message with correct value.
  ASSERT_EQ(1, ReceivedRpcMessageCount());
  const remoting::pb::RpcMessage* rpc =
      PeekRpcMessage(remoting::pb::RpcMessage::RPC_R_STARTPLAYINGFROM);
  ASSERT_TRUE(rpc);
  ASSERT_EQ(rpc->integer64_value(), 100);
}

// SetVolume can be called anytime without conditions.
TEST_F(RemoteRendererImplTest, SetVolume) {
  // SetVolume() will send remoting::pb::RpcMessage::RPC_R_SETVOLUME RPC.
  remote_renderer_impl_->SetVolume(3.0);
  RunPendingTasks();

  // Checks if it sends out RPC message with correct value.
  ASSERT_EQ(1, ReceivedRpcMessageCount());
  const remoting::pb::RpcMessage* rpc =
      PeekRpcMessage(remoting::pb::RpcMessage::RPC_R_SETVOLUME);
  ASSERT_TRUE(rpc);
  ASSERT_TRUE(rpc->double_value() == 3.0);
}

// SetVolume can be called only when the state is playing.
TEST_F(RemoteRendererImplTest, SetPlaybackRate) {
  // Without initializing renderer, SetPlaybackRate() will be no-op.
  remote_renderer_impl_->SetPlaybackRate(-1.5);
  RunPendingTasks();
  ASSERT_EQ(0, ReceivedRpcMessageCount());

  // Initialize Renderer
  InitializeRenderer();
  RunPendingTasks();

  remoting_renderer_controller_->GetRpcBroker()->SetMessageCallbackForTesting(
      base::Bind(&RemoteRendererImplTest::OnSendMessageToSink,
                 base::Unretained(this)));
  remote_renderer_impl_->SetPlaybackRate(2.5);
  RunPendingTasks();
  ASSERT_EQ(1, ReceivedRpcMessageCount());
  // Checks if it sends out RPC message with correct value.
  const remoting::pb::RpcMessage* rpc =
      PeekRpcMessage(remoting::pb::RpcMessage::RPC_R_SETPLAYBACKRATE);
  ASSERT_TRUE(rpc);
  ASSERT_TRUE(rpc->double_value() == 2.5);
}

TEST_F(RemoteRendererImplTest, OnTimeUpdate) {
  // Issues RPC_RC_ONTIMEUPDATE RPC message.
  base::TimeDelta media_time = base::TimeDelta::FromMicroseconds(100);
  base::TimeDelta max_media_time = base::TimeDelta::FromMicroseconds(500);
  std::unique_ptr<remoting::pb::RpcMessage> rpc(new remoting::pb::RpcMessage());
  rpc->set_handle(5);
  rpc->set_proc(remoting::pb::RpcMessage::RPC_RC_ONTIMEUPDATE);
  auto* time_message = rpc->mutable_rendererclient_ontimeupdate_rpc();
  time_message->set_time_usec(media_time.InMicroseconds());
  time_message->set_max_time_usec(max_media_time.InMicroseconds());
  OnReceivedRpc(std::move(rpc));
  ValidateCurrentTime(media_time, max_media_time);

  // Issues RPC_RC_ONTIMEUPDATE RPC message with invalid time
  base::TimeDelta media_time2 = base::TimeDelta::FromMicroseconds(-100);
  base::TimeDelta max_media_time2 = base::TimeDelta::FromMicroseconds(500);
  std::unique_ptr<remoting::pb::RpcMessage> rpc2(
      new remoting::pb::RpcMessage());
  rpc2->set_handle(5);
  rpc2->set_proc(remoting::pb::RpcMessage::RPC_RC_ONTIMEUPDATE);
  auto* time_message2 = rpc2->mutable_rendererclient_ontimeupdate_rpc();
  time_message2->set_time_usec(media_time2.InMicroseconds());
  time_message2->set_max_time_usec(max_media_time2.InMicroseconds());
  OnReceivedRpc(std::move(rpc2));
  // Because of invalid value, the time will not be updated and remain the same.
  ValidateCurrentTime(media_time, max_media_time);
}

TEST_F(RemoteRendererImplTest, OnBufferingStateChange) {
  InitializeRenderer();
  // Issues RPC_RC_ONBUFFERINGSTATECHANGE RPC message.
  EXPECT_CALL(*render_client_,
              OnBufferingStateChange(media::BUFFERING_HAVE_NOTHING))
      .Times(1);
  std::unique_ptr<remoting::pb::RpcMessage> rpc(new remoting::pb::RpcMessage());
  rpc->set_handle(5);
  rpc->set_proc(remoting::pb::RpcMessage::RPC_RC_ONBUFFERINGSTATECHANGE);
  auto* buffering_state =
      rpc->mutable_rendererclient_onbufferingstatechange_rpc();
  buffering_state->set_state(
      remoting::pb::RendererClientOnBufferingStateChange::
          BUFFERING_HAVE_NOTHING);
  OnReceivedRpc(std::move(rpc));
  RunPendingTasks();
}

TEST_F(RemoteRendererImplTest, OnVideoNaturalSizeChange) {
  InitializeRenderer();
  // Makes sure initial value of video natural size is not set to
  // gfx::Size(100, 200).
  ASSERT_NE(render_client_->size().width(), 100);
  ASSERT_NE(render_client_->size().height(), 200);
  // Issues RPC_RC_ONVIDEONATURALSIZECHANGE RPC message.
  EXPECT_CALL(*render_client_, OnVideoNaturalSizeChange(gfx::Size(100, 200)))
      .Times(1);
  std::unique_ptr<remoting::pb::RpcMessage> rpc(new remoting::pb::RpcMessage());
  rpc->set_handle(5);
  rpc->set_proc(remoting::pb::RpcMessage::RPC_RC_ONVIDEONATURALSIZECHANGE);
  auto* size_message =
      rpc->mutable_rendererclient_onvideonatualsizechange_rpc();
  size_message->set_width(100);
  size_message->set_height(200);
  OnReceivedRpc(std::move(rpc));
  RunPendingTasks();
  ASSERT_EQ(render_client_->size().width(), 100);
  ASSERT_EQ(render_client_->size().height(), 200);
}

TEST_F(RemoteRendererImplTest, OnVideoNaturalSizeChangeWithInvalidValue) {
  InitializeRenderer();
  // Issues RPC_RC_ONVIDEONATURALSIZECHANGE RPC message.
  EXPECT_CALL(*render_client_, OnVideoNaturalSizeChange(_)).Times(0);
  std::unique_ptr<remoting::pb::RpcMessage> rpc(new remoting::pb::RpcMessage());
  rpc->set_handle(5);
  rpc->set_proc(remoting::pb::RpcMessage::RPC_RC_ONVIDEONATURALSIZECHANGE);
  auto* size_message =
      rpc->mutable_rendererclient_onvideonatualsizechange_rpc();
  size_message->set_width(-100);
  size_message->set_height(0);
  OnReceivedRpc(std::move(rpc));
  RunPendingTasks();
}

TEST_F(RemoteRendererImplTest, OnVideoOpacityChange) {
  InitializeRenderer();
  ASSERT_FALSE(render_client_->opaque());
  // Issues RPC_RC_ONVIDEOOPACITYCHANGE RPC message.
  EXPECT_CALL(*render_client_, OnVideoOpacityChange(true)).Times(1);
  std::unique_ptr<remoting::pb::RpcMessage> rpc(new remoting::pb::RpcMessage());
  rpc->set_handle(5);
  rpc->set_proc(remoting::pb::RpcMessage::RPC_RC_ONVIDEOOPACITYCHANGE);
  rpc->set_boolean_value(true);
  OnReceivedRpc(std::move(rpc));
  RunPendingTasks();
  ASSERT_TRUE(render_client_->opaque());
}

TEST_F(RemoteRendererImplTest, OnStatisticsUpdate) {
  InitializeRenderer();
  ASSERT_NE(render_client_->stats().audio_bytes_decoded, 1234U);
  ASSERT_NE(render_client_->stats().video_bytes_decoded, 2345U);
  ASSERT_NE(render_client_->stats().video_frames_decoded, 3456U);
  ASSERT_NE(render_client_->stats().video_frames_dropped, 4567U);
  ASSERT_NE(render_client_->stats().audio_memory_usage, 5678);
  ASSERT_NE(render_client_->stats().video_memory_usage, 6789);
  // Issues RPC_RC_ONSTATISTICSUPDATE RPC message.
  EXPECT_CALL(*render_client_, OnStatisticsUpdate(_)).Times(1);
  std::unique_ptr<remoting::pb::RpcMessage> rpc(new remoting::pb::RpcMessage());
  rpc->set_handle(5);
  rpc->set_proc(remoting::pb::RpcMessage::RPC_RC_ONSTATISTICSUPDATE);
  auto* message = rpc->mutable_rendererclient_onstatisticsupdate_rpc();
  message->set_audio_bytes_decoded(1234U);
  message->set_video_bytes_decoded(2345U);
  message->set_video_frames_decoded(3456U);
  message->set_video_frames_dropped(4567U);
  message->set_audio_memory_usage(5678);
  message->set_video_memory_usage(6789);
  OnReceivedRpc(std::move(rpc));
  RunPendingTasks();
  ASSERT_EQ(render_client_->stats().audio_bytes_decoded, 1234U);
  ASSERT_EQ(render_client_->stats().video_bytes_decoded, 2345U);
  ASSERT_EQ(render_client_->stats().video_frames_decoded, 3456U);
  ASSERT_EQ(render_client_->stats().video_frames_dropped, 4567U);
  ASSERT_EQ(render_client_->stats().audio_memory_usage, 5678);
  ASSERT_EQ(render_client_->stats().video_memory_usage, 6789);
}

TEST_F(RemoteRendererImplTest, OnDurationChange) {
  InitializeRenderer();
  ASSERT_NE(render_client_->duration(),
            base::TimeDelta::FromMicroseconds(1234));
  // Issues RPC_RC_ONDURATIONCHANGE RPC message.
  EXPECT_CALL(*render_client_,
              OnDurationChange(base::TimeDelta::FromMicroseconds(1234)))
      .Times(1);
  std::unique_ptr<remoting::pb::RpcMessage> rpc(new remoting::pb::RpcMessage());
  rpc->set_handle(5);
  rpc->set_proc(remoting::pb::RpcMessage::RPC_RC_ONDURATIONCHANGE);
  rpc->set_integer64_value(1234);
  OnReceivedRpc(std::move(rpc));
  RunPendingTasks();
  ASSERT_EQ(render_client_->duration(),
            base::TimeDelta::FromMicroseconds(1234));
}

TEST_F(RemoteRendererImplTest, OnDurationChangeWithInvalidValue) {
  InitializeRenderer();
  // Issues RPC_RC_ONDURATIONCHANGE RPC message.
  EXPECT_CALL(*render_client_, OnDurationChange(_)).Times(0);
  std::unique_ptr<remoting::pb::RpcMessage> rpc(new remoting::pb::RpcMessage());
  rpc->set_handle(5);
  rpc->set_proc(remoting::pb::RpcMessage::RPC_RC_ONDURATIONCHANGE);
  rpc->set_integer64_value(-345);
  OnReceivedRpc(std::move(rpc));
  RunPendingTasks();
}
}  // namespace media
