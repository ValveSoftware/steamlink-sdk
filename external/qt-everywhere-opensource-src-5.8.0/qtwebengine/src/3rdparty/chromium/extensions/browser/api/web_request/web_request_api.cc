// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/web_request/web_request_api.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/child_process_host.h"
#include "extensions/browser/api/activity_log/web_request_constants.h"
#include "extensions/browser/api/declarative/rules_registry_service.h"
#include "extensions/browser/api/declarative_webrequest/request_stage.h"
#include "extensions/browser/api/declarative_webrequest/webrequest_constants.h"
#include "extensions/browser/api/declarative_webrequest/webrequest_rules_registry.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/web_request/web_request_api_constants.h"
#include "extensions/browser/api/web_request/web_request_api_helpers.h"
#include "extensions/browser/api/web_request/web_request_event_details.h"
#include "extensions/browser/api/web_request/web_request_event_router_delegate.h"
#include "extensions/browser/api/web_request/web_request_time_tracker.h"
#include "extensions/browser/api_activity_monitor.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/guest_view/guest_view_events.h"
#include "extensions/browser/guest_view/web_view/web_view_constants.h"
#include "extensions/browser/guest_view/web_view/web_view_renderer_state.h"
#include "extensions/browser/info_map.h"
#include "extensions/browser/io_thread_extension_message_filter.h"
#include "extensions/browser/runtime_data.h"
#include "extensions/browser/warning_service.h"
#include "extensions/browser/warning_set.h"
#include "extensions/common/api/web_request.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/event_filtering_info.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/url_pattern.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "net/base/auth.h"
#include "net/base/net_errors.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

using content::BrowserThread;
using content::ResourceRequestInfo;
using extension_web_request_api_helpers::ExtraInfoSpec;

namespace activity_log = activity_log_web_request_constants;
namespace helpers = extension_web_request_api_helpers;
namespace keys = extension_web_request_api_constants;

namespace extensions {

namespace declarative_keys = declarative_webrequest_constants;
namespace web_request = api::web_request;

namespace {

const char kWebRequestEventPrefix[] = "webRequest.";

// List of all the webRequest events.
const char* const kWebRequestEvents[] = {
  keys::kOnBeforeRedirectEvent,
  web_request::OnBeforeRequest::kEventName,
  keys::kOnBeforeSendHeadersEvent,
  keys::kOnCompletedEvent,
  web_request::OnErrorOccurred::kEventName,
  keys::kOnSendHeadersEvent,
  keys::kOnAuthRequiredEvent,
  keys::kOnResponseStartedEvent,
  keys::kOnHeadersReceivedEvent,
};

const char* GetRequestStageAsString(
    ExtensionWebRequestEventRouter::EventTypes type) {
  switch (type) {
    case ExtensionWebRequestEventRouter::kInvalidEvent:
      return "Invalid";
    case ExtensionWebRequestEventRouter::kOnBeforeRequest:
      return keys::kOnBeforeRequest;
    case ExtensionWebRequestEventRouter::kOnBeforeSendHeaders:
      return keys::kOnBeforeSendHeaders;
    case ExtensionWebRequestEventRouter::kOnSendHeaders:
      return keys::kOnSendHeaders;
    case ExtensionWebRequestEventRouter::kOnHeadersReceived:
      return keys::kOnHeadersReceived;
    case ExtensionWebRequestEventRouter::kOnBeforeRedirect:
      return keys::kOnBeforeRedirect;
    case ExtensionWebRequestEventRouter::kOnAuthRequired:
      return keys::kOnAuthRequired;
    case ExtensionWebRequestEventRouter::kOnResponseStarted:
      return keys::kOnResponseStarted;
    case ExtensionWebRequestEventRouter::kOnErrorOccurred:
      return keys::kOnErrorOccurred;
    case ExtensionWebRequestEventRouter::kOnCompleted:
      return keys::kOnCompleted;
  }
  NOTREACHED();
  return "Not reached";
}

bool IsWebRequestEvent(const std::string& event_name) {
  std::string web_request_event_name(event_name);
  if (base::StartsWith(web_request_event_name,
                       webview::kWebViewEventPrefix,
                       base::CompareCase::SENSITIVE)) {
    web_request_event_name.replace(
        0, strlen(webview::kWebViewEventPrefix), kWebRequestEventPrefix);
  }
  auto* const* web_request_events_end =
      kWebRequestEvents + arraysize(kWebRequestEvents);
  return std::find(kWebRequestEvents, web_request_events_end,
                   web_request_event_name) != web_request_events_end;
}

// Returns whether |request| has been triggered by an extension in
// |extension_info_map|.
bool IsRequestFromExtension(const net::URLRequest* request,
                            const InfoMap* extension_info_map) {
  // |extension_info_map| is NULL for system-level requests.
  if (!extension_info_map)
    return false;

  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request);

  // If this request was not created by the ResourceDispatcher, |info| is NULL.
  // All requests from extensions are created by the ResourceDispatcher.
  if (!info)
    return false;

  const std::set<std::string> extension_ids =
      extension_info_map->process_map().GetExtensionsInProcess(
          info->GetChildID());
  if (extension_ids.empty())
    return false;

  // Treat hosted apps as normal web pages (crbug.com/526413).
  for (const std::string& extension_id : extension_ids) {
    const Extension* extension =
        extension_info_map->extensions().GetByID(extension_id);
    if (extension && !extension->is_hosted_app())
      return true;
  }
  return false;
}

void ExtractRequestRoutingInfo(const net::URLRequest* request,
                               int* render_process_host_id,
                               int* routing_id) {
  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request);
  if (!info)
    return;
  *render_process_host_id = info->GetChildID();
  *routing_id = info->GetRouteID();
}

// Given a |request|, this function determines whether it originated from
// a <webview> guest process or not. If it is from a <webview> guest process,
// then |web_view_info| is returned with information about the instance ID
// that uniquely identifies the <webview> and its embedder.
bool GetWebViewInfo(const net::URLRequest* request,
                    WebViewRendererState::WebViewInfo* web_view_info) {
  int render_process_host_id = -1;
  int routing_id = -1;
  ExtractRequestRoutingInfo(request, &render_process_host_id, &routing_id);
  return WebViewRendererState::GetInstance()->GetInfo(
      render_process_host_id, routing_id, web_view_info);
}

// Converts a HttpHeaders dictionary to a |name|, |value| pair. Returns
// true if successful.
bool FromHeaderDictionary(const base::DictionaryValue* header_value,
                          std::string* name,
                          std::string* value) {
  if (!header_value->GetString(keys::kHeaderNameKey, name))
    return false;

  // We require either a "value" or a "binaryValue" entry.
  if (!(header_value->HasKey(keys::kHeaderValueKey) ^
        header_value->HasKey(keys::kHeaderBinaryValueKey))) {
    return false;
  }

  if (header_value->HasKey(keys::kHeaderValueKey)) {
    if (!header_value->GetString(keys::kHeaderValueKey, value)) {
      return false;
    }
  } else if (header_value->HasKey(keys::kHeaderBinaryValueKey)) {
    const base::ListValue* list = NULL;
    if (!header_value->HasKey(keys::kHeaderBinaryValueKey)) {
      *value = "";
    } else if (!header_value->GetList(keys::kHeaderBinaryValueKey, &list) ||
               !helpers::CharListToString(list, value)) {
      return false;
    }
  }
  return true;
}

// Sends an event to subscribers of chrome.declarativeWebRequest.onMessage or
// to subscribers of webview.onMessage if the action is being operated upon
// a <webview> guest renderer.
// |extension_id| identifies the extension that sends and receives the event.
// |is_web_view_guest| indicates whether the action is for a <webview>.
// |web_view_info| is a struct containing information about the <webview>
// embedder.
// |event_details| is passed to the event listener.
void SendOnMessageEventOnUI(
    void* browser_context_id,
    const std::string& extension_id,
    bool is_web_view_guest,
    const WebViewRendererState::WebViewInfo& web_view_info,
    std::unique_ptr<WebRequestEventDetails> event_details) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  content::BrowserContext* browser_context =
      reinterpret_cast<content::BrowserContext*>(browser_context_id);
  if (!ExtensionsBrowserClient::Get()->IsValidContext(browser_context))
    return;

  std::unique_ptr<base::ListValue> event_args(new base::ListValue);
  event_details->DetermineFrameDataOnUI();
  event_args->Append(event_details->GetAndClearDict());

  EventRouter* event_router = EventRouter::Get(browser_context);

  EventFilteringInfo event_filtering_info;

  events::HistogramValue histogram_value = events::UNKNOWN;
  std::string event_name;
  // The instance ID uniquely identifies a <webview> instance within an embedder
  // process. We use a filter here so that only event listeners for a particular
  // <webview> will fire.
  if (is_web_view_guest) {
    event_filtering_info.SetInstanceID(web_view_info.instance_id);
    histogram_value = events::WEB_VIEW_INTERNAL_ON_MESSAGE;
    event_name = webview::kEventMessage;
  } else {
    histogram_value = events::DECLARATIVE_WEB_REQUEST_ON_MESSAGE;
    event_name = declarative_keys::kOnMessage;
  }

  std::unique_ptr<Event> event(new Event(
      histogram_value, event_name, std::move(event_args), browser_context,
      GURL(), EventRouter::USER_GESTURE_UNKNOWN, event_filtering_info));
  event_router->DispatchEventToExtension(extension_id, std::move(event));
}

void RemoveEventListenerOnIOThread(
    void* browser_context,
    const std::string& extension_id,
    const std::string& sub_event_name,
    int embedder_process_id,
    int web_view_instance_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ExtensionWebRequestEventRouter::GetInstance()->RemoveEventListener(
      browser_context, extension_id, sub_event_name,
      embedder_process_id, web_view_instance_id);
}

