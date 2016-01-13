// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains code that should be used for initializing, or querying the state
// of the media library as a whole.

#ifndef MEDIA_BASE_MEDIA_H_
#define MEDIA_BASE_MEDIA_H_

#include "media/base/media_export.h"

namespace base {
class FilePath;
}

namespace media {

// Attempts to initialize the media library (loading DLLs, DSOs, etc.).
//
// If |module_dir| is the empty string, then the system default library paths
// are searched for the dynamic libraries.  If a |module_dir| is provided, then
// only the specified |module_dir| will be searched for the dynamic libraries.
//
// If multiple initializations are attempted with different |module_dir|s
// specified then the first one to succeed remains effective for the lifetime
// of the process.
//
// Returns true if everything was successfully initialized, false otherwise.
MEDIA_EXPORT bool InitializeMediaLibrary(const base::FilePath& module_dir);

// Helper function for unit tests to avoid boiler plate code everywhere. This
// function will crash if it fails to load the media library. This ensures tests
// fail if the media library is not available.
MEDIA_EXPORT void InitializeMediaLibraryForTesting();

// Use this if you need to check whether the media library is initialized
// for the this process, without actually trying to initialize it.
MEDIA_EXPORT bool IsMediaLibraryInitialized();

// Use this if you need to initialize CPU specific features WITHOUT loading
// DLLs, DSOs, etc.  Only necessary if InitializeMediaLibrary() is not called;
// does nothing if the media library has already been initialized.
MEDIA_EXPORT void InitializeCPUSpecificMediaFeatures();

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_H_
