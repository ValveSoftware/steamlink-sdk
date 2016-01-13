// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/dump_cache/upgrade_win.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/threading/thread.h"
#include "base/win/scoped_handle.h"
#include "net/base/cache_type.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/disk_cache/blockfile/backend_impl.h"
#include "net/disk_cache/blockfile/entry_impl.h"
#include "net/http/http_cache.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/tools/dump_cache/cache_dumper.h"
#include "url/gurl.h"

namespace {

const wchar_t kPipePrefix[] = L"\\\\.\\pipe\\dump_cache_";
const int kChannelSize = 64 * 1024;
const int kNumStreams = 4;

// Simple macro to print out formatted debug messages. It is similar to a DLOG
// except that it doesn't include a header.
#ifdef NDEBUG
#define DEBUGMSG(...) {}
#else
#define DEBUGMSG(...) { printf(__VA_ARGS__); }
#endif

HANDLE OpenServer(const base::string16& pipe_number) {
  base::string16 pipe_name(kPipePrefix);
  pipe_name.append(pipe_number);
  return CreateFile(pipe_name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL,
                    OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
}

// This is the basic message to use between the two processes. It is intended
// to transmit a single action (like "get the key name for entry xx"), with up
// to 5 32-bit arguments and 4 64-bit arguments. After this structure, the rest
// of the message has |buffer_bytes| of length with the actual data.
struct Message {
  int32 command;
  int32 result;
  int32 buffer_bytes;
  int32 arg1;
  int32 arg2;
  int32 arg3;
  int32 arg4;
  int32 arg5;
  int64 long_arg1;
  int64 long_arg2;
  int64 long_arg3;
  int64 long_arg4;
  Message() {
    memset(this, 0, sizeof(*this));
  }
  Message& operator= (const Message& other) {
    memcpy(this, &other, sizeof(*this));
    return *this;
  }
};

const int kBufferSize = kChannelSize - sizeof(Message);
struct IoBuffer {
  Message msg;
  char buffer[kBufferSize];
};
COMPILE_ASSERT(sizeof(IoBuffer) == kChannelSize, invalid_io_buffer);


// The list of commands.
// Currently, there is support for working ONLY with one entry at a time.
enum {
  // Get the entry from list |arg1| that follows |long_arg1|.
  // The result is placed on |long_arg1| (closes the previous one).
  GET_NEXT_ENTRY = 1,
  // Get the entry from list |arg1| that precedes |long_arg1|.
  // The result is placed on |long_arg1| (closes the previous one).
  GET_PREV_ENTRY,
  // Closes the entry |long_arg1|.
  CLOSE_ENTRY,
  // Get the key of the entry |long_arg1|.
  GET_KEY,
  // Get last used (long_arg2) and last modified (long_arg3) times for the
  // entry at |long_arg1|.
  GET_USE_TIMES,
  // Returns on |arg2| the data size in bytes if the stream |arg1| of entry at
  // |long_arg1|.
  GET_DATA_SIZE,
  // Returns |arg2| bytes of the stream |arg1| for the entry at |long_arg1|,
  // starting at offset |arg3|.
  READ_DATA,
  // End processing requests.
  QUIT
};

// The list of return codes.
enum {
  RESULT_OK = 0,
  RESULT_UNKNOWN_COMMAND,
  RESULT_INVALID_PARAMETER,
  RESULT_NAME_OVERFLOW,
  RESULT_PENDING  // This error code is NOT expected by the master process.
};

// -----------------------------------------------------------------------

class BaseSM : public base::MessageLoopForIO::IOHandler {
 public:
  explicit BaseSM(HANDLE channel);
  virtual ~BaseSM();

 protected:
  bool SendMsg(const Message& msg);
  bool ReceiveMsg();
  bool ConnectChannel();
  bool IsPending();

  base::MessageLoopForIO::IOContext in_context_;
  base::MessageLoopForIO::IOContext out_context_;
  disk_cache::EntryImpl* entry_;
  HANDLE channel_;
  int state_;
  int pending_count_;
  scoped_ptr<char[]> in_buffer_;
  scoped_ptr<char[]> out_buffer_;
  IoBuffer* input_;
  IoBuffer* output_;
  base::Thread cache_thread_;

  DISALLOW_COPY_AND_ASSIGN(BaseSM);
};

BaseSM::BaseSM(HANDLE channel)
      : entry_(NULL), channel_(channel), state_(0), pending_count_(0),
        cache_thread_("cache") {
  in_buffer_.reset(new char[kChannelSize]);
  out_buffer_.reset(new char[kChannelSize]);
  input_ = reinterpret_cast<IoBuffer*>(in_buffer_.get());
  output_ = reinterpret_cast<IoBuffer*>(out_buffer_.get());

  memset(&in_context_, 0, sizeof(in_context_));
  memset(&out_context_, 0, sizeof(out_context_));
  in_context_.handler = this;
  out_context_.handler = this;
  base::MessageLoopForIO::current()->RegisterIOHandler(channel_, this);
  CHECK(cache_thread_.StartWithOptions(
      base::Thread::Options(base::MessageLoop::TYPE_IO, 0)));
}

BaseSM::~BaseSM() {
  if (entry_)
    entry_->Close();
}

bool BaseSM::SendMsg(const Message& msg) {
  // Only one command will be in-flight at a time. Let's start the Read IO here
  // when we know that it will be pending.
  if (!ReceiveMsg())
    return false;

  output_->msg = msg;
  DWORD written;
  if (!WriteFile(channel_, output_, sizeof(msg) + msg.buffer_bytes, &written,
                 &out_context_.overlapped)) {
    if (ERROR_IO_PENDING != GetLastError())
      return false;
  }
  pending_count_++;
  return true;
}

bool BaseSM::ReceiveMsg() {
  DWORD read;
  if (!ReadFile(channel_, input_, kChannelSize, &read,
                &in_context_.overlapped)) {
    if (ERROR_IO_PENDING != GetLastError())
      return false;
  }
  pending_count_++;
  return true;
}

bool BaseSM::ConnectChannel() {
  if (!ConnectNamedPipe(channel_, &in_context_.overlapped)) {
    DWORD error = GetLastError();
    if (ERROR_PIPE_CONNECTED == error)
      return true;
    // By returning true in case of a generic error, we allow the operation to
    // fail while sending the first message.
    if (ERROR_IO_PENDING != error)
      return true;
  }
  pending_count_++;
  return false;
}

bool BaseSM::IsPending() {
  return pending_count_ != 0;
}

// -----------------------------------------------------------------------

class MasterSM : public BaseSM {
 public:
  MasterSM(const base::FilePath& path, HANDLE channel)
      : BaseSM(channel),
        path_(path) {
  }
  virtual ~MasterSM() {
    delete writer_;
  }

  bool DoInit();
  virtual void OnIOCompleted(base::MessageLoopForIO::IOContext* context,
                             DWORD bytes_transfered,
                             DWORD error);

 private:
  enum {
    MASTER_INITIAL = 0,
    MASTER_CONNECT,
    MASTER_GET_ENTRY,
    MASTER_GET_NEXT_ENTRY,
    MASTER_GET_KEY,
    MASTER_GET_USE_TIMES,
    MASTER_GET_DATA_SIZE,
    MASTER_READ_DATA,
    MASTER_END
  };

  void SendGetPrevEntry();
  void DoGetEntry();
  void DoGetKey(int bytes_read);
  void DoCreateEntryComplete(int result);
  void DoGetUseTimes();
  void SendGetDataSize();
  void DoGetDataSize();
  void CloseEntry();
  void SendReadData();
  void DoReadData(int bytes_read);
  void DoReadDataComplete(int ret);
  void SendQuit();
  void DoEnd();
  void Fail();

  base::Time last_used_;
  base::Time last_modified_;
  int64 remote_entry_;
  int stream_;
  int bytes_remaining_;
  int offset_;
  int copied_entries_;
  int read_size_;
  scoped_ptr<disk_cache::Backend> cache_;
  CacheDumpWriter* writer_;
  const base::FilePath path_;
};

void MasterSM::OnIOCompleted(base::MessageLoopForIO::IOContext* context,
                             DWORD bytes_transfered,
                             DWORD error) {
  pending_count_--;
  if (context == &out_context_) {
    if (!error)
      return;
    return Fail();
  }

  int bytes_read = static_cast<int>(bytes_transfered);
  if (bytes_read < sizeof(Message) && state_ != MASTER_END &&
      state_ != MASTER_CONNECT) {
    printf("Communication breakdown\n");
    return Fail();
  }

  switch (state_) {
    case MASTER_CONNECT:
      SendGetPrevEntry();
      break;
    case MASTER_GET_ENTRY:
      DoGetEntry();
      break;
    case MASTER_GET_KEY:
      DoGetKey(bytes_read);
      break;
    case MASTER_GET_USE_TIMES:
      DoGetUseTimes();
      break;
    case MASTER_GET_DATA_SIZE:
      DoGetDataSize();
      break;
    case MASTER_READ_DATA:
      DoReadData(bytes_read);
      break;
    case MASTER_END:
      if (!IsPending())
        DoEnd();
      break;
    default:
      NOTREACHED();
      break;
  }
}

bool MasterSM::DoInit() {
  DEBUGMSG("Master DoInit\n");
  DCHECK(state_ == MASTER_INITIAL);

  scoped_ptr<disk_cache::Backend> cache;
  net::TestCompletionCallback cb;
  int rv = disk_cache::CreateCacheBackend(net::DISK_CACHE,
                                          net::CACHE_BACKEND_DEFAULT, path_, 0,
                                          false,
                                          cache_thread_.message_loop_proxy(),
                                          NULL, &cache, cb.callback());
  if (cb.GetResult(rv) != net::OK) {
    printf("Unable to initialize new files\n");
    return false;
  }
  cache_ = cache.Pass();
  writer_ = new CacheDumper(cache_.get());

  copied_entries_ = 0;
  remote_entry_ = 0;

  if (ConnectChannel()) {
    SendGetPrevEntry();
    // If we don't have pending operations we couldn't connect.
    return IsPending();
  }

  state_ = MASTER_CONNECT;
  return true;
}

void MasterSM::SendGetPrevEntry() {
  DEBUGMSG("Master SendGetPrevEntry\n");
  state_ = MASTER_GET_ENTRY;
  Message msg;
  msg.command = GET_PREV_ENTRY;
  msg.long_arg1 = remote_entry_;
  SendMsg(msg);
}

void MasterSM::DoGetEntry() {
  DEBUGMSG("Master DoGetEntry\n");
  DCHECK(state_ == MASTER_GET_ENTRY);
  DCHECK(input_->msg.command == GET_PREV_ENTRY);
  if (input_->msg.result != RESULT_OK)
    return Fail();

  if (!input_->msg.long_arg1) {
    printf("Done: %d entries copied over.\n", copied_entries_);
    return SendQuit();
  }
  remote_entry_ = input_->msg.long_arg1;
  state_ = MASTER_GET_KEY;
  Message msg;
  msg.command = GET_KEY;
  msg.long_arg1 = remote_entry_;
  SendMsg(msg);
}

void MasterSM::DoGetKey(int bytes_read) {
  DEBUGMSG("Master DoGetKey\n");
  DCHECK(state_ == MASTER_GET_KEY);
  DCHECK(input_->msg.command == GET_KEY);
  if (input_->msg.result == RESULT_NAME_OVERFLOW) {
    // The key is too long. Just move on.
    printf("Skipping entry (name too long)\n");
    return SendGetPrevEntry();
  }

  if (input_->msg.result != RESULT_OK)
    return Fail();

  std::string key(input_->buffer);
  DCHECK(key.size() == static_cast<size_t>(input_->msg.buffer_bytes - 1));

  int rv = writer_->CreateEntry(
      key, reinterpret_cast<disk_cache::Entry**>(&entry_),
      base::Bind(&MasterSM::DoCreateEntryComplete, base::Unretained(this)));

  if (rv != net::ERR_IO_PENDING)
    DoCreateEntryComplete(rv);
}

void MasterSM::DoCreateEntryComplete(int result) {
  std::string key(input_->buffer);
  if (result != net::OK) {
    printf("Skipping entry \"%s\": %d\n", key.c_str(), GetLastError());
    return SendGetPrevEntry();
  }

  if (key.size() >= 64) {
    key[60] = '.';
    key[61] = '.';
    key[62] = '.';
    key[63] = '\0';
  }
  DEBUGMSG("Entry \"%s\" created\n", key.c_str());
  state_ = MASTER_GET_USE_TIMES;
  Message msg;
  msg.command = GET_USE_TIMES;
  msg.long_arg1 = remote_entry_;
  SendMsg(msg);
}

void MasterSM::DoGetUseTimes() {
  DEBUGMSG("Master DoGetUseTimes\n");
  DCHECK(state_ == MASTER_GET_USE_TIMES);
  DCHECK(input_->msg.command == GET_USE_TIMES);
  if (input_->msg.result != RESULT_OK)
    return Fail();

  last_used_ = base::Time::FromInternalValue(input_->msg.long_arg2);
  last_modified_ = base::Time::FromInternalValue(input_->msg.long_arg3);
  stream_ = 0;
  SendGetDataSize();
}

void MasterSM::SendGetDataSize() {
  DEBUGMSG("Master SendGetDataSize (%d)\n", stream_);
  state_ = MASTER_GET_DATA_SIZE;
  Message msg;
  msg.command = GET_DATA_SIZE;
  msg.arg1 = stream_;
  msg.long_arg1 = remote_entry_;
  SendMsg(msg);
}

void MasterSM::DoGetDataSize() {
  DEBUGMSG("Master DoGetDataSize: %d\n", input_->msg.arg2);
  DCHECK(state_ == MASTER_GET_DATA_SIZE);
  DCHECK(input_->msg.command == GET_DATA_SIZE);
  if (input_->msg.result == RESULT_INVALID_PARAMETER)
    // No more streams, move to the next entry.
    return CloseEntry();

  if (input_->msg.result != RESULT_OK)
    return Fail();

  bytes_remaining_ = input_->msg.arg2;
  offset_ = 0;
  SendReadData();
}

void MasterSM::CloseEntry() {
  DEBUGMSG("Master CloseEntry\n");
  printf("%c\r", copied_entries_ % 2 ? 'x' : '+');
  writer_->CloseEntry(entry_, last_used_, last_modified_);
  entry_ = NULL;
  copied_entries_++;
  SendGetPrevEntry();
}

void MasterSM::SendReadData() {
  int read_size = std::min(bytes_remaining_, kBufferSize);
  DEBUGMSG("Master SendReadData (%d): %d bytes at %d\n", stream_, read_size,
           offset_);
  if (bytes_remaining_ <= 0) {
    stream_++;
    if (stream_ >= kNumStreams)
      return CloseEntry();
    return SendGetDataSize();
  }

  state_ = MASTER_READ_DATA;
  Message msg;
  msg.command = READ_DATA;
  msg.arg1 = stream_;
  msg.arg2 = read_size;
  msg.arg3 = offset_;
  msg.long_arg1 = remote_entry_;
  SendMsg(msg);
}

void MasterSM::DoReadData(int bytes_read) {
  DEBUGMSG("Master DoReadData: %d bytes\n", input_->msg.buffer_bytes);
  DCHECK(state_ == MASTER_READ_DATA);
  DCHECK(input_->msg.command == READ_DATA);
  if (input_->msg.result != RESULT_OK)
    return Fail();

  int read_size = input_->msg.buffer_bytes;
  if (!read_size) {
    printf("Read failed, entry \"%s\" truncated!\n", entry_->GetKey().c_str());
    bytes_remaining_ = 0;
    return SendReadData();
  }

  scoped_refptr<net::WrappedIOBuffer> buf =
      new net::WrappedIOBuffer(input_->buffer);
  int rv = writer_->WriteEntry(
      entry_, stream_, offset_, buf, read_size,
      base::Bind(&MasterSM::DoReadDataComplete, base::Unretained(this)));
  if (rv == net::ERR_IO_PENDING) {
    // We'll continue in DoReadDataComplete.
    read_size_ = read_size;
    return;
  }

  if (rv <= 0)
    return Fail();

  offset_ += read_size;
  bytes_remaining_ -= read_size;
  // Read some more.
  SendReadData();
}

void MasterSM::DoReadDataComplete(int ret) {
  if (ret != read_size_)
    return Fail();

  offset_ += ret;
  bytes_remaining_ -= ret;
  // Read some more.
  SendReadData();
}

void MasterSM::SendQuit() {
  DEBUGMSG("Master SendQuit\n");
  state_ = MASTER_END;
  Message msg;
  msg.command = QUIT;
  SendMsg(msg);
  if (!IsPending())
    DoEnd();
}

void MasterSM::DoEnd() {
  DEBUGMSG("Master DoEnd\n");
  base::MessageLoop::current()->PostTask(FROM_HERE,
                                         base::MessageLoop::QuitClosure());
}

void MasterSM::Fail() {
  DEBUGMSG("Master Fail\n");
  printf("Unexpected failure\n");
  SendQuit();
}

// -----------------------------------------------------------------------

class SlaveSM : public BaseSM {
 public:
  SlaveSM(const base::FilePath& path, HANDLE channel);
  virtual ~SlaveSM();

  bool DoInit();
  virtual void OnIOCompleted(base::MessageLoopForIO::IOContext* context,
                             DWORD bytes_transfered,
                             DWORD error);

 private:
  enum {
    SLAVE_INITIAL = 0,
    SLAVE_WAITING,
    SLAVE_END
  };

  void DoGetNextEntry();
  void DoGetPrevEntry();
  int32 GetEntryFromList();
  void DoGetEntryComplete(int result);
  void DoCloseEntry();
  void DoGetKey();
  void DoGetUseTimes();
  void DoGetDataSize();
  void DoReadData();
  void DoReadDataComplete(int ret);
  void DoEnd();
  void Fail();

  void* iterator_;
  Message msg_;  // Used for DoReadDataComplete and DoGetEntryComplete.

  scoped_ptr<disk_cache::BackendImpl> cache_;
};

SlaveSM::SlaveSM(const base::FilePath& path, HANDLE channel)
    : BaseSM(channel), iterator_(NULL) {
  scoped_ptr<disk_cache::Backend> cache;
  net::TestCompletionCallback cb;
  int rv = disk_cache::CreateCacheBackend(net::DISK_CACHE,
                                          net::CACHE_BACKEND_BLOCKFILE, path, 0,
                                          false,
                                          cache_thread_.message_loop_proxy(),
                                          NULL, &cache, cb.callback());
  if (cb.GetResult(rv) != net::OK) {
    printf("Unable to open cache files\n");
    return;
  }
  cache_.reset(reinterpret_cast<disk_cache::BackendImpl*>(cache.release()));
  cache_->SetUpgradeMode();
}

SlaveSM::~SlaveSM() {
  if (iterator_)
    cache_->EndEnumeration(&iterator_);
}

void SlaveSM::OnIOCompleted(base::MessageLoopForIO::IOContext* context,
                            DWORD bytes_transfered,
                            DWORD error) {
  pending_count_--;
  if (state_ == SLAVE_END) {
    if (IsPending())
      return;
    return DoEnd();
  }

  if (context == &out_context_) {
    if (!error)
      return;
    return Fail();
  }

  int bytes_read = static_cast<int>(bytes_transfered);
  if (bytes_read < sizeof(Message)) {
    printf("Communication breakdown\n");
    return Fail();
  }
  DCHECK(state_ == SLAVE_WAITING);

  switch (input_->msg.command) {
    case GET_NEXT_ENTRY:
      DoGetNextEntry();
      break;
    case GET_PREV_ENTRY:
      DoGetPrevEntry();
      break;
    case CLOSE_ENTRY:
      DoCloseEntry();
      break;
    case GET_KEY:
      DoGetKey();
      break;
    case GET_USE_TIMES:
      DoGetUseTimes();
      break;
    case GET_DATA_SIZE:
      DoGetDataSize();
      break;
    case READ_DATA:
      DoReadData();
      break;
    case QUIT:
      DoEnd();
      break;
    default:
      NOTREACHED();
      break;
  }
}

bool SlaveSM::DoInit() {
  DEBUGMSG("\t\t\tSlave DoInit\n");
  DCHECK(state_ == SLAVE_INITIAL);
  state_ = SLAVE_WAITING;
  if (!cache_.get())
    return false;

  return ReceiveMsg();
}

void SlaveSM::DoGetNextEntry() {
  DEBUGMSG("\t\t\tSlave DoGetNextEntry\n");
  Message msg;
  msg.command = GET_NEXT_ENTRY;

  if (input_->msg.arg1) {
    // We only support one list.
    msg.result = RESULT_UNKNOWN_COMMAND;
  } else {
    msg.result = GetEntryFromList();
    msg.long_arg1 = reinterpret_cast<int64>(entry_);
  }
  SendMsg(msg);
}

void SlaveSM::DoGetPrevEntry() {
  DEBUGMSG("\t\t\tSlave DoGetPrevEntry\n");
  Message msg;
  msg.command = GET_PREV_ENTRY;

  if (input_->msg.arg1) {
    // We only support one list.
    msg.result = RESULT_UNKNOWN_COMMAND;
  } else {
    msg.result = GetEntryFromList();
    if (msg.result == RESULT_PENDING) {
      // We are not done yet.
      msg_ = msg;
      return;
    }
    msg.long_arg1 = reinterpret_cast<int64>(entry_);
  }
  SendMsg(msg);
}

// Move to the next or previous entry on the list.
int32 SlaveSM::GetEntryFromList() {
  DEBUGMSG("\t\t\tSlave GetEntryFromList\n");
  if (input_->msg.long_arg1 != reinterpret_cast<int64>(entry_))
    return RESULT_INVALID_PARAMETER;

  // We know that the current iteration is valid.
  if (entry_)
    entry_->Close();

  int rv;
  if (input_->msg.command == GET_NEXT_ENTRY) {
    rv = cache_->OpenNextEntry(
        &iterator_, reinterpret_cast<disk_cache::Entry**>(&entry_),
        base::Bind(&SlaveSM::DoGetEntryComplete, base::Unretained(this)));
  } else {
    DCHECK(input_->msg.command == GET_PREV_ENTRY);
    rv = cache_->OpenPrevEntry(&iterator_,
                               reinterpret_cast<disk_cache::Entry**>(&entry_),
                               base::Bind(&SlaveSM::DoGetEntryComplete,
                                          base::Unretained(this)));
  }
  DCHECK_EQ(net::ERR_IO_PENDING, rv);
  return RESULT_PENDING;
}

void SlaveSM::DoGetEntryComplete(int result) {
  DEBUGMSG("\t\t\tSlave DoGetEntryComplete\n");
  if (result != net::OK) {
    entry_ = NULL;
    DEBUGMSG("\t\t\tSlave end of list\n");
  }

  msg_.result = RESULT_OK;
  msg_.long_arg1 = reinterpret_cast<int64>(entry_);
  SendMsg(msg_);
}

void SlaveSM::DoCloseEntry() {
  DEBUGMSG("\t\t\tSlave DoCloseEntry\n");
  Message msg;
  msg.command = GET_KEY;

  if (!entry_ || input_->msg.long_arg1 != reinterpret_cast<int64>(entry_)) {
    msg.result =  RESULT_INVALID_PARAMETER;
  } else {
    entry_->Close();
    entry_ = NULL;
    cache_->EndEnumeration(&iterator_);
    msg.result = RESULT_OK;
  }
  SendMsg(msg);
}

void SlaveSM::DoGetKey() {
  DEBUGMSG("\t\t\tSlave DoGetKey\n");
  Message msg;
  msg.command = GET_KEY;

  if (!entry_ || input_->msg.long_arg1 != reinterpret_cast<int64>(entry_)) {
    msg.result =  RESULT_INVALID_PARAMETER;
  } else {
    std::string key = entry_->GetKey();
    msg.buffer_bytes = std::min(key.size() + 1,
                                static_cast<size_t>(kBufferSize));
    memcpy(output_->buffer, key.c_str(), msg.buffer_bytes);
    if (msg.buffer_bytes != static_cast<int32>(key.size() + 1)) {
      // We don't support moving this entry. Just tell the master.
      msg.result = RESULT_NAME_OVERFLOW;
    } else {
      msg.result = RESULT_OK;
    }
  }
  SendMsg(msg);
}

void SlaveSM::DoGetUseTimes() {
  DEBUGMSG("\t\t\tSlave DoGetUseTimes\n");
  Message msg;
  msg.command = GET_USE_TIMES;

  if (!entry_ || input_->msg.long_arg1 != reinterpret_cast<int64>(entry_)) {
    msg.result =  RESULT_INVALID_PARAMETER;
  } else {
    msg.long_arg2 = entry_->GetLastUsed().ToInternalValue();
    msg.long_arg3 = entry_->GetLastModified().ToInternalValue();
    msg.result = RESULT_OK;
  }
  SendMsg(msg);
}

void SlaveSM::DoGetDataSize() {
  DEBUGMSG("\t\t\tSlave DoGetDataSize\n");
  Message msg;
  msg.command = GET_DATA_SIZE;

  int stream = input_->msg.arg1;
  if (!entry_ || input_->msg.long_arg1 != reinterpret_cast<int64>(entry_) ||
      stream < 0 || stream >= kNumStreams) {
    msg.result =  RESULT_INVALID_PARAMETER;
  } else {
    msg.arg1 = stream;
    msg.arg2 = entry_->GetDataSize(stream);
    msg.result = RESULT_OK;
  }
  SendMsg(msg);
}

void SlaveSM::DoReadData() {
  DEBUGMSG("\t\t\tSlave DoReadData\n");
  Message msg;
  msg.command = READ_DATA;

  int stream = input_->msg.arg1;
  int size = input_->msg.arg2;
  if (!entry_ || input_->msg.long_arg1 != reinterpret_cast<int64>(entry_) ||
      stream < 0 || stream > 1 || size > kBufferSize) {
    msg.result =  RESULT_INVALID_PARAMETER;
  } else {
    scoped_refptr<net::WrappedIOBuffer> buf =
        new net::WrappedIOBuffer(output_->buffer);
    int ret = entry_->ReadData(stream, input_->msg.arg3, buf, size,
                               base::Bind(&SlaveSM::DoReadDataComplete,
                                          base::Unretained(this)));
    if (ret == net::ERR_IO_PENDING) {
      // Save the message so we can continue were we left.
      msg_ = msg;
      return;
    }

    msg.buffer_bytes = (ret < 0) ? 0 : ret;
    msg.result = RESULT_OK;
  }
  SendMsg(msg);
}

void SlaveSM::DoReadDataComplete(int ret) {
  DEBUGMSG("\t\t\tSlave DoReadDataComplete\n");
  DCHECK_EQ(READ_DATA, msg_.command);
  msg_.buffer_bytes = (ret < 0) ? 0 : ret;
  msg_.result = RESULT_OK;
  SendMsg(msg_);
}

void SlaveSM::DoEnd() {
  DEBUGMSG("\t\t\tSlave DoEnd\n");
  base::MessageLoop::current()->PostTask(FROM_HERE,
                                         base::MessageLoop::QuitClosure());
}

void SlaveSM::Fail() {
  DEBUGMSG("\t\t\tSlave Fail\n");
  printf("Unexpected failure\n");
  state_ = SLAVE_END;
  if (IsPending()) {
    CancelIo(channel_);
  } else {
    DoEnd();
  }
}

}  // namespace.

// -----------------------------------------------------------------------

HANDLE CreateServer(base::string16* pipe_number) {
  base::string16 pipe_name(kPipePrefix);
  srand(static_cast<int>(base::Time::Now().ToInternalValue()));
  *pipe_number = base::IntToString16(rand());
  pipe_name.append(*pipe_number);

  DWORD mode = PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE |
               FILE_FLAG_OVERLAPPED;

  return CreateNamedPipe(pipe_name.c_str(), mode, 0, 1, kChannelSize,
                         kChannelSize, 0, NULL);
}

// This is the controller process for an upgrade operation.
int UpgradeCache(const base::FilePath& output_path, HANDLE pipe) {
  base::MessageLoopForIO loop;

  MasterSM master(output_path, pipe);
  if (!master.DoInit()) {
    printf("Unable to talk with the helper\n");
    return -1;
  }

  loop.Run();
  return 0;
}

// This process will only execute commands from the controller.
int RunSlave(const base::FilePath& input_path,
             const base::string16& pipe_number) {
  base::MessageLoopForIO loop;

  base::win::ScopedHandle pipe(OpenServer(pipe_number));
  if (!pipe.IsValid()) {
    printf("Unable to open the server pipe\n");
    return -1;
  }

  SlaveSM slave(input_path, pipe);
  if (!slave.DoInit()) {
    printf("Unable to talk with the main process\n");
    return -1;
  }

  loop.Run();
  return 0;
}
