// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_PPAPI_SUPPORTED_CDM_VERSIONS_H_
#define MEDIA_CDM_PPAPI_SUPPORTED_CDM_VERSIONS_H_

#include "media/cdm/ppapi/api/content_decryption_module.h"

namespace media {

bool IsSupportedCdmModuleVersion(int version) {
  switch(version) {
    // Latest.
    case CDM_MODULE_VERSION:
      return true;
    default:
      return false;
  }
}

bool IsSupportedCdmInterfaceVersion(int version) {
  COMPILE_ASSERT(cdm::ContentDecryptionModule::kVersion ==
                     cdm::ContentDecryptionModule_5::kVersion,
                 update_code_below);
  switch(version) {
    // Supported versions in decreasing order.
    case cdm::ContentDecryptionModule_5::kVersion:
    case cdm::ContentDecryptionModule_4::kVersion:
      return true;
    default:
      return false;
  }
}

bool IsSupportedCdmHostVersion(int version) {
  COMPILE_ASSERT(cdm::ContentDecryptionModule::Host::kVersion ==
                     cdm::ContentDecryptionModule_5::Host::kVersion,
                 update_code_below);
  switch(version) {
    // Supported versions in decreasing order.
    case cdm::Host_5::kVersion:
    case cdm::Host_4::kVersion:
      return true;
    default:
      return false;
  }
}

}  // namespace media

#endif  // MEDIA_CDM_PPAPI_SUPPORTED_CDM_VERSIONS_H_
