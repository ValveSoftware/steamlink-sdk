// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/devtools/v8_sampling_profiler.h"

#include <stdint.h>
#include <string.h>

#include "base/format_macros.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/cancellation_flag.h"
#include "base/threading/platform_thread.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "build/build_config.h"
#include "content/renderer/devtools/lock_free_circular_queue.h"
#include "content/renderer/render_thread_impl.h"
#include "v8/include/v8.h"

#if defined(OS_POSIX)
#include <signal.h>
#define USE_SIGNALS
#endif

#if defined(OS_WIN)
#include <windows.h>
#endif

using base::trace_event::ConvertableToTraceFormat;
using base::trace_event::TraceLog;
using base::trace_event::TracedValue;
using v8::Isolate;

namespace content {

namespace {

class PlatformDataCommon {
 public:
  base::PlatformThreadId thread_id() { return thread_id_; }

 protected:
  PlatformDataCommon() : thread_id_(base::PlatformThread::CurrentId()) {}

 private:
  base::PlatformThreadId thread_id_;
};

#if defined(USE_SIGNALS)

class PlatformData : public PlatformDataCommon {
 public:
  PlatformData() : vm_tid_(pthread_self()) {}
  pthread_t vm_tid() const { return vm_tid_; }

 private:
  pthread_t vm_tid_;
};

#elif defined(OS_WIN)

class PlatformData : public PlatformDataCommon {
 public:
  // Get a handle to the calling thread. This is the thread that we are
  // going to profile. We need to make a copy of the handle because we are
  // going to use it in the sampler thread. Using GetThreadHandle() will
  // not work in this case. We're using OpenThread because DuplicateHandle
  // doesn't work in Chrome's sandbox.
  PlatformData()
      : thread_handle_(::OpenThread(THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME |
                                        THREAD_QUERY_INFORMATION,
                                    false,
                                    ::GetCurrentThreadId())) {}

  ~PlatformData() {
    if (thread_handle_ == NULL)
      return;
    ::CloseHandle(thread_handle_);
    thread_handle_ = NULL;
  }

  HANDLE thread_handle() { return thread_handle_; }

 private:
  HANDLE thread_handle_;
};
#endif

std::string PtrToString(const void* value) {
  return base::StringPrintf(
      "0x%" PRIx64, static_cast<uint64_t>(reinterpret_cast<intptr_t>(value)));
}

class SampleRecord {
 public:
  static const int kMaxFramesCountLog2 = 8;
  static const unsigned kMaxFramesCount = (1u << kMaxFramesCountLog2) - 1;

  SampleRecord() {}

  base::TimeTicks timestamp() const { return timestamp_; }
  void Collect(v8::Isolate* isolate,
               base::TimeTicks timestamp,
               const v8::RegisterState& state);
  std::unique_ptr<ConvertableToTraceFormat> ToTraceFormat() const;

 private:
  base::TimeTicks timestamp_;
  unsigned vm_state_ : 4;
  unsigned frames_count_ : kMaxFramesCountLog2;
  const void* frames_[kMaxFramesCount];