events::HistogramValue GetEventHistogramValue(const std::string& event_name) {
  // Event names will either be webRequest events, or guest view (probably web
  // view) events that map to webRequest events. Check webRequest first.
  static struct ValueAndName {
    events::HistogramValue histogram_value;
    const char* const event_name;
  } values_and_names[] = {
      {events::WEB_REQUEST_ON_BEFORE_REDIRECT, keys::kOnBeforeRedirectEvent},
      {events::WEB_REQUEST_ON_BEFORE_REQUEST,
       web_request::OnBeforeRequest::kEventName},
      {events::WEB_REQUEST_ON_BEFORE_SEND_HEADERS,
       keys::kOnBeforeSendHeadersEvent},
      {events::WEB_REQUEST_ON_COMPLETED, keys::kOnCompletedEvent},
      {events::WEB_REQUEST_ON_ERROR_OCCURRED,
       web_request::OnErrorOccurred::kEventName},
      {events::WEB_REQUEST_ON_SEND_HEADERS, keys::kOnSendHeadersEvent},
      {events::WEB_REQUEST_ON_AUTH_REQUIRED, keys::kOnAuthRequiredEvent},
      {events::WEB_REQUEST_ON_RESPONSE_STARTED, keys::kOnResponseStartedEvent},
      {events::WEB_REQUEST_ON_HEADERS_RECEIVED, keys::kOnHeadersReceivedEvent}};
  static_assert(arraysize(kWebRequestEvents) == arraysize(values_and_names),
                "kWebRequestEvents and values_and_names must be the same");
  for (const ValueAndName& value_and_name : values_and_names) {
    if (value_and_name.event_name == event_name)
      return value_and_name.histogram_value;
  }

  // If there is no webRequest event, it might be a guest view webRequest event.
  events::HistogramValue guest_view_histogram_value =
      guest_view_events::GetEventHistogramValue(event_name);
  if (guest_view_histogram_value != events::UNKNOWN)
    return guest_view_histogram_value;

  // There is no histogram value for this event name. It should be added to
  // either the mapping here, or in guest_view_events.
  NOTREACHED() << "Event " << event_name << " must have a histogram value";
  return events::UNKNOWN;
}

// We hide events from the system context as well as sensitive requests.
bool ShouldHideEvent(void* browser_context,
                     const InfoMap* extension_info_map,
                     const net::URLRequest* request) {
  return (!browser_context ||
          WebRequestPermissions::HideRequest(extension_info_map, request));
}

}  // namespace

WebRequestAPI::WebRequestAPI(content::BrowserContext* context)
    : browser_context_(context) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  for (size_t i = 0; i < arraysize(kWebRequestEvents); ++i) {
    // Observe the webRequest event.
    std::string event_name = kWebRequestEvents[i];
    event_router->RegisterObserver(this, event_name);

    // Also observe the corresponding webview event.
    event_name.replace(
        0, sizeof(kWebRequestEventPrefix) - 1, webview::kWebViewEventPrefix);
    event_router->RegisterObserver(this, event_name);
  }
}

WebRequestAPI::~WebRequestAPI() {
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<WebRequestAPI> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<WebRequestAPI>*
WebRequestAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

void WebRequestAPI::OnListenerRemoved(const EventListenerInfo& details) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Note that details.event_name includes the sub-event details (e.g. "/123").
  // TODO(fsamuel): <webview> events will not be removed through this code path.
  // <webview> events will be removed in RemoveWebViewEventListeners. Ideally,
  // this code should be decoupled from extensions, we should use the host ID
  // instead, and not have two different code paths. This is a huge undertaking
  // unfortunately, so we'll resort to two code paths for now.
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(&RemoveEventListenerOnIOThread,
                                     details.browser_context,
                                     details.extension_id,
                                     details.event_name,
                                     0 /* embedder_process_id (ignored) */,
                                     0 /* web_view_instance_id */));
}

// Represents a single unique listener to an event, along with whatever filter
// parameters and extra_info_spec were specified at the time the listener was
// added.
// NOTE(benjhayden) New APIs should not use this sub_event_name trick! It does
// not play well with event pages. See downloads.onDeterminingFilename and
// ExtensionDownloadsEventRouter for an alternative approach.
struct ExtensionWebRequestEventRouter::EventListener {
  std::string extension_id;
  std::string extension_name;
  events::HistogramValue histogram_value;
  std::string sub_event_name;
  RequestFilter filter;
  int extra_info_spec;
  int embedder_process_id;
  int web_view_instance_id;
  base::WeakPtr<IPC::Sender> ipc_sender;
  mutable std::set<uint64_t> blocked_requests;

  // Comparator to work with std::set.
  bool operator<(const EventListener& that) const {
    if (extension_id != that.extension_id)
      return extension_id < that.extension_id;

    if (sub_event_name != that.sub_event_name)
      return sub_event_name < that.sub_event_name;

    if (web_view_instance_id != that.web_view_instance_id)
      return web_view_instance_id < that.web_view_instance_id;

    if (web_view_instance_id == 0) {
      // Do not filter by process ID for non-webviews, because this comparator
      // is also used to find and remove an event listener when an extension is
      // unloaded. At this point, the event listener cannot be mapped back to
      // the original process, so 0 is used instead of the actual process ID.
      if (embedder_process_id == 0 || that.embedder_process_id == 0)
        return false;
    }

    if (embedder_process_id != that.embedder_process_id)
      return embedder_process_id < that.embedder_process_id;

    return false;
  }

  EventListener()
      : histogram_value(events::UNKNOWN),
        extra_info_spec(0),
        embedder_process_id(0),
        web_view_instance_id(0) {}
};

// Contains info about requests that are blocked waiting for a response from
// an extension.
struct ExtensionWebRequestEventRouter::BlockedRequest {
  // The request that is being blocked.
  net::URLRequest* request;

  // Whether the request originates from an incognito tab.
  bool is_incognito;

  // The event that we're currently blocked on.
  EventTypes event;

  // The number of event handlers that we are awaiting a response from.
  int num_handlers_blocking;

  // Pointer to NetLog to report significant changes to the request for
  // debugging.
  const net::BoundNetLog* net_log;

  // The callback to call when we get a response from all event handlers.
  net::CompletionCallback callback;

  // If non-empty, this contains the new URL that the request will redirect to.
  // Only valid for OnBeforeRequest and OnHeadersReceived.
  GURL* new_url;

  // The request headers that will be issued along with this request. Only valid
  // for OnBeforeSendHeaders.
  net::HttpRequestHeaders* request_headers;

  // The response headers that were received from the server. Only valid for
  // OnHeadersReceived.
  scoped_refptr<const net::HttpResponseHeaders> original_response_headers;

  // Location where to override response headers. Only valid for
  // OnHeadersReceived.
  scoped_refptr<net::HttpResponseHeaders>* override_response_headers;

  // If non-empty, this contains the auth credentials that may be filled in.
  // Only valid for OnAuthRequired.
  net::AuthCredentials* auth_credentials;

  // The callback to invoke for auth. If |auth_callback.is_null()| is false,
  // |callback| must be NULL.
  // Only valid for OnAuthRequired.
  net::NetworkDelegate::AuthCallback auth_callback;

  // Time the request was paused. Used for logging purposes.
  base::Time blocking_time;

  // Changes requested by extensions.
  helpers::EventResponseDeltas response_deltas;

  // Provider of meta data about extensions, only used and non-NULL for events
  // that are delayed until the rules registry is ready.
  const InfoMap* extension_info_map;

  BlockedRequest()
      : request(NULL),
        is_incognito(false),
        event(kInvalidEvent),
        num_handlers_blocking(0),
        net_log(NULL),
        new_url(NULL),
        request_headers(NULL),
        override_response_headers(NULL),
        auth_credentials(NULL),
        extension_info_map(NULL) {}
};

bool ExtensionWebRequestEventRouter::RequestFilter::InitFromValue(
    const base::DictionaryValue& value, std::string* error) {
  if (!value.HasKey("urls"))
    return false;

  for (base::DictionaryValue::Iterator it(value); !it.IsAtEnd(); it.Advance()) {
    if (it.key() == "urls") {
      const base::ListValue* urls_value = NULL;
      if (!it.value().GetAsList(&urls_value))
        return false;
      for (size_t i = 0; i < urls_value->GetSize(); ++i) {
        std::string url;
        URLPattern pattern(
            URLPattern::SCHEME_HTTP | URLPattern::SCHEME_HTTPS |
            URLPattern::SCHEME_FTP | URLPattern::SCHEME_FILE |
            URLPattern::SCHEME_EXTENSION);
        if (!urls_value->GetString(i, &url) ||
            pattern.Parse(url) != URLPattern::PARSE_SUCCESS) {
          *error = ErrorUtils::FormatErrorMessage(
              keys::kInvalidRequestFilterUrl, url);
          return false;
        }
        urls.AddPattern(pattern);
      }
    } else if (it.key() == "types") {
      const base::ListValue* types_value = NULL;
      if (!it.value().GetAsList(&types_value))
        return false;
      for (size_t i = 0; i < types_value->GetSize(); ++i) {
        std::string type_str;
        if (!types_value->GetString(i, &type_str) ||
            !helpers::ParseResourceType(type_str, &types)) {
          return false;
        }
      }
    } else if (it.key() == "tabId") {
      if (!it.value().GetAsInteger(&tab_id))
        return false;
    } else if (it.key() == "windowId") {
      if (!it.value().GetAsInteger(&window_id))
        return false;
    } else {
      return false;
    }
  }
  return true;
}

ExtensionWebRequestEventRouter::EventResponse::EventResponse(
    const std::string& extension_id, const base::Time& extension_install_time)
    : extension_id(extension_id),
      extension_install_time(extension_install_time),
      cancel(false) {
}

ExtensionWebRequestEventRouter::EventResponse::~EventResponse() {
}

ExtensionWebRequestEventRouter::RequestFilter::RequestFilter()
    : tab_id(-1), window_id(-1) {
}

ExtensionWebRequestEventRouter::RequestFilter::RequestFilter(
    const RequestFilter& other) = default;

ExtensionWebRequestEventRouter::RequestFilter::~RequestFilter() {
}

//
// ExtensionWebRequestEventRouter
//

// static
ExtensionWebRequestEventRouter* ExtensionWebRequestEventRouter::GetInstance() {
  return base::Singleton<ExtensionWebRequestEventRouter>::get();
}

ExtensionWebRequestEventRouter::ExtensionWebRequestEventRouter()
    : request_time_tracker_(new ExtensionWebRequestTimeTracker),
      web_request_event_router_delegate_(
          ExtensionsAPIClient::Get()->CreateWebRequestEventRouterDelegate()) {}

ExtensionWebRequestEventRouter::~ExtensionWebRequestEventRouter() {
}

void ExtensionWebRequestEventRouter::RegisterRulesRegistry(
    void* browser_context,
    int rules_registry_id,
    scoped_refptr<WebRequestRulesRegistry> rules_registry) {
  RulesRegistryKey key(browser_context, rules_registry_id);
  if (rules_registry.get())
    rules_registries_[key] = rules_registry;
  else
    rules_registries_.erase(key);
}

