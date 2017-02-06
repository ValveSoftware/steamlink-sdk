// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_file_impl.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/values.h"
#include "content/browser/byte_stream.h"
#include "content/browser/download/download_create_info.h"
#include "content/browser/download/download_destination_observer.h"
#include "content/browser/download/download_interrupt_reasons_impl.h"
#include "content/browser/download/download_net_log_parameters.h"
#include "content/browser/download/download_stats.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "net/base/io_buffer.h"

namespace content {

const int kUpdatePeriodMs = 500;
const int kMaxTimeBlockingFileThreadMs = 1000;

// These constants control the default retry behavior for failing renames. Each
// retry is performed after a delay that is twice the previous delay. The
// initial delay is specified by kInitialRenameRetryDelayMs.
const int kInitialRenameRetryDelayMs = 200;

// Number of times a failing rename is retried before giving up.
const int kMaxRenameRetries = 3;

DownloadFileImpl::DownloadFileImpl(
    std::unique_ptr<DownloadSaveInfo> save_info,
    const base::FilePath& default_download_directory,
    std::unique_ptr<ByteStreamReader> stream,
    const net::BoundNetLog& bound_net_log,
    base::WeakPtr<DownloadDestinationObserver> observer)
    : file_(bound_net_log),
      save_info_(std::move(save_info)),
      default_download_directory_(default_download_directory),
      stream_reader_(std::move(stream)),
      bytes_seen_(0),
      bound_net_log_(bound_net_log),
      observer_(observer),
      weak_factory_(this) {}

DownloadFileImpl::~DownloadFileImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
}

void DownloadFileImpl::Initialize(const InitializeCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  update_timer_.reset(new base::RepeatingTimer());
  DownloadInterruptReason result =
      file_.Initialize(save_info_->file_path,
                       default_download_directory_,
                       std::move(save_info_->file),
                       save_info_->offset,
                       save_info_->hash_of_partial_file,
                       std::move(save_info_->hash_state));
  if (result != DOWNLOAD_INTERRUPT_REASON_NONE) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE, base::Bind(callback, result));
    return;
  }

  stream_reader_->RegisterCallback(
      base::Bind(&DownloadFileImpl::StreamActive, weak_factory_.GetWeakPtr()));

  download_start_ = base::TimeTicks::Now();

  // Primarily to make reset to zero in restart visible to owner.
  SendUpdate();

  // Initial pull from the straw.
  StreamActive();

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE, base::Bind(
          callback, DOWNLOAD_INTERRUPT_REASON_NONE));
}

DownloadInterruptReason DownloadFileImpl::AppendDataToFile(
    const char* data, size_t data_len) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  if (!update_timer_->IsRunning()) {
    update_timer_->Start(FROM_HERE,
                         base::TimeDelta::FromMilliseconds(kUpdatePeriodMs),
                         this, &DownloadFileImpl::SendUpdate);
  }
  rate_estimator_.Increment(data_len);
  return file_.AppendDataToFile(data, data_len);
}

void DownloadFileImpl::RenameAndUniquify(
    const base::FilePath& full_path,
    const RenameCompletionCallback& callback) {
  std::unique_ptr<RenameParameters> parameters(
      new RenameParameters(UNIQUIFY, full_path, callback));
  RenameWithRetryInternal(std::move(parameters));
}

void DownloadFileImpl::RenameAndAnnotate(
    const base::FilePath& full_path,
    const std::string& client_guid,
    const GURL& source_url,
    const GURL& referrer_url,
    const RenameCompletionCallback& callback) {
  std::unique_ptr<RenameParameters> parameters(new RenameParameters(
      ANNOTATE_WITH_SOURCE_INFORMATION, full_path, callback));
  parameters->client_guid = client_guid;
  parameters->source_url = source_url;
  parameters->referrer_url = referrer_url;
  RenameWithRetryInternal(std::move(parameters));
}

