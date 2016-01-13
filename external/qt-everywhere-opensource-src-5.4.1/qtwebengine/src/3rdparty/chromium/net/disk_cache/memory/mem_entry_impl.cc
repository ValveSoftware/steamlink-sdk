// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/memory/mem_entry_impl.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/memory/mem_backend_impl.h"
#include "net/disk_cache/net_log_parameters.h"

using base::Time;

namespace {

const int kSparseData = 1;

// Maximum size of a sparse entry is 2 to the power of this number.
const int kMaxSparseEntryBits = 12;

// Sparse entry has maximum size of 4KB.
const int kMaxSparseEntrySize = 1 << kMaxSparseEntryBits;

// Convert global offset to child index.
inline int ToChildIndex(int64 offset) {
  return static_cast<int>(offset >> kMaxSparseEntryBits);
}

// Convert global offset to offset in child entry.
inline int ToChildOffset(int64 offset) {
  return static_cast<int>(offset & (kMaxSparseEntrySize - 1));
}

// Returns a name for a child entry given the base_name of the parent and the
// child_id.  This name is only used for logging purposes.
// If the entry is called entry_name, child entries will be named something
// like Range_entry_name:YYY where YYY is the number of the particular child.
std::string GenerateChildName(const std::string& base_name, int child_id) {
  return base::StringPrintf("Range_%s:%i", base_name.c_str(), child_id);
}

// Returns NetLog parameters for the creation of a child MemEntryImpl.  Separate
// function needed because child entries don't suppport GetKey().
base::Value* NetLogChildEntryCreationCallback(
    const disk_cache::MemEntryImpl* parent,
    int child_id,
    net::NetLog::LogLevel /* log_level */) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetString("key", GenerateChildName(parent->GetKey(), child_id));
  dict->SetBoolean("created", true);
  return dict;
}

}  // namespace

