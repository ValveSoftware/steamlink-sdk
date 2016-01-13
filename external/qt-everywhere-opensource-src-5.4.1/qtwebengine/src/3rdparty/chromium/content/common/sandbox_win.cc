// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/sandbox_win.h"

#include <string>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/debug/profiler.h"
#include "base/debug/trace_event.h"
#include "base/file_util.h"
#include "base/hash.h"
#include "base/metrics/field_trial.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/win/iat_patch_function.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_process_information.h"
#include "base/win/windows_version.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/sandbox_init.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "sandbox/win/src/process_mitigations.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_nt_util.h"
#include "sandbox/win/src/win_utils.h"
#include "ui/gfx/win/dpi.h"

static sandbox::BrokerServices* g_broker_services = NULL;
static sandbox::TargetServices* g_target_services = NULL;

namespace content {
namespace {

// The DLLs listed here are known (or under strong suspicion) of causing crashes
// when they are loaded in the renderer. Note: at runtime we generate short
// versions of the dll name only if the dll has an extension.
// For more information about how this list is generated, and how to get off
// of it, see:
// https://sites.google.com/a/chromium.org/dev/Home/third-party-developers
// If the size of this list exceeds 64, change kTroublesomeDllsMaxCount.
const wchar_t* const kTroublesomeDlls[] = {
  L"adialhk.dll",                 // Kaspersky Internet Security.
  L"acpiz.dll",                   // Unknown.
  L"akinsofthook32.dll",          // Akinsoft Software Engineering.
  L"assistant_x64.dll",           // Unknown.
  L"avcuf64.dll",                 // Bit Defender Internet Security x64.
  L"avgrsstx.dll",                // AVG 8.
  L"babylonchromepi.dll",         // Babylon translator.
  L"btkeyind.dll",                // Widcomm Bluetooth.
  L"cmcsyshk.dll",                // CMC Internet Security.
  L"cmsetac.dll",                 // Unknown (suspected malware).
  L"cooliris.dll",                // CoolIris.
  L"dockshellhook.dll",           // Stardock Objectdock.
  L"easyhook32.dll",              // GDIPP and others.
  L"googledesktopnetwork3.dll",   // Google Desktop Search v5.
  L"fwhook.dll",                  // PC Tools Firewall Plus.
  L"hookprocesscreation.dll",     // Blumentals Program protector.
  L"hookterminateapis.dll",       // Blumentals and Cyberprinter.
  L"hookprintapis.dll",           // Cyberprinter.
  L"imon.dll",                    // NOD32 Antivirus.
  L"ioloHL.dll",                  // Iolo (System Mechanic).
  L"kloehk.dll",                  // Kaspersky Internet Security.
  L"lawenforcer.dll",             // Spyware-Browser AntiSpyware (Spybro).
  L"libdivx.dll",                 // DivX.
  L"lvprcinj01.dll",              // Logitech QuickCam.
  L"madchook.dll",                // Madshi (generic hooking library).
  L"mdnsnsp.dll",                 // Bonjour.
  L"moonsysh.dll",                // Moon Secure Antivirus.
  L"mpk.dll",                     // KGB Spy.
  L"npdivx32.dll",                // DivX.
  L"npggNT.des",                  // GameGuard 2008.
  L"npggNT.dll",                  // GameGuard (older).
  L"oawatch.dll",                 // Online Armor.
  L"pavhook.dll",                 // Panda Internet Security.
  L"pavlsphook.dll",              // Panda Antivirus.
  L"pavshook.dll",                // Panda Antivirus.
  L"pavshookwow.dll",             // Panda Antivirus.
  L"pctavhook.dll",               // PC Tools Antivirus.
  L"pctgmhk.dll",                 // PC Tools Spyware Doctor.
  L"prntrack.dll",                // Pharos Systems.
  L"protector.dll",               // Unknown (suspected malware).
  L"radhslib.dll",                // Radiant Naomi Internet Filter.
  L"radprlib.dll",                // Radiant Naomi Internet Filter.
  L"rapportnikko.dll",            // Trustware Rapport.
  L"rlhook.dll",                  // Trustware Bufferzone.
  L"rooksdol.dll",                // Trustware Rapport.
  L"rndlpepperbrowserrecordhelper.dll", // RealPlayer.
  L"rpchromebrowserrecordhelper.dll",   // RealPlayer.
  L"r3hook.dll",                  // Kaspersky Internet Security.
  L"sahook.dll",                  // McAfee Site Advisor.
  L"sbrige.dll",                  // Unknown.
  L"sc2hook.dll",                 // Supercopier 2.
  L"sdhook32.dll",                // Spybot - Search & Destroy Live Protection.
  L"sguard.dll",                  // Iolo (System Guard).
  L"smum32.dll",                  // Spyware Doctor version 6.
  L"smumhook.dll",                // Spyware Doctor version 5.
  L"ssldivx.dll",                 // DivX.
  L"syncor11.dll",                // SynthCore Midi interface.
  L"systools.dll",                // Panda Antivirus.
  L"tfwah.dll",                   // Threatfire (PC tools).
  L"wblind.dll",                  // Stardock Object desktop.
  L"wbhelp.dll",                  // Stardock Object desktop.
  L"winstylerthemehelper.dll"     // Tuneup utilities 2006.
};

// Adds the policy rules for the path and path\ with the semantic |access|.
// If |children| is set to true, we need to add the wildcard rules to also
// apply the rule to the subfiles and subfolders.
bool AddDirectory(int path, const wchar_t* sub_dir, bool children,
                  sandbox::TargetPolicy::Semantics access,
                  sandbox::TargetPolicy* policy) {
  base::FilePath directory;
  if (!PathService::Get(path, &directory))
    return false;

  if (sub_dir)
    directory = base::MakeAbsoluteFilePath(directory.Append(sub_dir));

  sandbox::ResultCode result;
  result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES, access,
                           directory.value().c_str());
  if (result != sandbox::SBOX_ALL_OK)
    return false;

