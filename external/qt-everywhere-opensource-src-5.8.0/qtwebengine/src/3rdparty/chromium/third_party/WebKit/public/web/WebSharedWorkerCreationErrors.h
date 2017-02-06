// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef WebSharedWorkerCreationErrors_h
#define WebSharedWorkerCreationErrors_h

#include "../platform/WebCommon.h"

namespace blink {

// Describes errors that can occur while creating a SharedWorker.
enum WebWorkerCreationError {
    WebWorkerCreationErrorNone = 0,
    WebWorkerCreationErrorURLMismatch,
    WebWorkerCreationErrorSecureContextMismatch,
    WebWorkerCreationErrorLast = WebWorkerCreationErrorSecureContextMismatch
};

} // namespace blink

#endif // WebSharedWorkerCreationErrors_h
