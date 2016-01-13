// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GCM_ENGINE_CONNECTION_HANDLER_IMPL_H_
#define GOOGLE_APIS_GCM_ENGINE_CONNECTION_HANDLER_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "google_apis/gcm/engine/connection_handler.h"

namespace mcs_proto {
class LoginRequest;
}  // namespace mcs_proto

namespace gcm {

class GCM_EXPORT ConnectionHandlerImpl : public ConnectionHandler {
 public:
  // |read_callback| will be invoked with the contents of any received protobuf
  // message.
  // |write_callback| will be invoked anytime a message has been successfully
  // sent. Note: this just means the data was sent to the wire, not that the
  // other end received it.
  // |connection_callback| will be invoked with any fatal read/write errors
  // encountered.
  ConnectionHandlerImpl(
      base::TimeDelta read_timeout,
      const ProtoReceivedCallback& read_callback,
      const ProtoSentCallback& write_callback,
      const ConnectionChangedCallback& connection_callback);
  virtual ~ConnectionHandlerImpl();

  // ConnectionHandler implementation.
  virtual void Init(const mcs_proto::LoginRequest& login_request,
                    net::StreamSocket* socket) OVERRIDE;
  virtual void Reset() OVERRIDE;
  virtual bool CanSendMessage() const OVERRIDE;
  virtual void SendMessage(const google::protobuf::MessageLite& message)
      OVERRIDE;

 private:
  // State machine for handling incoming data. See WaitForData(..) for usage.
  enum ProcessingState {
    // Processing the version, tag, and size packets (assuming minimum length
    // size packet). Only used during the login handshake.
    MCS_VERSION_TAG_AND_SIZE = 0,
    // Processing the tag and size packets (assuming minimum length size
    // packet). Used for normal messages.
    MCS_TAG_AND_SIZE,
    // Processing a maximum length size packet (for messages with length > 128).
    // Used when a normal size packet was not sufficient to read the message
    // size.
    MCS_FULL_SIZE,
    // Processing the protocol buffer bytes (for those messages with non-zero
    // sizes).
    MCS_PROTO_BYTES
  };

  // Sends the protocol version and login request. First step in the MCS
  // connection handshake.
  void Login(const google::protobuf::MessageLite& login_request);

  // SendMessage continuation. Invoked when Socket::Write completes.
  void OnMessageSent();

  // Starts the message processing process, which is comprised of the tag,
  // message size, and bytes packet types.
  void GetNextMessage();

  // Performs any necessary SocketInputStream refreshing until the data
  // associated with |packet_type| is fully ready, then calls the appropriate
  // OnGot* message to process the packet data. If the read times out,
  // will close the stream and invoke the connection callback.
  void WaitForData(ProcessingState state);

  // Incoming data helper methods.
  void OnGotVersion();
  void OnGotMessageTag();
  void OnGotMessageSize();
  void OnGotMessageBytes();

  // Timeout handler.
  void OnTimeout();

  // Closes the current connection.
  void CloseConnection();

  // Timeout policy: the timeout is only enforced while waiting on the
  // handshake (version and/or LoginResponse) or once at least a tag packet has
  // been received. It is reset every time new data is received, and is
  // only stopped when a full message is processed.
  // TODO(zea): consider enforcing a separate timeout when waiting for
  // a message to send.
  const base::TimeDelta read_timeout_;
  base::OneShotTimer<ConnectionHandlerImpl> read_timeout_timer_;

  // This connection's socket and the input/output streams attached to it.
  net::StreamSocket* socket_;
  scoped_ptr<SocketInputStream> input_stream_;
  scoped_ptr<SocketOutputStream> output_stream_;

  // Whether the MCS login handshake has successfully completed. See Init(..)
  // description for more info on what the handshake involves.
  bool handshake_complete_;

  // State for the message currently being processed, if there is one.
  uint8 message_tag_;
  uint32 message_size_;

  ProtoReceivedCallback read_callback_;
  ProtoSentCallback write_callback_;
  ConnectionChangedCallback connection_callback_;

  base::WeakPtrFactory<ConnectionHandlerImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ConnectionHandlerImpl);
};

}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_ENGINE_CONNECTION_HANDLER_IMPL_H_