  DISALLOW_COPY_AND_ASSIGN(SampleRecord);
};

void SampleRecord::Collect(v8::Isolate* isolate,
                           base::TimeTicks timestamp,
                           const v8::RegisterState& state) {
  v8::SampleInfo sample_info;
  isolate->GetStackSample(state, (void**)frames_, kMaxFramesCount,
                          &sample_info);
  timestamp_ = timestamp;
  frames_count_ = sample_info.frames_count;
  vm_state_ = sample_info.vm_state;
}

std::unique_ptr<ConvertableToTraceFormat> SampleRecord::ToTraceFormat() const {
  std::unique_ptr<base::trace_event::TracedValue> data(
      new base::trace_event::TracedValue());
  const char* vm_state = nullptr;
  switch (vm_state_) {
    case v8::StateTag::JS:
      vm_state = "js";
      break;
    case v8::StateTag::GC:
      vm_state = "gc";
      break;
    case v8::StateTag::COMPILER:
      vm_state = "compiler";
      break;
    case v8::StateTag::OTHER:
      vm_state = "other";
      break;
    case v8::StateTag::EXTERNAL:
      vm_state = "external";
      break;
    case v8::StateTag::IDLE:
      vm_state = "idle";
      break;
    default:
      NOTREACHED();
  }
  data->SetString("vm_state", vm_state);
  data->BeginArray("stack");
  for (unsigned i = 0; i < frames_count_; ++i) {
    data->AppendString(PtrToString(frames_[i]));
  }
  data->EndArray();
  return std::move(data);
}

}  // namespace

// The class implements a sampler responsible for sampling a single thread.
class Sampler {
 public:
  ~Sampler();

  static std::unique_ptr<Sampler> CreateForCurrentThread();
  static Sampler* GetInstance() { return tls_instance_.Pointer()->Get(); }

  // These methods are called from the sampling thread.
  void Start();
  void Stop();
  void Sample();

  void DoSample(const v8::RegisterState& state);

  void SetEventsToCollectForTest(int code_added_events, int sample_events) {
    code_added_events_to_collect_for_test_ = code_added_events;
    sample_events_to_collect_for_test_ = sample_events;
  }

  bool EventsCollectedForTest() const {
    return base::subtle::NoBarrier_Load(&code_added_events_count_) >=
               code_added_events_to_collect_for_test_ &&
           base::subtle::NoBarrier_Load(&samples_count_) >=
               sample_events_to_collect_for_test_;
  }

 private:
  Sampler();

  static void InstallJitCodeEventHandler(Isolate* isolate, void* data);
  static void HandleJitCodeEvent(const v8::JitCodeEvent* event);
  static std::unique_ptr<ConvertableToTraceFormat> JitCodeEventToTraceFormat(
      const v8::JitCodeEvent* event);

  void InjectPendingEvents();

  static const unsigned kNumberOfSamples = 10;
  typedef LockFreeCircularQueue<SampleRecord, kNumberOfSamples> SamplingQueue;

  PlatformData platform_data_;
  Isolate* isolate_;
  std::unique_ptr<SamplingQueue> samples_data_;
  base::subtle::Atomic32 code_added_events_count_;
  base::subtle::Atomic32 samples_count_;
  int code_added_events_to_collect_for_test_;
  int sample_events_to_collect_for_test_;