  std::wstring directory_str = directory.value() + L"\\";
  if (children)
    directory_str += L"*";
  // Otherwise, add the version of the path that ends with a separator.

  result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES, access,
                           directory_str.c_str());
  if (result != sandbox::SBOX_ALL_OK)
    return false;

  return true;
}

// Adds the policy rules for the path and path\* with the semantic |access|.
// We need to add the wildcard rules to also apply the rule to the subkeys.
bool AddKeyAndSubkeys(std::wstring key,
                      sandbox::TargetPolicy::Semantics access,
                      sandbox::TargetPolicy* policy) {
  sandbox::ResultCode result;
  result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_REGISTRY, access,
                           key.c_str());
  if (result != sandbox::SBOX_ALL_OK)
    return false;

  key += L"\\*";
  result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_REGISTRY, access,
                           key.c_str());
  if (result != sandbox::SBOX_ALL_OK)
    return false;

  return true;
}

// Compares the loaded |module| file name matches |module_name|.
bool IsExpandedModuleName(HMODULE module, const wchar_t* module_name) {
  wchar_t path[MAX_PATH];
  DWORD sz = ::GetModuleFileNameW(module, path, arraysize(path));
  if ((sz == arraysize(path)) || (sz == 0)) {
    // XP does not set the last error properly, so we bail out anyway.
    return false;
  }
  if (!::GetLongPathName(path, path, arraysize(path)))
    return false;
  base::FilePath fname(path);
  return (fname.BaseName().value() == module_name);
}

