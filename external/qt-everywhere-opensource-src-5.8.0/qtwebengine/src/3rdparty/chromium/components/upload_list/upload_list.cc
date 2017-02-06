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
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread_task_runner_handle.h"

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

UploadList::UploadInfo::UploadInfo(const std::string& upload_id,
                                   const base::Time& upload_time)
    : upload_id(upload_id), upload_time(upload_time), state(State::Uploaded) {}

UploadList::UploadInfo::~UploadInfo() {}

UploadList::UploadList(
    Delegate* delegate,
    const base::FilePath& upload_log_path,
    const scoped_refptr<base::SequencedWorkerPool>& worker_pool)
    : delegate_(delegate),
      upload_log_path_(upload_log_path),
      worker_pool_(worker_pool) {}

UploadList::~UploadList() {}

void UploadList::LoadUploadListAsynchronously() {
  DCHECK(thread_checker_.CalledOnValidThread());
  worker_pool_->PostTask(
      FROM_HERE,
      base::Bind(&UploadList::PerformLoadAndNotifyDelegate,
                 this, base::ThreadTaskRunnerHandle::Get()));
}

void UploadList::ClearDelegate() {
  DCHECK(thread_checker_.CalledOnValidThread());
  delegate_ = NULL;
}

void UploadList::PerformLoadAndNotifyDelegate(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner) {
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
  DCHECK(thread_checker_.CalledOnValidThread());
  uploads_ = std::move(uploads);
  if (delegate_)
    delegate_->OnUploadListAvailable();
}

void UploadList::GetUploads(size_t max_count,
                            std::vector<UploadInfo>* uploads) {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::copy(uploads_.begin(),
            uploads_.begin() + std::min(uploads_.size(), max_count),
            std::back_inserter(*uploads));
}
