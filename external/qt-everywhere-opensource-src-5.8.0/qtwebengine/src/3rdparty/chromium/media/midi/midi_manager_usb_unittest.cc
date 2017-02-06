// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/midi_manager_usb.h"

#include <stddef.h>
#include <stdint.h>
#include <memory>
#include <string>
#include <utility>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "media/midi/usb_midi_device.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace midi {

namespace {

template<typename T, size_t N>
std::vector<T> ToVector(const T (&array)[N]) {
  return std::vector<T>(array, array + N);
}

class Logger {
 public:
  Logger() {}
  ~Logger() {}

  void AddLog(const std::string& message) { log_ += message; }
  std::string TakeLog() {
    std::string result;
    result.swap(log_);
    return result;
  }

 private:
  std::string log_;

  DISALLOW_COPY_AND_ASSIGN(Logger);
};

class FakeUsbMidiDevice : public UsbMidiDevice {
 public:
  explicit FakeUsbMidiDevice(Logger* logger) : logger_(logger) {}
  ~FakeUsbMidiDevice() override {}

  std::vector<uint8_t> GetDescriptors() override {
    logger_->AddLog("UsbMidiDevice::GetDescriptors\n");
    return descriptors_;
  }

  std::string GetManufacturer() override { return manufacturer_; }
  std::string GetProductName() override { return product_name_; }
  std::string GetDeviceVersion() override { return device_version_; }

  void Send(int endpoint_number, const std::vector<uint8_t>& data) override {
    logger_->AddLog("UsbMidiDevice::Send ");
    logger_->AddLog(base::StringPrintf("endpoint = %d data =",
                                       endpoint_number));
    for (size_t i = 0; i < data.size(); ++i)
      logger_->AddLog(base::StringPrintf(" 0x%02x", data[i]));
    logger_->AddLog("\n");
  }

  void SetDescriptors(const std::vector<uint8_t> descriptors) {
    descriptors_ = descriptors;
  }
  void SetManufacturer(const std::string& manufacturer) {
    manufacturer_ = manufacturer;
  }
  void SetProductName(const std::string& product_name) {
    product_name_ = product_name;
  }
  void SetDeviceVersion(const std::string& device_version) {
    device_version_ = device_version;
  }

 private:
  std::vector<uint8_t> descriptors_;
  std::string manufacturer_;
  std::string product_name_;
  std::string device_version_;
  Logger* logger_;

  DISALLOW_COPY_AND_ASSIGN(FakeUsbMidiDevice);
};

class FakeMidiManagerClient : public MidiManagerClient {
 public:
  explicit FakeMidiManagerClient(Logger* logger)
      : complete_start_session_(false),
        result_(Result::NOT_SUPPORTED),
        logger_(logger) {}
  ~FakeMidiManagerClient() override {}

  void AddInputPort(const MidiPortInfo& info) override {
    input_ports_.push_back(info);
  }

  void AddOutputPort(const MidiPortInfo& info) override {
    output_ports_.push_back(info);
  }

  void SetInputPortState(uint32_t port_index, MidiPortState state) override {}

  void SetOutputPortState(uint32_t port_index, MidiPortState state) override {}

  void CompleteStartSession(Result result) override {
    complete_start_session_ = true;
    result_ = result;
  }

  void ReceiveMidiData(uint32_t port_index,
                       const uint8_t* data,
                       size_t size,
                       double timestamp) override {
    logger_->AddLog("MidiManagerClient::ReceiveMidiData ");
    logger_->AddLog(
        base::StringPrintf("usb:port_index = %d data =", port_index));
    for (size_t i = 0; i < size; ++i)
      logger_->AddLog(base::StringPrintf(" 0x%02x", data[i]));
    logger_->AddLog("\n");
  }

  void AccumulateMidiBytesSent(size_t size) override {
    logger_->AddLog("MidiManagerClient::AccumulateMidiBytesSent ");
    // Windows has no "%zu".
    logger_->AddLog(base::StringPrintf("size = %u\n",
                                       static_cast<unsigned>(size)));
  }

  void Detach() override {}

  bool complete_start_session_;
  Result result_;
  MidiPortInfoList input_ports_;
  MidiPortInfoList output_ports_;

 private:
  Logger* logger_;

