// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_fetch_dispatcher.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/resource_message_filter.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/browser/loader/url_loader_factory_impl.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/service_worker/fetch_event_dispatcher.mojom.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "net/http/http_util.h"
#include "net/log/net_log.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_event_type.h"
#include "net/url_request/url_request.h"

namespace content {

namespace {

// This class wraps a mojo::AssociatedInterfacePtr<URLLoader>. It also is a
// URLLoader implementation and delegates URLLoader calls to the wrapped loader.
class DelegatingURLLoader final : public mojom::URLLoader {
 public:
  explicit DelegatingURLLoader(mojom::URLLoaderAssociatedPtr loader)
      : binding_(this), loader_(std::move(loader)) {}
  ~DelegatingURLLoader() override {}

  void FollowRedirect() override { loader_->FollowRedirect(); }

  mojom::URLLoaderPtr CreateInterfacePtrAndBind() {
    auto p = binding_.CreateInterfacePtrAndBind();
    // This unretained pointer is safe, because |binding_| is owned by |this|
    // and the callback will never be called after |this| is destroyed.
    binding_.set_connection_error_handler(
        base::Bind(&DelegatingURLLoader::Cancel, base::Unretained(this)));
    return p;
  }

 private:
  // Called when the mojom::URLLoaderPtr in the service worker is deleted.
  void Cancel() {
    // Cancel loading as stated in url_loader.mojom.
    loader_ = nullptr;
  }

  mojo::Binding<mojom::URLLoader> binding_;
  mojom::URLLoaderAssociatedPtr loader_;

  DISALLOW_COPY_AND_ASSIGN(DelegatingURLLoader);
};

// This class wraps a mojo::InterfacePtr<URLLoaderClient>. It also is a
// URLLoaderClient implementation and delegates URLLoaderClient calls to the
// wrapped client.
class DelegatingURLLoaderClient final : public mojom::URLLoaderClient {
 public:
  explicit DelegatingURLLoaderClient(mojom::URLLoaderClientPtr client)
      : binding_(this), client_(std::move(client)) {}
  ~DelegatingURLLoaderClient() override {}

  void OnDataDownloaded(int64_t data_length, int64_t encoded_length) override {
    client_->OnDataDownloaded(data_length, encoded_length);
  }
  void OnReceiveResponse(const ResourceResponseHead& head) override {
    client_->OnReceiveResponse(head);
  }
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         const ResourceResponseHead& head) override {
    client_->OnReceiveRedirect(redirect_info, head);
  }
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override {
    client_->OnStartLoadingResponseBody(std::move(body));
  }
  void OnComplete(
      const ResourceRequestCompletionStatus& completion_status) override {
    client_->OnComplete(completion_status);
  }

  void Bind(mojom::URLLoaderClientAssociatedPtrInfo* ptr_info,
            mojo::AssociatedGroup* associated_group) {
    binding_.Bind(ptr_info, associated_group);
  }

 private:
  mojo::AssociatedBinding<mojom::URLLoaderClient> binding_;
  mojom::URLLoaderClientPtr client_;

  DISALLOW_COPY_AND_ASSIGN(DelegatingURLLoaderClient);
};

using EventType = ServiceWorkerMetrics::EventType;
EventType ResourceTypeToEventType(ResourceType resource_type) {
  switch (resource_type) {
    case RESOURCE_TYPE_MAIN_FRAME:
      return EventType::FETCH_MAIN_FRAME;
    case RESOURCE_TYPE_SUB_FRAME:
      return EventType::FETCH_SUB_FRAME;
    case RESOURCE_TYPE_SHARED_WORKER:
      return EventType::FETCH_SHARED_WORKER;
    case RESOURCE_TYPE_SERVICE_WORKER:
    case RESOURCE_TYPE_LAST_TYPE:
      NOTREACHED() << resource_type;
      return EventType::FETCH_SUB_RESOURCE;
    default:
      return EventType::FETCH_SUB_RESOURCE;
  }
}

std::unique_ptr<base::Value> NetLogServiceWorkerStatusCallback(
    ServiceWorkerStatusCode status,
    net::NetLogCaptureMode) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
  dict->SetString("status", ServiceWorkerStatusToString(status));
  return std::move(dict);
}

std::unique_ptr<base::Value> NetLogFetchEventCallback(
    ServiceWorkerStatusCode status,
    ServiceWorkerFetchEventResult result,
    net::NetLogCaptureMode) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
  dict->SetString("status", ServiceWorkerStatusToString(status));
  dict->SetBoolean("has_response",
                   result == SERVICE_WORKER_FETCH_EVENT_RESULT_RESPONSE);
  return std::move(dict);
}

