// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// For linux_syscall_support.h. This makes it safe to call embedded system
// calls when in seccomp mode.

#include "components/crash/content/app/breakpad_linux.h"

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <string>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/linux_util.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/posix/eintr_wrapper.h"
#include "base/posix/global_descriptors.h"
#include "base/process/memory.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_checker.h"
#include "breakpad/src/client/linux/crash_generation/crash_generation_client.h"
#include "breakpad/src/client/linux/handler/exception_handler.h"
#include "breakpad/src/client/linux/minidump_writer/directory_reader.h"
#include "breakpad/src/common/linux/linux_libc_support.h"
#include "breakpad/src/common/memory.h"
#include "build/build_config.h"
#include "components/crash/content/app/breakpad_linux_impl.h"
#include "components/crash/content/app/crash_reporter_client.h"
#include "components/crash/core/common/crash_keys.h"
#include "content/public/common/content_descriptors.h"

#if defined(OS_ANDROID)
#include <android/log.h>
#include <sys/stat.h>

#include "base/android/build_info.h"
#include "base/android/path_utils.h"
#include "base/debug/leak_annotations.h"
#endif
#include "third_party/lss/linux_syscall_support.h"

#if defined(ADDRESS_SANITIZER)
#include <ucontext.h>  // for getcontext().
#endif

#if defined(OS_ANDROID)
#define STAT_STRUCT struct stat
#define FSTAT_FUNC fstat
#else
#define STAT_STRUCT struct kernel_stat
#define FSTAT_FUNC sys_fstat
#endif

// Some versions of gcc are prone to warn about unused return values. In cases
// where we either a) know the call cannot fail, or b) there is nothing we
// can do when a call fails, we mark the return code as ignored. This avoids
// spurious compiler warnings.
#define IGNORE_RET(x) ignore_result(x)

using crash_reporter::GetCrashReporterClient;
using google_breakpad::ExceptionHandler;
using google_breakpad::MinidumpDescriptor;

namespace breakpad {

namespace {

#if !defined(OS_CHROMEOS)
const char kUploadURL[] = "https://clients2.google.com/cr/report";
#endif

bool g_is_crash_reporter_enabled = false;
uint64_t g_process_start_time = 0;
pid_t g_pid = 0;
char* g_crash_log_path = nullptr;
ExceptionHandler* g_breakpad = nullptr;

#if defined(ADDRESS_SANITIZER)
const char* g_asan_report_str = nullptr;
#endif
#if defined(OS_ANDROID)
const char kWebViewProcessType[] = "webview";
char* g_process_type = nullptr;
ExceptionHandler* g_microdump = nullptr;
int g_signal_code_pipe_fd = -1;

class MicrodumpInfo {
 public:
  MicrodumpInfo()
      : microdump_build_fingerprint_(nullptr),
        microdump_product_info_(nullptr),
        microdump_gpu_fingerprint_(nullptr) {}

  // The order in which SetGpuFingerprint and Initialize are called
  // may be dependent on the timing of the availability of GPU
  // information. For this reason, they can be called in either order,
  // resulting in the same effect.
  //
  // The following restrictions apply, however:
  // * Both methods must be called from the same thread.
  // * Both methods must be called at most once.
  //
  // Microdumps will only be generated if Initialize is called. If
  // SetGpuFingerprint has not been called called at the point at
  // which a microdump is generated, then the GPU fingerprint will be
  // UNKNOWN.
  void SetGpuFingerprint(const std::string& gpu_fingerprint);
  void Initialize(const std::string& process_type,
                  const char* product_name,
                  const char* product_version,
                  const char* android_build_fp);

 private:
  base::ThreadChecker thread_checker_;
  const char* microdump_build_fingerprint_;
  const char* microdump_product_info_;
  const char* microdump_gpu_fingerprint_;
};

base::LazyInstance<MicrodumpInfo> g_microdump_info =
    LAZY_INSTANCE_INITIALIZER;

#endif

CrashKeyStorage* g_crash_keys = nullptr;

// Writes the value |v| as 16 hex characters to the memory pointed at by
// |output|.
void write_uint64_hex(char* output, uint64_t v) {
  static const char hextable[] = "0123456789abcdef";

  for (int i = 15; i >= 0; --i) {
    output[i] = hextable[v & 15];
    v >>= 4;
  }
}

// The following helper functions are for calculating uptime.

// Converts a struct timeval to milliseconds.
uint64_t timeval_to_ms(struct timeval *tv) {
  uint64_t ret = tv->tv_sec;  // Avoid overflow by explicitly using a uint64_t.
  ret *= 1000;
  ret += tv->tv_usec / 1000;
  return ret;
}

// Converts a struct timeval to milliseconds.
uint64_t kernel_timeval_to_ms(struct kernel_timeval *tv) {
  uint64_t ret = tv->tv_sec;  // Avoid overflow by explicitly using a uint64_t.
  ret *= 1000;
  ret += tv->tv_usec / 1000;
  return ret;
}

// String buffer size to use to convert a uint64_t to string.
const size_t kUint64StringSize = 21;

void SetProcessStartTime() {
  // Set the base process start time value.
  struct timeval tv;
  if (!gettimeofday(&tv, nullptr))
    g_process_start_time = timeval_to_ms(&tv);
  else
    g_process_start_time = 0;
}

// uint64_t version of my_int_len() from
// breakpad/src/common/linux/linux_libc_support.h. Return the length of the
// given, non-negative integer when expressed in base 10.
unsigned my_uint64_len(uint64_t i) {
  if (!i)
    return 1;

  unsigned len = 0;
  while (i) {
    len++;
    i /= 10;
  }

  return len;
}

// uint64_t version of my_uitos() from
// breakpad/src/common/linux/linux_libc_support.h. Convert a non-negative
// integer to a string (not null-terminated).
void my_uint64tos(char* output, uint64_t i, unsigned i_len) {
  for (unsigned index = i_len; index; --index, i /= 10)
    output[index - 1] = '0' + (i % 10);
}

#if !defined(OS_CHROMEOS)
bool my_isxdigit(char c) {
  return base::IsAsciiDigit(c) || ((c | 0x20) >= 'a' && (c | 0x20) <= 'f');
}
#endif

size_t LengthWithoutTrailingSpaces(const char* str, size_t len) {
  while (len > 0 && str[len - 1] == ' ') {
    len--;
  }
  return len;
}

bool GetEnableCrashReporterSwitchParts(const base::CommandLine& command_line,
                                       std::vector<std::string>* switch_parts) {
  std::string switch_value =
      command_line.GetSwitchValueASCII(switches::kEnableCrashReporter);
  std::vector<std::string> parts = base::SplitString(switch_value,
                                                     ",",
                                                     base::KEEP_WHITESPACE,
                                                     base::SPLIT_WANT_ALL);
  if (parts.size() != 2)
    return false;

  *switch_parts = parts;
  return true;
}

#if !defined(OS_ANDROID)
void SetChannelFromCommandLine(const base::CommandLine& command_line) {
  std::vector<std::string> switch_parts;
  if (!GetEnableCrashReporterSwitchParts(command_line, &switch_parts))
    return;

  base::debug::SetCrashKeyValue(crash_keys::kChannel, switch_parts[1]);
}
#endif

void SetClientIdFromCommandLine(const base::CommandLine& command_line) {
  std::vector<std::string> switch_parts;
  if (!GetEnableCrashReporterSwitchParts(command_line, &switch_parts))
    return;

  GetCrashReporterClient()->SetCrashReporterClientIdFromGUID(switch_parts[0]);
}

// MIME substrings.
#if defined(OS_CHROMEOS)
const char g_sep[] = ":";
#endif
const char g_rn[] = "\r\n";
const char g_form_data_msg[] = "Content-Disposition: form-data; name=\"";
const char g_quote_msg[] = "\"";
const char g_dashdash_msg[] = "--";
const char g_dump_msg[] = "upload_file_minidump\"; filename=\"dump\"";
#if defined(ADDRESS_SANITIZER)
const char g_log_msg[] = "upload_file_log\"; filename=\"log\"";
#endif
const char g_content_type_msg[] = "Content-Type: application/octet-stream";

// MimeWriter manages an iovec for writing MIMEs to a file.
class MimeWriter {
 public:
  static const int kIovCapacity = 30;
  static const size_t kMaxCrashChunkSize = 64;

  MimeWriter(int fd, const char* const mime_boundary);
  ~MimeWriter();

  // Append boundary.
  virtual void AddBoundary();

  // Append end of file boundary.
  virtual void AddEnd();

  // Append key/value pair with specified sizes.
  virtual void AddPairData(const char* msg_type,
                           size_t msg_type_size,
                           const char* msg_data,
                           size_t msg_data_size);

