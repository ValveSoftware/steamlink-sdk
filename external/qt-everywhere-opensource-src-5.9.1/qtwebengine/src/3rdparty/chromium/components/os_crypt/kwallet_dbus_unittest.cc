// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/kwallet_dbus.h"

#include <memory>
#include <string>

#include "base/logging.h"
#include "base/nix/xdg_util.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::_;
using testing::AllOf;
using testing::DoAll;
using testing::ElementsAreArray;
using testing::Invoke;
using testing::Return;
using testing::StrictMock;

const char kKWalletInterface[] = "org.kde.KWallet";
const char kKLauncherInterface[] = "org.kde.KLauncher";

dbus::Response* RespondBool(bool value) {
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  writer.AppendBool(value);
  return response.release();
}

dbus::Response* RespondString(const std::string& value) {
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  writer.AppendString(value);
  return response.release();
}

dbus::Response* RespondBytes(const std::vector<uint8_t>& bytes) {
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  writer.AppendArrayOfBytes(bytes.data(), bytes.size());
  return response.release();
}

dbus::Response* RespondArrayOfStrings(const std::vector<std::string>& strings) {
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  writer.AppendArrayOfStrings(strings);
  return response.release();
}

dbus::Response* RespondInt32(int value) {
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  writer.AppendInt32(value);
  return response.release();
}

dbus::Response* RespondEmpty() {
  return dbus::Response::CreateEmpty().release();
}

class KWalletDBusTest
    : public testing::TestWithParam<base::nix::DesktopEnvironment> {
 public:
  KWalletDBusTest() : desktop_env_(GetParam()), kwallet_dbus_(desktop_env_) {
    if (desktop_env_ == base::nix::DESKTOP_ENVIRONMENT_KDE5) {
      dbus_service_name_ = "org.kde.kwalletd5";
      dbus_path_ = "/modules/kwalletd5";
      kwalletd_name_ = "kwalletd5";
    } else {
      dbus_service_name_ = "org.kde.kwalletd";
      dbus_path_ = "/modules/kwalletd";
      kwalletd_name_ = "kwalletd";
    }

    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SESSION;
    mock_session_bus_ = new dbus::MockBus(options);

    mock_klauncher_proxy_ = new StrictMock<dbus::MockObjectProxy>(
        mock_session_bus_.get(), "org.kde.klauncher",
        dbus::ObjectPath("/KLauncher"));
    mock_kwallet_proxy_ = new StrictMock<dbus::MockObjectProxy>(
        mock_session_bus_.get(), dbus_service_name_,
        dbus::ObjectPath(dbus_path_));

    // The kwallet proxy is aquired once, when preparing |kwallet_dbus_|
    EXPECT_CALL(
        *mock_session_bus_.get(),
        GetObjectProxy(dbus_service_name_, dbus::ObjectPath(dbus_path_)))
        .WillOnce(Return(mock_kwallet_proxy_.get()));

    kwallet_dbus_.SetSessionBus(mock_session_bus_);

    testing::Mock::VerifyAndClearExpectations(mock_session_bus_.get());
  }

 protected:
  const base::nix::DesktopEnvironment desktop_env_;
  scoped_refptr<dbus::MockBus> mock_session_bus_;
  scoped_refptr<dbus::MockObjectProxy> mock_klauncher_proxy_;
  scoped_refptr<dbus::MockObjectProxy> mock_kwallet_proxy_;
  KWalletDBus kwallet_dbus_;

  std::string dbus_service_name_;
  std::string dbus_path_;
  std::string kwalletd_name_;

 private:
  DISALLOW_COPY_AND_ASSIGN(KWalletDBusTest);
};

INSTANTIATE_TEST_CASE_P(,
                        KWalletDBusTest,
                        ::testing::Values(base::nix::DESKTOP_ENVIRONMENT_KDE4,
                                          base::nix::DESKTOP_ENVIRONMENT_KDE5));

// Matches a method call to the specified dbus target.
MATCHER_P2(Calls, interface, member, "") {
  return arg->GetMember() == member && arg->GetInterface() == interface;
}