base::TimeDelta DownloadFileImpl::GetRetryDelayForFailedRename(
    int attempt_number) {
  DCHECK_GE(attempt_number, 0);
  // |delay| starts at kInitialRenameRetryDelayMs and increases by a factor of
  // 2 at each subsequent retry. Assumes that |retries_left| starts at
  // kMaxRenameRetries. Also assumes that kMaxRenameRetries is less than the
  // number of bits in an int.
  return base::TimeDelta::FromMilliseconds(kInitialRenameRetryDelayMs) *
         (1 << attempt_number);
}

bool DownloadFileImpl::ShouldRetryFailedRename(DownloadInterruptReason reason) {
  return reason == DOWNLOAD_INTERRUPT_REASON_FILE_TRANSIENT_ERROR;
}

void DownloadFileImpl::RenameWithRetryInternal(
    std::unique_ptr<RenameParameters> parameters) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  base::FilePath new_path = parameters->new_path;

  if ((parameters->option & UNIQUIFY) && new_path != file_.full_path()) {
    int uniquifier =
        base::GetUniquePathNumber(new_path, base::FilePath::StringType());
    if (uniquifier > 0)
      new_path = new_path.InsertBeforeExtensionASCII(
          base::StringPrintf(" (%d)", uniquifier));
  }

  DownloadInterruptReason reason = file_.Rename(new_path);

  // Attempt to retry the rename if possible. If the rename failed and the
  // subsequent open also failed, then in_progress() would be false. We don't
  // try to retry renames if the in_progress() was false to begin with since we
  // have less assurance that the file at file_.full_path() was the one we were
  // working with.
  if (ShouldRetryFailedRename(reason) && file_.in_progress() &&
      parameters->retries_left > 0) {
    int attempt_number = kMaxRenameRetries - parameters->retries_left;
    --parameters->retries_left;
    if (parameters->time_of_first_failure.is_null())
      parameters->time_of_first_failure = base::TimeTicks::Now();
    BrowserThread::PostDelayedTask(
        BrowserThread::FILE,
        FROM_HERE,
        base::Bind(&DownloadFileImpl::RenameWithRetryInternal,
                   weak_factory_.GetWeakPtr(),
                   base::Passed(std::move(parameters))),
        GetRetryDelayForFailedRename(attempt_number));
    return;
  }

  if (!parameters->time_of_first_failure.is_null())
    RecordDownloadFileRenameResultAfterRetry(
        base::TimeTicks::Now() - parameters->time_of_first_failure, reason);

  if (reason == DOWNLOAD_INTERRUPT_REASON_NONE &&
      (parameters->option & ANNOTATE_WITH_SOURCE_INFORMATION)) {
    // Doing the annotation after the rename rather than before leaves
    // a very small window during which the file has the final name but
    // hasn't been marked with the Mark Of The Web.  However, it allows
    // anti-virus scanners on Windows to actually see the data
    // (http://crbug.com/127999) under the correct name (which is information
    // it uses).
    reason = file_.AnnotateWithSourceInformation(parameters->client_guid,
                                                 parameters->source_url,
                                                 parameters->referrer_url);
  }

  if (reason != DOWNLOAD_INTERRUPT_REASON_NONE) {
    // Make sure our information is updated, since we're about to
    // error out.
    SendUpdate();

    // Null out callback so that we don't do any more stream processing.
    stream_reader_->RegisterCallback(base::Closure());

    new_path.clear();
  }

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(parameters->completion_callback, reason, new_path));
}

void DownloadFileImpl::Detach() {
  file_.Detach();
}

void DownloadFileImpl::Cancel() {
  file_.Cancel();
}

const base::FilePath& DownloadFileImpl::FullPath() const {
  return file_.full_path();
}

bool DownloadFileImpl::InProgress() const {
  return file_.in_progress();
}