void EndNetLogEventWithServiceWorkerStatus(const net::NetLogWithSource& net_log,
                                           net::NetLogEventType type,
                                           ServiceWorkerStatusCode status) {
  net_log.EndEvent(type,
                   base::Bind(&NetLogServiceWorkerStatusCallback, status));
}

ServiceWorkerMetrics::EventType FetchTypeToWaitUntilEventType(
    ServiceWorkerFetchType type) {
  if (type == ServiceWorkerFetchType::FOREIGN_FETCH)
    return ServiceWorkerMetrics::EventType::FOREIGN_FETCH_WAITUNTIL;
  return ServiceWorkerMetrics::EventType::FETCH_WAITUNTIL;
}

void OnFetchEventFinished(
    ServiceWorkerVersion* version,
    int event_finish_id,
    mojom::URLLoaderFactoryPtr url_loader_factory,
    std::unique_ptr<mojom::URLLoader> url_loader,
    std::unique_ptr<mojom::URLLoaderClient> url_loader_client,
    ServiceWorkerStatusCode status,
    base::Time dispatch_event_time) {
  version->FinishRequest(event_finish_id, status != SERVICE_WORKER_ERROR_ABORT,
                         dispatch_event_time);
}

}  // namespace

// Helper to receive the fetch event response even if
// ServiceWorkerFetchDispatcher has been destroyed.
class ServiceWorkerFetchDispatcher::ResponseCallback {
 public:
  ResponseCallback(base::WeakPtr<ServiceWorkerFetchDispatcher> fetch_dispatcher,
                   ServiceWorkerVersion* version)
      : fetch_dispatcher_(fetch_dispatcher), version_(version) {}

  void Run(int request_id,
           ServiceWorkerFetchEventResult fetch_result,
           const ServiceWorkerResponse& response,
           base::Time dispatch_event_time) {
    const bool handled =
        (fetch_result == SERVICE_WORKER_FETCH_EVENT_RESULT_RESPONSE);
    if (!version_->FinishRequest(request_id, handled, dispatch_event_time))
      NOTREACHED() << "Should only receive one reply per event";

    // |fetch_dispatcher| is null if the URLRequest was killed.
    if (fetch_dispatcher_)
      fetch_dispatcher_->DidFinish(request_id, fetch_result, response);
  }

 private:
  base::WeakPtr<ServiceWorkerFetchDispatcher> fetch_dispatcher_;
  // Owns |this|.
  ServiceWorkerVersion* version_;

  DISALLOW_COPY_AND_ASSIGN(ResponseCallback);
};

ServiceWorkerFetchDispatcher::ServiceWorkerFetchDispatcher(
    std::unique_ptr<ServiceWorkerFetchRequest> request,
    ServiceWorkerVersion* version,
    ResourceType resource_type,
    const net::NetLogWithSource& net_log,
    const base::Closure& prepare_callback,
    const FetchCallback& fetch_callback)
    : version_(version),
      net_log_(net_log),
      prepare_callback_(prepare_callback),
      fetch_callback_(fetch_callback),
      request_(std::move(request)),
      resource_type_(resource_type),
      did_complete_(false),
      weak_factory_(this) {
  net_log_.BeginEvent(net::NetLogEventType::SERVICE_WORKER_DISPATCH_FETCH_EVENT,
                      net::NetLog::StringCallback(
                          "event_type", ServiceWorkerMetrics::EventTypeToString(
                                            GetEventType())));
}

ServiceWorkerFetchDispatcher::~ServiceWorkerFetchDispatcher() {
  if (!did_complete_)
    net_log_.EndEvent(
        net::NetLogEventType::SERVICE_WORKER_DISPATCH_FETCH_EVENT);
}

void ServiceWorkerFetchDispatcher::Run() {
  DCHECK(version_->status() == ServiceWorkerVersion::ACTIVATING ||
         version_->status() == ServiceWorkerVersion::ACTIVATED)
      << version_->status();

  if (version_->status() == ServiceWorkerVersion::ACTIVATING) {
    net_log_.BeginEvent(
        net::NetLogEventType::SERVICE_WORKER_WAIT_FOR_ACTIVATION);
    version_->RegisterStatusChangeCallback(
        base::Bind(&ServiceWorkerFetchDispatcher::DidWaitForActivation,
                   weak_factory_.GetWeakPtr()));
    return;
  }
  StartWorker();
}

void ServiceWorkerFetchDispatcher::DidWaitForActivation() {
  net_log_.EndEvent(net::NetLogEventType::SERVICE_WORKER_WAIT_FOR_ACTIVATION);
  StartWorker();
}