// Pops items from the dbus message and compares them to the expected values.
MATCHER_P3(ArgumentsAreIntStringString, int_1, str_2, str_3, "") {
  dbus::MessageReader reader(arg);

  int i;
  EXPECT_TRUE(reader.PopInt32(&i));
  if (int_1 != i)
    return false;

  std::string str;
  EXPECT_TRUE(reader.PopString(&str));
  if (str_2 != str)
    return false;

  EXPECT_TRUE(reader.PopString(&str));
  if (str_3 != str)
    return false;

  return true;
}

// Pops items from the dbus message and compares them to the expected values.
MATCHER_P4(ArgumentsAreIntStringStringString, int_1, str_2, str_3, str_4, "") {
  dbus::MessageReader reader(arg);

  int i;
  EXPECT_TRUE(reader.PopInt32(&i));
  if (int_1 != i)
    return false;

  std::string str;
  EXPECT_TRUE(reader.PopString(&str));
  if (str_2 != str)
    return false;

  EXPECT_TRUE(reader.PopString(&str));
  if (str_3 != str)
    return false;

  EXPECT_TRUE(reader.PopString(&str));
  if (str_4 != str)
    return false;

  return true;
}

// Pops items from the dbus message and compares them to the expected values.
MATCHER_P5(ArgumentsAreIntStringStringBytesString,
           int_1,
           str_2,
           str_3,
           vec_4,
           str_5,
           "") {
  dbus::MessageReader reader(arg);

  int i;
  EXPECT_TRUE(reader.PopInt32(&i));
  if (int_1 != i)
    return false;

  std::string str;
  EXPECT_TRUE(reader.PopString(&str));
  if (str_2 != str)
    return false;

  EXPECT_TRUE(reader.PopString(&str));
  if (str_3 != str)
    return false;

  const uint8_t* bytes = nullptr;
  size_t length = 0;
  EXPECT_TRUE(reader.PopArrayOfBytes(&bytes, &length));
  std::vector<uint8_t> vec;
  vec.assign(bytes, bytes + length);
  if (vec_4 != vec)
    return false;

  EXPECT_TRUE(reader.PopString(&str));
  if (str_5 != str)
    return false;

  return true;
}

// Pops items from the dbus message and compares them to the expected values.
MATCHER_P3(ArgumentsAreStringInt64String, str_1, int_2, str_3, "") {
  dbus::MessageReader reader(arg);

  std::string str;
  EXPECT_TRUE(reader.PopString(&str));
  if (str_1 != str)
    return false;

  int64_t i;
  EXPECT_TRUE(reader.PopInt64(&i));
  if (int_2 != i)
    return false;

  EXPECT_TRUE(reader.PopString(&str));
  if (str_3 != str)
    return false;

  return true;
}

// Pops items from the dbus message and compares them to the expected values.
MATCHER_P5(ArgumentsAreStringStringsStringsStringBool,
           str_1,
           vec_2,
           vec_3,
           str_4,
           bool_5,
           "") {
  dbus::MessageReader reader(arg);

  std::string str;
  EXPECT_TRUE(reader.PopString(&str));
  if (str_1 != str)
    return false;

  std::vector<std::string> strings;
  EXPECT_TRUE(reader.PopArrayOfStrings(&strings));
  if (vec_2 != strings)
    return false;

  EXPECT_TRUE(reader.PopArrayOfStrings(&strings));
  if (vec_3 != strings)
    return false;

  EXPECT_TRUE(reader.PopString(&str));
  if (str_4 != str)
    return false;

  bool b;
  EXPECT_TRUE(reader.PopBool(&b));
  if (bool_5 != b)
    return false;

  return true;
}

MATCHER_P5(ArgumentsAreIntStringStringStringString,
           int_1,
           str_2,
           str_3,
           str_4,
           str_5,
           "") {
  dbus::MessageReader reader(arg);

  int32_t i;
  EXPECT_TRUE(reader.PopInt32(&i));
  if (int_1 != i)
    return false;

  std::string str;
  EXPECT_TRUE(reader.PopString(&str));
  if (str_2 != str)
    return false;

  EXPECT_TRUE(reader.PopString(&str));
  if (str_3 != str)
    return false;

  EXPECT_TRUE(reader.PopString(&str));
  if (str_4 != str)
    return false;

  EXPECT_TRUE(reader.PopString(&str));
  if (str_5 != str)
    return false;

  return true;
}

