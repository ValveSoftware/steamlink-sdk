// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CONTENT_APP_CRASH_REPORTER_CLIENT_H_
#define COMPONENTS_CRASH_CONTENT_APP_CRASH_REPORTER_CLIENT_H_

#include <stddef.h>

#include <string>

#include "base/strings/string16.h"
#include "build/build_config.h"

#if !defined(OS_WIN)
namespace base {
class FilePath;
}
#endif

#if defined(OS_MACOSX)
// We don't want to directly include
// breakpad/src/client/mac/Framework/Breakpad.h here, so we repeat the
// definition of BreakpadRef.
//
// On Mac, when compiling without breakpad support, a stub implementation is
// compiled in. Not having any includes of the breakpad library allows for
// reusing this header for the stub.
typedef void* BreakpadRef;
#endif

namespace crash_reporter {

class CrashReporterClient;

// Setter and getter for the client.  The client should be set early, before any
// crash reporter code is called, and should stay alive throughout the entire
// runtime.
void SetCrashReporterClient(CrashReporterClient* client);

#if defined(CRASH_IMPLEMENTATION)
// The components's embedder API should only be used by the component.
CrashReporterClient* GetCrashReporterClient();
#endif

// Interface that the embedder implements.
class CrashReporterClient {
 public:
  CrashReporterClient();
  virtual ~CrashReporterClient();

#if !defined(OS_MACOSX) && !defined(OS_WIN)
  // Sets the crash reporting client ID, a unique identifier for the client
  // that is sending crash reports. After it is set, it should not be changed.
  // |client_guid| may either be a full GUID or a GUID that was already stripped
  // from its dashes.
  //
  // On Mac OS X and Windows, this is the responsibility of Crashpad, and can
  // not be set directly by the client.
  virtual void SetCrashReporterClientIdFromGUID(const std::string& client_guid);
#endif

#if defined(OS_WIN)
  // Returns true if the pipe name to connect to breakpad should be computed and
  // stored in the process's environment block. By default, returns true for the
  // "browser" process.
  virtual bool ShouldCreatePipeName(const base::string16& process_type);

  // Returns true if an alternative location to store the minidump files was
  // specified. Returns true if |crash_dir| was set.
  virtual bool GetAlternativeCrashDumpLocation(base::string16* crash_dir);

  // Returns a textual description of the product type and version to include
  // in the crash report.
  virtual void GetProductNameAndVersion(const base::string16& exe_path,
                                        base::string16* product_name,
                                        base::string16* version,
                                        base::string16* special_build,
                                        base::string16* channel_name);

  // Returns true if a restart dialog should be displayed. In that case,
  // |message| and |title| are set to a message to display in a dialog box with
  // the given title before restarting, and |is_rtl_locale| indicates whether
  // to display the text as RTL.
  virtual bool ShouldShowRestartDialog(base::string16* title,
                                       base::string16* message,
                                       bool* is_rtl_locale);

  // Returns true if it is ok to restart the application. Invoked right before
  // restarting after a crash.
  virtual bool AboutToRestart();

  // Returns true if the crash report uploader supports deferred uploads.
  virtual bool GetDeferredUploadsSupported(bool is_per_user_install);

  // Returns true if the running binary is a per-user installation.
  virtual bool GetIsPerUserInstall(const base::string16& exe_path);

  // Returns true if larger crash dumps should be dumped.
  virtual bool GetShouldDumpLargerDumps(bool is_per_user_install);

  // Returns the result code to return when breakpad failed to respawn a
  // crashed process.
  virtual int GetResultCodeRespawnFailed();
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_IOS)
  // Returns a textual description of the product type and version to include
  // in the crash report. Neither out parameter should be set to NULL.
  virtual void GetProductNameAndVersion(const char** product_name,
                                        const char** version);

  virtual base::FilePath GetReporterLogFilename();

  // Custom crash minidump handler after the minidump is generated.
  // Returns true if the minidump is handled (client); otherwise, return false
  // to fallback to default handler.
  // WARNING: this handler runs in a compromised context. It may not call into
  // libc nor allocate memory normally.
  virtual bool HandleCrashDump(const char* crashdump_filename);
#endif

  // The location where minidump files should be written. Returns true if
  // |crash_dir| was set.
#if defined(OS_WIN)
  virtual bool GetCrashDumpLocation(base::string16* crash_dir);
#else
  virtual bool GetCrashDumpLocation(base::FilePath* crash_dir);
#endif

  // Register all of the potential crash keys that can be sent to the crash
  // reporting server. Returns the size of the union of all keys.
  virtual size_t RegisterCrashKeys();

  // Returns true if running in unattended mode (for automated testing).
  virtual bool IsRunningUnattended();

  // Returns true if the user has given consent to collect stats.
  virtual bool GetCollectStatsConsent();

#if defined(OS_WIN) || defined(OS_MACOSX)
  // Returns true if crash reporting is enforced via management policies. In
  // that case, |breakpad_enabled| is set to the value enforced by policies.
  virtual bool ReportingIsEnforcedByPolicy(bool* breakpad_enabled);
#endif

#if defined(OS_ANDROID)
  // Returns the descriptor key of the android minidump global descriptor.
  virtual int GetAndroidMinidumpDescriptor();

  // Returns true if breakpad microdumps should be enabled. This orthogonal to
  // the standard minidump uploader (which depends on the user consent).
  virtual bool ShouldEnableBreakpadMicrodumps();
#endif

  // Returns true if breakpad should run in the given process type.
  virtual bool EnableBreakpadForProcess(const std::string& process_type);
};

}  // namespace crash_reporter

#endif  // COMPONENTS_CRASH_CONTENT_APP_CRASH_REPORTER_CLIENT_H_