  static base::LazyInstance<base::ThreadLocalPointer<Sampler>>::Leaky
      tls_instance_;
};

base::LazyInstance<base::ThreadLocalPointer<Sampler>>::Leaky
    Sampler::tls_instance_ = LAZY_INSTANCE_INITIALIZER;

Sampler::Sampler()
    : isolate_(Isolate::GetCurrent()),
      code_added_events_count_(0),
      samples_count_(0),
      code_added_events_to_collect_for_test_(0),
      sample_events_to_collect_for_test_(0) {
  DCHECK(isolate_);
  DCHECK(!GetInstance());
  tls_instance_.Pointer()->Set(this);
}

Sampler::~Sampler() {
  DCHECK(GetInstance());
  tls_instance_.Pointer()->Set(nullptr);
}

// static
std::unique_ptr<Sampler> Sampler::CreateForCurrentThread() {
  return std::unique_ptr<Sampler>(new Sampler());
}

void Sampler::Start() {
  samples_data_.reset(new SamplingQueue());
  v8::JitCodeEventHandler handler = &HandleJitCodeEvent;
  isolate_->RequestInterrupt(&InstallJitCodeEventHandler,
                             reinterpret_cast<void*>(handler));
}

void Sampler::Stop() {
  isolate_->RequestInterrupt(&InstallJitCodeEventHandler, nullptr);
  samples_data_.reset();
}

#if ARCH_CPU_64_BITS
#define REG_64_32(reg64, reg32) reg64
#else
#define REG_64_32(reg64, reg32) reg32
#endif  // ARCH_CPU_64_BITS

void Sampler::Sample() {
#if defined(OS_WIN)
  const DWORD kSuspendFailed = static_cast<DWORD>(-1);
  if (::SuspendThread(platform_data_.thread_handle()) == kSuspendFailed)
    return;
  CONTEXT context;
  memset(&context, 0, sizeof(context));
  context.ContextFlags = CONTEXT_FULL;
  if (::GetThreadContext(platform_data_.thread_handle(), &context) != 0) {
    v8::RegisterState state;
    state.pc = reinterpret_cast<void*>(context.REG_64_32(Rip, Eip));
    state.sp = reinterpret_cast<void*>(context.REG_64_32(Rsp, Esp));
    state.fp = reinterpret_cast<void*>(context.REG_64_32(Rbp, Ebp));
    // TODO(alph): It is not needed to buffer the events on Windows.
    // We can just collect and fire trace event right away.
    DoSample(state);
  }
  ::ResumeThread(platform_data_.thread_handle());
#elif defined(USE_SIGNALS)
  int error = pthread_kill(platform_data_.vm_tid(), SIGPROF);
  if (error) {
    LOG(ERROR) << "pthread_kill failed with error " << error << " "
               << strerror(error);
  }
#endif
  InjectPendingEvents();
}

void Sampler::DoSample(const v8::RegisterState& state) {
  // Called in the sampled thread signal handler.
  // Because of that it is not allowed to do any memory allocation here.
  base::TimeTicks timestamp = base::TimeTicks::Now();
  SampleRecord* record = samples_data_->StartEnqueue();
  if (!record)
    return;
  record->Collect(isolate_, timestamp, state);
  samples_data_->FinishEnqueue();
}

void Sampler::InjectPendingEvents() {
  SampleRecord* record = samples_data_->Peek();
  while (record) {
    TRACE_EVENT_SAMPLE_WITH_TID_AND_TIMESTAMP1(
        TRACE_DISABLED_BY_DEFAULT("v8.cpu_profile"), "V8Sample",
        platform_data_.thread_id(),
        (record->timestamp() - base::TimeTicks()).InMicroseconds(), "data",
        record->ToTraceFormat());
    samples_data_->Remove();
    record = samples_data_->Peek();
    base::subtle::NoBarrier_AtomicIncrement(&samples_count_, 1);
  }
}

// static
void Sampler::InstallJitCodeEventHandler(Isolate* isolate, void* data) {
  // Called on the sampled V8 thread.
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.cpu_profile"),
               "Sampler::InstallJitCodeEventHandler");
  v8::JitCodeEventHandler handler =
      reinterpret_cast<v8::JitCodeEventHandler>(data);
  isolate->SetJitCodeEventHandler(
      v8::JitCodeEventOptions::kJitCodeEventEnumExisting, handler);
}

// static
void Sampler::HandleJitCodeEvent(const v8::JitCodeEvent* event) {
  // Called on the sampled V8 thread.
  Sampler* sampler = GetInstance();
  // The sampler may have already been destroyed.
  // That's fine, we're not interested in these events anymore.
  if (!sampler)
    return;
  switch (event->type) {
    case v8::JitCodeEvent::CODE_ADDED:
      TRACE_EVENT_METADATA1(TRACE_DISABLED_BY_DEFAULT("v8.cpu_profile"),
                            "JitCodeAdded", "data",
                            JitCodeEventToTraceFormat(event));
      base::subtle::NoBarrier_AtomicIncrement(
          &sampler->code_added_events_count_, 1);
      break;

    case v8::JitCodeEvent::CODE_MOVED:
      TRACE_EVENT_METADATA1(TRACE_DISABLED_BY_DEFAULT("v8.cpu_profile"),
                            "JitCodeMoved", "data",
                            JitCodeEventToTraceFormat(event));
      break;

    case v8::JitCodeEvent::CODE_REMOVED:
      TRACE_EVENT_METADATA1(TRACE_DISABLED_BY_DEFAULT("v8.cpu_profile"),
                            "JitCodeRemoved", "data",
                            JitCodeEventToTraceFormat(event));
      break;

    case v8::JitCodeEvent::CODE_ADD_LINE_POS_INFO:
    case v8::JitCodeEvent::CODE_START_LINE_INFO_RECORDING:
    case v8::JitCodeEvent::CODE_END_LINE_INFO_RECORDING:
      break;
  }
}

