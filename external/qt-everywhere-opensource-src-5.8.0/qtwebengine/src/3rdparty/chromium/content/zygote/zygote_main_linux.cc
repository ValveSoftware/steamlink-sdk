// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/zygote/zygote_main.h"

#include <dlfcn.h>
#include <fcntl.h>
#include <openssl/crypto.h>
#include <openssl/rand.h>
#include <pthread.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_vector.h"
#include "base/native_library.h"
#include "base/pickle.h"
#include "base/posix/eintr_wrapper.h"
#include "base/posix/unix_domain_socket_linux.h"
#include "base/rand_util.h"
#include "base/strings/safe_sprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/sys_info.h"
#include "build/build_config.h"
#include "content/common/child_process_sandbox_support_impl_linux.h"
#include "content/common/font_config_ipc_linux.h"
#include "content/common/sandbox_linux/sandbox_debug_handling_linux.h"
#include "content/common/sandbox_linux/sandbox_linux.h"
#include "content/common/zygote_commands_linux.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "content/public/common/sandbox_linux.h"
#include "content/public/common/zygote_fork_delegate_linux.h"
#include "content/zygote/zygote_linux.h"
#include "sandbox/linux/services/credentials.h"
#include "sandbox/linux/services/init_process_reaper.h"
#include "sandbox/linux/services/namespace_sandbox.h"
#include "sandbox/linux/services/thread_helpers.h"
#include "sandbox/linux/suid/client/setuid_sandbox_client.h"
#include "third_party/WebKit/public/web/linux/WebFontRendering.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"
#include "third_party/skia/include/ports/SkFontConfigInterface.h"
#include "third_party/skia/include/ports/SkFontMgr_android.h"

#if defined(OS_LINUX)
#include <sys/prctl.h>
#endif

#if defined(ENABLE_PLUGINS)
#include "content/common/pepper_plugin_list.h"
#include "content/public/common/pepper_plugin_info.h"
#endif

#if defined(ENABLE_WEBRTC)
#include "third_party/webrtc_overrides/init_webrtc.h"
#endif

#if defined(SANITIZER_COVERAGE)
#include <sanitizer/common_interface_defs.h>
#include <sanitizer/coverage_interface.h>
#endif

