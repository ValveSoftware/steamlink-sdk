// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/remoting_cdm_context.h"

#include "media/remoting/remoting_cdm.h"
#include "media/remoting/remoting_source_impl.h"

namespace media {

namespace {
// Used as an identifier for RemotingCdmContext::From().
void* const kClassIdentifier = const_cast<void**>(&kClassIdentifier);
}  // namespace

// TODO(xjz): Merge this with erickung's implementation.
RemotingCdmContext::RemotingCdmContext(RemotingCdm* remoting_cdm)
    : remoting_cdm_(remoting_cdm) {}

RemotingCdmContext::~RemotingCdmContext() {}

RemotingCdmContext* RemotingCdmContext::From(CdmContext* cdm_context) {
  if (cdm_context && cdm_context->GetClassIdentifier() == kClassIdentifier)
    return static_cast<RemotingCdmContext*>(cdm_context);
  return nullptr;
}

Decryptor* RemotingCdmContext::GetDecryptor() {
  // TODO(xjz): Merge with erickung's implementation.
  return nullptr;
}

int RemotingCdmContext::GetCdmId() const {
  // TODO(xjz): Merge with erickung's implementation.
  return CdmContext::kInvalidCdmId;
}

void* RemotingCdmContext::GetClassIdentifier() const {
  return kClassIdentifier;
}

RemotingSourceImpl* RemotingCdmContext::GetRemotingSource() {
  return remoting_cdm_->GetRemotingSource();
}

}  // namespace media
