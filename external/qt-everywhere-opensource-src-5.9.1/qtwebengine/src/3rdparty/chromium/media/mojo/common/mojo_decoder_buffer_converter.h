// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_COMMON_MOJO_DECODER_BUFFER_CONVERTER_
#define MEDIA_MOJO_COMMON_MOJO_DECODER_BUFFER_CONVERTER_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "media/base/demuxer_stream.h"
#include "media/mojo/interfaces/media_types.mojom.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/watcher.h"

namespace media {

class DecoderBuffer;

// A helper class that converts mojom::DecoderBuffer to media::DecoderBuffer.
// The data part of the DecoderBuffer is read from a DataPipe.
class MojoDecoderBufferReader {
 public:
  using ReadCB = base::OnceCallback<void(scoped_refptr<DecoderBuffer>)>;

  // Creates a MojoDecoderBufferReader of |type| and set the |producer_handle|.
  static std::unique_ptr<MojoDecoderBufferReader> Create(
      DemuxerStream::Type type,
      mojo::ScopedDataPipeProducerHandle* producer_handle);

  // Hold the consumer handle to read DecoderBuffer data.
  explicit MojoDecoderBufferReader(
      mojo::ScopedDataPipeConsumerHandle consumer_handle);

  ~MojoDecoderBufferReader();

  // Converts |buffer| into a DecoderBuffer (read data from DataPipe if needed).
  // |read_cb| is called with the result DecoderBuffer.
  // Reports a null DecoderBuffer in case of an error.
  void ReadDecoderBuffer(mojom::DecoderBufferPtr buffer, ReadCB read_cb);

 private:
  void OnPipeError(MojoResult result);
  void OnPipeReadable(MojoResult result);
  void ReadDecoderBufferData();

  // For reading the data section of a DecoderBuffer.
  mojo::ScopedDataPipeConsumerHandle consumer_handle_;
  mojo::Watcher pipe_watcher_;

  // Only valid during pending read.
  ReadCB read_cb_;
  scoped_refptr<DecoderBuffer> media_buffer_;
  uint32_t bytes_read_;

  DISALLOW_COPY_AND_ASSIGN(MojoDecoderBufferReader);
};

// A helper class that converts media::DecoderBuffer to mojom::DecoderBuffer.
// The data part of the DecoderBuffer is written into a DataPipe.
class MojoDecoderBufferWriter {
 public:
  // Creates a MojoDecoderBufferWriter of |type| and set the |consumer_handle|.
  static std::unique_ptr<MojoDecoderBufferWriter> Create(
      DemuxerStream::Type type,
      mojo::ScopedDataPipeConsumerHandle* consumer_handle);

  // Hold the producer handle to write DecoderBuffer data.
  explicit MojoDecoderBufferWriter(
      mojo::ScopedDataPipeProducerHandle producer_handle);

  ~MojoDecoderBufferWriter();

  // Converts a DecoderBuffer into mojo DecoderBuffer.
  // DecoderBuffer data is asynchronously written into DataPipe if needed.
  // Returns null if conversion failed or if the data pipe is already closed.
  mojom::DecoderBufferPtr WriteDecoderBuffer(
      const scoped_refptr<DecoderBuffer>& media_buffer);

 private:
  void OnPipeError(MojoResult result);
  void OnPipeWritable(MojoResult result);
  MojoResult WriteDecoderBufferData();

  // For writing the data section of DecoderBuffer into DataPipe.
  mojo::ScopedDataPipeProducerHandle producer_handle_;
  mojo::Watcher pipe_watcher_;

  // Only valid when data is being written to the pipe.
  scoped_refptr<DecoderBuffer> media_buffer_;
  uint32_t bytes_written_;

  DISALLOW_COPY_AND_ASSIGN(MojoDecoderBufferWriter);
};

}  // namespace media

#endif  // MEDIA_MOJO_COMMON_MOJO_DECODER_BUFFER_CONVERTER_
