// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_FILEAPI_WEBFILESYSTEM_IMPL_H_
#define CONTENT_CHILD_FILEAPI_WEBFILESYSTEM_IMPL_H_

#include <map>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/threading/non_thread_safe.h"
#include "content/child/worker_task_runner.h"
#include "third_party/WebKit/public/platform/WebFileSystem.h"

namespace base {
class MessageLoopProxy;
class WaitableEvent;
}

namespace blink {
class WebURL;
class WebFileWriter;
class WebFileWriterClient;
}

namespace content {

class WebFileSystemImpl : public blink::WebFileSystem,
                          public WorkerTaskRunner::Observer,
                          public base::NonThreadSafe {
 public:
  class WaitableCallbackResults;

  // Returns thread-specific instance.  If non-null |main_thread_loop|
  // is given and no thread-specific instance has been created it may
  // create a new instance.
  static WebFileSystemImpl* ThreadSpecificInstance(
      base::MessageLoopProxy* main_thread_loop);

  // Deletes thread-specific instance (if exists). For workers it deletes
  // itself in OnWorkerRunLoopStopped(), but for an instance created on the
  // main thread this method must be called.
  static void DeleteThreadSpecificInstance();

  explicit WebFileSystemImpl(base::MessageLoopProxy* main_thread_loop);
  virtual ~WebFileSystemImpl();

  // WorkerTaskRunner::Observer implementation.
  virtual void OnWorkerRunLoopStopped() OVERRIDE;

  // WebFileSystem implementation.
  virtual void openFileSystem(
      const blink::WebURL& storage_partition,
      const blink::WebFileSystemType type,
      blink::WebFileSystemCallbacks);
  virtual void resolveURL(
      const blink::WebURL& filesystem_url,
      blink::WebFileSystemCallbacks) OVERRIDE;
  virtual void deleteFileSystem(
      const blink::WebURL& storage_partition,
      const blink::WebFileSystemType type,
      blink::WebFileSystemCallbacks);
  virtual void move(
      const blink::WebURL& src_path,
      const blink::WebURL& dest_path,
      blink::WebFileSystemCallbacks) OVERRIDE;
  virtual void copy(
      const blink::WebURL& src_path,
      const blink::WebURL& dest_path,
      blink::WebFileSystemCallbacks) OVERRIDE;
  virtual void remove(
      const blink::WebURL& path,
      blink::WebFileSystemCallbacks) OVERRIDE;
  virtual void removeRecursively(
      const blink::WebURL& path,
      blink::WebFileSystemCallbacks) OVERRIDE;
  virtual void readMetadata(
      const blink::WebURL& path,
      blink::WebFileSystemCallbacks) OVERRIDE;
  virtual void createFile(
      const blink::WebURL& path,
      bool exclusive,
      blink::WebFileSystemCallbacks) OVERRIDE;
  virtual void createDirectory(
      const blink::WebURL& path,
      bool exclusive,
      blink::WebFileSystemCallbacks) OVERRIDE;
  virtual void fileExists(
      const blink::WebURL& path,
      blink::WebFileSystemCallbacks) OVERRIDE;
  virtual void directoryExists(
      const blink::WebURL& path,
      blink::WebFileSystemCallbacks) OVERRIDE;
  virtual int readDirectory(
      const blink::WebURL& path,
      blink::WebFileSystemCallbacks) OVERRIDE;
  virtual void createFileWriter(
      const blink::WebURL& path,
      blink::WebFileWriterClient*,
      blink::WebFileSystemCallbacks) OVERRIDE;
  virtual void createSnapshotFileAndReadMetadata(
      const blink::WebURL& path,
      blink::WebFileSystemCallbacks);
  virtual bool waitForAdditionalResult(int callbacksId);

  int RegisterCallbacks(const blink::WebFileSystemCallbacks& callbacks);
  blink::WebFileSystemCallbacks GetCallbacks(int callbacks_id);
  void UnregisterCallbacks(int callbacks_id);

 private:
  typedef std::map<int, blink::WebFileSystemCallbacks> CallbacksMap;
  typedef std::map<int, scoped_refptr<WaitableCallbackResults> >
      WaitableCallbackResultsMap;

  WaitableCallbackResults* MaybeCreateWaitableResults(
      const blink::WebFileSystemCallbacks& callbacks, int callbacks_id);

  scoped_refptr<base::MessageLoopProxy> main_thread_loop_;

  CallbacksMap callbacks_;
  int next_callbacks_id_;
  WaitableCallbackResultsMap waitable_results_;

  DISALLOW_COPY_AND_ASSIGN(WebFileSystemImpl);
};

}  // namespace content

#endif  // CONTENT_CHILD_FILEAPI_WEBFILESYSTEM_IMPL_H_
