// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/app/content_main_runner.h"

#include <stdlib.h>

#include "base/allocator/allocator_extension.h"
#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/debug/trace_event.h"
#include "base/files/file_path.h"
#include "base/i18n/icu_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/metrics/stats_table.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/memory.h"
#include "base/process/process_handle.h"
#include "base/profiler/alternate_timer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "content/browser/browser_main.h"
#include "content/common/set_process_title.h"
#include "content/common/url_schemes.h"
#include "content/gpu/in_process_gpu_thread.h"
#include "content/public/app/content_main.h"
#include "content/public/app/content_main_delegate.h"
#include "content/public/app/startup_helper_win.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "content/public/common/sandbox_init.h"
#include "content/renderer/in_process_renderer_thread.h"
#include "content/utility/in_process_utility_thread.h"
#include "crypto/nss_util.h"
#include "ipc/ipc_descriptors.h"
#include "ipc/ipc_switches.h"
#include "media/base/media.h"
#include "sandbox/win/src/sandbox_types.h"
#include "ui/base/ui_base_paths.h"
#include "ui/base/ui_base_switches.h"

#if defined(OS_ANDROID)
#include "content/public/common/content_descriptors.h"
#endif

#if defined(USE_TCMALLOC)
#include "third_party/tcmalloc/chromium/src/gperftools/malloc_extension.h"
#if defined(TYPE_PROFILING)
#include "base/allocator/type_profiler.h"
#include "base/allocator/type_profiler_tcmalloc.h"
#endif
#endif

#if !defined(OS_IOS)
#include "content/app/mojo/mojo_init.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/utility_process_host_impl.h"
#include "content/public/plugin/content_plugin_client.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/utility/content_utility_client.h"
#endif

#if defined(OS_WIN)
#include <malloc.h>
#include <cstring>

#include "base/strings/string_number_conversions.h"
#if !defined(TOOLKIT_QT)
#include "ui/base/win/atl_module.h"
#endif
#include "ui/base/win/dpi_setup.h"
#include "ui/gfx/win/dpi.h"
#elif defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#if !defined(OS_IOS)
#include "base/power_monitor/power_monitor_device_source.h"
#include "content/browser/mach_broker_mac.h"
#include "content/common/sandbox_init_mac.h"
#endif  // !OS_IOS
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

#if !defined(OS_MACOSX) && defined(USE_TCMALLOC)
extern "C" {
int tc_set_new_mode(int mode);
}
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
extern int WorkerMain(const MainFunctionParams&);
}  // namespace content

namespace content {

base::LazyInstance<ContentBrowserClient>
    g_empty_content_browser_client = LAZY_INSTANCE_INITIALIZER;
#if !defined(OS_IOS) && !defined(CHROME_MULTIPLE_DLL_BROWSER)
base::LazyInstance<ContentPluginClient>
    g_empty_content_plugin_client = LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<ContentRendererClient>
    g_empty_content_renderer_client = LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<ContentUtilityClient>
    g_empty_content_utility_client = LAZY_INSTANCE_INITIALIZER;
#endif  // !OS_IOS && !CHROME_MULTIPLE_DLL_BROWSER

#if defined(OS_WIN)

#endif  // defined(OS_WIN)

#if defined(OS_POSIX) && !defined(OS_IOS)

// Setup signal-handling state: resanitize most signals, ignore SIGPIPE.
void SetupSignalHandlers() {
  // Sanitise our signal handling state. Signals that were ignored by our
  // parent will also be ignored by us. We also inherit our parent's sigmask.
  sigset_t empty_signal_set;
  CHECK(0 == sigemptyset(&empty_signal_set));
  CHECK(0 == sigprocmask(SIG_SETMASK, &empty_signal_set, NULL));

  struct sigaction sigact;
  memset(&sigact, 0, sizeof(sigact));
  sigact.sa_handler = SIG_DFL;
  static const int signals_to_reset[] =
      {SIGHUP, SIGINT, SIGQUIT, SIGILL, SIGABRT, SIGFPE, SIGSEGV,
       SIGALRM, SIGTERM, SIGCHLD, SIGBUS, SIGTRAP};  // SIGPIPE is set below.
  for (unsigned i = 0; i < arraysize(signals_to_reset); i++) {
    CHECK(0 == sigaction(signals_to_reset[i], &sigact, NULL));
  }

  // Always ignore SIGPIPE.  We check the return value of write().
  CHECK(signal(SIGPIPE, SIG_IGN) != SIG_ERR);
}

#endif  // OS_POSIX && !OS_IOS

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
}

