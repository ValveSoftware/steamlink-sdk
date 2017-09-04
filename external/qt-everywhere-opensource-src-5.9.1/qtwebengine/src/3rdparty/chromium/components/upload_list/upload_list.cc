// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/upload_list/upload_list.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"

UploadList::UploadInfo::UploadInfo(const std::string& upload_id,
                                   const base::Time& upload_time,
                                   const std::string& local_id,
                                   const base::Time& capture_time,
                                   State state)
    : upload_id(upload_id),
      upload_time(upload_time),
      local_id(local_id),
      capture_time(capture_time),
      state(state) {}

UploadList::UploadInfo::UploadInfo(const std::string& local_id,
                                   const base::Time& capture_time,
                                   State state,
                                   const base::string16& file_size)
    : local_id(local_id),
      capture_time(capture_time),
      state(state),
      file_size(file_size) {}

UploadList::UploadInfo::UploadInfo(const std::string& upload_id,
                                   const base::Time& upload_time)
    : upload_id(upload_id), upload_time(upload_time), state(State::Uploaded) {}

UploadList::UploadInfo::UploadInfo(const UploadInfo& upload_info)
    : upload_id(upload_info.upload_id),
      upload_time(upload_info.upload_time),
      local_id(upload_info.local_id),
      capture_time(upload_info.capture_time),
      state(upload_info.state),
      file_size(upload_info.file_size) {}

UploadList::UploadInfo::~UploadInfo() = default;

UploadList::UploadList(Delegate* delegate,
                       const base::FilePath& upload_log_path,
                       scoped_refptr<base::TaskRunner> task_runner)
    : delegate_(delegate),
      upload_log_path_(upload_log_path),
      task_runner_(std::move(task_runner)) {}

UploadList::~UploadList() = default;

void UploadList::LoadUploadListAsynchronously() {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&UploadList::PerformLoadAndNotifyDelegate, this,
                            base::SequencedTaskRunnerHandle::Get()));
}

void UploadList::ClearDelegate() {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  delegate_ = NULL;
}

void UploadList::PerformLoadAndNotifyDelegate(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  std::vector<UploadInfo> uploads;
  LoadUploadList(&uploads);
  task_runner->PostTask(
      FROM_HERE,
      base::Bind(&UploadList::SetUploadsAndNotifyDelegate, this,
                 std::move(uploads)));
}

void UploadList::LoadUploadList(std::vector<UploadInfo>* uploads) {
  if (base::PathExists(upload_log_path_)) {
    std::string contents;
    base::ReadFileToString(upload_log_path_, &contents);
    std::vector<std::string> log_entries = base::SplitString(
        contents, base::kWhitespaceASCII, base::KEEP_WHITESPACE,
        base::SPLIT_WANT_NONEMPTY);
    ParseLogEntries(log_entries, uploads);
  }
}

const base::FilePath& UploadList::upload_log_path() const {
  return upload_log_path_;
}

void UploadList::ParseLogEntries(
    const std::vector<std::string>& log_entries,
    std::vector<UploadInfo>* uploads) {
  std::vector<std::string>::const_reverse_iterator i;
  for (i = log_entries.rbegin(); i != log_entries.rend(); ++i) {
    std::vector<std::string> components = base::SplitString(
        *i, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    // Skip any blank (or corrupted) lines.
    if (components.size() < 2 || components.size() > 5)
      continue;
    base::Time upload_time;
    double seconds_since_epoch;
    if (!components[0].empty()) {
      if (!base::StringToDouble(components[0], &seconds_since_epoch))
        continue;
      upload_time = base::Time::FromDoubleT(seconds_since_epoch);
    }
    UploadInfo info(components[1], upload_time);

    // Add local ID if present.
    if (components.size() > 2)
      info.local_id = components[2];

    // Add capture time if present.
    if (components.size() > 3 &&
        !components[3].empty() &&
        base::StringToDouble(components[3], &seconds_since_epoch)) {
      info.capture_time = base::Time::FromDoubleT(seconds_since_epoch);
    }

    int state;
    if (components.size() > 4 &&
        !components[4].empty() &&
        base::StringToInt(components[4], &state)) {
      info.state = static_cast<UploadInfo::State>(state);
    }

    uploads->push_back(info);
  }
}

void UploadList::SetUploadsAndNotifyDelegate(std::vector<UploadInfo> uploads) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  uploads_ = std::move(uploads);
  if (delegate_)
    delegate_->OnUploadListAvailable();
}

void UploadList::GetUploads(size_t max_count,
                            std::vector<UploadInfo>* uploads) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  std::copy(uploads_.begin(),
            uploads_.begin() + std::min(uploads_.size(), max_count),
            std::back_inserter(*uploads));
}

void UploadList::RequestSingleCrashUploadAsync(const std::string& local_id) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&UploadList::RequestSingleCrashUpload, this, local_id));
}

void UploadList::RequestSingleCrashUpload(const std::string& local_id) {
  // Manual uploads for not-yet uploaded crash reports are only available for
  // Crashpad systems and for Android.
  NOTREACHED();
}
