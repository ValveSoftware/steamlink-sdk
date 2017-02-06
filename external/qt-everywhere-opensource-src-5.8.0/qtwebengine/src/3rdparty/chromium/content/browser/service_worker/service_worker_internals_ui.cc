// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_internals_ui.h"

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/browser/devtools/service_worker_devtools_manager.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/service_worker_context_observer.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/grit/content_resources.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/url_constants.h"

using base::DictionaryValue;
using base::FundamentalValue;
using base::ListValue;
using base::StringValue;
using base::Value;
using base::WeakPtr;

namespace content {

namespace {

using GetRegistrationsCallback =
    base::Callback<void(const std::vector<ServiceWorkerRegistrationInfo>&,
                        const std::vector<ServiceWorkerVersionInfo>&,
                        const std::vector<ServiceWorkerRegistrationInfo>&)>;

void OperationCompleteCallback(WeakPtr<ServiceWorkerInternalsUI> internals,
                               int callback_id,
                               ServiceWorkerStatusCode status) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(OperationCompleteCallback, internals, callback_id, status));
    return;
  }
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (internals) {
    internals->web_ui()->CallJavascriptFunctionUnsafe(
        "serviceworker.onOperationComplete",
        FundamentalValue(static_cast<int>(status)),
        FundamentalValue(callback_id));
  }
}

void CallServiceWorkerVersionMethodWithVersionID(
    ServiceWorkerInternalsUI::ServiceWorkerVersionMethod method,
    scoped_refptr<ServiceWorkerContextWrapper> context,
    int64_t version_id,
    const ServiceWorkerInternalsUI::StatusCallback& callback) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(CallServiceWorkerVersionMethodWithVersionID,
                   method,
                   context,
                   version_id,
                   callback));
    return;
  }

  scoped_refptr<ServiceWorkerVersion> version =
      context->GetLiveVersion(version_id);
  if (!version.get()) {
    callback.Run(SERVICE_WORKER_ERROR_NOT_FOUND);
    return;
  }
  (*version.get().*method)(callback);
}

base::ProcessId GetRealProcessId(int process_host_id) {
  if (process_host_id == ChildProcessHost::kInvalidUniqueID)
    return base::kNullProcessId;

  RenderProcessHost* rph = RenderProcessHost::FromID(process_host_id);
  if (!rph)
    return base::kNullProcessId;

  base::ProcessHandle handle = rph->GetHandle();
  if (handle == base::kNullProcessHandle)
    return base::kNullProcessId;
  // TODO(nhiroki): On Windows, |rph->GetHandle()| does not duplicate ownership
  // of the process handle and the render host still retains it. Therefore, we
  // cannot create a base::Process object, which provides a proper way to get a
  // process id, from the handle. For a stopgap, we use this deprecated
  // function that does not require the ownership (http://crbug.com/417532).
  return base::GetProcId(handle);
}

void UpdateVersionInfo(const ServiceWorkerVersionInfo& version,
                       DictionaryValue* info) {
  switch (version.running_status) {
    case EmbeddedWorkerStatus::STOPPED:
      info->SetString("running_status", "STOPPED");
      break;
    case EmbeddedWorkerStatus::STARTING:
      info->SetString("running_status", "STARTING");
      break;
    case EmbeddedWorkerStatus::RUNNING:
      info->SetString("running_status", "RUNNING");
      break;
    case EmbeddedWorkerStatus::STOPPING:
      info->SetString("running_status", "STOPPING");
      break;
  }

  switch (version.status) {
    case ServiceWorkerVersion::NEW:
      info->SetString("status", "NEW");
      break;
    case ServiceWorkerVersion::INSTALLING:
      info->SetString("status", "INSTALLING");
      break;
    case ServiceWorkerVersion::INSTALLED:
      info->SetString("status", "INSTALLED");
      break;
    case ServiceWorkerVersion::ACTIVATING:
      info->SetString("status", "ACTIVATING");
      break;
    case ServiceWorkerVersion::ACTIVATED:
      info->SetString("status", "ACTIVATED");
      break;
    case ServiceWorkerVersion::REDUNDANT:
      info->SetString("status", "REDUNDANT");
      break;
  }
  info->SetString("script_url", version.script_url.spec());
  info->SetString("version_id", base::Int64ToString(version.version_id));
  info->SetInteger("process_id",
                   static_cast<int>(GetRealProcessId(version.process_id)));
  info->SetInteger("process_host_id", version.process_id);
  info->SetInteger("thread_id", version.thread_id);
  info->SetInteger("devtools_agent_route_id", version.devtools_agent_route_id);
}