std::unique_ptr<WebRequestEventDetails>
ExtensionWebRequestEventRouter::CreateEventDetails(
    const net::URLRequest* request,
    int extra_info_spec) {
  std::unique_ptr<WebRequestEventDetails> event_details(
      new WebRequestEventDetails(request, extra_info_spec));

  return event_details;
}

int ExtensionWebRequestEventRouter::OnBeforeRequest(
    void* browser_context,
    const InfoMap* extension_info_map,
    net::URLRequest* request,
    const net::CompletionCallback& callback,
    GURL* new_url) {
  if (ShouldHideEvent(browser_context, extension_info_map, request))
    return net::OK;

  if (IsPageLoad(request))
    NotifyPageLoad();

  request_time_tracker_->LogRequestStartTime(request->identifier(),
                                             base::Time::Now(),
                                             request->url(),
                                             browser_context);

  // Whether to initialized |blocked_requests_|.
  bool initialize_blocked_requests = false;

  initialize_blocked_requests |=
      ProcessDeclarativeRules(browser_context, extension_info_map,
                              web_request::OnBeforeRequest::kEventName, request,
                              ON_BEFORE_REQUEST, NULL);

  int extra_info_spec = 0;
  EventListeners listeners = GetMatchingListeners(
      browser_context, extension_info_map,
      web_request::OnBeforeRequest::kEventName, request, &extra_info_spec);
  if (!listeners.empty() &&
      !GetAndSetSignaled(request->identifier(), kOnBeforeRequest)) {
    std::unique_ptr<WebRequestEventDetails> event_details(
        CreateEventDetails(request, extra_info_spec));
    event_details->SetRequestBody(request);

    initialize_blocked_requests |= DispatchEvent(
        browser_context, request, listeners, std::move(event_details));
  }

  if (!initialize_blocked_requests)
    return net::OK;  // Nobody saw a reason for modifying the request.

  BlockedRequest& blocked_request = blocked_requests_[request->identifier()];
  blocked_request.event = kOnBeforeRequest;
  blocked_request.is_incognito |= IsIncognitoBrowserContext(browser_context);
  blocked_request.request = request;
  blocked_request.callback = callback;
  blocked_request.new_url = new_url;
  blocked_request.net_log = &request->net_log();

  if (blocked_request.num_handlers_blocking == 0) {
    // If there are no blocking handlers, only the declarative rules tried
    // to modify the request and we can respond synchronously.
    return ExecuteDeltas(browser_context, request->identifier(),
                         false /* call_callback*/);
  }
  return net::ERR_IO_PENDING;
}

int ExtensionWebRequestEventRouter::OnBeforeSendHeaders(
    void* browser_context,
    const InfoMap* extension_info_map,
    net::URLRequest* request,
    const net::CompletionCallback& callback,
    net::HttpRequestHeaders* headers) {
  if (ShouldHideEvent(browser_context, extension_info_map, request))
    return net::OK;

  bool initialize_blocked_requests = false;

  initialize_blocked_requests |= ProcessDeclarativeRules(
      browser_context, extension_info_map, keys::kOnBeforeSendHeadersEvent,
      request, ON_BEFORE_SEND_HEADERS, NULL);

  int extra_info_spec = 0;
  EventListeners listeners = GetMatchingListeners(
      browser_context, extension_info_map, keys::kOnBeforeSendHeadersEvent,
      request, &extra_info_spec);
  if (!listeners.empty() &&
      !GetAndSetSignaled(request->identifier(), kOnBeforeSendHeaders)) {
    std::unique_ptr<WebRequestEventDetails> event_details(
        CreateEventDetails(request, extra_info_spec));
    event_details->SetRequestHeaders(*headers);

    initialize_blocked_requests |= DispatchEvent(
        browser_context, request, listeners, std::move(event_details));
  }

  if (!initialize_blocked_requests)
    return net::OK;  // Nobody saw a reason for modifying the request.

  BlockedRequest& blocked_request = blocked_requests_[request->identifier()];
  blocked_request.event = kOnBeforeSendHeaders;
  blocked_request.is_incognito |= IsIncognitoBrowserContext(browser_context);
  blocked_request.request = request;
  blocked_request.callback = callback;
  blocked_request.request_headers = headers;
  blocked_request.net_log = &request->net_log();

  if (blocked_request.num_handlers_blocking == 0) {
    // If there are no blocking handlers, only the declarative rules tried
    // to modify the request and we can respond synchronously.
    return ExecuteDeltas(browser_context, request->identifier(),
                         false /* call_callback*/);
  }
  return net::ERR_IO_PENDING;
}

void ExtensionWebRequestEventRouter::OnSendHeaders(
    void* browser_context,
    const InfoMap* extension_info_map,
    net::URLRequest* request,
    const net::HttpRequestHeaders& headers) {
  if (ShouldHideEvent(browser_context, extension_info_map, request))
    return;

  if (GetAndSetSignaled(request->identifier(), kOnSendHeaders))
    return;

  ClearSignaled(request->identifier(), kOnBeforeRedirect);

  int extra_info_spec = 0;
  EventListeners listeners = GetMatchingListeners(
      browser_context, extension_info_map, keys::kOnSendHeadersEvent, request,
      &extra_info_spec);
  if (listeners.empty())
    return;

  std::unique_ptr<WebRequestEventDetails> event_details(
      CreateEventDetails(request, extra_info_spec));
  event_details->SetRequestHeaders(headers);

  DispatchEvent(browser_context, request, listeners, std::move(event_details));
}

int ExtensionWebRequestEventRouter::OnHeadersReceived(
    void* browser_context,
    const InfoMap* extension_info_map,
    net::URLRequest* request,
    const net::CompletionCallback& callback,
    const net::HttpResponseHeaders* original_response_headers,
    scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
    GURL* allowed_unsafe_redirect_url) {
  if (ShouldHideEvent(browser_context, extension_info_map, request))
    return net::OK;

  bool initialize_blocked_requests = false;

  initialize_blocked_requests |= ProcessDeclarativeRules(
      browser_context, extension_info_map, keys::kOnHeadersReceivedEvent,
      request, ON_HEADERS_RECEIVED, original_response_headers);

  int extra_info_spec = 0;
  EventListeners listeners = GetMatchingListeners(
      browser_context, extension_info_map, keys::kOnHeadersReceivedEvent,
      request, &extra_info_spec);

  if (!listeners.empty() &&
      !GetAndSetSignaled(request->identifier(), kOnHeadersReceived)) {
    std::unique_ptr<WebRequestEventDetails> event_details(
        CreateEventDetails(request, extra_info_spec));
    event_details->SetResponseHeaders(request, original_response_headers);

    initialize_blocked_requests |= DispatchEvent(
        browser_context, request, listeners, std::move(event_details));
  }

  if (!initialize_blocked_requests)
    return net::OK;  // Nobody saw a reason for modifying the request.

  BlockedRequest& blocked_request = blocked_requests_[request->identifier()];
  blocked_request.event = kOnHeadersReceived;
  blocked_request.is_incognito |= IsIncognitoBrowserContext(browser_context);
  blocked_request.request = request;
  blocked_request.callback = callback;
  blocked_request.net_log = &request->net_log();
  blocked_request.override_response_headers = override_response_headers;
  blocked_request.original_response_headers = original_response_headers;
  blocked_request.new_url = allowed_unsafe_redirect_url;

  if (blocked_request.num_handlers_blocking == 0) {
    // If there are no blocking handlers, only the declarative rules tried
    // to modify the request and we can respond synchronously.
    return ExecuteDeltas(browser_context, request->identifier(),
                         false /* call_callback*/);
  }
  return net::ERR_IO_PENDING;
}

net::NetworkDelegate::AuthRequiredResponse
ExtensionWebRequestEventRouter::OnAuthRequired(
    void* browser_context,
    const InfoMap* extension_info_map,
    net::URLRequest* request,
    const net::AuthChallengeInfo& auth_info,
    const net::NetworkDelegate::AuthCallback& callback,
    net::AuthCredentials* credentials) {
  // No browser_context means that this is for authentication challenges in the
  // system context. Skip in that case. Also skip sensitive requests.
  if (!browser_context ||
      WebRequestPermissions::HideRequest(extension_info_map, request)) {
    return net::NetworkDelegate::AUTH_REQUIRED_RESPONSE_NO_ACTION;
  }

  int extra_info_spec = 0;
  EventListeners listeners = GetMatchingListeners(
      browser_context, extension_info_map, keys::kOnAuthRequiredEvent, request,
      &extra_info_spec);
  if (listeners.empty())
    return net::NetworkDelegate::AUTH_REQUIRED_RESPONSE_NO_ACTION;

  std::unique_ptr<WebRequestEventDetails> event_details(
      CreateEventDetails(request, extra_info_spec));
  event_details->SetResponseHeaders(request, request->response_headers());
  event_details->SetAuthInfo(auth_info);

  if (DispatchEvent(browser_context, request, listeners,
                    std::move(event_details))) {
    BlockedRequest& blocked_request = blocked_requests_[request->identifier()];
    blocked_request.event = kOnAuthRequired;
    blocked_request.is_incognito |= IsIncognitoBrowserContext(browser_context);
    blocked_request.request = request;
    blocked_request.auth_callback = callback;
    blocked_request.auth_credentials = credentials;
    blocked_request.net_log = &request->net_log();
    return net::NetworkDelegate::AUTH_REQUIRED_RESPONSE_IO_PENDING;
  }
  return net::NetworkDelegate::AUTH_REQUIRED_RESPONSE_NO_ACTION;
}

void ExtensionWebRequestEventRouter::OnBeforeRedirect(
    void* browser_context,
    const InfoMap* extension_info_map,
    net::URLRequest* request,
    const GURL& new_location) {
  if (ShouldHideEvent(browser_context, extension_info_map, request))
    return;

  if (GetAndSetSignaled(request->identifier(), kOnBeforeRedirect))
    return;

  ClearSignaled(request->identifier(), kOnBeforeRequest);
  ClearSignaled(request->identifier(), kOnBeforeSendHeaders);
  ClearSignaled(request->identifier(), kOnSendHeaders);
  ClearSignaled(request->identifier(), kOnHeadersReceived);

  int extra_info_spec = 0;
  EventListeners listeners = GetMatchingListeners(
      browser_context, extension_info_map, keys::kOnBeforeRedirectEvent,
      request, &extra_info_spec);
  if (listeners.empty())
    return;

  std::unique_ptr<WebRequestEventDetails> event_details(
      CreateEventDetails(request, extra_info_spec));
  event_details->SetResponseHeaders(request, request->response_headers());
  event_details->SetResponseSource(request);
  event_details->SetString(keys::kRedirectUrlKey, new_location.spec());

  DispatchEvent(browser_context, request, listeners, std::move(event_details));
}

