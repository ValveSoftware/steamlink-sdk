// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_CLIENTS_MOJO_DECRYPTOR_H_
#define MEDIA_MOJO_CLIENTS_MOJO_DECRYPTOR_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "media/base/decryptor.h"
#include "media/mojo/interfaces/decryptor.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace media {

class MojoDecoderBufferReader;
class MojoDecoderBufferWriter;

// A Decryptor implementation based on mojom::DecryptorPtr.
// This class is single threaded. The |remote_decryptor| is connected before
// being passed to MojoDecryptor, but it is bound to the thread MojoDecryptor
// lives on the first time it is used in this class.
class MojoDecryptor : public Decryptor {
 public:
  explicit MojoDecryptor(mojom::DecryptorPtr remote_decryptor);
  ~MojoDecryptor() final;

  // Decryptor implementation.
  void RegisterNewKeyCB(StreamType stream_type,
                        const NewKeyCB& key_added_cb) final;
  void Decrypt(StreamType stream_type,
               const scoped_refptr<DecoderBuffer>& encrypted,
               const DecryptCB& decrypt_cb) final;
  void CancelDecrypt(StreamType stream_type) final;
  void InitializeAudioDecoder(const AudioDecoderConfig& config,
                              const DecoderInitCB& init_cb) final;
  void InitializeVideoDecoder(const VideoDecoderConfig& config,
                              const DecoderInitCB& init_cb) final;
  void DecryptAndDecodeAudio(const scoped_refptr<DecoderBuffer>& encrypted,
                             const AudioDecodeCB& audio_decode_cb) final;
  void DecryptAndDecodeVideo(const scoped_refptr<DecoderBuffer>& encrypted,
                             const VideoDecodeCB& video_decode_cb) final;
  void ResetDecoder(StreamType stream_type) final;
  void DeinitializeDecoder(StreamType stream_type) final;

  // Called when keys have changed and an additional key is available.
  void OnKeyAdded();

 private:
  // Called when a buffer is decrypted.
  void OnBufferDecrypted(const DecryptCB& decrypt_cb,
                         Status status,
                         mojom::DecoderBufferPtr buffer);
  void OnBufferRead(const DecryptCB& decrypt_cb,
                    Status status,
                    scoped_refptr<DecoderBuffer> buffer);
  void OnAudioDecoded(const AudioDecodeCB& audio_decode_cb,
                      Status status,
                      std::vector<mojom::AudioBufferPtr> audio_buffers);
  void OnVideoDecoded(const VideoDecodeCB& video_decode_cb,
                      Status status,
                      mojom::VideoFramePtr video_frame);

  // Called when done with a VideoFrame in order to reuse the shared memory.
  void ReleaseSharedBuffer(mojo::ScopedSharedBufferHandle buffer,
                           size_t buffer_size);

  base::ThreadChecker thread_checker_;

  mojom::DecryptorPtr remote_decryptor_;

  // Helper class to send DecoderBuffer to the |remote_decryptor_| for decrypt
  // or decrypt-and-decode.
  std::unique_ptr<MojoDecoderBufferWriter> mojo_decoder_buffer_writer_;

  // Helper class to receive decrypted DecoderBuffer from the
  // |remote_decryptor_|.
  std::unique_ptr<MojoDecoderBufferReader> mojo_decoder_buffer_reader_;

  NewKeyCB new_audio_key_cb_;
  NewKeyCB new_video_key_cb_;

  base::WeakPtrFactory<MojoDecryptor> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MojoDecryptor);
};

}  // namespace media

#endif  // MEDIA_MOJO_CLIENTS_MOJO_DECRYPTOR_H_
