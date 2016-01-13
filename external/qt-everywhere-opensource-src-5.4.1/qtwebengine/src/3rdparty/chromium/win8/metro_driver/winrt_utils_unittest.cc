// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "stdafx.h"

#include "winrt_utils.h"

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

template <typename Type>
static HRESULT CreateProperty(Type value, winfoundtn::IPropertyValue** prop) {
  return E_NOTIMPL;
}

template <>
static HRESULT CreateProperty<const wchar_t*>(
    const wchar_t* value, winfoundtn::IPropertyValue** prop) {
  mswrw::HString string_value;
  string_value.Attach(MakeHString(value));
  return winrt_utils::CreateStringProperty(string_value.Get(), prop);
}

template <>
static HRESULT CreateProperty<INT16>(INT16 value,
                                     winfoundtn::IPropertyValue** prop) {
  return winrt_utils::CreateInt16Property(value, prop);
}

template <>
static HRESULT CreateProperty<INT32>(INT32 value,
                                     winfoundtn::IPropertyValue** prop) {
  return winrt_utils::CreateInt32Property(value, prop);
}

template <>
static HRESULT CreateProperty<INT64>(INT64 value,
                                     winfoundtn::IPropertyValue** prop) {
  return winrt_utils::CreateInt64Property(value, prop);
}

template <>
static HRESULT CreateProperty<UINT8>(UINT8 value,
                                     winfoundtn::IPropertyValue** prop) {
  return winrt_utils::CreateUInt8Property(value, prop);
}

template <>
static HRESULT CreateProperty<UINT16>(UINT16 value,
                                      winfoundtn::IPropertyValue** prop) {
  return winrt_utils::CreateUInt16Property(value, prop);
}

template <>
static HRESULT CreateProperty<UINT32>(UINT32 value,
                                      winfoundtn::IPropertyValue** prop) {
  return winrt_utils::CreateUInt32Property(value, prop);
}

template <>
static HRESULT CreateProperty<UINT64>(UINT64 value,
                                      winfoundtn::IPropertyValue** prop) {
  return winrt_utils::CreateUInt64Property(value, prop);
}

template<typename Type>
void TestCompareProperties(Type value1, Type value2) {
  mswr::ComPtr<winfoundtn::IPropertyValue> property_1;
  HRESULT hr = CreateProperty<Type>(value1, property_1.GetAddressOf());
  ASSERT_TRUE(SUCCEEDED(hr)) << "Can't create Property value 1";

  mswr::ComPtr<winfoundtn::IPropertyValue> other_property_1;
  hr = CreateProperty<Type>(value1, other_property_1.GetAddressOf());
  ASSERT_TRUE(SUCCEEDED(hr)) << "Can't create another Property value 1";

  mswr::ComPtr<winfoundtn::IPropertyValue> property_2;
  hr = CreateProperty<Type>(value2, property_2.GetAddressOf());
  ASSERT_TRUE(SUCCEEDED(hr)) << "Can't create Property value 2";

  INT32 result = 42;
  hr = winrt_utils::CompareProperties(
      property_1.Get(), property_1.Get(), &result);
  ASSERT_TRUE(SUCCEEDED(hr)) << "Can't compare property_1 to itself";
  EXPECT_EQ(0, result) << "Bad result value while comparing same property";

  hr = winrt_utils::CompareProperties(
      property_1.Get(), other_property_1.Get(), &result);
  ASSERT_TRUE(SUCCEEDED(hr)) << "Can't compare property_1 to other_property_1";
  EXPECT_EQ(0, result) << "Bad result while comparing equal values";

  hr = winrt_utils::CompareProperties(
      property_1.Get(), property_2.Get(), &result);
  ASSERT_TRUE(SUCCEEDED(hr)) << "Can't compare property_1 to property_2";
  EXPECT_EQ(-1, result) << "Bad result while comparing values for less than";

  hr = winrt_utils::CompareProperties(
      property_2.Get(), property_1.Get(), &result);
  ASSERT_TRUE(SUCCEEDED(hr)) << "Can't compare property_1 to property_2";
  EXPECT_EQ(1, result) << "Bad result value while comparing for greater than";
}

TEST(PropertyValueCompareTest, CompareProperties) {
  TestCompareProperties<INT16>(42, 43);
  TestCompareProperties<INT32>(42, 43);
  TestCompareProperties<INT64>(42, 43);
  TestCompareProperties<UINT8>(42, 43);
  TestCompareProperties<UINT16>(42, 43);
  TestCompareProperties<UINT32>(42, 43);
  TestCompareProperties<UINT64>(42, 43);
  TestCompareProperties<const wchar_t*>(L"abc", L"bcd");
}

}  // namespace
