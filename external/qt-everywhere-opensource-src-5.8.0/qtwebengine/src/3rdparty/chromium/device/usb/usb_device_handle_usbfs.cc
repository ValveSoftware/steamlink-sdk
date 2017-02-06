// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/usb_device_handle_usbfs.h"

#if defined(OS_ANDROID) && __ANDROID_API__ < 21
#include <linux/usb_ch9.h>
#else
#include <linux/usb/ch9.h>
#endif

#include <linux/usbdevice_fs.h>
#include <sys/ioctl.h>

#include <numeric>

#include "base/bind.h"
#include "base/cancelable_callback.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_pump_libevent.h"
#include "base/posix/eintr_wrapper.h"
#include "base/stl_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/device_event_log/device_event_log.h"
#include "device/usb/usb_device_linux.h"
#include "net/base/io_buffer.h"

namespace device {

namespace {

uint8_t ConvertEndpointDirection(UsbEndpointDirection direction) {
  switch (direction) {
    case USB_DIRECTION_INBOUND:
      return USB_DIR_IN;
    case USB_DIRECTION_OUTBOUND:
      return USB_DIR_OUT;
  }
  NOTREACHED();
  return 0;
}

uint8_t ConvertRequestType(UsbDeviceHandle::TransferRequestType request_type) {
  switch (request_type) {
    case UsbDeviceHandle::STANDARD:
      return USB_TYPE_STANDARD;
    case UsbDeviceHandle::CLASS:
      return USB_TYPE_CLASS;
    case UsbDeviceHandle::VENDOR:
      return USB_TYPE_VENDOR;
    case UsbDeviceHandle::RESERVED:
      return USB_TYPE_RESERVED;
  }
  NOTREACHED();
  return 0;
}

uint8_t ConvertRecipient(UsbDeviceHandle::TransferRecipient recipient) {
  switch (recipient) {
    case UsbDeviceHandle::DEVICE:
      return USB_RECIP_DEVICE;
    case UsbDeviceHandle::INTERFACE:
      return USB_RECIP_INTERFACE;
    case UsbDeviceHandle::ENDPOINT:
      return USB_RECIP_ENDPOINT;
    case UsbDeviceHandle::OTHER:
      return USB_RECIP_OTHER;
  }
  NOTREACHED();
  return 0;
}

scoped_refptr<net::IOBuffer> BuildControlTransferBuffer(
    UsbEndpointDirection direction,
    UsbDeviceHandle::TransferRequestType request_type,
    UsbDeviceHandle::TransferRecipient recipient,
    uint8_t request,
    uint16_t value,
    uint16_t index,
    scoped_refptr<net::IOBuffer> original_buffer,
    size_t length) {
  scoped_refptr<net::IOBuffer> new_buffer(
      new net::IOBuffer(length + sizeof(usb_ctrlrequest)));
  usb_ctrlrequest* setup =
      reinterpret_cast<usb_ctrlrequest*>(new_buffer->data());
  setup->bRequestType = ConvertEndpointDirection(direction) |
                        ConvertRequestType(request_type) |
                        ConvertRecipient(recipient);
  setup->bRequest = request;
  setup->wValue = value;
  setup->wIndex = index;
  setup->wLength = length;
  memcpy(new_buffer->data() + sizeof(usb_ctrlrequest), original_buffer->data(),
         length);
  return new_buffer;
}

uint8_t ConvertTransferType(UsbTransferType type) {
  switch (type) {
    case USB_TRANSFER_CONTROL:
      return USBDEVFS_URB_TYPE_CONTROL;
    case USB_TRANSFER_ISOCHRONOUS:
      return USBDEVFS_URB_TYPE_ISO;
    case USB_TRANSFER_BULK:
      return USBDEVFS_URB_TYPE_BULK;
    case USB_TRANSFER_INTERRUPT:
      return USBDEVFS_URB_TYPE_INTERRUPT;
  }
  NOTREACHED();
  return 0;
}

UsbTransferStatus ConvertTransferResult(int rc) {
  switch (rc) {
    case 0:
      return USB_TRANSFER_COMPLETED;
    case EPIPE:
      return USB_TRANSFER_STALLED;
    case ENODEV:
    case ESHUTDOWN:
    case EPROTO:
      return USB_TRANSFER_DISCONNECT;
    default:
      // TODO(reillyg): Add a specific error message whenever one of the cases
      // above fails to match.
      USB_LOG(ERROR) << "Unknown system error: "
                     << logging::SystemErrorCodeToString(rc);
      return USB_TRANSFER_ERROR;
  }
}

}  // namespace

class UsbDeviceHandleUsbfs::FileThreadHelper
    : public base::MessagePumpLibevent::Watcher,
      public base::MessageLoop::DestructionObserver {
 public:
  FileThreadHelper(int fd,
                   scoped_refptr<UsbDeviceHandleUsbfs> device_handle,
                   scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~FileThreadHelper() override;

  static void Start(std::unique_ptr<FileThreadHelper> self);

  // base::MessagePumpLibevent::Watcher overrides.
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override;

  // base::MessageLoop::DestructionObserver overrides.
  void WillDestroyCurrentMessageLoop() override;

 private:
  int fd_;
  scoped_refptr<UsbDeviceHandleUsbfs> device_handle_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::MessagePumpLibevent::FileDescriptorWatcher file_watcher_;
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(FileThreadHelper);
};

UsbDeviceHandleUsbfs::FileThreadHelper::FileThreadHelper(
    int fd,
    scoped_refptr<UsbDeviceHandleUsbfs> device_handle,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : fd_(fd), device_handle_(device_handle), task_runner_(task_runner) {}

UsbDeviceHandleUsbfs::FileThreadHelper::~FileThreadHelper() {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::MessageLoop::current()->RemoveDestructionObserver(this);
}

// static
void UsbDeviceHandleUsbfs::FileThreadHelper::Start(
    std::unique_ptr<FileThreadHelper> self) {
  base::ThreadRestrictions::AssertIOAllowed();
  self->thread_checker_.DetachFromThread();

  // Linux indicates that URBs are available to reap by marking the file
  // descriptor writable.
  if (!base::MessageLoopForIO::current()->WatchFileDescriptor(
          self->fd_, true, base::MessageLoopForIO::WATCH_WRITE,
          &self->file_watcher_, self.get())) {
    USB_LOG(ERROR) << "Failed to start watching device file descriptor.";
  }

  // |self| is now owned by the current message loop.
  base::MessageLoop::current()->AddDestructionObserver(self.release());
}

void UsbDeviceHandleUsbfs::FileThreadHelper::OnFileCanReadWithoutBlocking(
    int fd) {
  NOTREACHED();  // Only listening for writability.
}

void UsbDeviceHandleUsbfs::FileThreadHelper::OnFileCanWriteWithoutBlocking(
    int fd) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(fd_, fd);

  const size_t MAX_URBS_PER_EVENT = 10;
  std::vector<usbdevfs_urb*> urbs;
  urbs.reserve(MAX_URBS_PER_EVENT);
  for (size_t i = 0; i < MAX_URBS_PER_EVENT; ++i) {
    usbdevfs_urb* urb;
    int rc = HANDLE_EINTR(ioctl(fd_, USBDEVFS_REAPURBNDELAY, &urb));
    if (rc) {
      if (errno == EAGAIN)
        break;
      USB_PLOG(DEBUG) << "Failed to reap urbs";
      if (errno == ENODEV) {
        // Device has disconnected. Stop watching the file descriptor to avoid
        // looping until |device_handle_| is closed.
        file_watcher_.StopWatchingFileDescriptor();
        break;
      }
    } else {
      urbs.push_back(urb);
    }
  }

  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&UsbDeviceHandleUsbfs::ReapedUrbs, device_handle_, urbs));
}