namespace content {

namespace {

void CloseFds(const std::vector<int>& fds) {
  for (const auto& it : fds) {
    PCHECK(0 == IGNORE_EINTR(close(it)));
  }
}

void RunTwoClosures(const base::Closure* first, const base::Closure* second) {
  first->Run();
  second->Run();
}

}  // namespace

// See https://chromium.googlesource.com/chromium/src/+/master/docs/linux_zygote.md

static void ProxyLocaltimeCallToBrowser(time_t input, struct tm* output,
                                        char* timezone_out,
                                        size_t timezone_out_len) {
  base::Pickle request;
  request.WriteInt(LinuxSandbox::METHOD_LOCALTIME);
  request.WriteString(
      std::string(reinterpret_cast<char*>(&input), sizeof(input)));

  uint8_t reply_buf[512];
  const ssize_t r = base::UnixDomainSocket::SendRecvMsg(
      GetSandboxFD(), reply_buf, sizeof(reply_buf), NULL, request);
  if (r == -1) {
    memset(output, 0, sizeof(struct tm));
    return;
  }

  base::Pickle reply(reinterpret_cast<char*>(reply_buf), r);
  base::PickleIterator iter(reply);
  std::string result, timezone;
  if (!iter.ReadString(&result) ||
      !iter.ReadString(&timezone) ||
      result.size() != sizeof(struct tm)) {
    memset(output, 0, sizeof(struct tm));
    return;
  }

  memcpy(output, result.data(), sizeof(struct tm));
  if (timezone_out_len) {
    const size_t copy_len = std::min(timezone_out_len - 1, timezone.size());
    memcpy(timezone_out, timezone.data(), copy_len);
    timezone_out[copy_len] = 0;
    output->tm_zone = timezone_out;
  } else {
    output->tm_zone = NULL;
  }
}

static bool g_am_zygote_or_renderer = false;

// Sandbox interception of libc calls.
//
// Because we are running in a sandbox certain libc calls will fail (localtime
// being the motivating example - it needs to read /etc/localtime). We need to
// intercept these calls and proxy them to the browser. However, these calls
// may come from us or from our libraries. In some cases we can't just change
// our code.
//
// It's for these cases that we have the following setup:
//
// We define global functions for those functions which we wish to override.
// Since we will be first in the dynamic resolution order, the dynamic linker
// will point callers to our versions of these functions. However, we have the
// same binary for both the browser and the renderers, which means that our
// overrides will apply in the browser too.
//
// The global |g_am_zygote_or_renderer| is true iff we are in a zygote or
// renderer process. It's set in ZygoteMain and inherited by the renderers when
// they fork. (This means that it'll be incorrect for global constructor
// functions and before ZygoteMain is called - beware).
//
// Our replacement functions can check this global and either proxy
// the call to the browser over the sandbox IPC
// (https://chromium.googlesource.com/chromium/src/+/master/docs/linux_sandbox_ipc.md) or they can use
// dlsym with RTLD_NEXT to resolve the symbol, ignoring any symbols in the
// current module.
//
// Other avenues:
//
// Our first attempt involved some assembly to patch the GOT of the current
// module. This worked, but was platform specific and doesn't catch the case
// where a library makes a call rather than current module.
//
// We also considered patching the function in place, but this would again by
// platform specific and the above technique seems to work well enough.

typedef struct tm* (*LocaltimeFunction)(const time_t* timep);
typedef struct tm* (*LocaltimeRFunction)(const time_t* timep,
                                         struct tm* result);

static pthread_once_t g_libc_localtime_funcs_guard = PTHREAD_ONCE_INIT;
static LocaltimeFunction g_libc_localtime;
static LocaltimeFunction g_libc_localtime64;
static LocaltimeRFunction g_libc_localtime_r;
static LocaltimeRFunction g_libc_localtime64_r;

static void InitLibcLocaltimeFunctions() {
  g_libc_localtime = reinterpret_cast<LocaltimeFunction>(
      dlsym(RTLD_NEXT, "localtime"));
  g_libc_localtime64 = reinterpret_cast<LocaltimeFunction>(
      dlsym(RTLD_NEXT, "localtime64"));
  g_libc_localtime_r = reinterpret_cast<LocaltimeRFunction>(
      dlsym(RTLD_NEXT, "localtime_r"));
  g_libc_localtime64_r = reinterpret_cast<LocaltimeRFunction>(
      dlsym(RTLD_NEXT, "localtime64_r"));

  if (!g_libc_localtime || !g_libc_localtime_r) {
    // http://code.google.com/p/chromium/issues/detail?id=16800
    //
    // Nvidia's libGL.so overrides dlsym for an unknown reason and replaces
    // it with a version which doesn't work. In this case we'll get a NULL
    // result. There's not a lot we can do at this point, so we just bodge it!
    LOG(ERROR) << "Your system is broken: dlsym doesn't work! This has been "
                  "reported to be caused by Nvidia's libGL. You should expect"
                  " time related functions to misbehave. "
                  "http://code.google.com/p/chromium/issues/detail?id=16800";
  }

  if (!g_libc_localtime)
    g_libc_localtime = gmtime;
  if (!g_libc_localtime64)
    g_libc_localtime64 = g_libc_localtime;
  if (!g_libc_localtime_r)
    g_libc_localtime_r = gmtime_r;
  if (!g_libc_localtime64_r)
    g_libc_localtime64_r = g_libc_localtime_r;
}

// Define localtime_override() function with asm name "localtime", so that all
// references to localtime() will resolve to this function. Notice that we need
// to set visibility attribute to "default" to export the symbol, as it is set
// to "hidden" by default in chrome per build/common.gypi.

__attribute__ ((__visibility__("default")))
struct tm* localtime_override(const time_t* timep) {
  if (g_am_zygote_or_renderer) {
    static struct tm time_struct;
    static char timezone_string[64];
    ProxyLocaltimeCallToBrowser(*timep, &time_struct, timezone_string,
                                sizeof(timezone_string));
    return &time_struct;
  } else {
    CHECK_EQ(0, pthread_once(&g_libc_localtime_funcs_guard,
                             InitLibcLocaltimeFunctions));
    struct tm* res = g_libc_localtime(timep);
#if defined(MEMORY_SANITIZER)
    if (res) __msan_unpoison(res, sizeof(*res));
    if (res->tm_zone) __msan_unpoison_string(res->tm_zone);
#endif
    return res;
  }
}

__attribute__ ((__visibility__("default")))
struct tm* localtime64_override(const time_t* timep) {
  if (g_am_zygote_or_renderer) {
    static struct tm time_struct;
    static char timezone_string[64];
    ProxyLocaltimeCallToBrowser(*timep, &time_struct, timezone_string,
                                sizeof(timezone_string));
    return &time_struct;
  } else {
    CHECK_EQ(0, pthread_once(&g_libc_localtime_funcs_guard,
                             InitLibcLocaltimeFunctions));
    struct tm* res = g_libc_localtime64(timep);
#if defined(MEMORY_SANITIZER)
    if (res) __msan_unpoison(res, sizeof(*res));
    if (res->tm_zone) __msan_unpoison_string(res->tm_zone);
#endif
    return res;
  }
}

__attribute__ ((__visibility__("default")))
struct tm* localtime_r_override(const time_t* timep, struct tm* result) {
  if (g_am_zygote_or_renderer) {
    ProxyLocaltimeCallToBrowser(*timep, result, NULL, 0);
    return result;
  } else {
    CHECK_EQ(0, pthread_once(&g_libc_localtime_funcs_guard,
                             InitLibcLocaltimeFunctions));
    struct tm* res = g_libc_localtime_r(timep, result);
#if defined(MEMORY_SANITIZER)
    if (res) __msan_unpoison(res, sizeof(*res));
    if (res->tm_zone) __msan_unpoison_string(res->tm_zone);
#endif
    return res;
  }
}

__attribute__ ((__visibility__("default")))
struct tm* localtime64_r_override(const time_t* timep, struct tm* result) {
  if (g_am_zygote_or_renderer) {
    ProxyLocaltimeCallToBrowser(*timep, result, NULL, 0);
    return result;
  } else {
    CHECK_EQ(0, pthread_once(&g_libc_localtime_funcs_guard,
                             InitLibcLocaltimeFunctions));
    struct tm* res = g_libc_localtime64_r(timep, result);
#if defined(MEMORY_SANITIZER)
    if (res) __msan_unpoison(res, sizeof(*res));
    if (res->tm_zone) __msan_unpoison_string(res->tm_zone);
#endif
    return res;
  }
}

#if defined(ENABLE_PLUGINS)
// Loads the (native) libraries but does not initialize them (i.e., does not
// call PPP_InitializeModule). This is needed by the zygote on Linux to get
// access to the plugins before entering the sandbox.
void PreloadPepperPlugins() {
  std::vector<PepperPluginInfo> plugins;
  ComputePepperPluginList(&plugins);
  for (size_t i = 0; i < plugins.size(); ++i) {
    if (!plugins[i].is_internal) {
      base::NativeLibraryLoadError error;
      base::NativeLibrary library = base::LoadNativeLibrary(plugins[i].path,
                                                            &error);
      VLOG_IF(1, !library) << "Unable to load plugin "
                           << plugins[i].path.value() << " "
                           << error.ToString();

      (void)library;  // Prevent release-mode warning.
    }
  }
}
#endif

// This function triggers the static and lazy construction of objects that need
// to be created before imposing the sandbox.
static void ZygotePreSandboxInit() {
  base::RandUint64();

  base::SysInfo::AmountOfPhysicalMemory();
  base::SysInfo::NumberOfProcessors();

  // ICU DateFormat class (used in base/time_format.cc) needs to get the
  // Olson timezone ID by accessing the zoneinfo files on disk. After
  // TimeZone::createDefault is called once here, the timezone ID is
  // cached and there's no more need to access the file system.
  std::unique_ptr<icu::TimeZone> zone(icu::TimeZone::createDefault());

#if defined(ARCH_CPU_ARM_FAMILY)
  // On ARM, BoringSSL requires access to /proc/cpuinfo to determine processor
  // features. Query this before entering the sandbox.
  CRYPTO_library_init();
#endif

  // Pass BoringSSL a copy of the /dev/urandom file descriptor so RAND_bytes
  // will work inside the sandbox.
  RAND_set_urandom_fd(base::GetUrandomFD());

#if defined(ENABLE_PLUGINS)
  // Ensure access to the Pepper plugins before the sandbox is turned on.
  PreloadPepperPlugins();
#endif
#if defined(ENABLE_WEBRTC)
  InitializeWebRtcModule();
#endif

  SkFontConfigInterface::SetGlobal(
      new FontConfigIPC(GetSandboxFD()))->unref();

  // Set the android SkFontMgr for blink. We need to ensure this is done
  // before the sandbox is initialized to allow the font manager to access
  // font configuration files on disk.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAndroidFontsPath)) {
    std::string android_fonts_dir =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kAndroidFontsPath);

