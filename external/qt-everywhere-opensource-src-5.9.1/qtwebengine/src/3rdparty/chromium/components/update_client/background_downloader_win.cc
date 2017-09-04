// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/background_downloader_win.h"

#include <atlbase.h>
#include <atlcom.h>
#include <stddef.h>

#include <stdint.h>
#include <functional>
#include <iomanip>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/sys_string_conversions.h"
#include "base/win/scoped_co_mem.h"
#include "components/update_client/utils.h"
#include "url/gurl.h"

using base::win::ScopedCoMem;
using base::win::ScopedComPtr;

// The class BackgroundDownloader in this module is an adapter between
// the CrxDownloader interface and the BITS service interfaces.
// The interface exposed on the CrxDownloader code runs on the main thread,
// while the BITS specific code runs on a separate thread passed in by the
// client. For every url to download, a BITS job is created, unless there is
// already an existing job for that url, in which case, the downloader
// connects to it. Once a job is associated with the url, the code looks for
// changes in the BITS job state. The checks are triggered by a timer.
// The BITS job contains just one file to download. There could only be one
// download in progress at a time. If Chrome closes down before the download is
// complete, the BITS job remains active and finishes in the background, without
// any intervention. The job can be completed next time the code runs, if the
// file is still needed, otherwise it will be cleaned up on a periodic basis.
//
// To list the BITS jobs for a user, use the |bitsadmin| tool. The command line
// to do that is: "bitsadmin /list /verbose". Another useful command is
// "bitsadmin /info" and provide the job id returned by the previous /list
// command.
//
// Ignoring the suspend/resume issues since this code is not using them, the
// job state machine implemented by BITS is something like this:
//
//  Suspended--->Queued--->Connecting---->Transferring--->Transferred
//       |          ^         |                 |               |
//       |          |         V                 V               | (complete)
//       +----------|---------+-----------------+-----+         V
//                  |         |                 |     |    Acknowledged
//                  |         V                 V     |
//                  |  Transient Error------->Error   |
//                  |         |                 |     |(cancel)
//                  |         +-------+---------+--->-+
//                  |                 V               |
//                  |   (resume)      |               |
//                  +------<----------+               +---->Cancelled
//
// The job is created in the "suspended" state. Once |Resume| is called,
// BITS queues up the job, then tries to connect, begins transferring the
// job bytes, and moves the job to the "transferred state, after the job files
// have been transferred. When calling |Complete| for a job, the job files are
// made available to the caller, and the job is moved to the "acknowledged"
// state.
// At any point, the job can be cancelled, in which case, the job is moved
// to the "cancelled state" and the job object is removed from the BITS queue.
// Along the way, the job can encounter recoverable and non-recoverable errors.
// BITS moves the job to "transient error" or "error", depending on which kind
// of error has occured.
// If  the job has reached the "transient error" state, BITS retries the
// job after a certain programmable delay. If the job can't be completed in a
// certain time interval, BITS stops retrying and errors the job out. This time
// interval is also programmable.
// If the job is in either of the error states, the job parameters can be
// adjusted to handle the error, after which the job can be resumed, and the
// whole cycle starts again.
// Jobs that are not touched in 90 days (or a value set by group policy) are
// automatically disposed off by BITS. This concludes the brief description of
// a job lifetime, according to BITS.
//
// In addition to how BITS is managing the life time of the job, there are a
// couple of special cases defined by the BackgroundDownloader.
// First, if the job encounters any of the 5xx HTTP responses, the job is
// not retried, in order to avoid DDOS-ing the servers.
// Second, there is a simple mechanism to detect stuck jobs, and allow the rest
// of the code to move on to trying other urls or trying other components.
// Last, after completing a job, irrespective of the outcome, the jobs older
// than a week are proactively cleaned up.