void UsbDeviceHandleUsbfs::FileThreadHelper::WillDestroyCurrentMessageLoop() {
  DCHECK(thread_checker_.CalledOnValidThread());
  delete this;
}

struct UsbDeviceHandleUsbfs::Transfer {
  Transfer() = delete;
  Transfer(scoped_refptr<net::IOBuffer> buffer,
           const TransferCallback& callback,
           scoped_refptr<base::SingleThreadTaskRunner> callback_runner);
  Transfer(scoped_refptr<net::IOBuffer> buffer,
           const IsochronousTransferCallback& callback);
  ~Transfer();

  void* operator new(std::size_t size, size_t number_of_iso_packets);
  void RunCallback(UsbTransferStatus status, size_t bytes_transferred);
  void RunIsochronousCallback(const std::vector<IsochronousPacket>& packets);

  scoped_refptr<net::IOBuffer> control_transfer_buffer;
  scoped_refptr<net::IOBuffer> buffer;
  TransferCallback callback;
  IsochronousTransferCallback isoc_callback;
  scoped_refptr<base::SingleThreadTaskRunner> callback_runner;
  base::CancelableClosure timeout_closure;
  bool cancelled = false;

  // When the URB is |cancelled| these two flags track whether the URB has both
  // been |discarded| and |reaped| since the possiblity of last-minute
  // completion makes these two conditions race.
  bool discarded = false;
  bool reaped = false;

