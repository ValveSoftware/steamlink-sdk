// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/channel.h"

#include <errno.h>
#include <sys/socket.h>

#include <algorithm>
#include <deque>
#include <limits>
#include <memory>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/lock.h"
#include "base/task_runner.h"
#include "mojo/edk/embedder/platform_channel_utils_posix.h"
#include "mojo/edk/embedder/platform_handle_vector.h"

#if !defined(OS_NACL)
#include <sys/uio.h>
#endif

namespace mojo {
namespace edk {

namespace {

const size_t kMaxBatchReadCapacity = 256 * 1024;

// A view over a Channel::Message object. The write queue uses these since
// large messages may need to be sent in chunks.
class MessageView {
 public:
  // Owns |message|. |offset| indexes the first unsent byte in the message.
  MessageView(Channel::MessagePtr message, size_t offset)
      : message_(std::move(message)),
        offset_(offset),
        handles_(message_->TakeHandlesForTransport()) {
    DCHECK_GT(message_->data_num_bytes(), offset_);
  }

  MessageView(MessageView&& other) { *this = std::move(other); }

  MessageView& operator=(MessageView&& other) {
    message_ = std::move(other.message_);
    offset_ = other.offset_;
    handles_ = std::move(other.handles_);
    return *this;
  }

  ~MessageView() {}

  const void* data() const {
    return static_cast<const char*>(message_->data()) + offset_;
  }

  size_t data_num_bytes() const { return message_->data_num_bytes() - offset_; }

  size_t data_offset() const { return offset_; }
  void advance_data_offset(size_t num_bytes) {
    DCHECK_GT(message_->data_num_bytes(), offset_ + num_bytes);
    offset_ += num_bytes;
  }

  ScopedPlatformHandleVectorPtr TakeHandles() { return std::move(handles_); }
  Channel::MessagePtr TakeMessage() { return std::move(message_); }

  void SetHandles(ScopedPlatformHandleVectorPtr handles) {
    handles_ = std::move(handles);
  }

 private:
  Channel::MessagePtr message_;
  size_t offset_;
  ScopedPlatformHandleVectorPtr handles_;