  DISALLOW_COPY_AND_ASSIGN(FakeMidiManagerClient);
};

class TestUsbMidiDeviceFactory : public UsbMidiDevice::Factory {
 public:
  TestUsbMidiDeviceFactory() {}
  ~TestUsbMidiDeviceFactory() override {}
  void EnumerateDevices(UsbMidiDeviceDelegate* device,
                        Callback callback) override {
    callback_ = callback;
  }

  Callback callback_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestUsbMidiDeviceFactory);
};

class MidiManagerUsbForTesting : public MidiManagerUsb {
 public:
  explicit MidiManagerUsbForTesting(
      std::unique_ptr<UsbMidiDevice::Factory> device_factory)
      : MidiManagerUsb(std::move(device_factory)) {}
  ~MidiManagerUsbForTesting() override {}

  void CallCompleteInitialization(Result result) {
    CompleteInitialization(result);
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MidiManagerUsbForTesting);
};

class MidiManagerUsbTest : public ::testing::Test {
 public:
  MidiManagerUsbTest() : message_loop_(new base::MessageLoop) {
    std::unique_ptr<TestUsbMidiDeviceFactory> factory(
        new TestUsbMidiDeviceFactory);
    factory_ = factory.get();
    manager_.reset(new MidiManagerUsbForTesting(std::move(factory)));
  }
  ~MidiManagerUsbTest() override {
    manager_->Shutdown();
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();

    std::string leftover_logs = logger_.TakeLog();
    if (!leftover_logs.empty()) {
      ADD_FAILURE() << "Log should be empty: " << leftover_logs;
    }
  }

 protected:
  void Initialize() {
    client_.reset(new FakeMidiManagerClient(&logger_));
    manager_->StartSession(client_.get());
  }

  void Finalize() {
    manager_->EndSession(client_.get());
  }

  bool IsInitializationCallbackInvoked() {
    return client_->complete_start_session_;
  }

  Result GetInitializationResult() { return client_->result_; }

  void RunCallbackUntilCallbackInvoked(
      bool result, UsbMidiDevice::Devices* devices) {
    factory_->callback_.Run(result, devices);
    while (!client_->complete_start_session_) {
      base::RunLoop run_loop;
      run_loop.RunUntilIdle();
    }
  }

  const MidiPortInfoList& input_ports() { return client_->input_ports_; }
  const MidiPortInfoList& output_ports() { return client_->output_ports_; }

  std::unique_ptr<MidiManagerUsbForTesting> manager_;
  std::unique_ptr<FakeMidiManagerClient> client_;
  // Owned by manager_.
  TestUsbMidiDeviceFactory* factory_;
  Logger logger_;

 private:
  std::unique_ptr<base::MessageLoop> message_loop_;

  DISALLOW_COPY_AND_ASSIGN(MidiManagerUsbTest);
};


