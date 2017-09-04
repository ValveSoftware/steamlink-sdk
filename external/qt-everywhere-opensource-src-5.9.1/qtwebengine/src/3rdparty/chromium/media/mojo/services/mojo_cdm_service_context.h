// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_CDM_SERVICE_CONTEXT_H_
#define MEDIA_MOJO_SERVICES_MOJO_CDM_SERVICE_CONTEXT_H_

#include <stdint.h>

#include <map>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "media/mojo/services/media_mojo_export.h"

namespace media {

class MediaKeys;
class MojoCdmService;

// A class that creates, owns and manages all MojoCdmService instances.
class MEDIA_MOJO_EXPORT MojoCdmServiceContext {
 public:
  MojoCdmServiceContext();
  ~MojoCdmServiceContext();

  base::WeakPtr<MojoCdmServiceContext> GetWeakPtr();

  // Registers The |cdm_service| with |cdm_id|.
  void RegisterCdm(int cdm_id, MojoCdmService* cdm_service);

  // Unregisters the CDM. Must be called before the CDM is destroyed.
  void UnregisterCdm(int cdm_id);

  // Returns the CDM associated with |cdm_id|.
  scoped_refptr<MediaKeys> GetCdm(int cdm_id);

 private:
  // A map between CDM ID and MojoCdmService.
  std::map<int, MojoCdmService*> cdm_services_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<MojoCdmServiceContext> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MojoCdmServiceContext);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_CDM_SERVICE_CONTEXT_H_
