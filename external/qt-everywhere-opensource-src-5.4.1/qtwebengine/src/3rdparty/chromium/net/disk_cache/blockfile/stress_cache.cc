// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a simple application that stress-tests the crash recovery of the disk
// cache. The main application starts a copy of itself on a loop, checking the
// exit code of the child process. When the child dies in an unexpected way,
// the main application quits.

// The child application has two threads: one to exercise the cache in an
// infinite loop, and another one to asynchronously kill the process.

// A regular build should never crash.
// To test that the disk cache doesn't generate critical errors with regular
// application level crashes, edit stress_support.h.

#include <string>
#include <vector>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/process/process_handle.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/disk_cache/blockfile/backend_impl.h"
#include "net/disk_cache/blockfile/stress_support.h"
#include "net/disk_cache/blockfile/trace.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/disk_cache_test_util.h"

#if defined(OS_WIN)
#include "base/logging_win.h"
#endif

using base::Time;

const int kError = -1;
const int kExpectedCrash = 100;

// Starts a new process.
int RunSlave(int iteration) {
  base::FilePath exe;
  PathService::Get(base::FILE_EXE, &exe);

  base::CommandLine cmdline(exe);
  cmdline.AppendArg(base::IntToString(iteration));

  base::ProcessHandle handle;
  if (!base::LaunchProcess(cmdline, base::LaunchOptions(), &handle)) {
    printf("Unable to run test\n");
    return kError;
  }

  int exit_code;
  if (!base::WaitForExitCode(handle, &exit_code)) {
    printf("Unable to get return code\n");
    return kError;
  }
  return exit_code;
}

// Main loop for the master process.
int MasterCode() {
  for (int i = 0; i < 100000; i++) {
    int ret = RunSlave(i);
    if (kExpectedCrash != ret)
      return ret;
  }

  printf("More than enough...\n");

  return 0;
}

// -----------------------------------------------------------------------

std::string GenerateStressKey() {
  char key[20 * 1024];
  size_t size = 50 + rand() % 20000;
  CacheTestFillBuffer(key, size, true);

  key[size - 1] = '\0';
  return std::string(key);
}

// This thread will loop forever, adding and removing entries from the cache.
// iteration is the current crash cycle, so the entries on the cache are marked
// to know which instance of the application wrote them.
void StressTheCache(int iteration) {
  int cache_size = 0x2000000;  // 32MB.
  uint32 mask = 0xfff;  // 4096 entries.

  base::FilePath path;
  PathService::Get(base::DIR_TEMP, &path);
  path = path.AppendASCII("cache_test_stress");

  base::Thread cache_thread("CacheThread");
  if (!cache_thread.StartWithOptions(
          base::Thread::Options(base::MessageLoop::TYPE_IO, 0)))
    return;

  disk_cache::BackendImpl* cache =
      new disk_cache::BackendImpl(path, mask,
                                  cache_thread.message_loop_proxy().get(),
                                  NULL);
  cache->SetMaxSize(cache_size);
  cache->SetFlags(disk_cache::kNoLoadProtection);

  net::TestCompletionCallback cb;
  int rv = cache->Init(cb.callback());

  if (cb.GetResult(rv) != net::OK) {
    printf("Unable to initialize cache.\n");
    return;
  }
  printf("Iteration %d, initial entries: %d\n", iteration,
         cache->GetEntryCount());

  int seed = static_cast<int>(Time::Now().ToInternalValue());
  srand(seed);

  // kNumKeys is meant to be enough to have about 3x or 4x iterations before
  // the process crashes.
#ifdef NDEBUG
  const int kNumKeys = 4000;
#else
  const int kNumKeys = 1200;
#endif
  const int kNumEntries = 30;
  std::string keys[kNumKeys];
  disk_cache::Entry* entries[kNumEntries] = {0};

  for (int i = 0; i < kNumKeys; i++) {
    keys[i] = GenerateStressKey();
  }

  const int kSize = 20000;
  scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(kSize));
  memset(buffer->data(), 'k', kSize);

  for (int i = 0;; i++) {
    int slot = rand() % kNumEntries;
    int key = rand() % kNumKeys;
    bool truncate = (rand() % 2 == 0);
    int size = kSize - (rand() % 20) * kSize / 20;

    if (entries[slot])
      entries[slot]->Close();

    net::TestCompletionCallback cb;
    rv = cache->OpenEntry(keys[key], &entries[slot], cb.callback());
    if (cb.GetResult(rv) != net::OK) {
      rv = cache->CreateEntry(keys[key], &entries[slot], cb.callback());
      CHECK_EQ(net::OK, cb.GetResult(rv));
    }

    base::snprintf(buffer->data(), kSize,
                   "i: %d iter: %d, size: %d, truncate: %d     ", i, iteration,
                   size, truncate ? 1 : 0);
    rv = entries[slot]->WriteData(0, 0, buffer.get(), size, cb.callback(),
                                  truncate);
    CHECK_EQ(size, cb.GetResult(rv));

    if (rand() % 100 > 80) {
      key = rand() % kNumKeys;
      net::TestCompletionCallback cb2;
      rv = cache->DoomEntry(keys[key], cb2.callback());
      cb2.GetResult(rv);
    }

    if (!(i % 100))
      printf("Entries: %d    \r", i);
  }
}