namespace update_client {

namespace {

// All jobs created by this module have a specific description so they can
// be found at run-time or by using system administration tools.
const base::char16 kJobDescription[] = L"Chrome Component Updater";

// How often the code looks for changes in the BITS job state.
const int kJobPollingIntervalSec = 4;

// How long BITS waits before retrying a job after the job encountered
// a transient error. If this value is not set, the BITS default is 10 minutes.
const int kMinimumRetryDelayMin = 1;

// How long to wait for stuck jobs. Stuck jobs could be queued for too long,
// have trouble connecting, could be suspended for any reason, or they have
// encountered some transient error.
const int kJobStuckTimeoutMin = 15;

// How long BITS waits before giving up on a job that could not be completed
// since the job has encountered its first transient error. If this value is
// not set, the BITS default is 14 days.
const int kSetNoProgressTimeoutDays = 1;

// How often the jobs which were started but not completed for any reason
// are cleaned up. Reasons for jobs to be left behind include browser restarts,
// system restarts, etc. Also, the check to purge stale jobs only happens
// at most once a day. If the job clean up code is not running, the BITS
// default policy is to cancel jobs after 90 days of inactivity.
const int kPurgeStaleJobsAfterDays = 7;
const int kPurgeStaleJobsIntervalBetweenChecksDays = 1;

// Retrieves the singleton instance of GIT for this process.
HRESULT GetGit(ScopedComPtr<IGlobalInterfaceTable>* git) {
  return git->CreateInstance(CLSID_StdGlobalInterfaceTable, NULL,
                             CLSCTX_INPROC_SERVER);
}

// Retrieves an interface pointer from the process GIT for a given |cookie|.
template <typename T>
HRESULT GetInterfaceFromGit(const ScopedComPtr<IGlobalInterfaceTable>& git,
                            DWORD cookie,
                            ScopedComPtr<T>* p) {
  return git->GetInterfaceFromGlobal(cookie, __uuidof(T), p->ReceiveVoid());
}

// Registers an interface pointer in GIT and returns its corresponding |cookie|.
template <typename T>
HRESULT RegisterInterfaceInGit(const ScopedComPtr<IGlobalInterfaceTable>& git,
                               const ScopedComPtr<T>& p,
                               DWORD* cookie) {
  return git->RegisterInterfaceInGlobal(p.get(), __uuidof(T), cookie);
}

// Returns the status code from a given BITS error.
int GetHttpStatusFromBitsError(HRESULT error) {
  // BITS errors are defined in bitsmsg.h. Although not documented, it is
  // clear that all errors corresponding to http status code have the high
  // word equal to 0x8019 and the low word equal to the http status code.
  const int kHttpStatusFirst = 100;  // Continue.
  const int kHttpStatusLast = 505;   // Version not supported.
  bool is_valid = HIWORD(error) == 0x8019 &&
                  LOWORD(error) >= kHttpStatusFirst &&
                  LOWORD(error) <= kHttpStatusLast;
  return is_valid ? LOWORD(error) : 0;
}

// Returns the files in a BITS job.
HRESULT GetFilesInJob(IBackgroundCopyJob* job,
                      std::vector<ScopedComPtr<IBackgroundCopyFile>>* files) {
  ScopedComPtr<IEnumBackgroundCopyFiles> enum_files;
  HRESULT hr = job->EnumFiles(enum_files.Receive());
  if (FAILED(hr))
    return hr;

  ULONG num_files = 0;
  hr = enum_files->GetCount(&num_files);
  if (FAILED(hr))
    return hr;

  for (ULONG i = 0; i != num_files; ++i) {
    ScopedComPtr<IBackgroundCopyFile> file;
    if (enum_files->Next(1, file.Receive(), NULL) == S_OK && file.get())
      files->push_back(file);
  }

  return S_OK;
}

// Returns the file name, the url, and some per-file progress information.
// The function out parameters can be NULL if that data is not requested.
HRESULT GetJobFileProperties(IBackgroundCopyFile* file,
                             base::string16* local_name,
                             base::string16* remote_name,
                             BG_FILE_PROGRESS* progress) {
  if (!file)
    return E_FAIL;

  HRESULT hr = S_OK;

  if (local_name) {
    ScopedCoMem<base::char16> name;
    hr = file->GetLocalName(&name);
    if (FAILED(hr))
      return hr;
    local_name->assign(name);
  }

  if (remote_name) {
    ScopedCoMem<base::char16> name;
    hr = file->GetRemoteName(&name);
    if (FAILED(hr))
      return hr;
    remote_name->assign(name);
  }

  if (progress) {
    BG_FILE_PROGRESS bg_file_progress = {};
    hr = file->GetProgress(&bg_file_progress);
    if (FAILED(hr))
      return hr;
    *progress = bg_file_progress;
  }

  return hr;
}

// Returns the number of bytes downloaded and bytes to download for all files
// in the job. If the values are not known or if an error has occurred,
// a value of -1 is reported.
HRESULT GetJobByteCount(IBackgroundCopyJob* job,
                        int64_t* downloaded_bytes,
                        int64_t* total_bytes) {
  *downloaded_bytes = -1;
  *total_bytes = -1;

  if (!job)
    return E_FAIL;

  BG_JOB_PROGRESS job_progress = {};
  HRESULT hr = job->GetProgress(&job_progress);
  if (FAILED(hr))
    return hr;

  const uint64_t kMaxNumBytes =
      static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
  if (job_progress.BytesTransferred <= kMaxNumBytes)
    *downloaded_bytes = job_progress.BytesTransferred;

  if (job_progress.BytesTotal <= kMaxNumBytes &&
      job_progress.BytesTotal != BG_SIZE_UNKNOWN)
    *total_bytes = job_progress.BytesTotal;

  return S_OK;
}

HRESULT GetJobDescription(IBackgroundCopyJob* job, const base::string16* name) {
  ScopedCoMem<base::char16> description;
  return job->GetDescription(&description);
}

// Returns the job error code in |error_code| if the job is in the transient
// or the final error state. Otherwise, the job error is not available and
// the function fails.
HRESULT GetJobError(IBackgroundCopyJob* job, HRESULT* error_code_out) {
  *error_code_out = S_OK;
  ScopedComPtr<IBackgroundCopyError> copy_error;
  HRESULT hr = job->GetError(copy_error.Receive());
  if (FAILED(hr))
    return hr;

  BG_ERROR_CONTEXT error_context = BG_ERROR_CONTEXT_NONE;
  HRESULT error_code = S_OK;
  hr = copy_error->GetError(&error_context, &error_code);
  if (FAILED(hr))
    return hr;

  *error_code_out = FAILED(error_code) ? error_code : E_FAIL;
  return S_OK;
}

// Finds the component updater jobs matching the given predicate.
// Returns S_OK if the function has found at least one job, returns S_FALSE if
// no job was found, and it returns an error otherwise.
template <class Predicate>
HRESULT FindBitsJobIf(Predicate pred,
                      IBackgroundCopyManager* bits_manager,
                      std::vector<ScopedComPtr<IBackgroundCopyJob>>* jobs) {
  ScopedComPtr<IEnumBackgroundCopyJobs> enum_jobs;
  HRESULT hr = bits_manager->EnumJobs(0, enum_jobs.Receive());
  if (FAILED(hr))
    return hr;

  ULONG job_count = 0;
  hr = enum_jobs->GetCount(&job_count);
  if (FAILED(hr))
    return hr;

  // Iterate over jobs, run the predicate, and select the job only if
  // the job description matches the component updater jobs.
  for (ULONG i = 0; i != job_count; ++i) {
    ScopedComPtr<IBackgroundCopyJob> current_job;
    if (enum_jobs->Next(1, current_job.Receive(), NULL) == S_OK &&
        pred(current_job.get())) {
      base::string16 job_description;
      hr = GetJobDescription(current_job.get(), &job_description);
      if (job_description.compare(kJobDescription) == 0)
        jobs->push_back(current_job);
    }
  }

  return jobs->empty() ? S_FALSE : S_OK;
}

// Compares the job creation time and returns true if the job creation time
// is older than |num_days|.
class JobCreationOlderThanDays {
 public:
  explicit JobCreationOlderThanDays(int num_days) : num_days_(num_days) {}
  bool operator()(IBackgroundCopyJob* job) const;

