// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/app/content_main_runner.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <memory>
#include <string>
#include <utility>

#include "base/allocator/allocator_check.h"
#include "base/allocator/allocator_extension.h"
#include "base/at_exit.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/debug/stack_trace.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/i18n/icu_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/statistics_recorder.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/memory.h"
#include "base/process/process_handle.h"
#include "base/profiler/scoped_tracker.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/tracing/browser/trace_config_file.h"
#include "components/tracing/common/trace_to_console.h"
#include "components/tracing/common/tracing_switches.h"
#include "content/app/mojo/mojo_init.h"
#include "content/browser/browser_main.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/utility_process_host_impl.h"
#include "content/common/set_process_title.h"
#include "content/common/url_schemes.h"
#include "content/gpu/in_process_gpu_thread.h"
#include "content/public/app/content_main.h"
#include "content/public/app/content_main_delegate.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "content/public/common/sandbox_init.h"
#include "content/public/gpu/content_gpu_client.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/utility/content_utility_client.h"
#include "content/renderer/in_process_renderer_thread.h"
#include "content/utility/in_process_utility_thread.h"
#include "ipc/ipc_descriptors.h"
#include "ipc/ipc_switches.h"
#include "media/base/media.h"
#include "sandbox/win/src/sandbox_types.h"
#include "ui/base/ui_base_paths.h"
#include "ui/base/ui_base_switches.h"

#ifdef V8_USE_EXTERNAL_STARTUP_DATA
#include "gin/v8_initializer.h"
#endif

#if defined(OS_WIN)
#include <malloc.h>
#include <cstring>

#include "base/trace_event/trace_event_etw_export_win.h"
#include "base/win/process_startup_helper.h"
#if !defined(TOOLKIT_QT)
#include "ui/base/win/atl_module.h"
#endif
#include "ui/display/win/dpi.h"
#elif defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/power_monitor/power_monitor_device_source.h"
#include "content/app/mac/mac_init.h"
#include "content/browser/mach_broker_mac.h"
#include "content/common/sandbox_init_mac.h"
#endif  // OS_WIN

#if defined(OS_POSIX)
#include <signal.h>

#include "base/posix/global_descriptors.h"
#include "content/public/common/content_descriptors.h"

#if !defined(OS_MACOSX)
#include "content/public/common/zygote_fork_delegate_linux.h"
#endif
#if !defined(OS_MACOSX) && !defined(OS_ANDROID)
#include "content/zygote/zygote_main.h"
#endif

#endif  // OS_POSIX

#if defined(USE_NSS_CERTS)
#include "crypto/nss_util.h"
#endif

namespace content {
extern int GpuMain(const content::MainFunctionParams&);
#if defined(ENABLE_PLUGINS)
#if !defined(OS_LINUX)
extern int PluginMain(const content::MainFunctionParams&);
#endif
extern int PpapiPluginMain(const MainFunctionParams&);
extern int PpapiBrokerMain(const MainFunctionParams&);
#endif
extern int RendererMain(const content::MainFunctionParams&);
extern int UtilityMain(const MainFunctionParams&);
#if defined(OS_ANDROID)
extern int DownloadMain(const MainFunctionParams&);
#endif
}  // namespace content