TEST_F(MidiManagerUsbTest, Initialize) {
  std::unique_ptr<FakeUsbMidiDevice> device(new FakeUsbMidiDevice(&logger_));
  uint8_t descriptors[] = {
      0x12, 0x01, 0x10, 0x01, 0x00, 0x00, 0x00, 0x08, 0x86, 0x1a, 0x2d, 0x75,
      0x54, 0x02, 0x00, 0x02, 0x00, 0x01, 0x09, 0x02, 0x75, 0x00, 0x02, 0x01,
      0x00, 0x80, 0x30, 0x09, 0x04, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00,
      0x09, 0x24, 0x01, 0x00, 0x01, 0x09, 0x00, 0x01, 0x01, 0x09, 0x04, 0x01,
      0x00, 0x02, 0x01, 0x03, 0x00, 0x00, 0x07, 0x24, 0x01, 0x00, 0x01, 0x51,
      0x00, 0x06, 0x24, 0x02, 0x01, 0x02, 0x00, 0x06, 0x24, 0x02, 0x01, 0x03,
      0x00, 0x06, 0x24, 0x02, 0x02, 0x06, 0x00, 0x09, 0x24, 0x03, 0x01, 0x07,
      0x01, 0x06, 0x01, 0x00, 0x09, 0x24, 0x03, 0x02, 0x04, 0x01, 0x02, 0x01,
      0x00, 0x09, 0x24, 0x03, 0x02, 0x05, 0x01, 0x03, 0x01, 0x00, 0x09, 0x05,
      0x02, 0x02, 0x20, 0x00, 0x00, 0x00, 0x00, 0x06, 0x25, 0x01, 0x02, 0x02,
      0x03, 0x09, 0x05, 0x82, 0x02, 0x20, 0x00, 0x00, 0x00, 0x00, 0x05, 0x25,
      0x01, 0x01, 0x07,
  };
  device->SetDescriptors(ToVector(descriptors));
  device->SetManufacturer("vendor1");
  device->SetProductName("device1");
  device->SetDeviceVersion("1.02");

  Initialize();
  ScopedVector<UsbMidiDevice> devices;
  devices.push_back(std::move(device));
  EXPECT_FALSE(IsInitializationCallbackInvoked());
  RunCallbackUntilCallbackInvoked(true, &devices);
  EXPECT_EQ(Result::OK, GetInitializationResult());

  ASSERT_EQ(1u, input_ports().size());
  EXPECT_EQ("usb:port-0-2", input_ports()[0].id);
  EXPECT_EQ("vendor1", input_ports()[0].manufacturer);
  EXPECT_EQ("device1", input_ports()[0].name);
  EXPECT_EQ("1.02", input_ports()[0].version);

  ASSERT_EQ(2u, output_ports().size());
  EXPECT_EQ("usb:port-0-0", output_ports()[0].id);
  EXPECT_EQ("vendor1", output_ports()[0].manufacturer);
  EXPECT_EQ("device1", output_ports()[0].name);
  EXPECT_EQ("1.02", output_ports()[0].version);
  EXPECT_EQ("usb:port-0-1", output_ports()[1].id);
  EXPECT_EQ("vendor1", output_ports()[1].manufacturer);
  EXPECT_EQ("device1", output_ports()[1].name);
  EXPECT_EQ("1.02", output_ports()[1].version);

  ASSERT_TRUE(manager_->input_stream());
  std::vector<UsbMidiJack> jacks = manager_->input_stream()->jacks();
  ASSERT_EQ(2u, manager_->output_streams().size());
  EXPECT_EQ(2u, manager_->output_streams()[0]->jack().jack_id);
  EXPECT_EQ(3u, manager_->output_streams()[1]->jack().jack_id);
  ASSERT_EQ(1u, jacks.size());
  EXPECT_EQ(2, jacks[0].endpoint_number());

  EXPECT_EQ("UsbMidiDevice::GetDescriptors\n", logger_.TakeLog());
}

