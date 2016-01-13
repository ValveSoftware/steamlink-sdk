// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_deflate_stream.h"

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/websockets/websocket_deflate_predictor.h"
#include "net/websockets/websocket_deflater.h"
#include "net/websockets/websocket_errors.h"
#include "net/websockets/websocket_frame.h"
#include "net/websockets/websocket_inflater.h"
#include "net/websockets/websocket_stream.h"

class GURL;

namespace net {

namespace {

const int kWindowBits = 15;
const size_t kChunkSize = 4 * 1024;

}  // namespace

WebSocketDeflateStream::WebSocketDeflateStream(
    scoped_ptr<WebSocketStream> stream,
    WebSocketDeflater::ContextTakeOverMode mode,
    int client_window_bits,
    scoped_ptr<WebSocketDeflatePredictor> predictor)
    : stream_(stream.Pass()),
      deflater_(mode),
      inflater_(kChunkSize, kChunkSize),
      reading_state_(NOT_READING),
      writing_state_(NOT_WRITING),
      current_reading_opcode_(WebSocketFrameHeader::kOpCodeText),
      current_writing_opcode_(WebSocketFrameHeader::kOpCodeText),
      predictor_(predictor.Pass()) {
  DCHECK(stream_);
  DCHECK_GE(client_window_bits, 8);
  DCHECK_LE(client_window_bits, 15);
  deflater_.Initialize(client_window_bits);
  inflater_.Initialize(kWindowBits);
}

WebSocketDeflateStream::~WebSocketDeflateStream() {}

int WebSocketDeflateStream::ReadFrames(ScopedVector<WebSocketFrame>* frames,
                                       const CompletionCallback& callback) {
  int result = stream_->ReadFrames(
      frames,
      base::Bind(&WebSocketDeflateStream::OnReadComplete,
                 base::Unretained(this),
                 base::Unretained(frames),
                 callback));
  if (result < 0)
    return result;
  DCHECK_EQ(OK, result);
  DCHECK(!frames->empty());

  return InflateAndReadIfNecessary(frames, callback);
}

int WebSocketDeflateStream::WriteFrames(ScopedVector<WebSocketFrame>* frames,
                                        const CompletionCallback& callback) {
  int result = Deflate(frames);
  if (result != OK)
    return result;
  if (frames->empty())
    return OK;
  return stream_->WriteFrames(frames, callback);
}

void WebSocketDeflateStream::Close() { stream_->Close(); }

std::string WebSocketDeflateStream::GetSubProtocol() const {
  return stream_->GetSubProtocol();
}

std::string WebSocketDeflateStream::GetExtensions() const {
  return stream_->GetExtensions();
}

void WebSocketDeflateStream::OnReadComplete(
    ScopedVector<WebSocketFrame>* frames,
    const CompletionCallback& callback,
    int result) {
  if (result != OK) {
    frames->clear();
    callback.Run(result);
    return;
  }

  int r = InflateAndReadIfNecessary(frames, callback);
  if (r != ERR_IO_PENDING)
    callback.Run(r);
}

int WebSocketDeflateStream::Deflate(ScopedVector<WebSocketFrame>* frames) {
  ScopedVector<WebSocketFrame> frames_to_write;
  // Store frames of the currently processed message if writing_state_ equals to
  // WRITING_POSSIBLY_COMPRESSED_MESSAGE.
  ScopedVector<WebSocketFrame> frames_of_message;
  for (size_t i = 0; i < frames->size(); ++i) {
    DCHECK(!(*frames)[i]->header.reserved1);
    if (!WebSocketFrameHeader::IsKnownDataOpCode((*frames)[i]->header.opcode)) {
      frames_to_write.push_back((*frames)[i]);
      (*frames)[i] = NULL;
      continue;
    }
    if (writing_state_ == NOT_WRITING)
      OnMessageStart(*frames, i);

    scoped_ptr<WebSocketFrame> frame((*frames)[i]);
    (*frames)[i] = NULL;
    predictor_->RecordInputDataFrame(frame.get());

    if (writing_state_ == WRITING_UNCOMPRESSED_MESSAGE) {
      if (frame->header.final)
        writing_state_ = NOT_WRITING;
      predictor_->RecordWrittenDataFrame(frame.get());
      frames_to_write.push_back(frame.release());
      current_writing_opcode_ = WebSocketFrameHeader::kOpCodeContinuation;
    } else {
      if (frame->data && !deflater_.AddBytes(frame->data->data(),
                                             frame->header.payload_length)) {
        DVLOG(1) << "WebSocket protocol error. "
                 << "deflater_.AddBytes() returns an error.";
        return ERR_WS_PROTOCOL_ERROR;
      }
      if (frame->header.final && !deflater_.Finish()) {
        DVLOG(1) << "WebSocket protocol error. "
                 << "deflater_.Finish() returns an error.";
        return ERR_WS_PROTOCOL_ERROR;
      }

      if (writing_state_ == WRITING_COMPRESSED_MESSAGE) {
        if (deflater_.CurrentOutputSize() >= kChunkSize ||
            frame->header.final) {
          int result = AppendCompressedFrame(frame->header, &frames_to_write);
          if (result != OK)
            return result;
        }
        if (frame->header.final)
          writing_state_ = NOT_WRITING;
      } else {
        DCHECK_EQ(WRITING_POSSIBLY_COMPRESSED_MESSAGE, writing_state_);
        bool final = frame->header.final;
        frames_of_message.push_back(frame.release());
        if (final) {
          int result = AppendPossiblyCompressedMessage(&frames_of_message,
                                                       &frames_to_write);
          if (result != OK)
            return result;
          frames_of_message.clear();
          writing_state_ = NOT_WRITING;
        }
      }
    }
  }
  DCHECK_NE(WRITING_POSSIBLY_COMPRESSED_MESSAGE, writing_state_);
  frames->swap(frames_to_write);
  return OK;
}

void WebSocketDeflateStream::OnMessageStart(
    const ScopedVector<WebSocketFrame>& frames, size_t index) {
  WebSocketFrame* frame = frames[index];
  current_writing_opcode_ = frame->header.opcode;
  DCHECK(current_writing_opcode_ == WebSocketFrameHeader::kOpCodeText ||
         current_writing_opcode_ == WebSocketFrameHeader::kOpCodeBinary);
  WebSocketDeflatePredictor::Result prediction =
      predictor_->Predict(frames, index);

  switch (prediction) {
    case WebSocketDeflatePredictor::DEFLATE:
      writing_state_ = WRITING_COMPRESSED_MESSAGE;
      return;
    case WebSocketDeflatePredictor::DO_NOT_DEFLATE:
      writing_state_ = WRITING_UNCOMPRESSED_MESSAGE;
      return;
    case WebSocketDeflatePredictor::TRY_DEFLATE:
      writing_state_ = WRITING_POSSIBLY_COMPRESSED_MESSAGE;
      return;
  }
  NOTREACHED();
}

int WebSocketDeflateStream::AppendCompressedFrame(
    const WebSocketFrameHeader& header,
    ScopedVector<WebSocketFrame>* frames_to_write) {
  const WebSocketFrameHeader::OpCode opcode = current_writing_opcode_;
  scoped_refptr<IOBufferWithSize> compressed_payload =
      deflater_.GetOutput(deflater_.CurrentOutputSize());
  if (!compressed_payload) {
    DVLOG(1) << "WebSocket protocol error. "
             << "deflater_.GetOutput() returns an error.";
    return ERR_WS_PROTOCOL_ERROR;
  }
  scoped_ptr<WebSocketFrame> compressed(new WebSocketFrame(opcode));
  compressed->header.CopyFrom(header);
  compressed->header.opcode = opcode;
  compressed->header.final = header.final;
  compressed->header.reserved1 =
      (opcode != WebSocketFrameHeader::kOpCodeContinuation);
  compressed->data = compressed_payload;
  compressed->header.payload_length = compressed_payload->size();

  current_writing_opcode_ = WebSocketFrameHeader::kOpCodeContinuation;
  predictor_->RecordWrittenDataFrame(compressed.get());
  frames_to_write->push_back(compressed.release());
  return OK;
}

int WebSocketDeflateStream::AppendPossiblyCompressedMessage(
    ScopedVector<WebSocketFrame>* frames,
    ScopedVector<WebSocketFrame>* frames_to_write) {
  DCHECK(!frames->empty());

  const WebSocketFrameHeader::OpCode opcode = current_writing_opcode_;
  scoped_refptr<IOBufferWithSize> compressed_payload =
      deflater_.GetOutput(deflater_.CurrentOutputSize());
  if (!compressed_payload) {
    DVLOG(1) << "WebSocket protocol error. "
             << "deflater_.GetOutput() returns an error.";
    return ERR_WS_PROTOCOL_ERROR;
  }

  uint64 original_payload_length = 0;
  for (size_t i = 0; i < frames->size(); ++i) {
    WebSocketFrame* frame = (*frames)[i];
    // Asserts checking that frames represent one whole data message.
    DCHECK(WebSocketFrameHeader::IsKnownDataOpCode(frame->header.opcode));
    DCHECK_EQ(i == 0,
              WebSocketFrameHeader::kOpCodeContinuation !=
              frame->header.opcode);
    DCHECK_EQ(i == frames->size() - 1, frame->header.final);
    original_payload_length += frame->header.payload_length;
  }
  if (original_payload_length <=
      static_cast<uint64>(compressed_payload->size())) {
    // Compression is not effective. Use the original frames.
    for (size_t i = 0; i < frames->size(); ++i) {
      WebSocketFrame* frame = (*frames)[i];
      frames_to_write->push_back(frame);
      predictor_->RecordWrittenDataFrame(frame);
      (*frames)[i] = NULL;
    }
    frames->weak_clear();
    return OK;
  }
  scoped_ptr<WebSocketFrame> compressed(new WebSocketFrame(opcode));
  compressed->header.CopyFrom((*frames)[0]->header);
  compressed->header.opcode = opcode;
  compressed->header.final = true;
  compressed->header.reserved1 = true;
  compressed->data = compressed_payload;
  compressed->header.payload_length = compressed_payload->size();

  predictor_->RecordWrittenDataFrame(compressed.get());
  frames_to_write->push_back(compressed.release());
  return OK;
}

int WebSocketDeflateStream::Inflate(ScopedVector<WebSocketFrame>* frames) {
  ScopedVector<WebSocketFrame> frames_to_output;
  ScopedVector<WebSocketFrame> frames_passed;
  frames->swap(frames_passed);
  for (size_t i = 0; i < frames_passed.size(); ++i) {
    scoped_ptr<WebSocketFrame> frame(frames_passed[i]);
    frames_passed[i] = NULL;
    DVLOG(3) << "Input frame: opcode=" << frame->header.opcode
             << " final=" << frame->header.final
             << " reserved1=" << frame->header.reserved1
             << " payload_length=" << frame->header.payload_length;

    if (!WebSocketFrameHeader::IsKnownDataOpCode(frame->header.opcode)) {
      frames_to_output.push_back(frame.release());
      continue;
    }

    if (reading_state_ == NOT_READING) {
      if (frame->header.reserved1)
        reading_state_ = READING_COMPRESSED_MESSAGE;
      else
        reading_state_ = READING_UNCOMPRESSED_MESSAGE;
      current_reading_opcode_ = frame->header.opcode;
    } else {
      if (frame->header.reserved1) {
        DVLOG(1) << "WebSocket protocol error. "
                 << "Receiving a non-first frame with RSV1 flag set.";
        return ERR_WS_PROTOCOL_ERROR;
      }
    }

    if (reading_state_ == READING_UNCOMPRESSED_MESSAGE) {
      if (frame->header.final)
        reading_state_ = NOT_READING;
      current_reading_opcode_ = WebSocketFrameHeader::kOpCodeContinuation;
      frames_to_output.push_back(frame.release());
    } else {
      DCHECK_EQ(reading_state_, READING_COMPRESSED_MESSAGE);
      if (frame->data && !inflater_.AddBytes(frame->data->data(),
                                             frame->header.payload_length)) {
        DVLOG(1) << "WebSocket protocol error. "
                 << "inflater_.AddBytes() returns an error.";
        return ERR_WS_PROTOCOL_ERROR;
      }
      if (frame->header.final) {
        if (!inflater_.Finish()) {
          DVLOG(1) << "WebSocket protocol error. "
                   << "inflater_.Finish() returns an error.";
          return ERR_WS_PROTOCOL_ERROR;
        }
      }
      // TODO(yhirano): Many frames can be generated by the inflater and
      // memory consumption can grow.
      // We could avoid it, but avoiding it makes this class much more
      // complicated.
      while (inflater_.CurrentOutputSize() >= kChunkSize ||
             frame->header.final) {
        size_t size = std::min(kChunkSize, inflater_.CurrentOutputSize());
        scoped_ptr<WebSocketFrame> inflated(
            new WebSocketFrame(WebSocketFrameHeader::kOpCodeText));
        scoped_refptr<IOBufferWithSize> data = inflater_.GetOutput(size);
        bool is_final = !inflater_.CurrentOutputSize() && frame->header.final;
        if (!data) {
          DVLOG(1) << "WebSocket protocol error. "
                   << "inflater_.GetOutput() returns an error.";
          return ERR_WS_PROTOCOL_ERROR;
        }
        inflated->header.CopyFrom(frame->header);
        inflated->header.opcode = current_reading_opcode_;
        inflated->header.final = is_final;
        inflated->header.reserved1 = false;
        inflated->data = data;
        inflated->header.payload_length = data->size();
        DVLOG(3) << "Inflated frame: opcode=" << inflated->header.opcode
                 << " final=" << inflated->header.final
                 << " reserved1=" << inflated->header.reserved1
                 << " payload_length=" << inflated->header.payload_length;
        frames_to_output.push_back(inflated.release());
        current_reading_opcode_ = WebSocketFrameHeader::kOpCodeContinuation;
        if (is_final)
          break;
      }
      if (frame->header.final)
        reading_state_ = NOT_READING;
    }
  }
  frames->swap(frames_to_output);
  return frames->empty() ? ERR_IO_PENDING : OK;
}

int WebSocketDeflateStream::InflateAndReadIfNecessary(
    ScopedVector<WebSocketFrame>* frames,
    const CompletionCallback& callback) {
  int result = Inflate(frames);
  while (result == ERR_IO_PENDING) {
    DCHECK(frames->empty());

    result = stream_->ReadFrames(
        frames,
        base::Bind(&WebSocketDeflateStream::OnReadComplete,
                   base::Unretained(this),
                   base::Unretained(frames),
                   callback));
    if (result < 0)
      break;
    DCHECK_EQ(OK, result);
    DCHECK(!frames->empty());

    result = Inflate(frames);
  }
  if (result < 0)
    frames->clear();
  return result;
}

}  // namespace net