// Only needed on Windows for creating stats tables.
#if defined(OS_WIN)
static base::ProcessId GetBrowserPid(const CommandLine& command_line) {
  base::ProcessId browser_pid = base::GetCurrentProcId();
  if (command_line.HasSwitch(switches::kProcessChannelID)) {
    std::string channel_name =
        command_line.GetSwitchValueASCII(switches::kProcessChannelID);

    int browser_pid_int;
    base::StringToInt(channel_name, &browser_pid_int);
    browser_pid = static_cast<base::ProcessId>(browser_pid_int);
    DCHECK_NE(browser_pid_int, 0);
  }
  return browser_pid;
}
#endif

static void InitializeStatsTable(const CommandLine& command_line) {
  // Initialize the Stats Counters table.  With this initialized,
  // the StatsViewer can be utilized to read counters outside of
  // Chrome.  These lines can be commented out to effectively turn
  // counters 'off'.  The table is created and exists for the life
  // of the process.  It is not cleaned up.
  if (command_line.HasSwitch(switches::kEnableStatsTable)) {
    // NOTIMPLEMENTED: we probably need to shut this down correctly to avoid
    // leaking shared memory regions on posix platforms.
#if defined(OS_POSIX)
    // Stats table is in the global file descriptors table on Posix.
    base::GlobalDescriptors* global_descriptors =
        base::GlobalDescriptors::GetInstance();
    base::FileDescriptor table_ident;
    if (global_descriptors->MaybeGet(kStatsTableSharedMemFd) != -1) {
      // Open the shared memory file descriptor passed by the browser process.
      table_ident = base::FileDescriptor(
          global_descriptors->Get(kStatsTableSharedMemFd), false);
    }
#elif defined(OS_WIN)
    // Stats table is in a named segment on Windows. Use the PID to make this
    // unique on the system.
    std::string table_ident =
      base::StringPrintf("%s-%u", kStatsFilename,
          static_cast<unsigned int>(GetBrowserPid(command_line)));
#endif
    base::StatsTable* stats_table =
        new base::StatsTable(table_ident, kStatsMaxThreads, kStatsMaxCounters);
    base::StatsTable::set_current(stats_table);
  }
}

class ContentClientInitializer {
 public:
  static void Set(const std::string& process_type,
                  ContentMainDelegate* delegate) {
    ContentClient* content_client = GetContentClient();
    if (process_type.empty()) {
      if (delegate)
        content_client->browser_ = delegate->CreateContentBrowserClient();
      if (!content_client->browser_)
        content_client->browser_ = &g_empty_content_browser_client.Get();
    }

#if !defined(OS_IOS) && !defined(CHROME_MULTIPLE_DLL_BROWSER)
    if (process_type == switches::kPluginProcess ||
        process_type == switches::kPpapiPluginProcess) {
      if (delegate)
        content_client->plugin_ = delegate->CreateContentPluginClient();
      if (!content_client->plugin_)
        content_client->plugin_ = &g_empty_content_plugin_client.Get();
      // Single process not supported in split dll mode.
    } else if (process_type == switches::kRendererProcess ||
               CommandLine::ForCurrentProcess()->HasSwitch(
                   switches::kSingleProcess)) {
      if (delegate)
        content_client->renderer_ = delegate->CreateContentRendererClient();
      if (!content_client->renderer_)
        content_client->renderer_ = &g_empty_content_renderer_client.Get();
    }

    if (process_type == switches::kUtilityProcess ||
        CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kSingleProcess)) {
      if (delegate)
        content_client->utility_ = delegate->CreateContentUtilityClient();
      // TODO(scottmg): http://crbug.com/237249 Should be in _child.
      if (!content_client->utility_)
        content_client->utility_ = &g_empty_content_utility_client.Get();
    }
#endif  // !OS_IOS && !CHROME_MULTIPLE_DLL_BROWSER
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
    { switches::kWorkerProcess,      WorkerMain },
#if defined(ENABLE_PLUGINS)
    { switches::kPpapiPluginProcess, PpapiPluginMain },
#endif
    { switches::kUtilityProcess,     UtilityMain },
  };

  ScopedVector<ZygoteForkDelegate> zygote_fork_delegates;
  if (delegate) {
    delegate->ZygoteStarting(&zygote_fork_delegates);
    // Each Renderer we spawn will re-attempt initialization of the media
    // libraries, at which point failure will be detected and handled, so
    // we do not need to cope with initialization failures here.
    base::FilePath media_path;
    if (PathService::Get(DIR_MEDIA_LIBS, &media_path))
      media::InitializeMediaLibrary(media_path);
  }

  // This function call can return multiple times, once per fork().
  if (!ZygoteMain(main_function_params, zygote_fork_delegates.Pass()))
    return 1;

  if (delegate) delegate->ZygoteForked();

  // Zygote::HandleForkRequest may have reallocated the command
  // line so update it here with the new version.
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);
  ContentClientInitializer::Set(process_type, delegate);

  // The StatsTable must be initialized in each process; we already
  // initialized for the browser process, now we need to initialize
  // within the new processes as well.
  InitializeStatsTable(command_line);

  MainFunctionParams main_params(command_line);

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

