// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/devtools/devtools_cpu_throttler.h"

#include "base/atomicops.h"
#include "base/macros.h"
#include "base/synchronization/cancellation_flag.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"

#if defined(OS_POSIX)
#include <signal.h>
#define USE_SIGNALS 1
#endif

using base::subtle::Atomic32;
using base::subtle::Acquire_Load;
using base::subtle::Release_Store;

namespace content {

class CPUThrottlingThread final : public base::PlatformThread::Delegate {
 public:
  explicit CPUThrottlingThread(double rate);
  ~CPUThrottlingThread() override;

  void SetThrottlingRate(double rate);

 private:
  void ThreadMain() override;

  void Start();
  void Stop();
  void Throttle();

  static void SuspendThread(base::PlatformThreadHandle thread_handle);
  static void ResumeThread(base::PlatformThreadHandle thread_handle);
  static void Sleep(base::TimeDelta duration);

#ifdef USE_SIGNALS
  void InstallSignalHandler();
  void RestoreSignalHandler();
  static void HandleSignal(int signal);

  static bool signal_handler_installed_;
  static struct sigaction old_signal_handler_;
  static Atomic32 suspended_;
#endif
  static Atomic32 thread_exists_;

  base::PlatformThreadHandle throttled_thread_handle_;
  base::PlatformThreadHandle throttling_thread_handle_;
  base::CancellationFlag cancellation_flag_;
  Atomic32 throttling_rate_percent_;

  DISALLOW_COPY_AND_ASSIGN(CPUThrottlingThread);
};

#ifdef USE_SIGNALS
bool CPUThrottlingThread::signal_handler_installed_;
struct sigaction CPUThrottlingThread::old_signal_handler_;
Atomic32 CPUThrottlingThread::suspended_;
#endif
Atomic32 CPUThrottlingThread::thread_exists_;

CPUThrottlingThread::CPUThrottlingThread(double rate)
    : throttled_thread_handle_(base::PlatformThread::CurrentHandle()),
      throttling_rate_percent_(static_cast<Atomic32>(rate * 100)) {
  CHECK(base::subtle::NoBarrier_AtomicExchange(&thread_exists_, 1) == 0);
  Start();
}

CPUThrottlingThread::~CPUThrottlingThread() {
  Stop();
  CHECK(base::subtle::NoBarrier_AtomicExchange(&thread_exists_, 0) == 1);
}

void CPUThrottlingThread::SetThrottlingRate(double rate) {
  Release_Store(&throttling_rate_percent_, static_cast<Atomic32>(rate * 100));
}

void CPUThrottlingThread::ThreadMain() {
  base::PlatformThread::SetName("DevToolsCPUThrottlingThread");
  while (!cancellation_flag_.IsSet()) {
    Throttle();
  }
}

#ifdef USE_SIGNALS

// static
void CPUThrottlingThread::InstallSignalHandler() {
  // There must be the only one!
  DCHECK(!signal_handler_installed_);
  struct sigaction sa;
  sa.sa_handler = &HandleSignal;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  signal_handler_installed_ =
      (sigaction(SIGUSR2, &sa, &old_signal_handler_) == 0);
}

// static
void CPUThrottlingThread::RestoreSignalHandler() {
  if (!signal_handler_installed_)
    return;
  sigaction(SIGUSR2, &old_signal_handler_, 0);
  signal_handler_installed_ = false;
}

// static
void CPUThrottlingThread::HandleSignal(int signal) {
  if (signal != SIGUSR2)
    return;
  while (Acquire_Load(&suspended_)) {
  }
}

#endif  // USE_SIGNALS

// static
void CPUThrottlingThread::SuspendThread(
    base::PlatformThreadHandle thread_handle) {
#if defined(USE_SIGNALS)
  Release_Store(&suspended_, 1);
  pthread_kill(thread_handle.platform_handle(), SIGUSR2);
#elif defined(OS_WIN)
  ::SuspendThread(thread_handle.platform_handle());
#endif
}

// static
void CPUThrottlingThread::ResumeThread(
    base::PlatformThreadHandle thread_handle) {
#if defined(USE_SIGNALS)
  Release_Store(&suspended_, 0);
#elif defined(OS_WIN)
  ::ResumeThread(thread_handle.platform_handle());
#endif
}

void CPUThrottlingThread::Start() {
#ifdef USE_SIGNALS
  InstallSignalHandler();
#elif !defined(OS_WIN)
  LOG(ERROR) << "CPU throttling is not supported."
#endif
  if (!base::PlatformThread::Create(0, this, &throttling_thread_handle_)) {
    LOG(ERROR) << "Failed to create throttling thread.";
  }
}

void CPUThrottlingThread::Sleep(base::TimeDelta duration) {
#if defined(OS_WIN)
  // We cannot rely on ::Sleep function as it's precision is not enough for
  // the purpose. Could be up to 16ms jitter.
  base::TimeTicks wakeup_time = base::TimeTicks::Now() + duration;
  while (base::TimeTicks::Now() < wakeup_time) {}
#else
  base::PlatformThread::Sleep(duration);
#endif
}

void CPUThrottlingThread::Stop() {
  cancellation_flag_.Set();
  base::PlatformThread::Join(throttling_thread_handle_);
#ifdef USE_SIGNALS
  RestoreSignalHandler();
#endif
}

void CPUThrottlingThread::Throttle() {
  const int quant_time_us = 200;
  double rate = Acquire_Load(&throttling_rate_percent_) / 100.;
  base::TimeDelta run_duration =
      base::TimeDelta::FromMicroseconds(static_cast<int>(quant_time_us / rate));
  base::TimeDelta sleep_duration =
      base::TimeDelta::FromMicroseconds(quant_time_us) - run_duration;
  Sleep(run_duration);
  SuspendThread(throttled_thread_handle_);
  Sleep(sleep_duration);
  ResumeThread(throttled_thread_handle_);
}

DevToolsCPUThrottler::DevToolsCPUThrottler() {}

DevToolsCPUThrottler::~DevToolsCPUThrottler() {}

void DevToolsCPUThrottler::SetThrottlingRate(double rate) {
  if (rate <= 1) {
    if (throttling_thread_) {
      throttling_thread_.reset();
    }
    return;
  }
  if (throttling_thread_) {
    throttling_thread_->SetThrottlingRate(rate);
  } else {
    throttling_thread_.reset(new CPUThrottlingThread(rate));
  }
}

}  // namespace content