namespace content {

namespace {

// This sets up two singletons responsible for managing field trials. The
// |field_trial_list| singleton lives on the stack and must outlive the Run()
// method of the process.
void InitializeFieldTrialAndFeatureList(
    std::unique_ptr<base::FieldTrialList>* field_trial_list) {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  // Initialize statistical testing infrastructure.  We set the entropy
  // provider to nullptr to disallow non-browser processes from creating
  // their own one-time randomized trials; they should be created in the
  // browser process.
  field_trial_list->reset(new base::FieldTrialList(nullptr));

  // Ensure any field trials in browser are reflected into the child
  // process.
  if (command_line.HasSwitch(switches::kForceFieldTrials)) {
    bool result = base::FieldTrialList::CreateTrialsFromString(
        command_line.GetSwitchValueASCII(switches::kForceFieldTrials),
        std::set<std::string>());
    DCHECK(result);
  }

  std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
  feature_list->InitializeFromCommandLine(
      command_line.GetSwitchValueASCII(switches::kEnableFeatures),
      command_line.GetSwitchValueASCII(switches::kDisableFeatures));
  base::FeatureList::SetInstance(std::move(feature_list));
}

}  // namespace

#if !defined(CHROME_MULTIPLE_DLL_CHILD)
base::LazyInstance<ContentBrowserClient>
    g_empty_content_browser_client = LAZY_INSTANCE_INITIALIZER;
#endif  //  !CHROME_MULTIPLE_DLL_CHILD

#if !defined(CHROME_MULTIPLE_DLL_BROWSER)
base::LazyInstance<ContentGpuClient>
    g_empty_content_gpu_client = LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<ContentRendererClient>
    g_empty_content_renderer_client = LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<ContentUtilityClient>
    g_empty_content_utility_client = LAZY_INSTANCE_INITIALIZER;
#endif  // !CHROME_MULTIPLE_DLL_BROWSER

#if defined(V8_USE_EXTERNAL_STARTUP_DATA) && defined(OS_ANDROID)
#if defined __LP64__
#define kV8SnapshotDataDescriptor kV8SnapshotDataDescriptor64
#else
#define kV8SnapshotDataDescriptor kV8SnapshotDataDescriptor32
#endif
#endif

#if defined(OS_POSIX)

// Setup signal-handling state: resanitize most signals, ignore SIGPIPE.
void SetupSignalHandlers() {
  // Sanitise our signal handling state. Signals that were ignored by our
  // parent will also be ignored by us. We also inherit our parent's sigmask.
  sigset_t empty_signal_set;
  CHECK_EQ(0, sigemptyset(&empty_signal_set));
  CHECK_EQ(0, sigprocmask(SIG_SETMASK, &empty_signal_set, NULL));

  struct sigaction sigact;
  memset(&sigact, 0, sizeof(sigact));
  sigact.sa_handler = SIG_DFL;
  static const int signals_to_reset[] =
      {SIGHUP, SIGINT, SIGQUIT, SIGILL, SIGABRT, SIGFPE, SIGSEGV,
       SIGALRM, SIGTERM, SIGCHLD, SIGBUS, SIGTRAP};  // SIGPIPE is set below.
  for (unsigned i = 0; i < arraysize(signals_to_reset); i++) {
    CHECK_EQ(0, sigaction(signals_to_reset[i], &sigact, NULL));
  }

  // Always ignore SIGPIPE.  We check the return value of write().
  CHECK_NE(SIG_ERR, signal(SIGPIPE, SIG_IGN));
}

#endif  // OS_POSIX

void CommonSubprocessInit(const std::string& process_type) {
#if defined(OS_WIN)
  // HACK: Let Windows know that we have started.  This is needed to suppress
  // the IDC_APPSTARTING cursor from being displayed for a prolonged period
  // while a subprocess is starting.
  PostThreadMessage(GetCurrentThreadId(), WM_NULL, 0, 0);
  MSG msg;
  PeekMessage(&msg, NULL, 0, 0, PM_REMOVE);
#endif
#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
  // Various things break when you're using a locale where the decimal
  // separator isn't a period.  See e.g. bugs 22782 and 39964.  For
  // all processes except the browser process (where we call system
  // APIs that may rely on the correct locale for formatting numbers
  // when presenting them to the user), reset the locale for numeric
  // formatting.
  // Note that this is not correct for plugin processes -- they can
  // surface UI -- but it's likely they get this wrong too so why not.
  setlocale(LC_NUMERIC, "C");
#endif

#if !defined(OFFICIAL_BUILD)
  // Print stack traces to stderr when crashes occur. This opens up security
  // holes so it should never be enabled for official builds.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableInProcessStackTraces)) {
    base::debug::EnableInProcessStackDumping();
  }
#if defined(OS_WIN)
  base::RouteStdioToConsole(false);
  LoadLibraryA("dbghelp.dll");
#endif
#endif
}

