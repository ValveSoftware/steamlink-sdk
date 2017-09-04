// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/scoped_handle.h"
#include "base/win/scoped_process_information.h"
#include "base/win/windows_version.h"
#include "sandbox/win/src/restricted_token_utils.h"

// launcher.exe is an application used to launch another application with a
// restricted token. This is to be used for testing only.
// The parameters are the level of security of the primary token, the
// impersonation token and the job object along with the command line to
// execute.
// See the usage (launcher.exe without parameters) for the correct format.

namespace {

// Starts the process described by the input parameter command_line in a job
// with a restricted token. Also set the main thread of this newly created
// process to impersonate a user with more rights so it can initialize
// correctly.
//
// Parameters: primary_level is the security level of the primary token.
// impersonation_level is the security level of the impersonation token used
// to initialize the process. job_level is the security level of the job
// object used to encapsulate the process.
//
// The output parameter job_handle is the handle to the job object. Closing this
// handle will kill the process started.
//
// Note: The process started with this function has to call RevertToSelf() as
// soon as possible to stop using the impersonation token and start being
// secure.
//
// Note: The Unicode version of this function will fail if the command_line
// parameter is a const string.
DWORD StartRestrictedProcessInJob(wchar_t* command_line,
                                  TokenLevel primary_level,
                                  TokenLevel impersonation_level,
                                  JobLevel job_level,
                                  base::win::ScopedHandle* job_handle) {
  Job job;
  DWORD err_code = job.Init(job_level, NULL, 0, 0);
  if (ERROR_SUCCESS != err_code)
    return err_code;

  if (JOB_UNPROTECTED != job_level) {
    // Share the Desktop handle to be able to use MessageBox() in the sandboxed
    // application.
    err_code = job.UserHandleGrantAccess(GetDesktopWindow());
    if (ERROR_SUCCESS != err_code)
      return err_code;
  }

  // Create the primary (restricted) token for the process
  base::win::ScopedHandle primary_token;
  err_code = sandbox::CreateRestrictedToken(primary_level, INTEGRITY_LEVEL_LAST,
                                            PRIMARY, &primary_token);
  if (ERROR_SUCCESS != err_code)
    return err_code;


  // Create the impersonation token (restricted) to be able to start the
  // process.
  base::win::ScopedHandle impersonation_token;
  err_code = sandbox::CreateRestrictedToken(impersonation_level,
                                            INTEGRITY_LEVEL_LAST,
                                            IMPERSONATION,
                                            &impersonation_token);
  if (ERROR_SUCCESS != err_code)
    return err_code;

  // Start the process
  STARTUPINFO startup_info = {0};
  PROCESS_INFORMATION temp_process_info = {};
  DWORD flags = CREATE_SUSPENDED;

  if (base::win::GetVersion() < base::win::VERSION_WIN8) {
    // Windows 8 implements nested jobs, but for older systems we need to
    // break out of any job we're in to enforce our restrictions.
    flags |= CREATE_BREAKAWAY_FROM_JOB;
  }

  if (!::CreateProcessAsUser(primary_token.Get(),
                             NULL,   // No application name.
                             command_line,
                             NULL,   // No security attribute.
                             NULL,   // No thread attribute.
                             FALSE,  // Do not inherit handles.
                             flags,
                             NULL,   // Use the environment of the caller.
                             NULL,   // Use current directory of the caller.
                             &startup_info,
                             &temp_process_info)) {
    return ::GetLastError();
  }
  base::win::ScopedProcessInformation process_info(temp_process_info);

  // Change the token of the main thread of the new process for the
  // impersonation token with more rights.
  {
    HANDLE temp_thread = process_info.thread_handle();
    if (!::SetThreadToken(&temp_thread, impersonation_token.Get())) {
      auto last_error = ::GetLastError();
      ::TerminateProcess(process_info.process_handle(),
                         0);  // exit code
      return last_error;
    }
  }

  err_code = job.AssignProcessToJob(process_info.process_handle());
  if (ERROR_SUCCESS != err_code) {
    auto last_error = ::GetLastError();
    ::TerminateProcess(process_info.process_handle(),
                       0);  // exit code
    return last_error;
  }

  // Start the application
  ::ResumeThread(process_info.thread_handle());

  *job_handle = job.Take();

  return ERROR_SUCCESS;
}

}  // namespace

#define PARAM_IS(y) (argc > i) && (_wcsicmp(argv[i], y) == 0)

void PrintUsage(const wchar_t *application_name) {
  wprintf(L"\n\nUsage: \n  %ls --main level --init level --job level cmd_line ",
          application_name);
  wprintf(L"\n\n  Levels : \n\tLOCKDOWN \n\tRESTRICTED "
      L"\n\tLIMITED_USER \n\tINTERACTIVE_USER \n\tNON_ADMIN \n\tUNPROTECTED");
  wprintf(L"\n\n  main: Security level of the main token");
  wprintf(L"\n  init: Security level of the impersonation token");
  wprintf(L"\n  job: Security level of the job object");
}

