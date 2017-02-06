// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_DECODER_STREAM_H_
#define MEDIA_FILTERS_DECODER_STREAM_H_

#include <deque>
#include <list>
#include <memory>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "media/base/audio_decoder.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_export.h"
#include "media/base/media_log.h"
#include "media/base/moving_average.h"
#include "media/base/pipeline_status.h"
#include "media/base/timestamp_constants.h"
#include "media/filters/decoder_selector.h"
#include "media/filters/decoder_stream_traits.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

class CdmContext;
class DecryptingDemuxerStream;

// Wraps a DemuxerStream and a list of Decoders and provides decoded
// output to its client (e.g. Audio/VideoRendererImpl).
template<DemuxerStream::Type StreamType>
class MEDIA_EXPORT DecoderStream {
 public:
  typedef DecoderStreamTraits<StreamType> StreamTraits;
  typedef typename StreamTraits::DecoderType Decoder;
  typedef typename StreamTraits::OutputType Output;

  enum Status {
    OK,  // Everything went as planned.
    ABORTED,  // Read aborted due to Reset() during pending read.
    DEMUXER_READ_ABORTED,  // Demuxer returned aborted read.
    DECODE_ERROR,  // Decoder returned decode error.
  };

  // Indicates completion of a DecoderStream initialization.
  typedef base::Callback<void(bool success)> InitCB;

  // Indicates completion of a DecoderStream read.
  typedef base::Callback<void(Status, const scoped_refptr<Output>&)> ReadCB;

  DecoderStream(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      ScopedVector<Decoder> decoders,
      const scoped_refptr<MediaLog>& media_log);
  virtual ~DecoderStream();

  // Returns the string representation of the StreamType for logging purpose.
  std::string GetStreamTypeString();

  // Initializes the DecoderStream and returns the initialization result
  // through |init_cb|. Note that |init_cb| is always called asynchronously.
  // |cdm_context| can be used to handle encrypted stream. Can be null if the
  // stream is not encrypted.
  void Initialize(DemuxerStream* stream,
                  const InitCB& init_cb,
                  CdmContext* cdm_context,
                  const StatisticsCB& statistics_cb,
                  const base::Closure& waiting_for_decryption_key_cb);

  // Reads a decoded Output and returns it via the |read_cb|. Note that
  // |read_cb| is always called asynchronously. This method should only be
  // called after initialization has succeeded and must not be called during
  // pending Reset().
  void Read(const ReadCB& read_cb);

  // Resets the decoder, flushes all decoded outputs and/or internal buffers,
  // fires any existing pending read callback and calls |closure| on completion.
  // Note that |closure| is always called asynchronously. This method should
  // only be called after initialization has succeeded and must not be called
  // during pending Reset().
  // N.B: If the decoder stream has run into an error, calling this method does
  // not 'reset' it to a normal state.
  void Reset(const base::Closure& closure);

  // Returns true if the decoder currently has the ability to decode and return
  // an Output.
  // TODO(rileya): Remove the need for this by refactoring Decoder queueing
  // behavior.
  bool CanReadWithoutStalling() const;

  // Returns maximum concurrent decode requests for the current |decoder_|.
  int GetMaxDecodeRequests() const;

  // Returns true if one more decode request can be submitted to the decoder.
  bool CanDecodeMore() const;

  base::TimeDelta AverageDuration() const;

  // Allows callers to register for notification of splice buffers from the
  // demuxer.  I.e., DecoderBuffer::splice_timestamp() is not kNoTimestamp().
  //
  // The observer will be notified of all buffers with a splice_timestamp() and
  // the first buffer after which has a splice_timestamp() of kNoTimestamp().
  typedef base::Callback<void(base::TimeDelta)> SpliceObserverCB;
  void set_splice_observer(const SpliceObserverCB& splice_observer) {
    splice_observer_cb_ = splice_observer;
  }

  // Allows callers to register for notification of config changes; this is
  // called immediately after receiving the 'kConfigChanged' status from the
  // DemuxerStream, before any action is taken to handle the config change.
  typedef base::Closure ConfigChangeObserverCB;
  void set_config_change_observer(
      const ConfigChangeObserverCB& config_change_observer) {
    config_change_observer_cb_ = config_change_observer;
  }

  const Decoder* get_previous_decoder_for_testing() const {
    return previous_decoder_.get();
  }

  int get_pending_buffers_size_for_testing() const {
    return pending_buffers_.size();
  }

  int get_fallback_buffers_size_for_testing() const {
    return fallback_buffers_.size();
  }

 private:
  enum State {
    STATE_UNINITIALIZED,
    STATE_INITIALIZING,
    STATE_NORMAL,  // Includes idle, pending decoder decode/reset.
    STATE_FLUSHING_DECODER,
    STATE_REINITIALIZING_DECODER,
    STATE_END_OF_STREAM,  // End of stream reached; returns EOS on all reads.
    STATE_ERROR,
  };

  void SelectDecoder(CdmContext* cdm_context);