  // The |urb| field must be the last in the struct so that the extra space
  // allocated by the overridden new function above extends the length of its
  // |iso_frame_desc| field.
  usbdevfs_urb urb;

 private:
  DISALLOW_COPY_AND_ASSIGN(Transfer);
};

UsbDeviceHandleUsbfs::Transfer::Transfer(
    scoped_refptr<net::IOBuffer> buffer,
    const TransferCallback& callback,
    scoped_refptr<base::SingleThreadTaskRunner> callback_runner)
    : buffer(buffer), callback(callback), callback_runner(callback_runner) {
  memset(&urb, 0, sizeof(urb));
  urb.usercontext = this;
  urb.buffer = buffer->data();
}

UsbDeviceHandleUsbfs::Transfer::Transfer(
    scoped_refptr<net::IOBuffer> buffer,
    const IsochronousTransferCallback& callback)
    : buffer(buffer), isoc_callback(callback) {
  memset(&urb, 0, sizeof(urb) +
                      sizeof(usbdevfs_iso_packet_desc) * urb.number_of_packets);
  urb.usercontext = this;
  urb.buffer = buffer->data();
}

UsbDeviceHandleUsbfs::Transfer::~Transfer() = default;

void* UsbDeviceHandleUsbfs::Transfer::operator new(
    std::size_t size,
    size_t number_of_iso_packets) {
  void* p = ::operator new(
      size + sizeof(usbdevfs_iso_packet_desc) * number_of_iso_packets);
  Transfer* transfer = static_cast<Transfer*>(p);
  transfer->urb.number_of_packets = number_of_iso_packets;
  return p;
}

void UsbDeviceHandleUsbfs::Transfer::RunCallback(UsbTransferStatus status,
                                                 size_t bytes_transferred) {
  DCHECK_NE(urb.type, USBDEVFS_URB_TYPE_ISO);
  DCHECK(!callback.is_null());
  if (!callback_runner || callback_runner->BelongsToCurrentThread()) {
    callback.Run(status, buffer, bytes_transferred);
  } else {
    callback_runner->PostTask(
        FROM_HERE, base::Bind(callback, status, buffer, bytes_transferred));
  }
}

void UsbDeviceHandleUsbfs::Transfer::RunIsochronousCallback(
    const std::vector<IsochronousPacket>& packets) {
  DCHECK_EQ(urb.type, USBDEVFS_URB_TYPE_ISO);
  DCHECK(!isoc_callback.is_null());
  isoc_callback.Run(buffer, packets);
}

UsbDeviceHandleUsbfs::UsbDeviceHandleUsbfs(
    scoped_refptr<UsbDevice> device,
    base::ScopedFD fd,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner)
    : device_(device),
      fd_(std::move(fd)),
      blocking_task_runner_(blocking_task_runner) {
  DCHECK(device_);
  DCHECK(fd_.is_valid());
  DCHECK(blocking_task_runner_);

  task_runner_ = base::ThreadTaskRunnerHandle::Get();

  std::unique_ptr<FileThreadHelper> helper(
      new FileThreadHelper(fd_.get(), this, task_runner_));
  helper_ = helper.get();
  blocking_task_runner_->PostTask(
      FROM_HERE, base::Bind(&FileThreadHelper::Start, base::Passed(&helper)));
}

scoped_refptr<UsbDevice> UsbDeviceHandleUsbfs::GetDevice() const {
  return device_;
}

void UsbDeviceHandleUsbfs::Close() {
  if (!device_)
    return;  // Already closed.

  // On the |task_runner_| thread check |device_| to see if the handle is
  // closed. On the |blocking_task_runner_| thread check |fd_.is_valid()| to
  // see if the handle is closed.
  for (const auto& transfer : transfers_)
    CancelTransfer(transfer.get(), USB_TRANSFER_CANCELLED);
  device_->HandleClosed(this);
  device_ = nullptr;
  blocking_task_runner_->PostTask(
      FROM_HERE, base::Bind(&UsbDeviceHandleUsbfs::CloseBlocking, this));
}