// We want to prevent the timer thread from killing the process while we are
// waiting for the debugger to attach.
bool g_crashing = false;

// RunSoon() and CrashCallback() reference each other, unfortunately.
void RunSoon(base::MessageLoop* target_loop);

void CrashCallback() {
  // Keep trying to run.
  RunSoon(base::MessageLoop::current());

  if (g_crashing)
    return;

  if (rand() % 100 > 30) {
    printf("sweet death...\n");
#if defined(OS_WIN)
    // Windows does more work on _exit() that we would like, so we use Kill.
    base::KillProcessById(base::GetCurrentProcId(), kExpectedCrash, false);
#elif defined(OS_POSIX)
    // On POSIX, _exit() will terminate the process with minimal cleanup,
    // and it is cleaner than killing.
    _exit(kExpectedCrash);
#endif
  }
}

void RunSoon(base::MessageLoop* target_loop) {
  const base::TimeDelta kTaskDelay = base::TimeDelta::FromSeconds(10);
  target_loop->PostDelayedTask(
      FROM_HERE, base::Bind(&CrashCallback), kTaskDelay);
}

// We leak everything here :)
bool StartCrashThread() {
  base::Thread* thread = new base::Thread("party_crasher");
  if (!thread->Start())
    return false;

  RunSoon(thread->message_loop());
  return true;
}

void CrashHandler(const std::string& str) {
  g_crashing = true;
  base::debug::BreakDebugger();
}

bool MessageHandler(int severity, const char* file, int line,
                    size_t message_start, const std::string& str) {
  const size_t kMaxMessageLen = 48;
  char message[kMaxMessageLen];
  size_t len = std::min(str.length() - message_start, kMaxMessageLen - 1);

  memcpy(message, str.c_str() + message_start, len);
  message[len] = '\0';
#if !defined(DISK_CACHE_TRACE_TO_LOG)
  disk_cache::Trace("%s", message);
#endif
  return false;
}

// -----------------------------------------------------------------------

#if defined(OS_WIN)
// {B9A153D4-31C3-48e4-9ABF-D54383F14A0D}
const GUID kStressCacheTraceProviderName = {
    0xb9a153d4, 0x31c3, 0x48e4,
        { 0x9a, 0xbf, 0xd5, 0x43, 0x83, 0xf1, 0x4a, 0xd } };
#endif

int main(int argc, const char* argv[]) {
  // Setup an AtExitManager so Singleton objects will be destructed.
  base::AtExitManager at_exit_manager;

  if (argc < 2)
    return MasterCode();

  logging::SetLogAssertHandler(CrashHandler);
  logging::SetLogMessageHandler(MessageHandler);

#if defined(OS_WIN)
  logging::LogEventProvider::Initialize(kStressCacheTraceProviderName);
#else
  base::CommandLine::Init(argc, argv);
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  logging::InitLogging(settings);
#endif

  // Some time for the memory manager to flush stuff.
  base::PlatformThread::Sleep(base::TimeDelta::FromSeconds(3));
  base::MessageLoopForIO message_loop;

  char* end;
  long int iteration = strtol(argv[1], &end, 0);

  if (!StartCrashThread()) {
    printf("failed to start thread\n");
    return kError;
  }

  StressTheCache(iteration);
  return 0;
}