void ServiceWorkerFetchDispatcher::StartWorker() {
  // We might be REDUNDANT if a new worker started activating and kicked us out
  // before we could finish activation.
  if (version_->status() != ServiceWorkerVersion::ACTIVATED) {
    DCHECK_EQ(ServiceWorkerVersion::REDUNDANT, version_->status());
    DidFail(SERVICE_WORKER_ERROR_ACTIVATE_WORKER_FAILED);
    return;
  }

  if (version_->running_status() == EmbeddedWorkerStatus::RUNNING) {
    DispatchFetchEvent();
    return;
  }

  net_log_.BeginEvent(net::NetLogEventType::SERVICE_WORKER_START_WORKER);
  version_->RunAfterStartWorker(
      GetEventType(), base::Bind(&ServiceWorkerFetchDispatcher::DidStartWorker,
                                 weak_factory_.GetWeakPtr()),
      base::Bind(&ServiceWorkerFetchDispatcher::DidFailToStartWorker,
                 weak_factory_.GetWeakPtr()));
}

void ServiceWorkerFetchDispatcher::DidStartWorker() {
  net_log_.EndEvent(net::NetLogEventType::SERVICE_WORKER_START_WORKER);
  DispatchFetchEvent();
}

void ServiceWorkerFetchDispatcher::DidFailToStartWorker(
    ServiceWorkerStatusCode status) {
  EndNetLogEventWithServiceWorkerStatus(
      net_log_, net::NetLogEventType::SERVICE_WORKER_START_WORKER, status);
  DidFail(status);
}

void ServiceWorkerFetchDispatcher::DispatchFetchEvent() {
  DCHECK_EQ(EmbeddedWorkerStatus::RUNNING, version_->running_status())
      << "Worker stopped too soon after it was started.";

  DCHECK(!prepare_callback_.is_null());
  base::Closure prepare_callback = prepare_callback_;
  prepare_callback.Run();

  net_log_.BeginEvent(net::NetLogEventType::SERVICE_WORKER_FETCH_EVENT);
  int fetch_event_id = version_->StartRequest(
      GetEventType(),
      base::Bind(&ServiceWorkerFetchDispatcher::DidFailToDispatch,
                 weak_factory_.GetWeakPtr()));
  int event_finish_id = version_->StartRequest(
      FetchTypeToWaitUntilEventType(request_->fetch_type),
      base::Bind(&ServiceWorkerUtils::NoOpStatusCallback));

  ResponseCallback* response_callback =
      new ResponseCallback(weak_factory_.GetWeakPtr(), version_.get());
  version_->RegisterRequestCallback<ServiceWorkerHostMsg_FetchEventResponse>(
      fetch_event_id,
      base::Bind(&ServiceWorkerFetchDispatcher::ResponseCallback::Run,
                 base::Owned(response_callback)));

  base::WeakPtr<mojom::FetchEventDispatcher> dispatcher =
      version_->GetMojoServiceForRequest<mojom::FetchEventDispatcher>(
          event_finish_id);
  // |dispatcher| is owned by |version_|. So it is safe to pass the unretained
  // raw pointer of |version_| to OnFetchEventFinished callback.
  // Pass |url_loader_factory_|, |url_Loader_| and |url_loader_client_| to the
  // callback to keep them alive while the FetchEvent is onging in the service
  // worker.
  dispatcher->DispatchFetchEvent(
      fetch_event_id, *request_, std::move(preload_handle_),
      base::Bind(&OnFetchEventFinished, base::Unretained(version_.get()),
                 event_finish_id, base::Passed(std::move(url_loader_factory_)),
                 base::Passed(std::move(url_loader_)),
                 base::Passed(std::move(url_loader_client_))));
}

void ServiceWorkerFetchDispatcher::DidFailToDispatch(
    ServiceWorkerStatusCode status) {
  EndNetLogEventWithServiceWorkerStatus(
      net_log_, net::NetLogEventType::SERVICE_WORKER_FETCH_EVENT, status);
  DidFail(status);
}

void ServiceWorkerFetchDispatcher::DidFail(ServiceWorkerStatusCode status) {
  DCHECK_NE(SERVICE_WORKER_OK, status);
  Complete(status, SERVICE_WORKER_FETCH_EVENT_RESULT_FALLBACK,
           ServiceWorkerResponse());
}

void ServiceWorkerFetchDispatcher::DidFinish(
    int request_id,
    ServiceWorkerFetchEventResult fetch_result,
    const ServiceWorkerResponse& response) {
  net_log_.EndEvent(net::NetLogEventType::SERVICE_WORKER_FETCH_EVENT);
  Complete(SERVICE_WORKER_OK, fetch_result, response);
}