void UsbDeviceHandleUsbfs::SetConfiguration(int configuration_value,
                                            const ResultCallback& callback) {
  // USBDEVFS_SETCONFIGURATION synchronously issues a SET_CONFIGURATION request
  // to the device so it must be performed on a thread where it is okay to
  // block.
  blocking_task_runner_->PostTask(
      FROM_HERE, base::Bind(&UsbDeviceHandleUsbfs::SetConfigurationBlocking,
                            this, configuration_value, callback));
}

void UsbDeviceHandleUsbfs::ClaimInterface(int interface_number,
                                          const ResultCallback& callback) {
  if (!device_) {
    task_runner_->PostTask(FROM_HERE, base::Bind(callback, false));
    return;
  }

  if (ContainsKey(interfaces_, interface_number)) {
    USB_LOG(DEBUG) << "Interface " << interface_number << " already claimed.";
    task_runner_->PostTask(FROM_HERE, base::Bind(callback, false));
    return;
  }

  // It appears safe to assume that this ioctl will not block.
  int rc = HANDLE_EINTR(
      ioctl(fd_.get(), USBDEVFS_CLAIMINTERFACE, &interface_number));
  if (rc) {
    USB_PLOG(DEBUG) << "Failed to claim interface " << interface_number;
  } else {
    interfaces_[interface_number].alternate_setting = 0;
    RefreshEndpointInfo();
  }
  task_runner_->PostTask(FROM_HERE, base::Bind(callback, rc == 0));
}

void UsbDeviceHandleUsbfs::ReleaseInterface(int interface_number,
                                            const ResultCallback& callback) {
  // USBDEVFS_RELEASEINTERFACE may issue a SET_INTERFACE request to the
  // device to restore alternate setting 0 so it must be performed on a thread
  // where it is okay to block.
  blocking_task_runner_->PostTask(
      FROM_HERE, base::Bind(&UsbDeviceHandleUsbfs::ReleaseInterfaceBlocking,
                            this, interface_number, callback));
}

void UsbDeviceHandleUsbfs::SetInterfaceAlternateSetting(
    int interface_number,
    int alternate_setting,
    const ResultCallback& callback) {
  // USBDEVFS_SETINTERFACE is synchronous because it issues a SET_INTERFACE
  // request to the device so it must be performed on a thread where it is okay
  // to block.
  blocking_task_runner_->PostTask(
      FROM_HERE, base::Bind(&UsbDeviceHandleUsbfs::SetInterfaceBlocking, this,
                            interface_number, alternate_setting, callback));
}

void UsbDeviceHandleUsbfs::ResetDevice(const ResultCallback& callback) {
  // USBDEVFS_RESET is synchronous because it waits for the port to be reset
  // and the device re-enumerated so it must be performed on a thread where it
  // is okay to block.
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&UsbDeviceHandleUsbfs::ResetDeviceBlocking, this, callback));
}

void UsbDeviceHandleUsbfs::ClearHalt(uint8_t endpoint_address,
                                     const ResultCallback& callback) {
  // USBDEVFS_CLEAR_HALT is synchronous because it issues a CLEAR_FEATURE
  // request to the device so it must be performed on a thread where it is okay
  // to block.
  blocking_task_runner_->PostTask(
      FROM_HERE, base::Bind(&UsbDeviceHandleUsbfs::ClearHaltBlocking, this,
                            endpoint_address, callback));
}

void UsbDeviceHandleUsbfs::ControlTransfer(UsbEndpointDirection direction,
                                           TransferRequestType request_type,
                                           TransferRecipient recipient,
                                           uint8_t request,
                                           uint16_t value,
                                           uint16_t index,
                                           scoped_refptr<net::IOBuffer> buffer,
                                           size_t length,
                                           unsigned int timeout,
                                           const TransferCallback& callback) {
  if (!device_) {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(callback, USB_TRANSFER_DISCONNECT, nullptr, 0));
    return;
  }

  std::unique_ptr<Transfer> transfer(new (0)
                                         Transfer(buffer, callback, nullptr));
  transfer->control_transfer_buffer =
      BuildControlTransferBuffer(direction, request_type, recipient, request,
                                 value, index, buffer, length);
  transfer->urb.type = USBDEVFS_URB_TYPE_CONTROL;
  transfer->urb.endpoint = 0;
  transfer->urb.buffer = transfer->control_transfer_buffer->data();
  transfer->urb.buffer_length = 8 + length;

  // USBDEVFS_SUBMITURB appears to be non-blocking as completion is reported
  // by USBDEVFS_REAPURBNDELAY.
  int rc = HANDLE_EINTR(ioctl(fd_.get(), USBDEVFS_SUBMITURB, &transfer->urb));
  if (rc) {
    rc = logging::GetLastSystemErrorCode();
    USB_PLOG(DEBUG) << "Failed to submit control transfer";
    task_runner_->PostTask(
        FROM_HERE, base::Bind(callback, ConvertTransferResult(rc), nullptr, 0));
  } else {
    SetUpTimeoutCallback(transfer.get(), timeout);
    transfers_.push_back(std::move(transfer));
  }
}