// Adds a single dll by |module_name| into the |policy| blacklist.
// If |check_in_browser| is true we only add an unload policy only if the dll
// is also loaded in this process.
void BlacklistAddOneDll(const wchar_t* module_name,
                        bool check_in_browser,
                        sandbox::TargetPolicy* policy) {
  HMODULE module = check_in_browser ? ::GetModuleHandleW(module_name) : NULL;
  if (!module) {
    // The module could have been loaded with a 8.3 short name. We check
    // the three most common cases: 'thelongname.dll' becomes
    // 'thelon~1.dll', 'thelon~2.dll' and 'thelon~3.dll'.
    std::wstring name(module_name);
    size_t period = name.rfind(L'.');
    DCHECK_NE(std::string::npos, period);
    DCHECK_LE(3U, (name.size() - period));
    if (period <= 8)
      return;
    for (int ix = 0; ix < 3; ++ix) {
      const wchar_t suffix[] = {'~', ('1' + ix), 0};
      std::wstring alt_name = name.substr(0, 6) + suffix;
      alt_name += name.substr(period, name.size());
      if (check_in_browser) {
        module = ::GetModuleHandleW(alt_name.c_str());
        if (!module)
          return;
        // We found it, but because it only has 6 significant letters, we
        // want to make sure it is the right one.
        if (!IsExpandedModuleName(module, module_name))
          return;
      }
      // Found a match. We add both forms to the policy.
      policy->AddDllToUnload(alt_name.c_str());
    }
  }
  policy->AddDllToUnload(module_name);
  DVLOG(1) << "dll to unload found: " << module_name;
  return;
}

// Adds policy rules for unloaded the known dlls that cause chrome to crash.
// Eviction of injected DLLs is done by the sandbox so that the injected module
// does not get a chance to execute any code.
void AddGenericDllEvictionPolicy(sandbox::TargetPolicy* policy) {
  for (int ix = 0; ix != arraysize(kTroublesomeDlls); ++ix)
    BlacklistAddOneDll(kTroublesomeDlls[ix], true, policy);
}

// Returns the object path prepended with the current logon session.
base::string16 PrependWindowsSessionPath(const base::char16* object) {
  // Cache this because it can't change after process creation.
  static uintptr_t s_session_id = 0;
  if (s_session_id == 0) {
    HANDLE token;
    DWORD session_id_length;
    DWORD session_id = 0;

    CHECK(::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token));
    CHECK(::GetTokenInformation(token, TokenSessionId, &session_id,
        sizeof(session_id), &session_id_length));
    CloseHandle(token);
    if (session_id)
      s_session_id = session_id;
  }

  return base::StringPrintf(L"\\Sessions\\%d%ls", s_session_id, object);
}

// Checks if the sandbox should be let to run without a job object assigned.
bool ShouldSetJobLevel(const CommandLine& cmd_line) {
  if (!cmd_line.HasSwitch(switches::kAllowNoSandboxJob))
    return true;

  // Windows 8 allows nested jobs so we don't need to check if we are in other
  // job.
  if (base::win::GetVersion() >= base::win::VERSION_WIN8)
    return true;

  BOOL in_job = true;
  // Either there is no job yet associated so we must add our job,
  if (!::IsProcessInJob(::GetCurrentProcess(), NULL, &in_job))
    NOTREACHED() << "IsProcessInJob failed. " << GetLastError();
  if (!in_job)
    return true;

  // ...or there is a job but the JOB_OBJECT_LIMIT_BREAKAWAY_OK limit is set.
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION job_info = {0};
  if (!::QueryInformationJobObject(NULL,
                                   JobObjectExtendedLimitInformation, &job_info,
                                   sizeof(job_info), NULL)) {
    NOTREACHED() << "QueryInformationJobObject failed. " << GetLastError();
    return true;
  }
  if (job_info.BasicLimitInformation.LimitFlags & JOB_OBJECT_LIMIT_BREAKAWAY_OK)
    return true;

  return false;
}

