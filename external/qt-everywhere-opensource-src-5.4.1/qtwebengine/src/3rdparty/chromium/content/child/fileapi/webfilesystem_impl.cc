// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/fileapi/webfilesystem_impl.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_local.h"
#include "content/child/blink_glue.h"
#include "content/child/child_thread.h"
#include "content/child/fileapi/file_system_dispatcher.h"
#include "content/child/fileapi/webfilewriter_impl.h"
#include "content/child/worker_task_runner.h"
#include "content/common/fileapi/file_system_messages.h"
#include "third_party/WebKit/public/platform/WebFileInfo.h"
#include "third_party/WebKit/public/platform/WebFileSystemCallbacks.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebHeap.h"
#include "url/gurl.h"
#include "webkit/common/fileapi/directory_entry.h"
#include "webkit/common/fileapi/file_system_util.h"

using blink::WebFileInfo;
using blink::WebFileSystemCallbacks;
using blink::WebFileSystemEntry;
using blink::WebString;
using blink::WebURL;
using blink::WebVector;

namespace content {

class WebFileSystemImpl::WaitableCallbackResults
    : public base::RefCountedThreadSafe<WaitableCallbackResults> {
 public:
  WaitableCallbackResults()
      : results_available_event_(true /* manual_reset */,
                                 false /* initially_signaled */) {}

  void AddResultsAndSignal(const base::Closure& results_closure) {
    base::AutoLock lock(lock_);
    results_closures_.push_back(results_closure);
    results_available_event_.Signal();
  }

  void WaitAndRun() {
    {
      blink::WebHeap::SafePointScope safe_point;
      results_available_event_.Wait();
    }
    Run();
  }

  void Run() {
    std::vector<base::Closure> closures;
    {
      base::AutoLock lock(lock_);
      results_closures_.swap(closures);
      results_available_event_.Reset();
    }
    for (size_t i = 0; i < closures.size(); ++i)
      closures[i].Run();
  }

 private:
  friend class base::RefCountedThreadSafe<WaitableCallbackResults>;

  ~WaitableCallbackResults() {}

  base::Lock lock_;
  base::WaitableEvent results_available_event_;
  std::vector<base::Closure> results_closures_;
  DISALLOW_COPY_AND_ASSIGN(WaitableCallbackResults);
};

namespace {

typedef WebFileSystemImpl::WaitableCallbackResults WaitableCallbackResults;

base::LazyInstance<base::ThreadLocalPointer<WebFileSystemImpl> >::Leaky
    g_webfilesystem_tls = LAZY_INSTANCE_INITIALIZER;

void DidReceiveSnapshotFile(int request_id) {
  if (ChildThread::current())
    ChildThread::current()->Send(
        new FileSystemHostMsg_DidReceiveSnapshotFile(request_id));
}

int CurrentWorkerId() {
  return WorkerTaskRunner::Instance()->CurrentWorkerId();
}

template <typename Method, typename Params>
void CallDispatcherOnMainThread(
    base::MessageLoopProxy* loop,
    Method method, const Params& params,
    WaitableCallbackResults* waitable_results) {
  if (!loop->RunsTasksOnCurrentThread()) {
    loop->PostTask(FROM_HERE,
                   base::Bind(&CallDispatcherOnMainThread<Method, Params>,
                              make_scoped_refptr(loop), method, params,
                              scoped_refptr<WaitableCallbackResults>()));
    if (!waitable_results)
      return;
    waitable_results->WaitAndRun();
  }
  if (!ChildThread::current() ||
      !ChildThread::current()->file_system_dispatcher())
    return;

  DCHECK(!waitable_results);
  DispatchToMethod(ChildThread::current()->file_system_dispatcher(),
                   method, params);
}

enum CallbacksUnregisterMode {
  UNREGISTER_CALLBACKS,
  DO_NOT_UNREGISTER_CALLBACKS,
};

// Run WebFileSystemCallbacks's |method| with |params|.
template <typename Method, typename Params>
void RunCallbacks(int callbacks_id, Method method, const Params& params,
                  CallbacksUnregisterMode callbacks_unregister_mode) {
  WebFileSystemImpl* filesystem =
      WebFileSystemImpl::ThreadSpecificInstance(NULL);
  if (!filesystem)
    return;
  WebFileSystemCallbacks callbacks = filesystem->GetCallbacks(callbacks_id);
  if (callbacks_unregister_mode == UNREGISTER_CALLBACKS)
    filesystem->UnregisterCallbacks(callbacks_id);
  DispatchToMethod(&callbacks, method, params);
}

void DispatchResultsClosure(int thread_id, int callbacks_id,
                            WaitableCallbackResults* waitable_results,
                            const base::Closure& results_closure) {
  if (thread_id != CurrentWorkerId()) {
    if (waitable_results) {
      // If someone is waiting, this should result in running the closure.
      waitable_results->AddResultsAndSignal(results_closure);
      // In case no one is waiting, post a task to run the closure.
      WorkerTaskRunner::Instance()->PostTask(
          thread_id,
          base::Bind(&WaitableCallbackResults::Run,
                     make_scoped_refptr(waitable_results)));
      return;
    }
    WorkerTaskRunner::Instance()->PostTask(thread_id, results_closure);
    return;
  }
  results_closure.Run();
}

template <typename Method, typename Params>
void CallbackFileSystemCallbacks(
    int thread_id, int callbacks_id,
    WaitableCallbackResults* waitable_results,
    Method method, const Params& params,
    CallbacksUnregisterMode callbacks_unregister_mode) {
  DispatchResultsClosure(
      thread_id, callbacks_id, waitable_results,
      base::Bind(&RunCallbacks<Method, Params>, callbacks_id, method, params,
                 callbacks_unregister_mode));
}

//-----------------------------------------------------------------------------
// Callback adapters. Callbacks must be called on the original calling thread,
// so these callback adapters relay back the results to the calling thread
// if necessary.

void OpenFileSystemCallbackAdapter(
    int thread_id, int callbacks_id,
    WaitableCallbackResults* waitable_results,
    const std::string& name, const GURL& root) {
  CallbackFileSystemCallbacks(
      thread_id, callbacks_id, waitable_results,
      &WebFileSystemCallbacks::didOpenFileSystem,
      MakeTuple(base::UTF8ToUTF16(name), root),
      UNREGISTER_CALLBACKS);
}

void ResolveURLCallbackAdapter(
    int thread_id, int callbacks_id,
    WaitableCallbackResults* waitable_results,
    const fileapi::FileSystemInfo& info,
    const base::FilePath& file_path, bool is_directory) {
  base::FilePath normalized_path(
      fileapi::VirtualPath::GetNormalizedFilePath(file_path));
  CallbackFileSystemCallbacks(
      thread_id, callbacks_id, waitable_results,
      &WebFileSystemCallbacks::didResolveURL,
      MakeTuple(base::UTF8ToUTF16(info.name), info.root_url,
                static_cast<blink::WebFileSystemType>(info.mount_type),
                normalized_path.AsUTF16Unsafe(), is_directory),
      UNREGISTER_CALLBACKS);
}

void StatusCallbackAdapter(int thread_id, int callbacks_id,
                           WaitableCallbackResults* waitable_results,
                           base::File::Error error) {
  if (error == base::File::FILE_OK) {
    CallbackFileSystemCallbacks(
        thread_id, callbacks_id, waitable_results,
        &WebFileSystemCallbacks::didSucceed, MakeTuple(),
        UNREGISTER_CALLBACKS);
  } else {
    CallbackFileSystemCallbacks(
        thread_id, callbacks_id, waitable_results,
        &WebFileSystemCallbacks::didFail,
        MakeTuple(fileapi::FileErrorToWebFileError(error)),
        UNREGISTER_CALLBACKS);
  }
}

void ReadMetadataCallbackAdapter(int thread_id, int callbacks_id,
                                 WaitableCallbackResults* waitable_results,
                                 const base::File::Info& file_info) {
  WebFileInfo web_file_info;
  FileInfoToWebFileInfo(file_info, &web_file_info);
  CallbackFileSystemCallbacks(
      thread_id, callbacks_id, waitable_results,
      &WebFileSystemCallbacks::didReadMetadata,
      MakeTuple(web_file_info),
      UNREGISTER_CALLBACKS);
}

void ReadDirectoryCallbackAdapter(
    int thread_id, int callbacks_id, WaitableCallbackResults* waitable_results,
    const std::vector<fileapi::DirectoryEntry>& entries,
    bool has_more) {
  WebVector<WebFileSystemEntry> file_system_entries(entries.size());
  for (size_t i = 0; i < entries.size(); i++) {
    file_system_entries[i].name =
        base::FilePath(entries[i].name).AsUTF16Unsafe();
    file_system_entries[i].isDirectory = entries[i].is_directory;
  }
  CallbackFileSystemCallbacks(
      thread_id, callbacks_id, waitable_results,
      &WebFileSystemCallbacks::didReadDirectory,
      MakeTuple(file_system_entries, has_more),
      has_more ? DO_NOT_UNREGISTER_CALLBACKS : UNREGISTER_CALLBACKS);
}

void DidCreateFileWriter(
    int callbacks_id,
    const GURL& path,
    blink::WebFileWriterClient* client,
    base::MessageLoopProxy* main_thread_loop,
    const base::File::Info& file_info) {
  WebFileSystemImpl* filesystem =
      WebFileSystemImpl::ThreadSpecificInstance(NULL);
  if (!filesystem)
    return;

  WebFileSystemCallbacks callbacks = filesystem->GetCallbacks(callbacks_id);
  filesystem->UnregisterCallbacks(callbacks_id);

  if (file_info.is_directory || file_info.size < 0) {
    callbacks.didFail(blink::WebFileErrorInvalidState);
    return;
  }
  WebFileWriterImpl::Type type =
      callbacks.shouldBlockUntilCompletion() ?
          WebFileWriterImpl::TYPE_SYNC : WebFileWriterImpl::TYPE_ASYNC;
  callbacks.didCreateFileWriter(
      new WebFileWriterImpl(path, client, type, main_thread_loop),
      file_info.size);
}

void CreateFileWriterCallbackAdapter(
    int thread_id, int callbacks_id,
    WaitableCallbackResults* waitable_results,
    base::MessageLoopProxy* main_thread_loop,
    const GURL& path,
    blink::WebFileWriterClient* client,
    const base::File::Info& file_info) {
  DispatchResultsClosure(
      thread_id, callbacks_id, waitable_results,
      base::Bind(&DidCreateFileWriter, callbacks_id, path, client,
                 make_scoped_refptr(main_thread_loop), file_info));
}

void DidCreateSnapshotFile(
    int callbacks_id,
    base::MessageLoopProxy* main_thread_loop,
    const base::File::Info& file_info,
    const base::FilePath& platform_path,
    int request_id) {
  WebFileSystemImpl* filesystem =
      WebFileSystemImpl::ThreadSpecificInstance(NULL);
  if (!filesystem)
    return;

  WebFileSystemCallbacks callbacks = filesystem->GetCallbacks(callbacks_id);
  filesystem->UnregisterCallbacks(callbacks_id);

  WebFileInfo web_file_info;
  FileInfoToWebFileInfo(file_info, &web_file_info);
  web_file_info.platformPath = platform_path.AsUTF16Unsafe();
  callbacks.didCreateSnapshotFile(web_file_info);

  // TODO(michaeln,kinuko): Use ThreadSafeSender when Blob becomes
  // non-bridge model.
  main_thread_loop->PostTask(
      FROM_HERE, base::Bind(&DidReceiveSnapshotFile, request_id));
}

void CreateSnapshotFileCallbackAdapter(
    int thread_id, int callbacks_id,
    WaitableCallbackResults* waitable_results,
    base::MessageLoopProxy* main_thread_loop,
    const base::File::Info& file_info,
    const base::FilePath& platform_path,
    int request_id) {
  DispatchResultsClosure(
      thread_id, callbacks_id, waitable_results,
      base::Bind(&DidCreateSnapshotFile, callbacks_id,
                 make_scoped_refptr(main_thread_loop),
                 file_info, platform_path, request_id));
}

}  // namespace

//-----------------------------------------------------------------------------
// WebFileSystemImpl

WebFileSystemImpl* WebFileSystemImpl::ThreadSpecificInstance(
    base::MessageLoopProxy* main_thread_loop) {
  if (g_webfilesystem_tls.Pointer()->Get() || !main_thread_loop)
    return g_webfilesystem_tls.Pointer()->Get();
  WebFileSystemImpl* filesystem = new WebFileSystemImpl(main_thread_loop);
  if (WorkerTaskRunner::Instance()->CurrentWorkerId())
    WorkerTaskRunner::Instance()->AddStopObserver(filesystem);
  return filesystem;
}

void WebFileSystemImpl::DeleteThreadSpecificInstance() {
  DCHECK(!WorkerTaskRunner::Instance()->CurrentWorkerId());
  if (g_webfilesystem_tls.Pointer()->Get())
    delete g_webfilesystem_tls.Pointer()->Get();
}

WebFileSystemImpl::WebFileSystemImpl(base::MessageLoopProxy* main_thread_loop)
    : main_thread_loop_(main_thread_loop),
      next_callbacks_id_(1) {
  g_webfilesystem_tls.Pointer()->Set(this);
}

WebFileSystemImpl::~WebFileSystemImpl() {
  g_webfilesystem_tls.Pointer()->Set(NULL);
}

void WebFileSystemImpl::OnWorkerRunLoopStopped() {
  delete this;
}

void WebFileSystemImpl::openFileSystem(
    const blink::WebURL& storage_partition,
    blink::WebFileSystemType type,
    WebFileSystemCallbacks callbacks) {
  int callbacks_id = RegisterCallbacks(callbacks);
  scoped_refptr<WaitableCallbackResults> waitable_results =
      MaybeCreateWaitableResults(callbacks, callbacks_id);
  CallDispatcherOnMainThread(
      main_thread_loop_.get(),
      &FileSystemDispatcher::OpenFileSystem,
      MakeTuple(GURL(storage_partition),
                static_cast<fileapi::FileSystemType>(type),
                base::Bind(&OpenFileSystemCallbackAdapter,
                           CurrentWorkerId(), callbacks_id, waitable_results),
                base::Bind(&StatusCallbackAdapter,
                           CurrentWorkerId(), callbacks_id, waitable_results)),
      waitable_results.get());
}

void WebFileSystemImpl::resolveURL(
    const blink::WebURL& filesystem_url,
    WebFileSystemCallbacks callbacks) {
  int callbacks_id = RegisterCallbacks(callbacks);
  scoped_refptr<WaitableCallbackResults> waitable_results =
      MaybeCreateWaitableResults(callbacks, callbacks_id);
  CallDispatcherOnMainThread(
      main_thread_loop_.get(),
      &FileSystemDispatcher::ResolveURL,
      MakeTuple(GURL(filesystem_url),
                base::Bind(&ResolveURLCallbackAdapter,
                           CurrentWorkerId(), callbacks_id, waitable_results),
                base::Bind(&StatusCallbackAdapter,
                           CurrentWorkerId(), callbacks_id, waitable_results)),
      waitable_results.get());
}

void WebFileSystemImpl::deleteFileSystem(
    const blink::WebURL& storage_partition,
    blink::WebFileSystemType type,
    WebFileSystemCallbacks callbacks) {
  int callbacks_id = RegisterCallbacks(callbacks);
  scoped_refptr<WaitableCallbackResults> waitable_results =
      MaybeCreateWaitableResults(callbacks, callbacks_id);
  CallDispatcherOnMainThread(
      main_thread_loop_.get(),
      &FileSystemDispatcher::DeleteFileSystem,
      MakeTuple(GURL(storage_partition),
                static_cast<fileapi::FileSystemType>(type),
                base::Bind(&StatusCallbackAdapter,
                           CurrentWorkerId(), callbacks_id, waitable_results)),
      waitable_results.get());
}

void WebFileSystemImpl::move(
    const blink::WebURL& src_path,
    const blink::WebURL& dest_path,
    WebFileSystemCallbacks callbacks) {
  int callbacks_id = RegisterCallbacks(callbacks);
  scoped_refptr<WaitableCallbackResults> waitable_results =
      MaybeCreateWaitableResults(callbacks, callbacks_id);
  CallDispatcherOnMainThread(
      main_thread_loop_.get(),
      &FileSystemDispatcher::Move,
      MakeTuple(GURL(src_path), GURL(dest_path),
                base::Bind(&StatusCallbackAdapter,
                           CurrentWorkerId(), callbacks_id, waitable_results)),
      waitable_results.get());
}

void WebFileSystemImpl::copy(
    const blink::WebURL& src_path,
    const blink::WebURL& dest_path,
    WebFileSystemCallbacks callbacks) {
  int callbacks_id = RegisterCallbacks(callbacks);
  scoped_refptr<WaitableCallbackResults> waitable_results =
      MaybeCreateWaitableResults(callbacks, callbacks_id);
  CallDispatcherOnMainThread(
      main_thread_loop_.get(),
      &FileSystemDispatcher::Copy,
      MakeTuple(GURL(src_path), GURL(dest_path),
                base::Bind(&StatusCallbackAdapter,
                           CurrentWorkerId(), callbacks_id, waitable_results)),
      waitable_results.get());
}

void WebFileSystemImpl::remove(
    const blink::WebURL& path,
    WebFileSystemCallbacks callbacks) {
  int callbacks_id = RegisterCallbacks(callbacks);
  scoped_refptr<WaitableCallbackResults> waitable_results =
      MaybeCreateWaitableResults(callbacks, callbacks_id);
  CallDispatcherOnMainThread(
      main_thread_loop_.get(),
      &FileSystemDispatcher::Remove,
      MakeTuple(GURL(path), false /* recursive */,
                base::Bind(&StatusCallbackAdapter,
                           CurrentWorkerId(), callbacks_id, waitable_results)),
      waitable_results.get());
}

void WebFileSystemImpl::removeRecursively(
    const blink::WebURL& path,
    WebFileSystemCallbacks callbacks) {
  int callbacks_id = RegisterCallbacks(callbacks);
  scoped_refptr<WaitableCallbackResults> waitable_results =
      MaybeCreateWaitableResults(callbacks, callbacks_id);
  CallDispatcherOnMainThread(
      main_thread_loop_.get(),
      &FileSystemDispatcher::Remove,
      MakeTuple(GURL(path), true /* recursive */,
                base::Bind(&StatusCallbackAdapter,
                           CurrentWorkerId(), callbacks_id, waitable_results)),
      waitable_results.get());
}

void WebFileSystemImpl::readMetadata(
    const blink::WebURL& path,
    WebFileSystemCallbacks callbacks) {
  int callbacks_id = RegisterCallbacks(callbacks);
  scoped_refptr<WaitableCallbackResults> waitable_results =
      MaybeCreateWaitableResults(callbacks, callbacks_id);
  CallDispatcherOnMainThread(
      main_thread_loop_.get(),
      &FileSystemDispatcher::ReadMetadata,
      MakeTuple(GURL(path),
                base::Bind(&ReadMetadataCallbackAdapter,
                           CurrentWorkerId(), callbacks_id, waitable_results),
                base::Bind(&StatusCallbackAdapter,
                           CurrentWorkerId(), callbacks_id, waitable_results)),
      waitable_results.get());
}

void WebFileSystemImpl::createFile(
    const blink::WebURL& path,
    bool exclusive,
    WebFileSystemCallbacks callbacks) {
  int callbacks_id = RegisterCallbacks(callbacks);
  scoped_refptr<WaitableCallbackResults> waitable_results =
      MaybeCreateWaitableResults(callbacks, callbacks_id);
  CallDispatcherOnMainThread(
      main_thread_loop_.get(),
      &FileSystemDispatcher::CreateFile,
      MakeTuple(GURL(path), exclusive,
                base::Bind(&StatusCallbackAdapter,
                           CurrentWorkerId(), callbacks_id, waitable_results)),
      waitable_results.get());
}

void WebFileSystemImpl::createDirectory(
    const blink::WebURL& path,
    bool exclusive,
    WebFileSystemCallbacks callbacks) {
  int callbacks_id = RegisterCallbacks(callbacks);
  scoped_refptr<WaitableCallbackResults> waitable_results =
      MaybeCreateWaitableResults(callbacks, callbacks_id);
  CallDispatcherOnMainThread(
      main_thread_loop_.get(),
      &FileSystemDispatcher::CreateDirectory,
      MakeTuple(GURL(path), exclusive, false /* recursive */,
                base::Bind(&StatusCallbackAdapter,
                           CurrentWorkerId(), callbacks_id, waitable_results)),
      waitable_results.get());
}

void WebFileSystemImpl::fileExists(
    const blink::WebURL& path,
    WebFileSystemCallbacks callbacks) {
  int callbacks_id = RegisterCallbacks(callbacks);
  scoped_refptr<WaitableCallbackResults> waitable_results =
      MaybeCreateWaitableResults(callbacks, callbacks_id);
  CallDispatcherOnMainThread(
      main_thread_loop_.get(),
      &FileSystemDispatcher::Exists,
      MakeTuple(GURL(path), false /* directory */,
                base::Bind(&StatusCallbackAdapter,
                           CurrentWorkerId(), callbacks_id, waitable_results)),
      waitable_results.get());
}

void WebFileSystemImpl::directoryExists(
    const blink::WebURL& path,
    WebFileSystemCallbacks callbacks) {
  int callbacks_id = RegisterCallbacks(callbacks);
  scoped_refptr<WaitableCallbackResults> waitable_results =
      MaybeCreateWaitableResults(callbacks, callbacks_id);
  CallDispatcherOnMainThread(
      main_thread_loop_.get(),
      &FileSystemDispatcher::Exists,
      MakeTuple(GURL(path), true /* directory */,
                base::Bind(&StatusCallbackAdapter,
                           CurrentWorkerId(), callbacks_id, waitable_results)),
      waitable_results.get());
}

int WebFileSystemImpl::readDirectory(
    const blink::WebURL& path,
    WebFileSystemCallbacks callbacks) {
  int callbacks_id = RegisterCallbacks(callbacks);
  scoped_refptr<WaitableCallbackResults> waitable_results =
      MaybeCreateWaitableResults(callbacks, callbacks_id);
  CallDispatcherOnMainThread(
      main_thread_loop_.get(),
      &FileSystemDispatcher::ReadDirectory,
      MakeTuple(GURL(path),
                base::Bind(&ReadDirectoryCallbackAdapter,
                           CurrentWorkerId(), callbacks_id, waitable_results),
                base::Bind(&StatusCallbackAdapter,
                           CurrentWorkerId(), callbacks_id, waitable_results)),
      waitable_results.get());
  return callbacks_id;
}

void WebFileSystemImpl::createFileWriter(
    const WebURL& path,
    blink::WebFileWriterClient* client,
    WebFileSystemCallbacks callbacks) {
  int callbacks_id = RegisterCallbacks(callbacks);
  scoped_refptr<WaitableCallbackResults> waitable_results =
      MaybeCreateWaitableResults(callbacks, callbacks_id);
  CallDispatcherOnMainThread(
      main_thread_loop_.get(),
      &FileSystemDispatcher::ReadMetadata,
      MakeTuple(GURL(path),
                base::Bind(&CreateFileWriterCallbackAdapter,
                           CurrentWorkerId(), callbacks_id, waitable_results,
                           main_thread_loop_, GURL(path), client),
                base::Bind(&StatusCallbackAdapter,
                           CurrentWorkerId(), callbacks_id, waitable_results)),
      waitable_results.get());
}

void WebFileSystemImpl::createSnapshotFileAndReadMetadata(
    const blink::WebURL& path,
    WebFileSystemCallbacks callbacks) {
  int callbacks_id = RegisterCallbacks(callbacks);
  scoped_refptr<WaitableCallbackResults> waitable_results =
      MaybeCreateWaitableResults(callbacks, callbacks_id);
  CallDispatcherOnMainThread(
      main_thread_loop_.get(),
      &FileSystemDispatcher::CreateSnapshotFile,
      MakeTuple(GURL(path),
                base::Bind(&CreateSnapshotFileCallbackAdapter,
                           CurrentWorkerId(), callbacks_id, waitable_results,
                           main_thread_loop_),
                base::Bind(&StatusCallbackAdapter,
                           CurrentWorkerId(), callbacks_id, waitable_results)),
      waitable_results.get());
}

bool WebFileSystemImpl::waitForAdditionalResult(int callbacksId) {
  WaitableCallbackResultsMap::iterator found =
      waitable_results_.find(callbacksId);
  if (found == waitable_results_.end())
    return false;

  found->second->WaitAndRun();
  return true;
}

int WebFileSystemImpl::RegisterCallbacks(
    const WebFileSystemCallbacks& callbacks) {
  DCHECK(CalledOnValidThread());
  int id = next_callbacks_id_++;
  callbacks_[id] = callbacks;
  return id;
}

WebFileSystemCallbacks WebFileSystemImpl::GetCallbacks(int callbacks_id) {
  DCHECK(CalledOnValidThread());
  CallbacksMap::iterator found = callbacks_.find(callbacks_id);
  DCHECK(found != callbacks_.end());
  return found->second;
}

void WebFileSystemImpl::UnregisterCallbacks(int callbacks_id) {
  DCHECK(CalledOnValidThread());
  CallbacksMap::iterator found = callbacks_.find(callbacks_id);
  DCHECK(found != callbacks_.end());
  callbacks_.erase(found);

  waitable_results_.erase(callbacks_id);
}

WaitableCallbackResults* WebFileSystemImpl::MaybeCreateWaitableResults(
    const WebFileSystemCallbacks& callbacks, int callbacks_id) {
  if (!callbacks.shouldBlockUntilCompletion())
    return NULL;
  WaitableCallbackResults* results = new WaitableCallbackResults();
  waitable_results_[callbacks_id] = results;
  return results;
}

}  // namespace content
