// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_MESSAGE_H_
#define IPC_IPC_MESSAGE_H_

#include <string>

#include "base/basictypes.h"
#include "base/debug/trace_event.h"
#include "base/pickle.h"
#include "ipc/ipc_export.h"

#if !defined(NDEBUG)
#define IPC_MESSAGE_LOG_ENABLED
#endif

#if defined(OS_POSIX)
#include "base/memory/ref_counted.h"
#endif

namespace base {
struct FileDescriptor;
}

class FileDescriptorSet;

namespace IPC {

//------------------------------------------------------------------------------

struct LogData;

class IPC_EXPORT Message : public Pickle {
 public:
  enum PriorityValue {
    PRIORITY_LOW = 1,
    PRIORITY_NORMAL,
    PRIORITY_HIGH
  };

  // Bit values used in the flags field.
  // Upper 24 bits of flags store a reference number, so this enum is limited to
  // 8 bits.
  enum {
    PRIORITY_MASK     = 0x03,  // Low 2 bits of store the priority value.
    SYNC_BIT          = 0x04,
    REPLY_BIT         = 0x08,
    REPLY_ERROR_BIT   = 0x10,
    UNBLOCK_BIT       = 0x20,
    PUMPING_MSGS_BIT  = 0x40,
    HAS_SENT_TIME_BIT = 0x80,
  };

  virtual ~Message();

  Message();

  // Initialize a message with a user-defined type, priority value, and
  // destination WebView ID.
  Message(int32 routing_id, uint32 type, PriorityValue priority);

  // Initializes a message from a const block of data.  The data is not copied;
  // instead the data is merely referenced by this message.  Only const methods
  // should be used on the message when initialized this way.
  Message(const char* data, int data_len);

  Message(const Message& other);
  Message& operator=(const Message& other);

  PriorityValue priority() const {
    return static_cast<PriorityValue>(header()->flags & PRIORITY_MASK);
  }

  // True if this is a synchronous message.
  void set_sync() {
    header()->flags |= SYNC_BIT;
  }
  bool is_sync() const {
    return (header()->flags & SYNC_BIT) != 0;
  }

  // Set this on a reply to a synchronous message.
  void set_reply() {
    header()->flags |= REPLY_BIT;
  }

  bool is_reply() const {
    return (header()->flags & REPLY_BIT) != 0;
  }

  // Set this on a reply to a synchronous message to indicate that no receiver
  // was found.
  void set_reply_error() {
    header()->flags |= REPLY_ERROR_BIT;
  }

  bool is_reply_error() const {
    return (header()->flags & REPLY_ERROR_BIT) != 0;
  }

  // Normally when a receiver gets a message and they're blocked on a
  // synchronous message Send, they buffer a message.  Setting this flag causes
  // the receiver to be unblocked and the message to be dispatched immediately.
  void set_unblock(bool unblock) {
    if (unblock) {
      header()->flags |= UNBLOCK_BIT;
    } else {
      header()->flags &= ~UNBLOCK_BIT;
    }
  }

  bool should_unblock() const {
    return (header()->flags & UNBLOCK_BIT) != 0;
  }

  // Tells the receiver that the caller is pumping messages while waiting
  // for the result.
  bool is_caller_pumping_messages() const {
    return (header()->flags & PUMPING_MSGS_BIT) != 0;
  }

  void set_dispatch_error() const {
    dispatch_error_ = true;
  }

  bool dispatch_error() const {
    return dispatch_error_;
  }

  uint32 type() const {
    return header()->type;
  }

  int32 routing_id() const {
    return header()->routing;
  }

  void set_routing_id(int32 new_id) {
    header()->routing = new_id;
  }

  uint32 flags() const {
    return header()->flags;
  }

  // Sets all the given header values. The message should be empty at this
  // call.
  void SetHeaderValues(int32 routing, uint32 type, uint32 flags);

  template<class T, class S, class P>
  static bool Dispatch(const Message* msg, T* obj, S* sender, P* parameter,
                       void (T::*func)()) {
    (obj->*func)();
    return true;
  }

  template<class T, class S, class P>
  static bool Dispatch(const Message* msg, T* obj, S* sender, P* parameter,
                       void (T::*func)(P*)) {
    (obj->*func)(parameter);
    return true;
  }

  // Used for async messages with no parameters.
  static void Log(std::string* name, const Message* msg, std::string* l) {
  }