void UsbDeviceHandleUsbfs::IsochronousTransferIn(
    uint8_t endpoint_number,
    const std::vector<uint32_t>& packet_lengths,
    unsigned int timeout,
    const IsochronousTransferCallback& callback) {
  uint8_t endpoint_address = USB_DIR_IN | endpoint_number;
  size_t total_length =
      std::accumulate(packet_lengths.begin(), packet_lengths.end(), 0u);
  scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(total_length));
  IsochronousTransferInternal(endpoint_address, buffer, total_length,
                              packet_lengths, timeout, callback);
}

void UsbDeviceHandleUsbfs::IsochronousTransferOut(
    uint8_t endpoint_number,
    scoped_refptr<net::IOBuffer> buffer,
    const std::vector<uint32_t>& packet_lengths,
    unsigned int timeout,
    const IsochronousTransferCallback& callback) {
  uint8_t endpoint_address = USB_DIR_OUT | endpoint_number;
  size_t total_length =
      std::accumulate(packet_lengths.begin(), packet_lengths.end(), 0u);
  IsochronousTransferInternal(endpoint_address, buffer, total_length,
                              packet_lengths, timeout, callback);
}

void UsbDeviceHandleUsbfs::GenericTransfer(UsbEndpointDirection direction,
                                           uint8_t endpoint_number,
                                           scoped_refptr<net::IOBuffer> buffer,
                                           size_t length,
                                           unsigned int timeout,
                                           const TransferCallback& callback) {
  if (task_runner_->BelongsToCurrentThread()) {
    GenericTransferInternal(direction, endpoint_number, buffer, length, timeout,
                            callback, task_runner_);
  } else {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&UsbDeviceHandleUsbfs::GenericTransferInternal, this,
                   direction, endpoint_number, buffer, length, timeout,
                   callback, base::ThreadTaskRunnerHandle::Get()));
  }
}

const UsbInterfaceDescriptor* UsbDeviceHandleUsbfs::FindInterfaceByEndpoint(
    uint8_t endpoint_address) {
  auto it = endpoints_.find(endpoint_address);
  if (it != endpoints_.end())
    return it->second.interface;
  return nullptr;
}

UsbDeviceHandleUsbfs::~UsbDeviceHandleUsbfs() {
  DCHECK(!device_) << "Handle must be closed before it is destroyed.";
}

void UsbDeviceHandleUsbfs::ReleaseFileDescriptor() {
  ignore_result(fd_.release());
  delete helper_;
}

void UsbDeviceHandleUsbfs::CloseBlocking() {
  fd_.reset(-1);
  delete helper_;
}

void UsbDeviceHandleUsbfs::SetConfigurationBlocking(
    int configuration_value,
    const ResultCallback& callback) {
  if (!fd_.is_valid()) {
    task_runner_->PostTask(FROM_HERE, base::Bind(callback, false));
    return;
  }

  int rc = HANDLE_EINTR(
      ioctl(fd_.get(), USBDEVFS_SETCONFIGURATION, &configuration_value));
  if (rc)
    USB_PLOG(DEBUG) << "Failed to set configuration " << configuration_value;
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&UsbDeviceHandleUsbfs::SetConfigurationComplete,
                            this, configuration_value, rc == 0, callback));
}

void UsbDeviceHandleUsbfs::SetConfigurationComplete(
    int configuration_value,
    bool success,
    const ResultCallback& callback) {
  if (success && device_) {
    device_->ActiveConfigurationChanged(configuration_value);
    // TODO(reillyg): If all interfaces are unclaimed before a new configuration
    // is set then this will do nothing. Investigate.
    RefreshEndpointInfo();
  }
  callback.Run(success);
}