// Adds the generic policy rules to a sandbox TargetPolicy.
bool AddGenericPolicy(sandbox::TargetPolicy* policy) {
  sandbox::ResultCode result;

  // Renderers need to copy sections for plugin DIBs and GPU.
  // GPU needs to copy sections to renderers.
  result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_HANDLES,
                           sandbox::TargetPolicy::HANDLES_DUP_ANY,
                           L"Section");
  if (result != sandbox::SBOX_ALL_OK)
    return false;

  // Add the policy for the client side of a pipe. It is just a file
  // in the \pipe\ namespace. We restrict it to pipes that start with
  // "chrome." so the sandboxed process cannot connect to system services.
  result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                           sandbox::TargetPolicy::FILES_ALLOW_ANY,
                           L"\\??\\pipe\\chrome.*");
  if (result != sandbox::SBOX_ALL_OK)
    return false;

  // Add the policy for the server side of nacl pipe. It is just a file
  // in the \pipe\ namespace. We restrict it to pipes that start with
  // "chrome.nacl" so the sandboxed process cannot connect to
  // system services.
  result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_NAMED_PIPES,
                           sandbox::TargetPolicy::NAMEDPIPES_ALLOW_ANY,
                           L"\\\\.\\pipe\\chrome.nacl.*");
  if (result != sandbox::SBOX_ALL_OK)
    return false;

  // Allow the server side of sync sockets, which are pipes that have
  // the "chrome.sync" namespace and a randomly generated suffix.
  result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_NAMED_PIPES,
                           sandbox::TargetPolicy::NAMEDPIPES_ALLOW_ANY,
                           L"\\\\.\\pipe\\chrome.sync.*");
  if (result != sandbox::SBOX_ALL_OK)
    return false;

  // Add the policy for debug message only in debug
#ifndef NDEBUG
  base::FilePath app_dir;
  if (!PathService::Get(base::DIR_MODULE, &app_dir))
    return false;

  wchar_t long_path_buf[MAX_PATH];
  DWORD long_path_return_value = GetLongPathName(app_dir.value().c_str(),
                                                 long_path_buf,
                                                 MAX_PATH);
  if (long_path_return_value == 0 || long_path_return_value >= MAX_PATH)
    return false;

  base::FilePath debug_message(long_path_buf);
  debug_message = debug_message.AppendASCII("debug_message.exe");
  result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_PROCESS,
                           sandbox::TargetPolicy::PROCESS_MIN_EXEC,
                           debug_message.value().c_str());
  if (result != sandbox::SBOX_ALL_OK)
    return false;
#endif  // NDEBUG

  AddGenericDllEvictionPolicy(policy);
  return true;
}

bool AddPolicyForSandboxedProcess(sandbox::TargetPolicy* policy) {
  sandbox::ResultCode result;
  // Renderers need to share events with plugins.
  result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_HANDLES,
                           sandbox::TargetPolicy::HANDLES_DUP_ANY,
                           L"Event");
  if (result != sandbox::SBOX_ALL_OK)
    return false;

  // Win8+ adds a device DeviceApi that we don't need.
  if (base::win::GetVersion() > base::win::VERSION_WIN7)
    policy->AddKernelObjectToClose(L"File", L"\\Device\\DeviceApi");

  sandbox::TokenLevel initial_token = sandbox::USER_UNPROTECTED;
  if (base::win::GetVersion() > base::win::VERSION_XP) {
    // On 2003/Vista the initial token has to be restricted if the main
    // token is restricted.
    initial_token = sandbox::USER_RESTRICTED_SAME_ACCESS;
  }

  policy->SetTokenLevel(initial_token, sandbox::USER_LOCKDOWN);
  // Prevents the renderers from manipulating low-integrity processes.
  policy->SetDelayedIntegrityLevel(sandbox::INTEGRITY_LEVEL_UNTRUSTED);

  if (sandbox::SBOX_ALL_OK !=  policy->SetAlternateDesktop(true)) {
    DLOG(WARNING) << "Failed to apply desktop security to the renderer";
  }

  return true;
}

// Updates the command line arguments with debug-related flags. If debug flags
// have been used with this process, they will be filtered and added to
// command_line as needed.
void ProcessDebugFlags(CommandLine* command_line) {
  const CommandLine& current_cmd_line = *CommandLine::ForCurrentProcess();
  std::string type = command_line->GetSwitchValueASCII(switches::kProcessType);
  if (current_cmd_line.HasSwitch(switches::kWaitForDebuggerChildren)) {
    // Look to pass-on the kWaitForDebugger flag.
    std::string value = current_cmd_line.GetSwitchValueASCII(
        switches::kWaitForDebuggerChildren);
    if (value.empty() || value == type) {
      command_line->AppendSwitch(switches::kWaitForDebugger);
    }
    command_line->AppendSwitchASCII(switches::kWaitForDebuggerChildren, value);
  }
}

