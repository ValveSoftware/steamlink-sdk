// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_PPAPI_EXTERNAL_CLEAR_KEY_CLEAR_KEY_CDM_COMMON_H_
#define MEDIA_CDM_PPAPI_EXTERNAL_CLEAR_KEY_CLEAR_KEY_CDM_COMMON_H_

#include "media/cdm/ppapi/api/content_decryption_module.h"

namespace media {

// Aliases for the version of the interfaces that this CDM implements.
typedef cdm::ContentDecryptionModule_5 ClearKeyCdmInterface;
typedef ClearKeyCdmInterface::Host ClearKeyCdmHost;

}  // namespace media

#endif  // MEDIA_CDM_PPAPI_EXTERNAL_CLEAR_KEY_CLEAR_KEY_CDM_COMMON_H_
