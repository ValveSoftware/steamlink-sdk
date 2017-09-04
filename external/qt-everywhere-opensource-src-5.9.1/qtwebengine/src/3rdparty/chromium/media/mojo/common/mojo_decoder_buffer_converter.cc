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

  return base::MakeUnique<mojo::DataPipe>(options);
}

bool IsPipeReadWriteError(MojoResult result) {
  return result != MOJO_RESULT_OK && result != MOJO_RESULT_SHOULD_WAIT;
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
    : consumer_handle_(std::move(consumer_handle)), bytes_read_(0) {
  DVLOG(1) << __FUNCTION__;

  MojoResult result =
      pipe_watcher_.Start(consumer_handle_.get(), MOJO_HANDLE_SIGNAL_READABLE,
                          base::Bind(&MojoDecoderBufferReader::OnPipeReadable,
                                     base::Unretained(this)));
  if (result != MOJO_RESULT_OK) {
    DVLOG(1) << __FUNCTION__
             << ": Failed to start watching the pipe. result=" << result;
    consumer_handle_.reset();
  }
}

MojoDecoderBufferReader::~MojoDecoderBufferReader() {
  DVLOG(1) << __FUNCTION__;
}

void MojoDecoderBufferReader::ReadDecoderBuffer(
    mojom::DecoderBufferPtr mojo_buffer,
    ReadCB read_cb) {
  DVLOG(3) << __FUNCTION__;

  // DecoderBuffer cannot be read if the pipe is already closed.
  if (!consumer_handle_.is_valid()) {
    DVLOG(1)
        << __FUNCTION__
        << ": Failed to read DecoderBuffer becuase the pipe is already closed";
    std::move(read_cb).Run(nullptr);
    return;
  }

  DCHECK(!read_cb_);
  DCHECK(!media_buffer_);
  DCHECK_EQ(bytes_read_, 0u);

  scoped_refptr<DecoderBuffer> media_buffer(
      mojo_buffer.To<scoped_refptr<DecoderBuffer>>());
  DCHECK(media_buffer);

  // A non-EOS buffer can have zero size. See http://crbug.com/663438
  if (media_buffer->end_of_stream() || media_buffer->data_size() == 0) {
    std::move(read_cb).Run(media_buffer);
    return;
  }

  // Read the data section of |media_buffer| from the pipe.
  read_cb_ = std::move(read_cb);
  media_buffer_ = std::move(media_buffer);
  ReadDecoderBufferData();
}

void MojoDecoderBufferReader::OnPipeError(MojoResult result) {
  DVLOG(1) << __FUNCTION__ << "(" << result << ")";
  DCHECK(IsPipeReadWriteError(result));

  if (media_buffer_) {
    DVLOG(1) << __FUNCTION__
             << ": reading from data pipe failed. result=" << result
             << ", buffer size=" << media_buffer_->data_size()
             << ", num_bytes(read)=" << bytes_read_;
    DCHECK(read_cb_);
    bytes_read_ = 0;
    media_buffer_ = nullptr;
    std::move(read_cb_).Run(nullptr);
  }
  consumer_handle_.reset();
}

void MojoDecoderBufferReader::OnPipeReadable(MojoResult result) {
  DVLOG(4) << __FUNCTION__ << "(" << result << ")";

  if (result != MOJO_RESULT_OK)
    OnPipeError(result);
  else if (media_buffer_)
    ReadDecoderBufferData();
}

void MojoDecoderBufferReader::ReadDecoderBufferData() {
  DVLOG(4) << __FUNCTION__;
  DCHECK(media_buffer_);

  uint32_t buffer_size =
      base::checked_cast<uint32_t>(media_buffer_->data_size());
  DCHECK_GT(buffer_size, 0u);

  uint32_t num_bytes = buffer_size - bytes_read_;
  DCHECK_GT(num_bytes, 0u);

  MojoResult result = ReadDataRaw(consumer_handle_.get(),
                                  media_buffer_->writable_data() + bytes_read_,
                                  &num_bytes, MOJO_WRITE_DATA_FLAG_NONE);

  if (IsPipeReadWriteError(result)) {
    OnPipeError(result);
  } else if (result == MOJO_RESULT_OK) {
    DCHECK_GT(num_bytes, 0u);
    bytes_read_ += num_bytes;
    if (bytes_read_ == buffer_size) {
      DCHECK(read_cb_);
      bytes_read_ = 0;
      std::move(read_cb_).Run(std::move(media_buffer_));
    }
  }
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
    : producer_handle_(std::move(producer_handle)), bytes_written_(0) {
  DVLOG(1) << __FUNCTION__;

  MojoResult result =
      pipe_watcher_.Start(producer_handle_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
                          base::Bind(&MojoDecoderBufferWriter::OnPipeWritable,
                                     base::Unretained(this)));
  if (result != MOJO_RESULT_OK) {
    DVLOG(1) << __FUNCTION__
             << ": Failed to start watching the pipe. result=" << result;
    producer_handle_.reset();
  }
}

MojoDecoderBufferWriter::~MojoDecoderBufferWriter() {
  DVLOG(1) << __FUNCTION__;
}

mojom::DecoderBufferPtr MojoDecoderBufferWriter::WriteDecoderBuffer(
    const scoped_refptr<DecoderBuffer>& media_buffer) {
  DVLOG(3) << __FUNCTION__;

  // DecoderBuffer cannot be written if the pipe is already closed.
  if (!producer_handle_.is_valid()) {
    DVLOG(1)
        << __FUNCTION__
        << ": Failed to write DecoderBuffer becuase the pipe is already closed";
    return nullptr;
  }

  DCHECK(!media_buffer_);
  DCHECK_EQ(bytes_written_, 0u);

  mojom::DecoderBufferPtr mojo_buffer =
      mojom::DecoderBuffer::From(media_buffer);

  // A non-EOS buffer can have zero size. See http://crbug.com/663438
  if (media_buffer->end_of_stream() || media_buffer->data_size() == 0)
    return mojo_buffer;

  // Serialize the data section of the DecoderBuffer into our pipe.
  media_buffer_ = media_buffer;
  MojoResult result = WriteDecoderBufferData();
  return IsPipeReadWriteError(result) ? nullptr : std::move(mojo_buffer);
}

void MojoDecoderBufferWriter::OnPipeError(MojoResult result) {
  DVLOG(1) << __FUNCTION__ << "(" << result << ")";
  DCHECK(IsPipeReadWriteError(result));

  if (media_buffer_) {
    DVLOG(1) << __FUNCTION__
             << ": writing to data pipe failed. result=" << result
             << ", buffer size=" << media_buffer_->data_size()
             << ", num_bytes(written)=" << bytes_written_;
    media_buffer_ = nullptr;
    bytes_written_ = 0;
  }
  producer_handle_.reset();
}

void MojoDecoderBufferWriter::OnPipeWritable(MojoResult result) {
  DVLOG(4) << __FUNCTION__ << "(" << result << ")";

  if (result != MOJO_RESULT_OK)
    OnPipeError(result);
  else if (media_buffer_)
    WriteDecoderBufferData();
}

MojoResult MojoDecoderBufferWriter::WriteDecoderBufferData() {
  DVLOG(4) << __FUNCTION__;
  DCHECK(media_buffer_);

  uint32_t buffer_size =
      base::checked_cast<uint32_t>(media_buffer_->data_size());
  DCHECK_GT(buffer_size, 0u);

  uint32_t num_bytes = buffer_size - bytes_written_;
  DCHECK_GT(num_bytes, 0u);

  MojoResult result = WriteDataRaw(producer_handle_.get(),
                                   media_buffer_->data() + bytes_written_,
                                   &num_bytes, MOJO_WRITE_DATA_FLAG_NONE);

  if (IsPipeReadWriteError(result)) {
    OnPipeError(result);
  } else if (result == MOJO_RESULT_OK) {
    DCHECK_GT(num_bytes, 0u);
    bytes_written_ += num_bytes;
    if (bytes_written_ == buffer_size) {
      media_buffer_ = nullptr;
      bytes_written_ = 0;
    }
  }

  return result;
}

}  // namespace media
