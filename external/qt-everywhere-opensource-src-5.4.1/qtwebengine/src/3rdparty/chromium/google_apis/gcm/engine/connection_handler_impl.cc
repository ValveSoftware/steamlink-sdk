// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/connection_handler_impl.h"

#include "base/message_loop/message_loop.h"
#include "google/protobuf/io/coded_stream.h"
#include "google_apis/gcm/base/mcs_util.h"
#include "google_apis/gcm/base/socket_stream.h"
#include "google_apis/gcm/protocol/mcs.pb.h"
#include "net/base/net_errors.h"
#include "net/socket/stream_socket.h"

using namespace google::protobuf::io;

namespace gcm {

namespace {

// # of bytes a MCS version packet consumes.
const int kVersionPacketLen = 1;
// # of bytes a tag packet consumes.
const int kTagPacketLen = 1;
// Max # of bytes a length packet consumes.
const int kSizePacketLenMin = 1;
const int kSizePacketLenMax = 2;

// The current MCS protocol version.
const int kMCSVersion = 41;

}  // namespace

ConnectionHandlerImpl::ConnectionHandlerImpl(
    base::TimeDelta read_timeout,
    const ProtoReceivedCallback& read_callback,
    const ProtoSentCallback& write_callback,
    const ConnectionChangedCallback& connection_callback)
    : read_timeout_(read_timeout),
      socket_(NULL),
      handshake_complete_(false),
      message_tag_(0),
      message_size_(0),
      read_callback_(read_callback),
      write_callback_(write_callback),
      connection_callback_(connection_callback),
      weak_ptr_factory_(this) {
}

ConnectionHandlerImpl::~ConnectionHandlerImpl() {
}

void ConnectionHandlerImpl::Init(
    const mcs_proto::LoginRequest& login_request,
    net::StreamSocket* socket) {
  DCHECK(!read_callback_.is_null());
  DCHECK(!write_callback_.is_null());
  DCHECK(!connection_callback_.is_null());

  // Invalidate any previously outstanding reads.
  weak_ptr_factory_.InvalidateWeakPtrs();

  handshake_complete_ = false;
  message_tag_ = 0;
  message_size_ = 0;
  socket_ = socket;
  input_stream_.reset(new SocketInputStream(socket_));
  output_stream_.reset(new SocketOutputStream(socket_));

  Login(login_request);
}

void ConnectionHandlerImpl::Reset() {
  CloseConnection();
}

bool ConnectionHandlerImpl::CanSendMessage() const {
  return handshake_complete_ && output_stream_.get() &&
      output_stream_->GetState() == SocketOutputStream::EMPTY;
}

void ConnectionHandlerImpl::SendMessage(
    const google::protobuf::MessageLite& message) {
  DCHECK_EQ(output_stream_->GetState(), SocketOutputStream::EMPTY);
  DCHECK(handshake_complete_);

  {
    CodedOutputStream coded_output_stream(output_stream_.get());
    DVLOG(1) << "Writing proto of size " << message.ByteSize();
    int tag = GetMCSProtoTag(message);
    DCHECK_NE(tag, -1);
    coded_output_stream.WriteRaw(&tag, 1);
    coded_output_stream.WriteVarint32(message.ByteSize());
    message.SerializeToCodedStream(&coded_output_stream);
  }

  if (output_stream_->Flush(
          base::Bind(&ConnectionHandlerImpl::OnMessageSent,
                     weak_ptr_factory_.GetWeakPtr())) != net::ERR_IO_PENDING) {
    OnMessageSent();
  }
}

void ConnectionHandlerImpl::Login(
    const google::protobuf::MessageLite& login_request) {
  DCHECK_EQ(output_stream_->GetState(), SocketOutputStream::EMPTY);

  const char version_byte[1] = {kMCSVersion};
  const char login_request_tag[1] = {kLoginRequestTag};
  {
    CodedOutputStream coded_output_stream(output_stream_.get());
    coded_output_stream.WriteRaw(version_byte, 1);
    coded_output_stream.WriteRaw(login_request_tag, 1);
    coded_output_stream.WriteVarint32(login_request.ByteSize());
    login_request.SerializeToCodedStream(&coded_output_stream);
  }

  if (output_stream_->Flush(
          base::Bind(&ConnectionHandlerImpl::OnMessageSent,
                     weak_ptr_factory_.GetWeakPtr())) != net::ERR_IO_PENDING) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&ConnectionHandlerImpl::OnMessageSent,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  read_timeout_timer_.Start(FROM_HERE,
                            read_timeout_,
                            base::Bind(&ConnectionHandlerImpl::OnTimeout,
                                       weak_ptr_factory_.GetWeakPtr()));
  WaitForData(MCS_VERSION_TAG_AND_SIZE);
}

void ConnectionHandlerImpl::OnMessageSent() {
  if (!output_stream_.get()) {
    // The connection has already been closed. Just return.
    DCHECK(!input_stream_.get());
    DCHECK(!read_timeout_timer_.IsRunning());
    return;
  }

  if (output_stream_->GetState() != SocketOutputStream::EMPTY) {
    int last_error = output_stream_->last_error();
    CloseConnection();
    // If the socket stream had an error, plumb it up, else plumb up FAILED.
    if (last_error == net::OK)
      last_error = net::ERR_FAILED;
    connection_callback_.Run(last_error);
    return;
  }

  write_callback_.Run();
}

void ConnectionHandlerImpl::GetNextMessage() {
  DCHECK(SocketInputStream::EMPTY == input_stream_->GetState() ||
         SocketInputStream::READY == input_stream_->GetState());
  message_tag_ = 0;
  message_size_ = 0;

  WaitForData(MCS_TAG_AND_SIZE);
}

void ConnectionHandlerImpl::WaitForData(ProcessingState state) {
  DVLOG(1) << "Waiting for MCS data: state == " << state;

  if (!input_stream_) {
    // The connection has already been closed. Just return.
    DCHECK(!output_stream_.get());
    DCHECK(!read_timeout_timer_.IsRunning());
    return;
  }

  if (input_stream_->GetState() != SocketInputStream::EMPTY &&
      input_stream_->GetState() != SocketInputStream::READY) {
    // An error occurred.
    int last_error = output_stream_->last_error();
    CloseConnection();
    // If the socket stream had an error, plumb it up, else plumb up FAILED.
    if (last_error == net::OK)
      last_error = net::ERR_FAILED;
    connection_callback_.Run(last_error);
    return;
  }

  // Used to determine whether a Socket::Read is necessary.
  size_t min_bytes_needed = 0;
  // Used to limit the size of the Socket::Read.
  size_t max_bytes_needed = 0;

  switch(state) {
    case MCS_VERSION_TAG_AND_SIZE:
      min_bytes_needed = kVersionPacketLen + kTagPacketLen + kSizePacketLenMin;
      max_bytes_needed = kVersionPacketLen + kTagPacketLen + kSizePacketLenMax;
      break;
    case MCS_TAG_AND_SIZE:
      min_bytes_needed = kTagPacketLen + kSizePacketLenMin;
      max_bytes_needed = kTagPacketLen + kSizePacketLenMax;
      break;
    case MCS_FULL_SIZE:
      // If in this state, the minimum size packet length must already have been
      // insufficient, so set both to the max length.
      min_bytes_needed = kSizePacketLenMax;
      max_bytes_needed = kSizePacketLenMax;
      break;
    case MCS_PROTO_BYTES:
      read_timeout_timer_.Reset();
      // No variability in the message size, set both to the same.
      min_bytes_needed = message_size_;
      max_bytes_needed = message_size_;
      break;
    default:
      NOTREACHED();
  }
  DCHECK_GE(max_bytes_needed, min_bytes_needed);

  size_t unread_byte_count = input_stream_->UnreadByteCount();
  if (min_bytes_needed > unread_byte_count &&
      input_stream_->Refresh(
          base::Bind(&ConnectionHandlerImpl::WaitForData,
                     weak_ptr_factory_.GetWeakPtr(),
                     state),
          max_bytes_needed - unread_byte_count) == net::ERR_IO_PENDING) {
    return;
  }

  // Check for refresh errors.
  if (input_stream_->GetState() != SocketInputStream::READY) {
    // An error occurred.
    int last_error = input_stream_->last_error();
    CloseConnection();
    // If the socket stream had an error, plumb it up, else plumb up FAILED.
    if (last_error == net::OK)
      last_error = net::ERR_FAILED;
    connection_callback_.Run(last_error);
    return;
  }

  // Check whether read is complete, or needs to be continued (
  // SocketInputStream::Refresh can finish without reading all the data).
  if (input_stream_->UnreadByteCount() < min_bytes_needed) {
    DVLOG(1) << "Socket read finished prematurely. Waiting for "
             << min_bytes_needed - input_stream_->UnreadByteCount()
             << " more bytes.";
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&ConnectionHandlerImpl::WaitForData,
                   weak_ptr_factory_.GetWeakPtr(),
                   MCS_PROTO_BYTES));
    return;
  }

  // Received enough bytes, process them.
  DVLOG(1) << "Processing MCS data: state == " << state;
  switch(state) {
    case MCS_VERSION_TAG_AND_SIZE:
      OnGotVersion();
      break;
    case MCS_TAG_AND_SIZE:
      OnGotMessageTag();
      break;
    case MCS_FULL_SIZE:
      OnGotMessageSize();
      break;
    case MCS_PROTO_BYTES:
      OnGotMessageBytes();
      break;
    default:
      NOTREACHED();
  }
}

