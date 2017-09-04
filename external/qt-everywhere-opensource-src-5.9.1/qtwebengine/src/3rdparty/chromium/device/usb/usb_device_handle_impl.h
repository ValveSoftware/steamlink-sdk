// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_USB_USB_DEVICE_HANDLE_IMPL_H_
#define DEVICE_USB_USB_DEVICE_HANDLE_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "device/usb/usb_device_handle.h"
#include "third_party/libusb/src/libusb/libusb.h"

namespace base {
class SequencedTaskRunner;
class SingleThreadTaskRunner;
class TaskRunner;
}

namespace net {
class IOBuffer;
}

namespace device {

struct EndpointMapValue {
  const UsbInterfaceDescriptor* interface;
  const UsbEndpointDescriptor* endpoint;
};

class UsbContext;
struct UsbConfigDescriptor;
class UsbDeviceImpl;

typedef libusb_device_handle* PlatformUsbDeviceHandle;
typedef libusb_iso_packet_descriptor* PlatformUsbIsoPacketDescriptor;
typedef libusb_transfer* PlatformUsbTransferHandle;

// UsbDeviceHandle class provides basic I/O related functionalities.
class UsbDeviceHandleImpl : public UsbDeviceHandle {
 public:
  scoped_refptr<UsbDevice> GetDevice() const override;
  void Close() override;
  void SetConfiguration(int configuration_value,
                        const ResultCallback& callback) override;
  void ClaimInterface(int interface_number,
                      const ResultCallback& callback) override;
  void ReleaseInterface(int interface_number,
                        const ResultCallback& callback) override;
  void SetInterfaceAlternateSetting(int interface_number,
                                    int alternate_setting,
                                    const ResultCallback& callback) override;
  void ResetDevice(const ResultCallback& callback) override;
  void ClearHalt(uint8_t endpoint, const ResultCallback& callback) override;

  void ControlTransfer(UsbEndpointDirection direction,
                       TransferRequestType request_type,
                       TransferRecipient recipient,
                       uint8_t request,
                       uint16_t value,
                       uint16_t index,
                       scoped_refptr<net::IOBuffer> buffer,
                       size_t length,
                       unsigned int timeout,
                       const TransferCallback& callback) override;

  void IsochronousTransferIn(
      uint8_t endpoint,
      const std::vector<uint32_t>& packet_lengths,
      unsigned int timeout,
      const IsochronousTransferCallback& callback) override;

  void IsochronousTransferOut(
      uint8_t endpoint,
      scoped_refptr<net::IOBuffer> buffer,
      const std::vector<uint32_t>& packet_lengths,
      unsigned int timeout,
      const IsochronousTransferCallback& callback) override;

  void GenericTransfer(UsbEndpointDirection direction,
                       uint8_t endpoint_number,
                       scoped_refptr<net::IOBuffer> buffer,
                       size_t length,
                       unsigned int timeout,
                       const TransferCallback& callback) override;
  const UsbInterfaceDescriptor* FindInterfaceByEndpoint(
      uint8_t endpoint_address) override;

 protected:
  friend class UsbDeviceImpl;

  // This constructor is called by UsbDeviceImpl.
  UsbDeviceHandleImpl(
      scoped_refptr<UsbContext> context,
      scoped_refptr<UsbDeviceImpl> device,
      PlatformUsbDeviceHandle handle,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner);

  ~UsbDeviceHandleImpl() override;

  PlatformUsbDeviceHandle handle() const { return handle_; }

 private:
  class InterfaceClaimer;
  class Transfer;

  void SetConfigurationOnBlockingThread(int configuration_value,
                                        const ResultCallback& callback);
  void SetConfigurationComplete(bool success, const ResultCallback& callback);
  void ClaimInterfaceOnBlockingThread(int interface_number,
                                      const ResultCallback& callback);
  void ClaimInterfaceComplete(scoped_refptr<InterfaceClaimer> interface_claimer,
                              const ResultCallback& callback);
  void SetInterfaceAlternateSettingOnBlockingThread(
      int interface_number,
      int alternate_setting,
      const ResultCallback& callback);
  void SetInterfaceAlternateSettingComplete(int interface_number,
                                            int alternate_setting,
                                            bool success,
                                            const ResultCallback& callback);
  void ResetDeviceOnBlockingThread(const ResultCallback& callback);
  void ClearHaltOnBlockingThread(uint8_t endpoint,
                                 const ResultCallback& callback);

  // Refresh endpoint_map_ after ClaimInterface, ReleaseInterface and
  // SetInterfaceAlternateSetting.
  void RefreshEndpointMap();

  // Look up the claimed interface by endpoint. Return NULL if the interface
  // of the endpoint is not found.
  scoped_refptr<InterfaceClaimer> GetClaimedInterfaceForEndpoint(
      uint8_t endpoint);

  void ControlTransferInternal(
      UsbEndpointDirection direction,
      TransferRequestType request_type,
      TransferRecipient recipient,
      uint8_t request,
      uint16_t value,
      uint16_t index,
      scoped_refptr<net::IOBuffer> buffer,
      size_t length,
      unsigned int timeout,
      scoped_refptr<base::TaskRunner> callback_task_runner,
      const TransferCallback& callback);

  void IsochronousTransferInInternal(
      uint8_t endpoint_address,
      const std::vector<uint32_t>& packet_lengths,
      unsigned int timeout,
      scoped_refptr<base::TaskRunner> callback_task_runner,
      const IsochronousTransferCallback& callback);

  void IsochronousTransferOutInternal(
      uint8_t endpoint_address,
      scoped_refptr<net::IOBuffer> buffer,
      const std::vector<uint32_t>& packet_lengths,
      unsigned int timeout,
      scoped_refptr<base::TaskRunner> callback_task_runner,
      const IsochronousTransferCallback& callback);

  void GenericTransferInternal(
      uint8_t endpoint_address,
      scoped_refptr<net::IOBuffer> buffer,
      size_t length,
      unsigned int timeout,
      scoped_refptr<base::TaskRunner> callback_task_runner,
      const TransferCallback& callback);

  // Submits a transfer and starts tracking it. Retains the buffer and copies
  // the completion callback until the transfer finishes, whereupon it invokes
  // the callback then releases the buffer.
  void SubmitTransfer(std::unique_ptr<Transfer> transfer);

  // Removes the transfer from the in-flight transfer set and invokes the
  // completion callback.
  void TransferComplete(Transfer* transfer, const base::Closure& callback);

  scoped_refptr<UsbDeviceImpl> device_;

  PlatformUsbDeviceHandle handle_;

  typedef std::map<int, scoped_refptr<InterfaceClaimer>> ClaimedInterfaceMap;
  ClaimedInterfaceMap claimed_interfaces_;

  // This set holds weak pointers to pending transfers.
  std::set<Transfer*> transfers_;

  // A map from endpoints to EndpointMapValue
  typedef std::map<int, EndpointMapValue> EndpointMap;
  EndpointMap endpoint_map_;

  // Retain the UsbContext so that the platform context will not be destroyed
  // before this handle.
  scoped_refptr<UsbContext> context_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(UsbDeviceHandleImpl);
};

}  // namespace device

#endif  // DEVICE_USB_USB_DEVICE_HANDLE_IMPL_H_