MATCHER_P3(ArgumentsAreIntBoolString, int_1, bool_2, str_3, "") {
  dbus::MessageReader reader(arg);

  int32_t i;
  EXPECT_TRUE(reader.PopInt32(&i));
  if (int_1 != i)
    return false;

  bool b;
  EXPECT_TRUE(reader.PopBool(&b));
  if (bool_2 != b)
    return false;

  std::string str;
  EXPECT_TRUE(reader.PopString(&str));
  if (str_3 != str)
    return false;

  return true;
}

TEST_P(KWalletDBusTest, StartWalletd) {
  // The receiver of the message takes ownership of the response object.
  dbus::Response* response_success = RespondEmpty();
  dbus::MessageWriter writer(response_success);
  writer.AppendInt32(0);  // return code
  writer.AppendString("dbus_name");
  writer.AppendString(std::string());  // error message
  writer.AppendInt32(100);             // pid

  EXPECT_CALL(
      *mock_session_bus_.get(),
      GetObjectProxy("org.kde.klauncher", dbus::ObjectPath("/KLauncher")))
      .WillOnce(Return(mock_klauncher_proxy_.get()));

  EXPECT_CALL(
      *mock_klauncher_proxy_.get(),
      MockCallMethodAndBlock(
          AllOf(Calls(kKLauncherInterface, "start_service_by_desktop_name"),
                ArgumentsAreStringStringsStringsStringBool(
                    kwalletd_name_, std::vector<std::string>(),
                    std::vector<std::string>(), std::string(), false)),
          _))
      .WillOnce(Return(response_success));

  EXPECT_TRUE(kwallet_dbus_.StartKWalletd());
}

TEST_P(KWalletDBusTest, StartWalletdErrorRead) {
  EXPECT_CALL(
      *mock_session_bus_.get(),
      GetObjectProxy("org.kde.klauncher", dbus::ObjectPath("/KLauncher")))
      .WillOnce(Return(mock_klauncher_proxy_.get()));

  EXPECT_CALL(
      *mock_klauncher_proxy_.get(),
      MockCallMethodAndBlock(
          Calls(kKLauncherInterface, "start_service_by_desktop_name"), _))
      .WillOnce(Return(RespondEmpty()));

  EXPECT_FALSE(kwallet_dbus_.StartKWalletd());
}

TEST_P(KWalletDBusTest, StartWalletdErrorContact) {
  EXPECT_CALL(
      *mock_session_bus_.get(),
      GetObjectProxy("org.kde.klauncher", dbus::ObjectPath("/KLauncher")))
      .WillRepeatedly(Return(mock_klauncher_proxy_.get()));

  EXPECT_CALL(
      *mock_klauncher_proxy_.get(),
      MockCallMethodAndBlock(
          Calls(kKLauncherInterface, "start_service_by_desktop_name"), _))
      .WillOnce(Return(nullptr));

  EXPECT_FALSE(kwallet_dbus_.StartKWalletd());
}

TEST_P(KWalletDBusTest, IsEnabledTrue) {
  EXPECT_CALL(*mock_kwallet_proxy_.get(),
              MockCallMethodAndBlock(Calls(kKWalletInterface, "isEnabled"), _))
      .WillOnce(Return(RespondBool(true)));

  bool is_enabled = false;
  EXPECT_EQ(KWalletDBus::Error::SUCCESS, kwallet_dbus_.IsEnabled(&is_enabled));
  EXPECT_TRUE(is_enabled);
}

TEST_P(KWalletDBusTest, IsEnabledFalse) {
  EXPECT_CALL(*mock_kwallet_proxy_.get(),
              MockCallMethodAndBlock(Calls(kKWalletInterface, "isEnabled"), _))
      .WillOnce(Return(RespondBool(false)));

  bool is_enabled = true;
  EXPECT_EQ(KWalletDBus::Error::SUCCESS, kwallet_dbus_.IsEnabled(&is_enabled));
  EXPECT_FALSE(is_enabled);
}

TEST_P(KWalletDBusTest, IsEnabledErrorRead) {
  EXPECT_CALL(*mock_kwallet_proxy_.get(),
              MockCallMethodAndBlock(Calls(kKWalletInterface, "isEnabled"), _))
      .WillOnce(Return(RespondEmpty()));

  bool is_enabled = true;
  EXPECT_EQ(KWalletDBus::Error::CANNOT_READ,
            kwallet_dbus_.IsEnabled(&is_enabled));
}