  // Append key/value pair.
  void AddPairString(const char* msg_type,
                     const char* msg_data) {
    AddPairData(msg_type, my_strlen(msg_type), msg_data, my_strlen(msg_data));
  }

  // Append key/value pair, splitting value into chunks no larger than
  // |chunk_size|. |chunk_size| cannot be greater than |kMaxCrashChunkSize|.
  // The msg_type string will have a counter suffix to distinguish each chunk.
  virtual void AddPairDataInChunks(const char* msg_type,
                                   size_t msg_type_size,
                                   const char* msg_data,
                                   size_t msg_data_size,
                                   size_t chunk_size,
                                   bool strip_trailing_spaces);

  // Add binary file contents to be uploaded with the specified filename.
  virtual void AddFileContents(const char* filename_msg,
                               uint8_t* file_data,
                               size_t file_size);

  // Flush any pending iovecs to the output file.
  void Flush() {
    IGNORE_RET(sys_writev(fd_, iov_, iov_index_));
    iov_index_ = 0;
  }

 protected:
  void AddItem(const void* base, size_t size);
  // Minor performance trade-off for easier-to-maintain code.
  void AddString(const char* str) {
    AddItem(str, my_strlen(str));
  }
  void AddItemWithoutTrailingSpaces(const void* base, size_t size);

  struct kernel_iovec iov_[kIovCapacity];
  int iov_index_;

  // Output file descriptor.
  int fd_;

  const char* const mime_boundary_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MimeWriter);
};

MimeWriter::MimeWriter(int fd, const char* const mime_boundary)
    : iov_index_(0),
      fd_(fd),
      mime_boundary_(mime_boundary) {
}

MimeWriter::~MimeWriter() {
}

void MimeWriter::AddBoundary() {
  AddString(mime_boundary_);
  AddString(g_rn);
}

void MimeWriter::AddEnd() {
  AddString(mime_boundary_);
  AddString(g_dashdash_msg);
  AddString(g_rn);
}

void MimeWriter::AddPairData(const char* msg_type,
                             size_t msg_type_size,
                             const char* msg_data,
                             size_t msg_data_size) {
  AddString(g_form_data_msg);
  AddItem(msg_type, msg_type_size);
  AddString(g_quote_msg);
  AddString(g_rn);
  AddString(g_rn);
  AddItem(msg_data, msg_data_size);
  AddString(g_rn);
}

void MimeWriter::AddPairDataInChunks(const char* msg_type,
                                     size_t msg_type_size,
                                     const char* msg_data,
                                     size_t msg_data_size,
                                     size_t chunk_size,
                                     bool strip_trailing_spaces) {
  if (chunk_size > kMaxCrashChunkSize)
    return;

  unsigned i = 0;
  size_t done = 0, msg_length = msg_data_size;

  while (msg_length) {
    char num[kUint64StringSize];
    const unsigned num_len = my_uint_len(++i);
    my_uitos(num, i, num_len);

    size_t chunk_len = std::min(chunk_size, msg_length);

    AddString(g_form_data_msg);
    AddItem(msg_type, msg_type_size);
    AddItem(num, num_len);
    AddString(g_quote_msg);
    AddString(g_rn);
    AddString(g_rn);
    if (strip_trailing_spaces) {
      AddItemWithoutTrailingSpaces(msg_data + done, chunk_len);
    } else {
      AddItem(msg_data + done, chunk_len);
    }
    AddString(g_rn);
    AddBoundary();
    Flush();

    done += chunk_len;
    msg_length -= chunk_len;
  }
}

void MimeWriter::AddFileContents(const char* filename_msg, uint8_t* file_data,
                                 size_t file_size) {
  AddString(g_form_data_msg);
  AddString(filename_msg);
  AddString(g_rn);
  AddString(g_content_type_msg);
  AddString(g_rn);
  AddString(g_rn);
  AddItem(file_data, file_size);
  AddString(g_rn);
}

void MimeWriter::AddItem(const void* base, size_t size) {
  // Check if the iovec is full and needs to be flushed to output file.
  if (iov_index_ == kIovCapacity) {
    Flush();
  }
  iov_[iov_index_].iov_base = const_cast<void*>(base);
  iov_[iov_index_].iov_len = size;
  ++iov_index_;
}

void MimeWriter::AddItemWithoutTrailingSpaces(const void* base, size_t size) {
  AddItem(base, LengthWithoutTrailingSpaces(static_cast<const char*>(base),
                                            size));
}

#if defined(OS_CHROMEOS)
// This subclass is used on Chromium OS to report crashes in a format easy for
// the central crash reporting facility to understand.
// Format is <name>:<data length in decimal>:<data>
class CrashReporterWriter : public MimeWriter {
 public:
  explicit CrashReporterWriter(int fd);

  void AddBoundary() override;

  void AddEnd() override;

  void AddPairData(const char* msg_type,
                   size_t msg_type_size,
                   const char* msg_data,
                   size_t msg_data_size) override;

  void AddPairDataInChunks(const char* msg_type,
                           size_t msg_type_size,
                           const char* msg_data,
                           size_t msg_data_size,
                           size_t chunk_size,
                           bool strip_trailing_spaces) override;

  void AddFileContents(const char* filename_msg,
                       uint8_t* file_data,
                       size_t file_size) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CrashReporterWriter);
};


CrashReporterWriter::CrashReporterWriter(int fd) : MimeWriter(fd, "") {}

// No-ops.
void CrashReporterWriter::AddBoundary() {}
void CrashReporterWriter::AddEnd() {}

void CrashReporterWriter::AddPairData(const char* msg_type,
                                      size_t msg_type_size,
                                      const char* msg_data,
                                      size_t msg_data_size) {
  char data[kUint64StringSize];
  const unsigned data_len = my_uint_len(msg_data_size);
  my_uitos(data, msg_data_size, data_len);

  AddItem(msg_type, msg_type_size);
  AddString(g_sep);
  AddItem(data, data_len);
  AddString(g_sep);
  AddItem(msg_data, msg_data_size);
  Flush();
}

void CrashReporterWriter::AddPairDataInChunks(const char* msg_type,
                                              size_t msg_type_size,
                                              const char* msg_data,
                                              size_t msg_data_size,
                                              size_t chunk_size,
                                              bool strip_trailing_spaces) {
  if (chunk_size > kMaxCrashChunkSize)
    return;

  unsigned i = 0;
  size_t done = 0;
  size_t msg_length = msg_data_size;

  while (msg_length) {
    char num[kUint64StringSize];
    const unsigned num_len = my_uint_len(++i);
    my_uitos(num, i, num_len);

    size_t chunk_len = std::min(chunk_size, msg_length);

    size_t write_len = chunk_len;
    if (strip_trailing_spaces) {
      // Take care of this here because we need to know the exact length of
      // what is going to be written.
      write_len = LengthWithoutTrailingSpaces(msg_data + done, write_len);
    }

    char data[kUint64StringSize];
    const unsigned data_len = my_uint_len(write_len);
    my_uitos(data, write_len, data_len);

    AddItem(msg_type, msg_type_size);
    AddItem(num, num_len);
    AddString(g_sep);
    AddItem(data, data_len);
    AddString(g_sep);
    AddItem(msg_data + done, write_len);
    Flush();

    done += chunk_len;
    msg_length -= chunk_len;
  }
}

void CrashReporterWriter::AddFileContents(const char* filename_msg,
                                          uint8_t* file_data,
                                          size_t file_size) {
  char data[kUint64StringSize];
  const unsigned data_len = my_uint_len(file_size);
  my_uitos(data, file_size, data_len);

  AddString(filename_msg);
  AddString(g_sep);
  AddItem(data, data_len);
  AddString(g_sep);
  AddItem(file_data, file_size);
  Flush();
}
#endif  // defined(OS_CHROMEOS)

void DumpProcess() {
  if (g_breakpad)
    g_breakpad->WriteMinidump();

#if defined(OS_ANDROID)
  // If microdumps are enabled write also a microdump on the system log.
  if (g_microdump)
    g_microdump->WriteMinidump();
#endif
}

#if defined(OS_ANDROID)
const char kGoogleBreakpad[] = "google-breakpad";
#endif

size_t WriteLog(const char* buf, size_t nbytes) {
#if defined(OS_ANDROID)
  return __android_log_write(ANDROID_LOG_WARN, kGoogleBreakpad, buf);
#else
  return sys_write(2, buf, nbytes);
#endif
}

size_t WriteNewline() {
  return WriteLog("\n", 1);
}

#if defined(OS_ANDROID)
void AndroidLogWriteHorizontalRule() {
  __android_log_write(ANDROID_LOG_WARN, kGoogleBreakpad,
                      "### ### ### ### ### ### ### ### ### ### ### ### ###");
}

