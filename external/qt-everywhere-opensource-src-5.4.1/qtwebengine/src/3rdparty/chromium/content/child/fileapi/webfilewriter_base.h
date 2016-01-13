// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_FILEAPI_WEBFILEWRITER_BASE_H_
#define CONTENT_CHILD_FILEAPI_WEBFILEWRITER_BASE_H_

#include "base/files/file.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebFileWriter.h"
#include "url/gurl.h"

namespace blink {
class WebFileWriterClient;
class WebURL;
}

namespace content {

class CONTENT_EXPORT WebFileWriterBase
    : public NON_EXPORTED_BASE(blink::WebFileWriter) {
 public:
  WebFileWriterBase(const GURL& path, blink::WebFileWriterClient* client);
  virtual ~WebFileWriterBase();

  // WebFileWriter implementation
  virtual void truncate(long long length);
  virtual void write(long long position, const blink::WebString& id);
  virtual void cancel();

 protected:
  // This calls DidSucceed() or DidFail() based on the value of |error_code|.
  void DidFinish(base::File::Error error_code);

  void DidWrite(int64 bytes, bool complete);
  void DidSucceed();
  void DidFail(base::File::Error error_code);

  // Derived classes must provide these methods to asynchronously perform
  // the requested operation, and they must call the appropiate DidSomething
  // method upon completion and as progress is made in the Write case.
  virtual void DoTruncate(const GURL& path, int64 offset) = 0;
  virtual void DoWrite(const GURL& path,
                       const std::string& blob_id,
                       int64 offset) = 0;
  virtual void DoCancel() = 0;

 private:
  enum OperationType {
    kOperationNone,
    kOperationWrite,
    kOperationTruncate
  };

  enum CancelState {
    kCancelNotInProgress,
    kCancelSent,
    kCancelReceivedWriteResponse,
  };

  void FinishCancel();

  GURL path_;
  blink::WebFileWriterClient* client_;
  OperationType operation_;
  CancelState cancel_state_;
};

}  // namespace content

#endif  // CONTENT_CHILD_FILEAPI_WEBFILEWRITER_BASE_H_