    if (android_fonts_dir.size() > 0 && android_fonts_dir.back() != '/')
      android_fonts_dir += '/';
    std::string font_config = android_fonts_dir + "fonts.xml";
    SkFontMgr_Android_CustomFonts custom;
    custom.fSystemFontUse =
        SkFontMgr_Android_CustomFonts::SystemFontUse::kOnlyCustom;
    custom.fBasePath = android_fonts_dir.c_str();
    custom.fFontsXml = font_config.c_str();
    custom.fFallbackFontsXml = nullptr;
    custom.fIsolated = true;

    blink::WebFontRendering::setSkiaFontManager(SkFontMgr_New_Android(&custom));
  }
}

static bool CreateInitProcessReaper(base::Closure* post_fork_parent_callback) {
  // The current process becomes init(1), this function returns from a
  // newly created process.
  const bool init_created =
      sandbox::CreateInitProcessReaper(post_fork_parent_callback);
  if (!init_created) {
    LOG(ERROR) << "Error creating an init process to reap zombies";
    return false;
  }
  return true;
}

// Enter the setuid sandbox. This requires the current process to have been
// created through the setuid sandbox.
static bool EnterSuidSandbox(sandbox::SetuidSandboxClient* setuid_sandbox,
                             base::Closure* post_fork_parent_callback) {
  DCHECK(setuid_sandbox);
  DCHECK(setuid_sandbox->IsSuidSandboxChild());

  // Use the SUID sandbox.  This still allows the seccomp sandbox to
  // be enabled by the process later.

  if (!setuid_sandbox->IsSuidSandboxUpToDate()) {
    LOG(WARNING) <<
        "You are using a wrong version of the setuid binary!\n"
        "Please read "
        "https://chromium.googlesource.com/chromium/src/+/master/docs/linux_suid_sandbox_development.md."
        "\n\n";
  }

  if (!setuid_sandbox->ChrootMe())
    return false;

  if (setuid_sandbox->IsInNewPIDNamespace()) {
    CHECK_EQ(1, getpid())
        << "The SUID sandbox created a new PID namespace but Zygote "
           "is not the init process. Please, make sure the SUID "
           "binary is up to date.";
  }

  if (getpid() == 1) {
    // The setuid sandbox has created a new PID namespace and we need
    // to assume the role of init.
    CHECK(CreateInitProcessReaper(post_fork_parent_callback));
  }

  CHECK(SandboxDebugHandling::SetDumpableStatusAndHandlers());
  return true;
}

