// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/mojo/device_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <numeric>
#include <queue>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "device/usb/mock_usb_device.h"
#include "device/usb/mock_usb_device_handle.h"
#include "device/usb/mojo/mock_permission_provider.h"
#include "device/usb/mojo/type_converters.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "net/base/io_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Invoke;
using ::testing::_;

namespace device {
namespace usb {

namespace {

class ConfigBuilder {
 public:
  explicit ConfigBuilder(uint8_t value) : config_(value, false, false, 0) {}

  ConfigBuilder& AddInterface(uint8_t interface_number,
                              uint8_t alternate_setting,
                              uint8_t class_code,
                              uint8_t subclass_code,
                              uint8_t protocol_code) {
    config_.interfaces.emplace_back(interface_number, alternate_setting,
                                    class_code, subclass_code, protocol_code);
    return *this;
  }

  const UsbConfigDescriptor& config() const { return config_; }

 private:
  UsbConfigDescriptor config_;
};

void ExpectOpenAndThen(OpenDeviceError expected,
                       const base::Closure& continuation,
                       OpenDeviceError error) {
  EXPECT_EQ(expected, error);
  continuation.Run();
}

void ExpectDeviceInfoAndThen(const std::string& guid,
                             uint16_t vendor_id,
                             uint16_t product_id,
                             const std::string& manufacturer_name,
                             const std::string& product_name,
                             const std::string& serial_number,
                             const base::Closure& continuation,
                             DeviceInfoPtr device_info) {
  EXPECT_EQ(guid, device_info->guid);
  EXPECT_EQ(vendor_id, device_info->vendor_id);
  EXPECT_EQ(product_id, device_info->product_id);
  EXPECT_EQ(manufacturer_name, device_info->manufacturer_name);
  EXPECT_EQ(product_name, device_info->product_name);
  EXPECT_EQ(serial_number, device_info->serial_number);
  continuation.Run();
}

void ExpectResultAndThen(bool expected_result,
                         const base::Closure& continuation,
                         bool actual_result) {
  EXPECT_EQ(expected_result, actual_result);
  continuation.Run();
}

void ExpectTransferInAndThen(TransferStatus expected_status,
                             const std::vector<uint8_t>& expected_bytes,
                             const base::Closure& continuation,
                             TransferStatus actual_status,
                             mojo::Array<uint8_t> actual_bytes) {
  EXPECT_EQ(expected_status, actual_status);
  ASSERT_EQ(expected_bytes.size(), actual_bytes.size());
  for (size_t i = 0; i < actual_bytes.size(); ++i) {
    EXPECT_EQ(expected_bytes[i], actual_bytes[i])
        << "Contents differ at index: " << i;
  }
  continuation.Run();
}

void ExpectPacketsOutAndThen(const std::vector<uint32_t>& expected_packets,
                             const base::Closure& continuation,
                             mojo::Array<IsochronousPacketPtr> actual_packets) {
  ASSERT_EQ(expected_packets.size(), actual_packets.size());
  for (size_t i = 0; i < expected_packets.size(); ++i) {
    EXPECT_EQ(expected_packets[i], actual_packets[i]->transferred_length)
        << "Packet lengths differ at index: " << i;
    EXPECT_EQ(TransferStatus::COMPLETED, actual_packets[i]->status)
        << "Packet at index " << i << " not completed.";
  }
  continuation.Run();
}

void ExpectPacketsInAndThen(const std::vector<uint8_t>& expected_bytes,
                            const std::vector<uint32_t>& expected_packets,
                            const base::Closure& continuation,
                            mojo::Array<uint8_t> actual_bytes,
                            mojo::Array<IsochronousPacketPtr> actual_packets) {
  ASSERT_EQ(expected_packets.size(), actual_packets.size());
  for (size_t i = 0; i < expected_packets.size(); ++i) {
    EXPECT_EQ(expected_packets[i], actual_packets[i]->transferred_length)
        << "Packet lengths differ at index: " << i;
    EXPECT_EQ(TransferStatus::COMPLETED, actual_packets[i]->status)
        << "Packet at index " << i << " not completed.";
  }
  ASSERT_EQ(expected_bytes.size(), actual_bytes.size());
  for (size_t i = 0; i < expected_bytes.size(); ++i) {
    EXPECT_EQ(expected_bytes[i], actual_bytes[i])
        << "Contents differ at index: " << i;
  }
  continuation.Run();
}

void ExpectTransferStatusAndThen(TransferStatus expected_status,
                                 const base::Closure& continuation,
                                 TransferStatus actual_status) {
  EXPECT_EQ(expected_status, actual_status);
  continuation.Run();
}

class USBDeviceImplTest : public testing::Test {
 public:
  USBDeviceImplTest()
      : message_loop_(new base::MessageLoop),
        is_device_open_(false),
        allow_reset_(false) {}

