// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/npapi/npobject_util.h"

#include "base/strings/string_util.h"
#include "content/child/npapi/np_channel_base.h"
#include "content/child/npapi/npobject_proxy.h"
#include "content/child/npapi/plugin_host.h"
#include "content/child/plugin_messages.h"
#include "third_party/WebKit/public/web/WebBindings.h"
#include "third_party/npapi/bindings/nphostapi.h"

using blink::WebBindings;

namespace content {

// true if the current process is a plugin process, false otherwise.
static bool g_plugin_process;

namespace {
#if defined(ENABLE_PLUGINS)
// The next 7 functions are called by the plugin code when it's using the
// NPObject.  Plugins always ignore the functions in NPClass (except allocate
// and deallocate), and instead just use the function pointers that were
// passed in NPInitialize.
// When the renderer interacts with an NPObject from the plugin, it of course
// uses the function pointers in its NPClass structure.
static bool NPN_HasMethodPatch(NPP npp,
                               NPObject *npobj,
                               NPIdentifier methodName) {
  return NPObjectProxy::NPHasMethod(npobj, methodName);
}

static bool NPN_InvokePatch(NPP npp, NPObject *npobj,
                            NPIdentifier methodName,
                            const NPVariant *args,
                            uint32_t argCount,
                            NPVariant *result) {
  return NPObjectProxy::NPInvokePrivate(npp, npobj, false, methodName, args,
                                        argCount, result);
}

static bool NPN_InvokeDefaultPatch(NPP npp,
                                   NPObject *npobj,
                                   const NPVariant *args,
                                   uint32_t argCount,
                                   NPVariant *result) {
  return NPObjectProxy::NPInvokePrivate(npp, npobj, true, 0, args, argCount,
                                        result);
}

static bool NPN_HasPropertyPatch(NPP npp,
                                 NPObject *npobj,
                                 NPIdentifier propertyName) {
  return NPObjectProxy::NPHasProperty(npobj, propertyName);
}

static bool NPN_GetPropertyPatch(NPP npp,
                                 NPObject *npobj,
                                 NPIdentifier propertyName,
                                 NPVariant *result) {
  return NPObjectProxy::NPGetProperty(npobj, propertyName, result);
}

static bool NPN_SetPropertyPatch(NPP npp,
                                 NPObject *npobj,
                                 NPIdentifier propertyName,
                                 const NPVariant *value) {
  return NPObjectProxy::NPSetProperty(npobj, propertyName, value);
}

static bool NPN_RemovePropertyPatch(NPP npp,
                                    NPObject *npobj,
                                    NPIdentifier propertyName) {
  return NPObjectProxy::NPRemoveProperty(npobj, propertyName);
}

static bool NPN_EvaluatePatch(NPP npp,
                              NPObject *npobj,
                              NPString *script,
                              NPVariant *result) {
  return NPObjectProxy::NPNEvaluate(npp, npobj, script, result);
}


static void NPN_SetExceptionPatch(NPObject *obj, const NPUTF8 *message) {
  std::string message_str(message);
  if (IsPluginProcess()) {
    NPChannelBase* renderer_channel = NPChannelBase::GetCurrentChannel();
    if (renderer_channel)
      renderer_channel->Send(new PluginHostMsg_SetException(message_str));
  } else {
    WebBindings::setException(obj, message_str.c_str());
  }
}

static bool NPN_EnumeratePatch(NPP npp, NPObject *obj,
                               NPIdentifier **identifier, uint32_t *count) {
  return NPObjectProxy::NPNEnumerate(obj, identifier, count);
}

// The overrided table of functions provided to the plugin.
NPNetscapeFuncs *GetHostFunctions() {
  static bool init = false;
  static NPNetscapeFuncs host_funcs;
  if (init)
    return &host_funcs;

  memset(&host_funcs, 0, sizeof(host_funcs));
  host_funcs.invoke = NPN_InvokePatch;
  host_funcs.invokeDefault = NPN_InvokeDefaultPatch;
  host_funcs.evaluate = NPN_EvaluatePatch;
  host_funcs.getproperty = NPN_GetPropertyPatch;
  host_funcs.setproperty = NPN_SetPropertyPatch;
  host_funcs.removeproperty = NPN_RemovePropertyPatch;
  host_funcs.hasproperty = NPN_HasPropertyPatch;
  host_funcs.hasmethod = NPN_HasMethodPatch;
  host_funcs.setexception = NPN_SetExceptionPatch;
  host_funcs.enumerate = NPN_EnumeratePatch;

  init = true;
  return &host_funcs;
}

#endif  // defined(ENABLE_PLUGINS)
}

#if defined(ENABLE_PLUGINS)
void PatchNPNFunctions() {
  g_plugin_process = true;
  NPNetscapeFuncs* funcs = GetHostFunctions();
  PluginHost::Singleton()->PatchNPNetscapeFuncs(funcs);
}
#endif

bool IsPluginProcess() {
  return g_plugin_process;
}

void CreateNPIdentifierParam(NPIdentifier id, NPIdentifier_Param* param) {
  param->identifier = id;
}

NPIdentifier CreateNPIdentifier(const NPIdentifier_Param& param) {
  return param.identifier;
}

void CreateNPVariantParam(const NPVariant& variant,
                          NPChannelBase* channel,
                          NPVariant_Param* param,
                          bool release,
                          int render_view_id,
                          const GURL& page_url) {
  switch (variant.type) {
    case NPVariantType_Void:
      param->type = NPVARIANT_PARAM_VOID;
      break;
    case NPVariantType_Null:
      param->type = NPVARIANT_PARAM_NULL;
      break;
    case NPVariantType_Bool:
      param->type = NPVARIANT_PARAM_BOOL;
      param->bool_value = variant.value.boolValue;
      break;
    case NPVariantType_Int32:
      param->type = NPVARIANT_PARAM_INT;
      param->int_value = variant.value.intValue;
      break;
    case NPVariantType_Double:
      param->type = NPVARIANT_PARAM_DOUBLE;
      param->double_value = variant.value.doubleValue;
      break;
    case NPVariantType_String:
      param->type = NPVARIANT_PARAM_STRING;
      if (variant.value.stringValue.UTF8Length) {
        param->string_value.assign(variant.value.stringValue.UTF8Characters,
                                   variant.value.stringValue.UTF8Length);
      }
      break;
    case NPVariantType_Object: {
      if (variant.value.objectValue->_class == NPObjectProxy::npclass()) {
        param->type = NPVARIANT_PARAM_RECEIVER_OBJECT_ROUTING_ID;
        NPObjectProxy* proxy =
            NPObjectProxy::GetProxy(variant.value.objectValue);
        DCHECK(proxy);
        param->npobject_routing_id = proxy->route_id();
        // Don't release, because our original variant is the same as our proxy.
        release = false;
      } else {
        // The channel could be NULL if there was a channel error. The caller's
        // Send call will fail anyways.
        if (channel) {
          // NPObjectStub adds its own reference to the NPObject it owns, so if
          // we were supposed to release the corresponding variant
          // (release==true), we should still do that.
          param->type = NPVARIANT_PARAM_SENDER_OBJECT_ROUTING_ID;
          int route_id = channel->GetExistingRouteForNPObjectStub(
              variant.value.objectValue);
          if (route_id != MSG_ROUTING_NONE) {
            param->npobject_routing_id = route_id;
          } else {
            route_id = channel->GenerateRouteID();
            new NPObjectStub(
                variant.value.objectValue, channel, route_id, render_view_id,
                page_url);
            param->npobject_routing_id = route_id;
          }

          // Include the object's owner.
          NPP owner = WebBindings::getObjectOwner(variant.value.objectValue);
          param->npobject_owner_id =
              channel->GetExistingRouteForNPObjectOwner(owner);
        } else {
          param->type = NPVARIANT_PARAM_VOID;
        }
      }
      break;
    }
    default:
      NOTREACHED();
  }

  if (release)
    WebBindings::releaseVariantValue(const_cast<NPVariant*>(&variant));
}

bool CreateNPVariant(const NPVariant_Param& param,
                     NPChannelBase* channel,
                     NPVariant* result,
                     int render_view_id,
                     const GURL& page_url) {
  switch (param.type) {
    case NPVARIANT_PARAM_VOID:
      result->type = NPVariantType_Void;
      break;
    case NPVARIANT_PARAM_NULL:
      result->type = NPVariantType_Null;
      break;
    case NPVARIANT_PARAM_BOOL:
      result->type = NPVariantType_Bool;
      result->value.boolValue = param.bool_value;
      break;
    case NPVARIANT_PARAM_INT:
      result->type = NPVariantType_Int32;
      result->value.intValue = param.int_value;
      break;
    case NPVARIANT_PARAM_DOUBLE:
      result->type = NPVariantType_Double;
      result->value.doubleValue = param.double_value;
      break;
    case NPVARIANT_PARAM_STRING: {
      result->type = NPVariantType_String;
      void* buffer = malloc(param.string_value.size());
      size_t size = param.string_value.size();
      result->value.stringValue.UTF8Characters = static_cast<NPUTF8*>(buffer);
      memcpy(buffer, param.string_value.c_str(), size);
      result->value.stringValue.UTF8Length = static_cast<int>(size);
      break;
    }
    case NPVARIANT_PARAM_SENDER_OBJECT_ROUTING_ID: {
      result->type = NPVariantType_Object;
      NPObject* object =
          channel->GetExistingNPObjectProxy(param.npobject_routing_id);
      if (object) {
        WebBindings::retainObject(object);
        result->value.objectValue = object;
      } else {
        NPP owner =
            channel->GetExistingNPObjectOwner(param.npobject_owner_id);
        // TODO(wez): Once NPObject tracking lands in Blink, check |owner| and
        // return NPVariantType_Void if it is NULL.
        result->value.objectValue =
            NPObjectProxy::Create(channel,
                                  param.npobject_routing_id,
                                  render_view_id,
                                  page_url,
                                  owner);
      }
      break;
    }
    case NPVARIANT_PARAM_RECEIVER_OBJECT_ROUTING_ID: {
      NPObjectBase* npobject_base =
          channel->GetNPObjectListenerForRoute(param.npobject_routing_id);
      if (!npobject_base) {
        DLOG(WARNING) << "Invalid routing id passed in"
                      << param.npobject_routing_id;
        return false;
      }

      DCHECK(npobject_base->GetUnderlyingNPObject() != NULL);

      result->type = NPVariantType_Object;
      result->value.objectValue = npobject_base->GetUnderlyingNPObject();
      WebBindings::retainObject(result->value.objectValue);
      break;
    }
    default:
      NOTREACHED();
  }
  return true;
}

}  // namespace content