 private:
  int num_days_;
};

bool JobCreationOlderThanDays::operator()(IBackgroundCopyJob* job) const {
  BG_JOB_TIMES times = {};
  HRESULT hr = job->GetTimes(&times);
  if (FAILED(hr))
    return false;

  const base::TimeDelta time_delta(base::TimeDelta::FromDays(num_days_));
  const base::Time creation_time(base::Time::FromFileTime(times.CreationTime));

  return creation_time + time_delta < base::Time::Now();
}

// Compares the url of a file in a job and returns true if the remote name
// of any file in a job matches the argument.
class JobFileUrlEqual {
 public:
  explicit JobFileUrlEqual(const base::string16& remote_name)
      : remote_name_(remote_name) {}
  bool operator()(IBackgroundCopyJob* job) const;

 private:
  base::string16 remote_name_;
};

bool JobFileUrlEqual::operator()(IBackgroundCopyJob* job) const {
  std::vector<ScopedComPtr<IBackgroundCopyFile>> files;
  HRESULT hr = GetFilesInJob(job, &files);
  if (FAILED(hr))
    return false;

  for (size_t i = 0; i != files.size(); ++i) {
    ScopedCoMem<base::char16> name;
    if (SUCCEEDED(files[i]->GetRemoteName(&name)) &&
        remote_name_.compare(name) == 0)
      return true;
  }

  return false;
}

// Creates an instance of the BITS manager.
HRESULT CreateBitsManager(IBackgroundCopyManager** bits_manager) {
  ScopedComPtr<IBackgroundCopyManager> object;
  HRESULT hr = object.CreateInstance(__uuidof(BackgroundCopyManager));
  if (FAILED(hr)) {
    return hr;
  }
  *bits_manager = object.Detach();
  return S_OK;
}

void CleanupJobFiles(IBackgroundCopyJob* job) {
  std::vector<ScopedComPtr<IBackgroundCopyFile>> files;
  if (FAILED(GetFilesInJob(job, &files)))
    return;
  for (size_t i = 0; i != files.size(); ++i) {
    base::string16 local_name;
    HRESULT hr(GetJobFileProperties(files[i].get(), &local_name, NULL, NULL));
    if (SUCCEEDED(hr))
      DeleteFileAndEmptyParentDirectory(base::FilePath(local_name));
  }
}

// Cleans up incompleted jobs that are too old.
HRESULT CleanupStaleJobs(
    const ScopedComPtr<IBackgroundCopyManager>& bits_manager) {
  if (!bits_manager.get())
    return E_FAIL;

  static base::Time last_sweep;

  const base::TimeDelta time_delta(
      base::TimeDelta::FromDays(kPurgeStaleJobsIntervalBetweenChecksDays));
  const base::Time current_time(base::Time::Now());
  if (last_sweep + time_delta > current_time)
    return S_OK;

  last_sweep = current_time;

  std::vector<ScopedComPtr<IBackgroundCopyJob>> jobs;
  HRESULT hr = FindBitsJobIf(
      JobCreationOlderThanDays(kPurgeStaleJobsAfterDays),
      bits_manager.get(), &jobs);
  if (FAILED(hr))
    return hr;

  for (size_t i = 0; i != jobs.size(); ++i) {
    jobs[i]->Cancel();
    CleanupJobFiles(jobs[i].get());
  }

  return S_OK;
}

}  // namespace

BackgroundDownloader::BackgroundDownloader(
    std::unique_ptr<CrxDownloader> successor,
    net::URLRequestContextGetter* context_getter,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : CrxDownloader(task_runner, std::move(successor)),
      context_getter_(context_getter),
      git_cookie_bits_manager_(0),
      git_cookie_job_(0) {}

BackgroundDownloader::~BackgroundDownloader() {
  DCHECK(thread_checker_.CalledOnValidThread());
  timer_.reset();
}

void BackgroundDownloader::StartTimer() {
  timer_.reset(new base::OneShotTimer);
  timer_->Start(FROM_HERE, base::TimeDelta::FromSeconds(kJobPollingIntervalSec),
                this, &BackgroundDownloader::OnTimer);
}

void BackgroundDownloader::OnTimer() {
  DCHECK(thread_checker_.CalledOnValidThread());
  task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&BackgroundDownloader::OnDownloading, base::Unretained(this)));
}