void ConnectionHandlerImpl::OnGotVersion() {
  uint8 version = 0;
  {
    CodedInputStream coded_input_stream(input_stream_.get());
    coded_input_stream.ReadRaw(&version, 1);
  }
  // TODO(zea): remove this when the server is ready.
  if (version < kMCSVersion && version != 38) {
    LOG(ERROR) << "Invalid GCM version response: " << static_cast<int>(version);
    connection_callback_.Run(net::ERR_FAILED);
    return;
  }

  input_stream_->RebuildBuffer();

  // Process the LoginResponse message tag.
  OnGotMessageTag();
}

void ConnectionHandlerImpl::OnGotMessageTag() {
  if (input_stream_->GetState() != SocketInputStream::READY) {
    LOG(ERROR) << "Failed to receive protobuf tag.";
    read_callback_.Run(scoped_ptr<google::protobuf::MessageLite>());
    return;
  }

  {
    CodedInputStream coded_input_stream(input_stream_.get());
    coded_input_stream.ReadRaw(&message_tag_, 1);
  }

  DVLOG(1) << "Received proto of type "
           << static_cast<unsigned int>(message_tag_);

  if (!read_timeout_timer_.IsRunning()) {
    read_timeout_timer_.Start(FROM_HERE,
                              read_timeout_,
                              base::Bind(&ConnectionHandlerImpl::OnTimeout,
                                         weak_ptr_factory_.GetWeakPtr()));
  }
  OnGotMessageSize();
}