bool GetTokenLevelFromString(const wchar_t *param,
                             sandbox::TokenLevel* level) {
  if (_wcsicmp(param, L"LOCKDOWN") == 0) {
    *level = sandbox::USER_LOCKDOWN;
  } else if (_wcsicmp(param, L"RESTRICTED") == 0) {
    *level = sandbox::USER_RESTRICTED;
  } else if (_wcsicmp(param, L"LIMITED_USER") == 0) {
    *level = sandbox::USER_LIMITED;
  } else if (_wcsicmp(param, L"INTERACTIVE_USER") == 0) {
    *level = sandbox::USER_INTERACTIVE;
  } else if (_wcsicmp(param, L"NON_ADMIN") == 0) {
    *level = sandbox::USER_NON_ADMIN;
  } else if (_wcsicmp(param, L"USER_RESTRICTED_SAME_ACCESS") == 0) {
    *level = sandbox::USER_RESTRICTED_SAME_ACCESS;
  } else if (_wcsicmp(param, L"UNPROTECTED") == 0) {
    *level = sandbox::USER_UNPROTECTED;
  } else {
    return false;
  }

  return true;
}

bool GetJobLevelFromString(const wchar_t *param, sandbox::JobLevel* level) {
  if (_wcsicmp(param, L"LOCKDOWN") == 0) {
    *level = sandbox::JOB_LOCKDOWN;
  } else if (_wcsicmp(param, L"RESTRICTED") == 0) {
    *level = sandbox::JOB_RESTRICTED;
  } else if (_wcsicmp(param, L"LIMITED_USER") == 0) {
    *level = sandbox::JOB_LIMITED_USER;
  } else if (_wcsicmp(param, L"INTERACTIVE_USER") == 0) {
    *level = sandbox::JOB_INTERACTIVE;
  } else if (_wcsicmp(param, L"NON_ADMIN") == 0) {
    wprintf(L"\nNON_ADMIN is not a supported job type");
    return false;
  } else if (_wcsicmp(param, L"UNPROTECTED") == 0) {
    *level = sandbox::JOB_UNPROTECTED;
  } else {
    return false;
  }

  return true;
}

int wmain(int argc, wchar_t *argv[]) {
  // Extract the filename from the path.
  wchar_t *app_name = wcsrchr(argv[0], L'\\');
  if (!app_name) {
    app_name = argv[0];
  } else {
    app_name++;
  }

  // no argument
  if (argc == 1) {
    PrintUsage(app_name);
    return -1;
  }

  sandbox::TokenLevel primary_level = sandbox::USER_LOCKDOWN;
  sandbox::TokenLevel impersonation_level =
      sandbox::USER_RESTRICTED_SAME_ACCESS;
  sandbox::JobLevel job_level = sandbox::JOB_LOCKDOWN;
  ATL::CString command_line;

  // parse command line.
  for (int i = 1; i < argc; ++i) {
    if (PARAM_IS(L"--main")) {
      i++;
      if (argc > i) {
        if (!GetTokenLevelFromString(argv[i], &primary_level)) {
          wprintf(L"\nAbord, Unrecognized main token level \"%ls\"", argv[i]);
          PrintUsage(app_name);
          return -1;
        }
      }
    } else if (PARAM_IS(L"--init")) {
      i++;
      if (argc > i) {
        if (!GetTokenLevelFromString(argv[i], &impersonation_level)) {
          wprintf(L"\nAbord, Unrecognized init token level \"%ls\"", argv[i]);
          PrintUsage(app_name);
          return -1;
        }
      }
    } else if (PARAM_IS(L"--job")) {
      i++;
      if (argc > i) {
        if (!GetJobLevelFromString(argv[i], &job_level)) {
          wprintf(L"\nAbord, Unrecognized job security level \"%ls\"", argv[i]);
          PrintUsage(app_name);
          return -1;
        }
      }
    } else {
      if (command_line.GetLength()) {
        command_line += L' ';
      }
      command_line += argv[i];
    }
  }

  if (!command_line.GetLength()) {
    wprintf(L"\nAbord, No command line specified");
    PrintUsage(app_name);
    return -1;
  }

  wprintf(L"\nLaunching command line: \"%ls\"\n", command_line.GetBuffer());

  base::win::ScopedHandle job_handle;
  DWORD err_code = StartRestrictedProcessInJob(
      command_line.GetBuffer(),
      primary_level,
      impersonation_level,
      job_level,
      &job_handle);
  if (ERROR_SUCCESS != err_code) {
    wprintf(L"\nAbord, Error %d while launching command line.", err_code);
    return -1;
  }

  wprintf(L"\nPress any key to continue.");
  while(!_kbhit()) {
    Sleep(100);
  }

  return 0;
}