bool BackgroundDownloader::TimerIsRunning() const {
  return timer_.get() && timer_->IsRunning();
}

void BackgroundDownloader::DoStartDownload(const GURL& url) {
  DCHECK(thread_checker_.CalledOnValidThread());
  task_runner()->PostTask(FROM_HERE,
                          base::Bind(&BackgroundDownloader::BeginDownload,
                                     base::Unretained(this), url));
}

// Called one time when this class is asked to do a download.
void BackgroundDownloader::BeginDownload(const GURL& url) {
  DCHECK(task_runner()->RunsTasksOnCurrentThread());

  download_start_time_ = base::TimeTicks::Now();
  job_stuck_begin_time_ = download_start_time_;

  HRESULT hr = BeginDownloadHelper(url);
  if (FAILED(hr)) {
    EndDownload(hr);
    return;
  }

  ResetInterfacePointers();
  main_task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&BackgroundDownloader::StartTimer, base::Unretained(this)));
}

// Creates or opens an existing bits job, and handles the marshalling of
// the interfaces in GIT.
HRESULT BackgroundDownloader::BeginDownloadHelper(const GURL& url) {
  ScopedComPtr<IGlobalInterfaceTable> git;
  HRESULT hr = GetGit(&git);
  if (FAILED(hr))
    return hr;

  hr = CreateBitsManager(bits_manager_.Receive());
  if (FAILED(hr))
    return hr;

  hr = RegisterInterfaceInGit(git, bits_manager_, &git_cookie_bits_manager_);
  if (FAILED(hr))
    return hr;

  hr = QueueBitsJob(url, job_.Receive());
  if (FAILED(hr))
    return hr;

  hr = RegisterInterfaceInGit(git, job_, &git_cookie_job_);
  if (FAILED(hr))
    return hr;

  return S_OK;
}