  // Called when |decoder_selector| selected the |selected_decoder|.
  // |decrypting_demuxer_stream| was also populated if a DecryptingDemuxerStream
  // is created to help decrypt the encrypted stream.
  void OnDecoderSelected(
      std::unique_ptr<Decoder> selected_decoder,
      std::unique_ptr<DecryptingDemuxerStream> decrypting_demuxer_stream);

  // Satisfy pending |read_cb_| with |status| and |output|.
  void SatisfyRead(Status status,
                   const scoped_refptr<Output>& output);

  // Decodes |buffer| and returns the result via OnDecodeOutputReady().
  // Saves |buffer| into |pending_buffers_| if appropriate.
  void Decode(const scoped_refptr<DecoderBuffer>& buffer);

  // Performs the heavy lifting of the decode call.
  void DecodeInternal(const scoped_refptr<DecoderBuffer>& buffer);

  // Flushes the decoder with an EOS buffer to retrieve internally buffered
  // decoder output.
  void FlushDecoder();

  // Callback for Decoder::Decode().
  void OnDecodeDone(int buffer_size, bool end_of_stream, DecodeStatus status);

  // Output callback passed to Decoder::Initialize().
  void OnDecodeOutputReady(const scoped_refptr<Output>& output);

  // Reads a buffer from |stream_| and returns the result via OnBufferReady().
  void ReadFromDemuxerStream();

  // Callback for DemuxerStream::Read().
  void OnBufferReady(DemuxerStream::Status status,
                     const scoped_refptr<DecoderBuffer>& buffer);

  void ReinitializeDecoder();

  // Callback for Decoder reinitialization.
  void OnDecoderReinitialized(bool success);

  void CompleteDecoderReinitialization(bool success);

  void ResetDecoder();
  void OnDecoderReset();

  DecoderStreamTraits<StreamType> traits_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  scoped_refptr<MediaLog> media_log_;

  State state_;

  StatisticsCB statistics_cb_;
  InitCB init_cb_;
  base::Closure waiting_for_decryption_key_cb_;

  ReadCB read_cb_;
  base::Closure reset_cb_;

  DemuxerStream* stream_;

  std::unique_ptr<DecoderSelector<StreamType>> decoder_selector_;

  std::unique_ptr<Decoder> decoder_;
  // When falling back from H/W decoding to S/W decoding, destructing the
  // GpuVideoDecoder too early results in black frames being displayed.
  // |previous_decoder_| is used to keep it alive.  It is destroyed once we've
  // decoded at least media::limits::kMaxVideoFrames frames after fallback.
  int decoded_frames_since_fallback_;
  std::unique_ptr<Decoder> previous_decoder_;
  std::unique_ptr<DecryptingDemuxerStream> decrypting_demuxer_stream_;

  SpliceObserverCB splice_observer_cb_;
  ConfigChangeObserverCB config_change_observer_cb_;

  // If a splice_timestamp() has been seen, this is true until a
  // splice_timestamp() of kNoTimestamp() is encountered.
  bool active_splice_;

  // An end-of-stream buffer has been sent for decoding, no more buffers should
  // be sent for decoding until it completes.
  // TODO(sandersd): Turn this into a State. http://crbug.com/408316
  bool decoding_eos_;

  // Decoded buffers that haven't been read yet. Used when the decoder supports
  // parallel decoding.
  std::list<scoped_refptr<Output> > ready_outputs_;

  // Number of outstanding decode requests sent to the |decoder_|.
  int pending_decode_requests_;

  // Tracks the duration of incoming packets over time.
  MovingAverage duration_tracker_;

  // Stores buffers that might be reused if the decoder fails right after
  // Initialize().
  std::deque<scoped_refptr<DecoderBuffer>> pending_buffers_;

  // Stores buffers that are guaranteed to be fed to the decoder before fetching
  // more from the demuxer stream. All buffers in this queue first were in
  // |pending_buffers_|.
  std::deque<scoped_refptr<DecoderBuffer>> fallback_buffers_;

  // TODO(tguilbert): support config changes during decoder fallback, see
  // crbug.com/603713
  bool received_config_change_during_reinit_;

  // Used to track read requests; not rolled into |state_| since that is
  // overwritten in many cases.
  bool pending_demuxer_read_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<DecoderStream<StreamType>> weak_factory_;

  // Used to invalidate pending decode requests and output callbacks when
  // falling back to a new decoder (on first decode error).
  base::WeakPtrFactory<DecoderStream<StreamType>> fallback_weak_factory_;
};

template <>
bool DecoderStream<DemuxerStream::AUDIO>::CanReadWithoutStalling() const;

template <>
int DecoderStream<DemuxerStream::AUDIO>::GetMaxDecodeRequests() const;

typedef DecoderStream<DemuxerStream::VIDEO> VideoFrameStream;
typedef DecoderStream<DemuxerStream::AUDIO> AudioBufferStream;

}  // namespace media

#endif  // MEDIA_FILTERS_DECODER_STREAM_H_
