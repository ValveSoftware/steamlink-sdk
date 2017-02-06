// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LOGIN_DATABASE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LOGIN_DATABASE_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/pickle.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_change.h"
#include "components/password_manager/core/browser/psl_matching_helper.h"
#include "components/password_manager/core/browser/statistics_table.h"
#include "sql/connection.h"
#include "sql/meta_table.h"

#if defined(OS_IOS)
#include "base/gtest_prod_util.h"
#endif

namespace password_manager {

extern const int kCurrentVersionNumber;
extern const int kCompatibleVersionNumber;

// Interface to the database storage of login information, intended as a helper
// for PasswordStore on platforms that need internal storage of some or all of
// the login information.
class LoginDatabase {
 public:
  LoginDatabase(const base::FilePath& db_path);
  virtual ~LoginDatabase();

  // Actually creates/opens the database. If false is returned, no other method
  // should be called.
  virtual bool Init();

  // Reports usage metrics to UMA.
  void ReportMetrics(const std::string& sync_username,
                     bool custom_passphrase_sync_enabled);

  // Adds |form| to the list of remembered password forms. Returns the list of
  // changes applied ({}, {ADD}, {REMOVE, ADD}). If it returns {REMOVE, ADD}
  // then the REMOVE is associated with the form that was added. Thus only the
  // primary key columns contain the values associated with the removed form.
  PasswordStoreChangeList AddLogin(const autofill::PasswordForm& form)
      WARN_UNUSED_RESULT;

  // Updates existing password form. Returns the list of applied changes
  // ({}, {UPDATE}). The password is looked up by the tuple {origin,
  // username_element, username_value, password_element, signon_realm}.
  // These columns stay intact.
  PasswordStoreChangeList UpdateLogin(const autofill::PasswordForm& form)
      WARN_UNUSED_RESULT;

  // Removes |form| from the list of remembered password forms. Returns true if
  // |form| was successfully removed from the database.
  bool RemoveLogin(const autofill::PasswordForm& form) WARN_UNUSED_RESULT;

  // Removes all logins created from |delete_begin| onwards (inclusive) and
  // before |delete_end|. You may use a null Time value to do an unbounded
  // delete in either direction.
  bool RemoveLoginsCreatedBetween(base::Time delete_begin,
                                  base::Time delete_end);

  // Removes all logins synced from |delete_begin| onwards (inclusive) and
  // before |delete_end|. You may use a null Time value to do an unbounded
  // delete in either direction.
  bool RemoveLoginsSyncedBetween(base::Time delete_begin,
                                 base::Time delete_end);

  // Sets the 'skip_zero_click' flag to 'true' for all logins.
  bool DisableAutoSignInForAllLogins();

  // All Get* methods below overwrite |forms| with the returned credentials. On
  // success, those methods return true.

  // Gets a list of credentials matching |form|, including blacklisted matches
  // and federated credentials.
  bool GetLogins(const autofill::PasswordForm& form,
                 ScopedVector<autofill::PasswordForm>* forms) const
      WARN_UNUSED_RESULT;

  // Gets all logins created from |begin| onwards (inclusive) and before |end|.
  // You may use a null Time value to do an unbounded search in either
  // direction.
  bool GetLoginsCreatedBetween(
      base::Time begin,
      base::Time end,
      ScopedVector<autofill::PasswordForm>* forms) const WARN_UNUSED_RESULT;

  // Gets all logins synced from |begin| onwards (inclusive) and before |end|.
  // You may use a null Time value to do an unbounded search in either
  // direction.
  bool GetLoginsSyncedBetween(base::Time begin,
                              base::Time end,
                              ScopedVector<autofill::PasswordForm>* forms) const
      WARN_UNUSED_RESULT;

  // Gets the complete list of not blacklisted credentials.
  bool GetAutofillableLogins(ScopedVector<autofill::PasswordForm>* forms) const
      WARN_UNUSED_RESULT;

  // Gets the complete list of blacklisted credentials.
  bool GetBlacklistLogins(ScopedVector<autofill::PasswordForm>* forms) const
      WARN_UNUSED_RESULT;

