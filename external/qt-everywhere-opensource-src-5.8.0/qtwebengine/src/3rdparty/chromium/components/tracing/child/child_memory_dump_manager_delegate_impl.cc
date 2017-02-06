// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/child/child_memory_dump_manager_delegate_impl.h"

#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "components/tracing/child/child_trace_message_filter.h"
#include "components/tracing/common/process_metrics_memory_dump_provider.h"

namespace tracing {

namespace {
void AbortDumpRequest(const base::trace_event::MemoryDumpRequestArgs& args,
                      const base::trace_event::MemoryDumpCallback& callback) {
  if (!callback.is_null())
    callback.Run(args.dump_guid, false /* success */);
}
}  // namespace

// static
ChildMemoryDumpManagerDelegateImpl*
ChildMemoryDumpManagerDelegateImpl::GetInstance() {
  return base::Singleton<
      ChildMemoryDumpManagerDelegateImpl,
      base::LeakySingletonTraits<ChildMemoryDumpManagerDelegateImpl>>::get();
}

ChildMemoryDumpManagerDelegateImpl::ChildMemoryDumpManagerDelegateImpl()
    : ctmf_(nullptr),
      tracing_process_id_(
          base::trace_event::MemoryDumpManager::kInvalidTracingProcessId) {
}

ChildMemoryDumpManagerDelegateImpl::~ChildMemoryDumpManagerDelegateImpl() {}

void ChildMemoryDumpManagerDelegateImpl::SetChildTraceMessageFilter(
    ChildTraceMessageFilter* ctmf) {
  const auto& task_runner = ctmf ? (ctmf->ipc_task_runner()) : nullptr;
  // Check that we are either registering the CTMF or tearing it down, but not
  // replacing a valid instance with another one (should never happen).
  DCHECK(!ctmf_ || (!ctmf && ctmf_task_runner_));
  ctmf_ = ctmf;

  {
    base::AutoLock lock(lock_);
    ctmf_task_runner_ = task_runner;
  }

  if (ctmf) {
    base::trace_event::MemoryDumpManager::GetInstance()->Initialize(
      this /* delegate */, false /* is_coordinator */);

#if !defined(OS_LINUX) && !defined(OS_NACL)
    // On linux the browser process takes care of dumping process metrics.
    // The child process is not allowed to do so due to BPF sandbox.
    tracing::ProcessMetricsMemoryDumpProvider::RegisterForProcess(
        base::kNullProcessId);
#endif
  }
}

// Invoked in child processes by the MemoryDumpManager.
void ChildMemoryDumpManagerDelegateImpl::RequestGlobalMemoryDump(
    const base::trace_event::MemoryDumpRequestArgs& args,
    const base::trace_event::MemoryDumpCallback& callback) {
  // RequestGlobalMemoryDump can be called on any thread, cannot access
  // ctmf_task_runner_ as it could be racy.
  scoped_refptr<base::SingleThreadTaskRunner> ctmf_task_runner;
  {
    base::AutoLock lock(lock_);
    ctmf_task_runner = ctmf_task_runner_;
  }

  // Bail out if we receive a dump request from the manager before the
  // ChildTraceMessageFilter has been initialized.
  if (!ctmf_task_runner) {
    VLOG(1) << base::trace_event::MemoryDumpManager::kLogPrefix
            << " failed because child trace message filter hasn't been"
            << " initialized";
    return AbortDumpRequest(args, callback);
  }

  // Make sure we access |ctmf_| only on the thread where it lives to avoid
  // races on shutdown.
  if (!ctmf_task_runner->BelongsToCurrentThread()) {
    const bool did_post_task = ctmf_task_runner->PostTask(
        FROM_HERE,
        base::Bind(&ChildMemoryDumpManagerDelegateImpl::RequestGlobalMemoryDump,
                   base::Unretained(this), args, callback));
    if (!did_post_task)
      return AbortDumpRequest(args, callback);
    return;
  }

  // The ChildTraceMessageFilter could have been destroyed while hopping on the
  // right thread. If this is the case, bail out.
  if (!ctmf_) {
    VLOG(1) << base::trace_event::MemoryDumpManager::kLogPrefix
            << " failed because child trace message filter was"
            << " destroyed while switching threads";
    return AbortDumpRequest(args, callback);
  }

  // Send the request up to the browser process' MessageDumpmanager.
  ctmf_->SendGlobalMemoryDumpRequest(args, callback);
}

uint64_t ChildMemoryDumpManagerDelegateImpl::GetTracingProcessId() const {
  return tracing_process_id_;
}

}  // namespace tracing
