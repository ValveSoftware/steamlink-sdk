// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_service_record_win.h"

#include <string>

#include "base/basictypes.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "device/bluetooth/bluetooth_init_win.h"
#include "device/bluetooth/bluetooth_uuid.h"

namespace {

const uint16 kProtocolDescriptorListId = 4;
const uint16 kRfcommUuid = 3;
const uint16 kUuidId = 1;

bool AdvanceToSdpType(const SDP_ELEMENT_DATA& sequence_data,
                      SDP_TYPE type,
                      HBLUETOOTH_CONTAINER_ELEMENT* element,
                      SDP_ELEMENT_DATA* sdp_data) {
  while (ERROR_SUCCESS == BluetoothSdpGetContainerElementData(
      sequence_data.data.sequence.value,
      sequence_data.data.sequence.length,
      element,
      sdp_data)) {
    if (sdp_data->type == type) {
      return true;
    }
  }
  return false;
}

void ExtractChannels(const SDP_ELEMENT_DATA& protocol_descriptor_list_data,
                     bool* supports_rfcomm,
                     uint8* rfcomm_channel) {
  HBLUETOOTH_CONTAINER_ELEMENT sequence_element = NULL;
  SDP_ELEMENT_DATA sequence_data;
  while (AdvanceToSdpType(protocol_descriptor_list_data,
                          SDP_TYPE_SEQUENCE,
                          &sequence_element,
                          &sequence_data)) {
    HBLUETOOTH_CONTAINER_ELEMENT inner_sequence_element = NULL;
    SDP_ELEMENT_DATA inner_sequence_data;
    if (AdvanceToSdpType(sequence_data,
                         SDP_TYPE_UUID,
                         &inner_sequence_element,
                         &inner_sequence_data) &&
        inner_sequence_data.data.uuid32 == kRfcommUuid &&
        AdvanceToSdpType(sequence_data,
                         SDP_TYPE_UINT,
                         &inner_sequence_element,
                         &inner_sequence_data) &&
        inner_sequence_data.specificType == SDP_ST_UINT8) {
      *rfcomm_channel = inner_sequence_data.data.uint8;
      *supports_rfcomm = true;
    }
  }
}

void ExtractUuid(const SDP_ELEMENT_DATA& uuid_data,
                 device::BluetoothUUID* uuid) {
  HBLUETOOTH_CONTAINER_ELEMENT inner_uuid_element = NULL;
  SDP_ELEMENT_DATA inner_uuid_data;
  if (AdvanceToSdpType(uuid_data,
                       SDP_TYPE_UUID,
                       &inner_uuid_element,
                       &inner_uuid_data)) {
    if (inner_uuid_data.specificType == SDP_ST_UUID16) {
      std::string uuid_hex =
          base::StringPrintf("%04x", inner_uuid_data.data.uuid16);
      *uuid = device::BluetoothUUID(uuid_hex);
    } else if (inner_uuid_data.specificType == SDP_ST_UUID32) {
      std::string uuid_hex =
          base::StringPrintf("%08x", inner_uuid_data.data.uuid32);
      *uuid = device::BluetoothUUID(uuid_hex);
    } else if (inner_uuid_data.specificType == SDP_ST_UUID128) {
      *uuid = device::BluetoothUUID(base::StringPrintf(
          "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
          inner_uuid_data.data.uuid128.Data1,
          inner_uuid_data.data.uuid128.Data2,
          inner_uuid_data.data.uuid128.Data3,
          inner_uuid_data.data.uuid128.Data4[0],
          inner_uuid_data.data.uuid128.Data4[1],
          inner_uuid_data.data.uuid128.Data4[2],
          inner_uuid_data.data.uuid128.Data4[3],
          inner_uuid_data.data.uuid128.Data4[4],
          inner_uuid_data.data.uuid128.Data4[5],
          inner_uuid_data.data.uuid128.Data4[6],
          inner_uuid_data.data.uuid128.Data4[7]));
    } else {
      *uuid = device::BluetoothUUID();
    }
  }
}

BTH_ADDR ConvertToBthAddr(const std::string& address) {
  BTH_ADDR bth_addr = 0;
  std::string numbers_only;
  for (int i = 0; i < 6; ++i) {
    numbers_only += address.substr(i * 3, 2);
  }

  std::vector<uint8> address_bytes;
  base::HexStringToBytes(numbers_only, &address_bytes);
  int byte_position = 0;
  for (std::vector<uint8>::reverse_iterator iter = address_bytes.rbegin();
      iter != address_bytes.rend();
      ++iter) {
    bth_addr += *iter * pow(256.0, byte_position);
    byte_position++;
  }
  return bth_addr;
}

}  // namespace

namespace device {

BluetoothServiceRecordWin::BluetoothServiceRecordWin(
    const std::string& name,
    const std::string& address,
    uint64 blob_size,
    uint8* blob_data) : bth_addr_(ConvertToBthAddr(address)) {
  name_ = name;
  address_ = address;
  supports_rfcomm_ = false;
  SDP_ELEMENT_DATA protocol_descriptor_list_data;
  if (ERROR_SUCCESS == BluetoothSdpGetAttributeValue(
      blob_data,
      blob_size,
      kProtocolDescriptorListId,
      &protocol_descriptor_list_data)) {
    ExtractChannels(protocol_descriptor_list_data,
                    &supports_rfcomm_,
                    &rfcomm_channel_);
  }
  SDP_ELEMENT_DATA uuid_data;
  if (ERROR_SUCCESS == BluetoothSdpGetAttributeValue(
      blob_data,
      blob_size,
      kUuidId,
      &uuid_data)) {
    ExtractUuid(uuid_data, &uuid_);
  }
}

}  // namespace device