// Android's native crash handler outputs a diagnostic tombstone to the device
// log. By returning false from the HandlerCallbacks, breakpad will reinstall
// the previous (i.e. native) signal handlers before returning from its own
// handler. A Chrome build fingerprint is written to the log, so that the
// specific build of Chrome and the location of the archived Chrome symbols can
// be determined directly from it.
bool FinalizeCrashDoneAndroid(bool is_browser_process) {
  base::android::BuildInfo* android_build_info =
      base::android::BuildInfo::GetInstance();

  AndroidLogWriteHorizontalRule();
  __android_log_write(ANDROID_LOG_WARN, kGoogleBreakpad,
                      "Chrome build fingerprint:");
  __android_log_write(ANDROID_LOG_WARN, kGoogleBreakpad,
                      android_build_info->package_version_name());
  __android_log_write(ANDROID_LOG_WARN, kGoogleBreakpad,
                      android_build_info->package_version_code());
  AndroidLogWriteHorizontalRule();

  if (!is_browser_process &&
      android_build_info->sdk_int() >= 18 &&
      my_strcmp(android_build_info->build_type(), "eng") != 0 &&
      my_strcmp(android_build_info->build_type(), "userdebug") != 0) {
    // On JB MR2 and later, the system crash handler displays a dialog. For
    // renderer crashes, this is a bad user experience and so this is disabled
    // for user builds of Android.
    // TODO(cjhopman): There should be some way to recover the crash stack from
    // non-uploading user clients. See http://crbug.com/273706.
    __android_log_write(ANDROID_LOG_WARN,
                        kGoogleBreakpad,
                        "Tombstones are disabled on JB MR2+ user builds.");
    AndroidLogWriteHorizontalRule();
    return true;
  }
  return false;
}
#endif

bool CrashDone(const MinidumpDescriptor& minidump,
               const bool upload,
               const bool should_finalize,
               const bool succeeded) {
  // WARNING: this code runs in a compromised context. It may not call into
  // libc nor allocate memory normally.
  if (!succeeded) {
    const char msg[] = "Failed to generate minidump.";
    WriteLog(msg, sizeof(msg) - 1);
    return false;
  }

  DCHECK(!(upload && minidump.IsFD()));

  BreakpadInfo info = {0};
  info.filename = minidump.path();
  info.fd = minidump.fd();
#if defined(ADDRESS_SANITIZER)
  google_breakpad::PageAllocator allocator;
  const size_t log_path_len = my_strlen(minidump.path());
  char* log_path = reinterpret_cast<char*>(allocator.Alloc(log_path_len + 1));
  my_memcpy(log_path, minidump.path(), log_path_len);
  my_memcpy(log_path + log_path_len - 4, ".log", 4);
  log_path[log_path_len] = '\0';
  info.log_filename = log_path;
#endif
  info.process_type = "browser";
  info.process_type_length = 7;
  info.distro = base::g_linux_distro;
  info.distro_length = my_strlen(base::g_linux_distro);
  info.upload = upload;
  info.process_start_time = g_process_start_time;
  info.oom_size = base::g_oom_size;
  info.pid = g_pid;
  info.crash_keys = g_crash_keys;
  HandleCrashDump(info);
#if defined(OS_ANDROID)
  return !should_finalize ||
         FinalizeCrashDoneAndroid(true /* is_browser_process */);
#else
  return true;
#endif
}

#if defined(OS_ANDROID)
// Wrapper function, do not add more code here.
bool MinidumpGenerated(const MinidumpDescriptor& minidump,
                       void* context,
                       bool succeeded) {
  return CrashDone(minidump, false, false, succeeded);
}
#endif

// Wrapper function, do not add more code here.
bool CrashDoneNoUpload(const MinidumpDescriptor& minidump,
                       void* context,
                       bool succeeded) {
  return CrashDone(minidump, false, true, succeeded);
}

#if !defined(OS_ANDROID)
// Wrapper function, do not add more code here.
bool CrashDoneUpload(const MinidumpDescriptor& minidump,
                     void* context,
                     bool succeeded) {
  return CrashDone(minidump, true, true, succeeded);
}
#endif

#if defined(ADDRESS_SANITIZER)
extern "C"
void __asan_set_error_report_callback(void (*cb)(const char*));

extern "C"
void AsanLinuxBreakpadCallback(const char* report) {
  g_asan_report_str = report;
  // Send minidump here.
  g_breakpad->SimulateSignalDelivery(SIGKILL);
}
#endif

void EnableCrashDumping(bool unattended) {
  g_is_crash_reporter_enabled = true;

  base::FilePath tmp_path("/tmp");
  PathService::Get(base::DIR_TEMP, &tmp_path);

  base::FilePath dumps_path(tmp_path);
  if (GetCrashReporterClient()->GetCrashDumpLocation(&dumps_path)) {
    base::FilePath logfile =
        dumps_path.Append(GetCrashReporterClient()->GetReporterLogFilename());
    std::string logfile_str = logfile.value();
    const size_t crash_log_path_len = logfile_str.size() + 1;
    g_crash_log_path = new char[crash_log_path_len];
    strncpy(g_crash_log_path, logfile_str.c_str(), crash_log_path_len);
  }
  DCHECK(!g_breakpad);
  MinidumpDescriptor minidump_descriptor(dumps_path.value());
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kFullMemoryCrashReport)) {
    minidump_descriptor.set_size_limit(-1);  // unlimited.
  } else {
    minidump_descriptor.set_size_limit(kMaxMinidumpFileSize);
  }
#if defined(OS_ANDROID)
  unattended = true;  // Android never uploads directly.
#endif
  if (unattended) {
    g_breakpad = new ExceptionHandler(
        minidump_descriptor,
        nullptr,
        CrashDoneNoUpload,
        nullptr,
        true,  // Install handlers.
        -1);   // Server file descriptor. -1 for in-process.
    return;
  }

#if !defined(OS_ANDROID)
  // Attended mode
  g_breakpad = new ExceptionHandler(
      minidump_descriptor,
      nullptr,
      CrashDoneUpload,
      nullptr,
      true,  // Install handlers.
      -1);   // Server file descriptor. -1 for in-process.
#endif
}

#if defined(OS_ANDROID)
bool MicrodumpCrashDone(const MinidumpDescriptor& minidump,
                        void* context,
                        bool succeeded) {
  // WARNING: this code runs in a compromised context. It may not call into
  // libc nor allocate memory normally.
  if (!succeeded) {
    static const char msg[] = "Microdump crash handler failed.\n";
    WriteLog(msg, sizeof(msg) - 1);
    return false;
  }

  const bool is_browser_process = (context != nullptr);
  return FinalizeCrashDoneAndroid(is_browser_process);
}

bool WriteSignalCodeToPipe(const void* crash_context,
                           size_t crash_context_size,
                           void* context) {
  if (g_signal_code_pipe_fd == -1)
    return false;
  int signo = INT_MAX;
  if (crash_context_size == sizeof(ExceptionHandler::CrashContext)) {
    const ExceptionHandler::CrashContext* eh_context =
        static_cast<const ExceptionHandler::CrashContext*>(crash_context);
    signo = eh_context->siginfo.si_signo;
  }
  sys_write(g_signal_code_pipe_fd, &signo, sizeof(signo));
  IGNORE_RET(sys_close(g_signal_code_pipe_fd));
  g_signal_code_pipe_fd = -1;
  return false;
}

bool CrashDoneInProcessNoUpload(
    const google_breakpad::MinidumpDescriptor& descriptor,
    void* context,
    const bool succeeded) {
  // WARNING: this code runs in a compromised context. It may not call into
  // libc nor allocate memory normally.
  if (!succeeded) {
    static const char msg[] = "Crash dump generation failed.\n";
    WriteLog(msg, sizeof(msg) - 1);
    return false;
  }

  // Start constructing the message to send to the browser.
  BreakpadInfo info = {0};
  info.filename = nullptr;
  info.fd = descriptor.fd();
  info.process_type = g_process_type;
  info.process_type_length = my_strlen(g_process_type);
  info.distro = nullptr;
  info.distro_length = 0;
  info.upload = false;
  info.process_start_time = g_process_start_time;
  info.pid = g_pid;
  info.crash_keys = g_crash_keys;
  HandleCrashDump(info);
  return FinalizeCrashDoneAndroid(false /* is_browser_process */);
}