void DownloadFileImpl::StreamActive() {
  base::TimeTicks start(base::TimeTicks::Now());
  base::TimeTicks now;
  scoped_refptr<net::IOBuffer> incoming_data;
  size_t incoming_data_size = 0;
  size_t total_incoming_data_size = 0;
  size_t num_buffers = 0;
  ByteStreamReader::StreamState state(ByteStreamReader::STREAM_EMPTY);
  DownloadInterruptReason reason = DOWNLOAD_INTERRUPT_REASON_NONE;
  base::TimeDelta delta(
      base::TimeDelta::FromMilliseconds(kMaxTimeBlockingFileThreadMs));

  // Take care of any file local activity required.
  do {
    state = stream_reader_->Read(&incoming_data, &incoming_data_size);

    switch (state) {
      case ByteStreamReader::STREAM_EMPTY:
        break;
      case ByteStreamReader::STREAM_HAS_DATA:
        {
          ++num_buffers;
          base::TimeTicks write_start(base::TimeTicks::Now());
          reason = AppendDataToFile(
              incoming_data.get()->data(), incoming_data_size);
          disk_writes_time_ += (base::TimeTicks::Now() - write_start);
          bytes_seen_ += incoming_data_size;
          total_incoming_data_size += incoming_data_size;
        }
        break;
      case ByteStreamReader::STREAM_COMPLETE:
        {
          reason = static_cast<DownloadInterruptReason>(
              stream_reader_->GetStatus());
          SendUpdate();
          base::TimeTicks close_start(base::TimeTicks::Now());
          base::TimeTicks now(base::TimeTicks::Now());
          disk_writes_time_ += (now - close_start);
          RecordFileBandwidth(
              bytes_seen_, disk_writes_time_, now - download_start_);
          update_timer_.reset();
        }
        break;
      default:
        NOTREACHED();
        break;
    }
    now = base::TimeTicks::Now();
  } while (state == ByteStreamReader::STREAM_HAS_DATA &&
           reason == DOWNLOAD_INTERRUPT_REASON_NONE &&
           now - start <= delta);

  // If we're stopping to yield the thread, post a task so we come back.
  if (state == ByteStreamReader::STREAM_HAS_DATA &&
      now - start > delta) {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&DownloadFileImpl::StreamActive,
                   weak_factory_.GetWeakPtr()));
  }

  if (total_incoming_data_size)
    RecordFileThreadReceiveBuffers(num_buffers);

  RecordContiguousWriteTime(now - start);

  // Take care of communication with our observer.
  if (reason != DOWNLOAD_INTERRUPT_REASON_NONE) {
    // Error case for both upstream source and file write.
    // Shut down processing and signal an error to our observer.
    // Our observer will clean us up.
    stream_reader_->RegisterCallback(base::Closure());
    weak_factory_.InvalidateWeakPtrs();
    SendUpdate();  // Make info up to date before error.
    std::unique_ptr<crypto::SecureHash> hash_state = file_.Finish();
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&DownloadDestinationObserver::DestinationError,
                   observer_,
                   reason,
                   file_.bytes_so_far(),
                   base::Passed(&hash_state)));
  } else if (state == ByteStreamReader::STREAM_COMPLETE) {
    // Signal successful completion and shut down processing.
    stream_reader_->RegisterCallback(base::Closure());
    weak_factory_.InvalidateWeakPtrs();
    SendUpdate();
    std::unique_ptr<crypto::SecureHash> hash_state = file_.Finish();
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&DownloadDestinationObserver::DestinationCompleted,
                   observer_,
                   file_.bytes_so_far(),
                   base::Passed(&hash_state)));
  }
  if (bound_net_log_.IsCapturing()) {
    bound_net_log_.AddEvent(
        net::NetLog::TYPE_DOWNLOAD_STREAM_DRAINED,
        base::Bind(&FileStreamDrainedNetLogCallback, total_incoming_data_size,
                   num_buffers));
  }
}

void DownloadFileImpl::SendUpdate() {
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&DownloadDestinationObserver::DestinationUpdate,
                 observer_,
                 file_.bytes_so_far(),
                 rate_estimator_.GetCountPerSecond()));
}

DownloadFileImpl::RenameParameters::RenameParameters(
    RenameOption option,
    const base::FilePath& new_path,
    const RenameCompletionCallback& completion_callback)
    : option(option),
      new_path(new_path),
      retries_left(kMaxRenameRetries),
      completion_callback(completion_callback) {}

DownloadFileImpl::RenameParameters::~RenameParameters() {}

}  // namespace content