static void DropAllCapabilities(int proc_fd) {
  CHECK(sandbox::Credentials::DropAllCapabilities(proc_fd));
}

static void EnterNamespaceSandbox(LinuxSandbox* linux_sandbox,
                                  base::Closure* post_fork_parent_callback) {
  linux_sandbox->EngageNamespaceSandbox();

  if (getpid() == 1) {
    base::Closure drop_all_caps_callback =
        base::Bind(&DropAllCapabilities, linux_sandbox->proc_fd());
    base::Closure callback = base::Bind(
        &RunTwoClosures, &drop_all_caps_callback, post_fork_parent_callback);
    CHECK(CreateInitProcessReaper(&callback));
  }
}

#if defined(SANITIZER_COVERAGE)
static int g_sanitizer_message_length = 1 * 1024 * 1024;

// A helper process which collects code coverage data from the renderers over a
// socket and dumps it to a file. See http://crbug.com/336212 for discussion.
static void SanitizerCoverageHelper(int socket_fd, int file_fd) {
  std::unique_ptr<char[]> buffer(new char[g_sanitizer_message_length]);
  while (true) {
    ssize_t received_size = HANDLE_EINTR(
        recv(socket_fd, buffer.get(), g_sanitizer_message_length, 0));
    PCHECK(received_size >= 0);
    if (received_size == 0)
      // All clients have closed the socket. We should die.
      _exit(0);
    PCHECK(file_fd >= 0);
    ssize_t written_size = 0;
    while (written_size < received_size) {
      ssize_t write_res =
          HANDLE_EINTR(write(file_fd, buffer.get() + written_size,
                             received_size - written_size));
      PCHECK(write_res >= 0);
      written_size += write_res;
    }
    PCHECK(0 == HANDLE_EINTR(fsync(file_fd)));
  }
}

