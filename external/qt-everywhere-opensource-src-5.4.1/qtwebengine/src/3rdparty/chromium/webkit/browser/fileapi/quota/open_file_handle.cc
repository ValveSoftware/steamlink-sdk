// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/fileapi/quota/open_file_handle.h"

#include "webkit/browser/fileapi/quota/open_file_handle_context.h"
#include "webkit/browser/fileapi/quota/quota_reservation.h"

namespace fileapi {

OpenFileHandle::~OpenFileHandle() {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
}

void OpenFileHandle::UpdateMaxWrittenOffset(int64 offset) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

  int64 growth = context_->UpdateMaxWrittenOffset(offset);
  if (growth > 0)
    reservation_->ConsumeReservation(growth);
}

void OpenFileHandle::AddAppendModeWriteAmount(int64 amount) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  if (amount <= 0)
    return;

  context_->AddAppendModeWriteAmount(amount);
  reservation_->ConsumeReservation(amount);
}

int64 OpenFileHandle::GetEstimatedFileSize() const {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  return context_->GetEstimatedFileSize();
}

int64 OpenFileHandle::GetMaxWrittenOffset() const {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  return context_->GetMaxWrittenOffset();
}

const base::FilePath& OpenFileHandle::platform_path() const {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  return context_->platform_path();
}

OpenFileHandle::OpenFileHandle(QuotaReservation* reservation,
                               OpenFileHandleContext* context)
    : reservation_(reservation),
      context_(context) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
}

}  // namespace fileapi