void EnableNonBrowserCrashDumping(const std::string& process_type,
                                  int minidump_fd) {
  // This will guarantee that the BuildInfo has been initialized and subsequent
  // calls will not require memory allocation.
  base::android::BuildInfo::GetInstance();
  SetClientIdFromCommandLine(*base::CommandLine::ForCurrentProcess());

  // On Android, the current sandboxing uses process isolation, in which the
  // child process runs with a different UID. That breaks the normal crash
  // reporting where the browser process generates the minidump by inspecting
  // the child process. This is because the browser process now does not have
  // the permission to access the states of the child process (as it has a
  // different UID).
  // TODO(jcivelli): http://b/issue?id=6776356 we should use a watchdog
  // process forked from the renderer process that generates the minidump.
  if (minidump_fd == -1) {
    LOG(ERROR) << "Minidump file descriptor not found, crash reporting will "
        " not work.";
    return;
  }
  SetProcessStartTime();
  g_pid = getpid();

  g_is_crash_reporter_enabled = true;
  // Save the process type (it is leaked).
  const size_t process_type_len = process_type.size() + 1;
  g_process_type = new char[process_type_len];
  strncpy(g_process_type, process_type.c_str(), process_type_len);
  new google_breakpad::ExceptionHandler(MinidumpDescriptor(minidump_fd),
      nullptr, CrashDoneInProcessNoUpload, nullptr, true, -1);
}

void GenerateMinidumpOnDemandForAndroid() {
  int dump_fd = GetCrashReporterClient()->GetAndroidMinidumpDescriptor();
  if (dump_fd >= 0) {
    MinidumpDescriptor minidump_descriptor(dump_fd);
    minidump_descriptor.set_size_limit(-1);
    ExceptionHandler(minidump_descriptor, nullptr, MinidumpGenerated, nullptr,
                     false, -1)
        .WriteMinidump();
  }
}

void MicrodumpInfo::SetGpuFingerprint(const std::string& gpu_fingerprint) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!microdump_gpu_fingerprint_);
  microdump_gpu_fingerprint_ = strdup(gpu_fingerprint.c_str());
  ANNOTATE_LEAKING_OBJECT_PTR(microdump_gpu_fingerprint_);

  if (g_microdump) {
    MinidumpDescriptor minidump_descriptor(g_microdump->minidump_descriptor());
    minidump_descriptor.microdump_extra_info()->gpu_fingerprint =
        microdump_gpu_fingerprint_;
    g_microdump->set_minidump_descriptor(minidump_descriptor);
  }
}

void MicrodumpInfo::Initialize(const std::string& process_type,
                               const char* product_name,
                               const char* product_version,
                               const char* android_build_fp) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!g_microdump);
  bool is_browser_process =
      process_type.empty() || process_type == kWebViewProcessType;

  MinidumpDescriptor descriptor(MinidumpDescriptor::kMicrodumpOnConsole);

  if (product_name && product_version) {
    microdump_product_info_ =
        strdup((product_name + std::string(":") + product_version).c_str());
    ANNOTATE_LEAKING_OBJECT_PTR(microdump_product_info_);
    descriptor.microdump_extra_info()->product_info = microdump_product_info_;
  }

  if (android_build_fp) {
    microdump_build_fingerprint_ = strdup(android_build_fp);
    ANNOTATE_LEAKING_OBJECT_PTR(microdump_build_fingerprint_);
    descriptor.microdump_extra_info()->build_fingerprint =
        microdump_build_fingerprint_;
  }

  if (microdump_gpu_fingerprint_) {
    descriptor.microdump_extra_info()->gpu_fingerprint =
        microdump_gpu_fingerprint_;
  }

  g_microdump =
      new ExceptionHandler(descriptor, nullptr, MicrodumpCrashDone,
                           reinterpret_cast<void*>(is_browser_process),
                           true,  // Install handlers.
                           -1);   // Server file descriptor. -1 for in-process.

  if (process_type == kWebViewProcessType) {
    // We do not use |DumpProcess()| for handling programatically
    // generated dumps for WebView because we only know the file
    // descriptor to which we are dumping at the time of the call to
    // |DumpWithoutCrashing()|. Therefore we need to construct the
    // |MinidumpDescriptor| and |ExceptionHandler| instances as
    // needed, instead of setting up |g_breakpad| at initialization
    // time.
    base::debug::SetDumpWithoutCrashingFunction(
        &GenerateMinidumpOnDemandForAndroid);
  } else if (!process_type.empty()) {
    g_signal_code_pipe_fd =
        GetCrashReporterClient()->GetAndroidMinidumpDescriptor();
    if (g_signal_code_pipe_fd != -1)
      g_microdump->set_crash_handler(WriteSignalCodeToPipe);
  }
}

#else
// Non-Browser = Extension, Gpu, Plugins, Ppapi and Renderer
class NonBrowserCrashHandler : public google_breakpad::CrashGenerationClient {
 public:
  NonBrowserCrashHandler()
      : server_fd_(base::GlobalDescriptors::GetInstance()->Get(
            kCrashDumpSignal)) {
  }

  ~NonBrowserCrashHandler() override {}

  bool RequestDump(const void* crash_context,
                   size_t crash_context_size) override {
    int fds[2] = { -1, -1 };
    if (sys_socketpair(AF_UNIX, SOCK_STREAM, 0, fds) < 0) {
      static const char msg[] = "Failed to create socket for crash dumping.\n";
      WriteLog(msg, sizeof(msg) - 1);
      return false;
    }

    // Start constructing the message to send to the browser.
    char b;  // Dummy variable for sys_read below.
    const char* b_addr = &b;  // Get the address of |b| so we can create the
                              // expected /proc/[pid]/syscall content in the
                              // browser to convert namespace tids.

    // The length of the control message:
    static const unsigned kControlMsgSize = sizeof(int);
    static const unsigned kControlMsgSpaceSize = CMSG_SPACE(kControlMsgSize);
    static const unsigned kControlMsgLenSize = CMSG_LEN(kControlMsgSize);

    struct kernel_msghdr msg;
    my_memset(&msg, 0, sizeof(struct kernel_msghdr));
    struct kernel_iovec iov[kCrashIovSize];
    iov[0].iov_base = const_cast<void*>(crash_context);
    iov[0].iov_len = crash_context_size;
    iov[1].iov_base = &b_addr;
    iov[1].iov_len = sizeof(b_addr);
    iov[2].iov_base = &fds[0];
    iov[2].iov_len = sizeof(fds[0]);
    iov[3].iov_base = &g_process_start_time;
    iov[3].iov_len = sizeof(g_process_start_time);
    iov[4].iov_base = &base::g_oom_size;
    iov[4].iov_len = sizeof(base::g_oom_size);
    google_breakpad::SerializedNonAllocatingMap* serialized_map;
    iov[5].iov_len = g_crash_keys->Serialize(
        const_cast<const google_breakpad::SerializedNonAllocatingMap**>(
            &serialized_map));
    iov[5].iov_base = serialized_map;
#if !defined(ADDRESS_SANITIZER)
    static_assert(5 == kCrashIovSize - 1, "kCrashIovSize should equal 6");
#else
    iov[6].iov_base = const_cast<char*>(g_asan_report_str);
    iov[6].iov_len = kMaxAsanReportSize + 1;
    static_assert(6 == kCrashIovSize - 1, "kCrashIovSize should equal 7");
#endif

    msg.msg_iov = iov;
    msg.msg_iovlen = kCrashIovSize;
    char cmsg[kControlMsgSpaceSize];
    my_memset(cmsg, 0, kControlMsgSpaceSize);
    msg.msg_control = cmsg;
    msg.msg_controllen = sizeof(cmsg);

    struct cmsghdr *hdr = CMSG_FIRSTHDR(&msg);
    hdr->cmsg_level = SOL_SOCKET;
    hdr->cmsg_type = SCM_RIGHTS;
    hdr->cmsg_len = kControlMsgLenSize;
    ((int*)CMSG_DATA(hdr))[0] = fds[1];

    if (HANDLE_EINTR(sys_sendmsg(server_fd_, &msg, 0)) < 0) {
      static const char errmsg[] = "Failed to tell parent about crash.\n";
      WriteLog(errmsg, sizeof(errmsg) - 1);
      IGNORE_RET(sys_close(fds[0]));
      IGNORE_RET(sys_close(fds[1]));
      return false;
    }
    IGNORE_RET(sys_close(fds[1]));

    if (HANDLE_EINTR(sys_read(fds[0], &b, 1)) != 1) {
      static const char errmsg[] = "Parent failed to complete crash dump.\n";
      WriteLog(errmsg, sizeof(errmsg) - 1);
    }
    IGNORE_RET(sys_close(fds[0]));

    return true;
  }

 private:
  // The pipe FD to the browser process, which will handle the crash dumping.
  const int server_fd_;

  DISALLOW_COPY_AND_ASSIGN(NonBrowserCrashHandler);
};

