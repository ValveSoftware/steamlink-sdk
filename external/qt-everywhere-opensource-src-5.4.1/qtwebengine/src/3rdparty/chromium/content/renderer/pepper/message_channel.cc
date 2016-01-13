// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/message_channel.h"

#include <cstdlib>
#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "content/renderer/pepper/host_array_buffer_var.h"
#include "content/renderer/pepper/npapi_glue.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/pepper/plugin_module.h"
#include "content/renderer/pepper/v8_var_converter.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/scoped_pp_var.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/shared_impl/var_tracker.h"
#include "third_party/WebKit/public/web/WebBindings.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebDOMMessageEvent.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebNode.h"
#include "third_party/WebKit/public/web/WebPluginContainer.h"
#include "third_party/WebKit/public/web/WebSerializedScriptValue.h"
#include "v8/include/v8.h"

using ppapi::ArrayBufferVar;
using ppapi::PpapiGlobals;
using ppapi::ScopedPPVar;
using ppapi::StringVar;
using blink::WebBindings;
using blink::WebElement;
using blink::WebDOMEvent;
using blink::WebDOMMessageEvent;
using blink::WebPluginContainer;
using blink::WebSerializedScriptValue;

namespace content {

namespace {

const char kPostMessage[] = "postMessage";
const char kPostMessageAndAwaitResponse[] = "postMessageAndAwaitResponse";
const char kV8ToVarConversionError[] =
    "Failed to convert a PostMessage "
    "argument from a JavaScript value to a PP_Var. It may have cycles or be of "
    "an unsupported type.";
const char kVarToV8ConversionError[] =
    "Failed to convert a PostMessage "
    "argument from a PP_Var to a Javascript value. It may have cycles or be of "
    "an unsupported type.";

// Helper function to get the MessageChannel that is associated with an
// NPObject*.
MessageChannel* ToMessageChannel(NPObject* object) {
  return static_cast<MessageChannel::MessageChannelNPObject*>(object)
      ->message_channel.get();
}

NPObject* ToPassThroughObject(NPObject* object) {
  MessageChannel* channel = ToMessageChannel(object);
  return channel ? channel->passthrough_object() : NULL;
}

// Return true iff |identifier| is equal to |string|.
bool IdentifierIs(NPIdentifier identifier, const char string[]) {
  return WebBindings::getStringIdentifier(string) == identifier;
}

bool HasDevChannelPermission(NPObject* channel_object) {
  MessageChannel* channel = ToMessageChannel(channel_object);
  if (!channel)
    return false;
  return channel->instance()->module()->permissions().HasPermission(
      ppapi::PERMISSION_DEV_CHANNEL);
}

//------------------------------------------------------------------------------
// Implementations of NPClass functions.  These are here to:
// - Implement postMessage behavior.
// - Forward calls to the 'passthrough' object to allow backwards-compatibility
//   with GetInstanceObject() objects.
//------------------------------------------------------------------------------
NPObject* MessageChannelAllocate(NPP npp, NPClass* the_class) {
  return new MessageChannel::MessageChannelNPObject;
}

void MessageChannelDeallocate(NPObject* object) {
  MessageChannel::MessageChannelNPObject* instance =
      static_cast<MessageChannel::MessageChannelNPObject*>(object);
  delete instance;
}

bool MessageChannelHasMethod(NPObject* np_obj, NPIdentifier name) {
  if (!np_obj)
    return false;

  if (IdentifierIs(name, kPostMessage))
    return true;
  if (IdentifierIs(name, kPostMessageAndAwaitResponse) &&
      HasDevChannelPermission(np_obj)) {
    return true;
  }
  // Other method names we will pass to the passthrough object, if we have one.
  NPObject* passthrough = ToPassThroughObject(np_obj);
  if (passthrough)
    return WebBindings::hasMethod(NULL, passthrough, name);
  return false;
}

bool MessageChannelInvoke(NPObject* np_obj,
                          NPIdentifier name,
                          const NPVariant* args,
                          uint32 arg_count,
                          NPVariant* result) {
  if (!np_obj)
    return false;

  MessageChannel* message_channel = ToMessageChannel(np_obj);
  if (!message_channel)
    return false;

  // Check to see if we should handle this function ourselves.
  if (IdentifierIs(name, kPostMessage) && (arg_count == 1)) {
    message_channel->PostMessageToNative(&args[0]);
    return true;
  } else if (IdentifierIs(name, kPostMessageAndAwaitResponse) &&
             (arg_count == 1) &&
             HasDevChannelPermission(np_obj)) {
    message_channel->PostBlockingMessageToNative(&args[0], result);
    return true;
  }

  // Other method calls we will pass to the passthrough object, if we have one.
  NPObject* passthrough = ToPassThroughObject(np_obj);
  if (passthrough) {
    return WebBindings::invoke(
        NULL, passthrough, name, args, arg_count, result);
  }
  return false;
}

bool MessageChannelInvokeDefault(NPObject* np_obj,
                                 const NPVariant* args,
                                 uint32 arg_count,
                                 NPVariant* result) {
  if (!np_obj)
    return false;

  // Invoke on the passthrough object, if we have one.
  NPObject* passthrough = ToPassThroughObject(np_obj);
  if (passthrough) {
    return WebBindings::invokeDefault(
        NULL, passthrough, args, arg_count, result);
  }
  return false;
}

bool MessageChannelHasProperty(NPObject* np_obj, NPIdentifier name) {
  if (!np_obj)
    return false;

  MessageChannel* message_channel = ToMessageChannel(np_obj);
  if (message_channel) {
    if (message_channel->GetReadOnlyProperty(name, NULL))
      return true;
  }

  // Invoke on the passthrough object, if we have one.
  NPObject* passthrough = ToPassThroughObject(np_obj);
  if (passthrough)
    return WebBindings::hasProperty(NULL, passthrough, name);
  return false;
}

bool MessageChannelGetProperty(NPObject* np_obj,
                               NPIdentifier name,
                               NPVariant* result) {
  if (!np_obj)
    return false;

  // Don't allow getting the postMessage functions.
  if (IdentifierIs(name, kPostMessage))
    return false;
  if (IdentifierIs(name, kPostMessageAndAwaitResponse) &&
      HasDevChannelPermission(np_obj)) {
     return false;
  }
  MessageChannel* message_channel = ToMessageChannel(np_obj);
  if (message_channel) {
    if (message_channel->GetReadOnlyProperty(name, result))
      return true;
  }

  // Invoke on the passthrough object, if we have one.
  NPObject* passthrough = ToPassThroughObject(np_obj);
  if (passthrough)
    return WebBindings::getProperty(NULL, passthrough, name, result);
  return false;
}

bool MessageChannelSetProperty(NPObject* np_obj,
                               NPIdentifier name,
                               const NPVariant* variant) {
  if (!np_obj)
    return false;

  // Don't allow setting the postMessage functions.
  if (IdentifierIs(name, kPostMessage))
    return false;
  if (IdentifierIs(name, kPostMessageAndAwaitResponse) &&
      HasDevChannelPermission(np_obj)) {
    return false;
  }
  // Invoke on the passthrough object, if we have one.
  NPObject* passthrough = ToPassThroughObject(np_obj);
  if (passthrough)
    return WebBindings::setProperty(NULL, passthrough, name, variant);
  return false;
}

bool MessageChannelEnumerate(NPObject* np_obj,
                             NPIdentifier** value,
                             uint32_t* count) {
  if (!np_obj)
    return false;

  // Invoke on the passthrough object, if we have one, to enumerate its
  // properties.
  NPObject* passthrough = ToPassThroughObject(np_obj);
  if (passthrough) {
    bool success = WebBindings::enumerate(NULL, passthrough, value, count);
    if (success) {
      // Add postMessage to the list and return it.
      if (std::numeric_limits<size_t>::max() / sizeof(NPIdentifier) <=
          static_cast<size_t>(*count) + 1)  // Else, "always false" x64 warning.
        return false;
      NPIdentifier* new_array = static_cast<NPIdentifier*>(
          std::malloc(sizeof(NPIdentifier) * (*count + 1)));
      std::memcpy(new_array, *value, sizeof(NPIdentifier) * (*count));
      new_array[*count] = WebBindings::getStringIdentifier(kPostMessage);
      std::free(*value);
      *value = new_array;
      ++(*count);
      return true;
    }
  }

  // Otherwise, build an array that includes only postMessage.
  *value = static_cast<NPIdentifier*>(malloc(sizeof(NPIdentifier)));
  (*value)[0] = WebBindings::getStringIdentifier(kPostMessage);
  *count = 1;
  return true;
}

NPClass message_channel_class = {
    NP_CLASS_STRUCT_VERSION,      &MessageChannelAllocate,
    &MessageChannelDeallocate,    NULL,
    &MessageChannelHasMethod,     &MessageChannelInvoke,
    &MessageChannelInvokeDefault, &MessageChannelHasProperty,
    &MessageChannelGetProperty,   &MessageChannelSetProperty,
    NULL,                         &MessageChannelEnumerate, };

}  // namespace

// MessageChannel --------------------------------------------------------------
struct MessageChannel::VarConversionResult {
  VarConversionResult() : success_(false), conversion_completed_(false) {}
  void ConversionCompleted(const ScopedPPVar& var,
                           bool success) {
    conversion_completed_ = true;
    var_ = var;
    success_ = success;
  }
  const ScopedPPVar& var() const { return var_; }
  bool success() const { return success_; }
  bool conversion_completed() const { return conversion_completed_; }