  DISALLOW_COPY_AND_ASSIGN(MessageView);
};

class ChannelPosix : public Channel,
                     public base::MessageLoop::DestructionObserver,
                     public base::MessageLoopForIO::Watcher {
 public:
  ChannelPosix(Delegate* delegate,
               ScopedPlatformHandle handle,
               scoped_refptr<base::TaskRunner> io_task_runner)
      : Channel(delegate),
        self_(this),
        handle_(std::move(handle)),
        io_task_runner_(io_task_runner)
#if defined(OS_MACOSX)
        ,
        handles_to_close_(new PlatformHandleVector)
#endif
  {
  }

  void Start() override {
    if (io_task_runner_->RunsTasksOnCurrentThread()) {
      StartOnIOThread();
    } else {
      io_task_runner_->PostTask(
          FROM_HERE, base::Bind(&ChannelPosix::StartOnIOThread, this));
    }
  }

  void ShutDownImpl() override {
    // Always shut down asynchronously when called through the public interface.
    io_task_runner_->PostTask(
        FROM_HERE, base::Bind(&ChannelPosix::ShutDownOnIOThread, this));
  }

  void Write(MessagePtr message) override {
    bool write_error = false;
    {
      base::AutoLock lock(write_lock_);
      if (reject_writes_)
        return;
      if (outgoing_messages_.empty()) {
        if (!WriteNoLock(MessageView(std::move(message), 0)))
          reject_writes_ = write_error = true;
      } else {
        outgoing_messages_.emplace_back(std::move(message), 0);
      }
    }
    if (write_error) {
      // Do not synchronously invoke OnError(). Write() may have been called by
      // the delegate and we don't want to re-enter it.
      io_task_runner_->PostTask(FROM_HERE,
                                base::Bind(&ChannelPosix::OnError, this));
    }
  }

  void LeakHandle() override {
    DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
    leak_handle_ = true;
  }

  bool GetReadPlatformHandles(
      size_t num_handles,
      const void* extra_header,
      size_t extra_header_size,
      ScopedPlatformHandleVectorPtr* handles) override {
    if (num_handles > std::numeric_limits<uint16_t>::max())
      return false;
#if defined(OS_MACOSX) && !defined(OS_IOS)
    // On OSX, we can have mach ports which are located in the extra header
    // section.
    using MachPortsEntry = Channel::Message::MachPortsEntry;
    using MachPortsExtraHeader = Channel::Message::MachPortsExtraHeader;
    CHECK(extra_header_size >=
          sizeof(MachPortsExtraHeader) + num_handles * sizeof(MachPortsEntry));
    const MachPortsExtraHeader* mach_ports_header =
        reinterpret_cast<const MachPortsExtraHeader*>(extra_header);
    size_t num_mach_ports = mach_ports_header->num_ports;
    CHECK(num_mach_ports <= num_handles);
    if (incoming_platform_handles_.size() + num_mach_ports < num_handles) {
      handles->reset();
      return true;
    }

    handles->reset(new PlatformHandleVector(num_handles));
    const MachPortsEntry* mach_ports = mach_ports_header->entries;
    for (size_t i = 0, mach_port_index = 0; i < num_handles; ++i) {
      if (mach_port_index < num_mach_ports &&
          mach_ports[mach_port_index].index == i) {
        (*handles)->at(i) = PlatformHandle(
            static_cast<mach_port_t>(mach_ports[mach_port_index].mach_port));
        CHECK((*handles)->at(i).type == PlatformHandle::Type::MACH);
        // These are actually just Mach port names until they're resolved from
        // the remote process.
        (*handles)->at(i).type = PlatformHandle::Type::MACH_NAME;
        mach_port_index++;
      } else {
        CHECK(!incoming_platform_handles_.empty());
        (*handles)->at(i) = incoming_platform_handles_.front();
        incoming_platform_handles_.pop_front();
      }
    }
#else
    if (incoming_platform_handles_.size() < num_handles) {
      handles->reset();
      return true;
    }

    handles->reset(new PlatformHandleVector(num_handles));
    for (size_t i = 0; i < num_handles; ++i) {
      (*handles)->at(i) = incoming_platform_handles_.front();
      incoming_platform_handles_.pop_front();
    }
#endif

    return true;
  }

 private:
  ~ChannelPosix() override {
    DCHECK(!read_watcher_);
    DCHECK(!write_watcher_);
    for (auto handle : incoming_platform_handles_)
      handle.CloseIfNecessary();
  }

  void StartOnIOThread() {
    DCHECK(!read_watcher_);
    DCHECK(!write_watcher_);
    read_watcher_.reset(new base::MessageLoopForIO::FileDescriptorWatcher);
    write_watcher_.reset(new base::MessageLoopForIO::FileDescriptorWatcher);
    base::MessageLoopForIO::current()->WatchFileDescriptor(
        handle_.get().handle, true /* persistent */,
        base::MessageLoopForIO::WATCH_READ, read_watcher_.get(), this);
    base::MessageLoop::current()->AddDestructionObserver(this);
  }

  void WaitForWriteOnIOThread() {
    base::AutoLock lock(write_lock_);
    WaitForWriteOnIOThreadNoLock();
  }

  void WaitForWriteOnIOThreadNoLock() {
    if (pending_write_)
      return;
    if (!write_watcher_)
      return;
    if (io_task_runner_->RunsTasksOnCurrentThread()) {
      pending_write_ = true;
      base::MessageLoopForIO::current()->WatchFileDescriptor(
          handle_.get().handle, false /* persistent */,
          base::MessageLoopForIO::WATCH_WRITE, write_watcher_.get(), this);
    } else {
      io_task_runner_->PostTask(
          FROM_HERE, base::Bind(&ChannelPosix::WaitForWriteOnIOThread, this));
    }
  }

  void ShutDownOnIOThread() {
    base::MessageLoop::current()->RemoveDestructionObserver(this);

    read_watcher_.reset();
    write_watcher_.reset();
    if (leak_handle_)
      ignore_result(handle_.release());
    handle_.reset();
#if defined(OS_MACOSX)
    handles_to_close_.reset();
#endif

    // May destroy the |this| if it was the last reference.
    self_ = nullptr;
  }

  // base::MessageLoop::DestructionObserver:
  void WillDestroyCurrentMessageLoop() override {
    DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
    if (self_)
      ShutDownOnIOThread();
  }

  // base::MessageLoopForIO::Watcher:
  void OnFileCanReadWithoutBlocking(int fd) override {
    CHECK_EQ(fd, handle_.get().handle);

    bool read_error = false;
    size_t next_read_size = 0;
    size_t buffer_capacity = 0;
    size_t total_bytes_read = 0;
    size_t bytes_read = 0;
    do {
      buffer_capacity = next_read_size;
      char* buffer = GetReadBuffer(&buffer_capacity);
      DCHECK_GT(buffer_capacity, 0u);

      ssize_t read_result = PlatformChannelRecvmsg(
          handle_.get(),
          buffer,
          buffer_capacity,
          &incoming_platform_handles_);

      if (read_result > 0) {
        bytes_read = static_cast<size_t>(read_result);
        total_bytes_read += bytes_read;
        if (!OnReadComplete(bytes_read, &next_read_size)) {
          read_error = true;
          break;
        }
      } else if (read_result == 0 ||
                 (errno != EAGAIN && errno != EWOULDBLOCK)) {
        read_error = true;
        break;
      }
    } while (bytes_read == buffer_capacity &&
             total_bytes_read < kMaxBatchReadCapacity &&
             next_read_size > 0);
    if (read_error) {
      // Stop receiving read notifications.
      read_watcher_.reset();

      OnError();
    }
  }

  void OnFileCanWriteWithoutBlocking(int fd) override {
    bool write_error = false;
    {
      base::AutoLock lock(write_lock_);
      pending_write_ = false;
      if (!FlushOutgoingMessagesNoLock())
        reject_writes_ = write_error = true;
    }
    if (write_error)
      OnError();
  }

  // Attempts to write a message directly to the channel. If the full message
  // cannot be written, it's queued and a wait is initiated to write the message
  // ASAP on the I/O thread.
  bool WriteNoLock(MessageView message_view) {
    size_t bytes_written = 0;
    do {
      message_view.advance_data_offset(bytes_written);

      ssize_t result;
      ScopedPlatformHandleVectorPtr handles = message_view.TakeHandles();
      if (handles && handles->size()) {
        iovec iov = {
          const_cast<void*>(message_view.data()),
          message_view.data_num_bytes()
        };
        // TODO: Handle lots of handles.
        result = PlatformChannelSendmsgWithHandles(
            handle_.get(), &iov, 1, handles->data(), handles->size());
        if (result >= 0) {
#if defined(OS_MACOSX)
          // There is a bug on OSX which makes it dangerous to close
          // a file descriptor while it is in transit. So instead we
          // store the file descriptor in a set and send a message to
          // the recipient, which is queued AFTER the message that
          // sent the FD. The recipient will reply to the message,
          // letting us know that it is now safe to close the file
          // descriptor. For more information, see:
          // http://crbug.com/298276
          std::vector<int> fds;
          for (auto& handle : *handles)
            fds.push_back(handle.handle);
          {
            base::AutoLock l(handles_to_close_lock_);
            for (auto& handle : *handles)
              handles_to_close_->push_back(handle);
          }
          MessagePtr fds_message(
              new Channel::Message(sizeof(fds[0]) * fds.size(), 0,
                                   Message::Header::MessageType::HANDLES_SENT));
          memcpy(fds_message->mutable_payload(), fds.data(),
                 sizeof(fds[0]) * fds.size());
          outgoing_messages_.emplace_back(std::move(fds_message), 0);
          handles->clear();
#else
          handles.reset();
#endif  // defined(OS_MACOSX)
        }
      } else {
        result = PlatformChannelWrite(handle_.get(), message_view.data(),
                                      message_view.data_num_bytes());
      }

      if (result < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK)
          return false;
        message_view.SetHandles(std::move(handles));
        outgoing_messages_.emplace_front(std::move(message_view));
        WaitForWriteOnIOThreadNoLock();
        return true;
      }

      bytes_written = static_cast<size_t>(result);
    } while (bytes_written < message_view.data_num_bytes());

    return FlushOutgoingMessagesNoLock();
  }

