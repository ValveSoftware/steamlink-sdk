// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_REMOTING_FAKE_REMOTING_CONTROLLER_H_
#define MEDIA_REMOTING_FAKE_REMOTING_CONTROLLER_H_

#include "media/base/decoder_buffer.h"
#include "media/mojo/interfaces/remoting.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace media {

class RemotingSourceImpl;

class FakeRemotingDataStreamSender : public mojom::RemotingDataStreamSender {
 public:
  FakeRemotingDataStreamSender(
      mojom::RemotingDataStreamSenderRequest request,
      mojo::ScopedDataPipeConsumerHandle consumer_handle);
  ~FakeRemotingDataStreamSender() override;

  uint32_t consume_data_chunk_count() const {
    return consume_data_chunk_count_;
  }
  uint32_t send_frame_count() const { return send_frame_count_; }
  uint32_t cancel_in_flight_count() const { return cancel_in_flight_count_; }
  void ResetHistory();
  bool ValidateFrameBuffer(size_t index,
                           size_t size,
                           bool key_frame,
                           int pts_ms);

 private:
  // mojom::RemotingDataStreamSender implementation.
  void ConsumeDataChunk(uint32_t offset,
                        uint32_t size,
                        uint32_t total_payload_size) final;
  void SendFrame() final;
  void CancelInFlightData() final;

  mojo::Binding<RemotingDataStreamSender> binding_;
  mojo::ScopedDataPipeConsumerHandle consumer_handle_;

  std::vector<uint8_t> next_frame_data_;
  std::vector<std::vector<uint8_t>> received_frame_list;
  uint32_t consume_data_chunk_count_;
  uint32_t send_frame_count_;
  uint32_t cancel_in_flight_count_;

  DISALLOW_COPY_AND_ASSIGN(FakeRemotingDataStreamSender);
};

class FakeRemoter final : public mojom::Remoter {
 public:
  // |start_will_fail| indicates whether starting remoting will fail.
  FakeRemoter(mojom::RemotingSourcePtr source, bool start_will_fail);
  ~FakeRemoter() override;

  // mojom::Remoter implementations.
  void Start() override;
  void StartDataStreams(
      mojo::ScopedDataPipeConsumerHandle audio_pipe,
      mojo::ScopedDataPipeConsumerHandle video_pipe,
      mojom::RemotingDataStreamSenderRequest audio_sender_request,
      mojom::RemotingDataStreamSenderRequest video_sender_request) override;
  void Stop(mojom::RemotingStopReason reason) override;
  void SendMessageToSink(const std::vector<uint8_t>& message) override;

 private:
  void Started();
  void StartFailed();
  void Stopped(mojom::RemotingStopReason reason);

  mojom::RemotingSourcePtr source_;
  bool start_will_fail_;

  std::unique_ptr<FakeRemotingDataStreamSender> audio_stream_sender_;
  std::unique_ptr<FakeRemotingDataStreamSender> video_stream_sender_;

  base::WeakPtrFactory<FakeRemoter> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeRemoter);
};

class FakeRemoterFactory final : public mojom::RemoterFactory {
 public:
  // |start_will_fail| indicates whether starting remoting will fail.
  explicit FakeRemoterFactory(bool start_will_fail);
  ~FakeRemoterFactory() override;

  // mojom::RemoterFactory implementation.
  void Create(mojom::RemotingSourcePtr source,
              mojom::RemoterRequest request) override;

 private:
  bool start_will_fail_;

  DISALLOW_COPY_AND_ASSIGN(FakeRemoterFactory);
};

scoped_refptr<RemotingSourceImpl> CreateRemotingSourceImpl(
    bool start_will_fail);

}  // namespace media

#endif  // MEDIA_REMOTING_FAKE_REMOTING_CONTROLLER_H_
