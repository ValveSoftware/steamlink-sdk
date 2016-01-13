// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_STREAMS_STREAM_HANDLE_IMPL_H_
#define CONTENT_BROWSER_STREAMS_STREAM_HANDLE_IMPL_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "content/public/browser/stream_handle.h"

namespace base {
class MessageLoopProxy;
}

namespace content {

class Stream;

class StreamHandleImpl : public StreamHandle {
 public:
  StreamHandleImpl(const base::WeakPtr<Stream>& stream,
                   const GURL& original_url,
                   const std::string& mime_type,
                   scoped_refptr<net::HttpResponseHeaders> response_headers);
  virtual ~StreamHandleImpl();

 private:
  // StreamHandle overrides
  virtual const GURL& GetURL() OVERRIDE;
  virtual const GURL& GetOriginalURL() OVERRIDE;
  virtual const std::string& GetMimeType() OVERRIDE;
  virtual scoped_refptr<net::HttpResponseHeaders> GetResponseHeaders() OVERRIDE;
  virtual void AddCloseListener(const base::Closure& callback) OVERRIDE;

  base::WeakPtr<Stream> stream_;
  GURL url_;
  GURL original_url_;
  std::string mime_type_;
  scoped_refptr<net::HttpResponseHeaders> response_headers_;
  base::MessageLoopProxy* stream_message_loop_;
  std::vector<base::Closure> close_listeners_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_STREAMS_STREAM_HANDLE_IMPL_H_

