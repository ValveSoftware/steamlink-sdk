// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/npapi/npobject_proxy.h"

#include "content/child/npapi/np_channel_base.h"
#include "content/child/npapi/npobject_util.h"
#include "content/child/plugin_messages.h"
#include "third_party/WebKit/public/web/WebBindings.h"

#if defined(ENABLE_PLUGINS)
#include "content/child/npapi/plugin_instance.h"
#endif

using blink::WebBindings;

namespace content {

struct NPObjectWrapper {
    NPObject object;
    NPObjectProxy* proxy;
};

NPClass NPObjectProxy::npclass_proxy_ = {
  NP_CLASS_STRUCT_VERSION,
  NPObjectProxy::NPAllocate,
  NPObjectProxy::NPDeallocate,
  NPObjectProxy::NPPInvalidate,
  NPObjectProxy::NPHasMethod,
  NPObjectProxy::NPInvoke,
  NPObjectProxy::NPInvokeDefault,
  NPObjectProxy::NPHasProperty,
  NPObjectProxy::NPGetProperty,
  NPObjectProxy::NPSetProperty,
  NPObjectProxy::NPRemoveProperty,
  NPObjectProxy::NPNEnumerate,
  NPObjectProxy::NPNConstruct
};

NPObjectProxy* NPObjectProxy::GetProxy(NPObject* object) {
  NPObjectProxy* proxy = NULL;

  // Wrapper exists only for NPObjects that we had created.
  if (&npclass_proxy_ == object->_class) {
    NPObjectWrapper* wrapper = reinterpret_cast<NPObjectWrapper*>(object);
    proxy = wrapper->proxy;
  }

  return proxy;
}

NPObject* NPObjectProxy::GetUnderlyingNPObject() {
  return NULL;
}

IPC::Listener* NPObjectProxy::GetChannelListener() {
  return static_cast<IPC::Listener*>(this);
}

NPObjectProxy::NPObjectProxy(
    NPChannelBase* channel,
    int route_id,
    int render_view_id,
    const GURL& page_url)
    : channel_(channel),
      route_id_(route_id),
      render_view_id_(render_view_id),
      page_url_(page_url) {
  channel_->AddRoute(route_id, this, this);
}

NPObjectProxy::~NPObjectProxy() {
  if (channel_.get()) {
    // This NPObjectProxy instance is now invalid and should not be reused for
    // requests initiated by plugins. We may receive requests for the
    // same NPObject in the context of the outgoing NPObjectMsg_Release call.
    // We should be creating new NPObjectProxy instances to wrap these
    // NPObjects.
    channel_->RemoveMappingForNPObjectProxy(route_id_);
    channel_->RemoveRoute(route_id_);
    Send(new NPObjectMsg_Release(route_id_));
  }
}

NPObject* NPObjectProxy::Create(NPChannelBase* channel,
                                int route_id,
                                int render_view_id,
                                const GURL& page_url,
                                NPP owner) {
  NPObjectWrapper* obj = reinterpret_cast<NPObjectWrapper*>(
      WebBindings::createObject(owner, &npclass_proxy_));
  obj->proxy = new NPObjectProxy(channel, route_id, render_view_id, page_url);
  channel->AddMappingForNPObjectProxy(route_id, &obj->object);
  return reinterpret_cast<NPObject*>(obj);
}

bool NPObjectProxy::Send(IPC::Message* msg) {
  if (channel_.get())
    return channel_->Send(msg);

  delete msg;
  return false;
}

NPObject* NPObjectProxy::NPAllocate(NPP, NPClass*) {
  return reinterpret_cast<NPObject*>(new NPObjectWrapper);
}

void NPObjectProxy::NPDeallocate(NPObject* npObj) {
  NPObjectWrapper* obj = reinterpret_cast<NPObjectWrapper*>(npObj);
  delete obj->proxy;
  delete obj;
}

bool NPObjectProxy::OnMessageReceived(const IPC::Message& msg) {
  NOTREACHED();
  return false;
}

void NPObjectProxy::OnChannelError() {
  // Release our ref count of the plugin channel object, as it addrefs the
  // process.
  channel_ = NULL;
}

bool NPObjectProxy::NPHasMethod(NPObject *obj,
                                NPIdentifier name) {
  if (obj == NULL)
    return false;

  bool result = false;
  NPObjectProxy* proxy = GetProxy(obj);

  if (!proxy) {
    return obj->_class->hasMethod(obj, name);
  }

  NPIdentifier_Param name_param;
  CreateNPIdentifierParam(name, &name_param);

  proxy->Send(new NPObjectMsg_HasMethod(proxy->route_id(), name_param,
                                        &result));
  return result;
}

bool NPObjectProxy::NPInvoke(NPObject *obj,
                             NPIdentifier name,
                             const NPVariant *args,
                             uint32_t arg_count,
                             NPVariant *result) {
  return NPInvokePrivate(0, obj, false, name, args, arg_count, result);
}

bool NPObjectProxy::NPInvokeDefault(NPObject *npobj,
                                    const NPVariant *args,
                                    uint32_t arg_count,
                                    NPVariant *result) {
  return NPInvokePrivate(0, npobj, true, 0, args, arg_count, result);
}

bool NPObjectProxy::NPInvokePrivate(NPP npp,
                                    NPObject *obj,
                                    bool is_default,
                                    NPIdentifier name,
                                    const NPVariant *args,
                                    uint32_t arg_count,
                                    NPVariant *np_result) {
  if (obj == NULL)
    return false;

  NPObjectProxy* proxy = GetProxy(obj);
  if (!proxy) {
    if (is_default) {
      return obj->_class->invokeDefault(obj, args, arg_count, np_result);
    } else {
      return obj->_class->invoke(obj, name, args, arg_count, np_result);
    }
  }

  bool result = false;
  int render_view_id = proxy->render_view_id_;
  NPIdentifier_Param name_param;
  if (is_default) {
    // The data won't actually get used, but set it so we don't send random
    // data.
    name_param.identifier = NULL;
  } else {
    CreateNPIdentifierParam(name, &name_param);
  }

  // Note: This instance can get destroyed in the context of
  // Send so addref the channel in this scope.
  scoped_refptr<NPChannelBase> channel_copy = proxy->channel_;
  std::vector<NPVariant_Param> args_param;
  for (unsigned int i = 0; i < arg_count; ++i) {
    NPVariant_Param param;
    CreateNPVariantParam(args[i],
                         channel_copy.get(),
                         &param,
                         false,
                         render_view_id,
                         proxy->page_url_);
    args_param.push_back(param);
  }

  NPVariant_Param param_result;
  NPObjectMsg_Invoke* msg = new NPObjectMsg_Invoke(
      proxy->route_id_, is_default, name_param, args_param, &param_result,
      &result);

  // If we're in the plugin process and this invoke leads to a dialog box, the
  // plugin will hang the window hierarchy unless we pump the window message
  // queue while waiting for a reply.  We need to do this to simulate what
  // happens when everything runs in-process (while calling MessageBox window
  // messages are pumped).
  if (IsPluginProcess() && proxy->channel()) {
    msg->set_pump_messages_event(
        proxy->channel()->GetModalDialogEvent(render_view_id));
  }

  GURL page_url = proxy->page_url_;
  proxy->Send(msg);

  // Send may delete proxy.
  proxy = NULL;

  if (!result)
    return false;

  CreateNPVariant(
      param_result, channel_copy.get(), np_result, render_view_id, page_url);
  return true;
}

bool NPObjectProxy::NPHasProperty(NPObject *obj,
                                  NPIdentifier name) {
  if (obj == NULL)
    return false;

  bool result = false;
  NPObjectProxy* proxy = GetProxy(obj);
  if (!proxy) {
    return obj->_class->hasProperty(obj, name);
  }

  NPIdentifier_Param name_param;
  CreateNPIdentifierParam(name, &name_param);

  NPVariant_Param param;
  proxy->Send(new NPObjectMsg_HasProperty(
      proxy->route_id(), name_param, &result));

  // Send may delete proxy.
  proxy = NULL;

  return result;
}

bool NPObjectProxy::NPGetProperty(NPObject *obj,
                                  NPIdentifier name,
                                  NPVariant *np_result) {
  // Please refer to http://code.google.com/p/chromium/issues/detail?id=2556,
  // which was a crash in the XStandard plugin during plugin shutdown. The
  // crash occured because the plugin requests the plugin script object,
  // which fails. The plugin does not check the result of the operation and
  // invokes NPN_GetProperty on a NULL object which lead to the crash. If
  // we observe similar crashes in other methods in the future, these null
  // checks may have to be replicated in the other methods in this class.
  if (obj == NULL)
    return false;

  NPObjectProxy* proxy = GetProxy(obj);
  if (!proxy) {
    return obj->_class->getProperty(obj, name, np_result);
  }

  bool result = false;
  int render_view_id = proxy->render_view_id_;
  NPIdentifier_Param name_param;
  CreateNPIdentifierParam(name, &name_param);

  NPVariant_Param param;
  scoped_refptr<NPChannelBase> channel(proxy->channel_);

  GURL page_url = proxy->page_url_;
  proxy->Send(new NPObjectMsg_GetProperty(
      proxy->route_id(), name_param, &param, &result));
  // Send may delete proxy.
  proxy = NULL;
  if (!result)
    return false;

  CreateNPVariant(
      param, channel.get(), np_result, render_view_id, page_url);

  return true;
}

bool NPObjectProxy::NPSetProperty(NPObject *obj,
                                  NPIdentifier name,
                                  const NPVariant *value) {
  if (obj == NULL)
    return false;

  NPObjectProxy* proxy = GetProxy(obj);
  if (!proxy) {
    return obj->_class->setProperty(obj, name, value);
  }

  bool result = false;
  int render_view_id = proxy->render_view_id_;
  NPIdentifier_Param name_param;
  CreateNPIdentifierParam(name, &name_param);

  NPVariant_Param value_param;
  CreateNPVariantParam(
      *value, proxy->channel(), &value_param, false, render_view_id,
      proxy->page_url_);

  proxy->Send(new NPObjectMsg_SetProperty(
      proxy->route_id(), name_param, value_param, &result));
  // Send may delete proxy.
  proxy = NULL;

  return result;
}

bool NPObjectProxy::NPRemoveProperty(NPObject *obj,
                                     NPIdentifier name) {
  if (obj == NULL)
    return false;

  bool result = false;
  NPObjectProxy* proxy = GetProxy(obj);
  if (!proxy) {
    return obj->_class->removeProperty(obj, name);
  }

  NPIdentifier_Param name_param;
  CreateNPIdentifierParam(name, &name_param);

  NPVariant_Param param;
  proxy->Send(new NPObjectMsg_RemoveProperty(
      proxy->route_id(), name_param, &result));
  // Send may delete proxy.
  proxy = NULL;

  return result;
}

void NPObjectProxy::NPPInvalidate(NPObject *obj) {
  if (obj == NULL)
    return;

  NPObjectProxy* proxy = GetProxy(obj);
  if (!proxy) {
    obj->_class->invalidate(obj);
    return;
  }

  proxy->Send(new NPObjectMsg_Invalidate(proxy->route_id()));
  // Send may delete proxy.
  proxy = NULL;
}

bool NPObjectProxy::NPNEnumerate(NPObject *obj,
                                 NPIdentifier **value,
                                 uint32_t *count) {
  if (obj == NULL)
    return false;

  bool result = false;
  NPObjectProxy* proxy = GetProxy(obj);
  if (!proxy) {
    if (obj->_class->structVersion >= NP_CLASS_STRUCT_VERSION_ENUM) {
      return obj->_class->enumerate(obj, value, count);
    } else {
      return false;
    }
  }

  std::vector<NPIdentifier_Param> value_param;
  proxy->Send(new NPObjectMsg_Enumeration(
      proxy->route_id(), &value_param, &result));
  // Send may delete proxy.
  proxy = NULL;

  if (!result)
    return false;

  *count = static_cast<unsigned int>(value_param.size());
  *value = static_cast<NPIdentifier *>(malloc(sizeof(NPIdentifier) * *count));
  for (unsigned int i = 0; i < *count; ++i)
    (*value)[i] = CreateNPIdentifier(value_param[i]);

  return true;
}

bool NPObjectProxy::NPNConstruct(NPObject *obj,
                                 const NPVariant *args,
                                 uint32_t arg_count,
                                 NPVariant *np_result) {
  if (obj == NULL)
    return false;

  NPObjectProxy* proxy = GetProxy(obj);
  if (!proxy) {
    if (obj->_class->structVersion >= NP_CLASS_STRUCT_VERSION_CTOR) {
      return obj->_class->construct(obj, args, arg_count, np_result);
    } else {
      return false;
    }
  }

  bool result = false;
  int render_view_id = proxy->render_view_id_;

  // Note: This instance can get destroyed in the context of
  // Send so addref the channel in this scope.
  scoped_refptr<NPChannelBase> channel_copy = proxy->channel_;
  std::vector<NPVariant_Param> args_param;
  for (unsigned int i = 0; i < arg_count; ++i) {
    NPVariant_Param param;
    CreateNPVariantParam(args[i],
                         channel_copy.get(),
                         &param,
                         false,
                         render_view_id,
                         proxy->page_url_);
    args_param.push_back(param);
  }

  NPVariant_Param param_result;
  NPObjectMsg_Construct* msg = new NPObjectMsg_Construct(
      proxy->route_id_, args_param, &param_result, &result);

  // See comment in NPObjectProxy::NPInvokePrivate.
  if (IsPluginProcess() && proxy->channel()) {
    msg->set_pump_messages_event(
        proxy->channel()->GetModalDialogEvent(proxy->render_view_id_));
  }

  GURL page_url = proxy->page_url_;
  proxy->Send(msg);

  // Send may delete proxy.
  proxy = NULL;

  if (!result)
    return false;

  CreateNPVariant(
      param_result, channel_copy.get(), np_result, render_view_id, page_url);
  return true;
}

bool NPObjectProxy::NPNEvaluate(NPP npp,
                                NPObject *obj,
                                NPString *script,
                                NPVariant *result_var) {
  NPObjectProxy* proxy = GetProxy(obj);
  if (!proxy) {
    return false;
  }

  bool result = false;
  int render_view_id = proxy->render_view_id_;
  bool popups_allowed = false;

#if defined(ENABLE_PLUGINS)
  if (npp) {
    PluginInstance* plugin_instance =
        reinterpret_cast<PluginInstance*>(npp->ndata);
    if (plugin_instance)
      popups_allowed = plugin_instance->popups_allowed();
  }
#endif

  NPVariant_Param result_param;
  std::string script_str = std::string(
      script->UTF8Characters, script->UTF8Length);

  NPObjectMsg_Evaluate* msg = new NPObjectMsg_Evaluate(proxy->route_id(),
                                                       script_str,
                                                       popups_allowed,
                                                       &result_param,
                                                       &result);

  // See comment in NPObjectProxy::NPInvokePrivate.
  if (IsPluginProcess() && proxy->channel()) {
    msg->set_pump_messages_event(
        proxy->channel()->GetModalDialogEvent(render_view_id));
  }
  scoped_refptr<NPChannelBase> channel(proxy->channel_);

  GURL page_url = proxy->page_url_;
  proxy->Send(msg);
  // Send may delete proxy.
  proxy = NULL;
  if (!result)
    return false;

  CreateNPVariant(
      result_param, channel.get(), result_var, render_view_id, page_url);
  return true;
}

}  // namespace content