TEST_P(KWalletDBusTest, IsEnabledErrorContact) {
  EXPECT_CALL(*mock_kwallet_proxy_.get(),
              MockCallMethodAndBlock(Calls(kKWalletInterface, "isEnabled"), _))
      .WillOnce(Return(nullptr));

  bool is_enabled = true;
  EXPECT_EQ(KWalletDBus::Error::CANNOT_CONTACT,
            kwallet_dbus_.IsEnabled(&is_enabled));
}

TEST_P(KWalletDBusTest, NetworkWallet) {
  EXPECT_CALL(
      *mock_kwallet_proxy_.get(),
      MockCallMethodAndBlock(Calls(kKWalletInterface, "networkWallet"), _))
      .WillOnce(Return(RespondString("mock_wallet")));

  std::string wallet;
  EXPECT_EQ(KWalletDBus::Error::SUCCESS, kwallet_dbus_.NetworkWallet(&wallet));
  EXPECT_EQ("mock_wallet", wallet);
}

TEST_P(KWalletDBusTest, NetworkWalletErrorRead) {
  EXPECT_CALL(
      *mock_kwallet_proxy_.get(),
      MockCallMethodAndBlock(Calls(kKWalletInterface, "networkWallet"), _))
      .WillOnce(Return(RespondEmpty()));

  std::string wallet;
  EXPECT_EQ(KWalletDBus::Error::CANNOT_READ,
            kwallet_dbus_.NetworkWallet(&wallet));
}

TEST_P(KWalletDBusTest, NetworkWalletErrorContact) {
  EXPECT_CALL(
      *mock_kwallet_proxy_.get(),
      MockCallMethodAndBlock(Calls(kKWalletInterface, "networkWallet"), _))
      .WillOnce(Return(nullptr));

  std::string wallet;
  EXPECT_EQ(KWalletDBus::Error::CANNOT_CONTACT,
            kwallet_dbus_.NetworkWallet(&wallet));
}

TEST_P(KWalletDBusTest, Open) {
  EXPECT_CALL(*mock_kwallet_proxy_.get(),
              MockCallMethodAndBlock(
                  AllOf(Calls(kKWalletInterface, "open"),
                        ArgumentsAreStringInt64String("wallet", 0, "app")),
                  _))
      .WillOnce(Return(RespondInt32(1234)));

  int ret;
  EXPECT_EQ(KWalletDBus::Error::SUCCESS,
            kwallet_dbus_.Open("wallet", "app", &ret));
  EXPECT_EQ(1234, ret);
}

TEST_P(KWalletDBusTest, OpenErrorRead) {
  EXPECT_CALL(*mock_kwallet_proxy_.get(),
              MockCallMethodAndBlock(Calls(kKWalletInterface, "open"), _))
      .WillOnce(Return(RespondEmpty()));

  int ret;
  EXPECT_EQ(KWalletDBus::Error::CANNOT_READ,
            kwallet_dbus_.Open("wallet", "app", &ret));
}

TEST_P(KWalletDBusTest, OpenErrorContact) {
  EXPECT_CALL(*mock_kwallet_proxy_.get(),
              MockCallMethodAndBlock(Calls(kKWalletInterface, "open"), _))
      .WillOnce(Return(nullptr));

  int ret;
  EXPECT_EQ(KWalletDBus::Error::CANNOT_CONTACT,
            kwallet_dbus_.Open("wallet", "app", &ret));
}

TEST_P(KWalletDBusTest, HasEntry) {
  EXPECT_CALL(*mock_kwallet_proxy_.get(),
              MockCallMethodAndBlock(AllOf(Calls(kKWalletInterface, "hasEntry"),
                                           ArgumentsAreIntStringStringString(
                                               123, "folder", "realm", "app")),
                                     _))
      .WillOnce(Return(RespondBool(true)));

  bool has_entry = false;
  EXPECT_EQ(KWalletDBus::Error::SUCCESS,
            kwallet_dbus_.HasEntry(123, "folder", "realm", "app", &has_entry));
  EXPECT_TRUE(has_entry);
}

TEST_P(KWalletDBusTest, HasEntryErrorRead) {
  EXPECT_CALL(*mock_kwallet_proxy_.get(),
              MockCallMethodAndBlock(Calls(kKWalletInterface, "hasEntry"), _))
      .WillOnce(Return(RespondEmpty()));

  bool has_entry = false;
  EXPECT_EQ(KWalletDBus::Error::CANNOT_READ,
            kwallet_dbus_.HasEntry(123, "folder", "realm", "app", &has_entry));
}

