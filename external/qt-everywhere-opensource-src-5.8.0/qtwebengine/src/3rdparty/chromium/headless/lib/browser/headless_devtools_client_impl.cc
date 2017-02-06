// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_devtools_client_impl.h"

#include <memory>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "content/public/browser/devtools_agent_host.h"

namespace headless {

// static
std::unique_ptr<HeadlessDevToolsClient> HeadlessDevToolsClient::Create() {
  return base::WrapUnique(new HeadlessDevToolsClientImpl());
}

// static
HeadlessDevToolsClientImpl* HeadlessDevToolsClientImpl::From(
    HeadlessDevToolsClient* client) {
  // This downcast is safe because there is only one implementation of
  // HeadlessDevToolsClient.
  return static_cast<HeadlessDevToolsClientImpl*>(client);
}

HeadlessDevToolsClientImpl::HeadlessDevToolsClientImpl()
    : agent_host_(nullptr),
      next_message_id_(0),
      accessibility_domain_(this),
      animation_domain_(this),
      application_cache_domain_(this),
      cache_storage_domain_(this),
      console_domain_(this),
      css_domain_(this),
      database_domain_(this),
      debugger_domain_(this),
      device_orientation_domain_(this),
      dom_debugger_domain_(this),
      dom_domain_(this),
      dom_storage_domain_(this),
      emulation_domain_(this),
      heap_profiler_domain_(this),
      indexeddb_domain_(this),
      input_domain_(this),
      inspector_domain_(this),
      io_domain_(this),
      layer_tree_domain_(this),
      memory_domain_(this),
      network_domain_(this),
      page_domain_(this),
      profiler_domain_(this),
      rendering_domain_(this),
      runtime_domain_(this),
      security_domain_(this),
      service_worker_domain_(this),
      tracing_domain_(this),
      worker_domain_(this) {}

HeadlessDevToolsClientImpl::~HeadlessDevToolsClientImpl() {}

void HeadlessDevToolsClientImpl::AttachToHost(
    content::DevToolsAgentHost* agent_host) {
  DCHECK(!agent_host_);
  agent_host_ = agent_host;
  agent_host_->AttachClient(this);
}

void HeadlessDevToolsClientImpl::DetachFromHost(
    content::DevToolsAgentHost* agent_host) {
  DCHECK_EQ(agent_host_, agent_host);
  agent_host_->DetachClient(this);
  agent_host_ = nullptr;
  pending_messages_.clear();
}

void HeadlessDevToolsClientImpl::DispatchProtocolMessage(
    content::DevToolsAgentHost* agent_host,
    const std::string& json_message) {
  DCHECK_EQ(agent_host_, agent_host);
  std::unique_ptr<base::Value> message =
      base::JSONReader::Read(json_message, base::JSON_PARSE_RFC);
  const base::DictionaryValue* message_dict;
  if (!message || !message->GetAsDictionary(&message_dict)) {
    NOTREACHED() << "Badly formed reply";
    return;
  }
  if (!DispatchMessageReply(*message_dict) && !DispatchEvent(*message_dict))
    DLOG(ERROR) << "Unhandled protocol message: " << json_message;
}

bool HeadlessDevToolsClientImpl::DispatchMessageReply(
    const base::DictionaryValue& message_dict) {
  int id = 0;
  if (!message_dict.GetInteger("id", &id))
    return false;
  auto it = pending_messages_.find(id);
  if (it == pending_messages_.end()) {
    NOTREACHED() << "Unexpected reply";
    return false;
  }
  Callback callback = std::move(it->second);
  pending_messages_.erase(it);
  if (!callback.callback_with_result.is_null()) {
    const base::DictionaryValue* result_dict;
    if (!message_dict.GetDictionary("result", &result_dict)) {
      NOTREACHED() << "Badly formed reply result";
      return false;
    }
    callback.callback_with_result.Run(*result_dict);
  } else if (!callback.callback.is_null()) {
    callback.callback.Run();
  }
  return true;
}

bool HeadlessDevToolsClientImpl::DispatchEvent(
    const base::DictionaryValue& message_dict) {
  std::string method;
  if (!message_dict.GetString("method", &method))
    return false;
  auto it = event_handlers_.find(method);
  if (it == event_handlers_.end()) {
    NOTREACHED() << "Unknown event: " << method;
    return false;
  }
  if (!it->second.is_null()) {
    const base::DictionaryValue* result_dict;
    if (!message_dict.GetDictionary("params", &result_dict)) {
      NOTREACHED() << "Badly formed event parameters";
      return false;
    }
    it->second.Run(*result_dict);
  }
  return true;
}

void HeadlessDevToolsClientImpl::AgentHostClosed(
    content::DevToolsAgentHost* agent_host,
    bool replaced_with_another_client) {
  DCHECK_EQ(agent_host_, agent_host);
  agent_host = nullptr;
  pending_messages_.clear();
}

accessibility::Domain* HeadlessDevToolsClientImpl::GetAccessibility() {
  return &accessibility_domain_;
}

animation::Domain* HeadlessDevToolsClientImpl::GetAnimation() {
  return &animation_domain_;
}

application_cache::Domain* HeadlessDevToolsClientImpl::GetApplicationCache() {
  return &application_cache_domain_;
}

cache_storage::Domain* HeadlessDevToolsClientImpl::GetCacheStorage() {
  return &cache_storage_domain_;
}

console::Domain* HeadlessDevToolsClientImpl::GetConsole() {
  return &console_domain_;
}

css::Domain* HeadlessDevToolsClientImpl::GetCSS() {
  return &css_domain_;
}

database::Domain* HeadlessDevToolsClientImpl::GetDatabase() {
  return &database_domain_;
}

debugger::Domain* HeadlessDevToolsClientImpl::GetDebugger() {
  return &debugger_domain_;
}

device_orientation::Domain* HeadlessDevToolsClientImpl::GetDeviceOrientation() {
  return &device_orientation_domain_;
}

dom_debugger::Domain* HeadlessDevToolsClientImpl::GetDOMDebugger() {
  return &dom_debugger_domain_;
}

dom::Domain* HeadlessDevToolsClientImpl::GetDOM() {
  return &dom_domain_;
}

dom_storage::Domain* HeadlessDevToolsClientImpl::GetDOMStorage() {
  return &dom_storage_domain_;
}

emulation::Domain* HeadlessDevToolsClientImpl::GetEmulation() {
  return &emulation_domain_;
}

heap_profiler::Domain* HeadlessDevToolsClientImpl::GetHeapProfiler() {
  return &heap_profiler_domain_;
}

indexeddb::Domain* HeadlessDevToolsClientImpl::GetIndexedDB() {
  return &indexeddb_domain_;
}

input::Domain* HeadlessDevToolsClientImpl::GetInput() {
  return &input_domain_;
}

inspector::Domain* HeadlessDevToolsClientImpl::GetInspector() {
  return &inspector_domain_;
}

io::Domain* HeadlessDevToolsClientImpl::GetIO() {
  return &io_domain_;
}

layer_tree::Domain* HeadlessDevToolsClientImpl::GetLayerTree() {
  return &layer_tree_domain_;
}

memory::Domain* HeadlessDevToolsClientImpl::GetMemory() {
  return &memory_domain_;
}

network::Domain* HeadlessDevToolsClientImpl::GetNetwork() {
  return &network_domain_;
}

page::Domain* HeadlessDevToolsClientImpl::GetPage() {
  return &page_domain_;
}

profiler::Domain* HeadlessDevToolsClientImpl::GetProfiler() {
  return &profiler_domain_;
}

rendering::Domain* HeadlessDevToolsClientImpl::GetRendering() {
  return &rendering_domain_;
}

runtime::Domain* HeadlessDevToolsClientImpl::GetRuntime() {
  return &runtime_domain_;
}

security::Domain* HeadlessDevToolsClientImpl::GetSecurity() {
  return &security_domain_;
}

service_worker::Domain* HeadlessDevToolsClientImpl::GetServiceWorker() {
  return &service_worker_domain_;
}

tracing::Domain* HeadlessDevToolsClientImpl::GetTracing() {
  return &tracing_domain_;
}

worker::Domain* HeadlessDevToolsClientImpl::GetWorker() {
  return &worker_domain_;
}

template <typename CallbackType>
void HeadlessDevToolsClientImpl::FinalizeAndSendMessage(
    base::DictionaryValue* message,
    CallbackType callback) {
  DCHECK(agent_host_);
  int id = next_message_id_++;
  message->SetInteger("id", id);
  std::string json_message;
  base::JSONWriter::Write(*message, &json_message);
  pending_messages_[id] = Callback(callback);
  agent_host_->DispatchProtocolMessage(this, json_message);
}

template <typename CallbackType>
void HeadlessDevToolsClientImpl::SendMessageWithParams(
    const char* method,
    std::unique_ptr<base::Value> params,
    CallbackType callback) {
  base::DictionaryValue message;
  message.SetString("method", method);
  message.Set("params", std::move(params));
  FinalizeAndSendMessage(&message, std::move(callback));
}

template <typename CallbackType>
void HeadlessDevToolsClientImpl::SendMessageWithoutParams(
    const char* method,
    CallbackType callback) {
  base::DictionaryValue message;
  message.SetString("method", method);
  FinalizeAndSendMessage(&message, std::move(callback));
}

void HeadlessDevToolsClientImpl::SendMessage(
    const char* method,
    std::unique_ptr<base::Value> params,
    base::Callback<void(const base::Value&)> callback) {
  SendMessageWithParams(method, std::move(params), std::move(callback));
}

void HeadlessDevToolsClientImpl::SendMessage(
    const char* method,
    std::unique_ptr<base::Value> params,
    base::Callback<void()> callback) {
  SendMessageWithParams(method, std::move(params), std::move(callback));
}

void HeadlessDevToolsClientImpl::SendMessage(
    const char* method,
    base::Callback<void(const base::Value&)> callback) {
  SendMessageWithoutParams(method, std::move(callback));
}

void HeadlessDevToolsClientImpl::SendMessage(const char* method,
                                             base::Callback<void()> callback) {
  SendMessageWithoutParams(method, std::move(callback));
}

void HeadlessDevToolsClientImpl::RegisterEventHandler(
    const char* method,
    base::Callback<void(const base::Value&)> callback) {
  DCHECK(event_handlers_.find(method) == event_handlers_.end());
  event_handlers_[method] = callback;
}

HeadlessDevToolsClientImpl::Callback::Callback() {}

HeadlessDevToolsClientImpl::Callback::Callback(Callback&& other) = default;

HeadlessDevToolsClientImpl::Callback::Callback(base::Callback<void()> callback)
    : callback(callback) {}

HeadlessDevToolsClientImpl::Callback::Callback(
    base::Callback<void(const base::Value&)> callback)
    : callback_with_result(callback) {}

HeadlessDevToolsClientImpl::Callback::~Callback() {}

HeadlessDevToolsClientImpl::Callback& HeadlessDevToolsClientImpl::Callback::
operator=(Callback&& other) = default;

}  // namespace headless