class ContentClientInitializer {
 public:
  static void Set(const std::string& process_type,
                  ContentMainDelegate* delegate) {
    ContentClient* content_client = GetContentClient();
#if !defined(CHROME_MULTIPLE_DLL_CHILD)
    if (process_type.empty()) {
      if (delegate)
        content_client->browser_ = delegate->CreateContentBrowserClient();
      if (!content_client->browser_)
        content_client->browser_ = &g_empty_content_browser_client.Get();
    }
#endif  // !CHROME_MULTIPLE_DLL_CHILD

#if !defined(CHROME_MULTIPLE_DLL_BROWSER)
    base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
    if (process_type == switches::kGpuProcess ||
        cmd->HasSwitch(switches::kSingleProcess) ||
        (process_type.empty() && cmd->HasSwitch(switches::kInProcessGPU))) {
      if (delegate)
        content_client->gpu_ = delegate->CreateContentGpuClient();
      if (!content_client->gpu_)
        content_client->gpu_ = &g_empty_content_gpu_client.Get();
    }

    if (process_type == switches::kRendererProcess ||
        cmd->HasSwitch(switches::kSingleProcess)) {
      if (delegate)
        content_client->renderer_ = delegate->CreateContentRendererClient();
      if (!content_client->renderer_)
        content_client->renderer_ = &g_empty_content_renderer_client.Get();
    }

    if (process_type == switches::kUtilityProcess ||
        cmd->HasSwitch(switches::kSingleProcess)) {
      if (delegate)
        content_client->utility_ = delegate->CreateContentUtilityClient();
      // TODO(scottmg): http://crbug.com/237249 Should be in _child.
      if (!content_client->utility_)
        content_client->utility_ = &g_empty_content_utility_client.Get();
    }
#endif  // !CHROME_MULTIPLE_DLL_BROWSER
  }
};

// We dispatch to a process-type-specific FooMain() based on a command-line
// flag.  This struct is used to build a table of (flag, main function) pairs.
struct MainFunction {
  const char* name;
  int (*function)(const MainFunctionParams&);
};

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
// On platforms that use the zygote, we have a special subset of
// subprocesses that are launched via the zygote.  This function
// fills in some process-launching bits around ZygoteMain().
// Returns the exit code of the subprocess.
int RunZygote(const MainFunctionParams& main_function_params,
              ContentMainDelegate* delegate) {
  static const MainFunction kMainFunctions[] = {
    { switches::kRendererProcess,    RendererMain },
#if defined(ENABLE_PLUGINS)
    { switches::kPpapiPluginProcess, PpapiPluginMain },
#endif
    { switches::kUtilityProcess,     UtilityMain },
  };

  ScopedVector<ZygoteForkDelegate> zygote_fork_delegates;
  if (delegate) {
    delegate->ZygoteStarting(&zygote_fork_delegates);
    media::InitializeMediaLibrary();
  }

  // This function call can return multiple times, once per fork().
  if (!ZygoteMain(main_function_params, std::move(zygote_fork_delegates)))
    return 1;

  if (delegate) delegate->ZygoteForked();

  // Zygote::HandleForkRequest may have reallocated the command
  // line so update it here with the new version.
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);
  ContentClientInitializer::Set(process_type, delegate);

  MainFunctionParams main_params(command_line);
  main_params.zygote_child = true;

  std::unique_ptr<base::FieldTrialList> field_trial_list;
  InitializeFieldTrialAndFeatureList(&field_trial_list);

  for (size_t i = 0; i < arraysize(kMainFunctions); ++i) {
    if (process_type == kMainFunctions[i].name)
      return kMainFunctions[i].function(main_params);
  }

  if (delegate)
    return delegate->RunProcess(process_type, main_params);

  NOTREACHED() << "Unknown zygote process type: " << process_type;
  return 1;
}
#endif  // defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)