// This code is test only, and attempts to catch unsafe uses of
// DuplicateHandle() that copy privileged handles into sandboxed processes.
#ifndef OFFICIAL_BUILD
base::win::IATPatchFunction g_iat_patch_duplicate_handle;

typedef BOOL (WINAPI *DuplicateHandleFunctionPtr)(HANDLE source_process_handle,
                                                  HANDLE source_handle,
                                                  HANDLE target_process_handle,
                                                  LPHANDLE target_handle,
                                                  DWORD desired_access,
                                                  BOOL inherit_handle,
                                                  DWORD options);

DuplicateHandleFunctionPtr g_iat_orig_duplicate_handle;

NtQueryObject g_QueryObject = NULL;

static const char* kDuplicateHandleWarning =
    "You are attempting to duplicate a privileged handle into a sandboxed"
    " process.\n Please use the sandbox::BrokerDuplicateHandle API or"
    " contact security@chromium.org for assistance.";

void CheckDuplicateHandle(HANDLE handle) {
  // Get the object type (32 characters is safe; current max is 14).
  BYTE buffer[sizeof(OBJECT_TYPE_INFORMATION) + 32 * sizeof(wchar_t)];
  OBJECT_TYPE_INFORMATION* type_info =
      reinterpret_cast<OBJECT_TYPE_INFORMATION*>(buffer);
  ULONG size = sizeof(buffer) - sizeof(wchar_t);
  NTSTATUS error;
  error = g_QueryObject(handle, ObjectTypeInformation, type_info, size, &size);
  CHECK(NT_SUCCESS(error));
  type_info->Name.Buffer[type_info->Name.Length / sizeof(wchar_t)] = L'\0';

  // Get the object basic information.
  OBJECT_BASIC_INFORMATION basic_info;
  size = sizeof(basic_info);
  error = g_QueryObject(handle, ObjectBasicInformation, &basic_info, size,
                        &size);
  CHECK(NT_SUCCESS(error));

  CHECK(!(basic_info.GrantedAccess & WRITE_DAC)) <<
      kDuplicateHandleWarning;

  if (0 == _wcsicmp(type_info->Name.Buffer, L"Process")) {
    const ACCESS_MASK kDangerousMask = ~(PROCESS_QUERY_LIMITED_INFORMATION |
                                         SYNCHRONIZE);
    CHECK(!(basic_info.GrantedAccess & kDangerousMask)) <<
        kDuplicateHandleWarning;
  }
}

BOOL WINAPI DuplicateHandlePatch(HANDLE source_process_handle,
                                 HANDLE source_handle,
                                 HANDLE target_process_handle,
                                 LPHANDLE target_handle,
                                 DWORD desired_access,
                                 BOOL inherit_handle,
                                 DWORD options) {
  // Duplicate the handle so we get the final access mask.
  if (!g_iat_orig_duplicate_handle(source_process_handle, source_handle,
                                   target_process_handle, target_handle,
                                   desired_access, inherit_handle, options))
    return FALSE;

  // We're not worried about broker handles or not crossing process boundaries.
  if (source_process_handle == target_process_handle ||
      target_process_handle == ::GetCurrentProcess())
    return TRUE;

  // Only sandboxed children are placed in jobs, so just check them.
  BOOL is_in_job = FALSE;
  if (!::IsProcessInJob(target_process_handle, NULL, &is_in_job)) {
    // We need a handle with permission to check the job object.
    if (ERROR_ACCESS_DENIED == ::GetLastError()) {
      HANDLE temp_handle;
      CHECK(g_iat_orig_duplicate_handle(::GetCurrentProcess(),
                                        target_process_handle,
                                        ::GetCurrentProcess(),
                                        &temp_handle,
                                        PROCESS_QUERY_INFORMATION,
                                        FALSE, 0));
      base::win::ScopedHandle process(temp_handle);
      CHECK(::IsProcessInJob(process, NULL, &is_in_job));
    }
  }

  if (is_in_job) {
    // We never allow inheritable child handles.
    CHECK(!inherit_handle) << kDuplicateHandleWarning;

    // Duplicate the handle again, to get the final permissions.
    HANDLE temp_handle;
    CHECK(g_iat_orig_duplicate_handle(target_process_handle, *target_handle,
                                      ::GetCurrentProcess(), &temp_handle,
                                      0, FALSE, DUPLICATE_SAME_ACCESS));
    base::win::ScopedHandle handle(temp_handle);

    // Callers use CHECK macro to make sure we get the right stack.
    CheckDuplicateHandle(handle);
  }

  return TRUE;
}
#endif

}  // namespace

