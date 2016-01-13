// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_FILEAPI_WEBFILEWRITER_IMPL_H_
#define CONTENT_CHILD_FILEAPI_WEBFILEWRITER_IMPL_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "content/child/fileapi/webfilewriter_base.h"

namespace content {

// An implementation of WebFileWriter for use in chrome renderers and workers.
class WebFileWriterImpl : public WebFileWriterBase,
                          public base::SupportsWeakPtr<WebFileWriterImpl> {
 public:
  enum Type {
    TYPE_SYNC,
    TYPE_ASYNC,
  };

  WebFileWriterImpl(const GURL& path,
                    blink::WebFileWriterClient* client,
                    Type type,
                    base::MessageLoopProxy* main_thread_loop);
  virtual ~WebFileWriterImpl();

 protected:
  // WebFileWriterBase overrides
  virtual void DoTruncate(const GURL& path, int64 offset) OVERRIDE;
  virtual void DoWrite(const GURL& path, const std::string& blob_id,
                       int64 offset) OVERRIDE;
  virtual void DoCancel() OVERRIDE;

 private:
  class WriterBridge;

  void RunOnMainThread(const base::Closure& closure);

  scoped_refptr<base::MessageLoopProxy> main_thread_loop_;
  scoped_refptr<WriterBridge> bridge_;
};

}  // namespace content

#endif  // CONTENT_CHILD_FILEAPI_WEBFILEWRITER_IMPL_H_
