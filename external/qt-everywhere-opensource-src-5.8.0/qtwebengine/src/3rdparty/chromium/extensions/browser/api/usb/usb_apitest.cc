// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <numeric>

#include "base/memory/ptr_util.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_utils.h"
#include "device/core/mock_device_client.h"
#include "device/usb/mock_usb_device.h"
#include "device/usb/mock_usb_device_handle.h"
#include "device/usb/mock_usb_service.h"
#include "extensions/browser/api/device_permissions_prompt.h"
#include "extensions/browser/api/usb/usb_api.h"
#include "extensions/shell/browser/shell_extensions_api_client.h"
#include "extensions/shell/test/shell_apitest.h"
#include "extensions/test/extension_test_message_listener.h"
#include "net/base/io_buffer.h"

using testing::_;
using testing::AnyNumber;
using testing::Invoke;
using testing::Return;
using device::MockDeviceClient;
using device::MockUsbDevice;
using device::MockUsbDeviceHandle;
using device::UsbConfigDescriptor;
using device::UsbDeviceHandle;
using device::UsbEndpointDirection;
using device::UsbInterfaceDescriptor;

namespace extensions {

namespace {

ACTION_TEMPLATE(InvokeCallback,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(p1)) {
  ::std::tr1::get<k>(args).Run(p1);
}

ACTION_TEMPLATE(InvokeUsbTransferCallback,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(p1)) {
  net::IOBuffer* io_buffer = nullptr;
  size_t length = 0;
  if (p1 != device::USB_TRANSFER_ERROR) {
    length = 1;
    io_buffer = new net::IOBuffer(length);
    memset(io_buffer->data(), 0, length);  // Avoid uninitialized reads.
  }
  ::std::tr1::get<k>(args).Run(p1, io_buffer, 1);
}

ACTION_P2(InvokeUsbIsochronousTransferOutCallback,
          transferred_length,
          success_packets) {
  std::vector<UsbDeviceHandle::IsochronousPacket> packets(arg2.size());
  for (size_t i = 0; i < packets.size(); ++i) {
    packets[i].length = arg2[i];
    if (i < success_packets) {
      packets[i].transferred_length = transferred_length;
      packets[i].status = device::USB_TRANSFER_COMPLETED;
    } else {
      packets[i].transferred_length = 0;
      packets[i].status = device::USB_TRANSFER_ERROR;
    }
  }
  arg4.Run(arg1, packets);
}

ACTION_P2(InvokeUsbIsochronousTransferInCallback,
          transferred_length,
          success_packets) {
  size_t total_length = std::accumulate(arg1.begin(), arg1.end(), 0u);
  net::IOBuffer* io_buffer = new net::IOBuffer(total_length);
  memset(io_buffer->data(), 0, total_length);  // Avoid uninitialized reads.
  std::vector<UsbDeviceHandle::IsochronousPacket> packets(arg1.size());
  for (size_t i = 0; i < packets.size(); ++i) {
    packets[i].length = arg1[i];
    packets[i].transferred_length = transferred_length;
    if (i < success_packets) {
      packets[i].transferred_length = transferred_length;
      packets[i].status = device::USB_TRANSFER_COMPLETED;
    } else {
      packets[i].transferred_length = 0;
      packets[i].status = device::USB_TRANSFER_ERROR;
    }
  }
  arg3.Run(io_buffer, packets);
}

ACTION_P(SetConfiguration, mock_device) {
  mock_device->ActiveConfigurationChanged(arg0);
  arg1.Run(true);
}

class TestDevicePermissionsPrompt
    : public DevicePermissionsPrompt,
      public DevicePermissionsPrompt::Prompt::Observer {
 public:
  explicit TestDevicePermissionsPrompt(content::WebContents* web_contents)
      : DevicePermissionsPrompt(web_contents) {}

  void ShowDialog() override { prompt()->SetObserver(this); }

  void OnDevicesChanged() override {
    for (size_t i = 0; i < prompt()->GetDeviceCount(); ++i) {
      prompt()->GrantDevicePermission(i);
      if (!prompt()->multiple()) {
        break;
      }
    }
    prompt()->Dismissed();
  }
};

class TestExtensionsAPIClient : public ShellExtensionsAPIClient {
 public:
  TestExtensionsAPIClient() : ShellExtensionsAPIClient() {}

