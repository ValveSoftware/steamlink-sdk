// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_WEBM_CHROMEOS_EBML_WRITER_H_
#define MEDIA_FORMATS_WEBM_CHROMEOS_EBML_WRITER_H_

#include "base/callback.h"

// This struct serves as a bridge betweeen static libmkv interface and Chrome's
// base::Callback. Must be in the global namespace. See EbmlWriter.h.
struct EbmlGlobal {
  EbmlGlobal();
  ~EbmlGlobal();

  base::Callback<void(const void* buffer, unsigned long len)> write_cb;
  base::Callback<void(const void* buffer, int buffer_size, unsigned long len)>
      serialize_cb;
};

#endif  // MEDIA_FORMATS_WEBM_CHROMEOS_EBML_WRITER_H_
