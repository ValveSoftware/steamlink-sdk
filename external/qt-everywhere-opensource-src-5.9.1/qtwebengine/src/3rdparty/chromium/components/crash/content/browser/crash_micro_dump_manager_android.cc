// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/content/browser/crash_micro_dump_manager_android.h"

#include <unistd.h>

#include "base/bind.h"
#include "base/logging.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"

using content::BrowserThread;

namespace breakpad {

// static
CrashMicroDumpManager* CrashMicroDumpManager::GetInstance() {
  return base::Singleton<CrashMicroDumpManager>::get();
}

CrashMicroDumpManager::CrashMicroDumpManager() : weak_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  notification_registrar_.Add(this,
                              content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
                              content::NotificationService::AllSources());
  notification_registrar_.Add(this,
                              content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                              content::NotificationService::AllSources());
}

CrashMicroDumpManager::~CrashMicroDumpManager() {
  base::AutoLock auto_lock(child_process_id_to_pipe_lock_);
  child_process_id_to_pipe_.clear();
}

base::ScopedFD CrashMicroDumpManager::CreateCrashInfoChannel(
    int child_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::PROCESS_LAUNCHER);
  std::unique_ptr<base::SyncSocket> local_pipe(new base::SyncSocket());
  std::unique_ptr<base::SyncSocket> child_pipe(new base::SyncSocket());
  if (!base::SyncSocket::CreatePair(local_pipe.get(), child_pipe.get()))
    return base::ScopedFD();
  {
    base::AutoLock auto_lock(child_process_id_to_pipe_lock_);
    DCHECK(!ContainsKey(child_process_id_to_pipe_, child_process_id));
    child_process_id_to_pipe_[child_process_id] = std::move(local_pipe);
  }
  return base::ScopedFD(dup(child_pipe->handle()));
}

void CrashMicroDumpManager::HandleChildTerminationOnFileThread(
    int child_process_id,
    base::TerminationStatus termination_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  std::unique_ptr<base::SyncSocket> pipe;
  {
    base::AutoLock auto_lock(child_process_id_to_pipe_lock_);
    const auto& iter = child_process_id_to_pipe_.find(child_process_id);
    if (iter == child_process_id_to_pipe_.end()) {
      // We might get a NOTIFICATION_RENDERER_PROCESS_TERMINATED and a
      // NOTIFICATION_RENDERER_PROCESS_CLOSED.
      return;
    }
    pipe = std::move(iter->second);
    child_process_id_to_pipe_.erase(iter);
  }
  DCHECK(pipe->handle() != base::SyncSocket::kInvalidHandle);

  if (termination_status == base::TERMINATION_STATUS_NORMAL_TERMINATION)
    return;
  if (pipe->Peek() >= sizeof(int)) {
    int exit_code;
    pipe->Receive(&exit_code, sizeof(exit_code));
    LOG(FATAL) << "Renderer process crash detected (code " << exit_code
               << "). Terminating browser.";
  } else {
    // The child process hasn't written anything into the pipe. This implies
    // that it was terminated via SIGKILL by the low memory killer, and thus we
    // need to perform a clean exit.
    exit(0);
  }
}

void CrashMicroDumpManager::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  content::RenderProcessHost* rph =
      content::Source<content::RenderProcessHost>(source).ptr();
  // In case of a normal termination we just need to close the pipe_fd we kept
  // open.
  base::TerminationStatus termination_status =
      base::TERMINATION_STATUS_NORMAL_TERMINATION;
  switch (type) {
    case content::NOTIFICATION_RENDERER_PROCESS_TERMINATED: {
      // NOTIFICATION_RENDERER_PROCESS_TERMINATED is sent when the renderer
      // process is cleanly shutdown.
      break;
    }
    case content::NOTIFICATION_RENDERER_PROCESS_CLOSED: {
      // Android fast shutdowns is a known case where the renderer is
      // intentionally killed when we are done with it.
      if (!rph->FastShutdownStarted()) {
        content::RenderProcessHost::RendererClosedDetails* process_details =
            content::Details<content::RenderProcessHost::RendererClosedDetails>(
                details).ptr();
        termination_status = process_details->status;
      }
      break;
    }
    default:
      NOTREACHED();
      return;
  }
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&CrashMicroDumpManager::HandleChildTerminationOnFileThread,
                 weak_factory_.GetWeakPtr(), rph->GetID(),
                 termination_status));
}

}  // namespace breakpad
