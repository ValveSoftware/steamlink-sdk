// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stdafx.h"
#include "win8/metro_driver/metro_driver.h"

#include <roerrorapi.h>
#include <shobjidl.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/logging_win.h"
#include "base/win/scoped_comptr.h"
#include "base/win/windows_version.h"
#include "win8/metro_driver/winrt_utils.h"

namespace {

LONG WINAPI ErrorReportingHandler(EXCEPTION_POINTERS* ex_info) {
  // See roerrorapi.h for a description of the
  // exception codes and parameters.
  DWORD code = ex_info->ExceptionRecord->ExceptionCode;
  ULONG_PTR* info = ex_info->ExceptionRecord->ExceptionInformation;
  if (code == EXCEPTION_RO_ORIGINATEERROR) {
    base::string16 msg(reinterpret_cast<wchar_t*>(info[2]), info[1]);
    LOG(ERROR) << "VEH: Metro error 0x" << std::hex << info[0] << ": " << msg;
  } else if (code == EXCEPTION_RO_TRANSFORMERROR) {
    base::string16 msg(reinterpret_cast<wchar_t*>(info[3]), info[2]);
    LOG(ERROR) << "VEH: Metro old error 0x" << std::hex << info[0]
               << " new error 0x" << info[1] << ": " << msg;
  }

  return EXCEPTION_CONTINUE_SEARCH;
}

void SetMetroReportingFlags() {
#if !defined(NDEBUG)
  // Set the error reporting flags to always raise an exception,
  // which is then processed by our vectored exception handling
  // above to log the error message.
  winfoundtn::Diagnostics::SetErrorReportingFlags(
      winfoundtn::Diagnostics::UseSetErrorInfo |
      winfoundtn::Diagnostics::ForceExceptions);
#endif
}

// TODO(robertshield): This GUID is hard-coded in a bunch of places that
//     don't allow explicit includes. Find a single place for it to live.
// {7FE69228-633E-4f06-80C1-527FEA23E3A7}
const GUID kChromeTraceProviderName = {
    0x7fe69228, 0x633e, 0x4f06,
        { 0x80, 0xc1, 0x52, 0x7f, 0xea, 0x23, 0xe3, 0xa7 } };

}  // namespace

#if !defined(COMPONENT_BUILD)
// Required for base initialization.
// TODO(siggi): This should be handled better, as this way our at exit
//     registrations will run under the loader's lock. However,
//     once metro_driver is merged into Chrome.dll, this will go away anyhow.
base::AtExitManager at_exit;
#endif

mswr::ComPtr<winapp::Core::ICoreApplication> InitWindows8() {
  SetMetroReportingFlags();
  HRESULT hr = ::Windows::Foundation::Initialize(RO_INIT_MULTITHREADED);
  if (FAILED(hr))
    CHECK(false);
  mswr::ComPtr<winapp::Core::ICoreApplication> core_app;
  hr = winrt_utils::CreateActivationFactory(
      RuntimeClass_Windows_ApplicationModel_Core_CoreApplication,
      core_app.GetAddressOf());
  if (FAILED(hr))
    CHECK(false);
  return core_app;
}

mswr::ComPtr<winapp::Core::ICoreApplication> InitWindows7();

extern "C" __declspec(dllexport)
int InitMetro() {
  // Metro mode or its emulation is not supported in Vista or XP.
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return 1;
  // Initialize the command line.
  CommandLine::Init(0, NULL);
  // Initialize the logging system.
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  logging::InitLogging(settings);
#if defined(NDEBUG)
  logging::SetMinLogLevel(logging::LOG_ERROR);
#else
  logging::SetMinLogLevel(logging::LOG_VERBOSE);
    HANDLE registration =
      ::AddVectoredExceptionHandler(TRUE, ErrorReportingHandler);
#endif
  // Enable trace control and transport through event tracing for Windows.
  logging::LogEventProvider::Initialize(kChromeTraceProviderName);
  DVLOG(1) << "InitMetro";

  // OS specific initialization.
  mswr::ComPtr<winapp::Core::ICoreApplication> core_app;
  if (base::win::GetVersion() < base::win::VERSION_WIN8)
    core_app = InitWindows7();
  else
    core_app = InitWindows8();

  auto view_factory = mswr::Make<ChromeAppViewFactory>(core_app.Get());
  HRESULT hr = core_app->Run(view_factory.Get());
  DVLOG(1) << "exiting InitMetro, hr=" << hr;

#if !defined(NDEBUG)
  ::RemoveVectoredExceptionHandler(registration);
#endif
  return hr;
}

// Activates the application known by |app_id|.  Returns, among other things,
// E_APPLICATION_NOT_REGISTERED if |app_id| identifies Chrome and Chrome is not
// the default browser.
extern "C" __declspec(dllexport)
HRESULT ActivateApplication(const wchar_t* app_id) {
  base::win::ScopedComPtr<IApplicationActivationManager> activator;
  HRESULT hr = activator.CreateInstance(CLSID_ApplicationActivationManager);
  if (SUCCEEDED(hr)) {
    DWORD pid = 0;
    hr = activator->ActivateApplication(app_id, L"", AO_NONE, &pid);
  }
  return hr;
}