  ~USBDeviceImplTest() override {}

 protected:
  MockPermissionProvider& permission_provider() { return permission_provider_; }
  MockUsbDevice& mock_device() { return *mock_device_.get(); }
  bool is_device_open() const { return is_device_open_; }
  MockUsbDeviceHandle& mock_handle() { return *mock_handle_.get(); }

  void set_allow_reset(bool allow_reset) { allow_reset_ = allow_reset; }

  // Creates a mock device and binds a Device proxy to a Device service impl
  // wrapping the mock device.
  DevicePtr GetMockDeviceProxy(uint16_t vendor_id,
                               uint16_t product_id,
                               const std::string& manufacturer,
                               const std::string& product,
                               const std::string& serial) {
    mock_device_ =
        new MockUsbDevice(vendor_id, product_id, manufacturer, product, serial);
    mock_handle_ = new MockUsbDeviceHandle(mock_device_.get());

    DevicePtr proxy;
    new DeviceImpl(
        mock_device_,
        DeviceInfo::From(static_cast<const UsbDevice&>(*mock_device_)),
        permission_provider_.GetWeakPtr(), mojo::GetProxy(&proxy));

    // Set up mock handle calls to respond based on mock device configs
    // established by the test.
    ON_CALL(mock_device(), Open(_))
        .WillByDefault(Invoke(this, &USBDeviceImplTest::OpenMockHandle));
    ON_CALL(mock_handle(), Close())
        .WillByDefault(Invoke(this, &USBDeviceImplTest::CloseMockHandle));
    ON_CALL(mock_handle(), SetConfiguration(_, _))
        .WillByDefault(Invoke(this, &USBDeviceImplTest::SetConfiguration));
    ON_CALL(mock_handle(), ClaimInterface(_, _))
        .WillByDefault(Invoke(this, &USBDeviceImplTest::ClaimInterface));
    ON_CALL(mock_handle(), ReleaseInterface(_, _))
        .WillByDefault(Invoke(this, &USBDeviceImplTest::ReleaseInterface));
    ON_CALL(mock_handle(), SetInterfaceAlternateSetting(_, _, _))
        .WillByDefault(
            Invoke(this, &USBDeviceImplTest::SetInterfaceAlternateSetting));
    ON_CALL(mock_handle(), ResetDevice(_))
        .WillByDefault(Invoke(this, &USBDeviceImplTest::ResetDevice));
    ON_CALL(mock_handle(), ControlTransfer(_, _, _, _, _, _, _, _, _, _))
        .WillByDefault(Invoke(this, &USBDeviceImplTest::ControlTransfer));
    ON_CALL(mock_handle(), GenericTransfer(_, _, _, _, _, _))
        .WillByDefault(Invoke(this, &USBDeviceImplTest::GenericTransfer));
    ON_CALL(mock_handle(), IsochronousTransferIn(_, _, _, _))
        .WillByDefault(Invoke(this, &USBDeviceImplTest::IsochronousTransferIn));
    ON_CALL(mock_handle(), IsochronousTransferOut(_, _, _, _, _))
        .WillByDefault(
            Invoke(this, &USBDeviceImplTest::IsochronousTransferOut));

    return proxy;
  }