  // Gets the list of auto-sign-inable credentials.
  bool GetAutoSignInLogins(ScopedVector<autofill::PasswordForm>* forms) const
      WARN_UNUSED_RESULT;

  // Deletes the login database file on disk, and creates a new, empty database.
  // This can be used after migrating passwords to some other store, to ensure
  // that SQLite doesn't leave fragments of passwords in the database file.
  // Returns true on success; otherwise, whether the file was deleted and
  // whether further use of this login database will succeed is unspecified.
  bool DeleteAndRecreateDatabaseFile();

  // Returns the encrypted password value for the specified |form|.  Returns an
  // empty string if the row for this |form| is not found.
  std::string GetEncryptedPassword(const autofill::PasswordForm& form) const;

  StatisticsTable& stats_table() { return stats_table_; }

  void set_clear_password_values(bool val) { clear_password_values_ = val; }

 private:
#if defined(OS_IOS)
  friend class LoginDatabaseIOSTest;
  FRIEND_TEST_ALL_PREFIXES(LoginDatabaseIOSTest, KeychainStorage);

  // On iOS, removes the keychain item that is used to store the
  // encrypted password for the supplied |form|.
  void DeleteEncryptedPassword(const autofill::PasswordForm& form);
#endif

  // Result values for encryption/decryption actions.
  enum EncryptionResult {
    // Success.
    ENCRYPTION_RESULT_SUCCESS,
    // Failure for a specific item (e.g., the encrypted value was manually
    // moved from another machine, and can't be decrypted on this machine).
    // This is presumed to be a permanent failure.
    ENCRYPTION_RESULT_ITEM_FAILURE,
    // A service-level failure (e.g., on a platform using a keyring, the keyring
    // is temporarily unavailable).
    // This is presumed to be a temporary failure.
    ENCRYPTION_RESULT_SERVICE_FAILURE,
  };

  // Encrypts plain_text, setting the value of cipher_text and returning true if
  // successful, or returning false and leaving cipher_text unchanged if
  // encryption fails (e.g., if the underlying OS encryption system is
  // temporarily unavailable).
  static EncryptionResult EncryptedString(const base::string16& plain_text,
                                          std::string* cipher_text);

  // Decrypts cipher_text, setting the value of plain_text and returning true if
  // successful, or returning false and leaving plain_text unchanged if
  // decryption fails (e.g., if the underlying OS encryption system is
  // temporarily unavailable).
  static EncryptionResult DecryptedString(const std::string& cipher_text,
                                          base::string16* plain_text);

  bool InitLoginsTable();
  bool MigrateOldVersionsAsNeeded();

  // Fills |form| from the values in the given statement (which is assumed to
  // be of the form used by the Get*Logins methods).
  // Returns the EncryptionResult from decrypting the password in |s|; if not
  // ENCRYPTION_RESULT_SUCCESS, |form| is not filled.
  static EncryptionResult InitPasswordFormFromStatement(
      autofill::PasswordForm* form,
      sql::Statement& s);

  // Gets all blacklisted or all non-blacklisted (depending on |blacklisted|)
  // credentials. On success returns true and overwrites |forms| with the
  // result.
  bool GetAllLoginsWithBlacklistSetting(
      bool blacklisted,
      ScopedVector<autofill::PasswordForm>* forms) const;

  // Overwrites |forms| with credentials retrieved from |statement|. If
  // |matched_form| is not null, filters out all results but those PSL-matching
  // |*matched_form| or federated credentials for it. On success returns true.
  static bool StatementToForms(sql::Statement* statement,
                               const autofill::PasswordForm* matched_form,
                               ScopedVector<autofill::PasswordForm>* forms);

  base::FilePath db_path_;
  mutable sql::Connection db_;
  sql::MetaTable meta_table_;
  StatisticsTable stats_table_;

  // If set to 'true', then the password values are cleared before encrypting
  // and storing in the database. At the same time AddLogin/UpdateLogin return
  // PasswordStoreChangeList containing the real password.
  // This is a temporary measure for migration the Keychain on Mac.
  // crbug.com/466638
  bool clear_password_values_;

  DISALLOW_COPY_AND_ASSIGN(LoginDatabase);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LOGIN_DATABASE_H_