void ExtensionWebRequestEventRouter::OnResponseStarted(
    void* browser_context,
    const InfoMap* extension_info_map,
    net::URLRequest* request) {
  if (ShouldHideEvent(browser_context, extension_info_map, request))
    return;

  // OnResponseStarted is even triggered, when the request was cancelled.
  if (request->status().status() != net::URLRequestStatus::SUCCESS)
    return;

  int extra_info_spec = 0;
  EventListeners listeners = GetMatchingListeners(
      browser_context, extension_info_map, keys::kOnResponseStartedEvent,
      request, &extra_info_spec);
  if (listeners.empty())
    return;

  std::unique_ptr<WebRequestEventDetails> event_details(
      CreateEventDetails(request, extra_info_spec));
  event_details->SetResponseHeaders(request, request->response_headers());
  event_details->SetResponseSource(request);

  DispatchEvent(browser_context, request, listeners, std::move(event_details));
}

void ExtensionWebRequestEventRouter::OnCompleted(
    void* browser_context,
    const InfoMap* extension_info_map,
    net::URLRequest* request) {
  // We hide events from the system context as well as sensitive requests.
  // However, if the request first became sensitive after redirecting we have
  // already signaled it and thus we have to signal the end of it. This is
  // risk-free because the handler cannot modify the request now.
  if (!browser_context ||
      (WebRequestPermissions::HideRequest(extension_info_map, request) &&
       !WasSignaled(*request))) {
    return;
  }

  request_time_tracker_->LogRequestEndTime(request->identifier(),
                                           base::Time::Now());

  DCHECK(request->status().status() == net::URLRequestStatus::SUCCESS);

  DCHECK(!GetAndSetSignaled(request->identifier(), kOnCompleted));

  ClearPendingCallbacks(request);

  int extra_info_spec = 0;
  EventListeners listeners =
      GetMatchingListeners(browser_context, extension_info_map,
                           keys::kOnCompletedEvent, request, &extra_info_spec);
  if (listeners.empty())
    return;

  std::unique_ptr<WebRequestEventDetails> event_details(
      CreateEventDetails(request, extra_info_spec));
  event_details->SetResponseHeaders(request, request->response_headers());
  event_details->SetResponseSource(request);

  DispatchEvent(browser_context, request, listeners, std::move(event_details));
}

void ExtensionWebRequestEventRouter::OnErrorOccurred(
    void* browser_context,
    const InfoMap* extension_info_map,
    net::URLRequest* request,
    bool started) {
  // We hide events from the system context as well as sensitive requests.
  // However, if the request first became sensitive after redirecting we have
  // already signaled it and thus we have to signal the end of it. This is
  // risk-free because the handler cannot modify the request now.
  if (!browser_context ||
      (WebRequestPermissions::HideRequest(extension_info_map, request) &&
       !WasSignaled(*request))) {
    return;
  }

  request_time_tracker_->LogRequestEndTime(request->identifier(),
                                           base::Time::Now());

  DCHECK(request->status().status() == net::URLRequestStatus::FAILED ||
         request->status().status() == net::URLRequestStatus::CANCELED);

  DCHECK(!GetAndSetSignaled(request->identifier(), kOnErrorOccurred));

  ClearPendingCallbacks(request);

  int extra_info_spec = 0;
  EventListeners listeners = GetMatchingListeners(
      browser_context, extension_info_map,
      web_request::OnErrorOccurred::kEventName, request, &extra_info_spec);
  if (listeners.empty())
    return;

  std::unique_ptr<WebRequestEventDetails> event_details(
      CreateEventDetails(request, extra_info_spec));
  if (started)
    event_details->SetResponseSource(request);
  else
    event_details->SetBoolean(keys::kFromCache, request->was_cached());
  event_details->SetString(keys::kErrorKey,
                           net::ErrorToString(request->status().error()));

  DispatchEvent(browser_context, request, listeners, std::move(event_details));
}

void ExtensionWebRequestEventRouter::OnURLRequestDestroyed(
    void* browser_context,
    const net::URLRequest* request) {
  ClearPendingCallbacks(request);

  signaled_requests_.erase(request->identifier());

  request_time_tracker_->LogRequestEndTime(request->identifier(),
                                           base::Time::Now());
}

void ExtensionWebRequestEventRouter::ClearPendingCallbacks(
    const net::URLRequest* request) {
  blocked_requests_.erase(request->identifier());
}

bool ExtensionWebRequestEventRouter::DispatchEvent(
    void* browser_context,
    net::URLRequest* request,
    const std::vector<const EventListener*>& listeners,
    std::unique_ptr<WebRequestEventDetails> event_details) {
  // TODO(mpcomplete): Consider consolidating common (extension_id,json_args)
  // pairs into a single message sent to a list of sub_event_names.
  int num_handlers_blocking = 0;

  std::unique_ptr<std::vector<EventListener>> listeners_to_dispatch(
      new std::vector<EventListener>());
  listeners_to_dispatch->reserve(listeners.size());
  for (const EventListener* listener : listeners) {
    listeners_to_dispatch->push_back(*listener);
    if (listener->extra_info_spec &
        (ExtraInfoSpec::BLOCKING | ExtraInfoSpec::ASYNC_BLOCKING)) {
      listener->blocked_requests.insert(request->identifier());
      // If this is the first delegate blocking the request, go ahead and log
      // it.
      if (num_handlers_blocking == 0) {
        std::string delegate_info = l10n_util::GetStringFUTF8(
            IDS_LOAD_STATE_PARAMETER_EXTENSION,
            base::UTF8ToUTF16(listener->extension_name));
        // LobAndReport allows extensions that block requests to be displayed in
        // the load status bar.
        request->LogAndReportBlockedBy(delegate_info.c_str());
      }
      ++num_handlers_blocking;
    }
  }

  event_details.release()->DetermineFrameDataOnIO(base::Bind(
      &ExtensionWebRequestEventRouter::DispatchEventToListeners, AsWeakPtr(),
      browser_context, base::Passed(&listeners_to_dispatch)));

  if (num_handlers_blocking > 0) {
    BlockedRequest& blocked_request = blocked_requests_[request->identifier()];
    blocked_request.request = request;
    blocked_request.is_incognito |= IsIncognitoBrowserContext(browser_context);
    blocked_request.num_handlers_blocking += num_handlers_blocking;
    blocked_request.blocking_time = base::Time::Now();
    return true;
  }

  return false;
}

void ExtensionWebRequestEventRouter::DispatchEventToListeners(
    void* browser_context,
    std::unique_ptr<std::vector<EventListener>> listeners,
    std::unique_ptr<WebRequestEventDetails> event_details) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(listeners.get());
  DCHECK_GT(listeners->size(), 0UL);
  DCHECK(event_details.get());

  std::string event_name =
      EventRouter::GetBaseEventName((*listeners)[0].sub_event_name);
  DCHECK(IsWebRequestEvent(event_name));

  const std::set<EventListener>& event_listeners =
      listeners_[browser_context][event_name];
  void* cross_browser_context = GetCrossBrowserContext(browser_context);
  const std::set<EventListener>* cross_event_listeners =
      cross_browser_context ? &listeners_[cross_browser_context][event_name]
                            : nullptr;

  for (const EventListener& target : *listeners) {
    std::set<EventListener>::const_iterator listener =
        event_listeners.find(target);
    // Ignore listener if it was removed between the thread hops.
    if (listener == event_listeners.end()) {
      if (!cross_event_listeners)
        continue;
      listener = cross_event_listeners->find(target);
      if (listener == cross_event_listeners->end())
        continue;
    }

    if (!listener->ipc_sender.get())
      continue;

    // Filter out the optional keys that this listener didn't request.
    std::unique_ptr<base::ListValue> args_filtered(new base::ListValue);
    args_filtered->Append(
        event_details->GetFilteredDict(listener->extra_info_spec));

    EventRouter::DispatchEventToSender(
        listener->ipc_sender.get(), browser_context, listener->extension_id,
        listener->histogram_value, listener->sub_event_name,
        std::move(args_filtered), EventRouter::USER_GESTURE_UNKNOWN,
        EventFilteringInfo());
  }
}

void ExtensionWebRequestEventRouter::OnEventHandled(
    void* browser_context,
    const std::string& extension_id,
    const std::string& event_name,
    const std::string& sub_event_name,
    uint64_t request_id,
    EventResponse* response) {
  // TODO(robwu): Does this also work with webviews? operator< (used by find)
  // takes the webview ID into account, which is not set on |listener|.
  EventListener listener;
  listener.extension_id = extension_id;
  listener.sub_event_name = sub_event_name;

  // The listener may have been removed (e.g. due to the process going away)
  // before we got here.
  std::set<EventListener>::iterator found =
      listeners_[browser_context][event_name].find(listener);
  if (found != listeners_[browser_context][event_name].end())
    found->blocked_requests.erase(request_id);

  DecrementBlockCount(
      browser_context, extension_id, event_name, request_id, response);
}

bool ExtensionWebRequestEventRouter::AddEventListener(
    void* browser_context,
    const std::string& extension_id,
    const std::string& extension_name,
    events::HistogramValue histogram_value,
    const std::string& event_name,
    const std::string& sub_event_name,
    const RequestFilter& filter,
    int extra_info_spec,
    int embedder_process_id,
    int web_view_instance_id,
    base::WeakPtr<IPC::Sender> ipc_sender) {
  if (!IsWebRequestEvent(event_name))
    return false;

  EventListener listener;
  listener.extension_id = extension_id;
  listener.extension_name = extension_name;
  listener.histogram_value = histogram_value;
  listener.sub_event_name = sub_event_name;
  listener.filter = filter;
  listener.extra_info_spec = extra_info_spec;
  listener.ipc_sender = ipc_sender;
  listener.embedder_process_id = embedder_process_id;
  listener.web_view_instance_id = web_view_instance_id;
  if (listener.web_view_instance_id) {
    content::RecordAction(
        base::UserMetricsAction("WebView.WebRequest.AddListener"));
  }

  if (ContainsKey(listeners_[browser_context][event_name], listener)) {
    // This is likely an abuse of the API by a malicious extension.
    return false;
  }
  listeners_[browser_context][event_name].insert(listener);
  return true;
}