void EnableNonBrowserCrashDumping() {
  g_is_crash_reporter_enabled = true;
  // We deliberately leak this object.
  DCHECK(!g_breakpad);

  g_breakpad = new ExceptionHandler(
      MinidumpDescriptor("/tmp"),  // Unused but needed or Breakpad will assert.
      nullptr,
      nullptr,
      nullptr,
      true,
      -1);
  g_breakpad->set_crash_generation_client(new NonBrowserCrashHandler());
}
#endif  // defined(OS_ANDROID)

void SetCrashKeyValue(const base::StringPiece& key,
                      const base::StringPiece& value) {
  g_crash_keys->SetKeyValue(key.data(), value.data());
}

void ClearCrashKey(const base::StringPiece& key) {
  g_crash_keys->RemoveKey(key.data());
}

// GetCrashReporterClient() cannot call any Set methods until after
// InitCrashKeys().
void InitCrashKeys() {
  g_crash_keys = new CrashKeyStorage;
  GetCrashReporterClient()->RegisterCrashKeys();
  base::debug::SetCrashKeyReportingFunctions(&SetCrashKeyValue, &ClearCrashKey);
}

// Miscellaneous initialization functions to call after Breakpad has been
// enabled.
void PostEnableBreakpadInitialization() {
  SetProcessStartTime();
  g_pid = getpid();

  base::debug::SetDumpWithoutCrashingFunction(&DumpProcess);
#if defined(ADDRESS_SANITIZER)
  // Register the callback for AddressSanitizer error reporting.
  __asan_set_error_report_callback(AsanLinuxBreakpadCallback);
#endif
}

}  // namespace

void LoadDataFromFD(google_breakpad::PageAllocator& allocator,
                    int fd, bool close_fd, uint8_t** file_data, size_t* size) {
  STAT_STRUCT st;
  if (FSTAT_FUNC(fd, &st) != 0) {
    static const char msg[] = "Cannot upload crash dump: stat failed\n";
    WriteLog(msg, sizeof(msg) - 1);
    if (close_fd)
      IGNORE_RET(sys_close(fd));
    return;
  }

  *file_data = reinterpret_cast<uint8_t*>(allocator.Alloc(st.st_size));
  if (!(*file_data)) {
    static const char msg[] = "Cannot upload crash dump: cannot alloc\n";
    WriteLog(msg, sizeof(msg) - 1);
    if (close_fd)
      IGNORE_RET(sys_close(fd));
    return;
  }
  my_memset(*file_data, 0xf, st.st_size);

  *size = st.st_size;
  int byte_read = sys_read(fd, *file_data, *size);
  if (byte_read == -1) {
    static const char msg[] = "Cannot upload crash dump: read failed\n";
    WriteLog(msg, sizeof(msg) - 1);
    if (close_fd)
      IGNORE_RET(sys_close(fd));
    return;
  }

  if (close_fd)
    IGNORE_RET(sys_close(fd));
}

void LoadDataFromFile(google_breakpad::PageAllocator& allocator,
                      const char* filename,
                      int* fd, uint8_t** file_data, size_t* size) {
  // WARNING: this code runs in a compromised context. It may not call into
  // libc nor allocate memory normally.
  *fd = sys_open(filename, O_RDONLY, 0);
  *size = 0;

  if (*fd < 0) {
    static const char msg[] = "Cannot upload crash dump: failed to open\n";
    WriteLog(msg, sizeof(msg) - 1);
    return;
  }

  LoadDataFromFD(allocator, *fd, true, file_data, size);
}

// Spawn the appropriate upload process for the current OS:
// - generic Linux invokes wget.
// - ChromeOS invokes crash_reporter.
// |dumpfile| is the path to the dump data file.
// |mime_boundary| is only used on Linux.
// |exe_buf| is only used on CrOS and is the crashing process' name.
void ExecUploadProcessOrTerminate(const BreakpadInfo& info,
                                  const char* dumpfile,
                                  const char* mime_boundary,
                                  const char* exe_buf,
                                  google_breakpad::PageAllocator* allocator) {
#if defined(OS_CHROMEOS)
  // CrOS uses crash_reporter instead of wget to report crashes,
  // it needs to know where the crash dump lives and the pid and uid of the
  // crashing process.
  static const char kCrashReporterBinary[] = "/sbin/crash_reporter";

  char pid_buf[kUint64StringSize];
  uint64_t pid_str_length = my_uint64_len(info.pid);
  my_uint64tos(pid_buf, info.pid, pid_str_length);
  pid_buf[pid_str_length] = '\0';

  char uid_buf[kUint64StringSize];
  uid_t uid = geteuid();
  uint64_t uid_str_length = my_uint64_len(uid);
  my_uint64tos(uid_buf, uid, uid_str_length);
  uid_buf[uid_str_length] = '\0';

  const char kChromeFlag[] = "--chrome=";
  size_t buf_len = my_strlen(dumpfile) + sizeof(kChromeFlag);
  char* chrome_flag = reinterpret_cast<char*>(allocator->Alloc(buf_len));
  chrome_flag[0] = '\0';
  my_strlcat(chrome_flag, kChromeFlag, buf_len);
  my_strlcat(chrome_flag, dumpfile, buf_len);

  const char kPidFlag[] = "--pid=";
  buf_len = my_strlen(pid_buf) + sizeof(kPidFlag);
  char* pid_flag = reinterpret_cast<char*>(allocator->Alloc(buf_len));
  pid_flag[0] = '\0';
  my_strlcat(pid_flag, kPidFlag, buf_len);
  my_strlcat(pid_flag, pid_buf, buf_len);

  const char kUidFlag[] = "--uid=";
  buf_len = my_strlen(uid_buf) + sizeof(kUidFlag);
  char* uid_flag = reinterpret_cast<char*>(allocator->Alloc(buf_len));
  uid_flag[0] = '\0';
  my_strlcat(uid_flag, kUidFlag, buf_len);
  my_strlcat(uid_flag, uid_buf, buf_len);

  const char kExeBuf[] = "--exe=";
  buf_len = my_strlen(exe_buf) + sizeof(kExeBuf);
  char* exe_flag = reinterpret_cast<char*>(allocator->Alloc(buf_len));
  exe_flag[0] = '\0';
  my_strlcat(exe_flag, kExeBuf, buf_len);
  my_strlcat(exe_flag, exe_buf, buf_len);

  const char* args[] = {
    kCrashReporterBinary,
    chrome_flag,
    pid_flag,
    uid_flag,
    exe_flag,
    nullptr,
  };
  static const char msg[] = "Cannot upload crash dump: cannot exec "
                            "/sbin/crash_reporter\n";
#else
  // Compress |dumpfile| with gzip.
  const pid_t gzip_child = sys_fork();
  if (gzip_child < 0) {
    static const char msg[] = "sys_fork() for gzip process failed.\n";
    WriteLog(msg, sizeof(msg) - 1);
    sys__exit(1);
  }
  if (!gzip_child) {
    // gzip process.
    const char* args[] = {
      "/bin/gzip",
      "-f",  // Do not prompt to verify before overwriting.
      dumpfile,
      nullptr,
    };
    execve(args[0], const_cast<char**>(args), environ);
    static const char msg[] = "Cannot exec gzip.\n";
    WriteLog(msg, sizeof(msg) - 1);
    sys__exit(1);
  }
  // Wait for gzip process.
  int status = 0;
  if (sys_waitpid(gzip_child, &status, 0) != gzip_child ||
      !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    static const char msg[] = "sys_waitpid() for gzip process failed.\n";
    WriteLog(msg, sizeof(msg) - 1);
    sys_kill(gzip_child, SIGKILL);
    sys__exit(1);
  }

  static const char kGzipExtension[] = ".gz";
  const size_t gzip_file_size = my_strlen(dumpfile) + sizeof(kGzipExtension);
  char* const gzip_file = reinterpret_cast<char*>(allocator->Alloc(
      gzip_file_size));
  my_strlcpy(gzip_file, dumpfile, gzip_file_size);
  my_strlcat(gzip_file, kGzipExtension, gzip_file_size);

  // Rename |gzip_file| to |dumpfile| (the original file was deleted by gzip).
  if (rename(gzip_file, dumpfile)) {
    static const char msg[] = "Failed to rename gzipped file.\n";
    WriteLog(msg, sizeof(msg) - 1);
    sys__exit(1);
  }

  // The --header argument to wget looks like:
  //   --header=Content-Encoding: gzip
  //   --header=Content-Type: multipart/form-data; boundary=XYZ
  // where the boundary has two fewer leading '-' chars
  static const char header_content_encoding[] =
      "--header=Content-Encoding: gzip";
  static const char header_msg[] =
      "--header=Content-Type: multipart/form-data; boundary=";
  const size_t header_content_type_size =
      sizeof(header_msg) - 1 + my_strlen(mime_boundary) - 2 + 1;
  char* const header_content_type = reinterpret_cast<char*>(allocator->Alloc(
      header_content_type_size));
  my_strlcpy(header_content_type, header_msg, header_content_type_size);
  my_strlcat(header_content_type, mime_boundary + 2, header_content_type_size);

  // The --post-file argument to wget looks like:
  //   --post-file=/tmp/...
  static const char post_file_msg[] = "--post-file=";
  const size_t post_file_size =
      sizeof(post_file_msg) - 1 + my_strlen(dumpfile) + 1;
  char* const post_file = reinterpret_cast<char*>(allocator->Alloc(
      post_file_size));
  my_strlcpy(post_file, post_file_msg, post_file_size);
  my_strlcat(post_file, dumpfile, post_file_size);

  static const char kWgetBinary[] = "/usr/bin/wget";
  const char* args[] = {
    kWgetBinary,
    header_content_encoding,
    header_content_type,
    post_file,
    kUploadURL,
    "--timeout=10",  // Set a timeout so we don't hang forever.
    "--tries=1",     // Don't retry if the upload fails.
    "-O",  // output reply to fd 3
    "/dev/fd/3",
    nullptr,
  };
  static const char msg[] = "Cannot upload crash dump: cannot exec "
                            "/usr/bin/wget\n";
#endif
  execve(args[0], const_cast<char**>(args), environ);
  WriteLog(msg, sizeof(msg) - 1);
  sys__exit(1);
}