  DevicePtr GetMockDeviceProxy() {
    return GetMockDeviceProxy(0x1234, 0x5678, "ACME", "Frobinator", "ABCDEF");
  }

  void AddMockConfig(const ConfigBuilder& builder) {
    const UsbConfigDescriptor& config = builder.config();
    DCHECK(!ContainsKey(mock_configs_, config.configuration_value));
    mock_configs_.insert(std::make_pair(config.configuration_value, config));
    mock_device_->AddMockConfig(config);
  }

  void AddMockInboundData(const std::vector<uint8_t>& data) {
    mock_inbound_data_.push(data);
  }

  void AddMockInboundPackets(
      const std::vector<uint8_t>& data,
      const std::vector<UsbDeviceHandle::IsochronousPacket>& packets) {
    mock_inbound_data_.push(data);
    mock_inbound_packets_.push(packets);
  }

  void AddMockOutboundData(const std::vector<uint8_t>& data) {
    mock_outbound_data_.push(data);
  }

  void AddMockOutboundPackets(
      const std::vector<uint8_t>& data,
      const std::vector<UsbDeviceHandle::IsochronousPacket>& packets) {
    mock_outbound_data_.push(data);
    mock_outbound_packets_.push(packets);
  }

 private:
  void OpenMockHandle(const UsbDevice::OpenCallback& callback) {
    EXPECT_FALSE(is_device_open_);
    is_device_open_ = true;
    callback.Run(mock_handle_);
  }

  void CloseMockHandle() {
    EXPECT_TRUE(is_device_open_);
    is_device_open_ = false;
  }

  void SetConfiguration(uint8_t value,
                        const UsbDeviceHandle::ResultCallback& callback) {
    if (mock_configs_.find(value) != mock_configs_.end()) {
      mock_device_->ActiveConfigurationChanged(value);
      callback.Run(true);
    } else {
      callback.Run(false);
    }
  }

  void ClaimInterface(uint8_t interface_number,
                      const UsbDeviceHandle::ResultCallback& callback) {
    for (const auto& config : mock_configs_) {
      for (const auto& interface : config.second.interfaces) {
        if (interface.interface_number == interface_number) {
          claimed_interfaces_.insert(interface_number);
          callback.Run(true);
          return;
        }
      }
    }
    callback.Run(false);
  }

  void ReleaseInterface(uint8_t interface_number,
                        const UsbDeviceHandle::ResultCallback& callback) {
    if (ContainsKey(claimed_interfaces_, interface_number)) {
      claimed_interfaces_.erase(interface_number);
      callback.Run(true);
    } else {
      callback.Run(false);
    }
  }

  void SetInterfaceAlternateSetting(
      uint8_t interface_number,
      uint8_t alternate_setting,
      const UsbDeviceHandle::ResultCallback& callback) {
    for (const auto& config : mock_configs_) {
      for (const auto& interface : config.second.interfaces) {
        if (interface.interface_number == interface_number &&
            interface.alternate_setting == alternate_setting) {
          callback.Run(true);
          return;
        }
      }
    }
    callback.Run(false);
  }

  void ResetDevice(const UsbDeviceHandle::ResultCallback& callback) {
    callback.Run(allow_reset_);
  }

  void InboundTransfer(const UsbDeviceHandle::TransferCallback& callback) {
    ASSERT_GE(mock_inbound_data_.size(), 1u);
    const std::vector<uint8_t>& bytes = mock_inbound_data_.front();
    size_t length = bytes.size();
    scoped_refptr<net::IOBuffer> buffer = new net::IOBuffer(length);
    std::copy(bytes.begin(), bytes.end(), buffer->data());
    mock_inbound_data_.pop();
    callback.Run(USB_TRANSFER_COMPLETED, buffer, length);
  }

