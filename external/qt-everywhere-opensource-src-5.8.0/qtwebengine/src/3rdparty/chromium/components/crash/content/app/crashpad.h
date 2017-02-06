// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CONTENT_APP_CRASHPAD_H_
#define COMPONENTS_CRASH_CONTENT_APP_CRASHPAD_H_

#include <time.h>

#include <map>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "third_party/kasko/kasko_features.h"

#if BUILDFLAG(ENABLE_KASKO)
#include "base/process/process.h"
#include "syzygy/kasko/api/crash_key.h"
#endif  // BUILDFLAG(ENABLE_KASKO)

namespace crash_reporter {

// Initializes Crashpad in a way that is appropriate for initial_client and
// process_type.
//
// If initial_client is true, this starts crashpad_handler and sets it as the
// exception handler. Child processes will inherit this exception handler, and
// should specify false for this parameter. Although they inherit the exception
// handler, child processes still need to call this function to perform
// additional initialization.
//
// If process_type is empty, initialization will be done for the browser
// process. The browser process performs additional initialization of the crash
// report database. The browser process is also the only process type that is
// eligible to have its crashes forwarded to the system crash report handler (in
// release mode only). Note that when process_type is empty, initial_client must
// be true.
//
// On Mac, process_type may be non-empty with initial_client set to true. This
// indicates that an exception handler has been inherited but should be
// discarded in favor of a new Crashpad handler. This configuration should be
// used infrequently. It is provided to allow an install-from-.dmg relauncher
// process to disassociate from an old Crashpad handler so that after performing
// an installation from a disk image, the relauncher process may unmount the
// disk image that contains its inherited crashpad_handler. This is only
// supported when initial_client is true and process_type is "relauncher".
//
// On Windows, use InitializeCrashpadWithEmbeddedHandler() when crashpad_handler
// is embedded into this binary and can be started by launching the current
// process with --type=crashpad-handler. Otherwise, this function should be used
// and will launch an external crashpad_handler.exe which is generally used for
// test situations.
void InitializeCrashpad(bool initial_client, const std::string& process_type);

#if defined(OS_WIN)
// This is the same as InitializeCrashpad(), but rather than launching a
// crashpad_handler executable, relaunches the current executable with a command
// line argument of --type=crashpad-handler.
void InitializeCrashpadWithEmbeddedHandler(bool initial_client,
                                           const std::string& process_type);
#endif  // OS_WIN

// Enables or disables crash report upload. This is a property of the Crashpad
// database. In a newly-created database, uploads will be disabled. This
// function only has an effect when called in the browser process. Its effect is
// immediate and applies to all other process types, including processes that
// are already running.
void SetUploadsEnabled(bool enabled);

// Determines whether uploads are enabled or disabled. This information is only
// available in the browser process.
bool GetUploadsEnabled();

enum class ReportUploadState {
  NotUploaded,
  Pending,
  Uploaded,
};

struct Report {
  std::string local_id;
  time_t capture_time;
  std::string remote_id;
  time_t upload_time;
  ReportUploadState state;
};

// Obtains a list of reports uploaded to the collection server. This function
// only operates when called in the browser process. All reports in the Crashpad
// database that have been successfully uploaded will be included in this list.
// The list will be sorted in descending order by report creation time (newest
// reports first).
void GetReports(std::vector<Report>* reports);

#if BUILDFLAG(ENABLE_KASKO)
// Returns a copy of the current crash keys for Kasko.
void GetCrashKeysForKasko(std::vector<kasko::api::CrashKey>* crash_keys);

// Reads the annotations for the executable module for |process| and puts them
// into |crash_keys|.
void ReadMainModuleAnnotationsForKasko(
    const base::Process& process,
    std::vector<kasko::api::CrashKey>* crash_keys);
#endif  // BUILDFLAG(ENABLE_KASKO)

namespace internal {

#if defined(OS_WIN)
// Returns platform specific annotations. This is broken out on Windows only so
// that it may be reused by GetCrashKeysForKasko.
void GetPlatformCrashpadAnnotations(
    std::map<std::string, std::string>* annotations);
#endif  // defined(OS_WIN)

// The platform-specific portion of InitializeCrashpad().
// Returns the database path, if initializing in the browser process.
base::FilePath PlatformCrashpadInitialization(bool initial_client,
                                              bool browser_process,
                                              bool embedded_handler);

}  // namespace internal

}  // namespace crash_reporter

#endif  // COMPONENTS_CRASH_CONTENT_APP_CRASHPAD_H_