static void RegisterMainThreadFactories() {
#if !defined(CHROME_MULTIPLE_DLL_BROWSER) && !defined(CHROME_MULTIPLE_DLL_CHILD)
  UtilityProcessHostImpl::RegisterUtilityMainThreadFactory(
      CreateInProcessUtilityThread);
  RenderProcessHostImpl::RegisterRendererMainThreadFactory(
      CreateInProcessRendererThread);
  GpuProcessHost::RegisterGpuMainThreadFactory(
      CreateInProcessGpuThread);
#else
  base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kSingleProcess)) {
    LOG(FATAL) <<
        "--single-process is not supported in chrome multiple dll browser.";
  }
  if (command_line.HasSwitch(switches::kInProcessGPU)) {
    LOG(FATAL) <<
        "--in-process-gpu is not supported in chrome multiple dll browser.";
  }
#endif  // !CHROME_MULTIPLE_DLL_BROWSER && !CHROME_MULTIPLE_DLL_CHILD
}

// Run the FooMain() for a given process type.
// If |process_type| is empty, runs BrowserMain().
// Returns the exit code for this process.
int RunNamedProcessTypeMain(
    const std::string& process_type,
    const MainFunctionParams& main_function_params,
    ContentMainDelegate* delegate) {
  static const MainFunction kMainFunctions[] = {
#if !defined(CHROME_MULTIPLE_DLL_CHILD)
    { "",                            BrowserMain },
#endif
#if !defined(CHROME_MULTIPLE_DLL_BROWSER)
#if defined(ENABLE_PLUGINS)
    { switches::kPpapiPluginProcess, PpapiPluginMain },
    { switches::kPpapiBrokerProcess, PpapiBrokerMain },
#endif  // ENABLE_PLUGINS
    { switches::kUtilityProcess,     UtilityMain },
    { switches::kRendererProcess,    RendererMain },
    { switches::kGpuProcess,         GpuMain },
#if defined(OS_ANDROID)
    { switches::kDownloadProcess,    DownloadMain},
#endif
#endif  // !CHROME_MULTIPLE_DLL_BROWSER
  };

  RegisterMainThreadFactories();

  for (size_t i = 0; i < arraysize(kMainFunctions); ++i) {
    if (process_type == kMainFunctions[i].name) {
      if (delegate) {
        int exit_code = delegate->RunProcess(process_type,
            main_function_params);
#if defined(OS_ANDROID)
        // In Android's browser process, the negative exit code doesn't mean the
        // default behavior should be used as the UI message loop is managed by
        // the Java and the browser process's default behavior is always
        // overridden.
        if (process_type.empty())
          return exit_code;
#endif
        if (exit_code >= 0)
          return exit_code;
      }
      return kMainFunctions[i].function(main_function_params);
    }
  }

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
  // Zygote startup is special -- see RunZygote comments above
  // for why we don't use ZygoteMain directly.
  if (process_type == switches::kZygoteProcess)
    return RunZygote(main_function_params, delegate);
#endif

  // If it's a process we don't know about, the embedder should know.
  if (delegate)
    return delegate->RunProcess(process_type, main_function_params);

  NOTREACHED() << "Unknown process type: " << process_type;
  return 1;
}

class ContentMainRunnerImpl : public ContentMainRunner {
 public:
  ContentMainRunnerImpl()
      : is_initialized_(false),
        is_shutdown_(false),
        completed_basic_startup_(false),
        delegate_(NULL),
        ui_task_(NULL) {
#if defined(OS_WIN)
    memset(&sandbox_info_, 0, sizeof(sandbox_info_));
#endif
  }

  ~ContentMainRunnerImpl() override {
    if (is_initialized_ && !is_shutdown_)
      Shutdown();
  }

