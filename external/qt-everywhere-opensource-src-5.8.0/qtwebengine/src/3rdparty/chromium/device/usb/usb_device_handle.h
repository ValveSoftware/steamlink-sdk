// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_USB_USB_DEVICE_HANDLE_H_
#define DEVICE_USB_USB_DEVICE_HANDLE_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "base/threading/thread_checker.h"
#include "device/usb/usb_descriptors.h"

namespace net {
class IOBuffer;
}

namespace device {

class UsbDevice;

enum UsbTransferStatus {
  USB_TRANSFER_COMPLETED = 0,
  USB_TRANSFER_ERROR,
  USB_TRANSFER_TIMEOUT,
  USB_TRANSFER_CANCELLED,
  USB_TRANSFER_STALLED,
  USB_TRANSFER_DISCONNECT,
  USB_TRANSFER_OVERFLOW,
  USB_TRANSFER_LENGTH_SHORT,
};

// UsbDeviceHandle class provides basic I/O related functionalities.
class UsbDeviceHandle : public base::RefCountedThreadSafe<UsbDeviceHandle> {
 public:
  struct IsochronousPacket {
    uint32_t length;
    uint32_t transferred_length;
    UsbTransferStatus status;
  };

  using ResultCallback = base::Callback<void(bool)>;
  using TransferCallback = base::Callback<
      void(UsbTransferStatus, scoped_refptr<net::IOBuffer>, size_t)>;
  using IsochronousTransferCallback =
      base::Callback<void(scoped_refptr<net::IOBuffer>,
                          const std::vector<IsochronousPacket>& packets)>;

  enum TransferRequestType { STANDARD, CLASS, VENDOR, RESERVED };
  enum TransferRecipient { DEVICE, INTERFACE, ENDPOINT, OTHER };

  virtual scoped_refptr<UsbDevice> GetDevice() const = 0;

  // Notifies UsbDevice to drop the reference of this object; cancels all the
  // flying transfers.
  // It is possible that the object has no other reference after this call. So
  // if it is called using a raw pointer, it could be invalidated.
  // The platform device handle will be closed when UsbDeviceHandle destructs.
  virtual void Close() = 0;

  // Device manipulation operations.
  virtual void SetConfiguration(int configuration_value,
                                const ResultCallback& callback) = 0;
  virtual void ClaimInterface(int interface_number,
                              const ResultCallback& callback) = 0;
  virtual void ReleaseInterface(int interface_number,
                                const ResultCallback& callback) = 0;
  virtual void SetInterfaceAlternateSetting(int interface_number,
                                            int alternate_setting,
                                            const ResultCallback& callback) = 0;
  virtual void ResetDevice(const ResultCallback& callback) = 0;
  virtual void ClearHalt(uint8_t endpoint, const ResultCallback& callback) = 0;

  // The transfer functions may be called from any thread. The provided callback
  // will be run on the caller's thread.
  virtual void ControlTransfer(UsbEndpointDirection direction,
                               TransferRequestType request_type,
                               TransferRecipient recipient,
                               uint8_t request,
                               uint16_t value,
                               uint16_t index,
                               scoped_refptr<net::IOBuffer> buffer,
                               size_t length,
                               unsigned int timeout,
                               const TransferCallback& callback) = 0;

  virtual void IsochronousTransferIn(
      uint8_t endpoint_number,
      const std::vector<uint32_t>& packet_lengths,
      unsigned int timeout,
      const IsochronousTransferCallback& callback) = 0;

  virtual void IsochronousTransferOut(
      uint8_t endpoint_number,
      scoped_refptr<net::IOBuffer> buffer,
      const std::vector<uint32_t>& packet_lengths,
      unsigned int timeout,
      const IsochronousTransferCallback& callback) = 0;

  virtual void GenericTransfer(UsbEndpointDirection direction,
                               uint8_t endpoint_number,
                               scoped_refptr<net::IOBuffer> buffer,
                               size_t length,
                               unsigned int timeout,
                               const TransferCallback& callback) = 0;

  // Gets the interface containing |endpoint_address|. Returns nullptr if no
  // claimed interface contains that endpoint.
  virtual const UsbInterfaceDescriptor* FindInterfaceByEndpoint(
      uint8_t endpoint_address) = 0;

 protected:
  friend class base::RefCountedThreadSafe<UsbDeviceHandle>;

  UsbDeviceHandle();
  virtual ~UsbDeviceHandle();

 private:
  DISALLOW_COPY_AND_ASSIGN(UsbDeviceHandle);
};

}  // namespace device

#endif  // DEVICE_USB_USB_DEVICE_HANDLE_H_
