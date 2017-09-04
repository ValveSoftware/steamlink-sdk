// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_REMOTING_REMOTING_CDM_CONTEXT_H_
#define MEDIA_REMOTING_REMOTING_CDM_CONTEXT_H_

#include "media/base/cdm_context.h"

namespace media {

class RemotingCdm;
class RemotingSourceImpl;

// TODO(xjz): Merge this with erickung's implementation.
class RemotingCdmContext : public CdmContext {
 public:
  explicit RemotingCdmContext(RemotingCdm* remoting_cdm);
  ~RemotingCdmContext() override;

  // If |cdm_context| is an instance of RemotingCdmContext, return a type-casted
  // pointer to it. Otherwise, return nullptr.
  static RemotingCdmContext* From(CdmContext* cdm_context);

  RemotingSourceImpl* GetRemotingSource();

  // CdmContext implementations.
  Decryptor* GetDecryptor() override;
  int GetCdmId() const override;
  void* GetClassIdentifier() const override;

 private:
  RemotingCdm* const remoting_cdm_;  // Outlives this class.

  DISALLOW_COPY_AND_ASSIGN(RemotingCdmContext);
};

}  // namespace media

#endif  // MEDIA_REMOTING_REMOTING_CDM_CONTEXT_H_