// Called any time the timer fires.
void BackgroundDownloader::OnDownloading() {
  DCHECK(task_runner()->RunsTasksOnCurrentThread());

  HRESULT hr = UpdateInterfacePointers();
  if (FAILED(hr)) {
    EndDownload(hr);
    return;
  }

  BG_JOB_STATE job_state = BG_JOB_STATE_ERROR;
  hr = job_->GetState(&job_state);
  if (FAILED(hr)) {
    EndDownload(hr);
    return;
  }

  bool is_handled =  false;
  switch (job_state) {
    case BG_JOB_STATE_TRANSFERRED:
      is_handled = OnStateTransferred();
      break;

    case BG_JOB_STATE_ERROR:
      is_handled = OnStateError();
      break;

    case BG_JOB_STATE_CANCELLED:
      is_handled = OnStateCancelled();
      break;

    case BG_JOB_STATE_ACKNOWLEDGED:
      is_handled = OnStateAcknowledged();
      break;

    case BG_JOB_STATE_QUEUED:
    // Fall through.
    case BG_JOB_STATE_CONNECTING:
    // Fall through.
    case BG_JOB_STATE_SUSPENDED:
      is_handled = OnStateQueued();
      break;

    case BG_JOB_STATE_TRANSIENT_ERROR:
      is_handled = OnStateTransientError();
      break;

    case BG_JOB_STATE_TRANSFERRING:
      is_handled = OnStateTransferring();
      break;

    default:
      break;
  }

  if (is_handled)
    return;

  ResetInterfacePointers();
  main_task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&BackgroundDownloader::StartTimer, base::Unretained(this)));
}

// Completes the BITS download, picks up the file path of the response, and
// notifies the CrxDownloader. The function should be called only once.
void BackgroundDownloader::EndDownload(HRESULT error) {
  DCHECK(task_runner()->RunsTasksOnCurrentThread());

  DCHECK(!TimerIsRunning());

  const base::TimeTicks download_end_time(base::TimeTicks::Now());
  const base::TimeDelta download_time =
      download_end_time >= download_start_time_
          ? download_end_time - download_start_time_
          : base::TimeDelta();

  int64_t downloaded_bytes = -1;
  int64_t total_bytes = -1;
  GetJobByteCount(job_.get(), &downloaded_bytes, &total_bytes);

  if (FAILED(error) && job_.get()) {
    job_->Cancel();
    CleanupJobFiles(job_.get());
  }

  CleanupStaleJobs(bits_manager_);

  ClearGit();

  // Consider the url handled if it has been successfully downloaded or a
  // 5xx has been received.
  const bool is_handled =
      SUCCEEDED(error) || IsHttpServerError(GetHttpStatusFromBitsError(error));

  const int error_to_report = SUCCEEDED(error) ? 0 : error;

  DownloadMetrics download_metrics;
  download_metrics.url = url();
  download_metrics.downloader = DownloadMetrics::kBits;
  download_metrics.error = error_to_report;
  download_metrics.downloaded_bytes = downloaded_bytes;
  download_metrics.total_bytes = total_bytes;
  download_metrics.download_time_ms = download_time.InMilliseconds();

  Result result;
  result.error = error_to_report;
  result.response = response_;
  result.downloaded_bytes = downloaded_bytes;
  result.total_bytes = total_bytes;
  main_task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&BackgroundDownloader::OnDownloadComplete,
                 base::Unretained(this), is_handled, result, download_metrics));

  // Once the task is posted to the the main thread, this object may be deleted
  // by its owner. It is not safe to access members of this object on the
  // task runner from this point on. The timer is stopped and all BITS
  // interface pointers have been released.
}