void SetJobLevel(const CommandLine& cmd_line,
                 sandbox::JobLevel job_level,
                 uint32 ui_exceptions,
                 sandbox::TargetPolicy* policy) {
  if (ShouldSetJobLevel(cmd_line)) {
#ifdef _WIN64
    policy->SetJobMemoryLimit(4ULL * 1024 * 1024 * 1024);
#endif
    policy->SetJobLevel(job_level, ui_exceptions);
  } else {
    policy->SetJobLevel(sandbox::JOB_NONE, 0);
  }
}

// TODO(jschuh): Need get these restrictions applied to NaCl and Pepper.
// Just have to figure out what needs to be warmed up first.
void AddBaseHandleClosePolicy(sandbox::TargetPolicy* policy) {
  // TODO(cpu): Add back the BaseNamedObjects policy.
  base::string16 object_path = PrependWindowsSessionPath(
      L"\\BaseNamedObjects\\windows_shell_global_counters");
  policy->AddKernelObjectToClose(L"Section", object_path.data());
}

bool InitBrokerServices(sandbox::BrokerServices* broker_services) {
  // TODO(abarth): DCHECK(CalledOnValidThread());
  //               See <http://b/1287166>.
  DCHECK(broker_services);
  DCHECK(!g_broker_services);
  sandbox::ResultCode result = broker_services->Init();
  g_broker_services = broker_services;

  // In non-official builds warn about dangerous uses of DuplicateHandle.
#ifndef OFFICIAL_BUILD
  BOOL is_in_job = FALSE;
  CHECK(::IsProcessInJob(::GetCurrentProcess(), NULL, &is_in_job));
  // In a Syzygy-profiled binary, instrumented for import profiling, this
  // patch will end in infinite recursion on the attempted delegation to the
  // original function.
  if (!base::debug::IsBinaryInstrumented() &&
      !is_in_job && !g_iat_patch_duplicate_handle.is_patched()) {
    HMODULE module = NULL;
    wchar_t module_name[MAX_PATH];
    CHECK(::GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                              reinterpret_cast<LPCWSTR>(InitBrokerServices),
                              &module));
    DWORD result = ::GetModuleFileNameW(module, module_name, MAX_PATH);
    if (result && (result != MAX_PATH)) {
      ResolveNTFunctionPtr("NtQueryObject", &g_QueryObject);
      result = g_iat_patch_duplicate_handle.Patch(
          module_name, "kernel32.dll", "DuplicateHandle",
          DuplicateHandlePatch);
      CHECK(result == 0);
      g_iat_orig_duplicate_handle =
          reinterpret_cast<DuplicateHandleFunctionPtr>(
              g_iat_patch_duplicate_handle.original_function());
    }
  }
#endif

  return sandbox::SBOX_ALL_OK == result;
}

bool InitTargetServices(sandbox::TargetServices* target_services) {
  DCHECK(target_services);
  DCHECK(!g_target_services);
  sandbox::ResultCode result = target_services->Init();
  g_target_services = target_services;
  return sandbox::SBOX_ALL_OK == result;
}

