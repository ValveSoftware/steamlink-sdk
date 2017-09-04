// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/messaging_bindings.h"

#include <stdint.h>

#include <map>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/lazy_instance.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram_macros.h"
#include "base/values.h"
#include "content/public/child/v8_value_converter.h"
#include "content/public/common/child_process_host.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "extensions/common/api/messaging/message.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/manifest_handlers/externally_connectable.h"
#include "extensions/renderer/extension_frame_helper.h"
#include "extensions/renderer/extension_port.h"
#include "extensions/renderer/gc_callback.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_context_set.h"
#include "extensions/renderer/v8_helpers.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebScopedUserGesture.h"
#include "third_party/WebKit/public/web/WebScopedWindowFocusAllowedIndicator.h"
#include "third_party/WebKit/public/web/WebUserGestureIndicator.h"
#include "v8/include/v8.h"

// Message passing API example (in a content script):
// var port = runtime.connect();
// port.postMessage('Can you hear me now?');
// port.onmessage.addListener(function(msg, port) {
//   alert('response=' + msg);
//   port.postMessage('I got your reponse');
// });

namespace extensions {

using v8_helpers::ToV8String;

namespace {

// A global map between ScriptContext and MessagingBindings.
base::LazyInstance<std::map<ScriptContext*, MessagingBindings*>>
    g_messaging_map = LAZY_INSTANCE_INITIALIZER;

void HasMessagePort(int global_port_id,
                    bool* has_port,
                    ScriptContext* script_context) {
  if (*has_port)
    return;  // Stop checking if the port was found.

  MessagingBindings* bindings = g_messaging_map.Get()[script_context];
  DCHECK(bindings);
  if (bindings->GetPortWithGlobalId(global_port_id))
    *has_port = true;
}

void DispatchOnConnectToScriptContext(
    int global_target_port_id,
    const std::string& channel_name,
    const ExtensionMsg_TabConnectionInfo* source,
    const ExtensionMsg_ExternalConnectionInfo& info,
    const std::string& tls_channel_id,
    bool* port_created,
    ScriptContext* script_context) {
  MessagingBindings* bindings = g_messaging_map.Get()[script_context];
  DCHECK(bindings);

  int opposite_port_id = global_target_port_id ^ 1;
  if (bindings->GetPortWithGlobalId(opposite_port_id))
    return;  // The channel was opened by this same context; ignore it.

  ExtensionPort* port =
      bindings->CreateNewPortWithGlobalId(global_target_port_id);
  int local_port_id = port->local_id();
  // Remove the port.
  base::ScopedClosureRunner remove_port(
      base::Bind(&MessagingBindings::RemovePortWithLocalId,
                 bindings->GetWeakPtr(), local_port_id));

  v8::Isolate* isolate = script_context->isolate();
  v8::HandleScope handle_scope(isolate);


  const std::string& source_url_spec = info.source_url.spec();
  std::string target_extension_id = script_context->GetExtensionID();
  const Extension* extension = script_context->extension();

  v8::Local<v8::Value> tab = v8::Null(isolate);
  v8::Local<v8::Value> tls_channel_id_value = v8::Undefined(isolate);
  v8::Local<v8::Value> guest_process_id = v8::Undefined(isolate);
  v8::Local<v8::Value> guest_render_frame_routing_id = v8::Undefined(isolate);

  if (extension) {
    if (!source->tab.empty() && !extension->is_platform_app()) {
      std::unique_ptr<content::V8ValueConverter> converter(
          content::V8ValueConverter::create());
      tab = converter->ToV8Value(&source->tab, script_context->v8_context());
    }

    ExternallyConnectableInfo* externally_connectable =
        ExternallyConnectableInfo::Get(extension);
    if (externally_connectable &&
        externally_connectable->accepts_tls_channel_id) {
      v8::Local<v8::String> v8_tls_channel_id;
      if (ToV8String(isolate, tls_channel_id.c_str(), &v8_tls_channel_id))
        tls_channel_id_value = v8_tls_channel_id;
    }

    if (info.guest_process_id != content::ChildProcessHost::kInvalidUniqueID) {
      guest_process_id = v8::Integer::New(isolate, info.guest_process_id);
      guest_render_frame_routing_id =
          v8::Integer::New(isolate, info.guest_render_frame_routing_id);
    }
  }

  v8::Local<v8::String> v8_channel_name;
  v8::Local<v8::String> v8_source_id;
  v8::Local<v8::String> v8_target_extension_id;
  v8::Local<v8::String> v8_source_url_spec;
  if (!ToV8String(isolate, channel_name.c_str(), &v8_channel_name) ||
      !ToV8String(isolate, info.source_id.c_str(), &v8_source_id) ||
      !ToV8String(isolate, target_extension_id.c_str(),
                  &v8_target_extension_id) ||
      !ToV8String(isolate, source_url_spec.c_str(), &v8_source_url_spec)) {
    NOTREACHED() << "dispatchOnConnect() passed non-string argument";
    return;
  }

  v8::Local<v8::Value> arguments[] = {
      // portId
      v8::Integer::New(isolate, local_port_id),
      // channelName
      v8_channel_name,
      // sourceTab
      tab,
      // source_frame_id
      v8::Integer::New(isolate, source->frame_id),
      // guestProcessId
      guest_process_id,
      // guestRenderFrameRoutingId
      guest_render_frame_routing_id,
      // sourceExtensionId
      v8_source_id,
      // targetExtensionId
      v8_target_extension_id,
      // sourceUrl
      v8_source_url_spec,
      // tlsChannelId
      tls_channel_id_value,
  };

  v8::Local<v8::Value> retval =
      script_context->module_system()->CallModuleMethod(
          "messaging", "dispatchOnConnect", arraysize(arguments), arguments);

  if (!retval.IsEmpty() && !retval->IsUndefined()) {
    CHECK(retval->IsBoolean());
    bool used = retval.As<v8::Boolean>()->Value();
    *port_created |= used;
    if (used)  // Port was used; don't remove it.
      remove_port.ReplaceClosure(base::Closure());
  } else {
    LOG(ERROR) << "Empty return value from dispatchOnConnect.";
  }
}

void DeliverMessageToScriptContext(const Message& message,
                                   int global_target_port_id,
                                   ScriptContext* script_context) {
  MessagingBindings* bindings = g_messaging_map.Get()[script_context];
  DCHECK(bindings);
  ExtensionPort* port = bindings->GetPortWithGlobalId(global_target_port_id);
  if (!port)
    return;

  v8::Isolate* isolate = script_context->isolate();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Value> port_id_handle =
      v8::Integer::New(isolate, port->local_id());

  v8::Local<v8::String> v8_data;
  if (!ToV8String(isolate, message.data.c_str(), &v8_data))
    return;
  std::vector<v8::Local<v8::Value>> arguments;
  arguments.push_back(v8_data);
  arguments.push_back(port_id_handle);

  std::unique_ptr<blink::WebScopedUserGesture> web_user_gesture;
  std::unique_ptr<blink::WebScopedWindowFocusAllowedIndicator>
      allow_window_focus;
  if (message.user_gesture) {
    web_user_gesture.reset(
        new blink::WebScopedUserGesture(script_context->web_frame()));

    if (script_context->web_frame()) {
      blink::WebDocument document = script_context->web_frame()->document();
      allow_window_focus.reset(new blink::WebScopedWindowFocusAllowedIndicator(
          &document));
    }
  }

  script_context->module_system()->CallModuleMethodSafe(
      "messaging", "dispatchOnMessage", &arguments);
}

void DispatchOnDisconnectToScriptContext(int global_port_id,
                                         const std::string& error_message,
                                         ScriptContext* script_context) {
  MessagingBindings* bindings = g_messaging_map.Get()[script_context];
  DCHECK(bindings);
  ExtensionPort* port = bindings->GetPortWithGlobalId(global_port_id);
  if (!port)
    return;

  v8::Isolate* isolate = script_context->isolate();
  v8::HandleScope handle_scope(isolate);

  std::vector<v8::Local<v8::Value>> arguments;
  arguments.push_back(v8::Integer::New(isolate, port->local_id()));
  v8::Local<v8::String> v8_error_message;
  if (!error_message.empty())
    ToV8String(isolate, error_message.c_str(), &v8_error_message);
  if (!v8_error_message.IsEmpty()) {
    arguments.push_back(v8_error_message);
  } else {
    arguments.push_back(v8::Null(isolate));
  }

  script_context->module_system()->CallModuleMethodSafe(
      "messaging", "dispatchOnDisconnect", &arguments);
}

}  // namespace

MessagingBindings::MessagingBindings(ScriptContext* context)
    : ObjectBackedNativeHandler(context), weak_ptr_factory_(this) {
  g_messaging_map.Get()[context] = this;
  RouteFunction("CloseChannel", base::Bind(&MessagingBindings::CloseChannel,
                                           base::Unretained(this)));
  RouteFunction("PostMessage", base::Bind(&MessagingBindings::PostMessage,
                                          base::Unretained(this)));
  // TODO(fsamuel, kalman): Move BindToGC out of messaging natives.
  RouteFunction("BindToGC", base::Bind(&MessagingBindings::BindToGC,
                                       base::Unretained(this)));
  RouteFunction("OpenChannelToExtension", "runtime.connect",
                base::Bind(&MessagingBindings::OpenChannelToExtension,
                           base::Unretained(this)));
  RouteFunction("OpenChannelToNativeApp", "runtime.connectNative",
                base::Bind(&MessagingBindings::OpenChannelToNativeApp,
                           base::Unretained(this)));
  RouteFunction(
      "OpenChannelToTab",
      base::Bind(&MessagingBindings::OpenChannelToTab, base::Unretained(this)));
}

MessagingBindings::~MessagingBindings() {
  g_messaging_map.Get().erase(context());
  if (ports_created_normal_ > 0 || ports_created_in_before_unload_ > 0 ||
      ports_created_in_unload_ > 0) {
    UMA_HISTOGRAM_COUNTS_1000(
        "Extensions.Messaging.ExtensionPortsCreated.InBeforeUnload",
        ports_created_in_before_unload_);
    UMA_HISTOGRAM_COUNTS_1000(
        "Extensions.Messaging.ExtensionPortsCreated.InUnload",
        ports_created_in_unload_);
    UMA_HISTOGRAM_COUNTS_1000(
        "Extensions.Messaging.ExtensionPortsCreated.Normal",
        ports_created_normal_);
  }
}

// static
void MessagingBindings::ValidateMessagePort(
    const ScriptContextSet& context_set,
    int global_port_id,
    content::RenderFrame* render_frame) {
  int routing_id = render_frame->GetRoutingID();

  bool has_port = false;
  context_set.ForEach(render_frame,
                      base::Bind(&HasMessagePort, global_port_id, &has_port));

  // A reply is only sent if the port is missing, because the port is assumed to
  // exist unless stated otherwise.
  if (!has_port) {
    content::RenderThread::Get()->Send(
        new ExtensionHostMsg_CloseMessagePort(routing_id,
                                              global_port_id, false));
  }
}

// static
void MessagingBindings::DispatchOnConnect(
    const ScriptContextSet& context_set,
    int target_port_id,
    const std::string& channel_name,
    const ExtensionMsg_TabConnectionInfo& source,
    const ExtensionMsg_ExternalConnectionInfo& info,
    const std::string& tls_channel_id,
    content::RenderFrame* restrict_to_render_frame) {
  int routing_id = restrict_to_render_frame
                       ? restrict_to_render_frame->GetRoutingID()
                       : MSG_ROUTING_NONE;
  bool port_created = false;
  context_set.ForEach(
      info.target_id, restrict_to_render_frame,
      base::Bind(&DispatchOnConnectToScriptContext, target_port_id,
                 channel_name, &source, info, tls_channel_id, &port_created));
  // Note: |restrict_to_render_frame| may have been deleted at this point!

  if (port_created) {
    content::RenderThread::Get()->Send(
        new ExtensionHostMsg_OpenMessagePort(routing_id, target_port_id));
  } else {
    content::RenderThread::Get()->Send(new ExtensionHostMsg_CloseMessagePort(
        routing_id, target_port_id, false));
  }
}

// static
void MessagingBindings::DeliverMessage(
    const ScriptContextSet& context_set,
    int target_port_id,
    const Message& message,
    content::RenderFrame* restrict_to_render_frame) {
  context_set.ForEach(
      restrict_to_render_frame,
      base::Bind(&DeliverMessageToScriptContext, message, target_port_id));
}

// static
void MessagingBindings::DispatchOnDisconnect(
    const ScriptContextSet& context_set,
    int port_id,
    const std::string& error_message,
    content::RenderFrame* restrict_to_render_frame) {
  context_set.ForEach(
      restrict_to_render_frame,
      base::Bind(&DispatchOnDisconnectToScriptContext, port_id, error_message));
}

ExtensionPort* MessagingBindings::GetPortWithGlobalId(int id) {
  for (const auto& key_value : ports_) {
    if (key_value.second->global_id() == id)
      return key_value.second.get();
  }
  return nullptr;
}

ExtensionPort* MessagingBindings::CreateNewPortWithGlobalId(int global_id) {
  int local_id = GetNextLocalId();
  ExtensionPort* port =
      ports_
          .insert(std::make_pair(
              local_id, base::MakeUnique<ExtensionPort>(context(), local_id)))
          .first->second.get();
  port->SetGlobalId(global_id);
  return port;
}

void MessagingBindings::RemovePortWithLocalId(int local_id) {
  ports_.erase(local_id);
}

base::WeakPtr<MessagingBindings> MessagingBindings::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void MessagingBindings::PostMessage(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  // Arguments are (int32_t port_id, string message).
  CHECK(args.Length() == 2);
  CHECK(args[0]->IsInt32());
  CHECK(args[1]->IsString());

  int port_id = args[0].As<v8::Int32>()->Value();
  auto iter = ports_.find(port_id);
  if (iter != ports_.end()) {
    iter->second->PostExtensionMessage(base::MakeUnique<Message>(
        *v8::String::Utf8Value(args[1]),
        blink::WebUserGestureIndicator::isProcessingUserGesture()));
  }
}

void MessagingBindings::CloseChannel(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  // Arguments are (int32_t port_id, bool force_close).
  CHECK_EQ(2, args.Length());
  CHECK(args[0]->IsInt32());
  CHECK(args[1]->IsBoolean());

  int port_id = args[0].As<v8::Int32>()->Value();
  bool force_close = args[1].As<v8::Boolean>()->Value();
  ClosePort(port_id, force_close);
}

void MessagingBindings::BindToGC(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(args.Length() == 3);
  CHECK(args[0]->IsObject());
  CHECK(args[1]->IsFunction());
  CHECK(args[2]->IsInt32());
  int local_port_id = args[2].As<v8::Int32>()->Value();
  base::Closure fallback = base::Bind(&base::DoNothing);
  if (local_port_id >= 0) {
    // TODO(robwu): Falling back to closing the port shouldn't be needed. If
    // the script context is destroyed, then the frame has navigated. But that
    // is already detected by the browser, so this logic is redundant. Remove
    // this fallback (and move BindToGC out of messaging because it is also
    // used in other places that have nothing to do with messaging...).
    fallback = base::Bind(&MessagingBindings::ClosePort,
                          weak_ptr_factory_.GetWeakPtr(), local_port_id,
                          false /* force_close */);
  }
  // Destroys itself when the object is GC'd or context is invalidated.
  new GCCallback(context(), args[0].As<v8::Object>(),
                 args[1].As<v8::Function>(), fallback);
}

void MessagingBindings::OpenChannelToExtension(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderFrame* render_frame = context()->GetRenderFrame();
  if (!render_frame)
    return;

  // The Javascript code should validate/fill the arguments.
  CHECK_EQ(args.Length(), 3);
  CHECK(args[0]->IsString());
  CHECK(args[1]->IsString());
  CHECK(args[2]->IsBoolean());

  int local_id = GetNextLocalId();
  ports_[local_id] = base::MakeUnique<ExtensionPort>(context(), local_id);

  ExtensionMsg_ExternalConnectionInfo info;
  // For messaging APIs, hosted apps should be considered a web page so hide
  // its extension ID.
  const Extension* extension = context()->extension();
  if (extension && !extension->is_hosted_app())
    info.source_id = extension->id();

  info.target_id = *v8::String::Utf8Value(args[0]);
  info.source_url = context()->url();
  std::string channel_name = *v8::String::Utf8Value(args[1]);
  // TODO(devlin): Why is this not part of info?
  bool include_tls_channel_id =
      args.Length() > 2 ? args[2]->BooleanValue() : false;

  ExtensionFrameHelper* frame_helper = ExtensionFrameHelper::Get(render_frame);
  DCHECK(frame_helper);

  blink::WebLocalFrame* web_frame = render_frame->GetWebFrame();
  // If the frame is unloading, we need to assign the port id synchronously to
  // avoid dropping the message on the floor; see crbug.com/660706.
  // TODO(devlin): Investigate whether we need to continue supporting this long-
  // term, and, if so, find an alternative that doesn't require synchronous
  // IPCs.
  if (!web_frame->document().isNull() &&
      (web_frame->document().unloadStartedDoNotUse() ||
       web_frame->document().processingBeforeUnloadDoNotUse())) {
    ports_created_in_before_unload_ +=
        web_frame->document().processingBeforeUnloadDoNotUse() ? 1 : 0;
    ports_created_in_unload_ +=
        web_frame->document().unloadStartedDoNotUse() ? 1 : 0;
    int global_id = frame_helper->RequestSyncPortId(info, channel_name,
                                                    include_tls_channel_id);
    ports_[local_id]->SetGlobalId(global_id);
  } else {
    ++ports_created_normal_;
    frame_helper->RequestPortId(
        info, channel_name, include_tls_channel_id,
        base::Bind(&MessagingBindings::SetGlobalPortId,
                   weak_ptr_factory_.GetWeakPtr(), local_id));
  }

  args.GetReturnValue().Set(static_cast<int32_t>(local_id));
}

void MessagingBindings::OpenChannelToNativeApp(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  // The Javascript code should validate/fill the arguments.
  CHECK_EQ(args.Length(), 1);
  CHECK(args[0]->IsString());
  // This should be checked by our function routing code.
  CHECK(context()->GetAvailability("runtime.connectNative").is_available());

  content::RenderFrame* render_frame = context()->GetRenderFrame();
  if (!render_frame)
    return;

  std::string native_app_name = *v8::String::Utf8Value(args[0]);

  int local_id = GetNextLocalId();
  ports_[local_id] = base::MakeUnique<ExtensionPort>(context(), local_id);

  ExtensionFrameHelper* frame_helper = ExtensionFrameHelper::Get(render_frame);
  DCHECK(frame_helper);
  frame_helper->RequestNativeAppPortId(
      native_app_name,
      base::Bind(&MessagingBindings::SetGlobalPortId,
                 weak_ptr_factory_.GetWeakPtr(), local_id));
  args.GetReturnValue().Set(static_cast<int32_t>(local_id));
}

void MessagingBindings::OpenChannelToTab(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderFrame* render_frame = context()->GetRenderFrame();
  if (!render_frame)
    return;

  // tabs_custom_bindings.js unwraps arguments to tabs.connect/sendMessage and
  // passes them to OpenChannelToTab, in the following order:
  // - |tab_id| - Positive number that specifies the destination of the channel.
  // - |frame_id| - Target frame(s) in the tab where onConnect is dispatched:
  //   -1 for all frames, 0 for the main frame, >0 for a child frame.
  // - |extension_id| - ID of the initiating extension.
  // - |channel_name| - A user-defined channel name.
  CHECK(args.Length() == 4);
  CHECK(args[0]->IsInt32());
  CHECK(args[1]->IsInt32());
  CHECK(args[2]->IsString());
  CHECK(args[3]->IsString());

  int local_id = GetNextLocalId();
  ports_[local_id] = base::MakeUnique<ExtensionPort>(context(), local_id);

  ExtensionMsg_TabTargetConnectionInfo info;
  info.tab_id = args[0]->Int32Value();
  info.frame_id = args[1]->Int32Value();
  // TODO(devlin): Why is this not part of info?
  std::string extension_id = *v8::String::Utf8Value(args[2]);
  std::string channel_name = *v8::String::Utf8Value(args[3]);

  ExtensionFrameHelper* frame_helper = ExtensionFrameHelper::Get(render_frame);
  DCHECK(frame_helper);
  frame_helper->RequestTabPortId(
      info, extension_id, channel_name,
      base::Bind(&MessagingBindings::SetGlobalPortId,
                 weak_ptr_factory_.GetWeakPtr(), local_id));

  args.GetReturnValue().Set(static_cast<int32_t>(local_id));
}

void MessagingBindings::ClosePort(int local_port_id, bool force_close) {
  // TODO(robwu): Merge this logic with CloseChannel once the TODO in BindToGC
  // has been addressed.
  auto iter = ports_.find(local_port_id);
  if (iter != ports_.end()) {
    std::unique_ptr<ExtensionPort> port = std::move(iter->second);
    ports_.erase(iter);
    port->Close(force_close);
    // If the port hasn't been initialized, we can't delete it just yet, because
    // we need to wait for it to send any pending messages. This is important
    // to support the flow:
    //  var port = chrome.runtime.connect();
    //  port.postMessage({message});
    //  port.disconnect();
    if (!port->initialized())
      disconnected_ports_[port->local_id()] = std::move(port);
  }
}

void MessagingBindings::SetGlobalPortId(int local_id, int global_id) {
  auto iter = ports_.find(local_id);
  if (iter != ports_.end()) {
    iter->second->SetGlobalId(global_id);
    return;
  }

  iter = disconnected_ports_.find(local_id);
  DCHECK(iter != disconnected_ports_.end());
  iter->second->SetGlobalId(global_id);
  // Setting the global id dispatches pending messages, so we can delete the
  // port now.
  disconnected_ports_.erase(iter);
}

int MessagingBindings::GetNextLocalId() { return next_local_id_++; }

}  // namespace extensions