// fds[0] is the read end, fds[1] is the write end.
static void CreateSanitizerCoverageSocketPair(int fds[2]) {
  PCHECK(0 == socketpair(AF_UNIX, SOCK_SEQPACKET, 0, fds));
  PCHECK(0 == shutdown(fds[0], SHUT_WR));
  PCHECK(0 == shutdown(fds[1], SHUT_RD));

  // Find the right buffer size for sending coverage data.
  // The kernel will silently set the buffer size to the allowed maximum when
  // the specified size is too large, so we set our desired size and read it
  // back.
  int* buf_size = &g_sanitizer_message_length;
  socklen_t option_length = sizeof(*buf_size);
  PCHECK(0 == setsockopt(fds[1], SOL_SOCKET, SO_SNDBUF,
                         buf_size, option_length));
  PCHECK(0 == getsockopt(fds[1], SOL_SOCKET, SO_SNDBUF,
                         buf_size, &option_length));
  DCHECK_EQ(sizeof(*buf_size), option_length);
  // The kernel returns the doubled buffer size.
  *buf_size /= 2;
  PCHECK(*buf_size > 0);
}

static pid_t ForkSanitizerCoverageHelper(
    int child_fd,
    int parent_fd,
    base::ScopedFD file_fd,
    const std::vector<int>& extra_fds_to_close) {
  pid_t pid = fork();
  PCHECK(pid >= 0);
  if (pid == 0) {
    // In the child.
    PCHECK(0 == IGNORE_EINTR(close(parent_fd)));
    CloseFds(extra_fds_to_close);
    SanitizerCoverageHelper(child_fd, file_fd.get());
    _exit(0);
  } else {
    // In the parent.
    PCHECK(0 == IGNORE_EINTR(close(child_fd)));
    return pid;
  }
}

#endif  // defined(SANITIZER_COVERAGE)

static void EnterLayerOneSandbox(LinuxSandbox* linux_sandbox,
                                 const bool using_layer1_sandbox,
                                 base::Closure* post_fork_parent_callback) {
  DCHECK(linux_sandbox);

  ZygotePreSandboxInit();

  // Check that the pre-sandbox initialization didn't spawn threads.
#if !defined(THREAD_SANITIZER)
  DCHECK(sandbox::ThreadHelpers::IsSingleThreaded());
#endif

  sandbox::SetuidSandboxClient* setuid_sandbox =
      linux_sandbox->setuid_sandbox_client();
  if (setuid_sandbox->IsSuidSandboxChild()) {
    CHECK(EnterSuidSandbox(setuid_sandbox, post_fork_parent_callback))
        << "Failed to enter setuid sandbox";
  } else if (sandbox::NamespaceSandbox::InNewUserNamespace()) {
    EnterNamespaceSandbox(linux_sandbox, post_fork_parent_callback);
  } else {
    CHECK(!using_layer1_sandbox);
  }
}

