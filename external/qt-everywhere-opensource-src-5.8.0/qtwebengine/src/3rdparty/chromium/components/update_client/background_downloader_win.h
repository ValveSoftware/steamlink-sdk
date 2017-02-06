// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_BACKGROUND_DOWNLOADER_WIN_H_
#define COMPONENTS_UPDATE_CLIENT_BACKGROUND_DOWNLOADER_WIN_H_

#include <windows.h>
#include <bits.h>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/win/scoped_comptr.h"
#include "components/update_client/crx_downloader.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}

namespace update_client {

// Implements a downloader in terms of the BITS service. The public interface
// of this class and the CrxDownloader overrides are expected to be called
// from the main thread. The rest of the class code runs on a sequenced
// task runner, usually associated with a blocking thread pool. The task runner
// must initialize COM.
//
// This class manages a COM client for Windows BITS. The client uses polling,
// triggered by an one-shot timer, to get state updates from BITS. Since the
// timer has thread afinity, the callbacks from the timer are delegated to
// a sequenced task runner, which handles all client COM interaction with
// the BITS service.
class BackgroundDownloader : public CrxDownloader {
 protected:
  friend class CrxDownloader;
  BackgroundDownloader(
      std::unique_ptr<CrxDownloader> successor,
      net::URLRequestContextGetter* context_getter,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);
  ~BackgroundDownloader() override;

 private:
  // Overrides for CrxDownloader.
  void DoStartDownload(const GURL& url) override;

  // Called asynchronously on the |task_runner_| at different stages during
  // the download. |OnDownloading| can be called multiple times.
  // |EndDownload| switches the execution flow from the |task_runner_| to the
  // main thread. Accessing any data members of this object from the
  // |task_runner_| after calling |EndDownload| is unsafe.
  void BeginDownload(const GURL& url);
  void OnDownloading();
  void EndDownload(HRESULT hr);

  HRESULT BeginDownloadHelper(const GURL& url);

  // Handles the job state transitions to a final state. Returns true always
  // since the download has reached a final state and no further processing for
  // this download is needed.
  bool OnStateTransferred();
  bool OnStateError();
  bool OnStateCancelled();
  bool OnStateAcknowledged();

  // Handles the transition to a transient state where the job is in the
  // queue but not actively transferring data. Returns true if the download has
  // been in this state for too long and it will be abandoned, or false, if
  // further processing for this download is needed.
  bool OnStateQueued();

  // Handles the job state transition to a transient error state, which may or
  // may not be considered final, depending on the error. Returns true if
  // the state is final, or false, if the download is allowed to continue.
  bool OnStateTransientError();

  // Handles the job state corresponding to transferring data. Returns false
  // always since this is never a final state.
  bool OnStateTransferring();

  void StartTimer();
  void OnTimer();

  // Returns true if the timer is running or false if the timer is not
  // created or not running at all.
  bool TimerIsRunning() const;

  HRESULT QueueBitsJob(const GURL& url, IBackgroundCopyJob** job);
  HRESULT CreateOrOpenJob(const GURL& url, IBackgroundCopyJob** job);
  HRESULT InitializeNewJob(
      const base::win::ScopedComPtr<IBackgroundCopyJob>& job,
      const GURL& url);

  // Returns true if at the time of the call, it appears that the job
  // has not been making progress toward completion.
  bool IsStuck();

  // Makes the downloaded file available to the caller by renaming the
  // temporary file to its destination and removing it from the BITS queue.
  HRESULT CompleteJob();

  // Revokes the interface pointers from GIT.
  HRESULT ClearGit();

  // Updates the BITS interface pointers so that they can be used by the
  // thread calling the function. Call this function to get valid COM interface
  // pointers when a thread from the thread pool enters the object.
  HRESULT UpdateInterfacePointers();

  // Resets the BITS interface pointers. Call this function when a thread
  // from the thread pool leaves the object to release the interface pointers.
  void ResetInterfacePointers();

  // Ensures that we are running on the same thread we created the object on.
  base::ThreadChecker thread_checker_;

  net::URLRequestContextGetter* context_getter_;

  // The timer has thread affinity. This member is initialized and destroyed
  // on the main task runner.
  std::unique_ptr<base::OneShotTimer> timer_;

  DWORD git_cookie_bits_manager_;
  DWORD git_cookie_job_;

  // COM interface pointers are valid for the thread that called
  // |UpdateInterfacePointers| to get pointers to COM proxies, which are valid
  // for that thread only.
  base::win::ScopedComPtr<IBackgroundCopyManager> bits_manager_;
  base::win::ScopedComPtr<IBackgroundCopyJob> job_;

  // Contains the time when the download of the current url has started.
  base::Time download_start_time_;

  // Contains the time when the BITS job is last seen making progress.
  base::Time job_stuck_begin_time_;

  // Contains the path of the downloaded file if the download was successful.
  base::FilePath response_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundDownloader);
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_BACKGROUND_DOWNLOADER_WIN_H_