ListValue* GetRegistrationListValue(
    const std::vector<ServiceWorkerRegistrationInfo>& registrations) {
  ListValue* result = new ListValue();
  for (std::vector<ServiceWorkerRegistrationInfo>::const_iterator it =
           registrations.begin();
       it != registrations.end();
       ++it) {
    const ServiceWorkerRegistrationInfo& registration = *it;
    std::unique_ptr<class base::DictionaryValue> registration_info(
        new DictionaryValue());
    registration_info->SetString("scope", registration.pattern.spec());
    registration_info->SetString(
        "registration_id", base::Int64ToString(registration.registration_id));

    if (registration.active_version.version_id !=
        kInvalidServiceWorkerVersionId) {
      DictionaryValue* active_info = new DictionaryValue();
      UpdateVersionInfo(registration.active_version, active_info);
      registration_info->Set("active", active_info);
    }

    if (registration.waiting_version.version_id !=
        kInvalidServiceWorkerVersionId) {
      DictionaryValue* waiting_info = new DictionaryValue();
      UpdateVersionInfo(registration.waiting_version, waiting_info);
      registration_info->Set("waiting", waiting_info);
    }

    result->Append(std::move(registration_info));
  }
  return result;
}

ListValue* GetVersionListValue(
    const std::vector<ServiceWorkerVersionInfo>& versions) {
  ListValue* result = new ListValue();
  for (std::vector<ServiceWorkerVersionInfo>::const_iterator it =
           versions.begin();
       it != versions.end();
       ++it) {
    DictionaryValue* info = new DictionaryValue();
    UpdateVersionInfo(*it, info);
    result->Append(info);
  }
  return result;
}

void DidGetStoredRegistrationsOnIOThread(
    scoped_refptr<ServiceWorkerContextWrapper> context,
    const GetRegistrationsCallback& callback,
    ServiceWorkerStatusCode status,
    const std::vector<ServiceWorkerRegistrationInfo>& stored_registrations) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(callback, context->GetAllLiveRegistrationInfo(),
                 context->GetAllLiveVersionInfo(), stored_registrations));
}

void GetRegistrationsOnIOThread(
    scoped_refptr<ServiceWorkerContextWrapper> context,
    const GetRegistrationsCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  context->GetAllRegistrations(
      base::Bind(DidGetStoredRegistrationsOnIOThread, context, callback));
}

void DidGetRegistrations(
    WeakPtr<ServiceWorkerInternalsUI> internals,
    int partition_id,
    const base::FilePath& context_path,
    const std::vector<ServiceWorkerRegistrationInfo>& live_registrations,
    const std::vector<ServiceWorkerVersionInfo>& live_versions,
    const std::vector<ServiceWorkerRegistrationInfo>& stored_registrations) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!internals)
    return;

  ScopedVector<const Value> args;
  args.push_back(GetRegistrationListValue(live_registrations));
  args.push_back(GetVersionListValue(live_versions));
  args.push_back(GetRegistrationListValue(stored_registrations));
  args.push_back(new FundamentalValue(partition_id));
  args.push_back(new StringValue(context_path.value()));
  internals->web_ui()->CallJavascriptFunctionUnsafe(
      "serviceworker.onPartitionData", args.get());
}

}  // namespace

