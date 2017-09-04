// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/runner/host/native_library_runner.h"

#include <stddef.h>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "mojo/edk/embedder/entrypoints.h"
#include "mojo/public/c/system/thunks.h"

namespace service_manager {

namespace {

template <typename Thunks>
bool SetThunks(Thunks (*make_thunks)(),
               const char* function_name,
               base::NativeLibrary library) {
  typedef size_t (*SetThunksFn)(const Thunks* thunks);
  SetThunksFn set_thunks = reinterpret_cast<SetThunksFn>(
      base::GetFunctionPointerFromNativeLibrary(library, function_name));
  if (!set_thunks)
    return false;
  Thunks thunks = make_thunks();
  size_t expected_size = set_thunks(&thunks);
  if (expected_size > sizeof(Thunks)) {
    LOG(ERROR) << "Invalid library: expected " << function_name
               << " to return thunks of size: " << expected_size;
    return false;
  }
  return true;
}

}  // namespace

base::NativeLibrary LoadNativeLibrary(const base::FilePath& path) {
  DVLOG(2) << "Loading Service in process from library: " << path.value();

  base::NativeLibraryLoadError error;
  base::NativeLibrary library = base::LoadNativeLibrary(path, &error);
  LOG_IF(ERROR, !library)
      << "Failed to load library (path: " << path.value()
      << " reason: " << error.ToString() << ")";
  return library;
}

bool RunServiceInNativeLibrary(base::NativeLibrary library,
                               mojom::ServiceRequest request) {
  // Tolerate |library| being null, to make life easier for callers.
  if (!library)
    return false;

// Thunks aren't needed/used in component build, since the thunked methods
// just live in their own dynamically loaded library.
#if !defined(COMPONENT_BUILD)
  if (!SetThunks(&mojo::edk::MakeSystemThunks, "MojoSetSystemThunks",
                 library)) {
    LOG(ERROR) << "MojoSetSystemThunks not found";
    return false;
  }

#if !defined(OS_WIN)
  // On Windows, initializing base::CommandLine with null parameters gets the
  // process's command line from the OS. Other platforms need it to be passed
  // in. This needs to be passed in before the service initializes the command
  // line, which is done as soon as it loads.
  typedef void (*InitCommandLineArgs)(int, const char* const*);
  InitCommandLineArgs init_command_line_args =
      reinterpret_cast<InitCommandLineArgs>(
          base::GetFunctionPointerFromNativeLibrary(library,
                                                    "InitCommandLineArgs"));
  if (init_command_line_args) {
    int argc = 0;
    base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
    const char** argv = new const char*[cmd_line->argv().size()];
    for (auto& arg : cmd_line->argv())
      argv[argc++] = arg.c_str();
    init_command_line_args(argc, argv);
  }
#endif

#endif  // !defined(COMPONENT_BUILD)

  typedef MojoResult (*ServiceMainFunction)(MojoHandle);
  ServiceMainFunction main_function = reinterpret_cast<ServiceMainFunction>(
      base::GetFunctionPointerFromNativeLibrary(library, "ServiceMain"));
  if (!main_function) {
    LOG(ERROR) << "ServiceMain not found";
    return false;
  }
  // |ServiceMain()| takes ownership of the service handle.
  MojoHandle handle = request.PassMessagePipe().release().value();
  MojoResult result = main_function(handle);
  if (result != MOJO_RESULT_OK) {
    LOG(ERROR) << "ServiceMain returned error (result: " << result << ")";
  }
  return true;
}

}  // namespace service_manager