void UsbDeviceHandleUsbfs::ReleaseInterfaceBlocking(
    int interface_number,
    const ResultCallback& callback) {
  if (!fd_.is_valid()) {
    task_runner_->PostTask(FROM_HERE, base::Bind(callback, false));
    return;
  }

  int rc = HANDLE_EINTR(
      ioctl(fd_.get(), USBDEVFS_RELEASEINTERFACE, &interface_number));
  if (rc) {
    USB_PLOG(DEBUG) << "Failed to release interface " << interface_number;
    task_runner_->PostTask(FROM_HERE, base::Bind(callback, false));
  } else {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&UsbDeviceHandleUsbfs::ReleaseInterfaceComplete,
                              this, interface_number, callback));
  }
}

void UsbDeviceHandleUsbfs::ReleaseInterfaceComplete(
    int interface_number,
    const ResultCallback& callback) {
  auto it = interfaces_.find(interface_number);
  DCHECK(it != interfaces_.end());
  interfaces_.erase(it);
  RefreshEndpointInfo();
  callback.Run(true);
}

void UsbDeviceHandleUsbfs::SetInterfaceBlocking(
    int interface_number,
    int alternate_setting,
    const ResultCallback& callback) {
  if (!fd_.is_valid()) {
    task_runner_->PostTask(FROM_HERE, base::Bind(callback, false));
    return;
  }

  usbdevfs_setinterface cmd = {0};
  cmd.interface = interface_number;
  cmd.altsetting = alternate_setting;
  int rc = HANDLE_EINTR(ioctl(fd_.get(), USBDEVFS_SETINTERFACE, &cmd));
  if (rc) {
    USB_PLOG(DEBUG) << "Failed to set interface " << interface_number
                    << " to alternate setting " << alternate_setting;
  }
  task_runner_->PostTask(FROM_HERE, base::Bind(callback, rc == 0));
}

void UsbDeviceHandleUsbfs::ResetDeviceBlocking(const ResultCallback& callback) {
  if (!fd_.is_valid()) {
    task_runner_->PostTask(FROM_HERE, base::Bind(callback, false));
    return;
  }

  // TODO(reillyg): libusb releases interfaces before and then reclaims
  // interfaces after a reset. We should probably do this too or document that
  // callers have to call ClaimInterface as well.
  int rc = HANDLE_EINTR(ioctl(fd_.get(), USBDEVFS_RESET, nullptr));
  if (rc)
    USB_PLOG(DEBUG) << "Failed to reset the device";
  task_runner_->PostTask(FROM_HERE, base::Bind(callback, rc == 0));
}

void UsbDeviceHandleUsbfs::ClearHaltBlocking(uint8_t endpoint_address,
                                             const ResultCallback& callback) {
  if (!fd_.is_valid()) {
    task_runner_->PostTask(FROM_HERE, base::Bind(callback, false));
    return;
  }

  int tmp_endpoint = endpoint_address;
  int rc = HANDLE_EINTR(ioctl(fd_.get(), USBDEVFS_CLEAR_HALT, &tmp_endpoint));
  if (rc) {
    USB_PLOG(DEBUG) << "Failed to clear the stall condition on endpoint "
                    << static_cast<int>(endpoint_address);
  }
  task_runner_->PostTask(FROM_HERE, base::Bind(callback, rc == 0));
}

void UsbDeviceHandleUsbfs::IsochronousTransferInternal(
    uint8_t endpoint_address,
    scoped_refptr<net::IOBuffer> buffer,
    size_t total_length,
    const std::vector<uint32_t>& packet_lengths,
    unsigned int timeout,
    const IsochronousTransferCallback& callback) {
  if (!device_) {
    ReportIsochronousError(packet_lengths, callback, USB_TRANSFER_DISCONNECT);
    return;
  }

  auto it = endpoints_.find(endpoint_address);
  if (it == endpoints_.end()) {
    USB_LOG(USER) << "Endpoint address " << static_cast<int>(endpoint_address)
                  << " is not part of a claimed interface.";
    ReportIsochronousError(packet_lengths, callback, USB_TRANSFER_ERROR);
    return;
  }

  std::unique_ptr<Transfer> transfer(new (packet_lengths.size())
                                         Transfer(buffer, callback));
  transfer->urb.type = USBDEVFS_URB_TYPE_ISO;
  transfer->urb.endpoint = endpoint_address;
  transfer->urb.buffer_length = total_length;

  for (size_t i = 0; i < packet_lengths.size(); ++i)
    transfer->urb.iso_frame_desc[i].length = packet_lengths[i];

  // USBDEVFS_SUBMITURB appears to be non-blocking as completion is reported
  // by USBDEVFS_REAPURBNDELAY. This code assumes a recent kernel that can
  // accept arbitrarily large transfer requests, hopefully also using a scatter-
  // gather list.
  int rc = HANDLE_EINTR(ioctl(fd_.get(), USBDEVFS_SUBMITURB, &transfer->urb));
  if (rc) {
    rc = logging::GetLastSystemErrorCode();
    USB_PLOG(DEBUG) << "Failed to submit transfer";
    ReportIsochronousError(packet_lengths, callback, ConvertTransferResult(rc));
  } else {
    SetUpTimeoutCallback(transfer.get(), timeout);
    transfers_.push_back(std::move(transfer));
  }
}

