// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file intentionally does not have header guards, it's included
// inside a macro to generate enum and a java class for the values.

#ifndef DEFINE_ANDROID_IMAGEFORMAT
#error "DEFINE_ANDROID_IMAGEFORMAT should be defined."
#endif

// Android graphics ImageFormat mapping, see reference in:
// http://developer.android.com/reference/android/graphics/ImageFormat.html

DEFINE_ANDROID_IMAGEFORMAT(ANDROID_IMAGEFORMAT_NV21, 17)
DEFINE_ANDROID_IMAGEFORMAT(ANDROID_IMAGEFORMAT_YV12, 842094169)

DEFINE_ANDROID_IMAGEFORMAT(ANDROID_IMAGEFORMAT_UNKNOWN, 0)
