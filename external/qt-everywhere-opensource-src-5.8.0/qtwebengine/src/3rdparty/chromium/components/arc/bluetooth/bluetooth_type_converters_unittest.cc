// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/bluetooth/bluetooth_type_converters.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/values.h"
#include "device/bluetooth/bluetooth_gatt_service.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "device/bluetooth/bluez/bluetooth_service_attribute_value_bluez.h"
#include "mojo/public/cpp/bindings/array.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kAddressStr[] = "1A:2B:3C:4D:5E:6F";
constexpr char kInvalidAddressStr[] = "00:00:00:00:00:00";
constexpr uint8_t kAddressArray[] = {0x1a, 0x2b, 0x3c, 0x4d, 0x5e, 0x6f};
constexpr size_t kAddressSize = 6;
constexpr char kUuidStr[] = "12345678-1234-5678-9abc-def123456789";
constexpr uint8_t kUuidArray[] = {0x12, 0x34, 0x56, 0x78, 0x12, 0x34,
                                  0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf1,
                                  0x23, 0x45, 0x67, 0x89};
constexpr size_t kUuidSize = 16;
constexpr uint8_t kFillerByte = 0x79;

arc::mojom::BluetoothSdpAttributePtr CreateDeepMojoSequenceAttribute(
    size_t depth) {
  auto value = arc::mojom::BluetoothSdpAttribute::New();

  if (depth > 0u) {
    value->type = bluez::BluetoothServiceAttributeValueBlueZ::SEQUENCE;
    value->value.Clear();
    value->sequence.push_back(CreateDeepMojoSequenceAttribute(depth - 1));
    value->type_size = static_cast<uint32_t>(value->sequence.size());
  } else {
    uint16_t data = 3;
    value->type = bluez::BluetoothServiceAttributeValueBlueZ::UINT;
    value->type_size = static_cast<uint32_t>(sizeof(data));
    value->value.AppendInteger(static_cast<int>(data));
    value->sequence = mojo::Array<arc::mojom::BluetoothSdpAttributePtr>::New(0);
  }
  return value;
}

size_t GetDepthOfMojoAttribute(
    const arc::mojom::BluetoothSdpAttributePtr& attribute) {
  size_t depth = 1;
  if (attribute->type == bluez::BluetoothServiceAttributeValueBlueZ::SEQUENCE) {
    for (const auto& value : attribute->sequence)
      depth = std::max(depth, GetDepthOfMojoAttribute(value) + 1);
  }
  return depth;
}

bluez::BluetoothServiceAttributeValueBlueZ CreateDeepBlueZSequenceAttribute(
    size_t depth) {
  if (depth > 0u) {
    std::unique_ptr<bluez::BluetoothServiceAttributeValueBlueZ::Sequence>
        sequence(new bluez::BluetoothServiceAttributeValueBlueZ::Sequence());
    sequence->push_back(CreateDeepBlueZSequenceAttribute(depth - 1));

    return bluez::BluetoothServiceAttributeValueBlueZ(std::move(sequence));
  } else {
    return bluez::BluetoothServiceAttributeValueBlueZ(
        bluez::BluetoothServiceAttributeValueBlueZ::UINT, sizeof(uint16_t),
        base::WrapUnique(new base::FundamentalValue(3)));
  }
}

size_t GetDepthOfBlueZAttribute(
    const bluez::BluetoothServiceAttributeValueBlueZ& attribute) {
  size_t depth = 1;
  if (attribute.type() ==
      bluez::BluetoothServiceAttributeValueBlueZ::SEQUENCE) {
    for (const auto& value : attribute.sequence())
      depth = std::max(depth, GetDepthOfBlueZAttribute(value) + 1);
  }
  return depth;
}

}  // namespace

