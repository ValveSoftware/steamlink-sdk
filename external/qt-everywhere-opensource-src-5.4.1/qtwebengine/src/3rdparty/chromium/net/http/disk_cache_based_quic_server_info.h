// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_DISK_CACHE_BASED_QUIC_SERVER_INFO_H_
#define NET_HTTP_DISK_CACHE_BASED_QUIC_SERVER_INFO_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "net/base/completion_callback.h"
#include "net/disk_cache/disk_cache.h"
#include "net/quic/crypto/quic_server_info.h"

namespace net {

class HttpCache;
class IOBuffer;
class QuicServerId;

// DiskCacheBasedQuicServerInfo fetches information about a QUIC server from
// our standard disk cache. Since the information is defined to be
// non-sensitive, it's ok for us to keep it on disk.
class NET_EXPORT_PRIVATE DiskCacheBasedQuicServerInfo
    : public QuicServerInfo,
      public NON_EXPORTED_BASE(base::NonThreadSafe) {
 public:
  DiskCacheBasedQuicServerInfo(const QuicServerId& server_id,
                               HttpCache* http_cache);

  // QuicServerInfo implementation.
  virtual void Start() OVERRIDE;
  virtual int WaitForDataReady(const CompletionCallback& callback) OVERRIDE;
  virtual bool IsDataReady() OVERRIDE;
  virtual bool IsReadyToPersist() OVERRIDE;
  virtual void Persist() OVERRIDE;

 private:
  struct CacheOperationDataShim;
  enum State {
    GET_BACKEND,
    GET_BACKEND_COMPLETE,
    OPEN,
    OPEN_COMPLETE,
    READ,
    READ_COMPLETE,
    WAIT_FOR_DATA_READY_DONE,
    CREATE_OR_OPEN,
    CREATE_OR_OPEN_COMPLETE,
    WRITE,
    WRITE_COMPLETE,
    SET_DONE,
    NONE,
  };

  virtual ~DiskCacheBasedQuicServerInfo();

  std::string key() const;

  // The |unused| parameter is a small hack so that we can have the
  // CacheOperationDataShim object owned by the Callback that is created for
  // this method.  See comment above CacheOperationDataShim for details.
  void OnIOComplete(CacheOperationDataShim* unused, int rv);

  int DoLoop(int rv);

  int DoGetBackendComplete(int rv);
  int DoOpenComplete(int rv);
  int DoReadComplete(int rv);
  int DoWriteComplete(int rv);
  int DoCreateOrOpenComplete(int rv);

  int DoGetBackend();
  int DoOpen();
  int DoRead();
  int DoWrite();
  int DoCreateOrOpen();

  // DoWaitForDataReadyDone is the terminal state of the read operation.
  int DoWaitForDataReadyDone();

  // DoSetDone is the terminal state of the write operation.
  int DoSetDone();

  base::WeakPtrFactory<DiskCacheBasedQuicServerInfo> weak_factory_;
  CacheOperationDataShim* data_shim_;  // Owned by |io_callback_|.
  CompletionCallback io_callback_;
  State state_;
  bool ready_;
  bool found_entry_;  // Controls the behavior of DoCreateOrOpen.
  std::string new_data_;
  const QuicServerId server_id_;
  HttpCache* const http_cache_;
  disk_cache::Backend* backend_;
  disk_cache::Entry* entry_;
  CompletionCallback user_callback_;
  scoped_refptr<IOBuffer> read_buffer_;
  scoped_refptr<IOBuffer> write_buffer_;
  std::string data_;
};

}  // namespace net

#endif  // NET_HTTP_DISK_CACHE_BASED_QUIC_SERVER_INFO_H_
