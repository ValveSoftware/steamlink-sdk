// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <map>

#include "base/callback.h"
#include "base/files/file.h"
#include "base/memory/ref_counted_delete_on_message_loop.h"
#include "base/memory/ref_counted_memory.h"

namespace content {
namespace devtools {

class DevToolsIOContext {
 public:
  class Stream : public base::RefCountedDeleteOnMessageLoop<Stream> {
   public:
    enum Status {
      StatusSuccess,
      StatusEOF,
      StatusFailure
    };

    using ReadCallback = base::Callback<
        void(const scoped_refptr<base::RefCountedString>& data, int status)>;

    void Read(off_t position, size_t max_size, ReadCallback callback);
    void Append(const scoped_refptr<base::RefCountedString>& data);
    const std::string& handle() const { return handle_; }

   private:
    Stream();
    ~Stream();
    friend class DevToolsIOContext;
    friend class base::RefCountedDeleteOnMessageLoop<Stream>;
    friend class base::DeleteHelper<Stream>;

    void ReadOnFileThread(off_t pos, size_t max_size, ReadCallback callback);
    void AppendOnFileThread(const scoped_refptr<base::RefCountedString>& data);
    bool InitOnFileThreadIfNeeded();

    const std::string handle_;
    base::File file_;
    bool had_errors_;
    off_t last_read_pos_;
  };

  DevToolsIOContext();
  ~DevToolsIOContext();

  scoped_refptr<Stream> CreateTempFileBackedStream();
  scoped_refptr<Stream> GetByHandle(const std::string& handle);
  bool Close(const std::string& handle);
  void DiscardAllStreams();

 private:
  using StreamsMap = std::map<std::string, scoped_refptr<Stream>>;
  StreamsMap streams_;
};

}  // namespace devtools
}  // namespace content