bool ZygoteMain(const MainFunctionParams& params,
                ScopedVector<ZygoteForkDelegate> fork_delegates) {
  g_am_zygote_or_renderer = true;

  std::vector<int> fds_to_close_post_fork;

  LinuxSandbox* linux_sandbox = LinuxSandbox::GetInstance();

#if defined(SANITIZER_COVERAGE)
  const std::string sancov_file_name =
      "zygote." + base::Uint64ToString(base::RandUint64());
  base::ScopedFD sancov_file_fd(
      __sanitizer_maybe_open_cov_file(sancov_file_name.c_str()));
  int sancov_socket_fds[2] = {-1, -1};
  CreateSanitizerCoverageSocketPair(sancov_socket_fds);
  linux_sandbox->sanitizer_args()->coverage_sandboxed = 1;
  linux_sandbox->sanitizer_args()->coverage_fd = sancov_socket_fds[1];
  linux_sandbox->sanitizer_args()->coverage_max_block_size =
      g_sanitizer_message_length;
  // Zygote termination will block until the helper process exits, which will
  // not happen until the write end of the socket is closed everywhere. Make
  // sure the init process does not hold on to it.
  fds_to_close_post_fork.push_back(sancov_socket_fds[0]);
  fds_to_close_post_fork.push_back(sancov_socket_fds[1]);
#endif  // SANITIZER_COVERAGE

  // Skip pre-initializing sandbox under --no-sandbox for crbug.com/444900.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kNoSandbox)) {
    // This will pre-initialize the various sandboxes that need it.
    linux_sandbox->PreinitializeSandbox();
  }

  const bool using_setuid_sandbox =
      linux_sandbox->setuid_sandbox_client()->IsSuidSandboxChild();
  const bool using_namespace_sandbox =
      sandbox::NamespaceSandbox::InNewUserNamespace();
  const bool using_layer1_sandbox =
      using_setuid_sandbox || using_namespace_sandbox;

  if (using_setuid_sandbox) {
    linux_sandbox->setuid_sandbox_client()->CloseDummyFile();
  }

  if (using_layer1_sandbox) {
    // Let the ZygoteHost know we're booting up.
    CHECK(base::UnixDomainSocket::SendMsg(kZygoteSocketPairFd,
                                          kZygoteBootMessage,
                                          sizeof(kZygoteBootMessage),
                                          std::vector<int>()));
  }

  VLOG(1) << "ZygoteMain: initializing " << fork_delegates.size()
          << " fork delegates";
  for (ZygoteForkDelegate* fork_delegate : fork_delegates) {
    fork_delegate->Init(GetSandboxFD(), using_layer1_sandbox);
  }

  const std::vector<int> sandbox_fds_to_close_post_fork =
      linux_sandbox->GetFileDescriptorsToClose();

  fds_to_close_post_fork.insert(fds_to_close_post_fork.end(),
                                sandbox_fds_to_close_post_fork.begin(),
                                sandbox_fds_to_close_post_fork.end());
  base::Closure post_fork_parent_callback =
      base::Bind(&CloseFds, fds_to_close_post_fork);

  // Turn on the first layer of the sandbox if the configuration warrants it.
  EnterLayerOneSandbox(linux_sandbox, using_layer1_sandbox,
                       &post_fork_parent_callback);

  // Extra children and file descriptors created that the Zygote must have
  // knowledge of.
  std::vector<pid_t> extra_children;
  std::vector<int> extra_fds;

#if defined(SANITIZER_COVERAGE)
  pid_t sancov_helper_pid = ForkSanitizerCoverageHelper(
      sancov_socket_fds[0], sancov_socket_fds[1], std::move(sancov_file_fd),
      sandbox_fds_to_close_post_fork);
  // It's important that the zygote reaps the helper before dying. Otherwise,
  // the destruction of the PID namespace could kill the helper before it
  // completes its I/O tasks. |sancov_helper_pid| will exit once the last
  // renderer holding the write end of |sancov_socket_fds| closes it.
  extra_children.push_back(sancov_helper_pid);
  // Sanitizer code in the renderers will inherit the write end of the socket
  // from the zygote. We must keep it open until the very end of the zygote's
  // lifetime, even though we don't explicitly use it.
  extra_fds.push_back(sancov_socket_fds[1]);
#endif  // SANITIZER_COVERAGE

  const int sandbox_flags = linux_sandbox->GetStatus();

  const bool setuid_sandbox_engaged = sandbox_flags & kSandboxLinuxSUID;
  CHECK_EQ(using_setuid_sandbox, setuid_sandbox_engaged);

  const bool namespace_sandbox_engaged = sandbox_flags & kSandboxLinuxUserNS;
  CHECK_EQ(using_namespace_sandbox, namespace_sandbox_engaged);

  Zygote zygote(sandbox_flags, std::move(fork_delegates), extra_children,
                extra_fds);
  // This function call can return multiple times, once per fork().
  return zygote.ProcessRequests();
}

}  // namespace content
