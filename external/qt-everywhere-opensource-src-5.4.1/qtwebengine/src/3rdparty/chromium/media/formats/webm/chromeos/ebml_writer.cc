// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/webm/chromeos/ebml_writer.h"

#include "media/base/media_export.h"

extern "C" {
#include "third_party/libvpx/source/libvpx/third_party/libmkv/EbmlWriter.h"

EbmlGlobal::EbmlGlobal() {
}

EbmlGlobal::~EbmlGlobal() {
}

// These functions must be in the global namespace and visible to libmkv.

void MEDIA_EXPORT Ebml_Write(EbmlGlobal* glob,
                             const void* buffer,
                             unsigned long len) {
  glob->write_cb.Run(buffer, len);
}

void MEDIA_EXPORT Ebml_Serialize(EbmlGlobal* glob,
                                 const void* buffer,
                                 int buffer_size,
                                 unsigned long len) {
  glob->serialize_cb.Run(buffer, buffer_size, len);
}

}  // extern "C"