TEST_P(KWalletDBusTest, HasEntryErrorContact) {
  EXPECT_CALL(*mock_kwallet_proxy_.get(),
              MockCallMethodAndBlock(Calls(kKWalletInterface, "hasEntry"), _))
      .WillOnce(Return(nullptr));

  bool has_entry = false;
  EXPECT_EQ(KWalletDBus::Error::CANNOT_CONTACT,
            kwallet_dbus_.HasEntry(123, "folder", "realm", "app", &has_entry));
}

TEST_P(KWalletDBusTest, ReadEntry) {
  const std::vector<uint8_t> bytes_expected = {1, 2, 1, 2};
  EXPECT_CALL(
      *mock_kwallet_proxy_.get(),
      MockCallMethodAndBlock(AllOf(Calls(kKWalletInterface, "readEntry"),
                                   ArgumentsAreIntStringStringString(
                                       123, "folder", "realm", "app")),
                             _))
      .WillOnce(Return(RespondBytes(bytes_expected)));

  std::vector<uint8_t> bytes;
  EXPECT_EQ(KWalletDBus::Error::SUCCESS,
            kwallet_dbus_.ReadEntry(123, "folder", "realm", "app", &bytes));
  EXPECT_EQ(bytes_expected, bytes);
}

TEST_P(KWalletDBusTest, ReadEntryErrorRead) {
  std::vector<uint8_t> bytes_expected = {1, 2, 1, 2};
  EXPECT_CALL(*mock_kwallet_proxy_.get(),
              MockCallMethodAndBlock(Calls(kKWalletInterface, "readEntry"), _))
      .WillOnce(Return(RespondEmpty()));

  std::vector<uint8_t> bytes;
  EXPECT_EQ(KWalletDBus::Error::CANNOT_READ,
            kwallet_dbus_.ReadEntry(123, "folder", "realm", "app", &bytes));
}

TEST_P(KWalletDBusTest, ReadEntryErrorContact) {
  std::vector<uint8_t> bytes_expected = {1, 2, 1, 2};
  EXPECT_CALL(*mock_kwallet_proxy_.get(),
              MockCallMethodAndBlock(Calls(kKWalletInterface, "readEntry"), _))
      .WillOnce(Return(nullptr));

  std::vector<uint8_t> bytes;
  EXPECT_EQ(KWalletDBus::Error::CANNOT_CONTACT,
            kwallet_dbus_.ReadEntry(123, "folder", "realm", "app", &bytes));
}

TEST_P(KWalletDBusTest, EntryList) {
  std::vector<std::string> strings_expected = {"one", "two"};
  EXPECT_CALL(*mock_kwallet_proxy_.get(),
              MockCallMethodAndBlock(
                  AllOf(Calls(kKWalletInterface, "entryList"),
                        ArgumentsAreIntStringString(123, "folder", "app")),
                  _))
      .WillOnce(Return(RespondArrayOfStrings(strings_expected)));

  std::vector<std::string> strings;
  EXPECT_EQ(KWalletDBus::Error::SUCCESS,
            kwallet_dbus_.EntryList(123, "folder", "app", &strings));
  EXPECT_EQ(strings_expected, strings);
}

TEST_P(KWalletDBusTest, EntryListErrorRead) {
  std::vector<std::string> strings_expected = {"one", "two"};
  EXPECT_CALL(*mock_kwallet_proxy_.get(),
              MockCallMethodAndBlock(Calls(kKWalletInterface, "entryList"), _))
      .WillOnce(Return(RespondEmpty()));

  std::vector<std::string> strings;
  EXPECT_EQ(KWalletDBus::Error::CANNOT_READ,
            kwallet_dbus_.EntryList(123, "folder", "app", &strings));
}

TEST_P(KWalletDBusTest, EntryListErrorContact) {
  std::vector<std::string> strings_expected = {"one", "two"};
  EXPECT_CALL(*mock_kwallet_proxy_.get(),
              MockCallMethodAndBlock(Calls(kKWalletInterface, "entryList"), _))
      .WillOnce(Return(nullptr));

  std::vector<std::string> strings;
  EXPECT_EQ(KWalletDBus::Error::CANNOT_CONTACT,
            kwallet_dbus_.EntryList(123, "folder", "app", &strings));
}