// Runs in the helper process to wait for the upload process running
// ExecUploadProcessOrTerminate() to finish. Returns the number of bytes written
// to |fd| and save the written contents to |buf|.
// |buf| needs to be big enough to hold |bytes_to_read| + 1 characters.
size_t WaitForCrashReportUploadProcess(int fd, size_t bytes_to_read,
                                       char* buf) {
  size_t bytes_read = 0;

  // Upload should finish in about 10 seconds. Add a few more 500 ms
  // internals to account for process startup time.
  for (size_t wait_count = 0; wait_count < 24; ++wait_count) {
    struct kernel_pollfd poll_fd;
    poll_fd.fd = fd;
    poll_fd.events = POLLIN | POLLPRI | POLLERR;
    int ret = sys_poll(&poll_fd, 1, 500);
    if (ret < 0) {
      // Error
      break;
    } else if (ret > 0) {
      // There is data to read.
      ssize_t len = HANDLE_EINTR(
          sys_read(fd, buf + bytes_read, bytes_to_read - bytes_read));
      if (len < 0)
        break;
      bytes_read += len;
      if (bytes_read == bytes_to_read)
        break;
    }
    // |ret| == 0 -> timed out, continue waiting.
    // or |bytes_read| < |bytes_to_read| still, keep reading.
  }
  buf[bytes_to_read] = 0;  // Always NUL terminate the buffer.
  return bytes_read;
}

// |buf| should be |expected_len| + 1 characters in size and nullptr terminated.
bool IsValidCrashReportId(const char* buf, size_t bytes_read,
                          size_t expected_len) {
  if (bytes_read != expected_len)
    return false;
#if defined(OS_CHROMEOS)
  return my_strcmp(buf, "_sys_cr_finished") == 0;
#else
  for (size_t i = 0; i < bytes_read; ++i) {
    if (!my_isxdigit(buf[i]))
      return false;
  }
  return true;
#endif
}

// |buf| should be |expected_len| + 1 characters in size and nullptr terminated.
void HandleCrashReportId(const char* buf, size_t bytes_read,
                         size_t expected_len) {
  WriteNewline();
  if (!IsValidCrashReportId(buf, bytes_read, expected_len)) {
#if defined(OS_CHROMEOS)
    static const char msg[] =
        "System crash-reporter failed to process crash report.";
#else
    static const char msg[] = "Failed to get crash dump id.";
#endif
    WriteLog(msg, sizeof(msg) - 1);
    WriteNewline();

    static const char id_msg[] = "Report Id: ";
    WriteLog(id_msg, sizeof(id_msg) - 1);
    WriteLog(buf, bytes_read);
    WriteNewline();
    return;
  }

#if defined(OS_CHROMEOS)
  static const char msg[] = "Crash dump received by crash_reporter\n";
  WriteLog(msg, sizeof(msg) - 1);
#else
  // Write crash dump id to stderr.
  static const char msg[] = "Crash dump id: ";
  WriteLog(msg, sizeof(msg) - 1);
  WriteLog(buf, my_strlen(buf));
  WriteNewline();

  // Write crash dump id to crash log as: seconds_since_epoch,crash_id
  struct kernel_timeval tv;
  if (g_crash_log_path && !sys_gettimeofday(&tv, nullptr)) {
    uint64_t time = kernel_timeval_to_ms(&tv) / 1000;
    char time_str[kUint64StringSize];
    const unsigned time_len = my_uint64_len(time);
    my_uint64tos(time_str, time, time_len);

    const int kLogOpenFlags = O_CREAT | O_WRONLY | O_APPEND | O_CLOEXEC;
    int log_fd = sys_open(g_crash_log_path, kLogOpenFlags, 0600);
    if (log_fd > 0) {
      sys_write(log_fd, time_str, time_len);
      sys_write(log_fd, ",", 1);
      sys_write(log_fd, buf, my_strlen(buf));
      sys_write(log_fd, "\n", 1);
      IGNORE_RET(sys_close(log_fd));
    }
  }
#endif
}

#if defined(OS_CHROMEOS)
const char* GetCrashingProcessName(const BreakpadInfo& info,
                                   google_breakpad::PageAllocator* allocator) {
  // Symlink to process binary is at /proc/###/exe.
  char linkpath[kUint64StringSize + sizeof("/proc/") + sizeof("/exe")] =
    "/proc/";
  uint64_t pid_value_len = my_uint64_len(info.pid);
  my_uint64tos(linkpath + sizeof("/proc/") - 1, info.pid, pid_value_len);
  linkpath[sizeof("/proc/") - 1 + pid_value_len] = '\0';
  my_strlcat(linkpath, "/exe", sizeof(linkpath));

  const int kMaxSize = 4096;
  char* link = reinterpret_cast<char*>(allocator->Alloc(kMaxSize));
  if (link) {
    ssize_t size = readlink(linkpath, link, kMaxSize);
    if (size < kMaxSize && size > 0) {
      // readlink(2) doesn't add a terminating NUL, so do it now.
      link[size] = '\0';

      const char* name = my_strrchr(link, '/');
      if (name)
        return name + 1;
      return link;
    }
  }
  // Either way too long, or a read error.
  return "chrome-crash-unknown-process";
}
#endif