 private:
  ScopedPPVar var_;
  bool success_;
  bool conversion_completed_;
};

MessageChannel::MessageChannelNPObject::MessageChannelNPObject() {}

MessageChannel::MessageChannelNPObject::~MessageChannelNPObject() {}

MessageChannel::MessageChannel(PepperPluginInstanceImpl* instance)
    : instance_(instance),
      passthrough_object_(NULL),
      np_object_(NULL),
      early_message_queue_state_(QUEUE_MESSAGES),
      weak_ptr_factory_(this) {
  // Now create an NPObject for receiving calls to postMessage. This sets the
  // reference count to 1.  We release it in the destructor.
  NPObject* obj = WebBindings::createObject(instance_->instanceNPP(),
                                            &message_channel_class);
  DCHECK(obj);
  np_object_ = static_cast<MessageChannel::MessageChannelNPObject*>(obj);
  np_object_->message_channel = weak_ptr_factory_.GetWeakPtr();
}

void MessageChannel::EnqueuePluginMessage(const NPVariant* variant) {
  plugin_message_queue_.push_back(VarConversionResult());
  if (variant->type == NPVariantType_Object) {
    // Convert NPVariantType_Object in to an appropriate PP_Var like Dictionary,
    // Array, etc. Note NPVariantToVar would convert to an "Object" PP_Var,
    // which we don't support for Messaging.

    // Calling WebBindings::toV8Value creates a wrapper around NPVariant so it
    // won't result in a deep copy.
    v8::Handle<v8::Value> v8_value = WebBindings::toV8Value(variant);
    V8VarConverter v8_var_converter(instance_->pp_instance());
    V8VarConverter::VarResult conversion_result =
        v8_var_converter.FromV8Value(
            v8_value,
            v8::Isolate::GetCurrent()->GetCurrentContext(),
            base::Bind(&MessageChannel::FromV8ValueComplete,
                       weak_ptr_factory_.GetWeakPtr(),
                       &plugin_message_queue_.back()));
    if (conversion_result.completed_synchronously) {
      plugin_message_queue_.back().ConversionCompleted(
          conversion_result.var,
          conversion_result.success);
    }
  } else {
    plugin_message_queue_.back().ConversionCompleted(
        ScopedPPVar(ScopedPPVar::PassRef(),
                    NPVariantToPPVar(instance(), variant)),
        true);
    DCHECK(plugin_message_queue_.back().var().get().type != PP_VARTYPE_OBJECT);
  }
}

void MessageChannel::PostMessageToJavaScript(PP_Var message_data) {
  v8::HandleScope scope(v8::Isolate::GetCurrent());

  // Because V8 is probably not on the stack for Native->JS calls, we need to
  // enter the appropriate context for the plugin.
  WebPluginContainer* container = instance_->container();
  // It's possible that container() is NULL if the plugin has been removed from
  // the DOM (but the PluginInstance is not destroyed yet).
  if (!container)
    return;

  v8::Local<v8::Context> context =
      container->element().document().frame()->mainWorldScriptContext();
  // If the page is being destroyed, the context may be empty.
  if (context.IsEmpty())
    return;
  v8::Context::Scope context_scope(context);

  v8::Handle<v8::Value> v8_val;
  if (!V8VarConverter(instance_->pp_instance())
           .ToV8Value(message_data, context, &v8_val)) {
    PpapiGlobals::Get()->LogWithSource(instance_->pp_instance(),
                                       PP_LOGLEVEL_ERROR,
                                       std::string(),
                                       kVarToV8ConversionError);
    return;
  }

  WebSerializedScriptValue serialized_val =
      WebSerializedScriptValue::serialize(v8_val);

  if (early_message_queue_state_ != SEND_DIRECTLY) {
    // We can't just PostTask here; the messages would arrive out of
    // order. Instead, we queue them up until we're ready to post
    // them.
    early_message_queue_.push_back(serialized_val);
  } else {
    // The proxy sent an asynchronous message, so the plugin is already
    // unblocked. Therefore, there's no need to PostTask.
    DCHECK(early_message_queue_.empty());
    PostMessageToJavaScriptImpl(serialized_val);
  }
}

void MessageChannel::Start() {
  // We PostTask here instead of draining the message queue directly
  // since we haven't finished initializing the PepperWebPluginImpl yet, so
  // the plugin isn't available in the DOM.
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&MessageChannel::DrainEarlyMessageQueue,
                 weak_ptr_factory_.GetWeakPtr()));
}