// static
std::unique_ptr<ConvertableToTraceFormat> Sampler::JitCodeEventToTraceFormat(
    const v8::JitCodeEvent* event) {
  switch (event->type) {
    case v8::JitCodeEvent::CODE_ADDED: {
      std::unique_ptr<base::trace_event::TracedValue> data(
          new base::trace_event::TracedValue());
      data->SetString("code_start", PtrToString(event->code_start));
      data->SetInteger("code_len", static_cast<unsigned>(event->code_len));
      data->SetString("name", std::string(event->name.str, event->name.len));
      if (!event->script.IsEmpty()) {
        data->SetInteger("script_id", event->script->GetId());
      }
      return std::move(data);
    }

    case v8::JitCodeEvent::CODE_MOVED: {
      std::unique_ptr<base::trace_event::TracedValue> data(
          new base::trace_event::TracedValue());
      data->SetString("code_start", PtrToString(event->code_start));
      data->SetInteger("code_len", static_cast<unsigned>(event->code_len));
      data->SetString("new_code_start", PtrToString(event->new_code_start));
      return std::move(data);
    }

    case v8::JitCodeEvent::CODE_REMOVED: {
      std::unique_ptr<base::trace_event::TracedValue> data(
          new base::trace_event::TracedValue());
      data->SetString("code_start", PtrToString(event->code_start));
      data->SetInteger("code_len", static_cast<unsigned>(event->code_len));
      return std::move(data);
    }

    case v8::JitCodeEvent::CODE_ADD_LINE_POS_INFO:
    case v8::JitCodeEvent::CODE_START_LINE_INFO_RECORDING:
    case v8::JitCodeEvent::CODE_END_LINE_INFO_RECORDING:
      return nullptr;
  }
  return nullptr;
}

class V8SamplingThread : public base::PlatformThread::Delegate {
 public:
  V8SamplingThread(Sampler*, base::WaitableEvent*);

  // Implementation of PlatformThread::Delegate:
  void ThreadMain() override;

  void Start();
  void Stop();

 private:
  void Sample();
  void InstallSamplers();
  void RemoveSamplers();
  void StartSamplers();
  void StopSamplers();

  static void InstallSignalHandler();
  static void RestoreSignalHandler();
#ifdef USE_SIGNALS
  static void HandleProfilerSignal(int signal, siginfo_t* info, void* context);
#endif

  static void HandleJitCodeEvent(const v8::JitCodeEvent* event);

  Sampler* render_thread_sampler_;
  base::CancellationFlag cancellation_flag_;
  base::WaitableEvent* waitable_event_for_testing_;
  base::PlatformThreadHandle sampling_thread_handle_;
  std::vector<Sampler*> samplers_;

#ifdef USE_SIGNALS
  static bool signal_handler_installed_;
  static struct sigaction old_signal_handler_;
#endif

  DISALLOW_COPY_AND_ASSIGN(V8SamplingThread);
};

#ifdef USE_SIGNALS
bool V8SamplingThread::signal_handler_installed_;
struct sigaction V8SamplingThread::old_signal_handler_;
#endif

V8SamplingThread::V8SamplingThread(Sampler* render_thread_sampler,
                                   base::WaitableEvent* event)
    : render_thread_sampler_(render_thread_sampler),
      waitable_event_for_testing_(event) {
}