#if !defined(OS_IOS)
static void RegisterMainThreadFactories() {
#if !defined(CHROME_MULTIPLE_DLL_BROWSER)
  UtilityProcessHostImpl::RegisterUtilityMainThreadFactory(
      CreateInProcessUtilityThread);
  RenderProcessHostImpl::RegisterRendererMainThreadFactory(
      CreateInProcessRendererThread);
  GpuProcessHost::RegisterGpuMainThreadFactory(
      CreateInProcessGpuThread);
#else
  CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kSingleProcess)) {
    LOG(FATAL) <<
        "--single-process is not supported in chrome multiple dll browser.";
  }
  if (command_line.HasSwitch(switches::kInProcessGPU)) {
    LOG(FATAL) <<
        "--in-process-gpu is not supported in chrome multiple dll browser.";
  }
#endif
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
#if !defined(OS_LINUX)
    { switches::kPluginProcess,      PluginMain },
#endif
    { switches::kWorkerProcess,      WorkerMain },
    { switches::kPpapiPluginProcess, PpapiPluginMain },
    { switches::kPpapiBrokerProcess, PpapiBrokerMain },
#endif  // ENABLE_PLUGINS
    { switches::kUtilityProcess,     UtilityMain },
    { switches::kRendererProcess,    RendererMain },
    { switches::kGpuProcess,         GpuMain },
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
#endif  // !OS_IOS

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

  virtual ~ContentMainRunnerImpl() {
    if (is_initialized_ && !is_shutdown_)
      Shutdown();
  }

#if defined(USE_TCMALLOC)
  static bool GetAllocatorWasteSizeThunk(size_t* size) {
    size_t heap_size, allocated_bytes, unmapped_bytes;
    MallocExtension* ext = MallocExtension::instance();
    if (ext->GetNumericProperty("generic.heap_size", &heap_size) &&
        ext->GetNumericProperty("generic.current_allocated_bytes",
                                &allocated_bytes) &&
        ext->GetNumericProperty("tcmalloc.pageheap_unmapped_bytes",
                                &unmapped_bytes)) {
      *size = heap_size - allocated_bytes - unmapped_bytes;
      return true;
    }
    DCHECK(false);
    return false;
  }

  static void GetStatsThunk(char* buffer, int buffer_length) {
    MallocExtension::instance()->GetStats(buffer, buffer_length);
  }

  static void ReleaseFreeMemoryThunk() {
    MallocExtension::instance()->ReleaseFreeMemory();
  }
