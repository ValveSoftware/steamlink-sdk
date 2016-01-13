// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_CONTENT_URI_UTILS_H_
#define BASE_ANDROID_CONTENT_URI_UTILS_H_

#include <jni.h>

#include "base/base_export.h"
#include "base/basictypes.h"
#include "base/files/file.h"
#include "base/files/file_path.h"

namespace base {

bool RegisterContentUriUtils(JNIEnv* env);

// Opens a content uri for read and returns the file descriptor to the caller.
// Returns -1 if the uri is invalid.
BASE_EXPORT File OpenContentUriForRead(const FilePath& content_uri);

// Check whether a content uri exists.
BASE_EXPORT bool ContentUriExists(const FilePath& content_uri);

}  // namespace base

#endif  // BASE_ANDROID_CONTENT_URI_UTILS_H_
