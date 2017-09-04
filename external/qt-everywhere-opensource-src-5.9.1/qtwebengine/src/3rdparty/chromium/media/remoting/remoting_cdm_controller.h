// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_REMOTING_REMOTING_CDM_CONTROLLER_H_
#define MEDIA_REMOTING_REMOTING_CDM_CONTROLLER_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "media/remoting/remoting_source_impl.h"

namespace media {

// This class controlls whether to start a remoting session to create CDM.
// The |remoting_source_| will be passed to the RemotingRendererController when
// the CDM is attached to a media element.
class RemotingCdmController final : public RemotingSourceImpl::Client {
 public:
  explicit RemotingCdmController(
      scoped_refptr<RemotingSourceImpl> remoting_source);
  ~RemotingCdmController();

  // RemotingSourceImpl::Client implementations.
  void OnStarted(bool success) override;
  void OnSessionStateChanged() override;

  // Returns whether we should create remoting CDM via |cb|, which could be run
  // synchronously or asynchronously, depending on whether the required
  // information is available now or later.
  using CdmCheckCallback = base::Callback<void(bool is_remoting)>;
  void ShouldCreateRemotingCdm(const CdmCheckCallback& cb);

  RemotingSourceImpl* remoting_source() const {
    DCHECK(thread_checker_.CalledOnValidThread());
    return remoting_source_.get();
  }

 private:
  const scoped_refptr<RemotingSourceImpl> remoting_source_;

  // This callback is run once to report whether to create remoting CDM.
  CdmCheckCallback cdm_check_cb_;

  // Indicates if the session is in remoting.
  bool is_remoting_ = false;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(RemotingCdmController);
};

}  // namespace media

#endif  // MEDIA_REMOTING_REMOTING_CDM_CONTROLLER_H_