void ExtensionWebRequestEventRouter::RemoveEventListener(
    void* browser_context,
    const std::string& extension_id,
    const std::string& sub_event_name,
    int embedder_process_id,
    int web_view_instance_id) {
  std::string event_name = EventRouter::GetBaseEventName(sub_event_name);
  DCHECK(IsWebRequestEvent(event_name));

  EventListener listener;
  listener.extension_id = extension_id;
  listener.sub_event_name = sub_event_name;
  listener.embedder_process_id = embedder_process_id;
  listener.web_view_instance_id = web_view_instance_id;

  std::set<EventListener>& event_listeners =
      listeners_[browser_context][event_name];
  // It's possible for AddEventListener to fail asynchronously. In that case,
  // the renderer believes the listener exists, while the browser does not.
  // Ignore a RemoveEventListener in that case.
  std::set<EventListener>::const_iterator it = event_listeners.find(listener);
  if (it == event_listeners.end())
    return;

#if defined(OS_WIN)
  // Debugging https://crbug.com/589735
  // Please post crash reports at the following lines to the above issue.
  unsigned event_listener_count = event_listeners.count(listener);
  CHECK_GE(event_listener_count, 0u);
  CHECK_GE(event_listener_count, 1u);
  CHECK_LE(event_listener_count, 2u);
  CHECK_EQ(event_listener_count, 1u);
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
#endif  // OS_WIN
  CHECK_EQ(event_listeners.count(listener), 1u) <<
      "extension=" << extension_id << " event=" << event_name;

  // Unblock any request that this event listener may have been blocking.
  for (uint64_t id : it->blocked_requests)
    DecrementBlockCount(browser_context, extension_id, event_name, id, NULL);

  event_listeners.erase(listener);

  helpers::ClearCacheOnNavigation();
}

void ExtensionWebRequestEventRouter::RemoveWebViewEventListeners(
    void* browser_context,
    int embedder_process_id,
    int web_view_instance_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Iterate over all listeners of all WebRequest events to delete
  // any listeners that belong to the provided <webview>.
  ListenerMapForBrowserContext& map_for_browser_context =
      listeners_[browser_context];
  for (const auto& event_iter : map_for_browser_context) {
    // Construct a listeners_to_delete vector so that we don't modify the set of
    // listeners as we iterate through it.
    std::vector<EventListener> listeners_to_delete;
    const std::set<EventListener>& listeners = event_iter.second;
    for (const auto& listener : listeners) {
      if (listener.embedder_process_id == embedder_process_id &&
          listener.web_view_instance_id == web_view_instance_id) {
        listeners_to_delete.push_back(listener);
      }
    }
    // Remove the listeners selected for deletion.
    for (const auto& listener : listeners_to_delete) {
      RemoveEventListenerOnIOThread(
          browser_context,
          listener.extension_id,
          listener.sub_event_name,
          listener.embedder_process_id,
          listener.web_view_instance_id);
    }
  }
}

void ExtensionWebRequestEventRouter::OnOTRBrowserContextCreated(
    void* original_browser_context, void* otr_browser_context) {
  cross_browser_context_map_[original_browser_context] =
      std::make_pair(false, otr_browser_context);
  cross_browser_context_map_[otr_browser_context] =
      std::make_pair(true, original_browser_context);
}

void ExtensionWebRequestEventRouter::OnOTRBrowserContextDestroyed(
    void* original_browser_context, void* otr_browser_context) {
  cross_browser_context_map_.erase(otr_browser_context);
  cross_browser_context_map_.erase(original_browser_context);
}

void ExtensionWebRequestEventRouter::AddCallbackForPageLoad(
    const base::Closure& callback) {
  callbacks_for_page_load_.push_back(callback);
}

bool ExtensionWebRequestEventRouter::IsPageLoad(
    const net::URLRequest* request) const {
  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request);
  if (!info)
    return false;

  return info->GetResourceType() == content::RESOURCE_TYPE_MAIN_FRAME;
}

void ExtensionWebRequestEventRouter::NotifyPageLoad() {
  for (const auto& callback : callbacks_for_page_load_)
    callback.Run();
  callbacks_for_page_load_.clear();
}

void* ExtensionWebRequestEventRouter::GetCrossBrowserContext(
    void* browser_context) const {
  CrossBrowserContextMap::const_iterator cross_browser_context =
      cross_browser_context_map_.find(browser_context);
  if (cross_browser_context == cross_browser_context_map_.end())
    return NULL;
  return cross_browser_context->second.second;
}

bool ExtensionWebRequestEventRouter::IsIncognitoBrowserContext(
    void* browser_context) const {
  CrossBrowserContextMap::const_iterator cross_browser_context =
      cross_browser_context_map_.find(browser_context);
  if (cross_browser_context == cross_browser_context_map_.end())
    return false;
  return cross_browser_context->second.first;
}

bool ExtensionWebRequestEventRouter::WasSignaled(
    const net::URLRequest& request) const {
  SignaledRequestMap::const_iterator flag =
      signaled_requests_.find(request.identifier());
  return (flag != signaled_requests_.end()) && (flag->second != 0);
}

void ExtensionWebRequestEventRouter::GetMatchingListenersImpl(
    void* browser_context,
    const net::URLRequest* request,
    const InfoMap* extension_info_map,
    bool crosses_incognito,
    const std::string& event_name,
    const GURL& url,
    int render_process_host_id,
    int routing_id,
    content::ResourceType resource_type,
    bool is_async_request,
    bool is_request_from_extension,
    int* extra_info_spec,
    EventListeners* matching_listeners) {
  std::string web_request_event_name(event_name);
  WebViewRendererState::WebViewInfo web_view_info;
  bool is_web_view_guest = WebViewRendererState::GetInstance()->GetInfo(
      render_process_host_id, routing_id, &web_view_info);
  if (is_web_view_guest) {
    web_request_event_name.replace(
        0, sizeof(kWebRequestEventPrefix) - 1, webview::kWebViewEventPrefix);
  }

  std::set<EventListener>& listeners =
      listeners_[browser_context][web_request_event_name];
  for (const EventListener& listener : listeners) {
    if (!listener.ipc_sender.get()) {
      // The IPC sender has been deleted. This listener will be removed soon
      // via a call to RemoveEventListener. For now, just skip it.
      continue;
    }

    if (is_web_view_guest &&
        (listener.embedder_process_id != web_view_info.embedder_process_id ||
         listener.web_view_instance_id != web_view_info.instance_id)) {
      continue;
    }

    // Filter requests from other extensions / apps. This does not work for
    // content scripts, or extension pages in non-extension processes.
    if (is_request_from_extension &&
        listener.embedder_process_id != render_process_host_id) {
      continue;
    }

    if (!listener.filter.urls.is_empty() &&
        !listener.filter.urls.MatchesURL(url)) {
      continue;
    }

    int render_process_id = -1;
    int render_frame_id = -1;
    // TODO(devlin): Figure out when one/both of these can fail, and if we
    // need to address it.
    bool found_render_frame =
        content::ResourceRequestInfo::GetRenderFrameForRequest(
            request, &render_process_id, &render_frame_id);
    UMA_HISTOGRAM_BOOLEAN("Extensions.WebRequestEventFoundFrame",
                          found_render_frame);
    ExtensionApiFrameIdMap::FrameData frame_data;
    if (found_render_frame) {
      ExtensionApiFrameIdMap::Get()->GetCachedFrameDataOnIO(
          render_process_id, render_frame_id, &frame_data);
    }
    // Check if the tab id and window id match, if they were set in the
    // listener params.
    if ((listener.filter.tab_id != -1 &&
         frame_data.tab_id != listener.filter.tab_id) ||
        (listener.filter.window_id != -1 &&
         frame_data.window_id != listener.filter.window_id)) {
      continue;
    }

    const std::vector<content::ResourceType>& types = listener.filter.types;
    if (!types.empty() &&
        std::find(types.begin(), types.end(), resource_type) == types.end()) {
      continue;
    }

    if (!is_web_view_guest) {
      PermissionsData::AccessType access =
          WebRequestPermissions::CanExtensionAccessURL(
              extension_info_map, listener.extension_id, url, frame_data.tab_id,
              crosses_incognito,
              WebRequestPermissions::REQUIRE_HOST_PERMISSION);
      if (access != PermissionsData::ACCESS_ALLOWED) {
        if (access == PermissionsData::ACCESS_WITHHELD &&
            web_request_event_router_delegate_) {
          web_request_event_router_delegate_->NotifyWebRequestWithheld(
              render_process_id, render_frame_id, listener.extension_id);
        }
        continue;
      }
    }

    bool blocking_listener =
        (listener.extra_info_spec &
         (ExtraInfoSpec::BLOCKING | ExtraInfoSpec::ASYNC_BLOCKING)) != 0;

    // We do not want to notify extensions about XHR requests that are
    // triggered by themselves. This is a workaround to prevent deadlocks
    // in case of synchronous XHR requests that block the extension renderer
    // and therefore prevent the extension from processing the request
    // handler. This is only a problem for blocking listeners.
    // http://crbug.com/105656
    bool synchronous_xhr_from_extension =
        !is_async_request && is_request_from_extension &&
        resource_type == content::RESOURCE_TYPE_XHR;

    // Only send webRequest events for URLs the extension has access to.
    if (blocking_listener && synchronous_xhr_from_extension)
      continue;

    matching_listeners->push_back(&listener);
    *extra_info_spec |= listener.extra_info_spec;
  }
}

ExtensionWebRequestEventRouter::EventListeners
ExtensionWebRequestEventRouter::GetMatchingListeners(
    void* browser_context,
    const InfoMap* extension_info_map,
    const std::string& event_name,
    const net::URLRequest* request,
    int* extra_info_spec) {
  // TODO(mpcomplete): handle browser_context == NULL (should collect all
  // listeners).
  *extra_info_spec = 0;

  const GURL& url = request->url();
  int render_process_host_id = content::ChildProcessHost::kInvalidUniqueID;
  int routing_id = MSG_ROUTING_NONE;
  content::ResourceType resource_type = content::RESOURCE_TYPE_LAST_TYPE;
  // We are conservative here and assume requests are asynchronous in case
  // we don't have an info object. We don't want to risk a deadlock.
  bool is_async_request = false;
  bool is_request_from_extension =
      IsRequestFromExtension(request, extension_info_map);

  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request);
  if (info) {
    is_async_request = info->IsAsync();
    if (helpers::IsRelevantResourceType(info->GetResourceType()))
      resource_type = info->GetResourceType();
    render_process_host_id = info->GetChildID();
    routing_id = info->GetRouteID();
  }

  EventListeners matching_listeners;
  GetMatchingListenersImpl(
      browser_context, request, extension_info_map, false, event_name,
      url, render_process_host_id, routing_id, resource_type,
      is_async_request, is_request_from_extension, extra_info_spec,
      &matching_listeners);
  void* cross_browser_context = GetCrossBrowserContext(browser_context);
  if (cross_browser_context) {
    GetMatchingListenersImpl(
        cross_browser_context, request, extension_info_map, true, event_name,
        url, render_process_host_id, routing_id, resource_type,
        is_async_request, is_request_from_extension, extra_info_spec,
        &matching_listeners);
  }

  return matching_listeners;
}

