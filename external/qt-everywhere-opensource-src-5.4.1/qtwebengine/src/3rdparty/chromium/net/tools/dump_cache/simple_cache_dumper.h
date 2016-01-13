// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_DUMP_CACHE_SIMPLE_CACHE_DUMPER_H_
#define NET_TOOLS_DUMP_CACHE_SIMPLE_CACHE_DUMPER_H_

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread.h"
#include "net/base/completion_callback.h"

class DiskDumper;

namespace disk_cache {
class Backend;
class Entry;
}  // namespace disk_cache

namespace net {

class IOBufferWithSize;

// A class for dumping the contents of a disk cache to a series of text
// files.  The files will contain the response headers, followed by the
// response body, as if the HTTP response were written directly to disk.
class SimpleCacheDumper {
 public:
  SimpleCacheDumper(base::FilePath input_path, base::FilePath output_path);
  ~SimpleCacheDumper();

  // Dumps the cache to disk. Returns OK if the operation was successful,
  // and a net error code otherwise.
  int Run();

 private:
  enum State {
    STATE_NONE,
    STATE_CREATE_CACHE,
    STATE_CREATE_CACHE_COMPLETE,
    STATE_OPEN_ENTRY,
    STATE_OPEN_ENTRY_COMPLETE,
    STATE_CREATE_ENTRY,
    STATE_CREATE_ENTRY_COMPLETE,
    STATE_READ_HEADERS,
    STATE_READ_HEADERS_COMPLETE,
    STATE_WRITE_HEADERS,
    STATE_WRITE_HEADERS_COMPLETE,
    STATE_READ_BODY,
    STATE_READ_BODY_COMPLETE,
    STATE_WRITE_BODY,
    STATE_WRITE_BODY_COMPLETE,
    STATE_DONE,
  };

  int DoLoop(int rv);

  int DoCreateCache();
  int DoCreateCacheComplete(int rv);
  int DoOpenEntry();
  int DoOpenEntryComplete(int rv);
  int DoCreateEntry();
  int DoCreateEntryComplete(int rv);
  int DoReadHeaders();
  int DoReadHeadersComplete(int rv);
  int DoWriteHeaders();
  int DoWriteHeadersComplete(int rv);
  int DoReadBody();
  int DoReadBodyComplete(int rv);
  int DoWriteBody();
  int DoWriteBodyComplete(int rv);
  int DoDone();

  void OnIOComplete(int rv);

  State state_;
  base::FilePath input_path_;
  base::FilePath output_path_;
  scoped_ptr<disk_cache::Backend> cache_;
  scoped_ptr<DiskDumper> writer_;
  base::Thread* cache_thread_;
  void* iter_;
  disk_cache::Entry* src_entry_;
  disk_cache::Entry* dst_entry_;
  CompletionCallback io_callback_;
  scoped_refptr<IOBufferWithSize> buf_;
  int rv_;

  DISALLOW_COPY_AND_ASSIGN(SimpleCacheDumper);
};

}  // namespace net

#endif  // NET_TOOLS_DUMP_CACHE_SIMPLE_CACHE_DUMPER_H_