TEST_P(KWalletDBusTest, RemoveEntry) {
  EXPECT_CALL(
      *mock_kwallet_proxy_.get(),
      MockCallMethodAndBlock(AllOf(Calls(kKWalletInterface, "removeEntry"),
                                   ArgumentsAreIntStringStringString(
                                       123, "folder", "realm", "app")),
                             _))
      .WillOnce(Return(RespondInt32(0)));

  int ret;
  EXPECT_EQ(KWalletDBus::Error::SUCCESS,
            kwallet_dbus_.RemoveEntry(123, "folder", "realm", "app", &ret));
  EXPECT_EQ(0, ret);
}

TEST_P(KWalletDBusTest, RemoveEntryErrorRead) {
  EXPECT_CALL(
      *mock_kwallet_proxy_.get(),
      MockCallMethodAndBlock(Calls(kKWalletInterface, "removeEntry"), _))
      .WillOnce(Return(RespondEmpty()));

  int ret;
  EXPECT_EQ(KWalletDBus::Error::CANNOT_READ,
            kwallet_dbus_.RemoveEntry(123, "folder", "realm", "app", &ret));
}

TEST_P(KWalletDBusTest, RemoveEntryErrorContact) {
  EXPECT_CALL(
      *mock_kwallet_proxy_.get(),
      MockCallMethodAndBlock(Calls(kKWalletInterface, "removeEntry"), _))
      .WillOnce(Return(nullptr));

  int ret;
  EXPECT_EQ(KWalletDBus::Error::CANNOT_CONTACT,
            kwallet_dbus_.RemoveEntry(123, "folder", "realm", "app", &ret));
}

TEST_P(KWalletDBusTest, WriteEntry) {
  std::vector<uint8_t> bytes = {1, 2, 3, 1};
  EXPECT_CALL(
      *mock_kwallet_proxy_.get(),
      MockCallMethodAndBlock(AllOf(Calls(kKWalletInterface, "writeEntry"),
                                   ArgumentsAreIntStringStringBytesString(
                                       123, "folder", "realm", bytes, "app")),
                             _))
      .WillOnce(Return(RespondInt32(0)));

  int ret;
  EXPECT_EQ(KWalletDBus::Error::SUCCESS,
            kwallet_dbus_.WriteEntry(123, "folder", "realm", "app",
                                     bytes.data(), bytes.size(), &ret));
  EXPECT_EQ(0, ret);
}

TEST_P(KWalletDBusTest, WriteEntryErrorRead) {
  std::vector<uint8_t> bytes = {1, 2, 3, 1};
  EXPECT_CALL(*mock_kwallet_proxy_.get(),
              MockCallMethodAndBlock(Calls(kKWalletInterface, "writeEntry"), _))
      .WillOnce(Return(RespondEmpty()));

  int ret;
  EXPECT_EQ(KWalletDBus::Error::CANNOT_READ,
            kwallet_dbus_.WriteEntry(123, "folder", "realm", "app",
                                     bytes.data(), bytes.size(), &ret));
}

TEST_P(KWalletDBusTest, WriteEntryErrorContact) {
  std::vector<uint8_t> bytes = {1, 2, 3, 1};
  EXPECT_CALL(*mock_kwallet_proxy_.get(),
              MockCallMethodAndBlock(Calls(kKWalletInterface, "writeEntry"), _))
      .WillOnce(Return(nullptr));

  int ret;
  EXPECT_EQ(KWalletDBus::Error::CANNOT_CONTACT,
            kwallet_dbus_.WriteEntry(123, "folder", "realm", "app",
                                     bytes.data(), bytes.size(), &ret));
}

TEST_P(KWalletDBusTest, HasFolder) {
  EXPECT_CALL(*mock_kwallet_proxy_.get(),
              MockCallMethodAndBlock(
                  AllOf(Calls(kKWalletInterface, "hasFolder"),
                        ArgumentsAreIntStringString(123, "wallet", "app")),
                  _))
      .WillOnce(Return(RespondBool(true)));

  bool has_folder = false;
  EXPECT_EQ(KWalletDBus::Error::SUCCESS,
            kwallet_dbus_.HasFolder(123, "wallet", "app", &has_folder));
  EXPECT_EQ(true, has_folder);
}