class ServiceWorkerInternalsUI::PartitionObserver
    : public ServiceWorkerContextObserver {
 public:
  PartitionObserver(int partition_id, WebUI* web_ui)
      : partition_id_(partition_id), web_ui_(web_ui) {}
  ~PartitionObserver() override {}
  // ServiceWorkerContextObserver overrides:
  void OnRunningStateChanged(int64_t version_id,
                             EmbeddedWorkerStatus) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    web_ui_->CallJavascriptFunctionUnsafe(
        "serviceworker.onRunningStateChanged", FundamentalValue(partition_id_),
        StringValue(base::Int64ToString(version_id)));
  }
  void OnVersionStateChanged(int64_t version_id,
                             ServiceWorkerVersion::Status) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    web_ui_->CallJavascriptFunctionUnsafe(
        "serviceworker.onVersionStateChanged", FundamentalValue(partition_id_),
        StringValue(base::Int64ToString(version_id)));
  }
  void OnErrorReported(int64_t version_id,
                       int process_id,
                       int thread_id,
                       const ErrorInfo& info) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    ScopedVector<const Value> args;
    args.push_back(new FundamentalValue(partition_id_));
    args.push_back(new StringValue(base::Int64ToString(version_id)));
    args.push_back(new FundamentalValue(process_id));
    args.push_back(new FundamentalValue(thread_id));
    std::unique_ptr<DictionaryValue> value(new DictionaryValue());
    value->SetString("message", info.error_message);
    value->SetInteger("lineNumber", info.line_number);
    value->SetInteger("columnNumber", info.column_number);
    value->SetString("sourceURL", info.source_url.spec());
    args.push_back(value.release());
    web_ui_->CallJavascriptFunctionUnsafe("serviceworker.onErrorReported",
                                          args.get());
  }
  void OnReportConsoleMessage(int64_t version_id,
                              int process_id,
                              int thread_id,
                              const ConsoleMessage& message) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    ScopedVector<const Value> args;
    args.push_back(new FundamentalValue(partition_id_));
    args.push_back(new StringValue(base::Int64ToString(version_id)));
    args.push_back(new FundamentalValue(process_id));
    args.push_back(new FundamentalValue(thread_id));
    std::unique_ptr<DictionaryValue> value(new DictionaryValue());
    value->SetInteger("sourceIdentifier", message.source_identifier);
    value->SetInteger("message_level", message.message_level);
    value->SetString("message", message.message);
    value->SetInteger("lineNumber", message.line_number);
    value->SetString("sourceURL", message.source_url.spec());
    args.push_back(value.release());
    web_ui_->CallJavascriptFunctionUnsafe(
        "serviceworker.onConsoleMessageReported", args.get());
  }
  void OnRegistrationStored(int64_t registration_id,
                            const GURL& pattern) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    web_ui_->CallJavascriptFunctionUnsafe("serviceworker.onRegistrationStored",
                                          StringValue(pattern.spec()));
  }
  void OnRegistrationDeleted(int64_t registration_id,
                             const GURL& pattern) override {
    web_ui_->CallJavascriptFunctionUnsafe("serviceworker.onRegistrationDeleted",
                                          StringValue(pattern.spec()));
  }
  int partition_id() const { return partition_id_; }

 private:
  const int partition_id_;
  WebUI* const web_ui_;
};

ServiceWorkerInternalsUI::ServiceWorkerInternalsUI(WebUI* web_ui)
    : WebUIController(web_ui), next_partition_id_(0) {
  WebUIDataSource* source =
      WebUIDataSource::Create(kChromeUIServiceWorkerInternalsHost);
  source->SetJsonPath("strings.js");
  source->AddResourcePath("serviceworker_internals.js",
                          IDR_SERVICE_WORKER_INTERNALS_JS);
  source->AddResourcePath("serviceworker_internals.css",
                          IDR_SERVICE_WORKER_INTERNALS_CSS);
  source->SetDefaultResource(IDR_SERVICE_WORKER_INTERNALS_HTML);
  source->DisableDenyXFrameOptions();

  BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  WebUIDataSource::Add(browser_context, source);

  web_ui->RegisterMessageCallback(
      "GetOptions",
      base::Bind(&ServiceWorkerInternalsUI::GetOptions,
                 base::Unretained(this)));
  web_ui->RegisterMessageCallback(
      "SetOption",
      base::Bind(&ServiceWorkerInternalsUI::SetOption, base::Unretained(this)));
  web_ui->RegisterMessageCallback(
      "getAllRegistrations",
      base::Bind(&ServiceWorkerInternalsUI::GetAllRegistrations,
                 base::Unretained(this)));
  web_ui->RegisterMessageCallback(
      "stop",
      base::Bind(&ServiceWorkerInternalsUI::CallServiceWorkerVersionMethod,
                 base::Unretained(this),
                 &ServiceWorkerVersion::StopWorker));
  web_ui->RegisterMessageCallback(
      "inspect",
      base::Bind(&ServiceWorkerInternalsUI::InspectWorker,
                 base::Unretained(this)));
  web_ui->RegisterMessageCallback(
      "unregister",
      base::Bind(&ServiceWorkerInternalsUI::Unregister,
                 base::Unretained(this)));
  web_ui->RegisterMessageCallback(
      "start",
      base::Bind(&ServiceWorkerInternalsUI::StartWorker,
                 base::Unretained(this)));
}