void HandleCrashDump(const BreakpadInfo& info) {
  int dumpfd;
  bool keep_fd = false;
  size_t dump_size;
  uint8_t* dump_data;
  google_breakpad::PageAllocator allocator;
  const char* exe_buf = nullptr;

  if (GetCrashReporterClient()->HandleCrashDump(info.filename)) {
    return;
  }

#if defined(OS_CHROMEOS)
  // Grab the crashing process' name now, when it should still be available.
  // If we try to do this later in our grandchild the crashing process has
  // already terminated.
  exe_buf = GetCrashingProcessName(info, &allocator);
#endif

  if (info.fd != -1) {
    // Dump is provided with an open FD.
    keep_fd = true;
    dumpfd = info.fd;

    // The FD is pointing to the end of the file.
    // Rewind, we'll read the data next.
    if (lseek(dumpfd, 0, SEEK_SET) == -1) {
      static const char msg[] = "Cannot upload crash dump: failed to "
          "reposition minidump FD\n";
      WriteLog(msg, sizeof(msg) - 1);
      IGNORE_RET(sys_close(dumpfd));
      return;
    }
    LoadDataFromFD(allocator, info.fd, false, &dump_data, &dump_size);
  } else {
    // Dump is provided with a path.
    keep_fd = false;
    LoadDataFromFile(allocator, info.filename, &dumpfd, &dump_data, &dump_size);
  }

  // TODO(jcivelli): make log work when using FDs.
#if defined(ADDRESS_SANITIZER)
  int logfd;
  size_t log_size;
  uint8_t* log_data;
  // Load the AddressSanitizer log into log_data.
  LoadDataFromFile(allocator, info.log_filename, &logfd, &log_data, &log_size);
#endif

  // We need to build a MIME block for uploading to the server. Since we are
  // going to fork and run wget, it needs to be written to a temp file.
  const int ufd = sys_open("/dev/urandom", O_RDONLY, 0);
  if (ufd < 0) {
    static const char msg[] = "Cannot upload crash dump because /dev/urandom"
                              " is missing\n";
    WriteLog(msg, sizeof(msg) - 1);
    return;
  }

  static const char temp_file_template[] =
      "/tmp/chromium-upload-XXXXXXXXXXXXXXXX";
  char temp_file[sizeof(temp_file_template)];
  int temp_file_fd = -1;
  if (keep_fd) {
    temp_file_fd = dumpfd;
    // Rewind the destination, we are going to overwrite it.
    if (lseek(dumpfd, 0, SEEK_SET) == -1) {
      static const char msg[] = "Cannot upload crash dump: failed to "
          "reposition minidump FD (2)\n";
      WriteLog(msg, sizeof(msg) - 1);
      IGNORE_RET(sys_close(dumpfd));
      return;
    }
  } else {
    if (info.upload) {
      my_memcpy(temp_file, temp_file_template, sizeof(temp_file_template));

      for (unsigned i = 0; i < 10; ++i) {
        uint64_t t;
        sys_read(ufd, &t, sizeof(t));
        write_uint64_hex(temp_file + sizeof(temp_file) - (16 + 1), t);

        temp_file_fd = sys_open(temp_file, O_WRONLY | O_CREAT | O_EXCL, 0600);
        if (temp_file_fd >= 0)
          break;
      }

      if (temp_file_fd < 0) {
        static const char msg[] = "Failed to create temporary file in /tmp: "
            "cannot upload crash dump\n";
        WriteLog(msg, sizeof(msg) - 1);
        IGNORE_RET(sys_close(ufd));
        return;
      }
    } else {
      temp_file_fd = sys_open(info.filename, O_WRONLY, 0600);
      if (temp_file_fd < 0) {
        static const char msg[] = "Failed to save crash dump: failed to open\n";
        WriteLog(msg, sizeof(msg) - 1);
        IGNORE_RET(sys_close(ufd));
        return;
      }
    }
  }

  // The MIME boundary is 28 hyphens, followed by a 64-bit nonce and a NUL.
  char mime_boundary[28 + 16 + 1];
  my_memset(mime_boundary, '-', 28);
  uint64_t boundary_rand;
  sys_read(ufd, &boundary_rand, sizeof(boundary_rand));
  write_uint64_hex(mime_boundary + 28, boundary_rand);
  mime_boundary[28 + 16] = 0;
  IGNORE_RET(sys_close(ufd));

  // The MIME block looks like this:
  //   BOUNDARY \r\n
  //   Content-Disposition: form-data; name="prod" \r\n \r\n
  //   Chrome_Linux \r\n
  //   BOUNDARY \r\n
  //   Content-Disposition: form-data; name="ver" \r\n \r\n
  //   1.2.3.4 \r\n
  //   BOUNDARY \r\n
  //
  //   zero or one:
  //   Content-Disposition: form-data; name="ptime" \r\n \r\n
  //   abcdef \r\n
  //   BOUNDARY \r\n
  //
  //   zero or one:
  //   Content-Disposition: form-data; name="ptype" \r\n \r\n
  //   abcdef \r\n
  //   BOUNDARY \r\n
  //
  //   zero or one:
  //   Content-Disposition: form-data; name="lsb-release" \r\n \r\n
  //   abcdef \r\n
  //   BOUNDARY \r\n
  //
  //   zero or one:
  //   Content-Disposition: form-data; name="oom-size" \r\n \r\n
  //   1234567890 \r\n
  //   BOUNDARY \r\n
  //
  //   zero or more (up to CrashKeyStorage::num_entries = 64):
  //   Content-Disposition: form-data; name=crash-key-name \r\n
  //   crash-key-value \r\n
  //   BOUNDARY \r\n
  //
  //   Content-Disposition: form-data; name="dump"; filename="dump" \r\n
  //   Content-Type: application/octet-stream \r\n \r\n
  //   <dump contents>
  //   \r\n BOUNDARY -- \r\n

#if defined(OS_CHROMEOS)
  CrashReporterWriter writer(temp_file_fd);
#else
  MimeWriter writer(temp_file_fd, mime_boundary);
#endif
  {
    const char* product_name = "";
    const char* version = "";

    GetCrashReporterClient()->GetProductNameAndVersion(&product_name, &version);

    writer.AddBoundary();
    writer.AddPairString("prod", product_name);
    writer.AddBoundary();
    writer.AddPairString("ver", version);
    writer.AddBoundary();
    if (info.pid > 0) {
      char pid_value_buf[kUint64StringSize];
      uint64_t pid_value_len = my_uint64_len(info.pid);
      my_uint64tos(pid_value_buf, info.pid, pid_value_len);
      static const char pid_key_name[] = "pid";
      writer.AddPairData(pid_key_name, sizeof(pid_key_name) - 1,
                         pid_value_buf, pid_value_len);
      writer.AddBoundary();
    }
#if defined(OS_ANDROID)
    // Addtional MIME blocks are added for logging on Android devices.
    static const char android_build_id[] = "android_build_id";
    static const char android_build_fp[] = "android_build_fp";
    static const char device[] = "device";
    static const char gms_core_version[] = "gms_core_version";
    static const char model[] = "model";
    static const char brand[] = "brand";
    static const char exception_info[] = "exception_info";

    base::android::BuildInfo* android_build_info =
        base::android::BuildInfo::GetInstance();
    writer.AddPairString(
        android_build_id, android_build_info->android_build_id());
    writer.AddBoundary();
    writer.AddPairString(
        android_build_fp, android_build_info->android_build_fp());
    writer.AddBoundary();
    writer.AddPairString(device, android_build_info->device());
    writer.AddBoundary();
    writer.AddPairString(model, android_build_info->model());
    writer.AddBoundary();
    writer.AddPairString(brand, android_build_info->brand());
    writer.AddBoundary();
    writer.AddPairString(gms_core_version,
        android_build_info->gms_version_code());
    writer.AddBoundary();
    if (android_build_info->java_exception_info() != nullptr) {
      writer.AddPairString(exception_info,
                           android_build_info->java_exception_info());
      writer.AddBoundary();
    }
#endif
    writer.Flush();
  }

  if (info.process_start_time > 0) {
    struct kernel_timeval tv;
    if (!sys_gettimeofday(&tv, nullptr)) {
      uint64_t time = kernel_timeval_to_ms(&tv);
      if (time > info.process_start_time) {
        time -= info.process_start_time;
        char time_str[kUint64StringSize];
        const unsigned time_len = my_uint64_len(time);
        my_uint64tos(time_str, time, time_len);

        static const char process_time_msg[] = "ptime";
        writer.AddPairData(process_time_msg, sizeof(process_time_msg) - 1,
                           time_str, time_len);
        writer.AddBoundary();
        writer.Flush();
      }
    }
  }

  if (info.process_type_length) {
    writer.AddPairString("ptype", info.process_type);
    writer.AddBoundary();
    writer.Flush();
  }

  if (info.distro_length) {
    static const char distro_msg[] = "lsb-release";
    writer.AddPairString(distro_msg, info.distro);
    writer.AddBoundary();
    writer.Flush();
  }

  if (info.oom_size) {
    char oom_size_str[kUint64StringSize];
    const unsigned oom_size_len = my_uint64_len(info.oom_size);
    my_uint64tos(oom_size_str, info.oom_size, oom_size_len);
    static const char oom_size_msg[] = "oom-size";
    writer.AddPairData(oom_size_msg, sizeof(oom_size_msg) - 1,
                       oom_size_str, oom_size_len);
    writer.AddBoundary();
    writer.Flush();
  }

  if (info.crash_keys) {
    CrashKeyStorage::Iterator crash_key_iterator(*info.crash_keys);
    const CrashKeyStorage::Entry* entry;
    while ((entry = crash_key_iterator.Next())) {
      writer.AddPairString(entry->key, entry->value);
      writer.AddBoundary();
      writer.Flush();
    }
  }

  writer.AddFileContents(g_dump_msg, dump_data, dump_size);
#if defined(ADDRESS_SANITIZER)
  // Append a multipart boundary and the contents of the AddressSanitizer log.
  writer.AddBoundary();
  writer.AddFileContents(g_log_msg, log_data, log_size);
#endif
  writer.AddEnd();
  writer.Flush();

  IGNORE_RET(sys_close(temp_file_fd));

#if defined(OS_ANDROID)
  if (info.filename) {
    size_t filename_length = my_strlen(info.filename);

    // If this was a file, we need to copy it to the right place and use the
    // right file name so it gets uploaded by the browser.
    const char msg[] = "Output crash dump file:";
    WriteLog(msg, sizeof(msg) - 1);
    WriteLog(info.filename, filename_length);

    char pid_buf[kUint64StringSize];
    size_t pid_str_length = my_uint64_len(info.pid);
    my_uint64tos(pid_buf, info.pid, pid_str_length);
    pid_buf[pid_str_length] = 0;  // my_uint64tos() doesn't null-terminate.

    size_t done_filename_len = filename_length + pid_str_length + 1;
    char* done_filename = reinterpret_cast<char*>(
        allocator.Alloc(done_filename_len));
    // Rename the file such that the pid is the suffix in order signal to other
    // processes that the minidump is complete. The advantage of using the pid
    // as the suffix is that it is trivial to associate the minidump with the
    // crashed process.
    my_strlcpy(done_filename, info.filename, done_filename_len);
    my_strlcat(done_filename, pid_buf, done_filename_len);
    // Rename the minidump file to signal that it is complete.
    if (rename(info.filename, done_filename)) {
      const char failed_msg[] = "Failed to rename:";
      WriteLog(failed_msg, sizeof(failed_msg) - 1);
      WriteLog(info.filename, filename_length);
      const char to_msg[] = "to";
      WriteLog(to_msg, sizeof(to_msg) - 1);
      WriteLog(done_filename, done_filename_len - 1);
    }
  }
#endif

  if (!info.upload)
    return;

  const pid_t child = sys_fork();
  if (!child) {
    // Spawned helper process.
    //
    // This code is called both when a browser is crashing (in which case,
    // nothing really matters any more) and when a renderer/plugin crashes, in
    // which case we need to continue.
    //
    // Since we are a multithreaded app, if we were just to fork(), we might
    // grab file descriptors which have just been created in another thread and
    // hold them open for too long.
    //
    // Thus, we have to loop and try and close everything.
    const int fd = sys_open("/proc/self/fd", O_DIRECTORY | O_RDONLY, 0);
    if (fd < 0) {
      for (unsigned i = 3; i < 8192; ++i)
        IGNORE_RET(sys_close(i));
    } else {
      google_breakpad::DirectoryReader reader(fd);
      const char* name;
      while (reader.GetNextEntry(&name)) {
        int i;
        if (my_strtoui(&i, name) && i > 2 && i != fd)
          IGNORE_RET(sys_close(i));
        reader.PopEntry();
      }

      IGNORE_RET(sys_close(fd));
    }

    IGNORE_RET(sys_setsid());

    // Leave one end of a pipe in the upload process and watch for it getting
    // closed by the upload process exiting.
    int fds[2];
    if (sys_pipe(fds) >= 0) {
      const pid_t upload_child = sys_fork();
      if (!upload_child) {
        // Upload process.
        IGNORE_RET(sys_close(fds[0]));
        IGNORE_RET(sys_dup2(fds[1], 3));
        ExecUploadProcessOrTerminate(info, temp_file, mime_boundary, exe_buf,
                                     &allocator);
      }

      // Helper process.
      if (upload_child > 0) {
        IGNORE_RET(sys_close(fds[1]));

        const size_t kCrashIdLength = 16;
        char id_buf[kCrashIdLength + 1];
        size_t bytes_read =
            WaitForCrashReportUploadProcess(fds[0], kCrashIdLength, id_buf);
        HandleCrashReportId(id_buf, bytes_read, kCrashIdLength);

        if (sys_waitpid(upload_child, nullptr, WNOHANG) == 0) {
          // Upload process is still around, kill it.
          sys_kill(upload_child, SIGKILL);
        }
      }
    }

    // Helper process.
    IGNORE_RET(sys_unlink(info.filename));
#if defined(ADDRESS_SANITIZER)
    IGNORE_RET(sys_unlink(info.log_filename));
#endif
    IGNORE_RET(sys_unlink(temp_file));
    sys__exit(0);
  }

  // Main browser process.
  if (child <= 0)
    return;
  (void) HANDLE_EINTR(sys_waitpid(child, nullptr, 0));
}

