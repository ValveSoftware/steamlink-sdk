// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef WebSharedWorkerCreationContextType_h
#define WebSharedWorkerCreationContextType_h

#include "../platform/WebCommon.h"

namespace blink {

// Describes the type of context (secure or non-secure) that created a SharedWorker.
enum WebSharedWorkerCreationContextType {
    // The shared worker was created from a nonsecure context.
    WebSharedWorkerCreationContextTypeNonsecure = 0,
    // The shared worker was created from a secure context.
    WebSharedWorkerCreationContextTypeSecure,
    WebSharedWorkerCreationContextTypeLast = WebSharedWorkerCreationContextTypeSecure
};

} // namespace blink

#endif // WebSharedWorkerCreationContextType_h
