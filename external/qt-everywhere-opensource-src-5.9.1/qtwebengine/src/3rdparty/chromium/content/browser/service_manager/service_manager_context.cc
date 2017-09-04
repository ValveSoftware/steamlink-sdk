// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_manager/service_manager_context.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/service_manager/merge_dictionary.h"
#include "content/common/service_manager/service_manager_connection_impl.h"
#include "content/grit/content_resources.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/utility_process_host.h"
#include "content/public/browser/utility_process_host_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "mojo/edk/embedder/embedder.h"
#include "services/catalog/catalog.h"
#include "services/catalog/manifest_provider.h"
#include "services/catalog/public/interfaces/constants.mojom.h"
#include "services/catalog/store.h"
#include "services/file/public/interfaces/constants.mojom.h"
#include "services/service_manager/connect_params.h"
#include "services/service_manager/native_runner.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/interfaces/service.mojom.h"
#include "services/service_manager/runner/common/client_util.h"
#include "services/service_manager/runner/host/in_process_native_runner.h"
#include "services/service_manager/service_manager.h"

namespace content {

namespace {

base::LazyInstance<std::unique_ptr<service_manager::Connector>>::Leaky
    g_io_thread_connector = LAZY_INSTANCE_INITIALIZER;

void DestroyConnectorOnIOThread() { g_io_thread_connector.Get().reset(); }

void StartUtilityProcessOnIOThread(
    service_manager::mojom::ServiceFactoryRequest request,
    const base::string16& process_name,
    bool use_sandbox) {
  UtilityProcessHost* process_host =
      UtilityProcessHost::Create(nullptr, nullptr);
  process_host->SetName(process_name);
  if (!use_sandbox)
    process_host->DisableSandbox();
  process_host->Start();
  process_host->GetRemoteInterfaces()->GetInterface(std::move(request));
}

void StartServiceInUtilityProcess(
    const std::string& service_name,
    const base::string16& process_name,
    bool use_sandbox,
    service_manager::mojom::ServiceRequest request) {
  service_manager::mojom::ServiceFactoryPtr service_factory;
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&StartUtilityProcessOnIOThread,
                                     base::Passed(GetProxy(&service_factory)),
                                     process_name, use_sandbox));
  service_factory->CreateService(std::move(request), service_name);
}

#if (ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)

// Request service_manager::mojom::ServiceFactory from GPU process host. Must be
// called on
// IO thread.
void RequestGpuServiceFactory(
    service_manager::mojom::ServiceFactoryRequest request) {
  GpuProcessHost* process_host =
      GpuProcessHost::Get(GpuProcessHost::GPU_PROCESS_KIND_SANDBOXED);
  if (!process_host) {
    DLOG(ERROR) << "GPU process host not available.";
    return;
  }

  // TODO(xhwang): It's possible that |process_host| is non-null, but the actual
  // process is dead. In that case, |request| will be dropped and application
  // load requests through ServiceFactory will also fail. Make sure we handle
  // these cases correctly.
  process_host->GetRemoteInterfaces()->GetInterface(std::move(request));
}

void StartServiceInGpuProcess(const std::string& service_name,
                              service_manager::mojom::ServiceRequest request) {
  service_manager::mojom::ServiceFactoryPtr service_factory;
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&RequestGpuServiceFactory,
                 base::Passed(GetProxy(&service_factory))));
  service_factory->CreateService(std::move(request), service_name);
}

#endif  // ENABLE_MOJO_MEDIA_IN_GPU_PROCESS

// A ManifestProvider which resolves application names to builtin manifest
// resources for the catalog service to consume.
class BuiltinManifestProvider : public catalog::ManifestProvider {
 public:
  BuiltinManifestProvider() {}
  ~BuiltinManifestProvider() override {}

  void AddManifestValue(const std::string& name,
                        std::unique_ptr<base::Value> manifest_contents) {
    auto result = manifests_.insert(
        std::make_pair(name, std::move(manifest_contents)));
    DCHECK(result.second) << "Duplicate manifest entry: " << name;
  }

 private:
  // catalog::ManifestProvider:
  std::unique_ptr<base::Value> GetManifest(const std::string& name) override {
    auto it = manifests_.find(name);
    return it != manifests_.end() ? it->second->CreateDeepCopy() : nullptr;
  }

