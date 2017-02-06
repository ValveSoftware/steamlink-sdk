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

namespace media {

class DecoderBuffer;

// A helper class that converts mojom::DecoderBuffer to media::DecoderBuffer.
// The data part of the DecoderBuffer is read from a DataPipe.
class MojoDecoderBufferReader {
 public:
  // Creates a MojoDecoderBufferReader of |type| and set the |producer_handle|.
  static std::unique_ptr<MojoDecoderBufferReader> Create(
      DemuxerStream::Type type,
      mojo::ScopedDataPipeProducerHandle* producer_handle);

  // Hold the consumer handle to read DecoderBuffer data.
  explicit MojoDecoderBufferReader(
      mojo::ScopedDataPipeConsumerHandle consumer_handle);

  ~MojoDecoderBufferReader();

  // Converts |buffer| into a DecoderBuffer (read data from DataPipe if needed).
  // Returns the result DecoderBuffer. Returns null in case of an error.
  scoped_refptr<DecoderBuffer> ReadDecoderBuffer(
      const mojom::DecoderBufferPtr& buffer);

 private:
  // For reading the data section of a DecoderBuffer.
  mojo::ScopedDataPipeConsumerHandle consumer_handle_;

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

  // Converts a DecoderBuffer into mojo DecoderBuffer, writing data into
  // DataPipe if needed. Returns null if conversion failed.
  mojom::DecoderBufferPtr WriteDecoderBuffer(
      const scoped_refptr<DecoderBuffer>& media_buffer);

 private:
  // For writing the data section of DecoderBuffer into DataPipe.
  mojo::ScopedDataPipeProducerHandle producer_handle_;

  DISALLOW_COPY_AND_ASSIGN(MojoDecoderBufferWriter);
};

}  // namespace media

#endif  // MEDIA_MOJO_COMMON_MOJO_DECODER_BUFFER_CONVERTER_
