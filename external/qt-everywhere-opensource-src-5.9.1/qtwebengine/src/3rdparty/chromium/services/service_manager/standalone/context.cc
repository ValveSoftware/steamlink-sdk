// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/standalone/context.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/process/process_info.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/tracing/common/tracing_switches.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/catalog/catalog.h"
#include "services/catalog/store.h"
#include "services/service_manager/connect_params.h"
#include "services/service_manager/connect_util.h"
#include "services/service_manager/runner/common/switches.h"
#include "services/service_manager/runner/host/in_process_native_runner.h"
#include "services/service_manager/runner/host/out_of_process_native_runner.h"
#include "services/service_manager/standalone/tracer.h"
#include "services/service_manager/switches.h"
#include "services/tracing/public/cpp/provider.h"
#include "services/tracing/public/cpp/switches.h"
#include "services/tracing/public/interfaces/constants.mojom.h"
#include "services/tracing/public/interfaces/tracing.mojom.h"

#if defined(OS_MACOSX)
#include "services/service_manager/runner/host/mach_broker.h"
#endif

namespace service_manager {
namespace {

base::FilePath::StringType GetPathFromCommandLineSwitch(
    const base::StringPiece& value) {
#if defined(OS_POSIX)
  return value.as_string();
#elif defined(OS_WIN)
  return base::UTF8ToUTF16(value);
#endif  // OS_POSIX
}

// Used to ensure we only init once.
class Setup {
 public:
  Setup() { mojo::edk::Init(); }