bool ShouldUseDirectWrite() {
  // If the flag is currently on, and we're on Win7 or above, we enable
  // DirectWrite. Skia does not require the additions to DirectWrite in QFE
  // 2670838, but a simple 'better than XP' check is not enough.
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return false;

  base::win::OSInfo::VersionNumber os_version =
      base::win::OSInfo::GetInstance()->version_number();
  if ((os_version.major == 6) && (os_version.minor == 1)) {
    // We can't use DirectWrite for pre-release versions of Windows 7.
    if (os_version.build < 7600)
      return false;
  }

  // If forced off, don't use it.
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kDisableDirectWrite))
    return false;

#if !defined(NACL_WIN64)
  // Can't use GDI on HiDPI.
  if (gfx::GetDPIScale() > 1.0f)
    return true;
#endif

  // Otherwise, check the field trial.
  const std::string group_name =
      base::FieldTrialList::FindFullName("DirectWrite");
  return group_name != "Disabled";
}

base::ProcessHandle StartSandboxedProcess(
    SandboxedProcessLauncherDelegate* delegate,
    CommandLine* cmd_line) {
  const CommandLine& browser_command_line = *CommandLine::ForCurrentProcess();
  std::string type_str = cmd_line->GetSwitchValueASCII(switches::kProcessType);

  TRACE_EVENT_BEGIN_ETW("StartProcessWithAccess", 0, type_str);

  // Propagate the --allow-no-job flag if present.
  if (browser_command_line.HasSwitch(switches::kAllowNoSandboxJob) &&
      !cmd_line->HasSwitch(switches::kAllowNoSandboxJob)) {
    cmd_line->AppendSwitch(switches::kAllowNoSandboxJob);
  }

  ProcessDebugFlags(cmd_line);

  // Prefetch hints on windows:
  // Using a different prefetch profile per process type will allow Windows
  // to create separate pretetch settings for browser, renderer etc.
  cmd_line->AppendArg(base::StringPrintf("/prefetch:%d", base::Hash(type_str)));

  if ((delegate && !delegate->ShouldSandbox()) ||
      browser_command_line.HasSwitch(switches::kNoSandbox) ||
      cmd_line->HasSwitch(switches::kNoSandbox)) {
    base::ProcessHandle process = 0;
    base::LaunchProcess(*cmd_line, base::LaunchOptions(), &process);
    g_broker_services->AddTargetPeer(process);
    return process;
  }

  sandbox::TargetPolicy* policy = g_broker_services->CreatePolicy();

  sandbox::MitigationFlags mitigations = sandbox::MITIGATION_HEAP_TERMINATE |
                                         sandbox::MITIGATION_BOTTOM_UP_ASLR |
                                         sandbox::MITIGATION_DEP |
                                         sandbox::MITIGATION_DEP_NO_ATL_THUNK |
                                         sandbox::MITIGATION_SEHOP;

 if (base::win::GetVersion() >= base::win::VERSION_WIN8 &&
     type_str == switches::kRendererProcess &&
     browser_command_line.HasSwitch(
        switches::kEnableWin32kRendererLockDown)) {
    if (policy->AddRule(sandbox::TargetPolicy::SUBSYS_WIN32K_LOCKDOWN,
                        sandbox::TargetPolicy::FAKE_USER_GDI_INIT,
                        NULL) != sandbox::SBOX_ALL_OK) {
      return 0;
    }
    mitigations |= sandbox::MITIGATION_WIN32K_DISABLE;
  }

  if (policy->SetProcessMitigations(mitigations) != sandbox::SBOX_ALL_OK)
    return 0;

  mitigations = sandbox::MITIGATION_STRICT_HANDLE_CHECKS |
                sandbox::MITIGATION_DLL_SEARCH_ORDER;

  if (policy->SetDelayedProcessMitigations(mitigations) != sandbox::SBOX_ALL_OK)
    return 0;

  SetJobLevel(*cmd_line, sandbox::JOB_LOCKDOWN, 0, policy);

  bool disable_default_policy = false;
  base::FilePath exposed_dir;
  if (delegate)
    delegate->PreSandbox(&disable_default_policy, &exposed_dir);

  if (!disable_default_policy && !AddPolicyForSandboxedProcess(policy))
    return 0;

  if (type_str == switches::kRendererProcess) {
    if (ShouldUseDirectWrite()) {
      AddDirectory(base::DIR_WINDOWS_FONTS,
                  NULL,
                  true,
                  sandbox::TargetPolicy::FILES_ALLOW_READONLY,
                  policy);
    }
  } else {
    // Hack for Google Desktop crash. Trick GD into not injecting its DLL into
    // this subprocess. See
    // http://code.google.com/p/chromium/issues/detail?id=25580
    cmd_line->AppendSwitchASCII("ignored", " --type=renderer ");
  }

  sandbox::ResultCode result;
  if (!exposed_dir.empty()) {
    result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                             sandbox::TargetPolicy::FILES_ALLOW_ANY,
                             exposed_dir.value().c_str());
    if (result != sandbox::SBOX_ALL_OK)
      return 0;

    base::FilePath exposed_files = exposed_dir.AppendASCII("*");
    result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                             sandbox::TargetPolicy::FILES_ALLOW_ANY,
                             exposed_files.value().c_str());
    if (result != sandbox::SBOX_ALL_OK)
      return 0;
  }

  if (!AddGenericPolicy(policy)) {
    NOTREACHED();
    return 0;
  }

  if (browser_command_line.HasSwitch(switches::kEnableLogging)) {
    // If stdout/stderr point to a Windows console, these calls will
    // have no effect.
    policy->SetStdoutHandle(GetStdHandle(STD_OUTPUT_HANDLE));
    policy->SetStderrHandle(GetStdHandle(STD_ERROR_HANDLE));
  }

  if (delegate) {
    bool success = true;
    delegate->PreSpawnTarget(policy, &success);
    if (!success)
      return 0;
  }

  TRACE_EVENT_BEGIN_ETW("StartProcessWithAccess::LAUNCHPROCESS", 0, 0);

  PROCESS_INFORMATION temp_process_info = {};
  result = g_broker_services->SpawnTarget(
               cmd_line->GetProgram().value().c_str(),
               cmd_line->GetCommandLineString().c_str(),
               policy, &temp_process_info);
  policy->Release();
  base::win::ScopedProcessInformation target(temp_process_info);

  TRACE_EVENT_END_ETW("StartProcessWithAccess::LAUNCHPROCESS", 0, 0);

  if (sandbox::SBOX_ALL_OK != result) {
    if (result == sandbox::SBOX_ERROR_GENERIC)
      DPLOG(ERROR) << "Failed to launch process";
    else
      DLOG(ERROR) << "Failed to launch process. Error: " << result;
    return 0;
  }

  if (delegate)
    delegate->PostSpawnTarget(target.process_handle());

  ResumeThread(target.thread_handle());
  return target.TakeProcessHandle();
}

