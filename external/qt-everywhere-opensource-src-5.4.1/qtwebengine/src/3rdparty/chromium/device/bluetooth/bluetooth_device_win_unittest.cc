// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/test_simple_task_runner.h"
#include "device/bluetooth/bluetooth_device_win.h"
#include "device/bluetooth/bluetooth_service_record_win.h"
#include "device/bluetooth/bluetooth_socket_thread.h"
#include "device/bluetooth/bluetooth_task_manager_win.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kDeviceName[] = "Device";
const char kDeviceAddress[] = "device address";

const char kTestAudioSdpName[] = "Audio";
const char kTestAudioSdpAddress[] = "01:02:03:0A:10:A0";
const char kTestAudioSdpBytes[] =
    "35510900000a00010001090001350319110a09000435103506190100090019350619001909"
    "010209000535031910020900093508350619110d090102090100250c417564696f20536f75"
    "726365090311090001";
const device::BluetoothUUID kTestAudioSdpUuid("110a");

const char kTestVideoSdpName[] = "Video";
const char kTestVideoSdpAddress[] = "A0:10:0A:03:02:01";
const char kTestVideoSdpBytes[] =
    "354b0900000a000100030900013506191112191203090004350c3503190100350519000308"
    "0b090005350319100209000935083506191108090100090100250d566f6963652047617465"
    "776179";
const device::BluetoothUUID kTestVideoSdpUuid("1112");

}  // namespace

namespace device {

class BluetoothDeviceWinTest : public testing::Test {
 public:
  BluetoothDeviceWinTest() {
    BluetoothTaskManagerWin::DeviceState device_state;
    device_state.name = kDeviceName;
    device_state.address = kDeviceAddress;

    // Add device with audio/video services.
    BluetoothTaskManagerWin::ServiceRecordState* audio_state =
        new BluetoothTaskManagerWin::ServiceRecordState();
    audio_state->name = kTestAudioSdpName;
    audio_state->address = kTestAudioSdpAddress;
    base::HexStringToBytes(kTestAudioSdpBytes, &audio_state->sdp_bytes);
    device_state.service_record_states.push_back(audio_state);

    BluetoothTaskManagerWin::ServiceRecordState* video_state =
        new BluetoothTaskManagerWin::ServiceRecordState();
    video_state->name = kTestVideoSdpName;
    video_state->address = kTestVideoSdpAddress;
    base::HexStringToBytes(kTestVideoSdpBytes, &video_state->sdp_bytes);
    device_state.service_record_states.push_back(video_state);

    scoped_refptr<base::SequencedTaskRunner> ui_task_runner(
        new base::TestSimpleTaskRunner());
    scoped_refptr<BluetoothSocketThread> socket_thread(
        BluetoothSocketThread::Get());
    device_.reset(new BluetoothDeviceWin(device_state,
                                         ui_task_runner,
                                         socket_thread,
                                         NULL,
                                         net::NetLog::Source()));

    // Add empty device.
    device_state.service_record_states.clear();
    empty_device_.reset(new BluetoothDeviceWin(device_state,
                                               ui_task_runner,
                                               socket_thread,
                                               NULL,
                                               net::NetLog::Source()));
  }

 protected:
  scoped_ptr<BluetoothDevice> device_;
  scoped_ptr<BluetoothDevice> empty_device_;
};

TEST_F(BluetoothDeviceWinTest, GetUUIDs) {
  BluetoothDevice::UUIDList uuids = device_->GetUUIDs();

  EXPECT_EQ(2, uuids.size());
  EXPECT_EQ(kTestAudioSdpUuid, uuids[0]);
  EXPECT_EQ(kTestVideoSdpUuid, uuids[1]);

  uuids = empty_device_->GetUUIDs();
  EXPECT_EQ(0, uuids.size());
}

}  // namespace device
