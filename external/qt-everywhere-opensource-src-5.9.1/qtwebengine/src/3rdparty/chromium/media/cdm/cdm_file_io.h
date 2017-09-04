// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_CDM_FILE_IO_H_
#define MEDIA_CDM_CDM_FILE_IO_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "media/base/media_export.h"
#include "media/cdm/api/content_decryption_module.h"

namespace media {

// Implements a version of cdm::FileIO with a public destructor so it can be
// used with std::unique_ptr.
class MEDIA_EXPORT CdmFileIO : NON_EXPORTED_BASE(public cdm::FileIO) {
 public:
  ~CdmFileIO() override;

 protected:
  CdmFileIO();

 private:
  DISALLOW_COPY_AND_ASSIGN(CdmFileIO);
};

}  // namespace media

#endif  // MEDIA_CDM_CDM_FILE_IO_H_
