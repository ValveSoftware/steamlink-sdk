// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_DEVICE_ENUMERATION_HOST_HELPER_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_DEVICE_ENUMERATION_HOST_HELPER_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "ppapi/c/dev/ppb_device_ref_dev.h"
#include "ppapi/host/host_message_context.h"
#include "url/gurl.h"

namespace ppapi {
struct DeviceRefData;

namespace host {
class ResourceHost;
}

}  // namespace ppapi

namespace IPC {
class Message;
}

namespace content {

// Resource hosts that support device enumeration can use this class to filter
// and process PpapiHostMsg_DeviceEnumeration_* messages.
// TODO(yzshen): Refactor ppapi::host::ResourceMessageFilter to support message
// handling on the same thread, and then derive this class from the filter
// class.
class CONTENT_EXPORT PepperDeviceEnumerationHostHelper {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    typedef base::Callback<
        void(int /* request_id */,
             const std::vector<ppapi::DeviceRefData>& /* devices */)>
        EnumerateDevicesCallback;

    // Enumerates devices of the specified type. The request ID passed into the
    // callback will be the same as the return value.
    virtual int EnumerateDevices(PP_DeviceType_Dev type,
                                 const GURL& document_url,
                                 const EnumerateDevicesCallback& callback) = 0;
    // Stop enumerating devices of the specified |request_id|. The |request_id|
    // is the return value of EnumerateDevicesCallback.
    virtual void StopEnumerateDevices(int request_id) = 0;
  };

  // |resource_host| and |delegate| must outlive this object.
  PepperDeviceEnumerationHostHelper(ppapi::host::ResourceHost* resource_host,
                                    base::WeakPtr<Delegate> delegate,
                                    PP_DeviceType_Dev device_type,
                                    const GURL& document_url);
  ~PepperDeviceEnumerationHostHelper();

  // Returns true if the message has been handled.
  bool HandleResourceMessage(const IPC::Message& msg,
                             ppapi::host::HostMessageContext* context,
                             int32_t* result);

 private:
  class ScopedRequest;

  // Has a different signature than HandleResourceMessage() in order to utilize
  // message dispatching macros.
  int32_t InternalHandleResourceMessage(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context,
      bool* handled);

  int32_t OnEnumerateDevices(ppapi::host::HostMessageContext* context);
  int32_t OnMonitorDeviceChange(ppapi::host::HostMessageContext* context,
                                uint32_t callback_id);
  int32_t OnStopMonitoringDeviceChange(
      ppapi::host::HostMessageContext* context);

  void OnEnumerateDevicesComplete(
      int request_id,
      const std::vector<ppapi::DeviceRefData>& devices);
  void OnNotifyDeviceChange(uint32_t callback_id,
                            int request_id,
                            const std::vector<ppapi::DeviceRefData>& devices);

  // Non-owning pointers.
  ppapi::host::ResourceHost* resource_host_;
  base::WeakPtr<Delegate> delegate_;

  PP_DeviceType_Dev device_type_;
  GURL document_url_;

  std::unique_ptr<ScopedRequest> enumerate_;
  std::unique_ptr<ScopedRequest> monitor_;

  ppapi::host::ReplyMessageContext enumerate_devices_context_;

  DISALLOW_COPY_AND_ASSIGN(PepperDeviceEnumerationHostHelper);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_DEVICE_ENUMERATION_HOST_HELPER_H_
