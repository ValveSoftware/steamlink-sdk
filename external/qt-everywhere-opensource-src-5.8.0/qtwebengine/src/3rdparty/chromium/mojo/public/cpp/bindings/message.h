// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_MESSAGE_H_
#define MOJO_PUBLIC_CPP_BINDINGS_MESSAGE_H_

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "base/logging.h"
#include "mojo/public/cpp/bindings/lib/message_buffer.h"
#include "mojo/public/cpp/bindings/lib/message_internal.h"
#include "mojo/public/cpp/system/message.h"

namespace mojo {

// Message is a holder for the data and handles to be sent over a MessagePipe.
// Message owns its data and handles, but a consumer of Message is free to
// mutate the data and handles. The message's data is comprised of a header
// followed by payload.
class Message {
 public:
  static const uint32_t kFlagExpectsResponse = 1 << 0;
  static const uint32_t kFlagIsResponse = 1 << 1;
  static const uint32_t kFlagIsSync = 1 << 2;

  Message();
  ~Message();

  // Initializes a Message with enough space for |capacity| bytes.
  void Initialize(size_t capacity, bool zero_initialized);

  // Initializes a Message from an existing Mojo MessageHandle.
  void InitializeFromMojoMessage(ScopedMessageHandle message,
                                 uint32_t num_bytes,
                                 std::vector<Handle>* handles);

  // Transfers data and handles to |destination|.
  void MoveTo(Message* destination);

  uint32_t data_num_bytes() const { return buffer_->data_num_bytes(); }

  // Access the raw bytes of the message.
  const uint8_t* data() const {
    return static_cast<const uint8_t*>(buffer_->data());
  }

  uint8_t* mutable_data() { return static_cast<uint8_t*>(buffer_->data()); }

  // Access the header.
  const internal::MessageHeader* header() const {
    return static_cast<const internal::MessageHeader*>(buffer_->data());
  }

  internal::MessageHeader* header() {
    return const_cast<internal::MessageHeader*>(
        static_cast<const Message*>(this)->header());
  }

  uint32_t interface_id() const { return header()->interface_id; }
  void set_interface_id(uint32_t id) { header()->interface_id = id; }

  uint32_t name() const { return header()->name; }
  bool has_flag(uint32_t flag) const { return !!(header()->flags & flag); }

  // Access the request_id field (if present).
  bool has_request_id() const { return header()->version >= 1; }
  uint64_t request_id() const {
    DCHECK(has_request_id());
    return static_cast<const internal::MessageHeaderWithRequestID*>(
               header())->request_id;
  }
  void set_request_id(uint64_t request_id) {
    DCHECK(has_request_id());
    static_cast<internal::MessageHeaderWithRequestID*>(header())
        ->request_id = request_id;
  }

  // Access the payload.
  const uint8_t* payload() const { return data() + header()->num_bytes; }
  uint8_t* mutable_payload() { return const_cast<uint8_t*>(payload()); }
  uint32_t payload_num_bytes() const {
    DCHECK(buffer_->data_num_bytes() >= header()->num_bytes);
    size_t num_bytes = buffer_->data_num_bytes() - header()->num_bytes;
    DCHECK(num_bytes <= std::numeric_limits<uint32_t>::max());
    return static_cast<uint32_t>(num_bytes);
  }

  // Access the handles.
  const std::vector<Handle>* handles() const { return &handles_; }
  std::vector<Handle>* mutable_handles() { return &handles_; }

  // Access the underlying Buffer interface.
  internal::Buffer* buffer() { return buffer_.get(); }

  // Takes a scoped MessageHandle which may be passed to |WriteMessageNew()| for
  // transmission. Note that this invalidates this Message object, taking
  // ownership of its internal storage and any attached handles.
  ScopedMessageHandle TakeMojoMessage();

  // Notifies the system that this message is "bad," in this case meaning it was
  // rejected by bindings validation code.
  void NotifyBadMessage(const std::string& error);

 private:
  void CloseHandles();

  std::unique_ptr<internal::MessageBuffer> buffer_;
  std::vector<Handle> handles_;

  DISALLOW_COPY_AND_ASSIGN(Message);
};

class MessageReceiver {
 public:
  virtual ~MessageReceiver() {}

  // The receiver may mutate the given message.  Returns true if the message
  // was accepted and false otherwise, indicating that the message was invalid
  // or malformed.
  virtual bool Accept(Message* message) WARN_UNUSED_RESULT = 0;
};

class MessageReceiverWithResponder : public MessageReceiver {
 public:
  ~MessageReceiverWithResponder() override {}

  // A variant on Accept that registers a MessageReceiver (known as the
  // responder) to handle the response message generated from the given
  // message. The responder's Accept method may be called during
  // AcceptWithResponder or some time after its return.
  //
  // NOTE: Upon returning true, AcceptWithResponder assumes ownership of
  // |responder| and will delete it after calling |responder->Accept| or upon
  // its own destruction.
  //
  // TODO(yzshen): consider changing |responder| to
  // std::unique_ptr<MessageReceiver>.
  virtual bool AcceptWithResponder(Message* message, MessageReceiver* responder)
      WARN_UNUSED_RESULT = 0;
};

// A MessageReceiver that is also able to provide status about the state
// of the underlying MessagePipe to which it will be forwarding messages
// received via the |Accept()| call.
class MessageReceiverWithStatus : public MessageReceiver {
 public:
  ~MessageReceiverWithStatus() override {}

  // Returns |true| if this MessageReceiver is currently bound to a MessagePipe,
  // the pipe has not been closed, and the pipe has not encountered an error.
  virtual bool IsValid() = 0;

  // DCHECKs if this MessageReceiver is currently bound to a MessagePipe, the
  // pipe has not been closed, and the pipe has not encountered an error.
  // This function may be called on any thread.
  virtual void DCheckInvalid(const std::string& message) = 0;
};

// An alternative to MessageReceiverWithResponder for cases in which it
// is necessary for the implementor of this interface to know about the status
// of the MessagePipe which will carry the responses.
class MessageReceiverWithResponderStatus : public MessageReceiver {
 public:
  ~MessageReceiverWithResponderStatus() override {}

  // A variant on Accept that registers a MessageReceiverWithStatus (known as
  // the responder) to handle the response message generated from the given
  // message. Any of the responder's methods (Accept or IsValid) may be called
  // during  AcceptWithResponder or some time after its return.
  //
  // NOTE: Upon returning true, AcceptWithResponder assumes ownership of
  // |responder| and will delete it after calling |responder->Accept| or upon
  // its own destruction.
  //
  // TODO(yzshen): consider changing |responder| to
  // std::unique_ptr<MessageReceiver>.
  virtual bool AcceptWithResponder(Message* message,
                                   MessageReceiverWithStatus* responder)
      WARN_UNUSED_RESULT = 0;
};

// Read a single message from the pipe. The caller should have created the
// Message, but not called Initialize(). Returns MOJO_RESULT_SHOULD_WAIT if
// the caller should wait on the handle to become readable. Returns
// MOJO_RESULT_OK if the message was read successfully and should be
// dispatched, otherwise returns an error code if something went wrong.
//
// NOTE: The message hasn't been validated and may be malformed!
MojoResult ReadMessage(MessagePipeHandle handle, Message* message);

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_MESSAGE_H_