  void OutboundTransfer(scoped_refptr<net::IOBuffer> buffer,
                        size_t length,
                        const UsbDeviceHandle::TransferCallback& callback) {
    ASSERT_GE(mock_outbound_data_.size(), 1u);
    const std::vector<uint8_t>& bytes = mock_outbound_data_.front();
    ASSERT_EQ(bytes.size(), length);
    for (size_t i = 0; i < length; ++i) {
      EXPECT_EQ(bytes[i], buffer->data()[i]) << "Contents differ at index: "
                                             << i;
    }
    mock_outbound_data_.pop();
    callback.Run(USB_TRANSFER_COMPLETED, buffer, length);
  }

  void ControlTransfer(UsbEndpointDirection direction,
                       UsbDeviceHandle::TransferRequestType request_type,
                       UsbDeviceHandle::TransferRecipient recipient,
                       uint8_t request,
                       uint16_t value,
                       uint16_t index,
                       scoped_refptr<net::IOBuffer> buffer,
                       size_t length,
                       unsigned int timeout,
                       const UsbDeviceHandle::TransferCallback& callback) {
    if (direction == USB_DIRECTION_INBOUND)
      InboundTransfer(callback);
    else
      OutboundTransfer(buffer, length, callback);
  }

  void GenericTransfer(UsbEndpointDirection direction,
                       uint8_t endpoint,
                       scoped_refptr<net::IOBuffer> buffer,
                       size_t length,
                       unsigned int timeout,
                       const UsbDeviceHandle::TransferCallback& callback) {
    if (direction == USB_DIRECTION_INBOUND)
      InboundTransfer(callback);
    else
      OutboundTransfer(buffer, length, callback);
  }

  void IsochronousTransferIn(
      uint8_t endpoint_number,
      const std::vector<uint32_t>& packet_lengths,
      unsigned int timeout,
      const UsbDeviceHandle::IsochronousTransferCallback& callback) {
    ASSERT_FALSE(mock_inbound_data_.empty());
    const std::vector<uint8_t>& bytes = mock_inbound_data_.front();
    size_t length = bytes.size();
    scoped_refptr<net::IOBuffer> buffer = new net::IOBuffer(length);
    std::copy(bytes.begin(), bytes.end(), buffer->data());
    mock_inbound_data_.pop();

    ASSERT_FALSE(mock_inbound_packets_.empty());
    std::vector<UsbDeviceHandle::IsochronousPacket> packets =
        mock_inbound_packets_.front();
    ASSERT_EQ(packets.size(), packet_lengths.size());
    for (size_t i = 0; i < packets.size(); ++i) {
      EXPECT_EQ(packets[i].length, packet_lengths[i])
          << "Packet lengths differ at index: " << i;
    }
    mock_inbound_packets_.pop();

    callback.Run(buffer, packets);
  }

  void IsochronousTransferOut(
      uint8_t endpoint_number,
      scoped_refptr<net::IOBuffer> buffer,
      const std::vector<uint32_t>& packet_lengths,
      unsigned int timeout,
      const UsbDeviceHandle::IsochronousTransferCallback& callback) {
    ASSERT_FALSE(mock_outbound_data_.empty());
    const std::vector<uint8_t>& bytes = mock_outbound_data_.front();
    size_t length =
        std::accumulate(packet_lengths.begin(), packet_lengths.end(), 0u);
    ASSERT_EQ(bytes.size(), length);
    for (size_t i = 0; i < length; ++i) {
      EXPECT_EQ(bytes[i], buffer->data()[i]) << "Contents differ at index: "
                                             << i;
    }
    mock_outbound_data_.pop();

    ASSERT_FALSE(mock_outbound_packets_.empty());
    std::vector<UsbDeviceHandle::IsochronousPacket> packets =
        mock_outbound_packets_.front();
    ASSERT_EQ(packets.size(), packet_lengths.size());
    for (size_t i = 0; i < packets.size(); ++i) {
      EXPECT_EQ(packets[i].length, packet_lengths[i])
          << "Packet lengths differ at index: " << i;
    }
    mock_outbound_packets_.pop();

    callback.Run(buffer, packets);
  }

