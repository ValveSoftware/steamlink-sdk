// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VT_MAC_H_
#define MEDIA_GPU_VT_MAC_H_

// Dynamic library loader.
#include "media/gpu/vt_stubs.h"

// CoreMedia and VideoToolbox types.
#include "media/gpu/vt_stubs_header.fragment"

// CoreMedia and VideoToolbox functions.
extern "C" {
#include "media/gpu/vt.sig"
}  // extern "C"

#endif  // MEDIA_GPU_VT_MAC_H_