  bool FlushOutgoingMessagesNoLock() {
    std::deque<MessageView> messages;
    std::swap(outgoing_messages_, messages);

    while (!messages.empty()) {
      if (!WriteNoLock(std::move(messages.front())))
        return false;

      messages.pop_front();
      if (!outgoing_messages_.empty()) {
        // The message was requeued by WriteNoLock(), so we have to wait for
        // pipe to become writable again. Repopulate the message queue and exit.
        // If sending the message triggered any control messages, they may be
        // in |outgoing_messages_| in addition to or instead of the message
        // being sent.
        std::swap(messages, outgoing_messages_);
        while (!messages.empty()) {
          outgoing_messages_.push_front(std::move(messages.back()));
          messages.pop_back();
        }
        return true;
      }
    }

    return true;
  }

#if defined(OS_MACOSX)
  bool OnControlMessage(Message::Header::MessageType message_type,
                        const void* payload,
                        size_t payload_size,
                        ScopedPlatformHandleVectorPtr handles) override {
    switch (message_type) {
      case Message::Header::MessageType::HANDLES_SENT: {
        if (payload_size == 0)
          break;
        MessagePtr message(new Channel::Message(
            payload_size, 0, Message::Header::MessageType::HANDLES_SENT_ACK));
        memcpy(message->mutable_payload(), payload, payload_size);
        Write(std::move(message));
        return true;
      }

      case Message::Header::MessageType::HANDLES_SENT_ACK: {
        size_t num_fds = payload_size / sizeof(int);
        if (num_fds == 0 || payload_size % sizeof(int) != 0)
          break;

        const int* fds = reinterpret_cast<const int*>(payload);
        if (!CloseHandles(fds, num_fds))
          break;
        return true;
      }

      default:
        break;
    }

    return false;
  }

