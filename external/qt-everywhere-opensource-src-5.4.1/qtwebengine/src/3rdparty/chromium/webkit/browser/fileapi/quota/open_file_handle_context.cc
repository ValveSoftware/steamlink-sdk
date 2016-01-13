// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/fileapi/quota/open_file_handle_context.h"

#include "base/file_util.h"
#include "webkit/browser/fileapi/quota/quota_reservation_buffer.h"

namespace fileapi {

OpenFileHandleContext::OpenFileHandleContext(
    const base::FilePath& platform_path,
    QuotaReservationBuffer* reservation_buffer)
    : initial_file_size_(0),
      maximum_written_offset_(0),
      append_mode_write_amount_(0),
      platform_path_(platform_path),
      reservation_buffer_(reservation_buffer) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

  base::GetFileSize(platform_path, &initial_file_size_);
  maximum_written_offset_ = initial_file_size_;
}

int64 OpenFileHandleContext::UpdateMaxWrittenOffset(int64 offset) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  if (offset <= maximum_written_offset_)
    return 0;

  int64 growth = offset - maximum_written_offset_;
  maximum_written_offset_ = offset;
  return growth;
}

void OpenFileHandleContext::AddAppendModeWriteAmount(int64 amount) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  append_mode_write_amount_ += amount;
}

int64 OpenFileHandleContext::GetEstimatedFileSize() const {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  return maximum_written_offset_ + append_mode_write_amount_;
}

int64 OpenFileHandleContext::GetMaxWrittenOffset() const {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  return maximum_written_offset_;
}

OpenFileHandleContext::~OpenFileHandleContext() {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

  // TODO(tzik): Optimize this for single operation.

  int64 file_size = 0;
  base::GetFileSize(platform_path_, &file_size);
  int64 usage_delta = file_size - initial_file_size_;

  // |reserved_quota_consumption| may be greater than the recorded file growth
  // when a plugin crashed before reporting its consumption.
  // In this case, the reserved quota for the plugin should be handled as
  // consumed quota.
  int64 reserved_quota_consumption =
      std::max(GetEstimatedFileSize(), file_size) - initial_file_size_;

  reservation_buffer_->CommitFileGrowth(
      reserved_quota_consumption, usage_delta);
  reservation_buffer_->DetachOpenFileHandleContext(this);
}

}  // namespace fileapi
