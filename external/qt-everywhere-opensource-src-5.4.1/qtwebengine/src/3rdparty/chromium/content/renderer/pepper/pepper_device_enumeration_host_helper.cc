// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_device_enumeration_host_helper.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "ipc/ipc_message.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/ppb_device_ref_shared.h"

using ppapi::host::HostMessageContext;

namespace content {

// Makes sure that StopEnumerateDevices() is called for each EnumerateDevices().
class PepperDeviceEnumerationHostHelper::ScopedRequest
    : public base::SupportsWeakPtr<ScopedRequest> {
 public:
  // |owner| must outlive this object.
  ScopedRequest(PepperDeviceEnumerationHostHelper* owner,
                const Delegate::EnumerateDevicesCallback& callback)
      : owner_(owner),
        callback_(callback),
        requested_(false),
        request_id_(0),
        sync_call_(false) {
    if (!owner_->document_url_.is_valid())
      return;

    requested_ = true;

    // Note that the callback passed into
    // PepperDeviceEnumerationHostHelper::Delegate::EnumerateDevices() may be
    // called synchronously. In that case, |request_id_| hasn't been updated
    // when the callback is called. Moreover, |callback| may destroy this
    // object. So we don't pass in |callback| directly. Instead, we use
    // EnumerateDevicesCallbackBody() to ensure that we always call |callback|
    // asynchronously.
    sync_call_ = true;
    request_id_ = owner_->delegate_->EnumerateDevices(
        owner_->device_type_,
        owner_->document_url_,
        base::Bind(&ScopedRequest::EnumerateDevicesCallbackBody, AsWeakPtr()));
    sync_call_ = false;
  }

  ~ScopedRequest() {
    if (requested_) {
      owner_->delegate_->StopEnumerateDevices(request_id_);
    }
  }

  bool requested() const { return requested_; }

 private:
  void EnumerateDevicesCallbackBody(
      int request_id,
      const std::vector<ppapi::DeviceRefData>& devices) {
    if (sync_call_) {
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&ScopedRequest::EnumerateDevicesCallbackBody,
                     AsWeakPtr(),
                     request_id,
                     devices));
    } else {
      DCHECK_EQ(request_id_, request_id);
      callback_.Run(request_id, devices);
      // This object may have been destroyed at this point.
    }
  }

  PepperDeviceEnumerationHostHelper* owner_;
  PepperDeviceEnumerationHostHelper::Delegate::EnumerateDevicesCallback
      callback_;
  bool requested_;
  int request_id_;
  bool sync_call_;

  DISALLOW_COPY_AND_ASSIGN(ScopedRequest);
};

PepperDeviceEnumerationHostHelper::PepperDeviceEnumerationHostHelper(
    ppapi::host::ResourceHost* resource_host,
    Delegate* delegate,
    PP_DeviceType_Dev device_type,
    const GURL& document_url)
    : resource_host_(resource_host),
      delegate_(delegate),
      device_type_(device_type),
      document_url_(document_url) {}

PepperDeviceEnumerationHostHelper::~PepperDeviceEnumerationHostHelper() {}

bool PepperDeviceEnumerationHostHelper::HandleResourceMessage(
    const IPC::Message& msg,
    HostMessageContext* context,
    int32_t* result) {
  bool return_value = false;
  *result = InternalHandleResourceMessage(msg, context, &return_value);
  return return_value;
}

int32_t PepperDeviceEnumerationHostHelper::InternalHandleResourceMessage(
    const IPC::Message& msg,
    HostMessageContext* context,
    bool* handled) {
  *handled = true;
  PPAPI_BEGIN_MESSAGE_MAP(PepperDeviceEnumerationHostHelper, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(
        PpapiHostMsg_DeviceEnumeration_EnumerateDevices, OnEnumerateDevices)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_DeviceEnumeration_MonitorDeviceChange,
        OnMonitorDeviceChange)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(
        PpapiHostMsg_DeviceEnumeration_StopMonitoringDeviceChange,
        OnStopMonitoringDeviceChange)
  PPAPI_END_MESSAGE_MAP()

  *handled = false;
  return PP_ERROR_FAILED;
}

int32_t PepperDeviceEnumerationHostHelper::OnEnumerateDevices(
    HostMessageContext* context) {
  if (enumerate_devices_context_.is_valid())
    return PP_ERROR_INPROGRESS;

  enumerate_.reset(new ScopedRequest(
      this,
      base::Bind(&PepperDeviceEnumerationHostHelper::OnEnumerateDevicesComplete,
                 base::Unretained(this))));
  if (!enumerate_->requested())
    return PP_ERROR_FAILED;

  enumerate_devices_context_ = context->MakeReplyMessageContext();
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperDeviceEnumerationHostHelper::OnMonitorDeviceChange(
    HostMessageContext* /* context */,
    uint32_t callback_id) {
  monitor_.reset(new ScopedRequest(
      this,
      base::Bind(&PepperDeviceEnumerationHostHelper::OnNotifyDeviceChange,
                 base::Unretained(this),
                 callback_id)));

  return monitor_->requested() ? PP_OK : PP_ERROR_FAILED;
}

int32_t PepperDeviceEnumerationHostHelper::OnStopMonitoringDeviceChange(
    HostMessageContext* /* context */) {
  monitor_.reset(NULL);
  return PP_OK;
}

void PepperDeviceEnumerationHostHelper::OnEnumerateDevicesComplete(
    int /* request_id */,
    const std::vector<ppapi::DeviceRefData>& devices) {
  DCHECK(enumerate_devices_context_.is_valid());

  enumerate_.reset(NULL);

  enumerate_devices_context_.params.set_result(PP_OK);
  resource_host_->host()->SendReply(
      enumerate_devices_context_,
      PpapiPluginMsg_DeviceEnumeration_EnumerateDevicesReply(devices));
  enumerate_devices_context_ = ppapi::host::ReplyMessageContext();
}

void PepperDeviceEnumerationHostHelper::OnNotifyDeviceChange(
    uint32_t callback_id,
    int /* request_id */,
    const std::vector<ppapi::DeviceRefData>& devices) {
  resource_host_->host()->SendUnsolicitedReply(
      resource_host_->pp_resource(),
      PpapiPluginMsg_DeviceEnumeration_NotifyDeviceChange(callback_id,
                                                          devices));
}

}  // namespace content