void ServiceWorkerFetchDispatcher::Complete(
    ServiceWorkerStatusCode status,
    ServiceWorkerFetchEventResult fetch_result,
    const ServiceWorkerResponse& response) {
  DCHECK(!fetch_callback_.is_null());

  did_complete_ = true;
  net_log_.EndEvent(
      net::NetLogEventType::SERVICE_WORKER_DISPATCH_FETCH_EVENT,
      base::Bind(&NetLogFetchEventCallback, status, fetch_result));

  FetchCallback fetch_callback = fetch_callback_;
  scoped_refptr<ServiceWorkerVersion> version = version_;
  fetch_callback.Run(status, fetch_result, response, version);
}

void ServiceWorkerFetchDispatcher::MaybeStartNavigationPreload(
    net::URLRequest* original_request) {
  if (resource_type_ != RESOURCE_TYPE_MAIN_FRAME &&
      resource_type_ != RESOURCE_TYPE_SUB_FRAME) {
    return;
  }
  if (!version_->navigation_preload_state().enabled)
    return;
  // TODO(horo): Currently NavigationPreload doesn't support request body.
  if (!request_->blob_uuid.empty())
    return;
  if (!base::FeatureList::IsEnabled(
          features::kServiceWorkerNavigationPreload)) {
    // TODO(horo): Check |version_|'s origin_trial_tokens() here if we use
    // Origin-Trial for NavigationPreload.
    return;
  }
  if (IsBrowserSideNavigationEnabled()) {
    // TODO(horo): Support NavigationPreload with PlzNavigate.
    NOTIMPLEMENTED();
    return;
  }

  DCHECK(!url_loader_factory_);
  const ResourceRequestInfoImpl* original_info =
      ResourceRequestInfoImpl::ForRequest(original_request);
  if (!original_info->filter())
    return;
  mojom::URLLoaderFactoryPtr factory;
  URLLoaderFactoryImpl::Create(original_info->filter(),
                               mojo::GetProxy(&url_loader_factory_));

  preload_handle_ = mojom::FetchEventPreloadHandle::New();

  ResourceRequest request;
  request.method = original_request->method();
  request.url = original_request->url();
  request.request_initiator = original_request->initiator();
  request.referrer = GURL(original_request->referrer());
  request.referrer_policy = original_info->GetReferrerPolicy();
  request.visibility_state = original_info->GetVisibilityState();
  request.load_flags = original_request->load_flags();
  // Set to SUB_RESOURCE because we shouldn't trigger NavigationResourceThrottle
  // for the service worker navigation preload request.
  request.resource_type = RESOURCE_TYPE_SUB_RESOURCE;
  request.priority = original_request->priority();
  request.skip_service_worker = SkipServiceWorker::ALL;
  request.do_not_prompt_for_login = true;
  request.render_frame_id = original_info->GetRenderFrameID();
  request.is_main_frame = original_info->IsMainFrame();
  request.parent_is_main_frame = original_info->ParentIsMainFrame();

  DCHECK(net::HttpUtil::IsValidHeaderValue(
      version_->navigation_preload_state().header));
  request.headers = "Service-Worker-Navigation-Preload: " +
                    version_->navigation_preload_state().header;

  const int request_id = ResourceDispatcherHostImpl::Get()->MakeRequestID();
  DCHECK_LT(request_id, -1);

  preload_handle_ = mojom::FetchEventPreloadHandle::New();
  mojom::URLLoaderClientPtr url_loader_client_ptr;
  preload_handle_->url_loader_client_request =
      mojo::GetProxy(&url_loader_client_ptr);
  auto url_loader_client = base::MakeUnique<DelegatingURLLoaderClient>(
      std::move(url_loader_client_ptr));
  mojom::URLLoaderClientAssociatedPtrInfo url_loader_client_associated_ptr_info;
  url_loader_client->Bind(&url_loader_client_associated_ptr_info,
                          url_loader_factory_.associated_group());
  mojom::URLLoaderAssociatedPtr url_loader_associated_ptr;

  url_loader_factory_->CreateLoaderAndStart(
      mojo::GetProxy(&url_loader_associated_ptr,
                     url_loader_factory_.associated_group()),
      original_info->GetRouteID(), request_id, request,
      std::move(url_loader_client_associated_ptr_info));

  std::unique_ptr<DelegatingURLLoader> url_loader(
      new DelegatingURLLoader(std::move(url_loader_associated_ptr)));
  preload_handle_->url_loader = url_loader->CreateInterfacePtrAndBind();
  url_loader_ = std::move(url_loader);
  url_loader_client_ = std::move(url_loader_client);
}

ServiceWorkerMetrics::EventType ServiceWorkerFetchDispatcher::GetEventType()
    const {
  if (request_->fetch_type == ServiceWorkerFetchType::FOREIGN_FETCH)
    return ServiceWorkerMetrics::EventType::FOREIGN_FETCH;
  return ResourceTypeToEventType(resource_type_);
}

}  // namespace content