  std::map<std::string, std::unique_ptr<base::Value>> manifests_;

  DISALLOW_COPY_AND_ASSIGN(BuiltinManifestProvider);
};

}  // namespace

// State which lives on the IO thread and drives the ServiceManager.
class ServiceManagerContext::InProcessServiceManagerContext
    : public base::RefCountedThreadSafe<InProcessServiceManagerContext> {
 public:
  InProcessServiceManagerContext() {}

  service_manager::mojom::ServiceRequest Start(
      std::unique_ptr<BuiltinManifestProvider> manifest_provider) {
    service_manager::mojom::ServicePtr embedder_service_proxy;
    service_manager::mojom::ServiceRequest embedder_service_request =
        mojo::GetProxy(&embedder_service_proxy);
    service_manager::mojom::ServicePtrInfo embedder_service_proxy_info =
        embedder_service_proxy.PassInterface();
    BrowserThread::GetTaskRunnerForThread(BrowserThread::IO)->PostTask(
        FROM_HERE,
        base::Bind(&InProcessServiceManagerContext::StartOnIOThread, this,
                   base::Passed(&manifest_provider),
                   base::Passed(&embedder_service_proxy_info)));
    return embedder_service_request;
  }

  void ShutDown() {
    BrowserThread::GetTaskRunnerForThread(BrowserThread::IO)->PostTask(
        FROM_HERE,
        base::Bind(&InProcessServiceManagerContext::ShutDownOnIOThread, this));
  }

 private:
  friend class base::RefCountedThreadSafe<InProcessServiceManagerContext>;

  ~InProcessServiceManagerContext() {}

  void StartOnIOThread(
      std::unique_ptr<BuiltinManifestProvider> manifest_provider,
      service_manager::mojom::ServicePtrInfo embedder_service_proxy_info) {
    manifest_provider_ = std::move(manifest_provider);

    base::SequencedWorkerPool* blocking_pool = BrowserThread::GetBlockingPool();
    std::unique_ptr<service_manager::NativeRunnerFactory> native_runner_factory(
        new service_manager::InProcessNativeRunnerFactory(blocking_pool));
    catalog_.reset(
        new catalog::Catalog(blocking_pool, nullptr, manifest_provider_.get()));
    service_manager_.reset(new service_manager::ServiceManager(
        std::move(native_runner_factory), catalog_->TakeService()));

    service_manager::mojom::ServiceRequest request =
        service_manager_->StartEmbedderService(mojom::kBrowserServiceName);
    mojo::FuseInterface(
        std::move(request), std::move(embedder_service_proxy_info));
  }

  void ShutDownOnIOThread() {
    service_manager_.reset();
    catalog_.reset();
    manifest_provider_.reset();
  }

  std::unique_ptr<BuiltinManifestProvider> manifest_provider_;
  std::unique_ptr<catalog::Catalog> catalog_;
  std::unique_ptr<service_manager::ServiceManager> service_manager_;

  DISALLOW_COPY_AND_ASSIGN(InProcessServiceManagerContext);
};

