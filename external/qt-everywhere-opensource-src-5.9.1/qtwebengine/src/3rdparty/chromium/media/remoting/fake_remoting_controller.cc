// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/fake_remoting_controller.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/remoting/remoting_source_impl.h"
#include "media/remoting/rpc/proto_utils.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

FakeRemotingDataStreamSender::FakeRemotingDataStreamSender(
    mojom::RemotingDataStreamSenderRequest request,
    mojo::ScopedDataPipeConsumerHandle consumer_handle)
    : binding_(this, std::move(request)),
      consumer_handle_(std::move(consumer_handle)),
      consume_data_chunk_count_(0),
      send_frame_count_(0),
      cancel_in_flight_count_(0) {}

FakeRemotingDataStreamSender::~FakeRemotingDataStreamSender() = default;

void FakeRemotingDataStreamSender::ResetHistory() {
  consume_data_chunk_count_ = 0;
  send_frame_count_ = 0;
  cancel_in_flight_count_ = 0;
  next_frame_data_.resize(0);
  received_frame_list.clear();
}

bool FakeRemotingDataStreamSender::ValidateFrameBuffer(size_t index,
                                                       size_t size,
                                                       bool key_frame,
                                                       int pts_ms) {
  if (index >= received_frame_list.size()) {
    VLOG(1) << "There is no such frame";
    return false;
  }

  const std::vector<uint8_t>& data = received_frame_list[index];
  scoped_refptr<::media::DecoderBuffer> media_buffer =
      remoting::ByteArrayToDecoderBuffer(data.data(), data.size());

  // Checks if pts is correct or not
  if (media_buffer->timestamp().InMilliseconds() != pts_ms) {
    VLOG(1) << "Pts should be:" << pts_ms << "("
            << media_buffer->timestamp().InMilliseconds() << ")";
    return false;
  }

  // Checks if key frame is set correct or not
  if (media_buffer->is_key_frame() != key_frame) {
    VLOG(1) << "Key frame should be:" << key_frame << "("
            << media_buffer->is_key_frame() << ")";
    return false;
  }

  // Checks if frame buffer size is correct or not
  if (media_buffer->data_size() != size) {
    VLOG(1) << "Buffer size should be:" << size << "("
            << media_buffer->data_size() << ")";
    return false;
  }

  // Checks if frame buffer is correct or not.
  bool return_value = true;
  const uint8_t* buffer = media_buffer->data();
  for (size_t i = 0; i < media_buffer->data_size(); ++i) {
    uint32_t value = static_cast<uint32_t>(i & 0xFF);
    if (value != static_cast<uint32_t>(buffer[i])) {
      VLOG(1) << "buffer index: " << i << " should be "
              << static_cast<uint32_t>(value) << " ("
              << static_cast<uint32_t>(buffer[i]) << ")";
      return_value = false;
    }
  }
  return return_value;
}

void FakeRemotingDataStreamSender::ConsumeDataChunk(
    uint32_t offset,
    uint32_t size,
    uint32_t total_payload_size) {
  next_frame_data_.resize(total_payload_size);
  MojoResult result = mojo::ReadDataRaw(consumer_handle_.get(),
                                        next_frame_data_.data() + offset, &size,
                                        MOJO_READ_DATA_FLAG_ALL_OR_NONE);
  CHECK(result == MOJO_RESULT_OK);
  ++consume_data_chunk_count_;
}

void FakeRemotingDataStreamSender::SendFrame() {
  ++send_frame_count_;
  received_frame_list.push_back(std::move(next_frame_data_));
  EXPECT_EQ(send_frame_count_, received_frame_list.size());
}

void FakeRemotingDataStreamSender::CancelInFlightData() {
  ++cancel_in_flight_count_;
}

FakeRemoter::FakeRemoter(mojom::RemotingSourcePtr source, bool start_will_fail)
    : source_(std::move(source)),
      start_will_fail_(start_will_fail),
      weak_factory_(this) {}

FakeRemoter::~FakeRemoter() {}

void FakeRemoter::Start() {
  if (start_will_fail_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&FakeRemoter::StartFailed, weak_factory_.GetWeakPtr()));
  } else {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&FakeRemoter::Started, weak_factory_.GetWeakPtr()));
  }
}

void FakeRemoter::StartDataStreams(
    mojo::ScopedDataPipeConsumerHandle audio_pipe,
    mojo::ScopedDataPipeConsumerHandle video_pipe,
    mojom::RemotingDataStreamSenderRequest audio_sender_request,
    mojom::RemotingDataStreamSenderRequest video_sender_request) {
  if (audio_pipe.is_valid()) {
    VLOG(2) << "Has audio";
    audio_stream_sender_.reset(new FakeRemotingDataStreamSender(
        std::move(audio_sender_request), std::move(audio_pipe)));
  }

  if (video_pipe.is_valid()) {
    VLOG(2) << "Has video";
    video_stream_sender_.reset(new FakeRemotingDataStreamSender(
        std::move(video_sender_request), std::move(video_pipe)));
  }
}

void FakeRemoter::Stop(mojom::RemotingStopReason reason) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&FakeRemoter::Stopped, weak_factory_.GetWeakPtr(), reason));
}

void FakeRemoter::SendMessageToSink(const std::vector<uint8_t>& message) {}

void FakeRemoter::Started() {
  source_->OnStarted();
}

void FakeRemoter::StartFailed() {
  source_->OnStartFailed(mojom::RemotingStartFailReason::ROUTE_TERMINATED);
}

void FakeRemoter::Stopped(mojom::RemotingStopReason reason) {
  source_->OnStopped(reason);
}

FakeRemoterFactory::FakeRemoterFactory(bool start_will_fail)
    : start_will_fail_(start_will_fail) {}

FakeRemoterFactory::~FakeRemoterFactory() {}

void FakeRemoterFactory::Create(mojom::RemotingSourcePtr source,
                                mojom::RemoterRequest request) {
  mojo::MakeStrongBinding(
      base::MakeUnique<FakeRemoter>(std::move(source), start_will_fail_),
      std::move(request));
}

scoped_refptr<RemotingSourceImpl> CreateRemotingSourceImpl(
    bool start_will_fail) {
  mojom::RemotingSourcePtr remoting_source;
  mojom::RemotingSourceRequest remoting_source_request =
      mojo::GetProxy(&remoting_source);
  mojom::RemoterPtr remoter;
  std::unique_ptr<mojom::RemoterFactory> remoter_factory =
      base::MakeUnique<FakeRemoterFactory>(start_will_fail);
  remoter_factory->Create(std::move(remoting_source), mojo::GetProxy(&remoter));
  return new RemotingSourceImpl(std::move(remoting_source_request),
                                std::move(remoter));
}

}  // namespace media