  std::unique_ptr<base::MessageLoop> message_loop_;
  scoped_refptr<MockUsbDevice> mock_device_;
  scoped_refptr<MockUsbDeviceHandle> mock_handle_;
  bool is_device_open_;
  bool allow_reset_;

  std::map<uint8_t, UsbConfigDescriptor> mock_configs_;

  std::queue<std::vector<uint8_t>> mock_inbound_data_;
  std::queue<std::vector<uint8_t>> mock_outbound_data_;
  std::queue<std::vector<UsbDeviceHandle::IsochronousPacket>>
      mock_inbound_packets_;
  std::queue<std::vector<UsbDeviceHandle::IsochronousPacket>>
      mock_outbound_packets_;

  std::set<uint8_t> claimed_interfaces_;

  MockPermissionProvider permission_provider_;

  DISALLOW_COPY_AND_ASSIGN(USBDeviceImplTest);
};

}  // namespace

TEST_F(USBDeviceImplTest, Disconnect) {
  DevicePtr device = GetMockDeviceProxy();

  base::RunLoop loop;
  device.set_connection_error_handler(loop.QuitClosure());
  mock_device().NotifyDeviceRemoved();
  loop.Run();
}

TEST_F(USBDeviceImplTest, Open) {
  DevicePtr device = GetMockDeviceProxy();

  EXPECT_FALSE(is_device_open());

  EXPECT_CALL(mock_device(), Open(_));

  {
    base::RunLoop loop;
    device->Open(base::Bind(&ExpectOpenAndThen, OpenDeviceError::OK,
                            loop.QuitClosure()));
    loop.Run();
  }

  {
    base::RunLoop loop;
    device->Open(base::Bind(&ExpectOpenAndThen, OpenDeviceError::ALREADY_OPEN,
                            loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_CALL(mock_handle(), Close());
}

TEST_F(USBDeviceImplTest, Close) {
  DevicePtr device = GetMockDeviceProxy();

  EXPECT_FALSE(is_device_open());

  EXPECT_CALL(mock_device(), Open(_));

  {
    base::RunLoop loop;
    device->Open(base::Bind(&ExpectOpenAndThen, OpenDeviceError::OK,
                            loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_CALL(mock_handle(), Close());

  {
    base::RunLoop loop;
    device->Close(loop.QuitClosure());
    loop.Run();
  }

  EXPECT_FALSE(is_device_open());
}

// Test that the information returned via the Device::GetDeviceInfo matches that
// of the underlying device.
TEST_F(USBDeviceImplTest, GetDeviceInfo) {
  DevicePtr device =
      GetMockDeviceProxy(0x1234, 0x5678, "ACME", "Frobinator", "ABCDEF");

  base::RunLoop loop;
  device->GetDeviceInfo(base::Bind(&ExpectDeviceInfoAndThen,
                                   mock_device().guid(), 0x1234, 0x5678, "ACME",
                                   "Frobinator", "ABCDEF", loop.QuitClosure()));
  loop.Run();
}

TEST_F(USBDeviceImplTest, SetInvalidConfiguration) {
  DevicePtr device = GetMockDeviceProxy();

  EXPECT_CALL(mock_device(), Open(_));

  {
    base::RunLoop loop;
    device->Open(base::Bind(&ExpectOpenAndThen, OpenDeviceError::OK,
                            loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_CALL(mock_handle(), SetConfiguration(42, _));
  EXPECT_CALL(permission_provider(), HasConfigurationPermission(42, _));

  {
    // SetConfiguration should fail because 42 is not a valid mock
    // configuration.
    base::RunLoop loop;
    device->SetConfiguration(
        42, base::Bind(&ExpectResultAndThen, false, loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_CALL(mock_handle(), Close());
}

TEST_F(USBDeviceImplTest, SetValidConfiguration) {
  DevicePtr device = GetMockDeviceProxy();

  EXPECT_CALL(mock_device(), Open(_));

  {
    base::RunLoop loop;
    device->Open(base::Bind(&ExpectOpenAndThen, OpenDeviceError::OK,
                            loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_CALL(mock_handle(), SetConfiguration(42, _));
  EXPECT_CALL(permission_provider(), HasConfigurationPermission(42, _));

  AddMockConfig(ConfigBuilder(42));

  {
    // SetConfiguration should succeed because 42 is a valid mock configuration.
    base::RunLoop loop;
    device->SetConfiguration(
        42, base::Bind(&ExpectResultAndThen, true, loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_CALL(mock_handle(), Close());
}

// Verify that the result of Reset() reflects the underlying UsbDeviceHandle's
// ResetDevice() result.
TEST_F(USBDeviceImplTest, Reset) {
  DevicePtr device = GetMockDeviceProxy();

  EXPECT_CALL(mock_device(), Open(_));

  {
    base::RunLoop loop;
    device->Open(base::Bind(&ExpectOpenAndThen, OpenDeviceError::OK,
                            loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_CALL(mock_handle(), ResetDevice(_));

  set_allow_reset(true);

  {
    base::RunLoop loop;
    device->Reset(base::Bind(&ExpectResultAndThen, true, loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_CALL(mock_handle(), ResetDevice(_));

  set_allow_reset(false);

  {
    base::RunLoop loop;
    device->Reset(base::Bind(&ExpectResultAndThen, false, loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_CALL(mock_handle(), Close());
}

TEST_F(USBDeviceImplTest, ClaimAndReleaseInterface) {
  DevicePtr device = GetMockDeviceProxy();

  EXPECT_CALL(mock_device(), Open(_));

  {
    base::RunLoop loop;
    device->Open(base::Bind(&ExpectOpenAndThen, OpenDeviceError::OK,
                            loop.QuitClosure()));
    loop.Run();
  }

  // Now add a mock interface #1.
  AddMockConfig(ConfigBuilder(1).AddInterface(1, 0, 1, 2, 3));

  EXPECT_CALL(mock_handle(), SetConfiguration(1, _));
  EXPECT_CALL(permission_provider(), HasConfigurationPermission(1, _));

  {
    base::RunLoop loop;
    device->SetConfiguration(
        1, base::Bind(&ExpectResultAndThen, true, loop.QuitClosure()));
    loop.Run();
  }

  {
    // Try to claim an invalid interface and expect failure.
    base::RunLoop loop;
    device->ClaimInterface(
        2, base::Bind(&ExpectResultAndThen, false, loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_CALL(mock_handle(), ClaimInterface(1, _));
  EXPECT_CALL(permission_provider(), HasFunctionPermission(1, 1, _));

  {
    base::RunLoop loop;
    device->ClaimInterface(
        1, base::Bind(&ExpectResultAndThen, true, loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_CALL(mock_handle(), ReleaseInterface(2, _));

  {
    // Releasing a non-existent interface should fail.
    base::RunLoop loop;
    device->ReleaseInterface(
        2, base::Bind(&ExpectResultAndThen, false, loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_CALL(mock_handle(), ReleaseInterface(1, _));

  {
    // Now this should release the claimed interface and close the handle.
    base::RunLoop loop;
    device->ReleaseInterface(
        1, base::Bind(&ExpectResultAndThen, true, loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_CALL(mock_handle(), Close());
}

TEST_F(USBDeviceImplTest, SetInterfaceAlternateSetting) {
  DevicePtr device = GetMockDeviceProxy();

  EXPECT_CALL(mock_device(), Open(_));

  {
    base::RunLoop loop;
    device->Open(base::Bind(&ExpectOpenAndThen, OpenDeviceError::OK,
                            loop.QuitClosure()));
    loop.Run();
  }

  AddMockConfig(ConfigBuilder(1)
                    .AddInterface(1, 0, 1, 2, 3)
                    .AddInterface(1, 42, 1, 2, 3)
                    .AddInterface(2, 0, 1, 2, 3));

  EXPECT_CALL(mock_handle(), SetInterfaceAlternateSetting(1, 42, _));

  {
    base::RunLoop loop;
    device->SetInterfaceAlternateSetting(
        1, 42, base::Bind(&ExpectResultAndThen, true, loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_CALL(mock_handle(), SetInterfaceAlternateSetting(1, 100, _));

  {
    base::RunLoop loop;
    device->SetInterfaceAlternateSetting(
        1, 100, base::Bind(&ExpectResultAndThen, false, loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_CALL(mock_handle(), Close());
}

TEST_F(USBDeviceImplTest, ControlTransfer) {
  DevicePtr device = GetMockDeviceProxy();

  EXPECT_CALL(mock_device(), Open(_));

  {
    base::RunLoop loop;
    device->Open(base::Bind(&ExpectOpenAndThen, OpenDeviceError::OK,
                            loop.QuitClosure()));
    loop.Run();
  }

  AddMockConfig(ConfigBuilder(1).AddInterface(7, 0, 1, 2, 3));

  EXPECT_CALL(mock_handle(), SetConfiguration(1, _));
  EXPECT_CALL(permission_provider(), HasConfigurationPermission(1, _));

  {
    base::RunLoop loop;
    device->SetConfiguration(
        1, base::Bind(&ExpectResultAndThen, true, loop.QuitClosure()));
    loop.Run();
  }

  std::vector<uint8_t> fake_data;
  fake_data.push_back(41);
  fake_data.push_back(42);
  fake_data.push_back(43);

  AddMockInboundData(fake_data);

  EXPECT_CALL(mock_handle(),
              ControlTransfer(USB_DIRECTION_INBOUND, UsbDeviceHandle::STANDARD,
                              UsbDeviceHandle::DEVICE, 5, 6, 7, _, _, 0, _));
  EXPECT_CALL(permission_provider(), HasConfigurationPermission(1, _));

  {
    auto params = ControlTransferParams::New();
    params->type = ControlTransferType::STANDARD;
    params->recipient = ControlTransferRecipient::DEVICE;
    params->request = 5;
    params->value = 6;
    params->index = 7;
    base::RunLoop loop;
    device->ControlTransferIn(
        std::move(params), static_cast<uint32_t>(fake_data.size()), 0,
        base::Bind(&ExpectTransferInAndThen, TransferStatus::COMPLETED,
                   fake_data, loop.QuitClosure()));
    loop.Run();
  }

  AddMockOutboundData(fake_data);

  EXPECT_CALL(mock_handle(),
              ControlTransfer(USB_DIRECTION_OUTBOUND, UsbDeviceHandle::STANDARD,
                              UsbDeviceHandle::INTERFACE, 5, 6, 7, _, _, 0, _));
  EXPECT_CALL(permission_provider(), HasFunctionPermission(7, 1, _));

  {
    auto params = ControlTransferParams::New();
    params->type = ControlTransferType::STANDARD;
    params->recipient = ControlTransferRecipient::INTERFACE;
    params->request = 5;
    params->value = 6;
    params->index = 7;
    base::RunLoop loop;
    device->ControlTransferOut(
        std::move(params), mojo::Array<uint8_t>::From(fake_data), 0,
        base::Bind(&ExpectTransferStatusAndThen, TransferStatus::COMPLETED,
                   loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_CALL(mock_handle(), Close());
}

TEST_F(USBDeviceImplTest, GenericTransfer) {
  DevicePtr device = GetMockDeviceProxy();

  EXPECT_CALL(mock_device(), Open(_));

  {
    base::RunLoop loop;
    device->Open(base::Bind(&ExpectOpenAndThen, OpenDeviceError::OK,
                            loop.QuitClosure()));
    loop.Run();
  }

  std::string message1 = "say hello please";
  std::vector<uint8_t> fake_outbound_data(message1.size());
  std::copy(message1.begin(), message1.end(), fake_outbound_data.begin());

  std::string message2 = "hello world!";
  std::vector<uint8_t> fake_inbound_data(message2.size());
  std::copy(message2.begin(), message2.end(), fake_inbound_data.begin());

  AddMockConfig(ConfigBuilder(1).AddInterface(7, 0, 1, 2, 3));
  AddMockOutboundData(fake_outbound_data);
  AddMockInboundData(fake_inbound_data);

  EXPECT_CALL(mock_handle(), GenericTransfer(USB_DIRECTION_OUTBOUND, 0x01, _,
                                             fake_outbound_data.size(), 0, _));

  {
    base::RunLoop loop;
    device->GenericTransferOut(
        1, mojo::Array<uint8_t>::From(fake_outbound_data), 0,
        base::Bind(&ExpectTransferStatusAndThen, TransferStatus::COMPLETED,
                   loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_CALL(mock_handle(), GenericTransfer(USB_DIRECTION_INBOUND, 0x81, _,
                                             fake_inbound_data.size(), 0, _));

  {
    base::RunLoop loop;
    device->GenericTransferIn(
        1, static_cast<uint32_t>(fake_inbound_data.size()), 0,
        base::Bind(&ExpectTransferInAndThen, TransferStatus::COMPLETED,
                   fake_inbound_data, loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_CALL(mock_handle(), Close());
}

TEST_F(USBDeviceImplTest, IsochronousTransfer) {
  DevicePtr device = GetMockDeviceProxy();

  EXPECT_CALL(mock_device(), Open(_));

  {
    base::RunLoop loop;
    device->Open(base::Bind(&ExpectOpenAndThen, OpenDeviceError::OK,
                            loop.QuitClosure()));
    loop.Run();
  }

  std::vector<UsbDeviceHandle::IsochronousPacket> fake_packets(4);
  for (size_t i = 0; i < fake_packets.size(); ++i) {
    fake_packets[i].length = 8;
    fake_packets[i].transferred_length = 8;
    fake_packets[i].status = USB_TRANSFER_COMPLETED;
  }
  std::vector<uint32_t> fake_packet_lengths(4, 8);

  std::vector<uint32_t> expected_transferred_lengths(4, 8);

  std::string outbound_data = "aaaaaaaabbbbbbbbccccccccdddddddd";
  std::vector<uint8_t> fake_outbound_data(outbound_data.size());
  std::copy(outbound_data.begin(), outbound_data.end(),
            fake_outbound_data.begin());

  std::string inbound_data = "ddddddddccccccccbbbbbbbbaaaaaaaa";
  std::vector<uint8_t> fake_inbound_data(inbound_data.size());
  std::copy(inbound_data.begin(), inbound_data.end(),
            fake_inbound_data.begin());

  AddMockConfig(ConfigBuilder(1).AddInterface(7, 0, 1, 2, 3));
  AddMockOutboundPackets(fake_outbound_data, fake_packets);
  AddMockInboundPackets(fake_inbound_data, fake_packets);

  EXPECT_CALL(mock_handle(),
              IsochronousTransferOut(0x01, _, fake_packet_lengths, 0, _));

  {
    base::RunLoop loop;
    device->IsochronousTransferOut(
        1, mojo::Array<uint8_t>::From(fake_outbound_data),
        mojo::Array<uint32_t>::From(fake_packet_lengths), 0,
        base::Bind(&ExpectPacketsOutAndThen, expected_transferred_lengths,
                   loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_CALL(mock_handle(),
              IsochronousTransferIn(0x81, fake_packet_lengths, 0, _));

  {
    base::RunLoop loop;
    device->IsochronousTransferIn(
        1, mojo::Array<uint32_t>::From(fake_packet_lengths), 0,
        base::Bind(&ExpectPacketsInAndThen, fake_inbound_data,
                   expected_transferred_lengths, loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_CALL(mock_handle(), Close());
}

}  // namespace usb
}  // namespace device