void UsbDeviceHandleUsbfs::GenericTransferInternal(
    UsbEndpointDirection direction,
    uint8_t endpoint_number,
    scoped_refptr<net::IOBuffer> buffer,
    size_t length,
    unsigned int timeout,
    const TransferCallback& callback,
    scoped_refptr<base::SingleThreadTaskRunner> callback_runner) {
  if (!device_) {
    callback_runner->PostTask(
        FROM_HERE, base::Bind(callback, USB_TRANSFER_DISCONNECT, nullptr, 0));
    return;
  }

  uint8_t endpoint_address =
      ConvertEndpointDirection(direction) | endpoint_number;
  auto it = endpoints_.find(endpoint_address);
  if (it == endpoints_.end()) {
    USB_LOG(USER) << "Endpoint address " << static_cast<int>(endpoint_address)
                  << " is not part of a claimed interface.";
    callback_runner->PostTask(
        FROM_HERE, base::Bind(callback, USB_TRANSFER_ERROR, nullptr, 0));
    return;
  }

  std::unique_ptr<Transfer> transfer(
      new (0) Transfer(buffer, callback, callback_runner));
  transfer->urb.endpoint = endpoint_address;
  transfer->urb.buffer_length = length;
  transfer->urb.type = ConvertTransferType(it->second.type);

  // USBDEVFS_SUBMITURB appears to be non-blocking as completion is reported
  // by USBDEVFS_REAPURBNDELAY. This code assumes a recent kernel that can
  // accept arbitrarily large transfer requests, hopefully also using a scatter-
  // gather list.
  int rc = HANDLE_EINTR(ioctl(fd_.get(), USBDEVFS_SUBMITURB, &transfer->urb));
  if (rc) {
    rc = logging::GetLastSystemErrorCode();
    USB_PLOG(DEBUG) << "Failed to submit transfer";
    callback_runner->PostTask(
        FROM_HERE, base::Bind(callback, ConvertTransferResult(rc), nullptr, 0));
  } else {
    SetUpTimeoutCallback(transfer.get(), timeout);
    transfers_.push_back(std::move(transfer));
  }
}

void UsbDeviceHandleUsbfs::ReapedUrbs(const std::vector<usbdevfs_urb*>& urbs) {
  for (auto* urb : urbs) {
    Transfer* transfer = static_cast<Transfer*>(urb->usercontext);
    DCHECK_EQ(urb, &transfer->urb);

    if (transfer->cancelled) {
      transfer->reaped = true;
      if (transfer->discarded)
        RemoveFromTransferList(transfer);
    } else {
      TransferComplete(RemoveFromTransferList(transfer));
    }
  }
}

void UsbDeviceHandleUsbfs::TransferComplete(
    std::unique_ptr<Transfer> transfer) {
  if (transfer->cancelled)
    return;

  // The transfer will soon be freed. Cancel the timeout callback so that the
  // raw pointer it holds to |transfer| is not used.
  transfer->timeout_closure.Cancel();

  if (transfer->urb.type == USBDEVFS_URB_TYPE_ISO) {
    std::vector<IsochronousPacket> packets(transfer->urb.number_of_packets);
    for (size_t i = 0; i < packets.size(); ++i) {
      packets[i].length = transfer->urb.iso_frame_desc[i].length;
      packets[i].transferred_length =
          transfer->urb.iso_frame_desc[i].actual_length;
      packets[i].status = ConvertTransferResult(
          transfer->urb.status == 0 ? transfer->urb.iso_frame_desc[i].status
                                    : transfer->urb.status);
    }

    transfer->RunIsochronousCallback(packets);
  } else {
    if (transfer->urb.status == 0 &&
        transfer->urb.type == USBDEVFS_URB_TYPE_CONTROL) {
      // Copy the result of the control transfer back into the original buffer.
      memcpy(transfer->buffer->data(),
             transfer->control_transfer_buffer->data() + 8,
             transfer->urb.actual_length);
    }

    transfer->RunCallback(ConvertTransferResult(-transfer->urb.status),
                          transfer->urb.actual_length);
  }
}

