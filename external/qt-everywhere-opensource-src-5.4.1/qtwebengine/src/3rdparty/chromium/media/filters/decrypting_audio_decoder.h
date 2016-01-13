// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_DECRYPTING_AUDIO_DECODER_H_
#define MEDIA_FILTERS_DECRYPTING_AUDIO_DECODER_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "media/base/audio_decoder.h"
#include "media/base/decryptor.h"
#include "media/base/demuxer_stream.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

class AudioTimestampHelper;
class DecoderBuffer;
class Decryptor;

// Decryptor-based AudioDecoder implementation that can decrypt and decode
// encrypted audio buffers and return decrypted and decompressed audio frames.
// All public APIs and callbacks are trampolined to the |task_runner_| so
// that no locks are required for thread safety.
class MEDIA_EXPORT DecryptingAudioDecoder : public AudioDecoder {
 public:
  // We do not currently have a way to let the Decryptor choose the output
  // audio sample format and notify us of its choice. Therefore, we require all
  // Decryptor implementations to decode audio into a fixed integer sample
  // format designated by kSupportedBitsPerChannel.
  // TODO(xhwang): Remove this restriction after http://crbug.com/169105 fixed.
  static const int kSupportedBitsPerChannel;

  DecryptingAudioDecoder(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      const SetDecryptorReadyCB& set_decryptor_ready_cb);
  virtual ~DecryptingAudioDecoder();

  // AudioDecoder implementation.
  virtual void Initialize(const AudioDecoderConfig& config,
                          const PipelineStatusCB& status_cb,
                          const OutputCB& output_cb) OVERRIDE;
  virtual void Decode(const scoped_refptr<DecoderBuffer>& buffer,
                      const DecodeCB& decode_cb) OVERRIDE;
  virtual void Reset(const base::Closure& closure) OVERRIDE;
  virtual void Stop() OVERRIDE;

 private:
  // For a detailed state diagram please see this link: http://goo.gl/8jAok
  // TODO(xhwang): Add a ASCII state diagram in this file after this class
  // stabilizes.
  // TODO(xhwang): Update this diagram for DecryptingAudioDecoder.
  enum State {
    kUninitialized = 0,
    kDecryptorRequested,
    kPendingDecoderInit,
    kIdle,
    kPendingDecode,
    kWaitingForKey,
    kDecodeFinished,
    kStopped,
  };

  // Callback for DecryptorHost::RequestDecryptor().
  void SetDecryptor(Decryptor* decryptor);

  // Initializes the audio decoder on the |decryptor_| with |config_|.
  void InitializeDecoder();

  // Callback for Decryptor::InitializeAudioDecoder() during initialization.
  void FinishInitialization(bool success);

  void DecodePendingBuffer();

  // Callback for Decryptor::DecryptAndDecodeAudio().
  void DeliverFrame(int buffer_size,
                    Decryptor::Status status,
                    const Decryptor::AudioBuffers& frames);

  // Callback for the |decryptor_| to notify this object that a new key has been
  // added.
  void OnKeyAdded();

  // Resets decoder and calls |reset_cb_|.
  void DoReset();

  // Sets timestamps for |frames| and then passes them to |output_cb_|.
  void ProcessDecodedFrames(const Decryptor::AudioBuffers& frames);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  State state_;

  PipelineStatusCB init_cb_;
  OutputCB output_cb_;
  DecodeCB decode_cb_;
  base::Closure reset_cb_;
  base::Closure stop_cb_;

  // The current decoder configuration.
  AudioDecoderConfig config_;

  // Callback to request/cancel decryptor creation notification.
  SetDecryptorReadyCB set_decryptor_ready_cb_;

  Decryptor* decryptor_;

  // The buffer that needs decrypting/decoding.
  scoped_refptr<media::DecoderBuffer> pending_buffer_to_decode_;

  // Indicates the situation where new key is added during pending decode
  // (in other words, this variable can only be set in state kPendingDecode).
  // If this variable is true and kNoKey is returned then we need to try
  // decrypting/decoding again in case the newly added key is the correct
  // decryption key.
  bool key_added_while_decode_pending_;

  scoped_ptr<AudioTimestampHelper> timestamp_helper_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<DecryptingAudioDecoder> weak_factory_;
  base::WeakPtr<DecryptingAudioDecoder> weak_this_;

  DISALLOW_COPY_AND_ASSIGN(DecryptingAudioDecoder);
};

}  // namespace media

#endif  // MEDIA_FILTERS_DECRYPTING_AUDIO_DECODER_H_