void InitCrashReporter(const std::string& process_type) {
  // The maximum lengths specified by breakpad include the trailing NULL, so the
  // actual length of the chunk is one less.
  static_assert(crash_keys::kChunkMaxLength == 63, "kChunkMaxLength mismatch");
  static_assert(crash_keys::kSmallSize <= crash_keys::kChunkMaxLength,
                "crash key chunk size too small");
#if defined(OS_ANDROID)
  // This will guarantee that the BuildInfo has been initialized and subsequent
  // calls will not require memory allocation.
  base::android::BuildInfo::GetInstance();

  // Handler registration is LIFO. Install the microdump handler first, such
  // that if conventional minidump crash reporting is enabled below, it takes
  // precedence (i.e. its handler is run first) over the microdump handler.
  InitMicrodumpCrashHandlerIfNecessary(process_type);
#endif
  // Determine the process type and take appropriate action.
  const base::CommandLine& parsed_command_line =
      *base::CommandLine::ForCurrentProcess();
  if (parsed_command_line.HasSwitch(switches::kDisableBreakpad))
    return;

  if (process_type.empty()) {
    bool enable_breakpad = GetCrashReporterClient()->GetCollectStatsConsent() ||
                           GetCrashReporterClient()->IsRunningUnattended();
    enable_breakpad &=
        !parsed_command_line.HasSwitch(switches::kDisableBreakpad);
    if (!enable_breakpad) {
      enable_breakpad = parsed_command_line.HasSwitch(
          switches::kEnableCrashReporterForTesting);
    }
    if (!enable_breakpad) {
      VLOG(1) << "Breakpad disabled";
      return;
    }

    InitCrashKeys();
    EnableCrashDumping(GetCrashReporterClient()->IsRunningUnattended());
  } else if (GetCrashReporterClient()->EnableBreakpadForProcess(process_type)) {
#if defined(OS_ANDROID)
    NOTREACHED() << "Breakpad initialized with InitCrashReporter() instead of "
      "InitNonBrowserCrashReporter in " << process_type << " process.";
    return;
#else
    // We might be chrooted in a zygote or renderer process so we cannot call
    // GetCollectStatsConsent because that needs access the the user's home
    // dir. Instead, we set a command line flag for these processes.
    // Even though plugins are not chrooted, we share the same code path for
    // simplicity.
    if (!parsed_command_line.HasSwitch(switches::kEnableCrashReporter))
      return;

    InitCrashKeys();
    SetChannelFromCommandLine(parsed_command_line);
    SetClientIdFromCommandLine(parsed_command_line);
    EnableNonBrowserCrashDumping();
    VLOG(1) << "Non Browser crash dumping enabled for: " << process_type;
#endif  // #if defined(OS_ANDROID)
  }

  PostEnableBreakpadInitialization();
}

#if defined(OS_ANDROID)
void InitNonBrowserCrashReporterForAndroid(const std::string& process_type) {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  // Handler registration is LIFO. Install the microdump handler first, such
  // that if conventional minidump crash reporting is enabled below, it takes
  // precedence (i.e. its handler is run first) over the microdump handler.
  InitMicrodumpCrashHandlerIfNecessary(process_type);

  if (command_line->HasSwitch(switches::kEnableCrashReporter)) {
    // On Android we need to provide a FD to the file where the minidump is
    // generated as the renderer and browser run with different UIDs
    // (preventing the browser from inspecting the renderer process).
    int minidump_fd = base::GlobalDescriptors::GetInstance()->MaybeGet(
        GetCrashReporterClient()->GetAndroidMinidumpDescriptor());
    if (minidump_fd < 0) {
      NOTREACHED() << "Could not find minidump FD, crash reporting disabled.";
    } else {
      InitCrashKeys();
      EnableNonBrowserCrashDumping(process_type, minidump_fd);
    }
  }
}

// The microdump handler does NOT upload anything. It just dumps out on the
// system console (logcat) a restricted and serialized variant of a minidump.
// See crbug.com/410294 for more details.
void InitMicrodumpCrashHandlerIfNecessary(const std::string& process_type) {
  if (!GetCrashReporterClient()->ShouldEnableBreakpadMicrodumps())
    return;

  VLOG(1) << "Enabling microdumps crash handler (process_type:"
          << process_type << ")";

  // The exception handler runs in a compromised context and cannot use c_str()
  // as that would require the heap. Therefore, we have to guarantee that the
  // build fingerprint and product info pointers are always valid.
  const char* product_name = nullptr;
  const char* product_version = nullptr;
  GetCrashReporterClient()->GetProductNameAndVersion(&product_name,
                                                     &product_version);

  const char* android_build_fp =
      base::android::BuildInfo::GetInstance()->android_build_fp();

  g_microdump_info.Get().Initialize(process_type, product_name, product_version,
                                    android_build_fp);
}

void AddGpuFingerprintToMicrodumpCrashHandler(
    const std::string& gpu_fingerprint) {
  g_microdump_info.Get().SetGpuFingerprint(gpu_fingerprint);
}
#endif  // OS_ANDROID

bool IsCrashReporterEnabled() {
  return g_is_crash_reporter_enabled;
}

}  // namespace breakpad