bool BrokerDuplicateHandle(HANDLE source_handle,
                           DWORD target_process_id,
                           HANDLE* target_handle,
                           DWORD desired_access,
                           DWORD options) {
  // If our process is the target just duplicate the handle.
  if (::GetCurrentProcessId() == target_process_id) {
    return !!::DuplicateHandle(::GetCurrentProcess(), source_handle,
                               ::GetCurrentProcess(), target_handle,
                               desired_access, FALSE, options);

  }

  // Try the broker next
  if (g_target_services &&
      g_target_services->DuplicateHandle(source_handle, target_process_id,
                                         target_handle, desired_access,
                                         options) == sandbox::SBOX_ALL_OK) {
    return true;
  }

  // Finally, see if we already have access to the process.
  base::win::ScopedHandle target_process;
  target_process.Set(::OpenProcess(PROCESS_DUP_HANDLE, FALSE,
                                    target_process_id));
  if (target_process.IsValid()) {
    return !!::DuplicateHandle(::GetCurrentProcess(), source_handle,
                                target_process, target_handle,
                                desired_access, FALSE, options);
  }

  return false;
}

bool BrokerAddTargetPeer(HANDLE peer_process) {
  return g_broker_services->AddTargetPeer(peer_process) == sandbox::SBOX_ALL_OK;
}

}  // namespace content