// Called when the BITS job has been transferred successfully. Completes the
// BITS job by removing it from the BITS queue and making the download
// available to the caller.
bool BackgroundDownloader::OnStateTransferred() {
  EndDownload(CompleteJob());
  return true;
}

// Called when the job has encountered an error and no further progress can
// be made. Cancels this job and removes it from the BITS queue.
bool BackgroundDownloader::OnStateError() {
  HRESULT error_code = S_OK;
  HRESULT hr = GetJobError(job_.get(), &error_code);
  if (FAILED(hr))
    error_code = hr;

  DCHECK(FAILED(error_code));
  EndDownload(error_code);
  return true;
}

// Called when the download was completed. This notification is not seen
// in the current implementation but provided here as a defensive programming
// measure.
bool BackgroundDownloader::OnStateAcknowledged() {
  EndDownload(E_UNEXPECTED);
  return true;
}

// Called when the download was cancelled. Same as above.
bool BackgroundDownloader::OnStateCancelled() {
  EndDownload(E_UNEXPECTED);
  return true;
}

// Called when the job has encountered a transient error, such as a
// network disconnect, a server error, or some other recoverable error.
bool BackgroundDownloader::OnStateTransientError() {
  // If the job appears to be stuck, handle the transient error as if
  // it were a final error. This causes the job to be cancelled and a specific
  // error be returned, if the error was available.
  if (IsStuck()) {
    return OnStateError();
  }

  // Don't retry at all if the transient error was a 5xx.
  HRESULT error_code = S_OK;
  HRESULT hr = GetJobError(job_.get(), &error_code);
  if (SUCCEEDED(hr) &&
      IsHttpServerError(GetHttpStatusFromBitsError(error_code))) {
    return OnStateError();
  }

  return false;
}

bool BackgroundDownloader::OnStateQueued() {
  if (!IsStuck())
    return false;

  // Terminate the download if the job has not made progress in a while.
  EndDownload(E_ABORT);
  return true;
}

bool BackgroundDownloader::OnStateTransferring() {
  // Resets the baseline for detecting a stuck job since the job is transferring
  // data and it is making progress.
  job_stuck_begin_time_ = base::TimeTicks::Now();

  int64_t downloaded_bytes = -1;
  int64_t total_bytes = -1;
  HRESULT hr = GetJobByteCount(job_.get(), &downloaded_bytes, &total_bytes);
  if (FAILED(hr))
    return false;

  Result result;
  result.downloaded_bytes = downloaded_bytes;
  result.total_bytes = total_bytes;

  main_task_runner()->PostTask(
      FROM_HERE, base::Bind(&BackgroundDownloader::OnDownloadProgress,
                            base::Unretained(this), result));
  return false;
}

// Creates or opens a job for the given url and queues it up. Tries to
// install a job observer but continues on if an observer can't be set up.
HRESULT BackgroundDownloader::QueueBitsJob(const GURL& url,
                                           IBackgroundCopyJob** job) {
  DCHECK(task_runner()->RunsTasksOnCurrentThread());

  ScopedComPtr<IBackgroundCopyJob> p;
  HRESULT hr = CreateOrOpenJob(url, p.Receive());
  if (FAILED(hr))
    return hr;

  if (hr == S_OK) {
    // This is a new job and it needs initialization.
    hr = InitializeNewJob(p, url);
    if (FAILED(hr))
      return hr;
  }

  hr = p->Resume();
  if (FAILED(hr))
    return hr;

  *job = p.Detach();

  return S_OK;
}

HRESULT BackgroundDownloader::CreateOrOpenJob(const GURL& url,
                                              IBackgroundCopyJob** job) {
  std::vector<ScopedComPtr<IBackgroundCopyJob>> jobs;
  HRESULT hr = FindBitsJobIf(
      JobFileUrlEqual(base::SysUTF8ToWide(url.spec())),
      bits_manager_.get(), &jobs);
  if (SUCCEEDED(hr) && !jobs.empty()) {
    *job = jobs.front().Detach();
    return S_FALSE;
  }

  // Use kJobDescription as a temporary job display name until the proper
  // display name is initialized later on.
  GUID guid = {0};
  hr = bits_manager_->CreateJob(kJobDescription, BG_JOB_TYPE_DOWNLOAD, &guid,
                                job);
  if (FAILED(hr))
    return hr;

  return S_OK;
}

