// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/embedded_worker_dispatcher.h"

#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "content/child/child_process.h"
#include "content/child/scoped_child_process_reference.h"
#include "content/child/thread_safe_sender.h"
#include "content/child/worker_task_runner.h"
#include "content/common/devtools_messages.h"
#include "content/common/service_worker/embedded_worker_messages.h"
#include "content/public/common/content_client.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/service_worker/embedded_worker_context_client.h"
#include "content/renderer/service_worker/embedded_worker_devtools_agent.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebEmbeddedWorker.h"
#include "third_party/WebKit/public/web/WebEmbeddedWorkerStartData.h"

namespace content {

// A thin wrapper of WebEmbeddedWorker which also adds and releases process
// references automatically.
class EmbeddedWorkerDispatcher::WorkerWrapper {
 public:
  WorkerWrapper(blink::WebEmbeddedWorker* worker, int devtools_agent_route_id)
      : worker_(worker),
        dev_tools_agent_(
            new EmbeddedWorkerDevToolsAgent(worker, devtools_agent_route_id)) {}
  ~WorkerWrapper() {}

  blink::WebEmbeddedWorker* worker() { return worker_.get(); }

 private:
  ScopedChildProcessReference process_ref_;
  scoped_ptr<blink::WebEmbeddedWorker> worker_;
  scoped_ptr<EmbeddedWorkerDevToolsAgent> dev_tools_agent_;
};

EmbeddedWorkerDispatcher::EmbeddedWorkerDispatcher() : weak_factory_(this) {}

EmbeddedWorkerDispatcher::~EmbeddedWorkerDispatcher() {}

bool EmbeddedWorkerDispatcher::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(EmbeddedWorkerDispatcher, message)
    IPC_MESSAGE_HANDLER(EmbeddedWorkerMsg_StartWorker, OnStartWorker)
    IPC_MESSAGE_HANDLER(EmbeddedWorkerMsg_StopWorker, OnStopWorker)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void EmbeddedWorkerDispatcher::WorkerContextDestroyed(
    int embedded_worker_id) {
  RenderThreadImpl::current()->thread_safe_sender()->Send(
      new EmbeddedWorkerHostMsg_WorkerStopped(embedded_worker_id));
  workers_.Remove(embedded_worker_id);
}

void EmbeddedWorkerDispatcher::OnStartWorker(
    const EmbeddedWorkerMsg_StartWorker_Params& params) {
  DCHECK(!workers_.Lookup(params.embedded_worker_id));
  RenderThread::Get()->EnsureWebKitInitialized();
  scoped_ptr<WorkerWrapper> wrapper(
      new WorkerWrapper(blink::WebEmbeddedWorker::create(
                            new EmbeddedWorkerContextClient(
                                params.embedded_worker_id,
                                params.service_worker_version_id,
                                params.scope,
                                params.script_url,
                                params.worker_devtools_agent_route_id),
                            NULL),
                        params.worker_devtools_agent_route_id));

  blink::WebEmbeddedWorkerStartData start_data;
  start_data.scriptURL = params.script_url;
  start_data.userAgent = base::UTF8ToUTF16(GetContentClient()->GetUserAgent());
  start_data.startMode =
      params.pause_on_start ? blink::WebEmbeddedWorkerStartModePauseOnStart
                            : blink::WebEmbeddedWorkerStartModeDontPauseOnStart;

  wrapper->worker()->startWorkerContext(start_data);
  workers_.AddWithID(wrapper.release(), params.embedded_worker_id);
}

void EmbeddedWorkerDispatcher::OnStopWorker(int embedded_worker_id) {
  WorkerWrapper* wrapper = workers_.Lookup(embedded_worker_id);
  if (!wrapper) {
    LOG(WARNING) << "Got OnStopWorker for nonexistent worker";
    return;
  }

  // This should eventually call WorkerContextDestroyed. (We may need to post
  // a delayed task to forcibly abort the worker context if we find it
  // necessary)
  wrapper->worker()->terminateWorkerContext();
}

}  // namespace content