  ~Setup() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(Setup);
};

const size_t kMaxBlockingPoolThreads = 3;

std::unique_ptr<base::Thread> CreateIOThread(const char* name) {
  std::unique_ptr<base::Thread> thread(new base::Thread(name));
  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  thread->StartWithOptions(options);
  return thread;
}

void OnInstanceQuit(const std::string& name, const Identity& identity) {
  if (name == identity.name())
    base::MessageLoop::current()->QuitWhenIdle();
}

const char kService[] = "service";

}  // namespace

Context::InitParams::InitParams() {}
Context::InitParams::~InitParams() {}

Context::Context()
    : io_thread_(CreateIOThread("io_thread")),
      main_entry_time_(base::Time::Now()) {}

Context::~Context() {
  DCHECK(!base::MessageLoop::current());
  blocking_pool_->Shutdown();
}

// static
void Context::EnsureEmbedderIsInitialized() {
  static base::LazyInstance<Setup>::Leaky setup = LAZY_INSTANCE_INITIALIZER;
  setup.Get();
}

void Context::Init(std::unique_ptr<InitParams> init_params) {
  TRACE_EVENT0("service_manager", "Context::Init");
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  bool trace_startup = command_line.HasSwitch(::switches::kTraceStartup);
  if (trace_startup) {
    tracer_.Start(
        command_line.GetSwitchValueASCII(::switches::kTraceStartup),
        command_line.GetSwitchValueASCII(::switches::kTraceStartupDuration),
        "mojo_runner.trace");
  }

  if (!init_params || init_params->init_edk)
    EnsureEmbedderIsInitialized();

  service_manager_runner_ = base::ThreadTaskRunnerHandle::Get();
  blocking_pool_ =
      new base::SequencedWorkerPool(kMaxBlockingPoolThreads, "blocking_pool",
                                    base::TaskPriority::USER_VISIBLE);

  init_edk_ = !init_params || init_params->init_edk;
  if (init_edk_) {
    mojo::edk::InitIPCSupport(this, io_thread_->task_runner().get());
#if defined(OS_MACOSX)
    mojo::edk::SetMachPortProvider(MachBroker::GetInstance()->port_provider());
#endif
  }

  std::unique_ptr<NativeRunnerFactory> runner_factory;
  if (command_line.HasSwitch(switches::kSingleProcess)) {
#if defined(COMPONENT_BUILD)
    LOG(ERROR) << "Running Mojo in single process component build, which isn't "
               << "supported because statics in apps interact. Use static build"
               << " or don't pass --single-process.";
#endif
    runner_factory.reset(
        new InProcessNativeRunnerFactory(blocking_pool_.get()));
  } else {
    NativeRunnerDelegate* native_runner_delegate = init_params ?
        init_params->native_runner_delegate : nullptr;
    runner_factory.reset(new OutOfProcessNativeRunnerFactory(
        blocking_pool_.get(), native_runner_delegate));
  }
  std::unique_ptr<catalog::Store> store;
  if (init_params)
    store = std::move(init_params->catalog_store);
  catalog_.reset(
      new catalog::Catalog(blocking_pool_.get(), std::move(store), nullptr));
  service_manager_.reset(new ServiceManager(std::move(runner_factory),
                                            catalog_->TakeService()));

  if (command_line.HasSwitch(::switches::kServiceOverrides)) {
    base::FilePath overrides_file(GetPathFromCommandLineSwitch(
        command_line.GetSwitchValueASCII(::switches::kServiceOverrides)));
    JSONFileValueDeserializer deserializer(overrides_file);
    int error = 0;
    std::string message;
    std::unique_ptr<base::Value> contents =
        deserializer.Deserialize(&error, &message);
    if (!contents) {
      LOG(ERROR) << "Failed to parse service overrides file: " << message;
    } else {
      std::unique_ptr<ServiceOverrides> service_overrides =
          base::MakeUnique<ServiceOverrides>(std::move(contents));
      for (const auto& iter : service_overrides->entries()) {
        if (!iter.second.package_name.empty())
          catalog_->OverridePackageName(iter.first, iter.second.package_name);
      }
      service_manager_->SetServiceOverrides(std::move(service_overrides));
    }
  }

  bool enable_stats_collection_bindings =
      command_line.HasSwitch(tracing::kEnableStatsCollectionBindings);

  if (enable_stats_collection_bindings ||
      command_line.HasSwitch(switches::kEnableTracing)) {
    Identity source_identity = CreateServiceManagerIdentity();
    Identity tracing_identity(tracing::mojom::kServiceName, mojom::kRootUserID);
    tracing::mojom::FactoryPtr factory;
    ConnectToInterface(service_manager(), source_identity, tracing_identity,
                       &factory);
    provider_.InitializeWithFactory(&factory);

    if (command_line.HasSwitch(tracing::kTraceStartup)) {
      tracing::mojom::CollectorPtr coordinator;
      ConnectToInterface(service_manager(), source_identity, tracing_identity,
                         &coordinator);
      tracer_.StartCollectingFromTracingService(std::move(coordinator));
    }

    // Record the service manager startup metrics used for performance testing.
    if (enable_stats_collection_bindings) {
      tracing::mojom::StartupPerformanceDataCollectorPtr collector;
      ConnectToInterface(service_manager(), source_identity, tracing_identity,
                         &collector);
#if defined(OS_MACOSX) || defined(OS_WIN) || defined(OS_LINUX)
      // CurrentProcessInfo::CreationTime is only defined on some platforms.
      const base::Time creation_time = base::CurrentProcessInfo::CreationTime();
      collector->SetServiceManagerProcessCreationTime(
          creation_time.ToInternalValue());
#endif
      collector->SetServiceManagerMainEntryPointTime(
          main_entry_time_.ToInternalValue());
    }
  }
}

void Context::Shutdown() {
  // Actions triggered by Service Manager's destructor may require a current
  // message loop,
  // so we should destruct it explicitly now as ~Context() occurs post message
  // loop shutdown.
  service_manager_.reset();

  DCHECK_EQ(base::ThreadTaskRunnerHandle::Get(), service_manager_runner_);

  // If we didn't initialize the edk we should not shut it down.
  if (!init_edk_)
    return;

  TRACE_EVENT0("service_manager", "Context::Shutdown");
  // Post a task in case OnShutdownComplete is called synchronously.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(mojo::edk::ShutdownIPCSupport));
  // We'll quit when we get OnShutdownComplete().
  base::RunLoop().Run();
}

void Context::OnShutdownComplete() {
  DCHECK_EQ(base::ThreadTaskRunnerHandle::Get(), service_manager_runner_);
  base::MessageLoop::current()->QuitWhenIdle();
}

void Context::RunCommandLineApplication() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kService))
    Run(command_line->GetSwitchValueASCII(kService));
}

void Context::Run(const std::string& name) {
  service_manager_->SetInstanceQuitCallback(base::Bind(&OnInstanceQuit, name));

  mojom::InterfaceProviderPtr remote_interfaces;
  mojom::InterfaceProviderPtr local_interfaces;

  std::unique_ptr<ConnectParams> params(new ConnectParams);
  params->set_source(CreateServiceManagerIdentity());
  params->set_target(Identity(name, mojom::kRootUserID));
  params->set_remote_interfaces(mojo::GetProxy(&remote_interfaces));
  service_manager_->Connect(std::move(params));
}

}  // namespace service_manager
