// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_UPLOAD_DATA_STREAM_H_
#define NET_BASE_UPLOAD_DATA_STREAM_H_

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "net/base/completion_callback.h"
#include "net/base/net_export.h"

namespace net {

class DrainableIOBuffer;
class IOBuffer;
class UploadElementReader;

// A class to read all elements from an UploadData object.
class NET_EXPORT UploadDataStream {
 public:
  // An enum used to construct chunked data stream.
  enum Chunked { CHUNKED };

  // Constructs a non-chunked data stream.
  UploadDataStream(ScopedVector<UploadElementReader> element_readers,
                   int64 identifier);

  // Constructs a chunked data stream.
  UploadDataStream(Chunked chunked, int64 identifier);

  ~UploadDataStream();

  // Creates UploadDataStream with a reader.
  static UploadDataStream* CreateWithReader(
      scoped_ptr<UploadElementReader> reader,
      int64 identifier);

  // Initializes the stream. This function must be called before calling any
  // other method. It is not valid to call any method (other than the
  // destructor) if Init() returns a failure. This method can be called multiple
  // times. Calling this method after a Init() success results in resetting the
  // state.
  //
  // Does the initialization synchronously and returns the result if possible,
  // otherwise returns ERR_IO_PENDING and runs the callback with the result.
  //
  // Returns OK on success. Returns ERR_UPLOAD_FILE_CHANGED if the expected
  // file modification time is set (usually not set, but set for sliced
  // files) and the target file is changed.
  int Init(const CompletionCallback& callback);

  // When possible, reads up to |buf_len| bytes synchronously from the upload
  // data stream to |buf| and returns the number of bytes read; otherwise,
  // returns ERR_IO_PENDING and calls |callback| with the number of bytes read.
  // Partial reads are allowed. Zero is returned on a call to Read when there
  // are no remaining bytes in the stream, and IsEof() will return true
  // hereafter.
  //
  // If there's less data to read than we initially observed (i.e. the actual
  // upload data is smaller than size()), zeros are padded to ensure that
  // size() bytes can be read, which can happen for TYPE_FILE payloads.
  int Read(IOBuffer* buf, int buf_len, const CompletionCallback& callback);

  // Identifies a particular upload instance, which is used by the cache to
  // formulate a cache key.  This value should be unique across browser
  // sessions.  A value of 0 is used to indicate an unspecified identifier.
  int64 identifier() const { return identifier_; }

  // Returns the total size of the data stream and the current position.
  // size() is not to be used to determine whether the stream has ended
  // because it is possible for the stream to end before its size is reached,
  // for example, if the file is truncated. When the data is chunked, size()
  // always returns zero.
  uint64 size() const { return total_size_; }
  uint64 position() const { return current_position_; }

  bool is_chunked() const { return is_chunked_; }
  bool last_chunk_appended() const { return last_chunk_appended_; }

  const ScopedVector<UploadElementReader>& element_readers() const {
    return element_readers_;
  }

  // Returns true if all data has been consumed from this upload data
  // stream.
  bool IsEOF() const;

  // Returns true if the upload data in the stream is entirely in memory.
  bool IsInMemory() const;

  // Adds the given chunk of bytes to be sent with chunked transfer encoding.
  void AppendChunk(const char* bytes, int bytes_len, bool is_last_chunk);

  // Resets this instance to the uninitialized state.
  void Reset();

 private:
  // Runs Init() for all element readers.
  // This method is used to implement Init().
  int InitInternal(int start_index, const CompletionCallback& callback);

  // Resumes initialization and runs callback with the result when necessary.
  void ResumePendingInit(int start_index,
                         const CompletionCallback& callback,
                         int previous_result);

  // Reads data from the element readers.
  // This method is used to implement Read().
  int ReadInternal(scoped_refptr<DrainableIOBuffer> buf,
                   const CompletionCallback& callback);

  // Resumes pending read and calls callback with the result when necessary.
  void ResumePendingRead(scoped_refptr<DrainableIOBuffer> buf,
                         const CompletionCallback& callback,
                         int previous_result);

  // Processes result of UploadElementReader::Read(). If |result| indicates
  // success, updates |buf|'s offset. Otherwise, sets |read_failed_| to true.
  void ProcessReadResult(scoped_refptr<DrainableIOBuffer> buf,
                         int result);

  ScopedVector<UploadElementReader> element_readers_;

  // Index of the current upload element (i.e. the element currently being
  // read). The index is used as a cursor to iterate over elements in
  // |upload_data_|.
  size_t element_index_;

  // Size and current read position within the upload data stream.
  // |total_size_| is set to zero when the data is chunked.
  uint64 total_size_;
  uint64 current_position_;

  const int64 identifier_;

  const bool is_chunked_;
  bool last_chunk_appended_;

  // True if an error occcured during read operation.
  bool read_failed_;

  // True if the initialization was successful.
  bool initialized_successfully_;

  // Callback to resume reading chunked data.
  base::Closure pending_chunked_read_callback_;

  base::WeakPtrFactory<UploadDataStream> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(UploadDataStream);
};

}  // namespace net

#endif  // NET_BASE_UPLOAD_DATA_STREAM_H_
