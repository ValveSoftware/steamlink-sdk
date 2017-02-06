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
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/values.h"
#include "components/guest_view/common/guest_view_constants.h"
#include "content/public/child/v8_value_converter.h"
#include "content/public/common/child_process_host.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "extensions/common/api/messaging/message.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/manifest_handlers/externally_connectable.h"
#include "extensions/renderer/event_bindings.h"
#include "extensions/renderer/extension_frame_helper.h"
#include "extensions/renderer/gc_callback.h"
#include "extensions/renderer/object_backed_native_handler.h"
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
// var extension =
//    new chrome.Extension('00123456789abcdef0123456789abcdef0123456');
// var port = runtime.connect();
// port.postMessage('Can you hear me now?');
// port.onmessage.addListener(function(msg, port) {
//   alert('response=' + msg);
//   port.postMessage('I got your reponse');
// });

using content::RenderThread;
using content::V8ValueConverter;

namespace extensions {

using v8_helpers::ToV8String;
using v8_helpers::ToV8StringUnsafe;
using v8_helpers::IsEmptyOrUndefied;

namespace {

class ExtensionImpl : public ObjectBackedNativeHandler {
 public:
  explicit ExtensionImpl(ScriptContext* context)
      : ObjectBackedNativeHandler(context), weak_ptr_factory_(this) {
    RouteFunction(
        "CloseChannel",
        base::Bind(&ExtensionImpl::CloseChannel, base::Unretained(this)));
    RouteFunction(
        "PostMessage",
        base::Bind(&ExtensionImpl::PostMessage, base::Unretained(this)));
    // TODO(fsamuel, kalman): Move BindToGC out of messaging natives.
    RouteFunction("BindToGC",
                  base::Bind(&ExtensionImpl::BindToGC, base::Unretained(this)));
  }

  ~ExtensionImpl() override {}

 private:
  // Sends a message along the given channel.
  void PostMessage(const v8::FunctionCallbackInfo<v8::Value>& args) {
    // Arguments are (int32_t port_id, string message).
    CHECK(args.Length() == 2 && args[0]->IsInt32() && args[1]->IsString());

    int port_id = args[0].As<v8::Int32>()->Value();

    content::RenderFrame* render_frame = context()->GetRenderFrame();
    if (render_frame) {
      render_frame->Send(new ExtensionHostMsg_PostMessage(
          render_frame->GetRoutingID(), port_id,
          Message(*v8::String::Utf8Value(args[1]),
                  blink::WebUserGestureIndicator::isProcessingUserGesture())));
    }
  }

  // Close a port, optionally forcefully (i.e. close the whole channel instead
  // of just the given port).
  void CloseChannel(const v8::FunctionCallbackInfo<v8::Value>& args) {
    // Arguments are (int32_t port_id, bool force_close).
    CHECK_EQ(2, args.Length());
    CHECK(args[0]->IsInt32());
    CHECK(args[1]->IsBoolean());

    int port_id = args[0].As<v8::Int32>()->Value();
    bool force_close = args[1].As<v8::Boolean>()->Value();
    ClosePort(port_id, force_close);
  }

  // void BindToGC(object, callback, port_id)
  //
  // Binds |callback| to be invoked *sometime after* |object| is garbage
  // collected. We don't call the method re-entrantly so as to avoid executing
  // JS in some bizarro undefined mid-GC state, nor do we then call into the
  // script context if it's been invalidated.
  //
  // If the script context *is* invalidated in the meantime, as a slight hack,
  // release the port with ID |port_id| if it's >= 0.
  void BindToGC(const v8::FunctionCallbackInfo<v8::Value>& args) {
    CHECK(args.Length() == 3 && args[0]->IsObject() && args[1]->IsFunction() &&
          args[2]->IsInt32());
    int port_id = args[2].As<v8::Int32>()->Value();
    base::Closure fallback = base::Bind(&base::DoNothing);
    if (port_id >= 0) {
      // TODO(robwu): Falling back to closing the port shouldn't be needed. If
      // the script context is destroyed, then the frame has navigated. But that
      // is already detected by the browser, so this logic is redundant. Remove
      // this fallback (and move BindToGC out of messaging because it is also
      // used in other places that have nothing to do with messaging...).
      fallback =
          base::Bind(&ExtensionImpl::ClosePort, weak_ptr_factory_.GetWeakPtr(),
                     port_id, false /* force_close */);
    }
    // Destroys itself when the object is GC'd or context is invalidated.
    new GCCallback(context(), args[0].As<v8::Object>(),
                   args[1].As<v8::Function>(), fallback);
  }

  // See ExtensionImpl::CloseChannel for documentation.
  // TODO(robwu): Merge this logic with CloseChannel once the TODO in BindToGC
  // has been addressed.
  void ClosePort(int port_id, bool force_close) {
    content::RenderFrame* render_frame = context()->GetRenderFrame();
    if (render_frame) {
      render_frame->Send(new ExtensionHostMsg_CloseMessagePort(
          render_frame->GetRoutingID(), port_id, force_close));
    }
  }