  int Initialize(const ContentMainParams& params) override {
    ui_task_ = params.ui_task;

    base::EnableTerminationOnOutOfMemory();
#if defined(OS_WIN)
    base::win::RegisterInvalidParamHandler();
#if !defined(TOOLKIT_QT)
    ui::win::CreateATLModuleIfNeeded();
#endif

    sandbox_info_ = *params.sandbox_info;
#else  // !OS_WIN

#if defined(OS_ANDROID)
    // See note at the initialization of ExitManager, below; basically,
    // only Android builds have the ctor/dtor handlers set up to use
    // TRACE_EVENT right away.
    TRACE_EVENT0("startup,benchmark", "ContentMainRunnerImpl::Initialize");
#endif  // OS_ANDROID

    base::GlobalDescriptors* g_fds = base::GlobalDescriptors::GetInstance();

    // On Android,
    // - setlocale() is not supported.
    // - We do not override the signal handlers so that we can get
    //   stack trace when crashing.
    // - The ipc_fd is passed through the Java service.
    // Thus, these are all disabled.
#if !defined(OS_ANDROID)
    // Set C library locale to make sure CommandLine can parse argument values
    // in correct encoding.
    setlocale(LC_ALL, "");

    if (params.setup_signal_handlers) {
      SetupSignalHandlers();
    } else {
        // Ignore SIGPIPE even if we are not resetting the other signal handlers.
        CHECK(signal(SIGPIPE, SIG_IGN) != SIG_ERR);
    }

    g_fds->Set(kPrimaryIPCChannel,
               kPrimaryIPCChannel + base::GlobalDescriptors::kBaseDescriptor);
    g_fds->Set(kMojoIPCChannel,
               kMojoIPCChannel + base::GlobalDescriptors::kBaseDescriptor);
#endif  // !OS_ANDROID

#if defined(OS_LINUX) || defined(OS_OPENBSD)
    g_fds->Set(kCrashDumpSignal,
               kCrashDumpSignal + base::GlobalDescriptors::kBaseDescriptor);
#endif  // OS_LINUX || OS_OPENBSD


#endif  // !OS_WIN

    is_initialized_ = true;
    delegate_ = params.delegate;

    // The exit manager is in charge of calling the dtors of singleton objects.
    // On Android, AtExitManager is set up when library is loaded.
    // A consequence of this is that you can't use the ctor/dtor-based
    // TRACE_EVENT methods on Linux or iOS builds till after we set this up.
#if !defined(OS_ANDROID)
    if (!ui_task_) {
      // When running browser tests, don't create a second AtExitManager as that
      // interfers with shutdown when objects created before ContentMain is
      // called are destructed when it returns.
      exit_manager_.reset(new base::AtExitManager);
    }
#endif  // !OS_ANDROID

#if defined(OS_MACOSX)
#if !defined(TOOLKIT_QT)
    // We need this pool for all the objects created before we get to the
    // event loop, but we don't want to leave them hanging around until the
    // app quits. Each "main" needs to flush this pool right before it goes into
    // its main event loop to get rid of the cruft.
    autorelease_pool_.reset(new base::mac::ScopedNSAutoreleasePool());
#endif
    InitializeMac();
#endif

    // On Android, the command line is initialized when library is loaded and
    // we have already started our TRACE_EVENT0.
#if !defined(OS_ANDROID)
    // argc/argv are ignored on Windows and Android; see command_line.h for
    // details.
    int argc = 0;
    const char** argv = NULL;

#if !defined(OS_WIN)
    argc = params.argc;
    argv = params.argv;
#endif

    base::CommandLine::Init(argc, argv);

#if !defined(TOOLKIT_QT)
    base::EnableTerminationOnHeapCorruption();
#endif

    // TODO(yiyaoliu, vadimt): Remove this once crbug.com/453640 is fixed.
    // Enable profiler recording right after command line is initialized so that
    // browser startup can be instrumented.
    if (delegate_ && delegate_->ShouldEnableProfilerRecording())
      tracked_objects::ScopedTracker::Enable();

    SetProcessTitleFromCommandLine(argv);
#endif  // !OS_ANDROID

    int exit_code = 0;
    if (delegate_ && delegate_->BasicStartupComplete(&exit_code))
      return exit_code;

    completed_basic_startup_ = true;

    const base::CommandLine& command_line =
        *base::CommandLine::ForCurrentProcess();
    std::string process_type =
        command_line.GetSwitchValueASCII(switches::kProcessType);

    // Initialize mojo here so that services can be registered.
    InitializeMojo();

#if defined(OS_WIN)
    if (command_line.HasSwitch(switches::kDeviceScaleFactor)) {
      std::string scale_factor_string = command_line.GetSwitchValueASCII(
          switches::kDeviceScaleFactor);
      double scale_factor = 0;
      if (base::StringToDouble(scale_factor_string, &scale_factor))
        display::win::SetDefaultDeviceScaleFactor(scale_factor);
    }
#endif

    if (!GetContentClient())
      SetContentClient(&empty_content_client_);
    ContentClientInitializer::Set(process_type, delegate_);

#if defined(OS_WIN)
    // Route stdio to parent console (if any) or create one.
    if (command_line.HasSwitch(switches::kEnableLogging))
      base::RouteStdioToConsole(true);
#endif

#if !defined(OS_ANDROID)
    // Enable startup tracing asap to avoid early TRACE_EVENT calls being
    // ignored. For Android, startup tracing is enabled in an even earlier place
    // content/app/android/library_loader_hooks.cc.
    if (command_line.HasSwitch(switches::kTraceStartup)) {
      base::trace_event::TraceConfig trace_config(
          command_line.GetSwitchValueASCII(switches::kTraceStartup),
          base::trace_event::RECORD_UNTIL_FULL);
      base::trace_event::TraceLog::GetInstance()->SetEnabled(
          trace_config,
          base::trace_event::TraceLog::RECORDING_MODE);
    } else if (command_line.HasSwitch(switches::kTraceToConsole)) {
      base::trace_event::TraceConfig trace_config =
          tracing::GetConfigForTraceToConsole();
      LOG(ERROR) << "Start " << switches::kTraceToConsole
                 << " with CategoryFilter '"
                 << trace_config.ToCategoryFilterString() << "'.";
      base::trace_event::TraceLog::GetInstance()->SetEnabled(
          trace_config,
          base::trace_event::TraceLog::RECORDING_MODE);
    } else if (process_type != switches::kZygoteProcess &&
               process_type != switches::kRendererProcess) {
      if (tracing::TraceConfigFile::GetInstance()->IsEnabled()) {
        // This checks kTraceConfigFile switch.
        base::trace_event::TraceLog::GetInstance()->SetEnabled(
            tracing::TraceConfigFile::GetInstance()->GetTraceConfig(),
            base::trace_event::TraceLog::RECORDING_MODE);
      }
    }
#endif  // !OS_ANDROID

#if defined(OS_WIN)
    // Enable exporting of events to ETW if requested on the command line.
    if (command_line.HasSwitch(switches::kTraceExportEventsToETW))
      base::trace_event::TraceEventETWExport::EnableETWExport();
#endif  // OS_WIN

#if !defined(OS_ANDROID)
    // Android tracing started at the beginning of the method.
    // Other OSes have to wait till we get here in order for all the memory
    // management setup to be completed.
    TRACE_EVENT0("startup,benchmark", "ContentMainRunnerImpl::Initialize");
#endif  // !OS_ANDROID

#if defined(OS_MACOSX)
    // We need to allocate the IO Ports before the Sandbox is initialized or
    // the first instance of PowerMonitor is created.
    // It's important not to allocate the ports for processes which don't
    // register with the power monitor - see crbug.com/88867.
    if (process_type.empty() ||
        (delegate_ &&
         delegate_->ProcessRegistersWithSystemProcess(process_type))) {
      base::PowerMonitorDeviceSource::AllocateSystemIOPorts();
    }

    if (!process_type.empty() &&
        (!delegate_ || delegate_->ShouldSendMachPort(process_type))) {
      MachBroker::ChildSendTaskPortToParent();
    }
#elif defined(OS_WIN)
    base::win::SetupCRT(command_line);
#endif

    // If we are on a platform where the default allocator is overridden (shim
    // layer on windows, tcmalloc on Linux Desktop) smoke-tests that the
    // overriding logic is working correctly. If not causes a hard crash, as its
    // unexpected absence has security implications.
    CHECK(base::allocator::IsAllocatorInitialized());

#if defined(OS_POSIX)
    if (!process_type.empty()) {
      // When you hit Ctrl-C in a terminal running the browser
      // process, a SIGINT is delivered to the entire process group.
      // When debugging the browser process via gdb, gdb catches the
      // SIGINT for the browser process (and dumps you back to the gdb
      // console) but doesn't for the child processes, killing them.
      // The fix is to have child processes ignore SIGINT; they'll die
      // on their own when the browser process goes away.
      //
      // Note that we *can't* rely on BeingDebugged to catch this case because
      // we are the child process, which is not being debugged.
      // TODO(evanm): move this to some shared subprocess-init function.
      if (!base::debug::BeingDebugged())
        signal(SIGINT, SIG_IGN);
    }
#endif

#if defined(USE_NSS_CERTS)
    crypto::EarlySetupForNSSInit();
#endif

    ui::RegisterPathProvider();
    RegisterPathProvider();
    RegisterContentSchemes(true);

#if defined(OS_ANDROID)
    int icudata_fd = g_fds->MaybeGet(kAndroidICUDataDescriptor);
    if (icudata_fd != -1) {
      auto icudata_region = g_fds->GetRegion(kAndroidICUDataDescriptor);
      CHECK(base::i18n::InitializeICUWithFileDescriptor(icudata_fd,
                                                        icudata_region));
    } else {
      CHECK(base::i18n::InitializeICU());
    }
#else
    CHECK(base::i18n::InitializeICU());
#endif  // OS_ANDROID

    base::StatisticsRecorder::Initialize();

#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
#if defined(OS_POSIX) && !defined(OS_MACOSX)
#if !defined(OS_ANDROID)
    // kV8NativesDataDescriptor and kV8SnapshotDataDescriptor could be shared
    // with child processes via file descriptors. On Android they are set in
    // ChildProcessService::InternalInitChildProcess, otherwise set them here.
    if (command_line.HasSwitch(switches::kV8NativesPassedByFD)) {
      g_fds->Set(
          kV8NativesDataDescriptor,
          kV8NativesDataDescriptor + base::GlobalDescriptors::kBaseDescriptor);
    }
    if (command_line.HasSwitch(switches::kV8SnapshotPassedByFD)) {
      g_fds->Set(
          kV8SnapshotDataDescriptor,
          kV8SnapshotDataDescriptor + base::GlobalDescriptors::kBaseDescriptor);
    }
#endif  // !OS_ANDROID
    int v8_natives_fd = g_fds->MaybeGet(kV8NativesDataDescriptor);
    int v8_snapshot_fd = g_fds->MaybeGet(kV8SnapshotDataDescriptor);
    if (v8_snapshot_fd != -1) {
      auto v8_snapshot_region = g_fds->GetRegion(kV8SnapshotDataDescriptor);
      gin::V8Initializer::LoadV8SnapshotFromFD(
          v8_snapshot_fd, v8_snapshot_region.offset, v8_snapshot_region.size);
    } else {
      gin::V8Initializer::LoadV8Snapshot();
    }
    if (v8_natives_fd != -1) {
      auto v8_natives_region = g_fds->GetRegion(kV8NativesDataDescriptor);
      gin::V8Initializer::LoadV8NativesFromFD(
          v8_natives_fd, v8_natives_region.offset, v8_natives_region.size);
    } else {
      gin::V8Initializer::LoadV8Natives();
    }
#else
    gin::V8Initializer::LoadV8Snapshot();
    gin::V8Initializer::LoadV8Natives();
#endif  // OS_POSIX && !OS_MACOSX
#endif  // V8_USE_EXTERNAL_STARTUP_DATA

    if (delegate_)
      delegate_->PreSandboxStartup();

    if (!process_type.empty())
      CommonSubprocessInit(process_type);

#if defined(OS_WIN)
    CHECK(InitializeSandbox(params.sandbox_info));
#elif defined(OS_MACOSX)
    if (process_type == switches::kRendererProcess ||
        process_type == switches::kPpapiPluginProcess ||
        (delegate_ && delegate_->DelaySandboxInitialization(process_type))) {
      // On OS X the renderer sandbox needs to be initialized later in the
      // startup sequence in RendererMainPlatformDelegate::EnableSandbox().
    } else {
      CHECK(InitializeSandbox());
    }
#endif

    if (delegate_)
      delegate_->SandboxInitialized(process_type);

    // Return -1 to indicate no early termination.
    return -1;
  }

