// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_STREAMS_STREAM_H_
#define CONTENT_BROWSER_STREAMS_STREAM_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/byte_stream.h"
#include "content/common/content_export.h"
#include "url/gurl.h"

namespace net {
class HttpResponseHeaders;
class IOBuffer;
}

namespace content {

class StreamHandle;
class StreamHandleImpl;
class StreamReadObserver;
class StreamRegistry;
class StreamWriteObserver;

// A stream that sends data from an arbitrary source to an internal URL
// that can be read by an internal consumer.  It will continue to pull from the
// original URL as long as there is data available.  It can be read from
// multiple clients, but only one can be reading at a time. This allows a
// reader to consume part of the stream, then pass it along to another client
// to continue processing the stream.
class CONTENT_EXPORT Stream : public base::RefCountedThreadSafe<Stream> {
 public:
  enum StreamState {
    STREAM_HAS_DATA,
    STREAM_COMPLETE,
    STREAM_EMPTY,
    STREAM_ABORTED,
  };

  // Creates a stream.
  //
  // Security origin of Streams is checked in Blink (See BlobRegistry,
  // BlobURL and SecurityOrigin to understand how it works). There's no security
  // origin check in Chromium side for now.
  Stream(StreamRegistry* registry,
         StreamWriteObserver* write_observer,
         const GURL& url);

  // Sets the reader of this stream. Returns true on success, or false if there
  // is already a reader.
  bool SetReadObserver(StreamReadObserver* observer);

  // Removes the read observer.  |observer| must be the current observer.
  void RemoveReadObserver(StreamReadObserver* observer);

  // Removes the write observer.  |observer| must be the current observer.
  void RemoveWriteObserver(StreamWriteObserver* observer);

  // Stops accepting new data, clears all buffer, unregisters this stream from
  // |registry_| and make coming ReadRawData() calls return STREAM_ABORTED.
  void Abort();

  // Adds the data in |buffer| to the stream.  Takes ownership of |buffer|.
  void AddData(scoped_refptr<net::IOBuffer> buffer, size_t size);
  // Adds data of |size| at |data| to the stream. This method creates a copy
  // of the data, and then passes it to |writer_|.
  void AddData(const char* data, size_t size);

  // Notifies this stream that it will not be receiving any more data.
  void Finalize();

  // Reads a maximum of |buf_size| from the stream into |buf|.  Sets
  // |*bytes_read| to the number of bytes actually read.
  // Returns STREAM_HAS_DATA if data was read, STREAM_EMPTY if no data was read,
  // and STREAM_COMPLETE if the stream is finalized and all data has been read.
  StreamState ReadRawData(net::IOBuffer* buf, int buf_size, int* bytes_read);

  scoped_ptr<StreamHandle> CreateHandle(
      const GURL& original_url,
      const std::string& mime_type,
      scoped_refptr<net::HttpResponseHeaders> response_headers);
  void CloseHandle();

  // Indicates whether there is space in the buffer to add more data.
  bool can_add_data() const { return can_add_data_; }

  const GURL& url() const { return url_; }

  // For StreamRegistry to remember the last memory usage reported to it.
  size_t last_total_buffered_bytes() const {
    return last_total_buffered_bytes_;
  }

 private:
  friend class base::RefCountedThreadSafe<Stream>;

  virtual ~Stream();

  void OnSpaceAvailable();
  void OnDataAvailable();

  // Clears |data_| and related variables.
  void ClearBuffer();

  bool can_add_data_;

  GURL url_;

  // Buffer for storing data read from |reader_| but not yet read out from this
  // Stream by ReadRawData() method.
  scoped_refptr<net::IOBuffer> data_;
  // Number of bytes read from |reader_| into |data_| including bytes already
  // read out.
  size_t data_length_;
  // Number of bytes in |data_| that are already read out.
  size_t data_bytes_read_;

  // Last value returned by writer_->TotalBufferedBytes() in AddData(). Stored
  // in order to check memory usage.
  size_t last_total_buffered_bytes_;

  scoped_ptr<ByteStreamWriter> writer_;
  scoped_ptr<ByteStreamReader> reader_;

  StreamRegistry* registry_;
  StreamReadObserver* read_observer_;
  StreamWriteObserver* write_observer_;

  StreamHandleImpl* stream_handle_;

  base::WeakPtrFactory<Stream> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(Stream);
};

}  // namespace content

#endif  // CONTENT_BROWSER_STREAMS_STREAM_H_