  // Closes handles referenced by |fds|. Returns false if |num_fds| is 0, or if
  // |fds| does not match a sequence of handles in |handles_to_close_|.
  bool CloseHandles(const int* fds, size_t num_fds) {
    base::AutoLock l(handles_to_close_lock_);
    if (!num_fds)
      return false;

    auto start =
        std::find_if(handles_to_close_->begin(), handles_to_close_->end(),
                     [&fds](const PlatformHandle& handle) {
                       return handle.handle == fds[0];
                     });
    if (start == handles_to_close_->end())
      return false;

    auto it = start;
    size_t i = 0;
    // The FDs in the message should match a sequence of handles in
    // |handles_to_close_|.
    for (; i < num_fds && it != handles_to_close_->end(); i++, ++it) {
      if (it->handle != fds[i])
        return false;

      it->CloseIfNecessary();
    }
    if (i != num_fds)
      return false;

    handles_to_close_->erase(start, it);
    return true;
  }
#endif  // defined(OS_MACOSX)

  // Keeps the Channel alive at least until explicit shutdown on the IO thread.
  scoped_refptr<Channel> self_;

  ScopedPlatformHandle handle_;
  scoped_refptr<base::TaskRunner> io_task_runner_;

  // These watchers must only be accessed on the IO thread.
  std::unique_ptr<base::MessageLoopForIO::FileDescriptorWatcher> read_watcher_;
  std::unique_ptr<base::MessageLoopForIO::FileDescriptorWatcher> write_watcher_;

  std::deque<PlatformHandle> incoming_platform_handles_;

  // Protects |pending_write_| and |outgoing_messages_|.
  base::Lock write_lock_;
  bool pending_write_ = false;
  bool reject_writes_ = false;
  std::deque<MessageView> outgoing_messages_;

  bool leak_handle_ = false;

#if defined(OS_MACOSX)
  base::Lock handles_to_close_lock_;
  ScopedPlatformHandleVectorPtr handles_to_close_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ChannelPosix);
};

}  // namespace

// static
scoped_refptr<Channel> Channel::Create(
    Delegate* delegate,
    ScopedPlatformHandle platform_handle,
    scoped_refptr<base::TaskRunner> io_task_runner) {
  return new ChannelPosix(delegate, std::move(platform_handle), io_task_runner);
}

}  // namespace edk
}  // namespace mojo