HRESULT BackgroundDownloader::InitializeNewJob(
    const base::win::ScopedComPtr<IBackgroundCopyJob>& job,
    const GURL& url) {
  const base::string16 filename(base::SysUTF8ToWide(url.ExtractFileName()));

  base::FilePath tempdir;
  if (!base::CreateNewTempDirectory(FILE_PATH_LITERAL("chrome_BITS_"),
                                    &tempdir))
    return E_FAIL;

  HRESULT hr = job->AddFile(base::SysUTF8ToWide(url.spec()).c_str(),
                            tempdir.Append(filename).AsUTF16Unsafe().c_str());
  if (FAILED(hr))
    return hr;

  hr = job->SetDisplayName(filename.c_str());
  if (FAILED(hr))
    return hr;

  hr = job->SetDescription(kJobDescription);
  if (FAILED(hr))
    return hr;

  hr = job->SetPriority(BG_JOB_PRIORITY_NORMAL);
  if (FAILED(hr))
    return hr;

  hr = job->SetMinimumRetryDelay(60 * kMinimumRetryDelayMin);
  if (FAILED(hr))
    return hr;

  const int kSecondsDay = 60 * 60 * 24;
  hr = job->SetNoProgressTimeout(kSecondsDay * kSetNoProgressTimeoutDays);
  if (FAILED(hr))
    return hr;

  return S_OK;
}

bool BackgroundDownloader::IsStuck() {
  const base::TimeDelta job_stuck_timeout(
      base::TimeDelta::FromMinutes(kJobStuckTimeoutMin));
  return job_stuck_begin_time_ + job_stuck_timeout < base::TimeTicks::Now();
}

HRESULT BackgroundDownloader::CompleteJob() {
  HRESULT hr = job_->Complete();
  if (FAILED(hr) && hr != BG_S_UNABLE_TO_DELETE_FILES)
    return hr;

  std::vector<ScopedComPtr<IBackgroundCopyFile>> files;
  hr = GetFilesInJob(job_.get(), &files);
  if (FAILED(hr))
    return hr;

  if (files.empty())
    return E_UNEXPECTED;

  base::string16 local_name;
  BG_FILE_PROGRESS progress = {0};
  hr = GetJobFileProperties(files.front().get(), &local_name, NULL, &progress);
  if (FAILED(hr))
    return hr;

  // Sanity check the post-conditions of a successful download, including
  // the file and job invariants. The byte counts for a job and its file
  // must match as a job only contains one file.
  DCHECK(progress.Completed);
  DCHECK_EQ(progress.BytesTotal, progress.BytesTransferred);

  response_ = base::FilePath(local_name);

  return S_OK;
}

HRESULT BackgroundDownloader::UpdateInterfacePointers() {
  DCHECK(task_runner()->RunsTasksOnCurrentThread());

  ScopedComPtr<IGlobalInterfaceTable> git;
  HRESULT hr = GetGit(&git);
  if (FAILED(hr))
    return hr;

  bits_manager_ = nullptr;
  hr = GetInterfaceFromGit(git, git_cookie_bits_manager_, &bits_manager_);
  if (FAILED(hr))
    return hr;

  job_ = nullptr;
  hr = GetInterfaceFromGit(git, git_cookie_job_, &job_);
  if (FAILED(hr))
    return hr;

  return S_OK;
}

void BackgroundDownloader::ResetInterfacePointers() {
  job_ = nullptr;
  bits_manager_ = nullptr;
}

HRESULT BackgroundDownloader::ClearGit() {
  DCHECK(task_runner()->RunsTasksOnCurrentThread());

  ResetInterfacePointers();

  ScopedComPtr<IGlobalInterfaceTable> git;
  HRESULT hr = GetGit(&git);
  if (FAILED(hr))
    return hr;

  const DWORD cookies[] = {
      git_cookie_job_, git_cookie_bits_manager_
  };

  for (auto cookie : cookies) {
    // TODO(sorin): check the result of the call, see crbug.com/644857.
    git->RevokeInterfaceFromGlobal(cookie);
  }

  return S_OK;
}

}  // namespace update_client