void MessageChannel::FromV8ValueComplete(VarConversionResult* result_holder,
                                         const ScopedPPVar& result,
                                         bool success) {
  result_holder->ConversionCompleted(result, success);
  DrainCompletedPluginMessages();
}

void MessageChannel::DrainCompletedPluginMessages() {
  if (early_message_queue_state_ == QUEUE_MESSAGES)
    return;

  while (!plugin_message_queue_.empty() &&
         plugin_message_queue_.front().conversion_completed()) {
    const VarConversionResult& front = plugin_message_queue_.front();
    if (front.success()) {
      instance_->HandleMessage(front.var());
    } else {
      PpapiGlobals::Get()->LogWithSource(instance()->pp_instance(),
                                         PP_LOGLEVEL_ERROR,
                                         std::string(),
                                         kV8ToVarConversionError);
    }
    plugin_message_queue_.pop_front();
  }
}

void MessageChannel::DrainEarlyMessageQueue() {
  DCHECK(early_message_queue_state_ == QUEUE_MESSAGES);

  // Take a reference on the PluginInstance. This is because JavaScript code
  // may delete the plugin, which would destroy the PluginInstance and its
  // corresponding MessageChannel.
  scoped_refptr<PepperPluginInstanceImpl> instance_ref(instance_);
  while (!early_message_queue_.empty()) {
    PostMessageToJavaScriptImpl(early_message_queue_.front());
    early_message_queue_.pop_front();
  }
  early_message_queue_state_ = SEND_DIRECTLY;

  DrainCompletedPluginMessages();
}