TEST_P(KWalletDBusTest, HasFolderErrorRead) {
  EXPECT_CALL(*mock_kwallet_proxy_.get(),
              MockCallMethodAndBlock(Calls(kKWalletInterface, "hasFolder"), _))
      .WillOnce(Return(RespondEmpty()));

  bool has_folder = false;
  EXPECT_EQ(KWalletDBus::Error::CANNOT_READ,
            kwallet_dbus_.HasFolder(123, "wallet", "app", &has_folder));
}

TEST_P(KWalletDBusTest, HasFolderErrorContact) {
  EXPECT_CALL(*mock_kwallet_proxy_.get(),
              MockCallMethodAndBlock(Calls(kKWalletInterface, "hasFolder"), _))
      .WillOnce(Return(nullptr));

  bool has_folder = false;
  EXPECT_EQ(KWalletDBus::Error::CANNOT_CONTACT,
            kwallet_dbus_.HasFolder(123, "wallet", "app", &has_folder));
}

TEST_P(KWalletDBusTest, CreateFolder) {
  EXPECT_CALL(*mock_kwallet_proxy_.get(),
              MockCallMethodAndBlock(
                  AllOf(Calls(kKWalletInterface, "createFolder"),
                        ArgumentsAreIntStringString(123, "folder", "app")),
                  _))
      .WillOnce(Return(RespondBool(true)));

  bool created_folder = false;
  EXPECT_EQ(KWalletDBus::Error::SUCCESS,
            kwallet_dbus_.CreateFolder(123, "folder", "app", &created_folder));
  EXPECT_EQ(true, created_folder);
}

TEST_P(KWalletDBusTest, CreateFolderErrorRead) {
  EXPECT_CALL(
      *mock_kwallet_proxy_.get(),
      MockCallMethodAndBlock(Calls(kKWalletInterface, "createFolder"), _))
      .WillOnce(Return(RespondEmpty()));

  bool created_folder = false;
  EXPECT_EQ(KWalletDBus::Error::CANNOT_READ,
            kwallet_dbus_.CreateFolder(123, "folder", "app", &created_folder));
}

TEST_P(KWalletDBusTest, CreateFolderErrorContact) {
  EXPECT_CALL(
      *mock_kwallet_proxy_.get(),
      MockCallMethodAndBlock(Calls(kKWalletInterface, "createFolder"), _))
      .WillOnce(Return(nullptr));

  bool created_folder = false;
  EXPECT_EQ(KWalletDBus::Error::CANNOT_CONTACT,
            kwallet_dbus_.CreateFolder(123, "folder", "app", &created_folder));
}

TEST_P(KWalletDBusTest, WritePassword) {
  EXPECT_CALL(*mock_kwallet_proxy_.get(),
              MockCallMethodAndBlock(
                  AllOf(Calls(kKWalletInterface, "writePassword"),
                        ArgumentsAreIntStringStringStringString(
                            123, "folder", "key", "password", "app")),
                  _))
      .WillOnce(Return(RespondInt32(0)));

  bool write_success = false;
  EXPECT_EQ(KWalletDBus::Error::SUCCESS,
            kwallet_dbus_.WritePassword(123, "folder", "key", "password", "app",
                                        &write_success));
  EXPECT_TRUE(write_success);
}

TEST_P(KWalletDBusTest, WritePasswordRejected) {
  EXPECT_CALL(*mock_kwallet_proxy_.get(),
              MockCallMethodAndBlock(
                  AllOf(Calls(kKWalletInterface, "writePassword"),
                        ArgumentsAreIntStringStringStringString(
                            123, "folder", "key", "password", "app")),
                  _))
      .WillOnce(Return(RespondInt32(-1)));

  bool write_success = true;
  EXPECT_EQ(KWalletDBus::Error::SUCCESS,
            kwallet_dbus_.WritePassword(123, "folder", "key", "password", "app",
                                        &write_success));
  EXPECT_FALSE(write_success);
}

TEST_P(KWalletDBusTest, WritePasswordErrorRead) {
  EXPECT_CALL(
      *mock_kwallet_proxy_.get(),
      MockCallMethodAndBlock(Calls(kKWalletInterface, "writePassword"), _))
      .WillOnce(Return(RespondEmpty()));

  bool write_success = false;
  EXPECT_EQ(KWalletDBus::Error::CANNOT_READ,
            kwallet_dbus_.WritePassword(123, "folder", "key", "password", "app",
                                        &write_success));
}