TEST_F(MidiManagerUsbTest, InitializeMultipleDevices) {
  std::unique_ptr<FakeUsbMidiDevice> device1(new FakeUsbMidiDevice(&logger_));
  std::unique_ptr<FakeUsbMidiDevice> device2(new FakeUsbMidiDevice(&logger_));
  uint8_t descriptors[] = {
      0x12, 0x01, 0x10, 0x01, 0x00, 0x00, 0x00, 0x08, 0x86, 0x1a, 0x2d, 0x75,
      0x54, 0x02, 0x00, 0x02, 0x00, 0x01, 0x09, 0x02, 0x75, 0x00, 0x02, 0x01,
      0x00, 0x80, 0x30, 0x09, 0x04, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00,
      0x09, 0x24, 0x01, 0x00, 0x01, 0x09, 0x00, 0x01, 0x01, 0x09, 0x04, 0x01,
      0x00, 0x02, 0x01, 0x03, 0x00, 0x00, 0x07, 0x24, 0x01, 0x00, 0x01, 0x51,
      0x00, 0x06, 0x24, 0x02, 0x01, 0x02, 0x00, 0x06, 0x24, 0x02, 0x01, 0x03,
      0x00, 0x06, 0x24, 0x02, 0x02, 0x06, 0x00, 0x09, 0x24, 0x03, 0x01, 0x07,
      0x01, 0x06, 0x01, 0x00, 0x09, 0x24, 0x03, 0x02, 0x04, 0x01, 0x02, 0x01,
      0x00, 0x09, 0x24, 0x03, 0x02, 0x05, 0x01, 0x03, 0x01, 0x00, 0x09, 0x05,
      0x02, 0x02, 0x20, 0x00, 0x00, 0x00, 0x00, 0x06, 0x25, 0x01, 0x02, 0x02,
      0x03, 0x09, 0x05, 0x82, 0x02, 0x20, 0x00, 0x00, 0x00, 0x00, 0x05, 0x25,
      0x01, 0x01, 0x07,
  };
  device1->SetDescriptors(ToVector(descriptors));
  device1->SetManufacturer("vendor1");
  device1->SetProductName("device1");
  device1->SetDeviceVersion("1.02");
  device2->SetDescriptors(ToVector(descriptors));
  device2->SetManufacturer("vendor2");
  device2->SetProductName("device2");
  device2->SetDeviceVersion("98.76");

  Initialize();
  ScopedVector<UsbMidiDevice> devices;
  devices.push_back(std::move(device1));
  devices.push_back(std::move(device2));
  EXPECT_FALSE(IsInitializationCallbackInvoked());
  RunCallbackUntilCallbackInvoked(true, &devices);
  EXPECT_EQ(Result::OK, GetInitializationResult());

  ASSERT_EQ(2u, input_ports().size());
  EXPECT_EQ("usb:port-0-2", input_ports()[0].id);
  EXPECT_EQ("vendor1", input_ports()[0].manufacturer);
  EXPECT_EQ("device1", input_ports()[0].name);
  EXPECT_EQ("1.02", input_ports()[0].version);
  EXPECT_EQ("usb:port-1-2", input_ports()[1].id);
  EXPECT_EQ("vendor2", input_ports()[1].manufacturer);
  EXPECT_EQ("device2", input_ports()[1].name);
  EXPECT_EQ("98.76", input_ports()[1].version);

  ASSERT_EQ(4u, output_ports().size());
  EXPECT_EQ("usb:port-0-0", output_ports()[0].id);
  EXPECT_EQ("vendor1", output_ports()[0].manufacturer);
  EXPECT_EQ("device1", output_ports()[0].name);
  EXPECT_EQ("1.02", output_ports()[0].version);
  EXPECT_EQ("usb:port-0-1", output_ports()[1].id);
  EXPECT_EQ("vendor1", output_ports()[1].manufacturer);
  EXPECT_EQ("device1", output_ports()[1].name);
  EXPECT_EQ("1.02", output_ports()[1].version);
  EXPECT_EQ("usb:port-1-0", output_ports()[2].id);
  EXPECT_EQ("vendor2", output_ports()[2].manufacturer);
  EXPECT_EQ("device2", output_ports()[2].name);
  EXPECT_EQ("98.76", output_ports()[2].version);
  EXPECT_EQ("usb:port-1-1", output_ports()[3].id);
  EXPECT_EQ("vendor2", output_ports()[3].manufacturer);
  EXPECT_EQ("device2", output_ports()[3].name);
  EXPECT_EQ("98.76", output_ports()[3].version);

  ASSERT_TRUE(manager_->input_stream());
  std::vector<UsbMidiJack> jacks = manager_->input_stream()->jacks();
  ASSERT_EQ(4u, manager_->output_streams().size());
  EXPECT_EQ(2u, manager_->output_streams()[0]->jack().jack_id);
  EXPECT_EQ(3u, manager_->output_streams()[1]->jack().jack_id);
  ASSERT_EQ(2u, jacks.size());
  EXPECT_EQ(2, jacks[0].endpoint_number());

  EXPECT_EQ(
      "UsbMidiDevice::GetDescriptors\n"
      "UsbMidiDevice::GetDescriptors\n",
      logger_.TakeLog());
}

TEST_F(MidiManagerUsbTest, InitializeFail) {
  Initialize();

  EXPECT_FALSE(IsInitializationCallbackInvoked());
  RunCallbackUntilCallbackInvoked(false, NULL);
  EXPECT_EQ(Result::INITIALIZATION_ERROR, GetInitializationResult());
}

TEST_F(MidiManagerUsbTest, InitializeFailBecauseOfInvalidDescriptors) {
  std::unique_ptr<FakeUsbMidiDevice> device(new FakeUsbMidiDevice(&logger_));
  uint8_t descriptors[] = {0x04};
  device->SetDescriptors(ToVector(descriptors));

  Initialize();
  ScopedVector<UsbMidiDevice> devices;
  devices.push_back(std::move(device));
  EXPECT_FALSE(IsInitializationCallbackInvoked());
  RunCallbackUntilCallbackInvoked(true, &devices);
  EXPECT_EQ(Result::INITIALIZATION_ERROR, GetInitializationResult());
  EXPECT_EQ("UsbMidiDevice::GetDescriptors\n", logger_.TakeLog());
}