namespace mojo {

TEST(BluetoothTypeConvertorTest, ConvertMojoBluetoothAddressFromString) {
  arc::mojom::BluetoothAddressPtr addressMojo =
      arc::mojom::BluetoothAddress::From(std::string(kAddressStr));
  EXPECT_EQ(kAddressSize, addressMojo->address.size());
  for (size_t i = 0; i < kAddressSize; i++) {
    EXPECT_EQ(kAddressArray[i], addressMojo->address[i]);
  }
}

TEST(BluetoothTypeConvertorTest, ConvertMojoBluetoothAddressToString) {
  arc::mojom::BluetoothAddressPtr addressMojo =
      arc::mojom::BluetoothAddress::New();
  for (size_t i = 0; i < kAddressSize - 1; i++) {
    addressMojo->address.push_back(kAddressArray[i]);
  }
  EXPECT_EQ(std::string(kInvalidAddressStr), addressMojo->To<std::string>());

  addressMojo->address.push_back(kAddressArray[kAddressSize - 1]);
  EXPECT_EQ(std::string(kAddressStr), addressMojo->To<std::string>());

  addressMojo->address.push_back(kFillerByte);

  EXPECT_EQ(std::string(kInvalidAddressStr), addressMojo->To<std::string>());
}

TEST(BluetoothTypeConvertorTest,
     ConvertMojoBluetoothUUIDFromDeviceBluetoothUUID) {
  device::BluetoothUUID uuidDevice((std::string(kUuidStr)));
  arc::mojom::BluetoothUUIDPtr uuidMojo =
      arc::mojom::BluetoothUUID::From(uuidDevice);
  EXPECT_EQ(kUuidSize, uuidMojo->uuid.size());
  for (size_t i = 0; i < kUuidSize; i++) {
    EXPECT_EQ(kUuidArray[i], uuidMojo->uuid[i]);
  }
}

TEST(BluetoothTypeConvertorTest,
     ConvertMojoBluetoothUUIDToDeviceBluetoothUUID) {
  arc::mojom::BluetoothUUIDPtr uuidMojo = arc::mojom::BluetoothUUID::New();
  for (size_t i = 0; i < kUuidSize - 1; i++) {
    uuidMojo->uuid.push_back(kUuidArray[i]);
  }
  EXPECT_FALSE(uuidMojo.To<device::BluetoothUUID>().IsValid());

  uuidMojo->uuid.push_back(kUuidArray[kUuidSize - 1]);
  EXPECT_TRUE(uuidMojo.To<device::BluetoothUUID>().IsValid());
  EXPECT_EQ(std::string(kUuidStr),
            uuidMojo.To<device::BluetoothUUID>().canonical_value());

  uuidMojo->uuid.push_back(kFillerByte);
  EXPECT_FALSE(uuidMojo.To<device::BluetoothUUID>().IsValid());
}

TEST(BluetoothTypeConvertorTest, ConvertMojoValueAttributeToBlueZAttribute) {
  // Construct Mojo attribute with NULLTYPE value.
  auto nulltypeAttributeMojo = arc::mojom::BluetoothSdpAttribute::New();
  nulltypeAttributeMojo->type =
      bluez::BluetoothServiceAttributeValueBlueZ::NULLTYPE;
  nulltypeAttributeMojo->type_size = 0;
  nulltypeAttributeMojo->value.Append(base::Value::CreateNullValue());
  nulltypeAttributeMojo->sequence =
      mojo::Array<arc::mojom::BluetoothSdpAttributePtr>::New(0);

  auto nulltypeAttributeBlueZ =
      nulltypeAttributeMojo.To<bluez::BluetoothServiceAttributeValueBlueZ>();

  EXPECT_EQ(bluez::BluetoothServiceAttributeValueBlueZ::NULLTYPE,
            nulltypeAttributeBlueZ.type());
  EXPECT_EQ(0u, nulltypeAttributeBlueZ.size());
  EXPECT_EQ(base::Value::TYPE_NULL, nulltypeAttributeBlueZ.value().GetType());

  // Construct Mojo attribute with TYPE_BOOLEAN value.
  bool valueBool = true;
  auto boolAttributeMojo = arc::mojom::BluetoothSdpAttribute::New();
  boolAttributeMojo->type = bluez::BluetoothServiceAttributeValueBlueZ::BOOL;
  boolAttributeMojo->type_size = static_cast<uint32_t>(sizeof(valueBool));
  boolAttributeMojo->value.AppendBoolean(valueBool);
  boolAttributeMojo->sequence =
      mojo::Array<arc::mojom::BluetoothSdpAttributePtr>::New(0);

  auto boolAttributeBlueZ =
      boolAttributeMojo.To<bluez::BluetoothServiceAttributeValueBlueZ>();

  EXPECT_EQ(bluez::BluetoothServiceAttributeValueBlueZ::BOOL,
            boolAttributeBlueZ.type());
  EXPECT_EQ(sizeof(valueBool), boolAttributeBlueZ.size());
  EXPECT_EQ(base::Value::TYPE_BOOLEAN, boolAttributeBlueZ.value().GetType());
  EXPECT_TRUE(boolAttributeBlueZ.value().GetAsBoolean(&valueBool));
  EXPECT_TRUE(valueBool);

  // Construct Mojo attribute with TYPE_UINT value.
  uint16_t valueUint16 = 10;
  int valueUint16AsInt = 0;
  auto uintAttributeMojo = arc::mojom::BluetoothSdpAttribute::New();
  uintAttributeMojo->type = bluez::BluetoothServiceAttributeValueBlueZ::UINT;
  uintAttributeMojo->type_size = static_cast<uint32_t>(sizeof(valueUint16));
  uintAttributeMojo->value.AppendInteger(static_cast<int>(valueUint16));
  uintAttributeMojo->sequence =
      mojo::Array<arc::mojom::BluetoothSdpAttributePtr>::New(0);

  auto uintAttributeBlueZ =
      uintAttributeMojo.To<bluez::BluetoothServiceAttributeValueBlueZ>();

  EXPECT_EQ(bluez::BluetoothServiceAttributeValueBlueZ::UINT,
            uintAttributeBlueZ.type());
  EXPECT_EQ(sizeof(valueUint16), uintAttributeBlueZ.size());
  EXPECT_EQ(base::Value::TYPE_INTEGER, uintAttributeBlueZ.value().GetType());
  EXPECT_TRUE(uintAttributeBlueZ.value().GetAsInteger(&valueUint16AsInt));
  EXPECT_EQ(valueUint16, static_cast<uint16_t>(valueUint16AsInt));

  // Construct Mojo attribute with TYPE_INT value.
  int16_t valueInt16 = 20;
  int valueInt16AsInt = 0;
  auto intAttributeMojo = arc::mojom::BluetoothSdpAttribute::New();
  intAttributeMojo->type = bluez::BluetoothServiceAttributeValueBlueZ::INT;
  intAttributeMojo->type_size = static_cast<uint32_t>(sizeof(valueInt16));
  intAttributeMojo->value.AppendInteger(static_cast<int>(valueInt16));
  intAttributeMojo->sequence =
      mojo::Array<arc::mojom::BluetoothSdpAttributePtr>::New(0);

  auto intAttributeBlueZ =
      intAttributeMojo.To<bluez::BluetoothServiceAttributeValueBlueZ>();

  EXPECT_EQ(bluez::BluetoothServiceAttributeValueBlueZ::INT,
            intAttributeBlueZ.type());
  EXPECT_EQ(sizeof(valueInt16), intAttributeBlueZ.size());
  EXPECT_EQ(base::Value::TYPE_INTEGER, intAttributeBlueZ.value().GetType());
  EXPECT_TRUE(intAttributeBlueZ.value().GetAsInteger(&valueInt16AsInt));
  EXPECT_EQ(valueInt16, static_cast<int16_t>(valueInt16AsInt));

  // Construct Mojo attribute with TYPE_UUID.
  std::string expectedUUID("00000000-0000-1000-8000-00805f9b34fb");
  std::string actualUUID;
  auto uuidAttributeMojo = arc::mojom::BluetoothSdpAttribute::New();
  uuidAttributeMojo->type = bluez::BluetoothServiceAttributeValueBlueZ::UUID;
  // UUIDs are all stored in string form, but it can be converted to one of
  // UUID16, UUID32 and UUID128.
  uuidAttributeMojo->type_size = static_cast<uint32_t>(sizeof(uint16_t));
  uuidAttributeMojo->value.AppendString(expectedUUID);
  uuidAttributeMojo->sequence =
      mojo::Array<arc::mojom::BluetoothSdpAttributePtr>::New(0);

  auto uuidAttributeBlueZ =
      uuidAttributeMojo.To<bluez::BluetoothServiceAttributeValueBlueZ>();

  EXPECT_EQ(bluez::BluetoothServiceAttributeValueBlueZ::UUID,
            uuidAttributeBlueZ.type());
  EXPECT_EQ(sizeof(uint16_t), uuidAttributeBlueZ.size());
  EXPECT_EQ(base::Value::TYPE_STRING, uuidAttributeBlueZ.value().GetType());
  EXPECT_TRUE(uuidAttributeBlueZ.value().GetAsString(&actualUUID));
  EXPECT_EQ(expectedUUID, actualUUID);

  // Construct Mojo attribute with TYPE_STRING. TYPE_URL is the same case as
  // TYPE_STRING.
  std::string expectedString("Some SDP service");
  std::string actualString;
  auto stringAttributeMojo = arc::mojom::BluetoothSdpAttribute::New();
  stringAttributeMojo->type =
      bluez::BluetoothServiceAttributeValueBlueZ::STRING;
  stringAttributeMojo->type_size =
      static_cast<uint32_t>(expectedString.length());
  stringAttributeMojo->value.AppendString(expectedString);
  stringAttributeMojo->sequence =
      mojo::Array<arc::mojom::BluetoothSdpAttributePtr>::New(0);

  auto stringAttributeBlueZ =
      stringAttributeMojo.To<bluez::BluetoothServiceAttributeValueBlueZ>();

  EXPECT_EQ(bluez::BluetoothServiceAttributeValueBlueZ::STRING,
            stringAttributeBlueZ.type());
  EXPECT_EQ(expectedString.length(), stringAttributeBlueZ.size());
  EXPECT_EQ(base::Value::TYPE_STRING, stringAttributeBlueZ.value().GetType());
  EXPECT_TRUE(stringAttributeBlueZ.value().GetAsString(&actualString));
  EXPECT_EQ(expectedString, actualString);
}

TEST(BluetoothTypeConvertorTest, ConvertMojoSequenceAttributeToBlueZAttribute) {
  // Create an UUID value.
  std::string l2capUUID("00000100-0000-1000-8000-00805f9b34fb");
  auto valueUUID = arc::mojom::BluetoothSdpAttribute::New();
  valueUUID->type = bluez::BluetoothServiceAttributeValueBlueZ::UUID;
  valueUUID->type_size = static_cast<uint32_t>(sizeof(uint16_t));
  valueUUID->value.AppendString(l2capUUID);
  valueUUID->sequence =
      mojo::Array<arc::mojom::BluetoothSdpAttributePtr>::New(0);

  // Create an UINT value.
  uint16_t l2capChannel = 3;
  auto valueUint16 = arc::mojom::BluetoothSdpAttribute::New();
  valueUint16->type = bluez::BluetoothServiceAttributeValueBlueZ::UINT;
  valueUint16->type_size = static_cast<uint32_t>(sizeof(l2capChannel));
  valueUint16->value.AppendInteger(static_cast<int>(l2capChannel));
  valueUint16->sequence =
      mojo::Array<arc::mojom::BluetoothSdpAttributePtr>::New(0);

  // Create a sequence with the above two values.
  auto sequenceMojo = arc::mojom::BluetoothSdpAttribute::New();
  sequenceMojo->type = bluez::BluetoothServiceAttributeValueBlueZ::SEQUENCE;
  sequenceMojo->sequence.push_back(std::move(valueUUID));
  sequenceMojo->sequence.push_back(std::move(valueUint16));
  sequenceMojo->type_size = sequenceMojo->sequence.size();
  sequenceMojo->value.Clear();

  auto sequenceBlueZ =
      sequenceMojo.To<bluez::BluetoothServiceAttributeValueBlueZ>();

  EXPECT_EQ(bluez::BluetoothServiceAttributeValueBlueZ::SEQUENCE,
            sequenceBlueZ.type());
  EXPECT_EQ(sequenceMojo->sequence.size(), sequenceBlueZ.sequence().size());

  const auto& sequence = sequenceBlueZ.sequence();
  std::string uuid;
  EXPECT_EQ(bluez::BluetoothServiceAttributeValueBlueZ::UUID,
            sequence.at(0).type());
  EXPECT_EQ(sizeof(uint16_t), sequence.at(0).size());
  EXPECT_TRUE(sequence.at(0).value().GetAsString(&uuid));
  EXPECT_EQ(l2capUUID, uuid);

  int channel;
  EXPECT_EQ(bluez::BluetoothServiceAttributeValueBlueZ::UINT,
            sequence.at(1).type());
  EXPECT_EQ(sizeof(l2capChannel), sequence.at(1).size());
  EXPECT_TRUE(sequence.at(1).value().GetAsInteger(&channel));
  EXPECT_EQ(l2capChannel, static_cast<uint16_t>(channel));
}

TEST(BluetoothTypeConvertorTest,
     ConvertInvalidMojoValueAttributeToBlueZAttribute) {
  // Create a Mojo attribute without value defined.
  auto valueNoData = arc::mojom::BluetoothSdpAttribute::New();
  valueNoData->type = bluez::BluetoothServiceAttributeValueBlueZ::UINT;
  valueNoData->type_size = static_cast<uint32_t>(sizeof(uint32_t));
  valueNoData->value.Clear();
  valueNoData->sequence =
      mojo::Array<arc::mojom::BluetoothSdpAttributePtr>::New(0);

  auto valueNoDataBlueZ =
      valueNoData.To<bluez::BluetoothServiceAttributeValueBlueZ>();

  EXPECT_EQ(bluez::BluetoothServiceAttributeValueBlueZ::NULLTYPE,
            valueNoDataBlueZ.type());
  EXPECT_EQ(0u, valueNoDataBlueZ.size());
  EXPECT_EQ(base::Value::TYPE_NULL, valueNoDataBlueZ.value().GetType());
}

TEST(BluetoothTypeConvertorTest,
     ConvertInvalidMojoSequenceAttributeToBlueZAttribute) {
  // Create a Mojo attribute with an empty sequence.
  auto sequenceNoData = arc::mojom::BluetoothSdpAttribute::New();
  sequenceNoData->type = bluez::BluetoothServiceAttributeValueBlueZ::SEQUENCE;
  sequenceNoData->type_size = 0;
  sequenceNoData->value.Append(base::Value::CreateNullValue());
  sequenceNoData->sequence =
      mojo::Array<arc::mojom::BluetoothSdpAttributePtr>::New(0);

  auto sequenceNoDataBlueZ =
      sequenceNoData.To<bluez::BluetoothServiceAttributeValueBlueZ>();

  EXPECT_EQ(bluez::BluetoothServiceAttributeValueBlueZ::NULLTYPE,
            sequenceNoDataBlueZ.type());
  EXPECT_EQ(0u, sequenceNoDataBlueZ.size());
  EXPECT_EQ(base::Value::TYPE_NULL, sequenceNoDataBlueZ.value().GetType());

  // Create a Mojo attribute with the depth = arc::kBluetoothSDPMaxDepth + 3.
  auto sequenceTooDeepMojo =
      CreateDeepMojoSequenceAttribute(arc::kBluetoothSDPMaxDepth + 3);
  auto sequenceTooDeepBlueZ =
      sequenceTooDeepMojo.To<bluez::BluetoothServiceAttributeValueBlueZ>();

  EXPECT_EQ(bluez::BluetoothServiceAttributeValueBlueZ::SEQUENCE,
            sequenceTooDeepBlueZ.type());
  EXPECT_EQ(1u, sequenceTooDeepBlueZ.size());
  EXPECT_EQ(arc::kBluetoothSDPMaxDepth,
            GetDepthOfBlueZAttribute(sequenceTooDeepBlueZ));
}

TEST(BluetoothTypeConvertorTest, ConvertBlueZValueAttributeToMojoAttribute) {
  // Check NULL type.
  auto nulltypeAttributeBlueZ = bluez::BluetoothServiceAttributeValueBlueZ();
  base::Value* actualValue;

  auto nulltypeAttributeMojo =
      ConvertTo<arc::mojom::BluetoothSdpAttributePtr>(nulltypeAttributeBlueZ);

  EXPECT_EQ(bluez::BluetoothServiceAttributeValueBlueZ::NULLTYPE,
            nulltypeAttributeMojo->type);
  EXPECT_EQ(0u, nulltypeAttributeMojo->type_size);
  EXPECT_TRUE(nulltypeAttributeMojo->value.Get(0, &actualValue));
  EXPECT_EQ(base::Value::TYPE_NULL, actualValue->GetType());

  // Check integer types (INT, UINT).
  uint16_t valueUint16 = 10;
  int valueUint16AsInt;
  auto uintAttributeBlueZ = bluez::BluetoothServiceAttributeValueBlueZ(
      bluez::BluetoothServiceAttributeValueBlueZ::UINT, sizeof(valueUint16),
      base::MakeUnique<base::FundamentalValue>(
          base::FundamentalValue(static_cast<int>(valueUint16))));

  auto uintAttributeMojo =
      ConvertTo<arc::mojom::BluetoothSdpAttributePtr>(uintAttributeBlueZ);

  EXPECT_EQ(bluez::BluetoothServiceAttributeValueBlueZ::UINT,
            uintAttributeMojo->type);
  EXPECT_EQ(sizeof(valueUint16), uintAttributeMojo->type_size);
  EXPECT_TRUE(uintAttributeMojo->value.GetInteger(0, &valueUint16AsInt));
  EXPECT_EQ(valueUint16, static_cast<uint16_t>(valueUint16AsInt));

  // Check bool type.
  bool valueBool = false;
  bool actualBool = true;
  auto boolAttributeBlueZ = bluez::BluetoothServiceAttributeValueBlueZ(
      bluez::BluetoothServiceAttributeValueBlueZ::BOOL, sizeof(bool),
      base::MakeUnique<base::FundamentalValue>(
          base::FundamentalValue(valueBool)));

  auto boolAttributeMojo =
      ConvertTo<arc::mojom::BluetoothSdpAttributePtr>(boolAttributeBlueZ);

  EXPECT_EQ(bluez::BluetoothServiceAttributeValueBlueZ::BOOL,
            boolAttributeMojo->type);
  EXPECT_EQ(static_cast<uint32_t>(sizeof(valueBool)),
            boolAttributeMojo->type_size);
  EXPECT_TRUE(boolAttributeMojo->value.GetBoolean(0, &actualBool));
  EXPECT_FALSE(valueBool);

  // Check UUID type.
  std::string valueUUID("00000100-0000-1000-8000-00805f9b34fb");
  std::string actualUUID;
  auto uuidAttributeBlueZ = bluez::BluetoothServiceAttributeValueBlueZ(
      bluez::BluetoothServiceAttributeValueBlueZ::UUID, sizeof(uint16_t),
      base::MakeUnique<base::StringValue>(base::StringValue(valueUUID)));

  auto uuidAttributeMojo =
      ConvertTo<arc::mojom::BluetoothSdpAttributePtr>(uuidAttributeBlueZ);

  EXPECT_EQ(bluez::BluetoothServiceAttributeValueBlueZ::UUID,
            uuidAttributeMojo->type);
  EXPECT_EQ(static_cast<uint32_t>(sizeof(uint16_t)),
            uuidAttributeMojo->type_size);
  EXPECT_TRUE(uuidAttributeMojo->value.GetString(0, &actualUUID));
  EXPECT_EQ(valueUUID, actualUUID);

  // Check string types (STRING, URL).
  std::string valueString("Some Service Name");
  std::string actualString;
  auto stringAttributeBlueZ = bluez::BluetoothServiceAttributeValueBlueZ(
      bluez::BluetoothServiceAttributeValueBlueZ::STRING, valueString.length(),
      base::MakeUnique<base::StringValue>(valueString));

  auto stringAttributeMojo =
      ConvertTo<arc::mojom::BluetoothSdpAttributePtr>(stringAttributeBlueZ);

  EXPECT_EQ(bluez::BluetoothServiceAttributeValueBlueZ::STRING,
            stringAttributeMojo->type);
  EXPECT_EQ(static_cast<uint32_t>(valueString.length()),
            stringAttributeMojo->type_size);
  EXPECT_TRUE(stringAttributeMojo->value.GetString(0, &actualString));
  EXPECT_EQ(valueString, actualString);
}

TEST(BluetoothTypeConvertorTest, ConvertBlueZSequenceAttributeToMojoAttribute) {
  std::string l2capUUID("00000100-0000-1000-8000-00805f9b34fb");
  uint16_t l2capChannel = 3;

  std::unique_ptr<bluez::BluetoothServiceAttributeValueBlueZ::Sequence>
      sequence(new bluez::BluetoothServiceAttributeValueBlueZ::Sequence());
  sequence->push_back(bluez::BluetoothServiceAttributeValueBlueZ(
      bluez::BluetoothServiceAttributeValueBlueZ::UUID, sizeof(uint16_t),
      base::MakeUnique<base::StringValue>(base::StringValue(l2capUUID))));
  sequence->push_back(bluez::BluetoothServiceAttributeValueBlueZ(
      bluez::BluetoothServiceAttributeValueBlueZ::UINT, sizeof(uint16_t),
      base::MakeUnique<base::FundamentalValue>(
          base::FundamentalValue(l2capChannel))));

  auto sequenceBlueZ =
      bluez::BluetoothServiceAttributeValueBlueZ(std::move(sequence));

  ASSERT_EQ(2u, sequenceBlueZ.sequence().size());

  auto sequenceMojo =
      ConvertTo<arc::mojom::BluetoothSdpAttributePtr>(sequenceBlueZ);

  EXPECT_EQ(bluez::BluetoothServiceAttributeValueBlueZ::SEQUENCE,
            sequenceMojo->type);
  EXPECT_EQ(static_cast<uint32_t>(sequenceBlueZ.size()),
            sequenceMojo->type_size);
  EXPECT_EQ(sequenceMojo->type_size, sequenceMojo->sequence.size());

  std::string uuid;
  EXPECT_EQ(bluez::BluetoothServiceAttributeValueBlueZ::UUID,
            sequenceMojo->sequence[0]->type);
  EXPECT_EQ(static_cast<uint32_t>(sizeof(uint16_t)),
            sequenceMojo->sequence[0]->type_size);
  EXPECT_TRUE(sequenceMojo->sequence[0]->value.GetString(0, &uuid));
  EXPECT_EQ(l2capUUID, uuid);

  int channel;
  EXPECT_EQ(bluez::BluetoothServiceAttributeValueBlueZ::UINT,
            sequenceMojo->sequence[1]->type);
  EXPECT_EQ(static_cast<uint32_t>(sizeof(uint16_t)),
            sequenceMojo->sequence[1]->type_size);
  EXPECT_TRUE(sequenceMojo->sequence[1]->value.GetInteger(0, &channel));
  EXPECT_EQ(l2capChannel, static_cast<uint16_t>(channel));
}

TEST(BluetoothTypeConvertorTest,
     ConvertDeepBlueZSequenceAttributeToBlueZAttribute) {
  bluez::BluetoothServiceAttributeValueBlueZ sequenceBlueZ =
      CreateDeepBlueZSequenceAttribute(arc::kBluetoothSDPMaxDepth + 3);

  auto sequenceMojo =
      ConvertTo<arc::mojom::BluetoothSdpAttributePtr>(sequenceBlueZ);

  EXPECT_EQ(bluez::BluetoothServiceAttributeValueBlueZ::SEQUENCE,
            sequenceMojo->type);
  EXPECT_EQ((uint32_t)1, sequenceMojo->type_size);
  EXPECT_EQ(arc::kBluetoothSDPMaxDepth, GetDepthOfMojoAttribute(sequenceMojo));
}

}  // namespace mojo
