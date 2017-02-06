// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/macros.h"
#include "components/os_crypt/key_storage_libsecret.h"
#include "components/os_crypt/libsecret_util_linux.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Mock functions use MockSecretValue, where SecretValue would appear, and are
// cast to the correct signature. We can reduce SecretValue to an std::string,
// because we don't use anything else from it.
using MockSecretValue = std::string;

// Replaces some of LibsecretLoader's methods with mocked ones.
class MockLibsecretLoader : public LibsecretLoader {
 public:
  // Sets up the minimum mock implementation necessary for OSCrypt to work
  // with Libsecret. Also resets the state to mock a clean database.
  static bool ResetForOSCrypt();

  // Sets OSCrypt's password in the libsecret mock to a specific value
  static void SetOSCryptPassword(const char*);

  // Releases memory and restores LibsecretLoader to an uninitialized state.
  static void TearDown();

 private:
  // These methods are used to redirect calls through LibsecretLoader
  static const gchar* mock_secret_value_get_text(MockSecretValue* value);

  static gboolean mock_secret_password_store_sync(const SecretSchema* schema,
                                                  const gchar* collection,
                                                  const gchar* label,
                                                  const gchar* password,
                                                  GCancellable* cancellable,
                                                  GError** error,
                                                  ...);

  static MockSecretValue* mock_secret_service_lookup_sync(
      SecretService* service,
      const SecretSchema* schema,
      GHashTable* attributes,
      GCancellable* cancellable,
      GError** error);

  static void mock_secret_value_unref(gpointer value);

  static GList* mock_secret_service_search_sync(SecretService* service,
                                                const SecretSchema* schema,
                                                GHashTable* attributes,
                                                SecretSearchFlags flags,
                                                GCancellable* cancellable,
                                                GError** error);

  // MockLibsecretLoader owns this object.
  static MockSecretValue* stored_password_mock_ptr_;
};

MockSecretValue* MockLibsecretLoader::stored_password_mock_ptr_ = nullptr;

const gchar* MockLibsecretLoader::mock_secret_value_get_text(
    MockSecretValue* value) {
  return value->c_str();
}

// static
gboolean MockLibsecretLoader::mock_secret_password_store_sync(
    const SecretSchema* schema,
    const gchar* collection,
    const gchar* label,
    const gchar* password,
    GCancellable* cancellable,
    GError** error,
    ...) {
  delete stored_password_mock_ptr_;
  stored_password_mock_ptr_ = new MockSecretValue(password);
  return true;
}

// static
MockSecretValue* MockLibsecretLoader::mock_secret_service_lookup_sync(
    SecretService* service,
    const SecretSchema* schema,
    GHashTable* attributes,
    GCancellable* cancellable,
    GError** error) {
  return stored_password_mock_ptr_;
}

// static
void MockLibsecretLoader::mock_secret_value_unref(gpointer value) {}

// static
GList* MockLibsecretLoader::mock_secret_service_search_sync(
    SecretService* service,
    const SecretSchema* schema,
    GHashTable* attributes,
    SecretSearchFlags flags,
    GCancellable* cancellable,
    GError** error) {
  *error = nullptr;
  return nullptr;
}

// static
bool MockLibsecretLoader::ResetForOSCrypt() {
  // 4 methods used by KeyStorageLibsecret::GetKey()
  secret_password_store_sync =
      &MockLibsecretLoader::mock_secret_password_store_sync;
  secret_value_get_text = (decltype(&::secret_value_get_text)) &
                          MockLibsecretLoader::mock_secret_value_get_text;
  secret_value_unref = &MockLibsecretLoader::mock_secret_value_unref;
  secret_service_lookup_sync =
      (decltype(&::secret_service_lookup_sync)) &
      MockLibsecretLoader::mock_secret_service_lookup_sync;
  // 1 method used by LibsecretLoader::EnsureLibsecretLoaded()
  secret_service_search_sync =
      &MockLibsecretLoader::mock_secret_service_search_sync;

  delete stored_password_mock_ptr_;
  stored_password_mock_ptr_ = nullptr;
  libsecret_loaded_ = true;

  return true;
}

// static
void MockLibsecretLoader::SetOSCryptPassword(const char* value) {
  delete stored_password_mock_ptr_;
  stored_password_mock_ptr_ = new MockSecretValue(value);
}

// static
void MockLibsecretLoader::TearDown() {
  delete stored_password_mock_ptr_;
  stored_password_mock_ptr_ = nullptr;
  libsecret_loaded_ =
      false;  // Function pointers will be restored when loading.
}

class LibsecretTest : public testing::Test {
 public:
  LibsecretTest() = default;
  ~LibsecretTest() override = default;

  void SetUp() override { MockLibsecretLoader::ResetForOSCrypt(); }

  void TearDown() override { MockLibsecretLoader::TearDown(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(LibsecretTest);
};

TEST_F(LibsecretTest, LibsecretRepeats) {
  KeyStorageLibsecret libsecret;
  MockLibsecretLoader::ResetForOSCrypt();
  std::string password = libsecret.GetKey();
  EXPECT_FALSE(password.empty());
  std::string password_repeat = libsecret.GetKey();
  EXPECT_EQ(password, password_repeat);
}

TEST_F(LibsecretTest, LibsecretCreatesRandomised) {
  KeyStorageLibsecret libsecret;
  MockLibsecretLoader::ResetForOSCrypt();
  std::string password = libsecret.GetKey();
  MockLibsecretLoader::ResetForOSCrypt();
  std::string password_new = libsecret.GetKey();
  EXPECT_NE(password, password_new);
}

}  // namespace