TEST_F(MidiManagerUsbTest, Send) {
  Initialize();
  std::unique_ptr<FakeUsbMidiDevice> device(new FakeUsbMidiDevice(&logger_));
  uint8_t descriptors[] = {
      0x12, 0x01, 0x10, 0x01, 0x00, 0x00, 0x00, 0x08, 0x86, 0x1a, 0x2d, 0x75,
      0x54, 0x02, 0x00, 0x02, 0x00, 0x01, 0x09, 0x02, 0x75, 0x00, 0x02, 0x01,
      0x00, 0x80, 0x30, 0x09, 0x04, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00,
      0x09, 0x24, 0x01, 0x00, 0x01, 0x09, 0x00, 0x01, 0x01, 0x09, 0x04, 0x01,
      0x00, 0x02, 0x01, 0x03, 0x00, 0x00, 0x07, 0x24, 0x01, 0x00, 0x01, 0x51,
      0x00, 0x06, 0x24, 0x02, 0x01, 0x02, 0x00, 0x06, 0x24, 0x02, 0x01, 0x03,
      0x00, 0x06, 0x24, 0x02, 0x02, 0x06, 0x00, 0x09, 0x24, 0x03, 0x01, 0x07,
      0x01, 0x06, 0x01, 0x00, 0x09, 0x24, 0x03, 0x02, 0x04, 0x01, 0x02, 0x01,
      0x00, 0x09, 0x24, 0x03, 0x02, 0x05, 0x01, 0x03, 0x01, 0x00, 0x09, 0x05,
      0x02, 0x02, 0x20, 0x00, 0x00, 0x00, 0x00, 0x06, 0x25, 0x01, 0x02, 0x02,
      0x03, 0x09, 0x05, 0x82, 0x02, 0x20, 0x00, 0x00, 0x00, 0x00, 0x05, 0x25,
      0x01, 0x01, 0x07,
  };

  device->SetDescriptors(ToVector(descriptors));
  uint8_t data[] = {
      0x90, 0x45, 0x7f, 0xf0, 0x00, 0x01, 0xf7,
  };

  ScopedVector<UsbMidiDevice> devices;
  devices.push_back(std::move(device));
  EXPECT_FALSE(IsInitializationCallbackInvoked());
  RunCallbackUntilCallbackInvoked(true, &devices);
  EXPECT_EQ(Result::OK, GetInitializationResult());
  ASSERT_EQ(2u, manager_->output_streams().size());

  manager_->DispatchSendMidiData(client_.get(), 1, ToVector(data), 0);
  // Since UsbMidiDevice::Send is posted as a task, RunLoop should run to
  // invoke the task.
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
  EXPECT_EQ("UsbMidiDevice::GetDescriptors\n"
            "UsbMidiDevice::Send endpoint = 2 data = "
            "0x19 0x90 0x45 0x7f "
            "0x14 0xf0 0x00 0x01 "
            "0x15 0xf7 0x00 0x00\n"
            "MidiManagerClient::AccumulateMidiBytesSent size = 7\n",
            logger_.TakeLog());
}

