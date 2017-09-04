// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_RUNNER_HOST_NATIVE_LIBRARY_RUNNER_H_
#define SERVICES_SERVICE_MANAGER_RUNNER_HOST_NATIVE_LIBRARY_RUNNER_H_

#include "base/native_library.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/service_manager/public/interfaces/service.mojom.h"

namespace base {
class FilePath;
}

namespace service_manager {

// Loads the native Service from the DSO specified by |app_path|.
// Returns the |base::NativeLibrary| for the service on success (or null on
// failure).
//
// Note: The caller may choose to eventually unload the returned DSO. If so,
// this should be done only after the thread on which |LoadNativeLibrary()|
// and |RunServiceInNativeLibrary()| were called has terminated, so that any
// thread-local destructors have been executed.
base::NativeLibrary LoadNativeLibrary(const base::FilePath& app_path);

// Runs the native Service from the DSO that was loaded using
// |LoadNativeLibrary()|; this tolerates |library| being null. This should be
// called on the same thread as |LoadNativeLibrary()|. Returns true if
// |ServiceMain()| was called (even if it returns an error), and false
// otherwise.
bool RunServiceInNativeLibrary(base::NativeLibrary library,
                               mojom::ServiceRequest request);

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_RUNNER_HOST_NATIVE_LIBRARY_RUNNER_H_
