// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/bind.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/platform_thread.h"
#include "media/base/cdm_config.h"
#include "media/base/cdm_context.h"
#include "media/base/gmock_callback_support.h"
#include "media/base/mock_filters.h"
#include "media/base/test_helpers.h"
#include "media/cdm/default_cdm_factory.h"
#include "media/mojo/clients/mojo_renderer.h"
#include "media/mojo/common/media_type_converters.h"
#include "media/mojo/interfaces/content_decryption_module.mojom.h"
#include "media/mojo/interfaces/renderer.mojom.h"
#include "media/mojo/services/mojo_cdm_service.h"
#include "media/mojo/services/mojo_cdm_service_context.h"
#include "media/mojo/services/mojo_renderer_service.h"
#include "media/renderers/video_overlay_factory.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;

namespace media {

const int64_t kStartPlayingTimeInMs = 100;
const char kClearKeyKeySystem[] = "org.w3.clearkey";

ACTION_P2(SetError, renderer_client, error) {
  renderer_client->OnError(error);
}

class MojoRendererTest : public ::testing::Test {
 public:
  MojoRendererTest() {
    std::unique_ptr<StrictMock<MockRenderer>> mock_renderer(
        new StrictMock<MockRenderer>());
    mock_renderer_ = mock_renderer.get();

    mojom::RendererPtr remote_renderer;

    mojo_renderer_service_ = new MojoRendererService(
        mojo_cdm_service_context_.GetWeakPtr(), std::move(mock_renderer),
        mojo::GetProxy(&remote_renderer));

    mojo_renderer_.reset(
        new MojoRenderer(message_loop_.task_runner(),
                         std::unique_ptr<VideoOverlayFactory>(nullptr), nullptr,
                         std::move(remote_renderer)));

    // CreateAudioStream() and CreateVideoStream() overrides expectations for
    // expected non-NULL streams.
    EXPECT_CALL(demuxer_, GetStream(_)).WillRepeatedly(Return(nullptr));

    EXPECT_CALL(*mock_renderer_, GetMediaTime())
        .WillRepeatedly(Return(base::TimeDelta()));
  }

  virtual ~MojoRendererTest() {}

  void Destroy() {
    mojo_renderer_.reset();
    base::RunLoop().RunUntilIdle();
  }

  // Completion callbacks.
  MOCK_METHOD1(OnInitialized, void(PipelineStatus));
  MOCK_METHOD0(OnFlushed, void());
  MOCK_METHOD1(OnCdmAttached, void(bool));

  std::unique_ptr<StrictMock<MockDemuxerStream>> CreateStream(
      DemuxerStream::Type type) {
    std::unique_ptr<StrictMock<MockDemuxerStream>> stream(
        new StrictMock<MockDemuxerStream>(type));
    return stream;
  }

  void CreateAudioStream() {
    audio_stream_ = CreateStream(DemuxerStream::AUDIO);
    EXPECT_CALL(demuxer_, GetStream(DemuxerStream::AUDIO))
        .WillRepeatedly(Return(audio_stream_.get()));
  }

  void CreateVideoStream(bool is_encrypted = false) {
    video_stream_ = CreateStream(DemuxerStream::VIDEO);
    video_stream_->set_video_decoder_config(
        is_encrypted ? TestVideoConfig::NormalEncrypted()
                     : TestVideoConfig::Normal());
    EXPECT_CALL(demuxer_, GetStream(DemuxerStream::VIDEO))
        .WillRepeatedly(Return(video_stream_.get()));
  }