TEST_F(MidiManagerUsbTest, SendFromCompromizedRenderer) {
  std::unique_ptr<FakeUsbMidiDevice> device(new FakeUsbMidiDevice(&logger_));
  uint8_t descriptors[] = {
      0x12, 0x01, 0x10, 0x01, 0x00, 0x00, 0x00, 0x08, 0x86, 0x1a, 0x2d, 0x75,
      0x54, 0x02, 0x00, 0x02, 0x00, 0x01, 0x09, 0x02, 0x75, 0x00, 0x02, 0x01,
      0x00, 0x80, 0x30, 0x09, 0x04, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00,
      0x09, 0x24, 0x01, 0x00, 0x01, 0x09, 0x00, 0x01, 0x01, 0x09, 0x04, 0x01,
      0x00, 0x02, 0x01, 0x03, 0x00, 0x00, 0x07, 0x24, 0x01, 0x00, 0x01, 0x51,
      0x00, 0x06, 0x24, 0x02, 0x01, 0x02, 0x00, 0x06, 0x24, 0x02, 0x01, 0x03,
      0x00, 0x06, 0x24, 0x02, 0x02, 0x06, 0x00, 0x09, 0x24, 0x03, 0x01, 0x07,
      0x01, 0x06, 0x01, 0x00, 0x09, 0x24, 0x03, 0x02, 0x04, 0x01, 0x02, 0x01,
      0x00, 0x09, 0x24, 0x03, 0x02, 0x05, 0x01, 0x03, 0x01, 0x00, 0x09, 0x05,
      0x02, 0x02, 0x20, 0x00, 0x00, 0x00, 0x00, 0x06, 0x25, 0x01, 0x02, 0x02,
      0x03, 0x09, 0x05, 0x82, 0x02, 0x20, 0x00, 0x00, 0x00, 0x00, 0x05, 0x25,
      0x01, 0x01, 0x07,
  };

  device->SetDescriptors(ToVector(descriptors));
  uint8_t data[] = {
      0x90, 0x45, 0x7f, 0xf0, 0x00, 0x01, 0xf7,
  };

  Initialize();
  ScopedVector<UsbMidiDevice> devices;
  devices.push_back(std::move(device));
  EXPECT_FALSE(IsInitializationCallbackInvoked());
  RunCallbackUntilCallbackInvoked(true, &devices);
  EXPECT_EQ(Result::OK, GetInitializationResult());
  ASSERT_EQ(2u, manager_->output_streams().size());
  EXPECT_EQ("UsbMidiDevice::GetDescriptors\n", logger_.TakeLog());

  // The specified port index is invalid. The manager must ignore the request.
  manager_->DispatchSendMidiData(client_.get(), 99, ToVector(data), 0);
  EXPECT_EQ("", logger_.TakeLog());

  // The specified port index is invalid. The manager must ignore the request.
  manager_->DispatchSendMidiData(client_.get(), 2, ToVector(data), 0);
  EXPECT_EQ("", logger_.TakeLog());
}

TEST_F(MidiManagerUsbTest, Receive) {
  std::unique_ptr<FakeUsbMidiDevice> device(new FakeUsbMidiDevice(&logger_));
  uint8_t descriptors[] = {
      0x12, 0x01, 0x10, 0x01, 0x00, 0x00, 0x00, 0x08, 0x86, 0x1a, 0x2d, 0x75,
      0x54, 0x02, 0x00, 0x02, 0x00, 0x01, 0x09, 0x02, 0x75, 0x00, 0x02, 0x01,
      0x00, 0x80, 0x30, 0x09, 0x04, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00,
      0x09, 0x24, 0x01, 0x00, 0x01, 0x09, 0x00, 0x01, 0x01, 0x09, 0x04, 0x01,
      0x00, 0x02, 0x01, 0x03, 0x00, 0x00, 0x07, 0x24, 0x01, 0x00, 0x01, 0x51,
      0x00, 0x06, 0x24, 0x02, 0x01, 0x02, 0x00, 0x06, 0x24, 0x02, 0x01, 0x03,
      0x00, 0x06, 0x24, 0x02, 0x02, 0x06, 0x00, 0x09, 0x24, 0x03, 0x01, 0x07,
      0x01, 0x06, 0x01, 0x00, 0x09, 0x24, 0x03, 0x02, 0x04, 0x01, 0x02, 0x01,
      0x00, 0x09, 0x24, 0x03, 0x02, 0x05, 0x01, 0x03, 0x01, 0x00, 0x09, 0x05,
      0x02, 0x02, 0x20, 0x00, 0x00, 0x00, 0x00, 0x06, 0x25, 0x01, 0x02, 0x02,
      0x03, 0x09, 0x05, 0x82, 0x02, 0x20, 0x00, 0x00, 0x00, 0x00, 0x05, 0x25,
      0x01, 0x01, 0x07,
  };

  device->SetDescriptors(ToVector(descriptors));
  uint8_t data[] = {
      0x09, 0x90, 0x45, 0x7f, 0x04, 0xf0, 0x00,
      0x01, 0x49, 0x90, 0x88, 0x99,  // This data should be ignored (CN = 4).
      0x05, 0xf7, 0x00, 0x00,
  };

  Initialize();
  ScopedVector<UsbMidiDevice> devices;
  UsbMidiDevice* device_raw = device.get();
  devices.push_back(std::move(device));
  EXPECT_FALSE(IsInitializationCallbackInvoked());
  RunCallbackUntilCallbackInvoked(true, &devices);
  EXPECT_EQ(Result::OK, GetInitializationResult());

  manager_->ReceiveUsbMidiData(device_raw, 2, data, arraysize(data),
                               base::TimeTicks());
  Finalize();

  EXPECT_EQ(
      "UsbMidiDevice::GetDescriptors\n"
      "MidiManagerClient::ReceiveMidiData usb:port_index = 0 "
      "data = 0x90 0x45 0x7f\n"
      "MidiManagerClient::ReceiveMidiData usb:port_index = 0 "
      "data = 0xf0 0x00 0x01\n"
      "MidiManagerClient::ReceiveMidiData usb:port_index = 0 data = 0xf7\n",
      logger_.TakeLog());
}