namespace {

helpers::EventResponseDelta* CalculateDelta(
    ExtensionWebRequestEventRouter::BlockedRequest* blocked_request,
    ExtensionWebRequestEventRouter::EventResponse* response) {
  switch (blocked_request->event) {
    case ExtensionWebRequestEventRouter::kOnBeforeRequest:
      return helpers::CalculateOnBeforeRequestDelta(
          response->extension_id, response->extension_install_time,
          response->cancel, response->new_url);
    case ExtensionWebRequestEventRouter::kOnBeforeSendHeaders: {
      net::HttpRequestHeaders* old_headers = blocked_request->request_headers;
      net::HttpRequestHeaders* new_headers = response->request_headers.get();
      return helpers::CalculateOnBeforeSendHeadersDelta(
          response->extension_id, response->extension_install_time,
          response->cancel, old_headers, new_headers);
    }
    case ExtensionWebRequestEventRouter::kOnHeadersReceived: {
      const net::HttpResponseHeaders* old_headers =
          blocked_request->original_response_headers.get();
      helpers::ResponseHeaders* new_headers =
          response->response_headers.get();
      return helpers::CalculateOnHeadersReceivedDelta(
          response->extension_id,
          response->extension_install_time,
          response->cancel,
          response->new_url,
          old_headers,
          new_headers);
    }
    case ExtensionWebRequestEventRouter::kOnAuthRequired:
      return helpers::CalculateOnAuthRequiredDelta(
          response->extension_id, response->extension_install_time,
          response->cancel, &response->auth_credentials);
    default:
      NOTREACHED();
      return nullptr;
  }
}

base::Value* SerializeResponseHeaders(const helpers::ResponseHeaders& headers) {
  std::unique_ptr<base::ListValue> serialized_headers(new base::ListValue());
  for (const auto& it : headers) {
    serialized_headers->Append(
        helpers::CreateHeaderDictionary(it.first, it.second));
  }
  return serialized_headers.release();
}

// Convert a RequestCookieModifications/ResponseCookieModifications object to a
// base::ListValue which summarizes the changes made.  This is templated since
// the two types (request/response) are different but contain essentially the
// same fields.
template <typename CookieType>
base::ListValue* SummarizeCookieModifications(
    const std::vector<linked_ptr<CookieType>>& modifications) {
  std::unique_ptr<base::ListValue> cookie_modifications(new base::ListValue());
  for (const auto& it : modifications) {
    std::unique_ptr<base::DictionaryValue> summary(new base::DictionaryValue());
    const CookieType& mod = *(it.get());
    switch (mod.type) {
      case helpers::ADD:
        summary->SetString(activity_log::kCookieModificationTypeKey,
                           activity_log::kCookieModificationAdd);
        break;
      case helpers::EDIT:
        summary->SetString(activity_log::kCookieModificationTypeKey,
                           activity_log::kCookieModificationEdit);
        break;
      case helpers::REMOVE:
        summary->SetString(activity_log::kCookieModificationTypeKey,
                           activity_log::kCookieModificationRemove);
        break;
    }
    if (mod.filter) {
      if (mod.filter->name) {
        summary->SetString(activity_log::kCookieFilterNameKey,
                           *mod.modification->name);
      }
      if (mod.filter->domain) {
        summary->SetString(activity_log::kCookieFilterDomainKey,
                           *mod.modification->name);
      }
    }
    if (mod.modification) {
      if (mod.modification->name) {
        summary->SetString(activity_log::kCookieModDomainKey,
                           *mod.modification->name);
      }
      if (mod.modification->domain) {
        summary->SetString(activity_log::kCookieModDomainKey,
                           *mod.modification->name);
      }
    }
    cookie_modifications->Append(std::move(summary));
  }
  return cookie_modifications.release();
}

// Converts an EventResponseDelta object to a dictionary value suitable for the
// activity log.
std::unique_ptr<base::DictionaryValue> SummarizeResponseDelta(
    const std::string& event_name,
    const helpers::EventResponseDelta& delta) {
  std::unique_ptr<base::DictionaryValue> details(new base::DictionaryValue());
  if (delta.cancel)
    details->SetBoolean(activity_log::kCancelKey, true);
  if (!delta.new_url.is_empty())
    details->SetString(activity_log::kNewUrlKey, delta.new_url.spec());

  std::unique_ptr<base::ListValue> modified_headers(new base::ListValue());
  net::HttpRequestHeaders::Iterator iter(delta.modified_request_headers);
  while (iter.GetNext()) {
    modified_headers->Append(
        helpers::CreateHeaderDictionary(iter.name(), iter.value()));
  }
  if (!modified_headers->empty()) {
    details->Set(activity_log::kModifiedRequestHeadersKey,
                 modified_headers.release());
  }

  std::unique_ptr<base::ListValue> deleted_headers(new base::ListValue());
  deleted_headers->AppendStrings(delta.deleted_request_headers);
  if (!deleted_headers->empty()) {
    details->Set(activity_log::kDeletedRequestHeadersKey,
                 deleted_headers.release());
  }

  if (!delta.added_response_headers.empty()) {
    details->Set(activity_log::kAddedRequestHeadersKey,
                 SerializeResponseHeaders(delta.added_response_headers));
  }
  if (!delta.deleted_response_headers.empty()) {
    details->Set(activity_log::kDeletedResponseHeadersKey,
                 SerializeResponseHeaders(delta.deleted_response_headers));
  }
  if (delta.auth_credentials) {
    details->SetString(
        activity_log::kAuthCredentialsKey,
        base::UTF16ToUTF8(delta.auth_credentials->username()) + ":*");
  }

  if (!delta.response_cookie_modifications.empty()) {
    details->Set(
        activity_log::kResponseCookieModificationsKey,
        SummarizeCookieModifications(delta.response_cookie_modifications));
  }

  return details;
}

}  // namespace

void ExtensionWebRequestEventRouter::DecrementBlockCount(
    void* browser_context,
    const std::string& extension_id,
    const std::string& event_name,
    uint64_t request_id,
    EventResponse* response) {
  std::unique_ptr<EventResponse> response_scoped(response);

  // It's possible that this request was deleted, or cancelled by a previous
  // event handler. If so, ignore this response.
  auto it = blocked_requests_.find(request_id);
  if (it == blocked_requests_.end())
    return;

  BlockedRequest& blocked_request = it->second;
  int num_handlers_blocking = --blocked_request.num_handlers_blocking;
  CHECK_GE(num_handlers_blocking, 0);

  if (response) {
    helpers::EventResponseDelta* delta =
        CalculateDelta(&blocked_request, response);

    activity_monitor::OnWebRequestApiUsed(
        static_cast<content::BrowserContext*>(browser_context), extension_id,
        blocked_request.request->url(), blocked_request.is_incognito,
        event_name, SummarizeResponseDelta(event_name, *delta));

    blocked_request.response_deltas.push_back(
        linked_ptr<helpers::EventResponseDelta>(delta));
  }

  base::TimeDelta block_time =
      base::Time::Now() - blocked_request.blocking_time;
  if (!extension_id.empty()) {
    request_time_tracker_->IncrementExtensionBlockTime(
        extension_id, request_id, block_time);
  } else {
    // |extension_id| is empty for requests blocked on startup waiting for the
    // declarative rules to be read from disk.
    UMA_HISTOGRAM_TIMES("Extensions.NetworkDelayStartup", block_time);
  }

  if (num_handlers_blocking == 0) {
    blocked_request.request->LogUnblocked();
    ExecuteDeltas(browser_context, request_id, true);
  } else {
    // Update the URLRequest to make sure it's tagged with an extension that's
    // still blocking it.  This may end up being the same extension as before.
    std::set<EventListener>& listeners =
        listeners_[browser_context][event_name];

    for (const auto& listener : listeners) {
      if (!ContainsKey(listener.blocked_requests, request_id))
        continue;
      std::string delegate_info =
          l10n_util::GetStringFUTF8(IDS_LOAD_STATE_PARAMETER_EXTENSION,
                                    base::UTF8ToUTF16(listener.extension_name));
      blocked_request.request->LogAndReportBlockedBy(delegate_info.c_str());
      break;
    }
  }
}

void ExtensionWebRequestEventRouter::SendMessages(
    void* browser_context,
    const BlockedRequest& blocked_request) {
  const helpers::EventResponseDeltas& deltas = blocked_request.response_deltas;
  for (const auto& delta : deltas) {
    const std::set<std::string>& messages = delta->messages_to_extension;
    for (const std::string& message : messages) {
      std::unique_ptr<WebRequestEventDetails> event_details(
          CreateEventDetails(blocked_request.request, /* extra_info_spec */ 0));
      WebViewRendererState::WebViewInfo web_view_info;
      bool is_web_view_guest = GetWebViewInfo(blocked_request.request,
                                              &web_view_info);
      event_details->SetString(keys::kMessageKey, message);
      event_details->SetString(keys::kStageKey,
                               GetRequestStageAsString(blocked_request.event));

      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&SendOnMessageEventOnUI, browser_context,
                     delta->extension_id, is_web_view_guest, web_view_info,
                     base::Passed(&event_details)));
    }
  }
}

