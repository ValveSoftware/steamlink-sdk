// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/property_converter.h"

#include <stdint.h>
#include <string>

#include "base/logging.h"
#include "base/macros.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/window.h"
#include "ui/aura/window_property.h"
#include "ui/gfx/geometry/rect.h"

// See aura_constants.cc for bool, int32_t, int64_t, std::string, and gfx::Rect.
// That same file also declares the uint32_t type via SkColor.
DECLARE_WINDOW_PROPERTY_TYPE(uint8_t)
DECLARE_WINDOW_PROPERTY_TYPE(uint16_t)
DECLARE_WINDOW_PROPERTY_TYPE(uint64_t)
DECLARE_WINDOW_PROPERTY_TYPE(int8_t)
DECLARE_WINDOW_PROPERTY_TYPE(int16_t)

namespace aura {

namespace {

DEFINE_WINDOW_PROPERTY_KEY(bool, kTestPropertyKey0, false);
DEFINE_WINDOW_PROPERTY_KEY(uint8_t, kTestPropertyKey1, 0);
DEFINE_WINDOW_PROPERTY_KEY(uint16_t, kTestPropertyKey2, 0);
DEFINE_WINDOW_PROPERTY_KEY(uint32_t, kTestPropertyKey3, 0);
DEFINE_WINDOW_PROPERTY_KEY(uint64_t, kTestPropertyKey4, 0);
DEFINE_WINDOW_PROPERTY_KEY(int8_t, kTestPropertyKey5, 0);
DEFINE_WINDOW_PROPERTY_KEY(int16_t, kTestPropertyKey6, 0);
DEFINE_WINDOW_PROPERTY_KEY(int32_t, kTestPropertyKey7, 0);
DEFINE_WINDOW_PROPERTY_KEY(int64_t, kTestPropertyKey8, 0);

DEFINE_OWNED_WINDOW_PROPERTY_KEY(gfx::Rect, kTestRectPropertyKey, nullptr);
DEFINE_OWNED_WINDOW_PROPERTY_KEY(std::string, kTestStringPropertyKey, nullptr);

const char kTestPropertyServerKey0[] = "test-property-server0";
const char kTestPropertyServerKey1[] = "test-property-server1";
const char kTestPropertyServerKey2[] = "test-property-server2";
const char kTestPropertyServerKey3[] = "test-property-server3";
const char kTestPropertyServerKey4[] = "test-property-server4";
const char kTestPropertyServerKey5[] = "test-property-server5";
const char kTestPropertyServerKey6[] = "test-property-server6";
const char kTestPropertyServerKey7[] = "test-property-server7";
const char kTestPropertyServerKey8[] = "test-property-server8";

const char kTestRectPropertyServerKey[] = "test-rect-property-server";
const char kTestStringPropertyServerKey[] = "test-string-property-server";

// Test registration, naming and value conversion for primitive property types.
template <typename T>
void TestPrimitiveProperty(PropertyConverter* property_converter,
                           Window* window,
                           const WindowProperty<T>* key,
                           const char* transport_name,
                           T value_1,
                           T value_2) {
  property_converter->RegisterProperty(key, transport_name);
  EXPECT_EQ(transport_name,
            property_converter->GetTransportNameForPropertyKey(key));

  window->SetProperty(key, value_1);
  EXPECT_EQ(value_1, window->GetProperty(key));

  std::string transport_name_out;
  std::unique_ptr<std::vector<uint8_t>> transport_value_out;
  EXPECT_TRUE(property_converter->ConvertPropertyForTransport(
      window, key, &transport_name_out, &transport_value_out));
  EXPECT_EQ(transport_name, transport_name_out);
  const int64_t storage_value_1 = static_cast<int64_t>(value_1);
  std::vector<uint8_t> transport_value1 =
      mojo::ConvertTo<std::vector<uint8_t>>(storage_value_1);
  EXPECT_EQ(transport_value1, *transport_value_out.get());

  const int64_t storage_value_2 = static_cast<int64_t>(value_2);
  std::vector<uint8_t> transport_value2 =
      mojo::ConvertTo<std::vector<uint8_t>>(storage_value_2);
  property_converter->SetPropertyFromTransportValue(window, transport_name,
                                                    &transport_value2);
  EXPECT_EQ(value_2, window->GetProperty(key));
}

}  // namespace

using PropertyConverterTest = test::AuraTestBase;

// Verifies property setting behavior for a std::string* property.
TEST_F(PropertyConverterTest, PrimitiveProperties) {
  PropertyConverter property_converter;
  std::unique_ptr<Window> window(CreateNormalWindow(1, root_window(), nullptr));

  const bool value_0a = true, value_0b = false;
  TestPrimitiveProperty(&property_converter, window.get(), kTestPropertyKey0,
                        kTestPropertyServerKey0, value_0a, value_0b);

  const uint8_t value_1a = UINT8_MAX / 2, value_1b = UINT8_MAX / 3;
  TestPrimitiveProperty(&property_converter, window.get(), kTestPropertyKey1,
                        kTestPropertyServerKey1, value_1a, value_1b);

  const uint16_t value_2a = UINT16_MAX / 3, value_2b = UINT16_MAX / 4;
  TestPrimitiveProperty(&property_converter, window.get(), kTestPropertyKey2,
                        kTestPropertyServerKey2, value_2a, value_2b);

  const uint32_t value_3a = UINT32_MAX / 4, value_3b = UINT32_MAX / 5;
  TestPrimitiveProperty(&property_converter, window.get(), kTestPropertyKey3,
                        kTestPropertyServerKey3, value_3a, value_3b);

  const uint64_t value_4a = UINT64_MAX / 5, value_4b = UINT64_MAX / 6;
  TestPrimitiveProperty(&property_converter, window.get(), kTestPropertyKey4,
                        kTestPropertyServerKey4, value_4a, value_4b);

  const int8_t value_5a = INT8_MIN / 2, value_5b = INT8_MIN / 3;
  TestPrimitiveProperty(&property_converter, window.get(), kTestPropertyKey5,
                        kTestPropertyServerKey5, value_5a, value_5b);

  const int16_t value_6a = INT16_MIN / 3, value_6b = INT16_MIN / 4;
  TestPrimitiveProperty(&property_converter, window.get(), kTestPropertyKey6,
                        kTestPropertyServerKey6, value_6a, value_6b);

  const int32_t value_7a = INT32_MIN / 4, value_7b = INT32_MIN / 5;
  TestPrimitiveProperty(&property_converter, window.get(), kTestPropertyKey7,
                        kTestPropertyServerKey7, value_7a, value_7b);

  const int64_t value_8a = INT64_MIN / 5, value_8b = INT64_MIN / 6;
  TestPrimitiveProperty(&property_converter, window.get(), kTestPropertyKey8,
                        kTestPropertyServerKey8, value_8a, value_8b);
}

// Verifies property setting behavior for a gfx::Rect* property.
TEST_F(PropertyConverterTest, RectProperty) {
  PropertyConverter property_converter;
  property_converter.RegisterProperty(kTestRectPropertyKey,
                                      kTestRectPropertyServerKey);
  EXPECT_EQ(
      kTestRectPropertyServerKey,
      property_converter.GetTransportNameForPropertyKey(kTestRectPropertyKey));

  gfx::Rect value_1(1, 2, 3, 4);
  std::unique_ptr<Window> window(CreateNormalWindow(1, root_window(), nullptr));
  window->SetProperty(kTestRectPropertyKey, new gfx::Rect(value_1));
  EXPECT_EQ(value_1, *window->GetProperty(kTestRectPropertyKey));

  std::string transport_name_out;
  std::unique_ptr<std::vector<uint8_t>> transport_value_out;
  EXPECT_TRUE(property_converter.ConvertPropertyForTransport(
      window.get(), kTestRectPropertyKey, &transport_name_out,
      &transport_value_out));
  EXPECT_EQ(kTestRectPropertyServerKey, transport_name_out);
  EXPECT_EQ(mojo::ConvertTo<std::vector<uint8_t>>(value_1),
            *transport_value_out.get());

  gfx::Rect value_2(1, 3, 5, 7);
  std::vector<uint8_t> transport_value =
      mojo::ConvertTo<std::vector<uint8_t>>(value_2);
  property_converter.SetPropertyFromTransportValue(
      window.get(), kTestRectPropertyServerKey, &transport_value);
  EXPECT_EQ(value_2, *window->GetProperty(kTestRectPropertyKey));
}

// Verifies property setting behavior for a std::string* property.
TEST_F(PropertyConverterTest, StringProperty) {
  PropertyConverter property_converter;
  property_converter.RegisterProperty(kTestStringPropertyKey,
                                      kTestStringPropertyServerKey);
  EXPECT_EQ(kTestStringPropertyServerKey,
            property_converter.GetTransportNameForPropertyKey(
                kTestStringPropertyKey));

  std::string value_1 = "test value";
  std::unique_ptr<Window> window(CreateNormalWindow(1, root_window(), nullptr));
  window->SetProperty(kTestStringPropertyKey, new std::string(value_1));
  EXPECT_EQ(value_1, *window->GetProperty(kTestStringPropertyKey));

  std::string transport_name_out;
  std::unique_ptr<std::vector<uint8_t>> transport_value_out;
  EXPECT_TRUE(property_converter.ConvertPropertyForTransport(
      window.get(), kTestStringPropertyKey, &transport_name_out,
      &transport_value_out));
  EXPECT_EQ(kTestStringPropertyServerKey, transport_name_out);
  EXPECT_EQ(mojo::ConvertTo<std::vector<uint8_t>>(value_1),
            *transport_value_out.get());

  std::string value_2 = "another test value";
  std::vector<uint8_t> transport_value =
      mojo::ConvertTo<std::vector<uint8_t>>(value_2);
  property_converter.SetPropertyFromTransportValue(
      window.get(), kTestStringPropertyServerKey, &transport_value);
  EXPECT_EQ(value_2, *window->GetProperty(kTestStringPropertyKey));
}

}  // namespace aura