TEST_P(KWalletDBusTest, WritePasswordErrorContact) {
  EXPECT_CALL(
      *mock_kwallet_proxy_.get(),
      MockCallMethodAndBlock(Calls(kKWalletInterface, "writePassword"), _))
      .WillOnce(Return(nullptr));

  bool write_success = false;
  EXPECT_EQ(KWalletDBus::Error::CANNOT_CONTACT,
            kwallet_dbus_.WritePassword(123, "folder", "key", "password", "app",
                                        &write_success));
}

TEST_P(KWalletDBusTest, ReadPassword) {
  EXPECT_CALL(
      *mock_kwallet_proxy_.get(),
      MockCallMethodAndBlock(
          AllOf(Calls(kKWalletInterface, "readPassword"),
                ArgumentsAreIntStringStringString(123, "folder", "key", "app")),
          _))
      .WillOnce(Return(RespondString("password")));

  std::string password;
  EXPECT_EQ(KWalletDBus::Error::SUCCESS,
            kwallet_dbus_.ReadPassword(123, "folder", "key", "app", &password));
  EXPECT_EQ("password", password);
}

TEST_P(KWalletDBusTest, ReadPasswordErrorRead) {
  EXPECT_CALL(
      *mock_kwallet_proxy_.get(),
      MockCallMethodAndBlock(Calls(kKWalletInterface, "readPassword"), _))
      .WillOnce(Return(RespondEmpty()));

  std::string password;
  EXPECT_EQ(KWalletDBus::Error::CANNOT_READ,
            kwallet_dbus_.ReadPassword(123, "folder", "key", "app", &password));
}

TEST_P(KWalletDBusTest, ReadPasswordErrorContact) {
  EXPECT_CALL(
      *mock_kwallet_proxy_.get(),
      MockCallMethodAndBlock(Calls(kKWalletInterface, "readPassword"), _))
      .WillOnce(Return(nullptr));

  std::string password;
  EXPECT_EQ(KWalletDBus::Error::CANNOT_CONTACT,
            kwallet_dbus_.ReadPassword(123, "folder", "key", "app", &password));
}

TEST_P(KWalletDBusTest, CloseSuccess) {
  EXPECT_CALL(*mock_kwallet_proxy_.get(),
              MockCallMethodAndBlock(
                  AllOf(Calls(kKWalletInterface, "close"),
                        ArgumentsAreIntBoolString(123, false, "app")),
                  _))
      .WillOnce(Return(RespondInt32(0)));

  bool success = false;
  EXPECT_EQ(KWalletDBus::Error::SUCCESS,
            kwallet_dbus_.Close(123, false, "app", &success));
  EXPECT_TRUE(success);
}

TEST_P(KWalletDBusTest, CloseUnsuccessful) {
  EXPECT_CALL(*mock_kwallet_proxy_.get(),
              MockCallMethodAndBlock(
                  AllOf(Calls(kKWalletInterface, "close"),
                        ArgumentsAreIntBoolString(123, false, "app")),
                  _))
      .WillOnce(Return(RespondInt32(1)));

  bool success = true;
  EXPECT_EQ(KWalletDBus::Error::SUCCESS,
            kwallet_dbus_.Close(123, false, "app", &success));
  EXPECT_FALSE(success);
}

TEST_P(KWalletDBusTest, CloseErrorRead) {
  EXPECT_CALL(*mock_kwallet_proxy_.get(),
              MockCallMethodAndBlock(Calls(kKWalletInterface, "close"), _))
      .WillOnce(Return(RespondEmpty()));

  bool success = true;
  EXPECT_EQ(KWalletDBus::Error::CANNOT_READ,
            kwallet_dbus_.Close(123, false, "app", &success));
}

TEST_P(KWalletDBusTest, CloseErrorContact) {
  EXPECT_CALL(*mock_kwallet_proxy_.get(),
              MockCallMethodAndBlock(Calls(kKWalletInterface, "close"), _))
      .WillOnce(Return(nullptr));

  bool success = true;
  EXPECT_EQ(KWalletDBus::Error::CANNOT_CONTACT,
            kwallet_dbus_.Close(123, false, "app", &success));
}

}  // namespace