void UsbDeviceHandleUsbfs::RefreshEndpointInfo() {
  endpoints_.clear();

  const UsbConfigDescriptor* config = device_->active_configuration();
  if (!config)
    return;

  for (const auto& entry : interfaces_) {
    auto interface_it = std::find_if(
        config->interfaces.begin(), config->interfaces.end(),
        [entry](const UsbInterfaceDescriptor& interface) {
          uint8_t interface_number = entry.first;
          uint8_t alternate_setting = entry.second.alternate_setting;
          return interface.interface_number == interface_number &&
                 interface.alternate_setting == alternate_setting;
        });
    DCHECK(interface_it != config->interfaces.end());

    for (const auto& endpoint : interface_it->endpoints) {
      EndpointInfo& info = endpoints_[endpoint.address];
      info.type = endpoint.transfer_type;
      info.interface = &*interface_it;
    }
  }
}

void UsbDeviceHandleUsbfs::ReportIsochronousError(
    const std::vector<uint32_t>& packet_lengths,
    const UsbDeviceHandle::IsochronousTransferCallback& callback,
    UsbTransferStatus status) {
  std::vector<UsbDeviceHandle::IsochronousPacket> packets(
      packet_lengths.size());
  for (size_t i = 0; i < packet_lengths.size(); ++i) {
    packets[i].length = packet_lengths[i];
    packets[i].transferred_length = 0;
    packets[i].status = status;
  }
  task_runner_->PostTask(FROM_HERE, base::Bind(callback, nullptr, packets));
}

void UsbDeviceHandleUsbfs::SetUpTimeoutCallback(Transfer* transfer,
                                                unsigned int timeout) {
  if (timeout > 0) {
    transfer->timeout_closure.Reset(
        base::Bind(&UsbDeviceHandleUsbfs::CancelTransfer, this, transfer,
                   USB_TRANSFER_TIMEOUT));
    task_runner_->PostDelayedTask(FROM_HERE,
                                  transfer->timeout_closure.callback(),
                                  base::TimeDelta::FromMilliseconds(timeout));
  }
}

std::unique_ptr<UsbDeviceHandleUsbfs::Transfer>
UsbDeviceHandleUsbfs::RemoveFromTransferList(Transfer* transfer_ptr) {
  auto it = std::find_if(
      transfers_.begin(), transfers_.end(),
      [transfer_ptr](const std::unique_ptr<Transfer>& transfer) -> bool {
        return transfer.get() == transfer_ptr;
      });
  DCHECK(it != transfers_.end());
  std::unique_ptr<Transfer> transfer = std::move(*it);
  transfers_.erase(it);
  return transfer;
}

void UsbDeviceHandleUsbfs::CancelTransfer(Transfer* transfer,
                                          UsbTransferStatus status) {
  // |transfer| must stay in |transfers_| as it is still being processed by the
  // kernel and will be reaped later.
  transfer->cancelled = true;
  transfer->timeout_closure.Cancel();

  if (transfer->urb.type == USBDEVFS_URB_TYPE_ISO) {
    std::vector<IsochronousPacket> packets(transfer->urb.number_of_packets);
    for (size_t i = 0; i < packets.size(); ++i) {
      packets[i].length = transfer->urb.iso_frame_desc[i].length;
      packets[i].transferred_length = 0;
      packets[i].status = status;
    }
    transfer->RunIsochronousCallback(packets);
  } else {
    transfer->RunCallback(status, 0);
  }

  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&UsbDeviceHandleUsbfs::DiscardUrbBlocking, this, transfer));
}

void UsbDeviceHandleUsbfs::DiscardUrbBlocking(Transfer* transfer) {
  if (fd_.is_valid())
    HANDLE_EINTR(ioctl(fd_.get(), USBDEVFS_DISCARDURB, &transfer->urb));

  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&UsbDeviceHandleUsbfs::UrbDiscarded, this, transfer));
}

void UsbDeviceHandleUsbfs::UrbDiscarded(Transfer* transfer) {
  transfer->discarded = true;
  if (transfer->reaped)
    RemoveFromTransferList(transfer);
}

}  // namespace device