  std::unique_ptr<DevicePermissionsPrompt> CreateDevicePermissionsPrompt(
      content::WebContents* web_contents) const override {
    return base::WrapUnique(new TestDevicePermissionsPrompt(web_contents));
  }
};

class UsbApiTest : public ShellApiTest {
 public:
  void SetUpOnMainThread() override {
    ShellApiTest::SetUpOnMainThread();

    // MockDeviceClient replaces ShellDeviceClient.
    device_client_.reset(new MockDeviceClient());

    std::vector<UsbConfigDescriptor> configs;
    configs.emplace_back(1, false, false, 0);
    configs.emplace_back(2, false, false, 0);

    mock_device_ = new MockUsbDevice(0, 0, "Test Manufacturer", "Test Device",
                                     "ABC123", configs);
    mock_device_handle_ = new MockUsbDeviceHandle(mock_device_.get());
    EXPECT_CALL(*mock_device_.get(), Open(_))
        .WillRepeatedly(InvokeCallback<0>(mock_device_handle_));
    device_client_->usb_service()->AddDevice(mock_device_);
  }

 protected:
  scoped_refptr<MockUsbDeviceHandle> mock_device_handle_;
  scoped_refptr<MockUsbDevice> mock_device_;
  std::unique_ptr<MockDeviceClient> device_client_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(UsbApiTest, DeviceHandling) {
  mock_device_->ActiveConfigurationChanged(1);
  EXPECT_CALL(*mock_device_handle_.get(), Close()).Times(2);
  ASSERT_TRUE(RunAppTest("api_test/usb/device_handling"));
}

IN_PROC_BROWSER_TEST_F(UsbApiTest, ResetDevice) {
  EXPECT_CALL(*mock_device_handle_.get(), Close()).Times(2);
  EXPECT_CALL(*mock_device_handle_.get(), ResetDevice(_))
      .WillOnce(InvokeCallback<0>(true))
      .WillOnce(InvokeCallback<0>(false));
  EXPECT_CALL(*mock_device_handle_.get(),
              GenericTransfer(device::USB_DIRECTION_OUTBOUND, 2, _, 1, _, _))
      .WillOnce(InvokeUsbTransferCallback<5>(device::USB_TRANSFER_COMPLETED));
  ASSERT_TRUE(RunAppTest("api_test/usb/reset_device"));
}

IN_PROC_BROWSER_TEST_F(UsbApiTest, SetConfiguration) {
  EXPECT_CALL(*mock_device_handle_.get(), SetConfiguration(1, _))
      .WillOnce(SetConfiguration(mock_device_.get()));
  EXPECT_CALL(*mock_device_handle_.get(), Close()).Times(1);
  ASSERT_TRUE(RunAppTest("api_test/usb/set_configuration"));
}

IN_PROC_BROWSER_TEST_F(UsbApiTest, ListInterfaces) {
  mock_device_->ActiveConfigurationChanged(1);
  EXPECT_CALL(*mock_device_handle_.get(), Close()).Times(1);
  ASSERT_TRUE(RunAppTest("api_test/usb/list_interfaces"));
}

IN_PROC_BROWSER_TEST_F(UsbApiTest, TransferEvent) {
  EXPECT_CALL(*mock_device_handle_.get(),
              ControlTransfer(device::USB_DIRECTION_OUTBOUND,
                              UsbDeviceHandle::STANDARD,
                              UsbDeviceHandle::DEVICE,
                              1,
                              2,
                              3,
                              _,
                              1,
                              _,
                              _))
      .WillOnce(InvokeUsbTransferCallback<9>(device::USB_TRANSFER_COMPLETED));
  EXPECT_CALL(*mock_device_handle_.get(),
              GenericTransfer(device::USB_DIRECTION_OUTBOUND, 1, _, 1, _, _))
      .WillOnce(InvokeUsbTransferCallback<5>(device::USB_TRANSFER_COMPLETED));
  EXPECT_CALL(*mock_device_handle_.get(),
              GenericTransfer(device::USB_DIRECTION_OUTBOUND, 2, _, 1, _, _))
      .WillOnce(InvokeUsbTransferCallback<5>(device::USB_TRANSFER_COMPLETED));
  EXPECT_CALL(*mock_device_handle_.get(), IsochronousTransferOut(3, _, _, _, _))
      .WillOnce(InvokeUsbIsochronousTransferOutCallback(1, 1u));
  EXPECT_CALL(*mock_device_handle_.get(), Close()).Times(AnyNumber());
  ASSERT_TRUE(RunAppTest("api_test/usb/transfer_event"));
}

IN_PROC_BROWSER_TEST_F(UsbApiTest, ZeroLengthTransfer) {
  EXPECT_CALL(*mock_device_handle_.get(), GenericTransfer(_, _, _, 0, _, _))
      .WillOnce(InvokeUsbTransferCallback<5>(device::USB_TRANSFER_COMPLETED));
  EXPECT_CALL(*mock_device_handle_.get(), Close()).Times(AnyNumber());
  ASSERT_TRUE(RunAppTest("api_test/usb/zero_length_transfer"));
}

IN_PROC_BROWSER_TEST_F(UsbApiTest, TransferFailure) {
  EXPECT_CALL(*mock_device_handle_.get(),
              GenericTransfer(device::USB_DIRECTION_OUTBOUND, 1, _, _, _, _))
      .WillOnce(InvokeUsbTransferCallback<5>(device::USB_TRANSFER_COMPLETED))
      .WillOnce(InvokeUsbTransferCallback<5>(device::USB_TRANSFER_ERROR))
      .WillOnce(InvokeUsbTransferCallback<5>(device::USB_TRANSFER_TIMEOUT));
  EXPECT_CALL(*mock_device_handle_.get(), IsochronousTransferIn(2, _, _, _))
      .WillOnce(InvokeUsbIsochronousTransferInCallback(8, 10u))
      .WillOnce(InvokeUsbIsochronousTransferInCallback(8, 5u));
  EXPECT_CALL(*mock_device_handle_.get(), Close()).Times(AnyNumber());
  ASSERT_TRUE(RunAppTest("api_test/usb/transfer_failure"));
}

IN_PROC_BROWSER_TEST_F(UsbApiTest, InvalidLengthTransfer) {
  EXPECT_CALL(*mock_device_handle_.get(), Close()).Times(AnyNumber());
  ASSERT_TRUE(RunAppTest("api_test/usb/invalid_length_transfer"));
}

IN_PROC_BROWSER_TEST_F(UsbApiTest, InvalidTimeout) {
  EXPECT_CALL(*mock_device_handle_.get(), Close()).Times(AnyNumber());
  ASSERT_TRUE(RunAppTest("api_test/usb/invalid_timeout"));
}

IN_PROC_BROWSER_TEST_F(UsbApiTest, OnDeviceAdded) {
  ExtensionTestMessageListener load_listener("loaded", false);
  ExtensionTestMessageListener result_listener("success", false);
  result_listener.set_failure_message("failure");

  ASSERT_TRUE(LoadApp("api_test/usb/add_event"));
  ASSERT_TRUE(load_listener.WaitUntilSatisfied());

  scoped_refptr<MockUsbDevice> device(new MockUsbDevice(0x18D1, 0x58F0));
  device_client_->usb_service()->AddDevice(device);

  device = new MockUsbDevice(0x18D1, 0x58F1);
  device_client_->usb_service()->AddDevice(device);

  ASSERT_TRUE(result_listener.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(UsbApiTest, OnDeviceRemoved) {
  ExtensionTestMessageListener load_listener("loaded", false);
  ExtensionTestMessageListener result_listener("success", false);
  result_listener.set_failure_message("failure");

  ASSERT_TRUE(LoadApp("api_test/usb/remove_event"));
  ASSERT_TRUE(load_listener.WaitUntilSatisfied());

  device_client_->usb_service()->RemoveDevice(mock_device_);
  ASSERT_TRUE(result_listener.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(UsbApiTest, GetUserSelectedDevices) {
  ExtensionTestMessageListener ready_listener("opened_device", false);
  ExtensionTestMessageListener result_listener("success", false);
  result_listener.set_failure_message("failure");

  EXPECT_CALL(*mock_device_handle_.get(), Close()).Times(1);

  TestExtensionsAPIClient test_api_client;
  ASSERT_TRUE(LoadApp("api_test/usb/get_user_selected_devices"));
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());

  device_client_->usb_service()->RemoveDevice(mock_device_);
  ASSERT_TRUE(result_listener.WaitUntilSatisfied());
}

}  // namespace extensions