void ConnectionHandlerImpl::OnGotMessageSize() {
  if (input_stream_->GetState() != SocketInputStream::READY) {
    LOG(ERROR) << "Failed to receive message size.";
    read_callback_.Run(scoped_ptr<google::protobuf::MessageLite>());
    return;
  }

  bool need_another_byte = false;
  int prev_byte_count = input_stream_->ByteCount();
  {
    CodedInputStream coded_input_stream(input_stream_.get());
    if (!coded_input_stream.ReadVarint32(&message_size_))
      need_another_byte = true;
  }

  if (need_another_byte) {
    DVLOG(1) << "Expecting another message size byte.";
    if (prev_byte_count >= kSizePacketLenMax) {
      // Already had enough bytes, something else went wrong.
      LOG(ERROR) << "Failed to process message size.";
      read_callback_.Run(scoped_ptr<google::protobuf::MessageLite>());
      return;
    }
    // Back up by the amount read (should always be 1 byte).
    int bytes_read = prev_byte_count - input_stream_->ByteCount();
    DCHECK_EQ(bytes_read, 1);
    input_stream_->BackUp(bytes_read);
    WaitForData(MCS_FULL_SIZE);
    return;
  }

  DVLOG(1) << "Proto size: " << message_size_;

  if (message_size_ > 0)
    WaitForData(MCS_PROTO_BYTES);
  else
    OnGotMessageBytes();
}

void ConnectionHandlerImpl::OnGotMessageBytes() {
  read_timeout_timer_.Stop();
  scoped_ptr<google::protobuf::MessageLite> protobuf(
      BuildProtobufFromTag(message_tag_));
  // Messages with no content are valid; just use the default protobuf for
  // that tag.
  if (protobuf.get() && message_size_ == 0) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&ConnectionHandlerImpl::GetNextMessage,
                   weak_ptr_factory_.GetWeakPtr()));
    read_callback_.Run(protobuf.Pass());
    return;
  }

  if (!protobuf.get() ||
      input_stream_->GetState() != SocketInputStream::READY) {
    LOG(ERROR) << "Failed to extract protobuf bytes of type "
               << static_cast<unsigned int>(message_tag_);
    // Reset the connection.
    connection_callback_.Run(net::ERR_FAILED);
    return;
  }

  {
    CodedInputStream coded_input_stream(input_stream_.get());
    if (!protobuf->ParsePartialFromCodedStream(&coded_input_stream)) {
      LOG(ERROR) << "Unable to parse GCM message of type "
                 << static_cast<unsigned int>(message_tag_);
      // Reset the connection.
      connection_callback_.Run(net::ERR_FAILED);
      return;
    }
  }

  input_stream_->RebuildBuffer();
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&ConnectionHandlerImpl::GetNextMessage,
                 weak_ptr_factory_.GetWeakPtr()));
  if (message_tag_ == kLoginResponseTag) {
    if (handshake_complete_) {
      LOG(ERROR) << "Unexpected login response.";
    } else {
      handshake_complete_ = true;
      DVLOG(1) << "GCM Handshake complete.";
      connection_callback_.Run(net::OK);
    }
  }
  read_callback_.Run(protobuf.Pass());
}

void ConnectionHandlerImpl::OnTimeout() {
  LOG(ERROR) << "Timed out waiting for GCM Protocol buffer.";
  CloseConnection();
  connection_callback_.Run(net::ERR_TIMED_OUT);
}

void ConnectionHandlerImpl::CloseConnection() {
  DVLOG(1) << "Closing connection.";
  read_timeout_timer_.Stop();
  if (socket_)
    socket_->Disconnect();
  socket_ = NULL;
  handshake_complete_ = false;
  message_tag_ = 0;
  message_size_ = 0;
  input_stream_.reset();
  output_stream_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

}  // namespace gcm