#endif

  virtual int Initialize(const ContentMainParams& params) OVERRIDE {
    ui_task_ = params.ui_task;

#if defined(OS_WIN)
    RegisterInvalidParamHandler();
#if !defined(TOOLKIT_QT)
    ui::win::CreateATLModuleIfNeeded();
#endif

    sandbox_info_ = *params.sandbox_info;
#else  // !OS_WIN

#if defined(OS_ANDROID)
    // See note at the initialization of ExitManager, below; basically,
    // only Android builds have the ctor/dtor handlers set up to use
    // TRACE_EVENT right away.
    TRACE_EVENT0("startup", "ContentMainRunnerImpl::Initialize");
#endif  // OS_ANDROID

    // NOTE(willchan): One might ask why these TCMalloc-related calls are done
    // here rather than in process_util_linux.cc with the definition of
    // EnableTerminationOnOutOfMemory().  That's because base shouldn't have a
    // dependency on TCMalloc.  Really, we ought to have our allocator shim code
    // implement this EnableTerminationOnOutOfMemory() function.  Whateverz.
    // This works for now.
#if !defined(OS_MACOSX) && defined(USE_TCMALLOC)

#if defined(TYPE_PROFILING)
    base::type_profiler::InterceptFunctions::SetFunctions(
        base::type_profiler::NewInterceptForTCMalloc,
        base::type_profiler::DeleteInterceptForTCMalloc);
#endif

    // For tcmalloc, we need to tell it to behave like new.
    tc_set_new_mode(1);

    // On windows, we've already set these thunks up in _heap_init()
    base::allocator::SetGetAllocatorWasteSizeFunction(
        GetAllocatorWasteSizeThunk);
    base::allocator::SetGetStatsFunction(GetStatsThunk);
    base::allocator::SetReleaseFreeMemoryFunction(ReleaseFreeMemoryThunk);

    // Provide optional hook for monitoring allocation quantities on a
    // per-thread basis.  Only set the hook if the environment indicates this
    // needs to be enabled.
    const char* profiling = getenv(tracked_objects::kAlternateProfilerTime);
    if (profiling &&
        (atoi(profiling) == tracked_objects::TIME_SOURCE_TYPE_TCMALLOC)) {
      tracked_objects::SetAlternateTimeSource(
          MallocExtension::GetBytesAllocatedOnCurrentThread,
          tracked_objects::TIME_SOURCE_TYPE_TCMALLOC);
    }
#endif  // !OS_MACOSX && USE_TCMALLOC

    // On Android,
    // - setlocale() is not supported.
    // - We do not override the signal handlers so that we can get
    //   stack trace when crashing.
    // - The ipc_fd is passed through the Java service.
    // Thus, these are all disabled.
#if !defined(OS_ANDROID) && !defined(OS_IOS)
    // Set C library locale to make sure CommandLine can parse argument values
    // in correct encoding.
    setlocale(LC_ALL, "");

    if (params.setup_signal_handlers) {
      SetupSignalHandlers();
    }

    base::GlobalDescriptors* g_fds = base::GlobalDescriptors::GetInstance();
    g_fds->Set(kPrimaryIPCChannel,
               kPrimaryIPCChannel + base::GlobalDescriptors::kBaseDescriptor);
#endif  // !OS_ANDROID && !OS_IOS

#if defined(OS_LINUX) || defined(OS_OPENBSD)
    g_fds->Set(kCrashDumpSignal,
               kCrashDumpSignal + base::GlobalDescriptors::kBaseDescriptor);
#endif

#endif  // !OS_WIN

    is_initialized_ = true;
    delegate_ = params.delegate;

    base::EnableTerminationOnHeapCorruption();
    base::EnableTerminationOnOutOfMemory();

    // The exit manager is in charge of calling the dtors of singleton objects.
    // On Android, AtExitManager is set up when library is loaded.
    // On iOS, it's set up in main(), which can't call directly through to here.
    // A consequence of this is that you can't use the ctor/dtor-based
    // TRACE_EVENT methods on Linux or iOS builds till after we set this up.
#if !defined(OS_ANDROID) && !defined(OS_IOS)
    if (!ui_task_) {
      // When running browser tests, don't create a second AtExitManager as that
      // interfers with shutdown when objects created before ContentMain is
      // called are destructed when it returns.
      exit_manager_.reset(new base::AtExitManager);
    }
#endif  // !OS_ANDROID && !OS_IOS

#if defined(OS_MACOSX) && !defined(TOOLKIT_QT)
    // We need this pool for all the objects created before we get to the
    // event loop, but we don't want to leave them hanging around until the
    // app quits. Each "main" needs to flush this pool right before it goes into
    // its main event loop to get rid of the cruft.
    autorelease_pool_.reset(new base::mac::ScopedNSAutoreleasePool());
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

    CommandLine::Init(argc, argv);

#if !defined(OS_IOS)
    SetProcessTitleFromCommandLine(argv);
#endif
#endif // !OS_ANDROID

    int exit_code;
    if (delegate_ && delegate_->BasicStartupComplete(&exit_code))
      return exit_code;

    completed_basic_startup_ = true;

    const CommandLine& command_line = *CommandLine::ForCurrentProcess();
    std::string process_type =
        command_line.GetSwitchValueASCII(switches::kProcessType);

#if !defined(OS_IOS)
    // Initialize mojo here so that services can be registered.
    InitializeMojo();
#endif

    if (!GetContentClient())
      SetContentClient(&empty_content_client_);
    ContentClientInitializer::Set(process_type, delegate_);

#if defined(OS_WIN)
    // Route stdio to parent console (if any) or create one.
    if (command_line.HasSwitch(switches::kEnableLogging))
      base::RouteStdioToConsole();
#endif

    // Enable startup tracing asap to avoid early TRACE_EVENT calls being
    // ignored.
    if (command_line.HasSwitch(switches::kTraceStartup)) {
      base::debug::CategoryFilter category_filter(
          command_line.GetSwitchValueASCII(switches::kTraceStartup));
      base::debug::TraceLog::GetInstance()->SetEnabled(
          category_filter,
          base::debug::TraceLog::RECORDING_MODE,
          base::debug::TraceLog::RECORD_UNTIL_FULL);
    }
#if !defined(OS_ANDROID)
    // Android tracing started at the beginning of the method.
    // Other OSes have to wait till we get here in order for all the memory
    // management setup to be completed.
    TRACE_EVENT0("startup", "ContentMainRunnerImpl::Initialize");
#endif // !OS_ANDROID

#if defined(OS_MACOSX) && !defined(OS_IOS)
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
    if (command_line.HasSwitch(switches::kEnableHighResolutionTime))
      base::TimeTicks::SetNowIsHighResNowIfSupported();

    bool init_device_scale_factor = true;
    if (command_line.HasSwitch(switches::kDeviceScaleFactor)) {
      std::string scale_factor_string = command_line.GetSwitchValueASCII(
          switches::kDeviceScaleFactor);
      double scale_factor = 0;
      if (base::StringToDouble(scale_factor_string, &scale_factor)) {
        init_device_scale_factor = false;
        gfx::InitDeviceScaleFactor(scale_factor);
      }
    }
    if (init_device_scale_factor)
      ui::win::InitDeviceScaleFactor();

    SetupCRT(command_line);
#endif

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

#if defined(USE_NSS)
    crypto::EarlySetupForNSSInit();
#endif

    ui::RegisterPathProvider();
    RegisterPathProvider();
    RegisterContentSchemes(true);

#if defined(OS_ANDROID)
    int icudata_fd = base::GlobalDescriptors::GetInstance()->MaybeGet(
        kAndroidICUDataDescriptor);
    if (icudata_fd != -1)
      CHECK(base::i18n::InitializeICUWithFileDescriptor(icudata_fd));
    else
      CHECK(base::i18n::InitializeICU());
#else
    CHECK(base::i18n::InitializeICU());
#endif

    InitializeStatsTable(command_line);

    if (delegate_)
      delegate_->PreSandboxStartup();

    if (!process_type.empty())
      CommonSubprocessInit(process_type);

#if defined(OS_WIN)
    CHECK(InitializeSandbox(params.sandbox_info));
#elif defined(OS_MACOSX) && !defined(OS_IOS)
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

  virtual int Run() OVERRIDE {
    DCHECK(is_initialized_);
    DCHECK(!is_shutdown_);
    const CommandLine& command_line = *CommandLine::ForCurrentProcess();
    std::string process_type =
          command_line.GetSwitchValueASCII(switches::kProcessType);

    MainFunctionParams main_params(command_line);
    main_params.ui_task = ui_task_;
#if defined(OS_WIN)
    main_params.sandbox_info = &sandbox_info_;
#elif defined(OS_MACOSX) && !defined(TOOLKIT_QT)
    main_params.autorelease_pool = autorelease_pool_.get();
#endif

#if !defined(OS_IOS)
    return RunNamedProcessTypeMain(process_type, main_params, delegate_);
#else
    return 1;
#endif
  }

  virtual void Shutdown() OVERRIDE {
    DCHECK(is_initialized_);
    DCHECK(!is_shutdown_);

    if (completed_basic_startup_ && delegate_) {
      const CommandLine& command_line = *CommandLine::ForCurrentProcess();
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

  scoped_ptr<base::AtExitManager> exit_manager_;
#if defined(OS_WIN)
  sandbox::SandboxInterfaceInfo sandbox_info_;
#elif defined(OS_MACOSX) && !defined(TOOLKIT_QT)
  scoped_ptr<base::mac::ScopedNSAutoreleasePool> autorelease_pool_;
#endif

  base::Closure* ui_task_;

  DISALLOW_COPY_AND_ASSIGN(ContentMainRunnerImpl);
};

// static
ContentMainRunner* ContentMainRunner::Create() {
  return new ContentMainRunnerImpl();
}

}  // namespace content