void V8SamplingThread::ThreadMain() {
  base::PlatformThread::SetName("V8SamplingProfilerThread");
  InstallSamplers();
  StartSamplers();
  InstallSignalHandler();
  bool enabled_hires;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(
      TRACE_DISABLED_BY_DEFAULT("v8.cpu_profile.hires"), &enabled_hires);
  const int kSamplingFrequencyMicroseconds = enabled_hires ? 100 : 1000;
  while (!cancellation_flag_.IsSet()) {
    Sample();
    if (waitable_event_for_testing_ &&
        render_thread_sampler_->EventsCollectedForTest()) {
      waitable_event_for_testing_->Signal();
    }
    // TODO(alph): make the samples firing interval not depend on the sample
    // taking duration.
    base::PlatformThread::Sleep(
        base::TimeDelta::FromMicroseconds(kSamplingFrequencyMicroseconds));
  }
  RestoreSignalHandler();
  StopSamplers();
  RemoveSamplers();
}

void V8SamplingThread::Sample() {
  for (Sampler* sampler : samplers_) {
    sampler->Sample();
  }
}

void V8SamplingThread::InstallSamplers() {
  // Note that the list does not own samplers.
  samplers_.push_back(render_thread_sampler_);
  // TODO: add worker samplers.
}

void V8SamplingThread::RemoveSamplers() {
  samplers_.clear();
}

void V8SamplingThread::StartSamplers() {
  for (Sampler* sampler : samplers_) {
    sampler->Start();
  }
}

void V8SamplingThread::StopSamplers() {
  for (Sampler* sampler : samplers_) {
    sampler->Stop();
  }
}

// static
void V8SamplingThread::InstallSignalHandler() {
#ifdef USE_SIGNALS
  // There must be the only one!
  DCHECK(!signal_handler_installed_);
  struct sigaction sa;
  sa.sa_sigaction = &HandleProfilerSignal;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART | SA_SIGINFO;
  signal_handler_installed_ =
      (sigaction(SIGPROF, &sa, &old_signal_handler_) == 0);
#endif
}

// static
void V8SamplingThread::RestoreSignalHandler() {
#ifdef USE_SIGNALS
  if (!signal_handler_installed_)
    return;
  sigaction(SIGPROF, &old_signal_handler_, 0);
  signal_handler_installed_ = false;
#endif
}

#ifdef USE_SIGNALS
// static
void V8SamplingThread::HandleProfilerSignal(int signal,
                                            siginfo_t* info,
                                            void* context) {
  if (signal != SIGPROF)
    return;
  ucontext_t* ucontext = reinterpret_cast<ucontext_t*>(context);
  mcontext_t& mcontext = ucontext->uc_mcontext;
  v8::RegisterState state;

#if defined(ARCH_CPU_ARM_FAMILY)
  state.pc = reinterpret_cast<void*>(mcontext.REG_64_32(pc, arm_pc));
  state.sp = reinterpret_cast<void*>(mcontext.REG_64_32(sp, arm_sp));
  state.fp = reinterpret_cast<void*>(mcontext.REG_64_32(regs[29], arm_fp));

#elif defined(ARCH_CPU_X86_FAMILY)
#if defined(OS_MACOSX)
  state.pc = reinterpret_cast<void*>(mcontext->__ss.REG_64_32(__rip, __eip));
  state.sp = reinterpret_cast<void*>(mcontext->__ss.REG_64_32(__rsp, __esp));
  state.fp = reinterpret_cast<void*>(mcontext->__ss.REG_64_32(__rbp, __ebp));
#else
  state.pc =
      reinterpret_cast<void*>(mcontext.gregs[REG_64_32(REG_RIP, REG_EIP)]);
  state.sp =
      reinterpret_cast<void*>(mcontext.gregs[REG_64_32(REG_RSP, REG_ESP)]);
  state.fp =
      reinterpret_cast<void*>(mcontext.gregs[REG_64_32(REG_RBP, REG_EBP)]);
#endif  // OS_MACOS
#elif defined(ARCH_CPU_MIPS_FAMILY)
  state.pc = reinterpret_cast<void*>(mcontext.pc);
  state.sp = reinterpret_cast<void*>(mcontext.gregs[29]);
  state.fp = reinterpret_cast<void*>(mcontext.gregs[30]);
#endif  // ARCH_CPU_MIPS_FAMILY

  Sampler::GetInstance()->DoSample(state);
}
#endif