ServiceManagerContext::ServiceManagerContext() {
  service_manager::mojom::ServiceRequest request;
  if (service_manager::ServiceManagerIsRemote()) {
    mojo::edk::SetParentPipeHandleFromCommandLine();
    request = service_manager::GetServiceRequestFromCommandLine();
  } else {
    std::unique_ptr<BuiltinManifestProvider> manifest_provider =
        base::MakeUnique<BuiltinManifestProvider>();

    static const struct ManifestInfo {
      const char* name;
      int resource_id;
    } kManifests[] = {
      { mojom::kBrowserServiceName, IDR_MOJO_CONTENT_BROWSER_MANIFEST },
      { mojom::kGpuServiceName, IDR_MOJO_CONTENT_GPU_MANIFEST },
      { mojom::kPluginServiceName, IDR_MOJO_CONTENT_PLUGIN_MANIFEST },
      { mojom::kRendererServiceName, IDR_MOJO_CONTENT_RENDERER_MANIFEST },
      { mojom::kUtilityServiceName, IDR_MOJO_CONTENT_UTILITY_MANIFEST },
      { catalog::mojom::kServiceName, IDR_MOJO_CATALOG_MANIFEST },
      { file::mojom::kServiceName, IDR_MOJO_FILE_MANIFEST }
    };

    for (size_t i = 0; i < arraysize(kManifests); ++i) {
      std::string contents = GetContentClient()->GetDataResource(
          kManifests[i].resource_id,
          ui::ScaleFactor::SCALE_FACTOR_NONE).as_string();
      base::debug::Alias(&i);
      CHECK(!contents.empty());

      std::unique_ptr<base::Value> manifest_value =
          base::JSONReader::Read(contents);
      base::debug::Alias(&contents);
      CHECK(manifest_value);

      std::unique_ptr<base::Value> overlay_value =
          GetContentClient()->browser()->GetServiceManifestOverlay(
              kManifests[i].name);
      if (overlay_value) {
        base::DictionaryValue* manifest_dictionary = nullptr;
        CHECK(manifest_value->GetAsDictionary(&manifest_dictionary));
        base::DictionaryValue* overlay_dictionary = nullptr;
        CHECK(overlay_value->GetAsDictionary(&overlay_dictionary));
        MergeDictionary(manifest_dictionary, overlay_dictionary);
      }

      manifest_provider->AddManifestValue(kManifests[i].name,
                                          std::move(manifest_value));
    }
    in_process_context_ = new InProcessServiceManagerContext;
    request = in_process_context_->Start(std::move(manifest_provider));
  }
  ServiceManagerConnection::SetForProcess(ServiceManagerConnection::Create(
      std::move(request),
      BrowserThread::GetTaskRunnerForThread(BrowserThread::IO)));

  ContentBrowserClient::StaticServiceMap services;
  GetContentClient()->browser()->RegisterInProcessServices(&services);
  for (const auto& entry : services) {
    ServiceManagerConnection::GetForProcess()->AddEmbeddedService(entry.first,
                                                                  entry.second);
  }

  // This is safe to assign directly from any thread, because
  // ServiceManagerContext must be constructed before anyone can call
  // GetConnectorForIOThread().
  g_io_thread_connector.Get() =
      ServiceManagerConnection::GetForProcess()->GetConnector()->Clone();

  ServiceManagerConnection::GetForProcess()->Start();

  ContentBrowserClient::OutOfProcessServiceMap sandboxed_services;
  GetContentClient()
      ->browser()
      ->RegisterOutOfProcessServices(&sandboxed_services);
  for (const auto& service : sandboxed_services) {
    ServiceManagerConnection::GetForProcess()->AddServiceRequestHandler(
      service.first,
        base::Bind(&StartServiceInUtilityProcess, service.first, service.second,
                   true /* use_sandbox */));
  }

  ContentBrowserClient::OutOfProcessServiceMap unsandboxed_services;
  GetContentClient()
      ->browser()
      ->RegisterUnsandboxedOutOfProcessServices(&unsandboxed_services);
  for (const auto& service : unsandboxed_services) {
    ServiceManagerConnection::GetForProcess()->AddServiceRequestHandler(
        service.first,
        base::Bind(&StartServiceInUtilityProcess, service.first, service.second,
                   false /* use_sandbox */));
  }

#if (ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
  ServiceManagerConnection::GetForProcess()->AddServiceRequestHandler(
      "media", base::Bind(&StartServiceInGpuProcess, "media"));
#endif
}

ServiceManagerContext::~ServiceManagerContext() {
  // NOTE: The in-process ServiceManager MUST be destroyed before the browser
  // process-wide ServiceManagerConnection. Otherwise it's possible for the
  // ServiceManager to receive connection requests for service:content_browser
  // which it may attempt to service by launching a new instance of the browser.
  if (in_process_context_)
    in_process_context_->ShutDown();
  if (ServiceManagerConnection::GetForProcess())
    ServiceManagerConnection::DestroyForProcess();
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&DestroyConnectorOnIOThread));
}

// static
service_manager::Connector* ServiceManagerContext::GetConnectorForIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return g_io_thread_connector.Get().get();
}

}  // namespace content
