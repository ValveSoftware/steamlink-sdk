// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_FILEAPI_COPY_OR_MOVE_OPERATION_DELEGATE_H_
#define WEBKIT_BROWSER_FILEAPI_COPY_OR_MOVE_OPERATION_DELEGATE_H_

#include <set>
#include <stack>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "webkit/browser/fileapi/recursive_operation_delegate.h"

namespace net {
class DrainableIOBuffer;
class IOBufferWithSize;
}

namespace webkit_blob {
class FileStreamReader;
class ShareableFileReference;
}

namespace fileapi {

class CopyOrMoveFileValidator;
class FileStreamWriter;

// A delegate class for recursive copy or move operations.
class CopyOrMoveOperationDelegate
    : public RecursiveOperationDelegate {
 public:
  class CopyOrMoveImpl;
  typedef FileSystemOperation::CopyProgressCallback CopyProgressCallback;
  typedef FileSystemOperation::CopyOrMoveOption CopyOrMoveOption;

  enum OperationType {
    OPERATION_COPY,
    OPERATION_MOVE
  };

  // Helper to copy a file by reader and writer streams.
  // Export for testing.
  class WEBKIT_STORAGE_BROWSER_EXPORT StreamCopyHelper {
   public:
    StreamCopyHelper(
        scoped_ptr<webkit_blob::FileStreamReader> reader,
        scoped_ptr<FileStreamWriter> writer,
        bool need_flush,
        int buffer_size,
        const FileSystemOperation::CopyFileProgressCallback&
            file_progress_callback,
        const base::TimeDelta& min_progress_callback_invocation_span);
    ~StreamCopyHelper();

    void Run(const StatusCallback& callback);

    // Requests cancelling. After the cancelling is done, |callback| passed to
    // Run will be called.
    void Cancel();

   private:
    // Reads the content from the |reader_|.
    void Read(const StatusCallback& callback);
    void DidRead(const StatusCallback& callback, int result);

    // Writes the content in |buffer| to |writer_|.
    void Write(const StatusCallback& callback,
               scoped_refptr<net::DrainableIOBuffer> buffer);
    void DidWrite(const StatusCallback& callback,
                  scoped_refptr<net::DrainableIOBuffer> buffer, int result);

    // Flushes the written content in |writer_|.
    void Flush(const StatusCallback& callback, bool is_eof);
    void DidFlush(const StatusCallback& callback, bool is_eof, int result);

    scoped_ptr<webkit_blob::FileStreamReader> reader_;
    scoped_ptr<FileStreamWriter> writer_;
    const bool need_flush_;
    FileSystemOperation::CopyFileProgressCallback file_progress_callback_;
    scoped_refptr<net::IOBufferWithSize> io_buffer_;
    int64 num_copied_bytes_;
    int64 previous_flush_offset_;
    base::Time last_progress_callback_invocation_time_;
    base::TimeDelta min_progress_callback_invocation_span_;
    bool cancel_requested_;
    base::WeakPtrFactory<StreamCopyHelper> weak_factory_;
    DISALLOW_COPY_AND_ASSIGN(StreamCopyHelper);
  };

  CopyOrMoveOperationDelegate(
      FileSystemContext* file_system_context,
      const FileSystemURL& src_root,
      const FileSystemURL& dest_root,
      OperationType operation_type,
      CopyOrMoveOption option,
      const CopyProgressCallback& progress_callback,
      const StatusCallback& callback);
  virtual ~CopyOrMoveOperationDelegate();

  // RecursiveOperationDelegate overrides:
  virtual void Run() OVERRIDE;
  virtual void RunRecursively() OVERRIDE;
  virtual void ProcessFile(const FileSystemURL& url,
                           const StatusCallback& callback) OVERRIDE;
  virtual void ProcessDirectory(const FileSystemURL& url,
                                const StatusCallback& callback) OVERRIDE;
  virtual void PostProcessDirectory(const FileSystemURL& url,
                                    const StatusCallback& callback) OVERRIDE;


 protected:
  virtual void OnCancel() OVERRIDE;

 private:
  void DidCopyOrMoveFile(const FileSystemURL& src_url,
                         const FileSystemURL& dest_url,
                         const StatusCallback& callback,
                         CopyOrMoveImpl* impl,
                         base::File::Error error);
  void DidTryRemoveDestRoot(const StatusCallback& callback,
                            base::File::Error error);
  void ProcessDirectoryInternal(const FileSystemURL& src_url,
                                const FileSystemURL& dest_url,
                                const StatusCallback& callback);
  void DidCreateDirectory(const FileSystemURL& src_url,
                          const FileSystemURL& dest_url,
                          const StatusCallback& callback,
                          base::File::Error error);
  void PostProcessDirectoryAfterGetMetadata(
      const FileSystemURL& src_url,
      const StatusCallback& callback,
      base::File::Error error,
      const base::File::Info& file_info);
  void PostProcessDirectoryAfterTouchFile(const FileSystemURL& src_url,
                                          const StatusCallback& callback,
                                          base::File::Error error);
  void DidRemoveSourceForMove(const StatusCallback& callback,
                              base::File::Error error);

  void OnCopyFileProgress(const FileSystemURL& src_url, int64 size);
  FileSystemURL CreateDestURL(const FileSystemURL& src_url) const;

  FileSystemURL src_root_;
  FileSystemURL dest_root_;
  bool same_file_system_;
  OperationType operation_type_;
  CopyOrMoveOption option_;
  CopyProgressCallback progress_callback_;
  StatusCallback callback_;

  std::set<CopyOrMoveImpl*> running_copy_set_;
  base::WeakPtrFactory<CopyOrMoveOperationDelegate> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CopyOrMoveOperationDelegate);
};

}  // namespace fileapi

#endif  // WEBKIT_BROWSER_FILEAPI_COPY_OR_MOVE_OPERATION_DELEGATE_H_
