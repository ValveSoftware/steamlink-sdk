// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_file_impl.h"

#include <string>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "content/browser/byte_stream.h"
#include "content/browser/download/download_create_info.h"
#include "content/browser/download/download_interrupt_reasons_impl.h"
#include "content/browser/download/download_net_log_parameters.h"
#include "content/browser/download/download_stats.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_destination_observer.h"
#include "net/base/io_buffer.h"

namespace content {

const int kUpdatePeriodMs = 500;
const int kMaxTimeBlockingFileThreadMs = 1000;

int DownloadFile::number_active_objects_ = 0;

DownloadFileImpl::DownloadFileImpl(
    scoped_ptr<DownloadSaveInfo> save_info,
    const base::FilePath& default_download_directory,
    const GURL& url,
    const GURL& referrer_url,
    bool calculate_hash,
    scoped_ptr<ByteStreamReader> stream,
    const net::BoundNetLog& bound_net_log,
    base::WeakPtr<DownloadDestinationObserver> observer)
        : file_(save_info->file_path,
                url,
                referrer_url,
                save_info->offset,
                calculate_hash,
                save_info->hash_state,
                save_info->file.Pass(),
                bound_net_log),
          default_download_directory_(default_download_directory),
          stream_reader_(stream.Pass()),
          bytes_seen_(0),
          bound_net_log_(bound_net_log),
          observer_(observer),
          weak_factory_(this) {
}

DownloadFileImpl::~DownloadFileImpl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  --number_active_objects_;
}

void DownloadFileImpl::Initialize(const InitializeCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  update_timer_.reset(new base::RepeatingTimer<DownloadFileImpl>());
  DownloadInterruptReason result =
      file_.Initialize(default_download_directory_);
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

  ++number_active_objects_;
}

DownloadInterruptReason DownloadFileImpl::AppendDataToFile(
    const char* data, size_t data_len) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  base::FilePath new_path(full_path);

  int uniquifier = base::GetUniquePathNumber(
      new_path, base::FilePath::StringType());
  if (uniquifier > 0) {
    new_path = new_path.InsertBeforeExtensionASCII(
        base::StringPrintf(" (%d)", uniquifier));
  }

  DownloadInterruptReason reason = file_.Rename(new_path);
  if (reason != DOWNLOAD_INTERRUPT_REASON_NONE) {
    // Make sure our information is updated, since we're about to
    // error out.
    SendUpdate();

    // Null out callback so that we don't do any more stream processing.
    stream_reader_->RegisterCallback(base::Closure());

    new_path.clear();
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(callback, reason, new_path));
}

void DownloadFileImpl::RenameAndAnnotate(
    const base::FilePath& full_path,
    const RenameCompletionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  base::FilePath new_path(full_path);

  DownloadInterruptReason reason = DOWNLOAD_INTERRUPT_REASON_NONE;
  // Short circuit null rename.
  if (full_path != file_.full_path())
    reason = file_.Rename(new_path);

  if (reason == DOWNLOAD_INTERRUPT_REASON_NONE) {
    // Doing the annotation after the rename rather than before leaves
    // a very small window during which the file has the final name but
    // hasn't been marked with the Mark Of The Web.  However, it allows
    // anti-virus scanners on Windows to actually see the data
    // (http://crbug.com/127999) under the correct name (which is information
    // it uses).
    reason = file_.AnnotateWithSourceInformation();
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
      BrowserThread::UI, FROM_HERE,
      base::Bind(callback, reason, new_path));
}

void DownloadFileImpl::Detach() {
  file_.Detach();
}

void DownloadFileImpl::Cancel() {
  file_.Cancel();
}

base::FilePath DownloadFileImpl::FullPath() const {
  return file_.full_path();
}

bool DownloadFileImpl::InProgress() const {
  return file_.in_progress();
}

int64 DownloadFileImpl::CurrentSpeed() const {
  return rate_estimator_.GetCountPerSecond();
}

bool DownloadFileImpl::GetHash(std::string* hash) {
  return file_.GetHash(hash);
}

std::string DownloadFileImpl::GetHashState() {
  return file_.GetHashState();
}

void DownloadFileImpl::SetClientGuid(const std::string& guid) {
  file_.SetClientGuid(guid);
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
          file_.Finish();
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
    SendUpdate();                       // Make info up to date before error.
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&DownloadDestinationObserver::DestinationError,
                   observer_, reason));
  } else if (state == ByteStreamReader::STREAM_COMPLETE) {
    // Signal successful completion and shut down processing.
    stream_reader_->RegisterCallback(base::Closure());
    weak_factory_.InvalidateWeakPtrs();
    std::string hash;
    if (!GetHash(&hash) || file_.IsEmptyHash(hash))
      hash.clear();
    SendUpdate();
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(
            &DownloadDestinationObserver::DestinationCompleted,
            observer_, hash));
  }
  if (bound_net_log_.IsLogging()) {
    bound_net_log_.AddEvent(
        net::NetLog::TYPE_DOWNLOAD_STREAM_DRAINED,
        base::Bind(&FileStreamDrainedNetLogCallback, total_incoming_data_size,
                   num_buffers));
  }
}

void DownloadFileImpl::SendUpdate() {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&DownloadDestinationObserver::DestinationUpdate,
                 observer_, file_.bytes_so_far(), CurrentSpeed(),
                 GetHashState()));
}

// static
int DownloadFile::GetNumberOfDownloadFiles() {
  return number_active_objects_;
}

}  // namespace content