  base::WeakPtrFactory<ExtensionImpl> weak_ptr_factory_;
};

void HasMessagePort(int port_id,
                    bool* has_port,
                    ScriptContext* script_context) {
  if (*has_port)
    return;  // Stop checking if the port was found.

  v8::Isolate* isolate = script_context->isolate();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Value> port_id_handle = v8::Integer::New(isolate, port_id);
  v8::Local<v8::Value> v8_has_port =
      script_context->module_system()->CallModuleMethod("messaging", "hasPort",
                                                        1, &port_id_handle);
  if (IsEmptyOrUndefied(v8_has_port))
    return;
  CHECK(v8_has_port->IsBoolean());
  if (!v8_has_port.As<v8::Boolean>()->Value())
    return;
  *has_port = true;
}

void DispatchOnConnectToScriptContext(
    int target_port_id,
    const std::string& channel_name,
    const ExtensionMsg_TabConnectionInfo* source,
    const ExtensionMsg_ExternalConnectionInfo& info,
    const std::string& tls_channel_id,
    bool* port_created,
    ScriptContext* script_context) {
  v8::Isolate* isolate = script_context->isolate();
  v8::HandleScope handle_scope(isolate);

  std::unique_ptr<V8ValueConverter> converter(V8ValueConverter::create());

  const std::string& source_url_spec = info.source_url.spec();
  std::string target_extension_id = script_context->GetExtensionID();
  const Extension* extension = script_context->extension();

  v8::Local<v8::Value> tab = v8::Null(isolate);
  v8::Local<v8::Value> tls_channel_id_value = v8::Undefined(isolate);
  v8::Local<v8::Value> guest_process_id = v8::Undefined(isolate);
  v8::Local<v8::Value> guest_render_frame_routing_id = v8::Undefined(isolate);

  if (extension) {
    if (!source->tab.empty() && !extension->is_platform_app())
      tab = converter->ToV8Value(&source->tab, script_context->v8_context());

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
      v8::Integer::New(isolate, target_port_id),
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

  if (!IsEmptyOrUndefied(retval)) {
    CHECK(retval->IsBoolean());
    *port_created |= retval.As<v8::Boolean>()->Value();
  } else {
    LOG(ERROR) << "Empty return value from dispatchOnConnect.";
  }
}

void DeliverMessageToScriptContext(const Message& message,
                                   int target_port_id,
                                   ScriptContext* script_context) {
  v8::Isolate* isolate = script_context->isolate();
  v8::HandleScope handle_scope(isolate);

  // Check to see whether the context has this port before bothering to create
  // the message.
  v8::Local<v8::Value> port_id_handle =
      v8::Integer::New(isolate, target_port_id);
  v8::Local<v8::Value> has_port =
      script_context->module_system()->CallModuleMethod("messaging", "hasPort",
                                                        1, &port_id_handle);
  // Could be empty/undefined if an exception was thrown.
  // TODO(kalman): Should this be built into CallModuleMethod?
  if (IsEmptyOrUndefied(has_port))
    return;
  CHECK(has_port->IsBoolean());
  if (!has_port.As<v8::Boolean>()->Value())
    return;

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
    web_user_gesture.reset(new blink::WebScopedUserGesture);

    if (script_context->web_frame()) {
      blink::WebDocument document = script_context->web_frame()->document();
      allow_window_focus.reset(new blink::WebScopedWindowFocusAllowedIndicator(
          &document));
    }
  }

  script_context->module_system()->CallModuleMethod(
      "messaging", "dispatchOnMessage", &arguments);
}

void DispatchOnDisconnectToScriptContext(int port_id,
                                         const std::string& error_message,
                                         ScriptContext* script_context) {
  v8::Isolate* isolate = script_context->isolate();
  v8::HandleScope handle_scope(isolate);

  std::vector<v8::Local<v8::Value>> arguments;
  arguments.push_back(v8::Integer::New(isolate, port_id));
  v8::Local<v8::String> v8_error_message;
  if (!error_message.empty())
    ToV8String(isolate, error_message.c_str(), &v8_error_message);
  if (!v8_error_message.IsEmpty()) {
    arguments.push_back(v8_error_message);
  } else {
    arguments.push_back(v8::Null(isolate));
  }

  script_context->module_system()->CallModuleMethod(
      "messaging", "dispatchOnDisconnect", &arguments);
}

}  // namespace

ObjectBackedNativeHandler* MessagingBindings::Get(ScriptContext* context) {
  return new ExtensionImpl(context);
}

void MessagingBindings::ValidateMessagePort(
    const ScriptContextSet& context_set,
    int port_id,
    content::RenderFrame* render_frame) {
  int routing_id = render_frame->GetRoutingID();

  bool has_port = false;
  context_set.ForEach(render_frame,
                      base::Bind(&HasMessagePort, port_id, &has_port));
  // Note: HasMessagePort invokes a JavaScript function. If the runtime of the
  // extension bindings in JS have been compromised, then |render_frame| may be
  // invalid at this point.

  // A reply is only sent if the port is missing, because the port is assumed to
  // exist unless stated otherwise.
  if (!has_port) {
    content::RenderThread::Get()->Send(
        new ExtensionHostMsg_CloseMessagePort(routing_id, port_id, false));
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

}  // namespace extensions
