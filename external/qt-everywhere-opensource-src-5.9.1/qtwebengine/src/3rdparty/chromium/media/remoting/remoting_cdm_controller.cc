// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/remoting_cdm_controller.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/threading/thread_checker.h"

namespace media {

RemotingCdmController::RemotingCdmController(
    scoped_refptr<RemotingSourceImpl> remoting_source)
    : remoting_source_(std::move(remoting_source)) {
  remoting_source_->AddClient(this);
}

RemotingCdmController::~RemotingCdmController() {
  DCHECK(thread_checker_.CalledOnValidThread());

  remoting_source_->RemoveClient(this);
}

void RemotingCdmController::OnStarted(bool success) {
  DCHECK(thread_checker_.CalledOnValidThread());

  DCHECK(!cdm_check_cb_.is_null());
  is_remoting_ = success;
  base::ResetAndReturn(&cdm_check_cb_).Run(success);
}

void RemotingCdmController::OnSessionStateChanged() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (is_remoting_ &&
      remoting_source_->state() == RemotingSessionState::SESSION_STOPPING) {
    remoting_source_->Shutdown();
    is_remoting_ = false;
  }
}

void RemotingCdmController::ShouldCreateRemotingCdm(
    const CdmCheckCallback& cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!cb.is_null());

  if (is_remoting_) {
    cb.Run(true);
    return;
  }

  DCHECK(cdm_check_cb_.is_null());
  cdm_check_cb_ = cb;
  remoting_source_->StartRemoting(this);
}

}  // namespace media
