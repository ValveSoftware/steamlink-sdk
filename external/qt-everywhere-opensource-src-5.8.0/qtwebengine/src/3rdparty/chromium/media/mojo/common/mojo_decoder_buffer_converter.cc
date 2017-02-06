// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/common/mojo_decoder_buffer_converter.h"

#include <memory>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/audio_buffer.h"
#include "media/base/cdm_context.h"
#include "media/base/decoder_buffer.h"
#include "media/mojo/common/media_type_converters.h"

namespace media {

namespace {

std::unique_ptr<mojo::DataPipe> CreateDataPipe(DemuxerStream::Type type) {
  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE;
  options.element_num_bytes = 1;

  if (type == DemuxerStream::AUDIO) {
    // TODO(timav): Consider capacity calculation based on AudioDecoderConfig.
    options.capacity_num_bytes = 512 * 1024;
  } else if (type == DemuxerStream::VIDEO) {
    // Video can get quite large; at 4K, VP9 delivers packets which are ~1MB in
    // size; so allow for some head room.
    // TODO(xhwang, sandersd): Provide a better way to customize this value.
    options.capacity_num_bytes = 2 * (1024 * 1024);
  } else {
    NOTREACHED() << "Unsupported type: " << type;
    // Choose an arbitrary size.
    options.capacity_num_bytes = 512 * 1024;
  }

  return base::WrapUnique(new mojo::DataPipe(options));
}

}  // namespace

// MojoDecoderBufferReader

// static
std::unique_ptr<MojoDecoderBufferReader> MojoDecoderBufferReader::Create(
    DemuxerStream::Type type,
    mojo::ScopedDataPipeProducerHandle* producer_handle) {
  DVLOG(1) << __FUNCTION__;
  std::unique_ptr<mojo::DataPipe> data_pipe = CreateDataPipe(type);
  *producer_handle = std::move(data_pipe->producer_handle);
  return base::WrapUnique(
      new MojoDecoderBufferReader(std::move(data_pipe->consumer_handle)));
}

MojoDecoderBufferReader::MojoDecoderBufferReader(
    mojo::ScopedDataPipeConsumerHandle consumer_handle)
    : consumer_handle_(std::move(consumer_handle)) {
  DVLOG(1) << __FUNCTION__;
}

MojoDecoderBufferReader::~MojoDecoderBufferReader() {
  DVLOG(1) << __FUNCTION__;
}

scoped_refptr<DecoderBuffer> MojoDecoderBufferReader::ReadDecoderBuffer(
    const mojom::DecoderBufferPtr& buffer) {
  DVLOG(3) << __FUNCTION__;
  scoped_refptr<DecoderBuffer> media_buffer(
      buffer.To<scoped_refptr<DecoderBuffer>>());
  DCHECK(media_buffer);

  if (media_buffer->end_of_stream())
    return media_buffer;

  // Wait for the data to become available in the DataPipe.
  // TODO(sandersd): Do not wait indefinitely.
  MojoHandleSignalsState state;
  MojoResult result =
      MojoWait(consumer_handle_.get().value(), MOJO_HANDLE_SIGNAL_READABLE,
               MOJO_DEADLINE_INDEFINITE, &state);

  if (result != MOJO_RESULT_OK) {
    DVLOG(1) << __FUNCTION__ << ": Peer closed the data pipe";
    return nullptr;
  }

  // Read the inner data for the DecoderBuffer from our DataPipe.
  uint32_t data_size = static_cast<uint32_t>(media_buffer->data_size());
  DCHECK_EQ(data_size, buffer->data_size);
  DCHECK_GT(data_size, 0u);

  uint32_t bytes_read = data_size;
  result = ReadDataRaw(consumer_handle_.get(), media_buffer->writable_data(),
                       &bytes_read, MOJO_READ_DATA_FLAG_ALL_OR_NONE);
  if (result != MOJO_RESULT_OK || bytes_read != data_size) {
    DVLOG(1) << __FUNCTION__ << ": reading from pipe failed";
    return nullptr;
  }

  return media_buffer;
}

// MojoDecoderBufferWriter

// static
std::unique_ptr<MojoDecoderBufferWriter> MojoDecoderBufferWriter::Create(
    DemuxerStream::Type type,
    mojo::ScopedDataPipeConsumerHandle* consumer_handle) {
  DVLOG(1) << __FUNCTION__;
  std::unique_ptr<mojo::DataPipe> data_pipe = CreateDataPipe(type);
  *consumer_handle = std::move(data_pipe->consumer_handle);
  return base::WrapUnique(
      new MojoDecoderBufferWriter(std::move(data_pipe->producer_handle)));
}

MojoDecoderBufferWriter::MojoDecoderBufferWriter(
    mojo::ScopedDataPipeProducerHandle producer_handle)
    : producer_handle_(std::move(producer_handle)) {
  DVLOG(1) << __FUNCTION__;
}

MojoDecoderBufferWriter::~MojoDecoderBufferWriter() {
  DVLOG(1) << __FUNCTION__;
}

mojom::DecoderBufferPtr MojoDecoderBufferWriter::WriteDecoderBuffer(
    const scoped_refptr<DecoderBuffer>& media_buffer) {
  DVLOG(3) << __FUNCTION__;
  mojom::DecoderBufferPtr buffer = mojom::DecoderBuffer::From(media_buffer);

  if (media_buffer->end_of_stream())
    return buffer;

  // Serialize the data section of the DecoderBuffer into our pipe.
  uint32_t num_bytes = base::checked_cast<uint32_t>(media_buffer->data_size());
  DCHECK_GT(num_bytes, 0u);
  MojoResult result =
      WriteDataRaw(producer_handle_.get(), media_buffer->data(), &num_bytes,
                   MOJO_WRITE_DATA_FLAG_ALL_OR_NONE);
  if (result != MOJO_RESULT_OK || num_bytes != media_buffer->data_size()) {
    DVLOG(1) << __FUNCTION__ << ": writing to data pipe failed";
    return nullptr;
  }

  return buffer;
}

}  // namespace media
