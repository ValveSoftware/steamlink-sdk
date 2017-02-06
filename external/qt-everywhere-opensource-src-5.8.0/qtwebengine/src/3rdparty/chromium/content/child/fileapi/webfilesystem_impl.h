// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_FILEAPI_WEBFILESYSTEM_IMPL_H_
#define CONTENT_CHILD_FILEAPI_WEBFILESYSTEM_IMPL_H_

#include <map>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/non_thread_safe.h"
#include "content/public/child/worker_thread.h"
#include "third_party/WebKit/public/platform/WebFileSystem.h"

namespace base {
class WaitableEvent;
class SingleThreadTaskRunner;
}

namespace blink {
class WebURL;
class WebFileWriter;
class WebFileWriterClient;
}

namespace content {

class WebFileSystemImpl : public blink::WebFileSystem,
                          public WorkerThread::Observer,
                          public base::NonThreadSafe {
 public:
  class WaitableCallbackResults;

  // Returns thread-specific instance.  If non-null |main_thread_loop|
  // is given and no thread-specific instance has been created it may
  // create a new instance.
  static WebFileSystemImpl* ThreadSpecificInstance(
      const scoped_refptr<base::SingleThreadTaskRunner>&
          main_thread_task_runner);

  // Deletes thread-specific instance (if exists). For workers it deletes
  // itself in WillStopCurrentWorkerThread(), but for an instance created on the
  // main thread this method must be called.
  static void DeleteThreadSpecificInstance();

  explicit WebFileSystemImpl(
      const scoped_refptr<base::SingleThreadTaskRunner>&
          main_thread_task_runner);
  ~WebFileSystemImpl() override;

  // WorkerThread::Observer implementation.
  void WillStopCurrentWorkerThread() override;

  // WebFileSystem implementation.
  void openFileSystem(const blink::WebURL& storage_partition,
                      const blink::WebFileSystemType type,
                      blink::WebFileSystemCallbacks) override;
  void resolveURL(const blink::WebURL& filesystem_url,
                  blink::WebFileSystemCallbacks) override;
  void deleteFileSystem(const blink::WebURL& storage_partition,
                        const blink::WebFileSystemType type,
                        blink::WebFileSystemCallbacks) override;
  void move(const blink::WebURL& src_path,
            const blink::WebURL& dest_path,
            blink::WebFileSystemCallbacks) override;
  void copy(const blink::WebURL& src_path,
            const blink::WebURL& dest_path,
            blink::WebFileSystemCallbacks) override;
  void remove(const blink::WebURL& path,
              blink::WebFileSystemCallbacks) override;
  void removeRecursively(const blink::WebURL& path,
                         blink::WebFileSystemCallbacks) override;
  void readMetadata(const blink::WebURL& path,
                    blink::WebFileSystemCallbacks) override;
  void createFile(const blink::WebURL& path,
                  bool exclusive,
                  blink::WebFileSystemCallbacks) override;
  void createDirectory(const blink::WebURL& path,
                       bool exclusive,
                       blink::WebFileSystemCallbacks) override;
  void fileExists(const blink::WebURL& path,
                  blink::WebFileSystemCallbacks) override;
  void directoryExists(const blink::WebURL& path,
                       blink::WebFileSystemCallbacks) override;
  int readDirectory(const blink::WebURL& path,
                    blink::WebFileSystemCallbacks) override;
  void createFileWriter(const blink::WebURL& path,
                        blink::WebFileWriterClient*,
                        blink::WebFileSystemCallbacks) override;
  void createSnapshotFileAndReadMetadata(
      const blink::WebURL& path,
      blink::WebFileSystemCallbacks) override;
  bool waitForAdditionalResult(int callbacksId) override;

  int RegisterCallbacks(const blink::WebFileSystemCallbacks& callbacks);
  blink::WebFileSystemCallbacks GetCallbacks(int callbacks_id);
  void UnregisterCallbacks(int callbacks_id);

 private:
  typedef std::map<int, blink::WebFileSystemCallbacks> CallbacksMap;
  typedef std::map<int, scoped_refptr<WaitableCallbackResults> >
      WaitableCallbackResultsMap;

  WaitableCallbackResults* MaybeCreateWaitableResults(
      const blink::WebFileSystemCallbacks& callbacks, int callbacks_id);

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  CallbacksMap callbacks_;
  int next_callbacks_id_;
  WaitableCallbackResultsMap waitable_results_;

  DISALLOW_COPY_AND_ASSIGN(WebFileSystemImpl);
};

}  // namespace content

#endif  // CONTENT_CHILD_FILEAPI_WEBFILESYSTEM_IMPL_H_
