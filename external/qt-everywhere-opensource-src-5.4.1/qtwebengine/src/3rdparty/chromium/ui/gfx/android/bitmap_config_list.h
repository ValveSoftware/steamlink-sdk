// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file intentionally does not have header guards, it's included
// inside a macro to generate enum values.

#ifndef DEFINE_BITMAP_CONFIG
#error "DEFINE_BITMAP_CONFIG should be defined before including this file"
#endif
DEFINE_BITMAP_CONFIG(FORMAT_NO_CONFIG, 0)
DEFINE_BITMAP_CONFIG(FORMAT_ALPHA_8, 1)
DEFINE_BITMAP_CONFIG(FORMAT_ARGB_4444, 2)
DEFINE_BITMAP_CONFIG(FORMAT_ARGB_8888, 3)
DEFINE_BITMAP_CONFIG(FORMAT_RGB_565, 4)
