// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/mhtml_generation_manager.h"

#include "base/bind.h"
#include "base/files/file.h"
#include "base/stl_util.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/web_contents.h"
#include "content/common/view_messages.h"

namespace content {

class MHTMLGenerationManager::Job : public RenderProcessHostObserver {
 public:
  Job();
  virtual ~Job();

  void SetWebContents(WebContents* web_contents);

  base::File browser_file() { return browser_file_.Pass(); }
  void set_browser_file(base::File file) { browser_file_ = file.Pass(); }

  int process_id() { return process_id_; }
  int routing_id() { return routing_id_; }

  GenerateMHTMLCallback callback() { return callback_; }
  void set_callback(GenerateMHTMLCallback callback) { callback_ = callback; }

  // RenderProcessHostObserver:
  virtual void RenderProcessExited(RenderProcessHost* host,
                                   base::ProcessHandle handle,
                                   base::TerminationStatus status,
                                   int exit_code) OVERRIDE;
  virtual void RenderProcessHostDestroyed(RenderProcessHost* host) OVERRIDE;


 private:
  // The handle to the file the MHTML is saved to for the browser process.
  base::File browser_file_;

  // The IDs mapping to a specific contents.
  int process_id_;
  int routing_id_;

  // The callback to call once generation is complete.
  GenerateMHTMLCallback callback_;

  // The RenderProcessHost being observed, or NULL if none is.
  RenderProcessHost* host_;
  DISALLOW_COPY_AND_ASSIGN(Job);
};

MHTMLGenerationManager::Job::Job()
    : process_id_(-1),
      routing_id_(-1),
      host_(NULL) {
}

MHTMLGenerationManager::Job::~Job() {
  if (host_)
    host_->RemoveObserver(this);
}

void MHTMLGenerationManager::Job::SetWebContents(WebContents* web_contents) {
  process_id_ = web_contents->GetRenderProcessHost()->GetID();
  routing_id_ = web_contents->GetRenderViewHost()->GetRoutingID();
  host_ = web_contents->GetRenderProcessHost();
  host_->AddObserver(this);
}

void MHTMLGenerationManager::Job::RenderProcessExited(
    RenderProcessHost* host,
    base::ProcessHandle handle,
    base::TerminationStatus status,
    int exit_code) {
  MHTMLGenerationManager::GetInstance()->RenderProcessExited(this);
}

void MHTMLGenerationManager::Job::RenderProcessHostDestroyed(
    RenderProcessHost* host) {
  host_ = NULL;
}

MHTMLGenerationManager* MHTMLGenerationManager::GetInstance() {
  return Singleton<MHTMLGenerationManager>::get();
}

MHTMLGenerationManager::MHTMLGenerationManager() {
}

MHTMLGenerationManager::~MHTMLGenerationManager() {
  STLDeleteValues(&id_to_job_);
}

void MHTMLGenerationManager::SaveMHTML(WebContents* web_contents,
                                       const base::FilePath& file,
                                       const GenerateMHTMLCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  int job_id = NewJob(web_contents, callback);

  base::ProcessHandle renderer_process =
      web_contents->GetRenderProcessHost()->GetHandle();
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      base::Bind(&MHTMLGenerationManager::CreateFile, base::Unretained(this),
                 job_id, file, renderer_process));
}

void MHTMLGenerationManager::StreamMHTML(
    WebContents* web_contents,
    base::File browser_file,
    const GenerateMHTMLCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  int job_id = NewJob(web_contents, callback);

  base::ProcessHandle renderer_process =
      web_contents->GetRenderProcessHost()->GetHandle();
  IPC::PlatformFileForTransit renderer_file =
     IPC::GetFileHandleForProcess(browser_file.GetPlatformFile(),
                                  renderer_process, false);

  FileAvailable(job_id, browser_file.Pass(), renderer_file);
}


void MHTMLGenerationManager::MHTMLGenerated(int job_id, int64 mhtml_data_size) {
  JobFinished(job_id, mhtml_data_size);
}

void MHTMLGenerationManager::CreateFile(
    int job_id, const base::FilePath& file_path,
    base::ProcessHandle renderer_process) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  base::File browser_file(
      file_path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  if (!browser_file.IsValid()) {
    LOG(ERROR) << "Failed to create file to save MHTML at: " <<
        file_path.value();
  }

  IPC::PlatformFileForTransit renderer_file =
      IPC::GetFileHandleForProcess(browser_file.GetPlatformFile(),
                                   renderer_process, false);

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&MHTMLGenerationManager::FileAvailable,
                 base::Unretained(this),
                 job_id,
                 base::Passed(&browser_file),
                 renderer_file));
}

void MHTMLGenerationManager::FileAvailable(
    int job_id,
    base::File browser_file,
    IPC::PlatformFileForTransit renderer_file) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!browser_file.IsValid()) {
    LOG(ERROR) << "Failed to create file";
    JobFinished(job_id, -1);
    return;
  }

  IDToJobMap::iterator iter = id_to_job_.find(job_id);
  if (iter == id_to_job_.end()) {
    NOTREACHED();
    return;
  }

  Job* job = iter->second;
  job->set_browser_file(browser_file.Pass());

  RenderViewHost* rvh = RenderViewHost::FromID(
      job->process_id(), job->routing_id());
  if (!rvh) {
    // The contents went away.
    JobFinished(job_id, -1);
    return;
  }

  rvh->Send(new ViewMsg_SavePageAsMHTML(rvh->GetRoutingID(), job_id,
                                        renderer_file));
}

void MHTMLGenerationManager::JobFinished(int job_id, int64 file_size) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  IDToJobMap::iterator iter = id_to_job_.find(job_id);
  if (iter == id_to_job_.end()) {
    NOTREACHED();
    return;
  }

  Job* job = iter->second;
  job->callback().Run(file_size);

  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      base::Bind(&MHTMLGenerationManager::CloseFile, base::Unretained(this),
                 base::Passed(job->browser_file())));

  id_to_job_.erase(job_id);
  delete job;
}

void MHTMLGenerationManager::CloseFile(base::File file) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  file.Close();
}

int MHTMLGenerationManager::NewJob(WebContents* web_contents,
                                   const GenerateMHTMLCallback& callback) {
  static int id_counter = 0;
  int job_id = id_counter++;
  Job* job = new Job();
  id_to_job_[job_id] = job;
  job->SetWebContents(web_contents);
  job->set_callback(callback);
  return job_id;
}

void MHTMLGenerationManager::RenderProcessExited(Job* job) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  for (IDToJobMap::iterator it = id_to_job_.begin(); it != id_to_job_.end();
       ++it) {
    if (it->second == job) {
      JobFinished(it->first, -1);
      return;
    }
  }
  NOTREACHED();
}

}  // namespace content