ServiceWorkerInternalsUI::~ServiceWorkerInternalsUI() {
  BrowserContext* browser_context =
      web_ui()->GetWebContents()->GetBrowserContext();
  // Safe to use base::Unretained(this) because
  // ForEachStoragePartition is synchronous.
  BrowserContext::StoragePartitionCallback remove_observer_cb =
      base::Bind(&ServiceWorkerInternalsUI::RemoveObserverFromStoragePartition,
                 base::Unretained(this));
  BrowserContext::ForEachStoragePartition(browser_context, remove_observer_cb);
}

void ServiceWorkerInternalsUI::GetOptions(const ListValue* args) {
  DictionaryValue options;
  options.SetBoolean("debug_on_start",
                     ServiceWorkerDevToolsManager::GetInstance()
                         ->debug_service_worker_on_start());
  web_ui()->CallJavascriptFunctionUnsafe("serviceworker.onOptions", options);
}

void ServiceWorkerInternalsUI::SetOption(const ListValue* args) {
  std::string option_name;
  bool option_boolean;
  if (!args->GetString(0, &option_name) || option_name != "debug_on_start" ||
      !args->GetBoolean(1, &option_boolean)) {
    return;
  }
  ServiceWorkerDevToolsManager::GetInstance()
      ->set_debug_service_worker_on_start(option_boolean);
}

void ServiceWorkerInternalsUI::GetAllRegistrations(const ListValue* args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserContext* browser_context =
      web_ui()->GetWebContents()->GetBrowserContext();
  // Safe to use base::Unretained(this) because
  // ForEachStoragePartition is synchronous.
  BrowserContext::StoragePartitionCallback add_context_cb =
      base::Bind(&ServiceWorkerInternalsUI::AddContextFromStoragePartition,
                 base::Unretained(this));
  BrowserContext::ForEachStoragePartition(browser_context, add_context_cb);
}

void ServiceWorkerInternalsUI::AddContextFromStoragePartition(
    StoragePartition* partition) {
  int partition_id = 0;
  scoped_refptr<ServiceWorkerContextWrapper> context =
      static_cast<ServiceWorkerContextWrapper*>(
          partition->GetServiceWorkerContext());
  if (PartitionObserver* observer =
          observers_.get(reinterpret_cast<uintptr_t>(partition))) {
    partition_id = observer->partition_id();
  } else {
    partition_id = next_partition_id_++;
    std::unique_ptr<PartitionObserver> new_observer(
        new PartitionObserver(partition_id, web_ui()));
    context->AddObserver(new_observer.get());
    observers_.set(reinterpret_cast<uintptr_t>(partition),
                   std::move(new_observer));
  }

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(GetRegistrationsOnIOThread, context,
                 base::Bind(DidGetRegistrations, AsWeakPtr(), partition_id,
                            context->is_incognito() ? base::FilePath()
                                                    : partition->GetPath())));
}

void ServiceWorkerInternalsUI::RemoveObserverFromStoragePartition(
    StoragePartition* partition) {
  std::unique_ptr<PartitionObserver> observer(
      observers_.take_and_erase(reinterpret_cast<uintptr_t>(partition)));
  if (!observer.get())
    return;
  scoped_refptr<ServiceWorkerContextWrapper> context =
      static_cast<ServiceWorkerContextWrapper*>(
          partition->GetServiceWorkerContext());
  context->RemoveObserver(observer.get());
}

void ServiceWorkerInternalsUI::FindContext(
    int partition_id,
    StoragePartition** result_partition,
    StoragePartition* storage_partition) const {
  PartitionObserver* observer =
      observers_.get(reinterpret_cast<uintptr_t>(storage_partition));
  if (observer && partition_id == observer->partition_id()) {
    *result_partition = storage_partition;
  }
}

bool ServiceWorkerInternalsUI::GetServiceWorkerContext(
    int partition_id,
    scoped_refptr<ServiceWorkerContextWrapper>* context) const {
  BrowserContext* browser_context =
      web_ui()->GetWebContents()->GetBrowserContext();
  StoragePartition* result_partition(NULL);
  BrowserContext::StoragePartitionCallback find_context_cb =
      base::Bind(&ServiceWorkerInternalsUI::FindContext,
                 base::Unretained(this),
                 partition_id,
                 &result_partition);
  BrowserContext::ForEachStoragePartition(browser_context, find_context_cb);
  if (!result_partition)
    return false;
  *context = static_cast<ServiceWorkerContextWrapper*>(
      result_partition->GetServiceWorkerContext());
  return true;
}

