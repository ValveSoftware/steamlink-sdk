// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_REMOTING_REMOTE_DEMUXER_STREAM_ADAPTER_H_
#define MEDIA_REMOTING_REMOTE_DEMUXER_STREAM_ADAPTER_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/demuxer_stream.h"
#include "media/base/video_decoder_config.h"
#include "media/mojo/interfaces/remoting.mojom.h"
#include "media/remoting/rpc/rpc_broker.h"
#include "mojo/public/cpp/system/data_pipe.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

class DemuxerStream;

namespace remoting {

// Class to fetch audio/video buffer from demuxer and send it to browser process
// via mojo::Remoting interface. Note the class is created and run on media
// thread using |media_task_runner|, Mojo data pipe should run on media thread,
// while RPC message should be sent on main thread using |main_task_runner|.
class RemoteDemuxerStreamAdapter {
 public:
  // |main_task_runner|: Task runner to post RPC message on main thread
  // |media_task_runner|: Task runner to run whole class on media thread.
  // |name|: Demuxer stream name. For troubleshooting purposes.
  // |demuxer_stream|: Demuxer component.
  // |rpc_broker|: Broker class to handle incoming and outgoing RPC message. It
  //               is used only on the main thread.
  // |stream_sender_info|: Transfer of pipe binding on the media thread. It is
  //                       to access mojo interface for sending data stream.
  // |producer_handle|: handle to send data using mojo data pipe.
  RemoteDemuxerStreamAdapter(
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
      const std::string& name,
      ::media::DemuxerStream* demuxer_stream,
      const base::WeakPtr<RpcBroker>& rpc_broker,
      mojom::RemotingDataStreamSenderPtrInfo stream_sender_info,
      mojo::ScopedDataPipeProducerHandle producer_handle);
  ~RemoteDemuxerStreamAdapter();

  // Rpc handle for this class. This is used for sending/receiving RPC message
  // with specific hanle using Rpcbroker.
  int rpc_handle() const { return rpc_handle_; }

  // Signals if system is in flushing state. The caller uses |flushing| to
  // signal when flush starts and when is done. During flush operation, all
  // fetching data actions will be discarded. The return value indicates frame
  // count in order to signal receiver what frames are in flight before flush,
  // or base::nullopt if the flushing state was unchanged.
  base::Optional<uint32_t> SignalFlush(bool flushing);

 private:
  friend class MockRemoteDemuxerStreamAdapter;

  // Receives RPC message from RpcBroker.
  void OnReceivedRpc(std::unique_ptr<remoting::pb::RpcMessage> message);

  // RPC message tasks.
  void Initialize(int remote_callback_handle);
  void ReadUntil(std::unique_ptr<remoting::pb::RpcMessage> message);
  void RequestBuffer(int callback_handle);
  void SendReadAck(int callback_handle);

  // Callback function when retrieving data from demuxer.
  void OnNewBuffer(int callback_handle,
                   ::media::DemuxerStream::Status status,
                   const scoped_refptr<::media::DecoderBuffer>& input);
  void TryWriteData(int callback_handle, MojoResult result);
  void ResetPendingFrame();

  // Callback function when data pipe error occurs.
  void OnFatalError();

  const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  const scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;

  // Name of demuxer stream. Debug only.
  const std::string name_;

  // Weak pointer of RpcBroker. It should use |main_task_runner_| to access the
  // interfaces.
  const base::WeakPtr<RpcBroker> rpc_broker_;

  // RPC handle for this demuxer stream service.
  const int rpc_handle_;

  // Demuxer stream and stream type.
  ::media::DemuxerStream* const demuxer_stream_;
  const ::media::DemuxerStream::Type type_;

  // Remote RPC handles to process demuxer stream data.
  int remote_callback_handle_;

  // Current frame count issued by RPC_DS_READUNTIL RPC message. It should send
  // all frame data with count id smaller than |read_until_count_| before
  // sending RPC_DS_READUNTIL_CALLBACK back to receiver.
  uint32_t read_until_count_;

  // Count id of last frame sent.
  uint32_t last_count_;

  // Indicates if the class is processing RPC_DS_READUNTIL RPC message. The flag
  // is to prevent from receiving multiple RPC_DS_READUNTIL RPC messages. This
  // is set to true after receiving RPC_DS_READUNTIL and before sending
  // RPC_DS_READUNTIL_CALLBACK back to receiver.
  bool processing_read_rpc_;

  // Flag to indicate if it's on flushing operation. All actions should be
  // aborted and data should be discarded when the value is true.
  bool pending_flush_;

  // Frame buffer and its information that is currently in process of writing to
  // Mojo data pipe.
  std::vector<uint8_t> pending_frame_;
  uint32_t current_pending_frame_offset_;
  bool pending_frame_is_eos_;

  // Monitor if data pipe is available to write data.
  mojo::Watcher write_watcher_;

  // Keeps latest demuxer stream status and audio/video decoder config.
  ::media::DemuxerStream::Status media_status_;
  ::media::AudioDecoderConfig audio_config_;
  ::media::VideoDecoderConfig video_config_;

  ::media::mojom::RemotingDataStreamSenderPtr stream_sender_;
  mojo::ScopedDataPipeProducerHandle producer_handle_;

  // WeakPtrFactory only for reading buffer from demuxer stream. This is used
  // for canceling all read callbacks provided to the |demuxer_stream_| before a
  // flush.
  base::WeakPtrFactory<RemoteDemuxerStreamAdapter> request_buffer_weak_factory_;
  // WeakPtrFactory for normal usage.
  base::WeakPtrFactory<RemoteDemuxerStreamAdapter> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RemoteDemuxerStreamAdapter);
};

mojo::DataPipe* CreateDataPipe();

}  // namespace remoting
}  // namespace media

#endif  // MEDIA_REMOTING_REMOTE_DEMUXER_STREAM_ADAPTER_H_