void MessageChannel::PostMessageToJavaScriptImpl(
    const WebSerializedScriptValue& message_data) {
  DCHECK(instance_);

  WebPluginContainer* container = instance_->container();
  // It's possible that container() is NULL if the plugin has been removed from
  // the DOM (but the PluginInstance is not destroyed yet).
  if (!container)
    return;

  WebDOMEvent event =
      container->element().document().createEvent("MessageEvent");
  WebDOMMessageEvent msg_event = event.to<WebDOMMessageEvent>();
  msg_event.initMessageEvent("message",     // type
                             false,         // canBubble
                             false,         // cancelable
                             message_data,  // data
                             "",            // origin [*]
                             NULL,          // source [*]
                             "");           // lastEventId
  // [*] Note that the |origin| is only specified for cross-document and server-
  //     sent messages, while |source| is only specified for cross-document
  //     messages:
  //      http://www.whatwg.org/specs/web-apps/current-work/multipage/comms.html
  //     This currently behaves like Web Workers. On Firefox, Chrome, and Safari
  //     at least, postMessage on Workers does not provide the origin or source.
  //     TODO(dmichael):  Add origin if we change to a more iframe-like origin
  //                      policy (see crbug.com/81537)
  container->element().dispatchEvent(msg_event);
}

void MessageChannel::PostMessageToNative(const NPVariant* message_data) {
  EnqueuePluginMessage(message_data);
  DrainCompletedPluginMessages();
}