  int Run() override {
    DCHECK(is_initialized_);
    DCHECK(!is_shutdown_);
    const base::CommandLine& command_line =
        *base::CommandLine::ForCurrentProcess();
    std::string process_type =
        command_line.GetSwitchValueASCII(switches::kProcessType);

    // Run this logic on all child processes. Zygotes will run this at a later
    // point in time when the command line has been updated.
    std::unique_ptr<base::FieldTrialList> field_trial_list;
    if (!process_type.empty() && process_type != switches::kZygoteProcess)
      InitializeFieldTrialAndFeatureList(&field_trial_list);

    base::HistogramBase::EnableActivityReportHistogram(process_type);

    MainFunctionParams main_params(command_line);
    main_params.ui_task = ui_task_;
#if defined(OS_WIN)
    main_params.sandbox_info = &sandbox_info_;
#elif defined(OS_MACOSX) && !defined(TOOLKIT_QT)
    main_params.autorelease_pool = autorelease_pool_.get();
#endif

    return RunNamedProcessTypeMain(process_type, main_params, delegate_);
  }

  void Shutdown() override {
    DCHECK(is_initialized_);
    DCHECK(!is_shutdown_);

    if (completed_basic_startup_ && delegate_) {
      const base::CommandLine& command_line =
          *base::CommandLine::ForCurrentProcess();
      std::string process_type =
          command_line.GetSwitchValueASCII(switches::kProcessType);

      delegate_->ProcessExiting(process_type);
    }

#if defined(OS_WIN)
#ifdef _CRTDBG_MAP_ALLOC
    _CrtDumpMemoryLeaks();
#endif  // _CRTDBG_MAP_ALLOC
#endif  // OS_WIN

#if defined(OS_MACOSX) && !defined(TOOLKIT_QT)
    autorelease_pool_.reset(NULL);
#endif

    exit_manager_.reset(NULL);

    delegate_ = NULL;
    is_shutdown_ = true;
  }

 private:
  // True if the runner has been initialized.
  bool is_initialized_;

  // True if the runner has been shut down.
  bool is_shutdown_;

  // True if basic startup was completed.
  bool completed_basic_startup_;

  // Used if the embedder doesn't set one.
  ContentClient empty_content_client_;

  // The delegate will outlive this object.
  ContentMainDelegate* delegate_;

  std::unique_ptr<base::AtExitManager> exit_manager_;
#if defined(OS_WIN)
  sandbox::SandboxInterfaceInfo sandbox_info_;
#elif defined(OS_MACOSX) && !defined(TOOLKIT_QT)
  std::unique_ptr<base::mac::ScopedNSAutoreleasePool> autorelease_pool_;
#endif

  base::Closure* ui_task_;

  DISALLOW_COPY_AND_ASSIGN(ContentMainRunnerImpl);
};

// static
ContentMainRunner* ContentMainRunner::Create() {
  return new ContentMainRunnerImpl();
}

}  // namespace content