TEST_F(MidiManagerUsbTest, AttachDevice) {
  uint8_t descriptors[] = {
      0x12, 0x01, 0x10, 0x01, 0x00, 0x00, 0x00, 0x08, 0x86, 0x1a, 0x2d, 0x75,
      0x54, 0x02, 0x00, 0x02, 0x00, 0x01, 0x09, 0x02, 0x75, 0x00, 0x02, 0x01,
      0x00, 0x80, 0x30, 0x09, 0x04, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00,
      0x09, 0x24, 0x01, 0x00, 0x01, 0x09, 0x00, 0x01, 0x01, 0x09, 0x04, 0x01,
      0x00, 0x02, 0x01, 0x03, 0x00, 0x00, 0x07, 0x24, 0x01, 0x00, 0x01, 0x51,
      0x00, 0x06, 0x24, 0x02, 0x01, 0x02, 0x00, 0x06, 0x24, 0x02, 0x01, 0x03,
      0x00, 0x06, 0x24, 0x02, 0x02, 0x06, 0x00, 0x09, 0x24, 0x03, 0x01, 0x07,
      0x01, 0x06, 0x01, 0x00, 0x09, 0x24, 0x03, 0x02, 0x04, 0x01, 0x02, 0x01,
      0x00, 0x09, 0x24, 0x03, 0x02, 0x05, 0x01, 0x03, 0x01, 0x00, 0x09, 0x05,
      0x02, 0x02, 0x20, 0x00, 0x00, 0x00, 0x00, 0x06, 0x25, 0x01, 0x02, 0x02,
      0x03, 0x09, 0x05, 0x82, 0x02, 0x20, 0x00, 0x00, 0x00, 0x00, 0x05, 0x25,
      0x01, 0x01, 0x07,
  };

  Initialize();
  ScopedVector<UsbMidiDevice> devices;
  EXPECT_FALSE(IsInitializationCallbackInvoked());
  RunCallbackUntilCallbackInvoked(true, &devices);
  EXPECT_EQ(Result::OK, GetInitializationResult());

  ASSERT_EQ(0u, input_ports().size());
  ASSERT_EQ(0u, output_ports().size());
  ASSERT_TRUE(manager_->input_stream());
  std::vector<UsbMidiJack> jacks = manager_->input_stream()->jacks();
  ASSERT_EQ(0u, manager_->output_streams().size());
  ASSERT_EQ(0u, jacks.size());
  EXPECT_EQ("", logger_.TakeLog());

  std::unique_ptr<FakeUsbMidiDevice> new_device(
      new FakeUsbMidiDevice(&logger_));
  new_device->SetDescriptors(ToVector(descriptors));
  manager_->OnDeviceAttached(std::move(new_device));

  ASSERT_EQ(1u, input_ports().size());
  ASSERT_EQ(2u, output_ports().size());
  ASSERT_TRUE(manager_->input_stream());
  jacks = manager_->input_stream()->jacks();
  ASSERT_EQ(2u, manager_->output_streams().size());
  EXPECT_EQ(2u, manager_->output_streams()[0]->jack().jack_id);
  EXPECT_EQ(3u, manager_->output_streams()[1]->jack().jack_id);
  ASSERT_EQ(1u, jacks.size());
  EXPECT_EQ(2, jacks[0].endpoint_number());
  EXPECT_EQ("UsbMidiDevice::GetDescriptors\n", logger_.TakeLog());
}

}  // namespace

}  // namespace midi
}  // namespace media
