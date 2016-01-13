// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppp_printing_proxy.h"

#include <string.h>

#include "ppapi/c/dev/ppp_printing_dev.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/resource_tracker.h"

namespace ppapi {
namespace proxy {

namespace {

#if !defined(OS_NACL)
bool HasPrintingPermission(PP_Instance instance) {
  Dispatcher* dispatcher = HostDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return false;
  return dispatcher->permissions().HasPermission(PERMISSION_DEV);
}

uint32_t QuerySupportedFormats(PP_Instance instance) {
  if (!HasPrintingPermission(instance))
    return 0;
  uint32_t result = 0;
  HostDispatcher::GetForInstance(instance)->Send(
      new PpapiMsg_PPPPrinting_QuerySupportedFormats(API_ID_PPP_PRINTING,
                                                     instance, &result));
  return result;
}

int32_t Begin(PP_Instance instance,
              const struct PP_PrintSettings_Dev* print_settings) {
  if (!HasPrintingPermission(instance))
    return 0;
  // Settings is just serialized as a string.
  std::string settings_string;
  settings_string.resize(sizeof(*print_settings));
  memcpy(&settings_string[0], print_settings, sizeof(*print_settings));

  int32_t result = 0;
  HostDispatcher::GetForInstance(instance)->Send(
      new PpapiMsg_PPPPrinting_Begin(API_ID_PPP_PRINTING, instance,
                                     settings_string, &result));
  return result;
}

PP_Resource PrintPages(PP_Instance instance,
                       const PP_PrintPageNumberRange_Dev* page_ranges,
                       uint32_t page_range_count) {
  if (!HasPrintingPermission(instance))
    return 0;
  std::vector<PP_PrintPageNumberRange_Dev> pages(
      page_ranges, page_ranges + page_range_count);

  HostResource result;
  HostDispatcher::GetForInstance(instance)->Send(
      new PpapiMsg_PPPPrinting_PrintPages(API_ID_PPP_PRINTING,
                                          instance, pages, &result));

  // How refcounting works when returning a resource:
  //
  // The plugin in the plugin process makes a resource that it returns to the
  // browser. The plugin proxy code returns that ref to us and asynchronously
  // releases it. Before any release message associated with that operation
  // comes, we'll get this reply. We need to add one ref since our caller
  // expects us to add one ref for it to consume.
  PpapiGlobals::Get()->GetResourceTracker()->AddRefResource(
      result.host_resource());
  return result.host_resource();
}

void End(PP_Instance instance) {
  if (!HasPrintingPermission(instance))
    return;
  HostDispatcher::GetForInstance(instance)->Send(
      new PpapiMsg_PPPPrinting_End(API_ID_PPP_PRINTING, instance));
}

PP_Bool IsScalingDisabled(PP_Instance instance) {
  if (!HasPrintingPermission(instance))
    return PP_FALSE;
  bool result = false;
  HostDispatcher::GetForInstance(instance)->Send(
      new PpapiMsg_PPPPrinting_IsScalingDisabled(API_ID_PPP_PRINTING,
                                                 instance, &result));
  return PP_FromBool(result);
}

const PPP_Printing_Dev ppp_printing_interface = {
  &QuerySupportedFormats,
  &Begin,
  &PrintPages,
  &End,
  &IsScalingDisabled
};
#else
// The NaCl plugin doesn't need the host side interface - stub it out.
static const PPP_Printing_Dev ppp_printing_interface = {};
#endif  // !defined(OS_NACL)

}  // namespace

PPP_Printing_Proxy::PPP_Printing_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher),
      ppp_printing_impl_(NULL) {
  if (dispatcher->IsPlugin()) {
    ppp_printing_impl_ = static_cast<const PPP_Printing_Dev*>(
        dispatcher->local_get_interface()(PPP_PRINTING_DEV_INTERFACE));
  }
}

PPP_Printing_Proxy::~PPP_Printing_Proxy() {
}

// static
const PPP_Printing_Dev* PPP_Printing_Proxy::GetProxyInterface() {
  return &ppp_printing_interface;
}

bool PPP_Printing_Proxy::OnMessageReceived(const IPC::Message& msg) {
  if (!dispatcher()->IsPlugin())
    return false;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPP_Printing_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPPrinting_QuerySupportedFormats,
                        OnPluginMsgQuerySupportedFormats)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPPrinting_Begin,
                        OnPluginMsgBegin)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPPrinting_PrintPages,
                        OnPluginMsgPrintPages)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPPrinting_End,
                        OnPluginMsgEnd)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPPrinting_IsScalingDisabled,
                        OnPluginMsgIsScalingDisabled)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PPP_Printing_Proxy::OnPluginMsgQuerySupportedFormats(PP_Instance instance,
                                                          uint32_t* result) {
  if (ppp_printing_impl_) {
    *result = CallWhileUnlocked(ppp_printing_impl_->QuerySupportedFormats,
                                instance);
  } else {
    *result = 0;
  }
}

void PPP_Printing_Proxy::OnPluginMsgBegin(PP_Instance instance,
                                          const std::string& settings_string,
                                          int32_t* result) {
  *result = 0;

  PP_PrintSettings_Dev settings;
  if (settings_string.size() != sizeof(settings))
    return;
  memcpy(&settings, &settings_string[0], sizeof(settings));

  if (ppp_printing_impl_) {
    *result = CallWhileUnlocked(ppp_printing_impl_->Begin,
        instance,
        const_cast<const PP_PrintSettings_Dev*>(&settings));
  }
}

void PPP_Printing_Proxy::OnPluginMsgPrintPages(
    PP_Instance instance,
    const std::vector<PP_PrintPageNumberRange_Dev>& pages,
    HostResource* result) {
  if (!ppp_printing_impl_ || pages.empty())
    return;

  PP_Resource plugin_resource = CallWhileUnlocked(
      ppp_printing_impl_->PrintPages,
      instance, &pages[0], static_cast<uint32_t>(pages.size()));
  ResourceTracker* resource_tracker = PpapiGlobals::Get()->GetResourceTracker();
  Resource* resource_object = resource_tracker->GetResource(plugin_resource);
  if (!resource_object)
    return;

  *result = resource_object->host_resource();

  // See PrintPages above for how refcounting works.
  resource_tracker->ReleaseResourceSoon(plugin_resource);
}

void PPP_Printing_Proxy::OnPluginMsgEnd(PP_Instance instance) {
  if (ppp_printing_impl_)
    CallWhileUnlocked(ppp_printing_impl_->End, instance);
}

void PPP_Printing_Proxy::OnPluginMsgIsScalingDisabled(PP_Instance instance,
                                                      bool* result) {
  if (ppp_printing_impl_) {
    *result = PP_ToBool(CallWhileUnlocked(ppp_printing_impl_->IsScalingDisabled,
                                          instance));
  } else {
    *result = false;
  }
}

}  // namespace proxy
}  // namespace ppapi
