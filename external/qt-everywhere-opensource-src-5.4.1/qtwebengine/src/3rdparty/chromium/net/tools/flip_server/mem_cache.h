// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_FLIP_SERVER_MEM_CACHE_H_
#define NET_TOOLS_FLIP_SERVER_MEM_CACHE_H_

#include <map>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "net/tools/balsa/balsa_headers.h"
#include "net/tools/balsa/balsa_visitor_interface.h"
#include "net/tools/flip_server/constants.h"

namespace net {

class StoreBodyAndHeadersVisitor : public BalsaVisitorInterface {
 public:
  void HandleError() { error_ = true; }

  // BalsaVisitorInterface:
  virtual void ProcessBodyInput(const char* input, size_t size) OVERRIDE {}
  virtual void ProcessBodyData(const char* input, size_t size) OVERRIDE;
  virtual void ProcessHeaderInput(const char* input, size_t size) OVERRIDE {}
  virtual void ProcessTrailerInput(const char* input, size_t size) OVERRIDE {}
  virtual void ProcessHeaders(const BalsaHeaders& headers) OVERRIDE {
    // nothing to do here-- we're assuming that the BalsaFrame has
    // been handed our headers.
  }
  virtual void ProcessRequestFirstLine(const char* line_input,
                                       size_t line_length,
                                       const char* method_input,
                                       size_t method_length,
                                       const char* request_uri_input,
                                       size_t request_uri_length,
                                       const char* version_input,
                                       size_t version_length) OVERRIDE {}
  virtual void ProcessResponseFirstLine(const char* line_input,
                                        size_t line_length,
                                        const char* version_input,
                                        size_t version_length,
                                        const char* status_input,
                                        size_t status_length,
                                        const char* reason_input,
                                        size_t reason_length) OVERRIDE {}
  virtual void ProcessChunkLength(size_t chunk_length) OVERRIDE {}
  virtual void ProcessChunkExtensions(const char* input, size_t size) OVERRIDE {
  }
  virtual void HeaderDone() OVERRIDE {}
  virtual void MessageDone() OVERRIDE {}
  virtual void HandleHeaderError(BalsaFrame* framer) OVERRIDE;
  virtual void HandleHeaderWarning(BalsaFrame* framer) OVERRIDE;
  virtual void HandleChunkingError(BalsaFrame* framer) OVERRIDE;
  virtual void HandleBodyError(BalsaFrame* framer) OVERRIDE;

  BalsaHeaders headers;
  std::string body;
  bool error_;
};

class FileData {
 public:
  FileData();
  FileData(const BalsaHeaders* headers,
           const std::string& filename,
           const std::string& body);
  ~FileData();

  BalsaHeaders* headers() { return headers_.get(); }
  const BalsaHeaders* headers() const { return headers_.get(); }

  const std::string& filename() { return filename_; }
  const std::string& body() { return body_; }

 private:
  scoped_ptr<BalsaHeaders> headers_;
  std::string filename_;
  std::string body_;

  DISALLOW_COPY_AND_ASSIGN(FileData);
};

class MemCacheIter {
 public:
  MemCacheIter()
      : file_data(NULL),
        priority(0),
        transformed_header(false),
        body_bytes_consumed(0),
        stream_id(0),
        max_segment_size(kInitialDataSendersThreshold),
        bytes_sent(0) {}
  explicit MemCacheIter(FileData* fd)
      : file_data(fd),
        priority(0),
        transformed_header(false),
        body_bytes_consumed(0),
        stream_id(0),
        max_segment_size(kInitialDataSendersThreshold),
        bytes_sent(0) {}
  FileData* file_data;
  int priority;
  bool transformed_header;
  size_t body_bytes_consumed;
  uint32 stream_id;
  uint32 max_segment_size;
  size_t bytes_sent;
};

class MemoryCache {
 public:
  typedef std::map<std::string, FileData*> Files;

 public:
  MemoryCache();
  virtual ~MemoryCache();

  void CloneFrom(const MemoryCache& mc);

  void AddFiles();

  // virtual for unittests
  virtual void ReadToString(const char* filename, std::string* output);

  void ReadAndStoreFileContents(const char* filename);

  FileData* GetFileData(const std::string& filename);

  bool AssignFileData(const std::string& filename, MemCacheIter* mci);

  // For unittests
  void InsertFile(const BalsaHeaders* headers,
                  const std::string& filename,
                  const std::string& body);

 private:
  void InsertFile(FileData* file_data);
  void ClearFiles();

  Files files_;
  std::string cwd_;
};

class NotifierInterface {
 public:
  virtual ~NotifierInterface() {}
  virtual void Notify() = 0;
};

}  // namespace net

#endif  // NET_TOOLS_FLIP_SERVER_MEM_CACHE_H_