int ExtensionWebRequestEventRouter::ExecuteDeltas(void* browser_context,
                                                  uint64_t request_id,
                                                  bool call_callback) {
  BlockedRequest& blocked_request = blocked_requests_[request_id];
  CHECK_EQ(0, blocked_request.num_handlers_blocking);
  helpers::EventResponseDeltas& deltas = blocked_request.response_deltas;
  base::TimeDelta block_time =
      base::Time::Now() - blocked_request.blocking_time;
  request_time_tracker_->IncrementTotalBlockTime(request_id, block_time);

  bool credentials_set = false;

  deltas.sort(&helpers::InDecreasingExtensionInstallationTimeOrder);

  bool canceled = false;
  helpers::MergeCancelOfResponses(blocked_request.response_deltas, &canceled,
                                  blocked_request.net_log);

  WarningSet warnings;
  if (blocked_request.event == kOnBeforeRequest) {
    CHECK(!blocked_request.callback.is_null());
    helpers::MergeOnBeforeRequestResponses(
        blocked_request.response_deltas,
        blocked_request.new_url,
        &warnings,
        blocked_request.net_log);
  } else if (blocked_request.event == kOnBeforeSendHeaders) {
    CHECK(!blocked_request.callback.is_null());
    helpers::MergeOnBeforeSendHeadersResponses(
        blocked_request.response_deltas,
        blocked_request.request_headers,
        &warnings,
        blocked_request.net_log);
  } else if (blocked_request.event == kOnHeadersReceived) {
    CHECK(!blocked_request.callback.is_null());
    helpers::MergeOnHeadersReceivedResponses(
        blocked_request.response_deltas,
        blocked_request.original_response_headers.get(),
        blocked_request.override_response_headers,
        blocked_request.new_url,
        &warnings,
        blocked_request.net_log);
  } else if (blocked_request.event == kOnAuthRequired) {
    CHECK(blocked_request.callback.is_null());
    CHECK(!blocked_request.auth_callback.is_null());
    credentials_set = helpers::MergeOnAuthRequiredResponses(
       blocked_request.response_deltas,
       blocked_request.auth_credentials,
       &warnings,
       blocked_request.net_log);
  } else {
    NOTREACHED();
  }

  SendMessages(browser_context, blocked_request);

  if (!warnings.empty()) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&WarningService::NotifyWarningsOnUI,
                   browser_context, warnings));
  }

  if (canceled) {
    request_time_tracker_->SetRequestCanceled(request_id);
  } else if (blocked_request.new_url &&
             !blocked_request.new_url->is_empty()) {
    request_time_tracker_->SetRequestRedirected(request_id);
  }

  // This triggers onErrorOccurred if canceled is true.
  int rv = canceled ? net::ERR_BLOCKED_BY_CLIENT : net::OK;

  if (!blocked_request.callback.is_null()) {
    net::CompletionCallback callback = blocked_request.callback;
    // Ensure that request is removed before callback because the callback
    // might trigger the next event.
    blocked_requests_.erase(request_id);
    if (call_callback)
      callback.Run(rv);
  } else if (!blocked_request.auth_callback.is_null()) {
    net::NetworkDelegate::AuthRequiredResponse response;
    if (canceled)
      response = net::NetworkDelegate::AUTH_REQUIRED_RESPONSE_CANCEL_AUTH;
    else if (credentials_set)
      response = net::NetworkDelegate::AUTH_REQUIRED_RESPONSE_SET_AUTH;
    else
      response = net::NetworkDelegate::AUTH_REQUIRED_RESPONSE_NO_ACTION;

    net::NetworkDelegate::AuthCallback callback = blocked_request.auth_callback;
    blocked_requests_.erase(request_id);
    if (call_callback)
      callback.Run(response);
  } else {
    blocked_requests_.erase(request_id);
  }
  return rv;
}

bool ExtensionWebRequestEventRouter::ProcessDeclarativeRules(
    void* browser_context,
    const InfoMap* extension_info_map,
    const std::string& event_name,
    net::URLRequest* request,
    RequestStage request_stage,
    const net::HttpResponseHeaders* original_response_headers) {
  WebViewRendererState::WebViewInfo web_view_info;
  bool is_web_view_guest = GetWebViewInfo(request, &web_view_info);
  int rules_registry_id = is_web_view_guest
                              ? web_view_info.rules_registry_id
                              : RulesRegistryService::kDefaultRulesRegistryID;

  RulesRegistryKey rules_key(browser_context, rules_registry_id);
  // If this check fails, check that the active stages are up to date in
  // extensions/browser/api/declarative_webrequest/request_stage.h .
  DCHECK(request_stage & kActiveStages);

  // Rules of the current |browser_context| may apply but we need to check also
  // whether there are applicable rules from extensions whose background page
  // spans from regular to incognito mode.

  // First parameter identifies the registry, the second indicates whether the
  // registry belongs to the cross browser_context.
  using RelevantRegistry = std::pair<WebRequestRulesRegistry*, bool>;
  std::vector<RelevantRegistry> relevant_registries;

  auto rules_key_it = rules_registries_.find(rules_key);
  if (rules_key_it != rules_registries_.end()) {
    relevant_registries.push_back(
        std::make_pair(rules_key_it->second.get(), false));
  }

  void* cross_browser_context = GetCrossBrowserContext(browser_context);
  RulesRegistryKey cross_browser_context_rules_key(cross_browser_context,
                                                   rules_registry_id);
  if (cross_browser_context) {
    auto it = rules_registries_.find(cross_browser_context_rules_key);
    if (it != rules_registries_.end())
      relevant_registries.push_back(std::make_pair(it->second.get(), true));
  }

  // The following block is experimentally enabled and its impact on load time
  // logged with UMA Extensions.NetworkDelayRegistryLoad. crbug.com/175961
  for (auto it : relevant_registries) {
    WebRequestRulesRegistry* rules_registry = it.first;
    if (rules_registry->ready().is_signaled())
      continue;

    // The rules registry is still loading. Block this request until it
    // finishes.
    rules_registry->ready().Post(
        FROM_HERE,
        base::Bind(&ExtensionWebRequestEventRouter::OnRulesRegistryReady,
                   AsWeakPtr(), browser_context, event_name,
                   request->identifier(), request_stage));
    BlockedRequest& blocked_request = blocked_requests_[request->identifier()];
    blocked_request.num_handlers_blocking++;
    blocked_request.request = request;
    blocked_request.is_incognito |= IsIncognitoBrowserContext(browser_context);
    blocked_request.blocking_time = base::Time::Now();
    blocked_request.original_response_headers = original_response_headers;
    blocked_request.extension_info_map = extension_info_map;
    return true;
  }

  base::Time start = base::Time::Now();

  bool deltas_created = false;
  for (const auto& it : relevant_registries) {
    WebRequestRulesRegistry* rules_registry = it.first;
    helpers::EventResponseDeltas result = rules_registry->CreateDeltas(
        extension_info_map,
        WebRequestData(request, request_stage, original_response_headers),
        it.second);

    if (!result.empty()) {
      helpers::EventResponseDeltas& deltas =
          blocked_requests_[request->identifier()].response_deltas;
      deltas.insert(deltas.end(), result.begin(), result.end());
      deltas_created = true;
    }
  }

  base::TimeDelta elapsed_time = start - base::Time::Now();
  UMA_HISTOGRAM_TIMES("Extensions.DeclarativeWebRequestNetworkDelay",
                      elapsed_time);

  return deltas_created;
}

void ExtensionWebRequestEventRouter::OnRulesRegistryReady(
    void* browser_context,
    const std::string& event_name,
    uint64_t request_id,
    RequestStage request_stage) {
  // It's possible that this request was deleted, or cancelled by a previous
  // event handler. If so, ignore this response.
  auto it = blocked_requests_.find(request_id);
  if (it == blocked_requests_.end())
    return;

  BlockedRequest& blocked_request = it->second;
  base::TimeDelta block_time =
      base::Time::Now() - blocked_request.blocking_time;
  UMA_HISTOGRAM_TIMES("Extensions.NetworkDelayRegistryLoad", block_time);

  ProcessDeclarativeRules(browser_context,
                          blocked_request.extension_info_map,
                          event_name,
                          blocked_request.request,
                          request_stage,
                          blocked_request.original_response_headers.get());
  // Reset to NULL so that nobody relies on this being set.
  blocked_request.extension_info_map = NULL;
  DecrementBlockCount(
      browser_context, std::string(), event_name, request_id, NULL);
}

bool ExtensionWebRequestEventRouter::GetAndSetSignaled(uint64_t request_id,
                                                       EventTypes event_type) {
  SignaledRequestMap::iterator iter = signaled_requests_.find(request_id);
  if (iter == signaled_requests_.end()) {
    signaled_requests_[request_id] = event_type;
    return false;
  }
  bool was_signaled_before = (iter->second & event_type) != 0;
  iter->second |= event_type;
  return was_signaled_before;
}

void ExtensionWebRequestEventRouter::ClearSignaled(uint64_t request_id,
                                                   EventTypes event_type) {
  SignaledRequestMap::iterator iter = signaled_requests_.find(request_id);
  if (iter == signaled_requests_.end())
    return;
  iter->second &= ~event_type;
}

// Special QuotaLimitHeuristic for WebRequestHandlerBehaviorChangedFunction.
//
// Each call of webRequest.handlerBehaviorChanged() clears the in-memory cache
// of WebKit at the time of the next page load (top level navigation event).
// This quota heuristic is intended to limit the number of times the cache is
// cleared by an extension.
//
// As we want to account for the number of times the cache is really cleared
// (opposed to the number of times webRequest.handlerBehaviorChanged() is
// called), we cannot decide whether a call of
// webRequest.handlerBehaviorChanged() should trigger a quota violation at the
// time it is called. Instead we only decrement the bucket counter at the time
// when the cache is cleared (when page loads happen).
class ClearCacheQuotaHeuristic : public QuotaLimitHeuristic {
 public:
  ClearCacheQuotaHeuristic(const Config& config, BucketMapper* map)
      : QuotaLimitHeuristic(
            config,
            map,
            "MAX_HANDLER_BEHAVIOR_CHANGED_CALLS_PER_10_MINUTES"),
        callback_registered_(false),
        weak_ptr_factory_(this) {}
  ~ClearCacheQuotaHeuristic() override {}
  bool Apply(Bucket* bucket, const base::TimeTicks& event_time) override;

 private:
  // Callback that is triggered by the ExtensionWebRequestEventRouter on a page
  // load.
  //
  // We don't need to take care of the life time of |bucket|: It is owned by the
  // BucketMapper of our base class in |QuotaLimitHeuristic::bucket_mapper_|. As
  // long as |this| exists, the respective BucketMapper and its bucket will
  // exist as well.
  void OnPageLoad(Bucket* bucket);

  // Flag to prevent that we register more than one call back in-between
  // clearing the cache.
  bool callback_registered_;

  base::WeakPtrFactory<ClearCacheQuotaHeuristic> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ClearCacheQuotaHeuristic);
};