  void InitializeAndExpect(PipelineStatus status) {
    DVLOG(1) << __FUNCTION__ << ": " << status;
    EXPECT_CALL(*this, OnInitialized(status));
    mojo_renderer_->Initialize(
        &demuxer_, &renderer_client_,
        base::Bind(&MojoRendererTest::OnInitialized, base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
  }

  void Initialize() {
    CreateAudioStream();
    EXPECT_CALL(*mock_renderer_, Initialize(_, _, _))
        .WillOnce(DoAll(SaveArg<1>(&remote_renderer_client_),
                        RunCallback<2>(PIPELINE_OK)));
    InitializeAndExpect(PIPELINE_OK);
  }

  void Flush() {
    DVLOG(1) << __FUNCTION__;
    // Flush callback should always be fired.
    EXPECT_CALL(*this, OnFlushed());
    mojo_renderer_->Flush(
        base::Bind(&MojoRendererTest::OnFlushed, base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
  }

  void SetCdmAndExpect(bool success) {
    DVLOG(1) << __FUNCTION__;
    // Set CDM callback should always be fired.
    EXPECT_CALL(*this, OnCdmAttached(success));
    mojo_renderer_->SetCdm(
        &cdm_context_,
        base::Bind(&MojoRendererTest::OnCdmAttached, base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
  }

  // Simulates a connection error at the client side by killing the service.
  // Note that |mock_renderer_| will also be destroyed, do NOT expect anything
  // on it. Otherwise the test will crash.
  void ConnectionError() {
    DVLOG(1) << __FUNCTION__;
    delete mojo_renderer_service_;
    base::RunLoop().RunUntilIdle();
  }

  void OnCdmCreated(mojom::CdmPromiseResultPtr result,
                    int cdm_id,
                    mojom::DecryptorPtr decryptor) {
    EXPECT_TRUE(result->success);
    EXPECT_NE(CdmContext::kInvalidCdmId, cdm_id);
    cdm_context_.set_cdm_id(cdm_id);
  }

  void CreateCdm() {
    new MojoCdmService(mojo_cdm_service_context_.GetWeakPtr(), &cdm_factory_,
                       mojo::GetProxy(&remote_cdm_));
    remote_cdm_->Initialize(
        kClearKeyKeySystem, "https://www.test.com",
        mojom::CdmConfig::From(CdmConfig()),
        base::Bind(&MojoRendererTest::OnCdmCreated, base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
  }

  void StartPlayingFrom(base::TimeDelta start_time) {
    EXPECT_CALL(*mock_renderer_, StartPlayingFrom(start_time));
    mojo_renderer_->StartPlayingFrom(start_time);
    EXPECT_EQ(start_time, mojo_renderer_->GetMediaTime());
    base::RunLoop().RunUntilIdle();
  }

  void Play() {
    StartPlayingFrom(base::TimeDelta::FromMilliseconds(kStartPlayingTimeInMs));
  }

  // Fixture members.
  base::MessageLoop message_loop_;

  // The MojoRenderer that we are testing.
  std::unique_ptr<MojoRenderer> mojo_renderer_;

  // Client side mocks and helpers.
  StrictMock<MockRendererClient> renderer_client_;
  StrictMock<MockCdmContext> cdm_context_;
  mojom::ContentDecryptionModulePtr remote_cdm_;

  // Client side mock demuxer and demuxer streams.
  StrictMock<MockDemuxer> demuxer_;
  std::unique_ptr<StrictMock<MockDemuxerStream>> audio_stream_;
  std::unique_ptr<StrictMock<MockDemuxerStream>> video_stream_;

  // Service side mocks and helpers.
  StrictMock<MockRenderer>* mock_renderer_;
  MojoCdmServiceContext mojo_cdm_service_context_;
  RendererClient* remote_renderer_client_;
  DefaultCdmFactory cdm_factory_;

  // Owned by the connection. But we can delete it manually to trigger a
  // connection error at the client side. See ConnectionError();
  MojoRendererService* mojo_renderer_service_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MojoRendererTest);
};

TEST_F(MojoRendererTest, Initialize_Success) {
  Initialize();
}

TEST_F(MojoRendererTest, Initialize_Failure) {
  CreateAudioStream();
  // Mojo Renderer only expects a boolean result, which will be translated
  // to PIPELINE_OK or PIPELINE_ERROR_INITIALIZATION_FAILED.
  EXPECT_CALL(*mock_renderer_, Initialize(_, _, _))
      .WillOnce(RunCallback<2>(PIPELINE_ERROR_ABORT));
  InitializeAndExpect(PIPELINE_ERROR_INITIALIZATION_FAILED);
}

TEST_F(MojoRendererTest, Initialize_BeforeConnectionError) {
  CreateAudioStream();
  EXPECT_CALL(*mock_renderer_, Initialize(_, _, _))
      .WillOnce(InvokeWithoutArgs(this, &MojoRendererTest::ConnectionError));
  InitializeAndExpect(PIPELINE_ERROR_INITIALIZATION_FAILED);
}

TEST_F(MojoRendererTest, Initialize_AfterConnectionError) {
  ConnectionError();
  CreateAudioStream();
  InitializeAndExpect(PIPELINE_ERROR_INITIALIZATION_FAILED);
}

TEST_F(MojoRendererTest, Flush_Success) {
  Initialize();

  EXPECT_CALL(*mock_renderer_, Flush(_)).WillOnce(RunClosure<0>());
  Flush();
}

TEST_F(MojoRendererTest, Flush_ConnectionError) {
  Initialize();

  // Upon connection error, OnError() should be called once and only once.
  EXPECT_CALL(renderer_client_, OnError(PIPELINE_ERROR_DECODE)).Times(1);
  EXPECT_CALL(*mock_renderer_, Flush(_))
      .WillOnce(InvokeWithoutArgs(this, &MojoRendererTest::ConnectionError));
  Flush();
}

TEST_F(MojoRendererTest, Flush_AfterConnectionError) {
  Initialize();

  // Upon connection error, OnError() should be called once and only once.
  EXPECT_CALL(renderer_client_, OnError(PIPELINE_ERROR_DECODE)).Times(1);
  ConnectionError();

  Flush();
}

TEST_F(MojoRendererTest, SetCdm_Success) {
  Initialize();
  CreateCdm();
  EXPECT_CALL(*mock_renderer_, SetCdm(_, _)).WillOnce(RunCallback<1>(true));
  SetCdmAndExpect(true);
}

TEST_F(MojoRendererTest, SetCdm_Failure) {
  Initialize();
  CreateCdm();
  EXPECT_CALL(*mock_renderer_, SetCdm(_, _)).WillOnce(RunCallback<1>(false));
  SetCdmAndExpect(false);
}

TEST_F(MojoRendererTest, SetCdm_InvalidCdmId) {
  Initialize();
  SetCdmAndExpect(false);
}

TEST_F(MojoRendererTest, SetCdm_NonExistCdmId) {
  Initialize();
  cdm_context_.set_cdm_id(1);
  SetCdmAndExpect(false);
}

TEST_F(MojoRendererTest, SetCdm_BeforeInitialize) {
  CreateCdm();
  EXPECT_CALL(*mock_renderer_, SetCdm(_, _)).WillOnce(RunCallback<1>(true));
  SetCdmAndExpect(true);
}

TEST_F(MojoRendererTest, SetCdm_AfterInitializeAndConnectionError) {
  CreateCdm();
  Initialize();
  EXPECT_CALL(renderer_client_, OnError(PIPELINE_ERROR_DECODE)).Times(1);
  ConnectionError();
  SetCdmAndExpect(false);
}

TEST_F(MojoRendererTest, SetCdm_AfterConnectionErrorAndBeforeInitialize) {
  CreateCdm();
  // Initialize() is not called so RendererClient::OnError() is not expected.
  ConnectionError();
  SetCdmAndExpect(false);
  InitializeAndExpect(PIPELINE_ERROR_INITIALIZATION_FAILED);
}

TEST_F(MojoRendererTest, SetCdm_BeforeInitializeAndConnectionError) {
  CreateCdm();
  EXPECT_CALL(*mock_renderer_, SetCdm(_, _)).WillOnce(RunCallback<1>(true));
  SetCdmAndExpect(true);
  // Initialize() is not called so RendererClient::OnError() is not expected.
  ConnectionError();
  CreateAudioStream();
  InitializeAndExpect(PIPELINE_ERROR_INITIALIZATION_FAILED);
}

TEST_F(MojoRendererTest, StartPlayingFrom) {
  Initialize();
  Play();
}

TEST_F(MojoRendererTest, GetMediaTime) {
  Initialize();
  EXPECT_EQ(base::TimeDelta(), mojo_renderer_->GetMediaTime());

  Play();

  // Time is updated periodically with a short delay.
  const base::TimeDelta kUpdatedTime = base::TimeDelta::FromMilliseconds(500);
  EXPECT_CALL(*mock_renderer_, GetMediaTime())
      .WillRepeatedly(Return(kUpdatedTime));
  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(100));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kUpdatedTime, mojo_renderer_->GetMediaTime());
}

TEST_F(MojoRendererTest, OnEnded) {
  Initialize();
  Play();

  EXPECT_CALL(renderer_client_, OnEnded()).Times(1);
  remote_renderer_client_->OnEnded();
  base::RunLoop().RunUntilIdle();
}

// TODO(xhwang): Add tests for all RendererClient methods.

TEST_F(MojoRendererTest, Destroy_PendingInitialize) {
  CreateAudioStream();
  EXPECT_CALL(*mock_renderer_, Initialize(_, _, _))
      .WillRepeatedly(RunCallback<2>(PIPELINE_ERROR_ABORT));
  EXPECT_CALL(*this, OnInitialized(PIPELINE_ERROR_INITIALIZATION_FAILED));
  mojo_renderer_->Initialize(
      &demuxer_, &renderer_client_,
      base::Bind(&MojoRendererTest::OnInitialized, base::Unretained(this)));
  Destroy();
}

TEST_F(MojoRendererTest, Destroy_PendingFlush) {
  EXPECT_CALL(*mock_renderer_, SetCdm(_, _))
      .WillRepeatedly(RunCallback<1>(true));
  EXPECT_CALL(*this, OnCdmAttached(false));
  mojo_renderer_->SetCdm(
      &cdm_context_,
      base::Bind(&MojoRendererTest::OnCdmAttached, base::Unretained(this)));
  Destroy();
}

TEST_F(MojoRendererTest, Destroy_PendingSetCdm) {
  Initialize();
  EXPECT_CALL(*mock_renderer_, Flush(_)).WillRepeatedly(RunClosure<0>());
  EXPECT_CALL(*this, OnFlushed());
  mojo_renderer_->Flush(
      base::Bind(&MojoRendererTest::OnFlushed, base::Unretained(this)));
  Destroy();
}

// TODO(xhwang): Add more tests on OnError. For example, ErrorDuringFlush,
// ErrorAfterFlush etc.

}  // namespace media