void ServiceWorkerInternalsUI::CallServiceWorkerVersionMethod(
    ServiceWorkerVersionMethod method,
    const ListValue* args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  int callback_id;
  const DictionaryValue* cmd_args = NULL;
  int partition_id;
  scoped_refptr<ServiceWorkerContextWrapper> context;
  std::string version_id_string;
  int64_t version_id = 0;
  if (!args->GetInteger(0, &callback_id) ||
      !args->GetDictionary(1, &cmd_args) ||
      !cmd_args->GetInteger("partition_id", &partition_id) ||
      !GetServiceWorkerContext(partition_id, &context) ||
      !cmd_args->GetString("version_id", &version_id_string) ||
      !base::StringToInt64(version_id_string, &version_id)) {
    return;
  }

  base::Callback<void(ServiceWorkerStatusCode)> callback =
      base::Bind(OperationCompleteCallback, AsWeakPtr(), callback_id);
  CallServiceWorkerVersionMethodWithVersionID(
      method, context, version_id, callback);
}

void ServiceWorkerInternalsUI::InspectWorker(const ListValue* args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  int callback_id;
  const DictionaryValue* cmd_args = NULL;
  int process_host_id = 0;
  int devtools_agent_route_id = 0;
  if (!args->GetInteger(0, &callback_id) ||
      !args->GetDictionary(1, &cmd_args) ||
      !cmd_args->GetInteger("process_host_id", &process_host_id) ||
      !cmd_args->GetInteger("devtools_agent_route_id",
                            &devtools_agent_route_id)) {
    return;
  }
  base::Callback<void(ServiceWorkerStatusCode)> callback =
      base::Bind(OperationCompleteCallback, AsWeakPtr(), callback_id);
  scoped_refptr<DevToolsAgentHostImpl> agent_host(
      ServiceWorkerDevToolsManager::GetInstance()
          ->GetDevToolsAgentHostForWorker(process_host_id,
                                          devtools_agent_route_id));
  if (!agent_host.get()) {
    callback.Run(SERVICE_WORKER_ERROR_NOT_FOUND);
    return;
  }
  agent_host->Inspect(web_ui()->GetWebContents()->GetBrowserContext());
  callback.Run(SERVICE_WORKER_OK);
}

void ServiceWorkerInternalsUI::Unregister(const ListValue* args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  int callback_id;
  int partition_id;
  std::string scope_string;
  const DictionaryValue* cmd_args = NULL;
  scoped_refptr<ServiceWorkerContextWrapper> context;
  if (!args->GetInteger(0, &callback_id) ||
      !args->GetDictionary(1, &cmd_args) ||
      !cmd_args->GetInteger("partition_id", &partition_id) ||
      !GetServiceWorkerContext(partition_id, &context) ||
      !cmd_args->GetString("scope", &scope_string)) {
    return;
  }

  base::Callback<void(ServiceWorkerStatusCode)> callback =
      base::Bind(OperationCompleteCallback, AsWeakPtr(), callback_id);
  UnregisterWithScope(context, GURL(scope_string), callback);
}

void ServiceWorkerInternalsUI::StartWorker(const ListValue* args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  int callback_id;
  int partition_id;
  std::string scope_string;
  const DictionaryValue* cmd_args = NULL;
  scoped_refptr<ServiceWorkerContextWrapper> context;
  if (!args->GetInteger(0, &callback_id) ||
      !args->GetDictionary(1, &cmd_args) ||
      !cmd_args->GetInteger("partition_id", &partition_id) ||
      !GetServiceWorkerContext(partition_id, &context) ||
      !cmd_args->GetString("scope", &scope_string)) {
    return;
  }
  base::Callback<void(ServiceWorkerStatusCode)> callback =
      base::Bind(OperationCompleteCallback, AsWeakPtr(), callback_id);
  context->StartServiceWorker(GURL(scope_string), callback);
}

void ServiceWorkerInternalsUI::UnregisterWithScope(
    scoped_refptr<ServiceWorkerContextWrapper> context,
    const GURL& scope,
    const ServiceWorkerInternalsUI::StatusCallback& callback) const {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ServiceWorkerInternalsUI::UnregisterWithScope,
                   base::Unretained(this), context, scope, callback));
    return;
  }

  if (!context->context()) {
    callback.Run(SERVICE_WORKER_ERROR_ABORT);
    return;
  }

  // ServiceWorkerContextWrapper::UnregisterServiceWorker doesn't work here
  // because that reduces a status code to boolean.
  context->context()->UnregisterServiceWorker(scope, callback);
}

}  // namespace content