bool ClearCacheQuotaHeuristic::Apply(Bucket* bucket,
                                     const base::TimeTicks& event_time) {
  if (event_time > bucket->expiration())
    bucket->Reset(config(), event_time);

  // Call bucket->DeductToken() on a new page load, this is when
  // webRequest.handlerBehaviorChanged() clears the cache.
  if (!callback_registered_) {
    ExtensionWebRequestEventRouter::GetInstance()->AddCallbackForPageLoad(
        base::Bind(&ClearCacheQuotaHeuristic::OnPageLoad,
                   weak_ptr_factory_.GetWeakPtr(),
                   bucket));
    callback_registered_ = true;
  }

  // We only check whether tokens are left here. Deducting a token happens in
  // OnPageLoad().
  return bucket->has_tokens();
}

void ClearCacheQuotaHeuristic::OnPageLoad(Bucket* bucket) {
  callback_registered_ = false;
  bucket->DeductToken();
}

bool WebRequestInternalAddEventListenerFunction::RunSync() {
  // Argument 0 is the callback, which we don't use here.
  ExtensionWebRequestEventRouter::RequestFilter filter;
  base::DictionaryValue* value = NULL;
  error_.clear();
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(1, &value));
  // Failure + an empty error string means a fatal error.
  EXTENSION_FUNCTION_VALIDATE(filter.InitFromValue(*value, &error_) ||
                              !error_.empty());
  if (!error_.empty())
    return false;

  int extra_info_spec = 0;
  if (HasOptionalArgument(2)) {
    base::ListValue* value = NULL;
    EXTENSION_FUNCTION_VALIDATE(args_->GetList(2, &value));
    EXTENSION_FUNCTION_VALIDATE(
        ExtraInfoSpec::InitFromValue(*value, &extra_info_spec));
  }

  std::string event_name;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(3, &event_name));

  std::string sub_event_name;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(4, &sub_event_name));

  int web_view_instance_id = 0;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(5, &web_view_instance_id));

  base::WeakPtr<IOThreadExtensionMessageFilter> ipc_sender = ipc_sender_weak();
  int embedder_process_id = ipc_sender ? ipc_sender->render_process_id() : 0;

  const Extension* extension =
      extension_info_map()->extensions().GetByID(extension_id_safe());
  std::string extension_name =
      extension ? extension->name() : extension_id_safe();

  if (!web_view_instance_id) {
    // We check automatically whether the extension has the 'webRequest'
    // permission. For blocking calls we require the additional permission
    // 'webRequestBlocking'.
    if ((extra_info_spec &
         (ExtraInfoSpec::BLOCKING | ExtraInfoSpec::ASYNC_BLOCKING)) &&
        !extension->permissions_data()->HasAPIPermission(
            APIPermission::kWebRequestBlocking)) {
      error_ = keys::kBlockingPermissionRequired;
      return false;
    }

    // We allow to subscribe to patterns that are broader than the host
    // permissions. E.g., we could subscribe to http://www.example.com/*
    // while having host permissions for http://www.example.com/foo/* and
    // http://www.example.com/bar/*.
    // For this reason we do only a coarse check here to warn the extension
    // developer if they do something obviously wrong.
    if (extension->permissions_data()
            ->GetEffectiveHostPermissions()
            .is_empty() &&
        extension->permissions_data()
            ->withheld_permissions()
            .explicit_hosts()
            .is_empty()) {
      error_ = keys::kHostPermissionsRequired;
      return false;
    }
  }

  bool success =
      ExtensionWebRequestEventRouter::GetInstance()->AddEventListener(
          profile_id(), extension_id_safe(), extension_name,
          GetEventHistogramValue(event_name), event_name, sub_event_name,
          filter, extra_info_spec, embedder_process_id, web_view_instance_id,
          ipc_sender_weak());
  EXTENSION_FUNCTION_VALIDATE(success);

  helpers::ClearCacheOnNavigation();

  if (!extension_id_safe().empty()) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(&helpers::NotifyWebRequestAPIUsed,
                                       profile_id(), extension_id_safe()));
  }

  return true;
}

void WebRequestInternalEventHandledFunction::RespondWithError(
    const std::string& event_name,
    const std::string& sub_event_name,
    uint64_t request_id,
    std::unique_ptr<ExtensionWebRequestEventRouter::EventResponse> response,
    const std::string& error) {
  error_ = error;
  ExtensionWebRequestEventRouter::GetInstance()->OnEventHandled(
      profile_id(),
      extension_id_safe(),
      event_name,
      sub_event_name,
      request_id,
      response.release());
}

bool WebRequestInternalEventHandledFunction::RunSync() {
  std::string event_name;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &event_name));

  std::string sub_event_name;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(1, &sub_event_name));

  std::string request_id_str;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(2, &request_id_str));
  uint64_t request_id;
  EXTENSION_FUNCTION_VALIDATE(base::StringToUint64(request_id_str,
                                                   &request_id));

  std::unique_ptr<ExtensionWebRequestEventRouter::EventResponse> response;
  if (HasOptionalArgument(3)) {
    base::DictionaryValue* value = NULL;
    EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(3, &value));

    if (!value->empty()) {
      base::Time install_time =
          extension_info_map()->GetInstallTime(extension_id_safe());
      response.reset(new ExtensionWebRequestEventRouter::EventResponse(
          extension_id_safe(), install_time));
    }

    if (value->HasKey("cancel")) {
      // Don't allow cancel mixed with other keys.
      if (value->size() != 1) {
        RespondWithError(event_name, sub_event_name, request_id,
                         std::move(response), keys::kInvalidBlockingResponse);
        return false;
      }

      bool cancel = false;
      EXTENSION_FUNCTION_VALIDATE(value->GetBoolean("cancel", &cancel));
      response->cancel = cancel;
    }

    if (value->HasKey("redirectUrl")) {
      std::string new_url_str;
      EXTENSION_FUNCTION_VALIDATE(value->GetString("redirectUrl",
                                                   &new_url_str));
      response->new_url = GURL(new_url_str);
      if (!response->new_url.is_valid()) {
        RespondWithError(event_name, sub_event_name, request_id,
                         std::move(response),
                         ErrorUtils::FormatErrorMessage(
                             keys::kInvalidRedirectUrl, new_url_str));
        return false;
      }
    }

    const bool has_request_headers = value->HasKey("requestHeaders");
    const bool has_response_headers = value->HasKey("responseHeaders");
    if (has_request_headers || has_response_headers) {
      if (has_request_headers && has_response_headers) {
        // Allow only one of the keys, not both.
        RespondWithError(event_name, sub_event_name, request_id,
                         std::move(response),
                         keys::kInvalidHeaderKeyCombination);
        return false;
      }

      base::ListValue* headers_value = NULL;
      std::unique_ptr<net::HttpRequestHeaders> request_headers;
      std::unique_ptr<helpers::ResponseHeaders> response_headers;
      if (has_request_headers) {
        request_headers.reset(new net::HttpRequestHeaders());
        EXTENSION_FUNCTION_VALIDATE(value->GetList(keys::kRequestHeadersKey,
                                                   &headers_value));
      } else {
        response_headers.reset(new helpers::ResponseHeaders());
        EXTENSION_FUNCTION_VALIDATE(value->GetList(keys::kResponseHeadersKey,
                                                   &headers_value));
      }

      for (size_t i = 0; i < headers_value->GetSize(); ++i) {
        base::DictionaryValue* header_value = NULL;
        std::string name;
        std::string value;
        EXTENSION_FUNCTION_VALIDATE(
            headers_value->GetDictionary(i, &header_value));
        if (!FromHeaderDictionary(header_value, &name, &value)) {
          std::string serialized_header;
          base::JSONWriter::Write(*header_value, &serialized_header);
          RespondWithError(event_name, sub_event_name, request_id,
                           std::move(response),
                           ErrorUtils::FormatErrorMessage(keys::kInvalidHeader,
                                                          serialized_header));
          return false;
        }
        if (!net::HttpUtil::IsValidHeaderName(name)) {
          RespondWithError(event_name, sub_event_name, request_id,
                           std::move(response), keys::kInvalidHeaderName);
          return false;
        }
        if (!net::HttpUtil::IsValidHeaderValue(value)) {
          RespondWithError(
              event_name, sub_event_name, request_id, std::move(response),
              ErrorUtils::FormatErrorMessage(keys::kInvalidHeaderValue, name));
          return false;
        }
        if (has_request_headers)
          request_headers->SetHeader(name, value);
        else
          response_headers->push_back(helpers::ResponseHeader(name, value));
      }
      if (has_request_headers)
        response->request_headers.reset(request_headers.release());
      else
        response->response_headers.reset(response_headers.release());
    }

    if (value->HasKey(keys::kAuthCredentialsKey)) {
      base::DictionaryValue* credentials_value = NULL;
      EXTENSION_FUNCTION_VALIDATE(value->GetDictionary(
          keys::kAuthCredentialsKey,
          &credentials_value));
      base::string16 username;
      base::string16 password;
      EXTENSION_FUNCTION_VALIDATE(
          credentials_value->GetString(keys::kUsernameKey, &username));
      EXTENSION_FUNCTION_VALIDATE(
          credentials_value->GetString(keys::kPasswordKey, &password));
      response->auth_credentials.reset(
          new net::AuthCredentials(username, password));
    }
  }

  ExtensionWebRequestEventRouter::GetInstance()->OnEventHandled(
      profile_id(), extension_id_safe(), event_name, sub_event_name, request_id,
      response.release());

  return true;
}

void WebRequestHandlerBehaviorChangedFunction::GetQuotaLimitHeuristics(
    QuotaLimitHeuristics* heuristics) const {
  QuotaLimitHeuristic::Config config = {
      // See web_request.json for current value.
      web_request::MAX_HANDLER_BEHAVIOR_CHANGED_CALLS_PER_10_MINUTES,
      base::TimeDelta::FromMinutes(10)};
  QuotaLimitHeuristic::BucketMapper* bucket_mapper =
      new QuotaLimitHeuristic::SingletonBucketMapper();
  ClearCacheQuotaHeuristic* heuristic =
      new ClearCacheQuotaHeuristic(config, bucket_mapper);
  heuristics->push_back(heuristic);
}

void WebRequestHandlerBehaviorChangedFunction::OnQuotaExceeded(
    const std::string& violation_error) {
  // Post warning message.
  WarningSet warnings;
  warnings.insert(
      Warning::CreateRepeatedCacheFlushesWarning(extension_id_safe()));
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&WarningService::NotifyWarningsOnUI, profile_id(), warnings));

  // Continue gracefully.
  RunSync();
}

bool WebRequestHandlerBehaviorChangedFunction::RunSync() {
  helpers::ClearCacheOnNavigation();
  return true;
}

}  // namespace extensions