void V8SamplingThread::Start() {
  if (!base::PlatformThread::Create(0, this, &sampling_thread_handle_)) {
    DCHECK(false) << "failed to create sampling thread";
  }
}

void V8SamplingThread::Stop() {
  cancellation_flag_.Set();
  base::PlatformThread::Join(sampling_thread_handle_);
}

V8SamplingProfiler::V8SamplingProfiler(bool underTest)
    : sampling_thread_(nullptr),
      render_thread_sampler_(Sampler::CreateForCurrentThread()),
      weak_factory_(this) {
  DCHECK(underTest || RenderThreadImpl::current());
  DCHECK(thread_checker_.CalledOnValidThread());
  // Force the "v8.cpu_profile*" categories to show up in the trace viewer.
  TraceLog::GetCategoryGroupEnabled(
      TRACE_DISABLED_BY_DEFAULT("v8.cpu_profile"));
  TraceLog::GetCategoryGroupEnabled(
      TRACE_DISABLED_BY_DEFAULT("v8.cpu_profile.hires"));
  TraceLog::GetInstance()->AddAsyncEnabledStateObserver(
      weak_factory_.GetWeakPtr());
}

V8SamplingProfiler::~V8SamplingProfiler() {
  DCHECK(thread_checker_.CalledOnValidThread());
  TraceLog::GetInstance()->RemoveAsyncEnabledStateObserver(this);
  DCHECK(!sampling_thread_.get());
}

void V8SamplingProfiler::StartSamplingThread() {
  DCHECK(!sampling_thread_.get());
  DCHECK(thread_checker_.CalledOnValidThread());
  sampling_thread_.reset(new V8SamplingThread(
      render_thread_sampler_.get(), waitable_event_for_testing_.get()));
  sampling_thread_->Start();
}

void V8SamplingProfiler::StopSamplingThread() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!sampling_thread_.get())
    return;
  sampling_thread_->Stop();
  sampling_thread_.reset();
}

void V8SamplingProfiler::OnTraceLogEnabled() {
  bool enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(
      TRACE_DISABLED_BY_DEFAULT("v8.cpu_profile"), &enabled);
  if (!enabled)
    return;

  // Do not enable sampling profiler in continuous mode, as losing
  // Jit code events may not be afforded.
  // TODO(alph): add support of infinite recording of meta trace events.
  base::trace_event::TraceRecordMode record_mode =
      TraceLog::GetInstance()->GetCurrentTraceConfig().GetTraceRecordMode();
  if (record_mode == base::trace_event::TraceRecordMode::RECORD_CONTINUOUSLY)
    return;

  StartSamplingThread();
}

void V8SamplingProfiler::OnTraceLogDisabled() {
  StopSamplingThread();
}

void V8SamplingProfiler::EnableSamplingEventForTesting(int code_added_events,
                                                       int sample_events) {
  render_thread_sampler_->SetEventsToCollectForTest(code_added_events,
                                                    sample_events);
  waitable_event_for_testing_.reset(
      new base::WaitableEvent(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                              base::WaitableEvent::InitialState::NOT_SIGNALED));
}

void V8SamplingProfiler::WaitSamplingEventForTesting() {
  waitable_event_for_testing_->Wait();
}

}  // namespace content