namespace disk_cache {

MemEntryImpl::MemEntryImpl(MemBackendImpl* backend) {
  doomed_ = false;
  backend_ = backend;
  ref_count_ = 0;
  parent_ = NULL;
  child_id_ = 0;
  child_first_pos_ = 0;
  next_ = NULL;
  prev_ = NULL;
  for (int i = 0; i < NUM_STREAMS; i++)
    data_size_[i] = 0;
}

// ------------------------------------------------------------------------

bool MemEntryImpl::CreateEntry(const std::string& key, net::NetLog* net_log) {
  key_ = key;
  Time current = Time::Now();
  last_modified_ = current;
  last_used_ = current;

  net_log_ = net::BoundNetLog::Make(net_log,
                                    net::NetLog::SOURCE_MEMORY_CACHE_ENTRY);
  // Must be called after |key_| is set, so GetKey() works.
  net_log_.BeginEvent(
      net::NetLog::TYPE_DISK_CACHE_MEM_ENTRY_IMPL,
      CreateNetLogEntryCreationCallback(this, true));

  Open();
  backend_->ModifyStorageSize(0, static_cast<int32>(key.size()));
  return true;
}

void MemEntryImpl::InternalDoom() {
  net_log_.AddEvent(net::NetLog::TYPE_ENTRY_DOOM);
  doomed_ = true;
  if (!ref_count_) {
    if (type() == kParentEntry) {
      // If this is a parent entry, we need to doom all the child entries.
      if (children_.get()) {
        EntryMap children;
        children.swap(*children_);
        for (EntryMap::iterator i = children.begin();
             i != children.end(); ++i) {
          // Since a pointer to this object is also saved in the map, avoid
          // dooming it.
          if (i->second != this)
            i->second->Doom();
        }
        DCHECK(children_->empty());
      }
    } else {
      // If this is a child entry, detach it from the parent.
      parent_->DetachChild(child_id_);
    }
    delete this;
  }
}

void MemEntryImpl::Open() {
  // Only a parent entry can be opened.
  // TODO(hclam): make sure it's correct to not apply the concept of ref
  // counting to child entry.
  DCHECK(type() == kParentEntry);
  ref_count_++;
  DCHECK_GE(ref_count_, 0);
  DCHECK(!doomed_);
}

bool MemEntryImpl::InUse() {
  if (type() == kParentEntry) {
    return ref_count_ > 0;
  } else {
    // A child entry is always not in use. The consequence is that a child entry
    // can always be evicted while the associated parent entry is currently in
    // used (i.e. opened).
    return false;
  }
}

// ------------------------------------------------------------------------

void MemEntryImpl::Doom() {
  if (doomed_)
    return;
  if (type() == kParentEntry) {
    // Perform internal doom from the backend if this is a parent entry.
    backend_->InternalDoomEntry(this);
  } else {
    // Manually detach from the backend and perform internal doom.
    backend_->RemoveFromRankingList(this);
    InternalDoom();
  }
}

void MemEntryImpl::Close() {
  // Only a parent entry can be closed.
  DCHECK(type() == kParentEntry);
  ref_count_--;
  DCHECK_GE(ref_count_, 0);
  if (!ref_count_ && doomed_)
    InternalDoom();
}

std::string MemEntryImpl::GetKey() const {
  // A child entry doesn't have key so this method should not be called.
  DCHECK(type() == kParentEntry);
  return key_;
}

Time MemEntryImpl::GetLastUsed() const {
  return last_used_;
}

Time MemEntryImpl::GetLastModified() const {
  return last_modified_;
}

int32 MemEntryImpl::GetDataSize(int index) const {
  if (index < 0 || index >= NUM_STREAMS)
    return 0;
  return data_size_[index];
}

int MemEntryImpl::ReadData(int index, int offset, IOBuffer* buf, int buf_len,
                           const CompletionCallback& callback) {
  if (net_log_.IsLogging()) {
    net_log_.BeginEvent(
        net::NetLog::TYPE_ENTRY_READ_DATA,
        CreateNetLogReadWriteDataCallback(index, offset, buf_len, false));
  }

  int result = InternalReadData(index, offset, buf, buf_len);

  if (net_log_.IsLogging()) {
    net_log_.EndEvent(
        net::NetLog::TYPE_ENTRY_READ_DATA,
        CreateNetLogReadWriteCompleteCallback(result));
  }
  return result;
}

int MemEntryImpl::WriteData(int index, int offset, IOBuffer* buf, int buf_len,
                            const CompletionCallback& callback, bool truncate) {
  if (net_log_.IsLogging()) {
    net_log_.BeginEvent(
        net::NetLog::TYPE_ENTRY_WRITE_DATA,
        CreateNetLogReadWriteDataCallback(index, offset, buf_len, truncate));
  }

  int result = InternalWriteData(index, offset, buf, buf_len, truncate);

  if (net_log_.IsLogging()) {
    net_log_.EndEvent(
        net::NetLog::TYPE_ENTRY_WRITE_DATA,
        CreateNetLogReadWriteCompleteCallback(result));
  }
  return result;
}

int MemEntryImpl::ReadSparseData(int64 offset, IOBuffer* buf, int buf_len,
                                 const CompletionCallback& callback) {
  if (net_log_.IsLogging()) {
    net_log_.BeginEvent(
        net::NetLog::TYPE_SPARSE_READ,
        CreateNetLogSparseOperationCallback(offset, buf_len));
  }
  int result = InternalReadSparseData(offset, buf, buf_len);
  if (net_log_.IsLogging())
    net_log_.EndEvent(net::NetLog::TYPE_SPARSE_READ);
  return result;
}

int MemEntryImpl::WriteSparseData(int64 offset, IOBuffer* buf, int buf_len,
                                  const CompletionCallback& callback) {
  if (net_log_.IsLogging()) {
    net_log_.BeginEvent(
        net::NetLog::TYPE_SPARSE_WRITE,
        CreateNetLogSparseOperationCallback(offset, buf_len));
  }
  int result = InternalWriteSparseData(offset, buf, buf_len);
  if (net_log_.IsLogging())
    net_log_.EndEvent(net::NetLog::TYPE_SPARSE_WRITE);
  return result;
}

int MemEntryImpl::GetAvailableRange(int64 offset, int len, int64* start,
                                    const CompletionCallback& callback) {
  if (net_log_.IsLogging()) {
    net_log_.BeginEvent(
        net::NetLog::TYPE_SPARSE_GET_RANGE,
        CreateNetLogSparseOperationCallback(offset, len));
  }
  int result = GetAvailableRange(offset, len, start);
  if (net_log_.IsLogging()) {
    net_log_.EndEvent(
        net::NetLog::TYPE_SPARSE_GET_RANGE,
        CreateNetLogGetAvailableRangeResultCallback(*start, result));
  }
  return result;
}

bool MemEntryImpl::CouldBeSparse() const {
  DCHECK_EQ(kParentEntry, type());
  return (children_.get() != NULL);
}

int MemEntryImpl::ReadyForSparseIO(const CompletionCallback& callback) {
  return net::OK;
}

// ------------------------------------------------------------------------

MemEntryImpl::~MemEntryImpl() {
  for (int i = 0; i < NUM_STREAMS; i++)
    backend_->ModifyStorageSize(data_size_[i], 0);
  backend_->ModifyStorageSize(static_cast<int32>(key_.size()), 0);
  net_log_.EndEvent(net::NetLog::TYPE_DISK_CACHE_MEM_ENTRY_IMPL);
}

int MemEntryImpl::InternalReadData(int index, int offset, IOBuffer* buf,
                                   int buf_len) {
  DCHECK(type() == kParentEntry || index == kSparseData);

  if (index < 0 || index >= NUM_STREAMS)
    return net::ERR_INVALID_ARGUMENT;

  int entry_size = GetDataSize(index);
  if (offset >= entry_size || offset < 0 || !buf_len)
    return 0;

  if (buf_len < 0)
    return net::ERR_INVALID_ARGUMENT;

  if (offset + buf_len > entry_size)
    buf_len = entry_size - offset;

  UpdateRank(false);

  memcpy(buf->data(), &(data_[index])[offset], buf_len);
  return buf_len;
}

int MemEntryImpl::InternalWriteData(int index, int offset, IOBuffer* buf,
                                    int buf_len, bool truncate) {
  DCHECK(type() == kParentEntry || index == kSparseData);

  if (index < 0 || index >= NUM_STREAMS)
    return net::ERR_INVALID_ARGUMENT;

  if (offset < 0 || buf_len < 0)
    return net::ERR_INVALID_ARGUMENT;

  int max_file_size = backend_->MaxFileSize();

  // offset of buf_len could be negative numbers.
  if (offset > max_file_size || buf_len > max_file_size ||
      offset + buf_len > max_file_size) {
    return net::ERR_FAILED;
  }

  // Read the size at this point.
  int entry_size = GetDataSize(index);

  PrepareTarget(index, offset, buf_len);

  if (entry_size < offset + buf_len) {
    backend_->ModifyStorageSize(entry_size, offset + buf_len);
    data_size_[index] = offset + buf_len;
  } else if (truncate) {
    if (entry_size > offset + buf_len) {
      backend_->ModifyStorageSize(entry_size, offset + buf_len);
      data_size_[index] = offset + buf_len;
    }
  }

  UpdateRank(true);

  if (!buf_len)
    return 0;

  memcpy(&(data_[index])[offset], buf->data(), buf_len);
  return buf_len;
}

int MemEntryImpl::InternalReadSparseData(int64 offset, IOBuffer* buf,
                                         int buf_len) {
  DCHECK(type() == kParentEntry);

  if (!InitSparseInfo())
    return net::ERR_CACHE_OPERATION_NOT_SUPPORTED;

  if (offset < 0 || buf_len < 0)
    return net::ERR_INVALID_ARGUMENT;

  // We will keep using this buffer and adjust the offset in this buffer.
  scoped_refptr<net::DrainableIOBuffer> io_buf(
      new net::DrainableIOBuffer(buf, buf_len));

  // Iterate until we have read enough.
  while (io_buf->BytesRemaining()) {
    MemEntryImpl* child = OpenChild(offset + io_buf->BytesConsumed(), false);

    // No child present for that offset.
    if (!child)
      break;

    // We then need to prepare the child offset and len.
    int child_offset = ToChildOffset(offset + io_buf->BytesConsumed());

    // If we are trying to read from a position that the child entry has no data
    // we should stop.
    if (child_offset < child->child_first_pos_)
      break;
    if (net_log_.IsLogging()) {
      net_log_.BeginEvent(
          net::NetLog::TYPE_SPARSE_READ_CHILD_DATA,
          CreateNetLogSparseReadWriteCallback(child->net_log().source(),
                                              io_buf->BytesRemaining()));
    }
    int ret = child->ReadData(kSparseData, child_offset, io_buf.get(),
                              io_buf->BytesRemaining(), CompletionCallback());
    if (net_log_.IsLogging()) {
      net_log_.EndEventWithNetErrorCode(
          net::NetLog::TYPE_SPARSE_READ_CHILD_DATA, ret);
    }

    // If we encounter an error in one entry, return immediately.
    if (ret < 0)
      return ret;
    else if (ret == 0)
      break;

    // Increment the counter by number of bytes read in the child entry.
    io_buf->DidConsume(ret);
  }

  UpdateRank(false);

  return io_buf->BytesConsumed();
}

int MemEntryImpl::InternalWriteSparseData(int64 offset, IOBuffer* buf,
                                          int buf_len) {
  DCHECK(type() == kParentEntry);

  if (!InitSparseInfo())
    return net::ERR_CACHE_OPERATION_NOT_SUPPORTED;

  if (offset < 0 || buf_len < 0)
    return net::ERR_INVALID_ARGUMENT;

  scoped_refptr<net::DrainableIOBuffer> io_buf(
      new net::DrainableIOBuffer(buf, buf_len));

  // This loop walks through child entries continuously starting from |offset|
  // and writes blocks of data (of maximum size kMaxSparseEntrySize) into each
  // child entry until all |buf_len| bytes are written. The write operation can
  // start in the middle of an entry.
  while (io_buf->BytesRemaining()) {
    MemEntryImpl* child = OpenChild(offset + io_buf->BytesConsumed(), true);
    int child_offset = ToChildOffset(offset + io_buf->BytesConsumed());

    // Find the right amount to write, this evaluates the remaining bytes to
    // write and remaining capacity of this child entry.
    int write_len = std::min(static_cast<int>(io_buf->BytesRemaining()),
                             kMaxSparseEntrySize - child_offset);

    // Keep a record of the last byte position (exclusive) in the child.
    int data_size = child->GetDataSize(kSparseData);

    if (net_log_.IsLogging()) {
      net_log_.BeginEvent(
          net::NetLog::TYPE_SPARSE_WRITE_CHILD_DATA,
          CreateNetLogSparseReadWriteCallback(child->net_log().source(),
                                              write_len));
    }

    // Always writes to the child entry. This operation may overwrite data
    // previously written.
    // TODO(hclam): if there is data in the entry and this write is not
    // continuous we may want to discard this write.
    int ret = child->WriteData(kSparseData, child_offset, io_buf.get(),
                               write_len, CompletionCallback(), true);
    if (net_log_.IsLogging()) {
      net_log_.EndEventWithNetErrorCode(
          net::NetLog::TYPE_SPARSE_WRITE_CHILD_DATA, ret);
    }
    if (ret < 0)
      return ret;
    else if (ret == 0)
      break;

    // Keep a record of the first byte position in the child if the write was
    // not aligned nor continuous. This is to enable witting to the middle
    // of an entry and still keep track of data off the aligned edge.
    if (data_size != child_offset)
      child->child_first_pos_ = child_offset;

    // Adjust the offset in the IO buffer.
    io_buf->DidConsume(ret);
  }

  UpdateRank(true);

  return io_buf->BytesConsumed();
}

int MemEntryImpl::GetAvailableRange(int64 offset, int len, int64* start) {
  DCHECK(type() == kParentEntry);
  DCHECK(start);

  if (!InitSparseInfo())
    return net::ERR_CACHE_OPERATION_NOT_SUPPORTED;

  if (offset < 0 || len < 0 || !start)
    return net::ERR_INVALID_ARGUMENT;

  MemEntryImpl* current_child = NULL;

  // Find the first child and record the number of empty bytes.
  int empty = FindNextChild(offset, len, &current_child);
  if (current_child) {
    *start = offset + empty;
    len -= empty;

    // Counts the number of continuous bytes.
    int continuous = 0;

    // This loop scan for continuous bytes.
    while (len && current_child) {
      // Number of bytes available in this child.
      int data_size = current_child->GetDataSize(kSparseData) -
                      ToChildOffset(*start + continuous);
      if (data_size > len)
        data_size = len;

      // We have found more continuous bytes so increment the count. Also
      // decrement the length we should scan.
      continuous += data_size;
      len -= data_size;

      // If the next child is discontinuous, break the loop.
      if (FindNextChild(*start + continuous, len, &current_child))
        break;
    }
    return continuous;
  }
  *start = offset;
  return 0;
}

void MemEntryImpl::PrepareTarget(int index, int offset, int buf_len) {
  int entry_size = GetDataSize(index);

  if (entry_size >= offset + buf_len)
    return;  // Not growing the stored data.

  if (static_cast<int>(data_[index].size()) < offset + buf_len)
    data_[index].resize(offset + buf_len);

  if (offset <= entry_size)
    return;  // There is no "hole" on the stored data.

  // Cleanup the hole not written by the user. The point is to avoid returning
  // random stuff later on.
  memset(&(data_[index])[entry_size], 0, offset - entry_size);
}

void MemEntryImpl::UpdateRank(bool modified) {
  Time current = Time::Now();
  last_used_ = current;

  if (modified)
    last_modified_ = current;

  if (!doomed_)
    backend_->UpdateRank(this);
}

bool MemEntryImpl::InitSparseInfo() {
  DCHECK(type() == kParentEntry);

  if (!children_.get()) {
    // If we already have some data in sparse stream but we are being
    // initialized as a sparse entry, we should fail.
    if (GetDataSize(kSparseData))
      return false;
    children_.reset(new EntryMap());

    // The parent entry stores data for the first block, so save this object to
    // index 0.
    (*children_)[0] = this;
  }
  return true;
}

bool MemEntryImpl::InitChildEntry(MemEntryImpl* parent, int child_id,
                                  net::NetLog* net_log) {
  DCHECK(!parent_);
  DCHECK(!child_id_);

  net_log_ = net::BoundNetLog::Make(net_log,
                                    net::NetLog::SOURCE_MEMORY_CACHE_ENTRY);
  net_log_.BeginEvent(
      net::NetLog::TYPE_DISK_CACHE_MEM_ENTRY_IMPL,
      base::Bind(&NetLogChildEntryCreationCallback, parent, child_id_));

  parent_ = parent;
  child_id_ = child_id;
  Time current = Time::Now();
  last_modified_ = current;
  last_used_ = current;
  // Insert this to the backend's ranking list.
  backend_->InsertIntoRankingList(this);
  return true;
}

MemEntryImpl* MemEntryImpl::OpenChild(int64 offset, bool create) {
  DCHECK(type() == kParentEntry);
  int index = ToChildIndex(offset);
  EntryMap::iterator i = children_->find(index);
  if (i != children_->end()) {
    return i->second;
  } else if (create) {
    MemEntryImpl* child = new MemEntryImpl(backend_);
    child->InitChildEntry(this, index, net_log_.net_log());
    (*children_)[index] = child;
    return child;
  }
  return NULL;
}

int MemEntryImpl::FindNextChild(int64 offset, int len, MemEntryImpl** child) {
  DCHECK(child);
  *child = NULL;
  int scanned_len = 0;

  // This loop tries to find the first existing child.
  while (scanned_len < len) {
    // This points to the current offset in the child.
    int current_child_offset = ToChildOffset(offset + scanned_len);
    MemEntryImpl* current_child = OpenChild(offset + scanned_len, false);
    if (current_child) {
      int child_first_pos = current_child->child_first_pos_;

      // This points to the first byte that we should be reading from, we need
      // to take care of the filled region and the current offset in the child.
      int first_pos =  std::max(current_child_offset, child_first_pos);

      // If the first byte position we should read from doesn't exceed the
      // filled region, we have found the first child.
      if (first_pos < current_child->GetDataSize(kSparseData)) {
         *child = current_child;

         // We need to advance the scanned length.
         scanned_len += first_pos - current_child_offset;
         break;
      }
    }
    scanned_len += kMaxSparseEntrySize - current_child_offset;
  }
  return scanned_len;
}

void MemEntryImpl::DetachChild(int child_id) {
  children_->erase(child_id);
}

}  // namespace disk_cache