  // Find the end of the message data that starts at range_start.  Returns NULL
  // if the entire message is not found in the given data range.
  static const char* FindNext(const char* range_start, const char* range_end) {
    return Pickle::FindNext(sizeof(Header), range_start, range_end);
  }

#if defined(OS_POSIX)
  // On POSIX, a message supports reading / writing FileDescriptor objects.
  // This is used to pass a file descriptor to the peer of an IPC channel.

  // Add a descriptor to the end of the set. Returns false if the set is full.
  bool WriteFileDescriptor(const base::FileDescriptor& descriptor);

  // Get a file descriptor from the message. Returns false on error.
  //   iter: a Pickle iterator to the current location in the message.
  bool ReadFileDescriptor(PickleIterator* iter,
                          base::FileDescriptor* descriptor) const;

  // Returns true if there are any file descriptors in this message.
  bool HasFileDescriptors() const;
#endif

#ifdef IPC_MESSAGE_LOG_ENABLED
  // Adds the outgoing time from Time::Now() at the end of the message and sets
  // a bit to indicate that it's been added.
  void set_sent_time(int64 time);
  int64 sent_time() const;

  void set_received_time(int64 time) const;
  int64 received_time() const { return received_time_; }
  void set_output_params(const std::string& op) const { output_params_ = op; }
  const std::string& output_params() const { return output_params_; }
  // The following four functions are needed so we can log sync messages with
  // delayed replies.  We stick the log data from the sent message into the
  // reply message, so that when it's sent and we have the output parameters
  // we can log it.  As such, we set a flag on the sent message to not log it.
  void set_sync_log_data(LogData* data) const { log_data_ = data; }
  LogData* sync_log_data() const { return log_data_; }
  void set_dont_log() const { dont_log_ = true; }
  bool dont_log() const { return dont_log_; }
#endif

  // Called to trace when message is sent.
  void TraceMessageBegin() {
    TRACE_EVENT_FLOW_BEGIN0(TRACE_DISABLED_BY_DEFAULT("ipc.flow"), "IPC",
        header()->flags);
  }
  // Called to trace when message is received.
  void TraceMessageEnd() {
    TRACE_EVENT_FLOW_END0(TRACE_DISABLED_BY_DEFAULT("ipc.flow"), "IPC",
        header()->flags);
  }

 protected:
  friend class Channel;
  friend class ChannelNacl;
  friend class ChannelPosix;
  friend class ChannelWin;
  friend class MessageReplyDeserializer;
  friend class SyncMessage;

#pragma pack(push, 4)
  struct Header : Pickle::Header {
    int32 routing;  // ID of the view that this message is destined for
    uint32 type;    // specifies the user-defined message type
    uint32 flags;   // specifies control flags for the message
#if defined(OS_POSIX)
    uint16 num_fds; // the number of descriptors included with this message
    uint16 pad;     // explicitly initialize this to appease valgrind
#endif
  };
#pragma pack(pop)

  Header* header() {
    return headerT<Header>();
  }
  const Header* header() const {
    return headerT<Header>();
  }

  void Init();

  // Used internally to support IPC::Listener::OnBadMessageReceived.
  mutable bool dispatch_error_;

#if defined(OS_POSIX)
  // The set of file descriptors associated with this message.
  scoped_refptr<FileDescriptorSet> file_descriptor_set_;

  // Ensure that a FileDescriptorSet is allocated
  void EnsureFileDescriptorSet();

  FileDescriptorSet* file_descriptor_set() {
    EnsureFileDescriptorSet();
    return file_descriptor_set_.get();
  }
  const FileDescriptorSet* file_descriptor_set() const {
    return file_descriptor_set_.get();
  }
#endif

#ifdef IPC_MESSAGE_LOG_ENABLED
  // Used for logging.
  mutable int64 received_time_;
  mutable std::string output_params_;
  mutable LogData* log_data_;
  mutable bool dont_log_;
#endif
};

//------------------------------------------------------------------------------

}  // namespace IPC

enum SpecialRoutingIDs {
  // indicates that we don't have a routing ID yet.
  MSG_ROUTING_NONE = -2,

  // indicates a general message not sent to a particular tab.
  MSG_ROUTING_CONTROL = kint32max,
};

#define IPC_REPLY_ID 0xFFFFFFF0  // Special message id for replies
#define IPC_LOGGING_ID 0xFFFFFFF1  // Special message id for logging

#endif  // IPC_IPC_MESSAGE_H_