void MessageChannel::PostBlockingMessageToNative(const NPVariant* message_data,
                                                 NPVariant* np_result) {
  if (early_message_queue_state_ == QUEUE_MESSAGES) {
    WebBindings::setException(
        np_object_,
        "Attempted to call a synchronous method on a plugin that was not "
        "yet loaded.");
    return;
  }

  // If the queue of messages to the plugin is non-empty, we're still waiting on
  // pending Var conversions. This means at some point in the past, JavaScript
  // called postMessage (the async one) and passed us something with a browser-
  // side host (e.g., FileSystem) and we haven't gotten a response from the
  // browser yet. We can't currently support sending a sync message if the
  // plugin does this, because it will break the ordering of the messages
  // arriving at the plugin.
  // TODO(dmichael): Fix this.
  // See https://code.google.com/p/chromium/issues/detail?id=367896#c4
  if (!plugin_message_queue_.empty()) {
    WebBindings::setException(
        np_object_,
        "Failed to convert parameter synchronously, because a prior "
        "call to postMessage contained a type which required asynchronous "
        "transfer which has not completed. Not all types are supported yet by "
        "postMessageAndAwaitResponse. See crbug.com/367896.");
    return;
  }
  ScopedPPVar param;
  if (message_data->type == NPVariantType_Object) {
    // Convert NPVariantType_Object in to an appropriate PP_Var like Dictionary,
    // Array, etc. Note NPVariantToVar would convert to an "Object" PP_Var,
    // which we don't support for Messaging.
    v8::Handle<v8::Value> v8_value = WebBindings::toV8Value(message_data);
    V8VarConverter v8_var_converter(instance_->pp_instance());
    bool success = v8_var_converter.FromV8ValueSync(
        v8_value,
        v8::Isolate::GetCurrent()->GetCurrentContext(),
        &param);
    if (!success) {
      WebBindings::setException(
          np_object_,
          "Failed to convert the given parameter to a PP_Var to send to "
          "the plugin.");
      return;
    }
  } else {
    param = ScopedPPVar(ScopedPPVar::PassRef(),
                        NPVariantToPPVar(instance(), message_data));
  }
  ScopedPPVar pp_result;
  bool was_handled = instance_->HandleBlockingMessage(param, &pp_result);
  if (!was_handled) {
    WebBindings::setException(
        np_object_,
        "The plugin has not registered a handler for synchronous messages. "
        "See the documentation for PPB_Messaging::RegisterMessageHandler "
        "and PPP_MessageHandler.");
    return;
  }
  v8::Handle<v8::Value> v8_val;
  if (!V8VarConverter(instance_->pp_instance()).ToV8Value(
          pp_result.get(),
          v8::Isolate::GetCurrent()->GetCurrentContext(),
          &v8_val)) {
    WebBindings::setException(
        np_object_,
        "Failed to convert the plugin's result to a JavaScript type.");
    return;
  }
  // Success! Convert the result to an NPVariant.
  WebBindings::toNPVariant(v8_val, NULL, np_result);
}

MessageChannel::~MessageChannel() {
  WebBindings::releaseObject(np_object_);
  if (passthrough_object_)
    WebBindings::releaseObject(passthrough_object_);
}

void MessageChannel::SetPassthroughObject(NPObject* passthrough) {
  // Retain the passthrough object; We need to ensure it lives as long as this
  // MessageChannel.
  if (passthrough)
    WebBindings::retainObject(passthrough);

  // If we had a passthrough set already, release it. Note that we retain the
  // incoming passthrough object first, so that we behave correctly if anyone
  // invokes:
  //   SetPassthroughObject(passthrough_object());
  if (passthrough_object_)
    WebBindings::releaseObject(passthrough_object_);

  passthrough_object_ = passthrough;
}

bool MessageChannel::GetReadOnlyProperty(NPIdentifier key,
                                         NPVariant* value) const {
  std::map<NPIdentifier, ScopedPPVar>::const_iterator it =
      internal_properties_.find(key);
  if (it != internal_properties_.end()) {
    if (value)
      return PPVarToNPVariant(it->second.get(), value);
    return true;
  }
  return false;
}

void MessageChannel::SetReadOnlyProperty(PP_Var key, PP_Var value) {
  internal_properties_[PPVarToNPIdentifier(key)] = ScopedPPVar(value);
}

}  // namespace content
