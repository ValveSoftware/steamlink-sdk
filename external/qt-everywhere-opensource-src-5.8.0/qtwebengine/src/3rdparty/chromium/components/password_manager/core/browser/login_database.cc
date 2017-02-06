// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/login_database.h"

#include <stddef.h>
#include <stdint.h>
#include <algorithm>
#include <limits>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/pickle.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/affiliation_utils.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "sql/connection.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "url/origin.h"
#include "url/url_constants.h"

using autofill::PasswordForm;

namespace password_manager {

// The current version number of the login database schema.
const int kCurrentVersionNumber = 17;
// The oldest version of the schema such that a legacy Chrome client using that
// version can still read/write the current database.
const int kCompatibleVersionNumber = 14;

base::Pickle SerializeVector(const std::vector<base::string16>& vec) {
  base::Pickle p;
  for (size_t i = 0; i < vec.size(); ++i) {
    p.WriteString16(vec[i]);
  }
  return p;
}

std::vector<base::string16> DeserializeVector(const base::Pickle& p) {
  std::vector<base::string16> ret;
  base::string16 str;

  base::PickleIterator iterator(p);
  while (iterator.ReadString16(&str)) {
    ret.push_back(str);
  }
  return ret;
}

namespace {

// Convenience enum for interacting with SQL queries that use all the columns.
enum LoginTableColumns {
  COLUMN_ORIGIN_URL = 0,
  COLUMN_ACTION_URL,
  COLUMN_USERNAME_ELEMENT,
  COLUMN_USERNAME_VALUE,
  COLUMN_PASSWORD_ELEMENT,
  COLUMN_PASSWORD_VALUE,
  COLUMN_SUBMIT_ELEMENT,
  COLUMN_SIGNON_REALM,
  COLUMN_SSL_VALID,
  COLUMN_PREFERRED,
  COLUMN_DATE_CREATED,
  COLUMN_BLACKLISTED_BY_USER,
  COLUMN_SCHEME,
  COLUMN_PASSWORD_TYPE,
  COLUMN_POSSIBLE_USERNAMES,
  COLUMN_TIMES_USED,
  COLUMN_FORM_DATA,
  COLUMN_DATE_SYNCED,
  COLUMN_DISPLAY_NAME,
  COLUMN_ICON_URL,
  COLUMN_FEDERATION_URL,
  COLUMN_SKIP_ZERO_CLICK,
  COLUMN_GENERATION_UPLOAD_STATUS,
};

enum class HistogramSize { SMALL, LARGE };

// An enum for UMA reporting. Add values to the end only.
enum DatabaseInitError {
  INIT_OK,
  OPEN_FILE_ERROR,
  START_TRANSACTION_ERROR,
  META_TABLE_INIT_ERROR,
  INCOMPATIBLE_VERSION,
  INIT_LOGINS_ERROR,
  INIT_STATS_ERROR,
  MIGRATION_ERROR,
  COMMIT_TRANSACTION_ERROR,

  DATABASE_INIT_ERROR_COUNT,
};

void BindAddStatement(const PasswordForm& form,
                      const std::string& encrypted_password,
                      sql::Statement* s) {
  s->BindString(COLUMN_ORIGIN_URL, form.origin.spec());
  s->BindString(COLUMN_ACTION_URL, form.action.spec());
  s->BindString16(COLUMN_USERNAME_ELEMENT, form.username_element);
  s->BindString16(COLUMN_USERNAME_VALUE, form.username_value);
  s->BindString16(COLUMN_PASSWORD_ELEMENT, form.password_element);
  s->BindBlob(COLUMN_PASSWORD_VALUE, encrypted_password.data(),
              static_cast<int>(encrypted_password.length()));
  s->BindString16(COLUMN_SUBMIT_ELEMENT, form.submit_element);
  s->BindString(COLUMN_SIGNON_REALM, form.signon_realm);
  s->BindInt(COLUMN_SSL_VALID, form.ssl_valid);
  s->BindInt(COLUMN_PREFERRED, form.preferred);
  s->BindInt64(COLUMN_DATE_CREATED, form.date_created.ToInternalValue());
  s->BindInt(COLUMN_BLACKLISTED_BY_USER, form.blacklisted_by_user);
  s->BindInt(COLUMN_SCHEME, form.scheme);
  s->BindInt(COLUMN_PASSWORD_TYPE, form.type);
  base::Pickle usernames_pickle =
      SerializeVector(form.other_possible_usernames);
  s->BindBlob(COLUMN_POSSIBLE_USERNAMES,
              usernames_pickle.data(),
              usernames_pickle.size());
  s->BindInt(COLUMN_TIMES_USED, form.times_used);
  base::Pickle form_data_pickle;
  autofill::SerializeFormData(form.form_data, &form_data_pickle);
  s->BindBlob(COLUMN_FORM_DATA,
              form_data_pickle.data(),
              form_data_pickle.size());
  s->BindInt64(COLUMN_DATE_SYNCED, form.date_synced.ToInternalValue());
  s->BindString16(COLUMN_DISPLAY_NAME, form.display_name);
  s->BindString(COLUMN_ICON_URL, form.icon_url.spec());
  // An empty Origin serializes as "null" which would be strange to store here.
  s->BindString(COLUMN_FEDERATION_URL,
                form.federation_origin.unique()
                    ? std::string()
                    : form.federation_origin.Serialize());
  s->BindInt(COLUMN_SKIP_ZERO_CLICK, form.skip_zero_click);
  s->BindInt(COLUMN_GENERATION_UPLOAD_STATUS, form.generation_upload_status);
}

void AddCallback(int err, sql::Statement* /*stmt*/) {
  if (err == 19 /*SQLITE_CONSTRAINT*/)
    DLOG(WARNING) << "LoginDatabase::AddLogin updated an existing form";
}

bool DoesMatchConstraints(const PasswordForm& form) {
  if (!IsValidAndroidFacetURI(form.signon_realm) && form.origin.is_empty()) {
    DLOG(ERROR) << "Constraint violation: form.origin is empty";
    return false;
  }
  if (form.signon_realm.empty()) {
    DLOG(ERROR) << "Constraint violation: form.signon_realm is empty";
    return false;
  }
  return true;
}

void LogDatabaseInitError(DatabaseInitError error) {
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.LoginDatabaseInit", error,
                            DATABASE_INIT_ERROR_COUNT);
}

// UMA_* macros assume that the name never changes. This is a helper function
// where this assumption doesn't hold.
void LogDynamicUMAStat(const std::string& name,
                       int sample,
                       int min,
                       int max,
                       int bucket_count) {
  base::HistogramBase* counter = base::Histogram::FactoryGet(
      name, min, max, bucket_count,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  counter->Add(sample);
}

void LogAccountStat(const std::string& name, int sample) {
  LogDynamicUMAStat(name, sample, 0, 32, 6);
}

void LogTimesUsedStat(const std::string& name, int sample) {
  LogDynamicUMAStat(name, sample, 0, 100, 10);
}

void LogNumberOfAccountsForScheme(const std::string& scheme, int sample) {
  LogDynamicUMAStat("PasswordManager.TotalAccountsHiRes.WithScheme." + scheme,
                    sample, 1, 1000, 100);
}

void LogNumberOfAccountsReusingPassword(const std::string& suffix,
                                        int sample,
                                        HistogramSize histogram_size) {
  int max = histogram_size == HistogramSize::LARGE ? 500 : 100;
  int bucket_count = histogram_size == HistogramSize::LARGE ? 50 : 20;
  LogDynamicUMAStat("PasswordManager.AccountsReusingPassword." + suffix, sample,
                    1, max, bucket_count);
}

// Records password reuse metrics given the |signon_realms| corresponding to a
// set of accounts that reuse the same password. See histograms.xml for details.
void LogPasswordReuseMetrics(const std::vector<std::string>& signon_realms) {
  struct StatisticsPerScheme {
    StatisticsPerScheme() : num_total_accounts(0) {}

    // The number of accounts for each registry controlled domain.
    std::map<std::string, int> num_accounts_per_registry_controlled_domain;

    // The number of accounts for each domain.
    std::map<std::string, int> num_accounts_per_domain;

    // Total number of accounts with this scheme. This equals the sum of counts
    // in either of the above maps.
    int num_total_accounts;
  };

  // The scheme (i.e. protocol) of the origin, not PasswordForm::scheme.
  enum Scheme { SCHEME_HTTP, SCHEME_HTTPS };
  const Scheme kAllSchemes[] = {SCHEME_HTTP, SCHEME_HTTPS};

  StatisticsPerScheme statistics[arraysize(kAllSchemes)];
  std::map<std::string, std::string> domain_to_registry_controlled_domain;

  for (const std::string& signon_realm : signon_realms) {
    const GURL signon_realm_url(signon_realm);
    const std::string domain = signon_realm_url.host();
    if (domain.empty())
      continue;

    if (!domain_to_registry_controlled_domain.count(domain)) {
      domain_to_registry_controlled_domain[domain] =
          GetRegistryControlledDomain(signon_realm_url);
      if (domain_to_registry_controlled_domain[domain].empty())
        domain_to_registry_controlled_domain[domain] = domain;
    }
    const std::string& registry_controlled_domain =
        domain_to_registry_controlled_domain[domain];

    Scheme scheme = SCHEME_HTTP;
    static_assert(arraysize(kAllSchemes) == 2, "Update this logic");
    if (signon_realm_url.SchemeIs(url::kHttpsScheme))
      scheme = SCHEME_HTTPS;
    else if (!signon_realm_url.SchemeIs(url::kHttpScheme))
      continue;

    statistics[scheme].num_accounts_per_domain[domain]++;
    statistics[scheme].num_accounts_per_registry_controlled_domain
        [registry_controlled_domain]++;
    statistics[scheme].num_total_accounts++;
  }

  // For each "source" account of either scheme, count the number of "target"
  // accounts reusing the same password (of either scheme).
  for (const Scheme scheme : kAllSchemes) {
    for (const auto& kv : statistics[scheme].num_accounts_per_domain) {
      const std::string& domain(kv.first);
      const int num_accounts_per_domain(kv.second);
      const std::string& registry_controlled_domain =
          domain_to_registry_controlled_domain[domain];

      Scheme other_scheme = scheme == SCHEME_HTTP ? SCHEME_HTTPS : SCHEME_HTTP;
      static_assert(arraysize(kAllSchemes) == 2, "Update |other_scheme|");

      // Discount the account at hand from the number of accounts with the same
      // domain and scheme.
      int num_accounts_for_same_domain[arraysize(kAllSchemes)] = {};
      num_accounts_for_same_domain[scheme] =
          statistics[scheme].num_accounts_per_domain[domain] - 1;
      num_accounts_for_same_domain[other_scheme] =
          statistics[other_scheme].num_accounts_per_domain[domain];

      // By definition, a PSL match requires the scheme to be the same.
      int num_psl_matching_accounts =
          statistics[scheme].num_accounts_per_registry_controlled_domain
              [registry_controlled_domain] -
          statistics[scheme].num_accounts_per_domain[domain];

      // Discount PSL matches from the number of accounts with different domains
      // but the same scheme.
      int num_accounts_for_different_domain[arraysize(kAllSchemes)] = {};
      num_accounts_for_different_domain[scheme] =
          statistics[scheme].num_total_accounts -
          statistics[scheme].num_accounts_per_registry_controlled_domain
              [registry_controlled_domain];
      num_accounts_for_different_domain[other_scheme] =
          statistics[other_scheme].num_total_accounts -
          statistics[other_scheme].num_accounts_per_domain[domain];

      std::string source_realm_kind =
          scheme == SCHEME_HTTP ? "FromHttpRealm" : "FromHttpsRealm";
      static_assert(arraysize(kAllSchemes) == 2, "Update |source_realm_kind|");

      // So far, the calculation has been carried out once per "source" domain,
      // but the metrics need to be recorded on a per-account basis. The set of
      // metrics are the same for all accounts for the same domain, so simply
      // report them as many times as accounts.
      for (int i = 0; i < num_accounts_per_domain; ++i) {
        LogNumberOfAccountsReusingPassword(
            source_realm_kind + ".OnHttpRealmWithSameHost",
            num_accounts_for_same_domain[SCHEME_HTTP], HistogramSize::SMALL);
        LogNumberOfAccountsReusingPassword(
            source_realm_kind + ".OnHttpsRealmWithSameHost",
            num_accounts_for_same_domain[SCHEME_HTTPS], HistogramSize::SMALL);
        LogNumberOfAccountsReusingPassword(
            source_realm_kind + ".OnPSLMatchingRealm",
            num_psl_matching_accounts, HistogramSize::SMALL);

        LogNumberOfAccountsReusingPassword(
            source_realm_kind + ".OnHttpRealmWithDifferentHost",
            num_accounts_for_different_domain[SCHEME_HTTP],
            HistogramSize::LARGE);
        LogNumberOfAccountsReusingPassword(
            source_realm_kind + ".OnHttpsRealmWithDifferentHost",
            num_accounts_for_different_domain[SCHEME_HTTPS],
            HistogramSize::LARGE);

        LogNumberOfAccountsReusingPassword(
            source_realm_kind + ".OnAnyRealmWithDifferentHost",
            num_accounts_for_different_domain[SCHEME_HTTP] +
                num_accounts_for_different_domain[SCHEME_HTTPS],
            HistogramSize::LARGE);
      }
    }
  }
}

// Creates a table named |table_name| using our current schema.
bool CreateNewTable(sql::Connection* db,
                    const char* table_name,
                    const char* extra_columns) {
  std::string query = base::StringPrintf(
      "CREATE TABLE %s ("
      "origin_url VARCHAR NOT NULL, "
      "action_url VARCHAR, "
      "username_element VARCHAR, "
      "username_value VARCHAR, "
      "password_element VARCHAR, "
      "password_value BLOB, "
      "submit_element VARCHAR, "
      "signon_realm VARCHAR NOT NULL,"
      "ssl_valid INTEGER NOT NULL,"
      "preferred INTEGER NOT NULL,"
      "date_created INTEGER NOT NULL,"
      "blacklisted_by_user INTEGER NOT NULL,"
      "scheme INTEGER NOT NULL,"
      "password_type INTEGER,"
      "possible_usernames BLOB,"
      "times_used INTEGER,"
      "form_data BLOB,"
      "date_synced INTEGER,"
      "display_name VARCHAR,"
      "icon_url VARCHAR,"
      "federation_url VARCHAR,"
      "skip_zero_click INTEGER,"
      "%s"
      "UNIQUE (origin_url, username_element, username_value, "
      "password_element, signon_realm))",
      table_name, extra_columns);
  return db->Execute(query.c_str());
}

bool CreateIndexOnSignonRealm(sql::Connection* db, const char* table_name) {
  std::string query = base::StringPrintf(
      "CREATE INDEX logins_signon ON %s (signon_realm)", table_name);
  return db->Execute(query.c_str());
}

}  // namespace

LoginDatabase::LoginDatabase(const base::FilePath& db_path)
    : db_path_(db_path), clear_password_values_(false) {
}

LoginDatabase::~LoginDatabase() {
}

bool LoginDatabase::Init() {
  // Set pragmas for a small, private database (based on WebDatabase).
  db_.set_page_size(2048);
  db_.set_cache_size(32);
  db_.set_exclusive_locking();
  db_.set_restrict_to_user();
  db_.set_histogram_tag("Passwords");

  if (!db_.Open(db_path_)) {
    LogDatabaseInitError(OPEN_FILE_ERROR);
    LOG(ERROR) << "Unable to open the password store database.";
    return false;
  }

  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    LogDatabaseInitError(START_TRANSACTION_ERROR);
    LOG(ERROR) << "Unable to start a transaction.";
    db_.Close();
    return false;
  }

  // Check the database version.
  if (!meta_table_.Init(&db_, kCurrentVersionNumber,
                        kCompatibleVersionNumber)) {
    LogDatabaseInitError(META_TABLE_INIT_ERROR);
    LOG(ERROR) << "Unable to create the meta table.";
    db_.Close();
    return false;
  }
  if (meta_table_.GetCompatibleVersionNumber() > kCurrentVersionNumber) {
    LogDatabaseInitError(INCOMPATIBLE_VERSION);
    LOG(ERROR) << "Password store database is too new, kCurrentVersionNumber="
               << kCurrentVersionNumber << ", GetCompatibleVersionNumber="
               << meta_table_.GetCompatibleVersionNumber();
    db_.Close();
    return false;
  }

  // Initialize the tables.
  if (!InitLoginsTable()) {
    LogDatabaseInitError(INIT_LOGINS_ERROR);
    LOG(ERROR) << "Unable to initialize the logins table.";
    db_.Close();
    return false;
  }
  stats_table_.Init(&db_);

  // If the file on disk is an older database version, bring it up to date.
  if (meta_table_.GetVersionNumber() < kCurrentVersionNumber &&
      !MigrateOldVersionsAsNeeded()) {
    LogDatabaseInitError(MIGRATION_ERROR);
    UMA_HISTOGRAM_SPARSE_SLOWLY("PasswordManager.LoginDatabaseFailedVersion",
                                meta_table_.GetVersionNumber());
    LOG(ERROR) << "Unable to migrate database from "
               << meta_table_.GetVersionNumber() << " to "
               << kCurrentVersionNumber;
    db_.Close();
    return false;
  }

  if (!stats_table_.CreateTableIfNecessary()) {
    LogDatabaseInitError(INIT_STATS_ERROR);
    LOG(ERROR) << "Unable to create the stats table.";
    db_.Close();
    return false;
  }

  if (!transaction.Commit()) {
    LogDatabaseInitError(COMMIT_TRANSACTION_ERROR);
    LOG(ERROR) << "Unable to commit a transaction.";
    db_.Close();
    return false;
  }

  LogDatabaseInitError(INIT_OK);
  return true;
}

bool LoginDatabase::MigrateOldVersionsAsNeeded() {
  const int original_version = meta_table_.GetVersionNumber();
  switch (original_version) {
    case 1:
      // Column could exist because of https://crbug.com/295851
      if (!db_.DoesColumnExist("logins", "password_type") &&
          !db_.Execute("ALTER TABLE logins "
                       "ADD COLUMN password_type INTEGER")) {
        return false;
      }
      if (!db_.DoesColumnExist("logins", "possible_usernames") &&
          !db_.Execute("ALTER TABLE logins "
                       "ADD COLUMN possible_usernames BLOB")) {
        return false;
      }
    // Fall through.
    case 2:
      // Column could exist because of https://crbug.com/295851
      if (!db_.DoesColumnExist("logins", "times_used") &&
          !db_.Execute("ALTER TABLE logins ADD COLUMN times_used INTEGER")) {
        return false;
      }
    // Fall through.
    case 3:
      // Column could exist because of https://crbug.com/295851
      if (!db_.DoesColumnExist("logins", "form_data") &&
          !db_.Execute("ALTER TABLE logins ADD COLUMN form_data BLOB")) {
        return false;
      }
    // Fall through.
    case 4:
      if (!db_.Execute(
              "ALTER TABLE logins ADD COLUMN use_additional_auth INTEGER")) {
        return false;
      }
    // Fall through.
    case 5:
      if (!db_.Execute("ALTER TABLE logins ADD COLUMN date_synced INTEGER")) {
        return false;
      }
    // Fall through.
    case 6:
      if (!db_.Execute("ALTER TABLE logins ADD COLUMN display_name VARCHAR") ||
          !db_.Execute("ALTER TABLE logins ADD COLUMN avatar_url VARCHAR") ||
          !db_.Execute("ALTER TABLE logins "
                       "ADD COLUMN federation_url VARCHAR") ||
          !db_.Execute("ALTER TABLE logins ADD COLUMN is_zero_click INTEGER")) {
        return false;
      }
    // Fall through.
    case 7: {
      // Keep version 8 around even though no changes are made. See
      // crbug.com/423716 for context.
      // Fall through.
    }
    case 8: {
      sql::Statement s;
      s.Assign(db_.GetCachedStatement(SQL_FROM_HERE,
                                      "UPDATE logins SET "
                                      "date_created = "
                                      "(date_created * ?) + ?"));
      s.BindInt64(0, base::Time::kMicrosecondsPerSecond);
      s.BindInt64(1, base::Time::kTimeTToMicrosecondsOffset);
      if (!s.Run())
        return false;
      // Fall through.
    }
    case 9: {
      // Remove use_additional_auth column from database schema
      // crbug.com/423716 for context.
      std::string fields_to_copy =
          "origin_url, action_url, username_element, username_value, "
          "password_element, password_value, submit_element, "
          "signon_realm, ssl_valid, preferred, date_created, "
          "blacklisted_by_user, scheme, password_type, possible_usernames, "
          "times_used, form_data, date_synced, display_name, avatar_url, "
          "federation_url, is_zero_click";
      auto copy_data_query =
          [&fields_to_copy](const std::string& from, const std::string& to) {
        return "INSERT INTO " + to + " SELECT " + fields_to_copy + " FROM " +
               from;
      };

      if (!db_.Execute(("CREATE TEMPORARY TABLE logins_data(" + fields_to_copy +
                        ")").c_str()) ||
          !db_.Execute(copy_data_query("logins", "logins_data").c_str()) ||
          !db_.Execute("DROP TABLE logins") ||
          !db_.Execute(
              ("CREATE TABLE logins(" + fields_to_copy + ")").c_str()) ||
          !db_.Execute(copy_data_query("logins_data", "logins").c_str()) ||
          !db_.Execute("DROP TABLE logins_data") ||
          !CreateIndexOnSignonRealm(&db_, "logins")) {
        return false;
      }
      // Fall through.
    }
    case 10: {
      // Rename is_zero_click -> skip_zero_click. Note that previous versions
      // may have incorrectly used a 6-column key (origin_url, username_element,
      // username_value, password_element, signon_realm, submit_element).
      // In that case, this step also restores the correct 5-column key;
      // that is, the above without "submit_element".
      const char copy_query[] = "INSERT OR REPLACE INTO logins_new SELECT "
          "origin_url, action_url, username_element, username_value, "
          "password_element, password_value, submit_element, signon_realm, "
          "ssl_valid, preferred, date_created, blacklisted_by_user, scheme, "
          "password_type, possible_usernames, times_used, form_data, "
          "date_synced, display_name, avatar_url, federation_url, is_zero_click"
          " FROM logins";
      if (!CreateNewTable(&db_, "logins_new", "") ||
          !db_.Execute(copy_query) ||
          !db_.Execute("DROP TABLE logins") ||
          !db_.Execute("ALTER TABLE logins_new RENAME TO logins") ||
          !CreateIndexOnSignonRealm(&db_, "logins")) {
        return false;
      }
      // Fall through.
    }
    case 11:
      if (!db_.Execute(
              "ALTER TABLE logins ADD COLUMN "
              "generation_upload_status INTEGER"))
        return false;
      // Fall through.
    case 12:
      // The stats table was added. Nothing to do really.
      // Fall through.
    case 13: {
      // Rename avatar_url -> icon_url. Note that if the original version was
      // at most 10, this renaming would have already happened in step 10,
      // as |CreateNewTable| would create a table with the new column name.
      if (original_version > 10) {
        const char copy_query[] = "INSERT OR REPLACE INTO logins_new SELECT "
            "origin_url, action_url, username_element, username_value, "
            "password_element, password_value, submit_element, signon_realm, "
            "ssl_valid, preferred, date_created, blacklisted_by_user, scheme, "
            "password_type, possible_usernames, times_used, form_data, "
            "date_synced, display_name, avatar_url, federation_url, "
            "skip_zero_click, generation_upload_status FROM logins";
        if (!CreateNewTable(
                &db_, "logins_new", "generation_upload_status INTEGER,") ||
            !db_.Execute(copy_query) ||
            !db_.Execute("DROP TABLE logins") ||
            !db_.Execute("ALTER TABLE logins_new RENAME TO logins") ||
            !CreateIndexOnSignonRealm(&db_, "logins")) {
          return false;
        }
      }
      // Fall through.
    }
    case 14:
      // No change of schema. Version 15 was introduced to force all databases
      // through an otherwise no-op migration process that will, however, now
      // correctly set the 'compatible version number'. Previously, it was
      // always being set to (and forever left at) version 1.
      meta_table_.SetCompatibleVersionNumber(kCompatibleVersionNumber);
    case 15:
      // Recreate the statistics.
      if (!stats_table_.MigrateToVersion(16))
        return false;
    case 16: {
      // No change in scheme: just disable auto sign-in by default in
      // preparation to launch the credential management API.
      if (!db_.Execute("UPDATE logins SET skip_zero_click = 1"))
        return false;
      // Fall through.
    }

    // -------------------------------------------------------------------------
    // DO NOT FORGET to update |kCompatibleVersionNumber| if you add a migration
    // step that is a breaking change. This is needed so that an older version
    // of the browser can fail with a meaningful error when opening a newer
    // database, as opposed to failing on the first database operation.
    // -------------------------------------------------------------------------
    case kCurrentVersionNumber:
      // Already up to date.
      meta_table_.SetVersionNumber(kCurrentVersionNumber);
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

bool LoginDatabase::InitLoginsTable() {
  if (!db_.DoesTableExist("logins")) {
    if (!CreateNewTable(&db_, "logins", "generation_upload_status INTEGER,")) {
      NOTREACHED();
      return false;
    }
    if (!CreateIndexOnSignonRealm(&db_, "logins")) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

void LoginDatabase::ReportMetrics(const std::string& sync_username,
                                  bool custom_passphrase_sync_enabled) {
  sql::Statement s(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT signon_realm, password_type, blacklisted_by_user,"
      "COUNT(username_value) FROM logins GROUP BY "
      "signon_realm, password_type, blacklisted_by_user"));

  if (!s.is_valid())
    return;

  std::string custom_passphrase = "WithoutCustomPassphrase";
  if (custom_passphrase_sync_enabled) {
    custom_passphrase = "WithCustomPassphrase";
  }

  int total_user_created_accounts = 0;
  int total_generated_accounts = 0;
  int blacklisted_sites = 0;
  while (s.Step()) {
    PasswordForm::Type password_type =
        static_cast<PasswordForm::Type>(s.ColumnInt(1));
    int blacklisted = s.ColumnInt(2);
    int accounts_per_site = s.ColumnInt(3);
    if (blacklisted) {
      ++blacklisted_sites;
    } else if (password_type == PasswordForm::TYPE_GENERATED) {
      total_generated_accounts += accounts_per_site;
      LogAccountStat(
          base::StringPrintf("PasswordManager.AccountsPerSite.AutoGenerated.%s",
                             custom_passphrase.c_str()),
          accounts_per_site);
    } else {
      total_user_created_accounts += accounts_per_site;
      LogAccountStat(
          base::StringPrintf("PasswordManager.AccountsPerSite.UserCreated.%s",
                             custom_passphrase.c_str()),
          accounts_per_site);
    }
  }
  LogAccountStat(
      base::StringPrintf("PasswordManager.TotalAccounts.UserCreated.%s",
                         custom_passphrase.c_str()),
      total_user_created_accounts);
  LogAccountStat(
      base::StringPrintf("PasswordManager.TotalAccounts.AutoGenerated.%s",
                         custom_passphrase.c_str()),
      total_generated_accounts);
  LogAccountStat(base::StringPrintf("PasswordManager.BlacklistedSites.%s",
                                    custom_passphrase.c_str()),
                 blacklisted_sites);

  sql::Statement usage_statement(db_.GetCachedStatement(
      SQL_FROM_HERE, "SELECT password_type, times_used FROM logins"));

  if (!usage_statement.is_valid())
    return;

  while (usage_statement.Step()) {
    PasswordForm::Type type =
        static_cast<PasswordForm::Type>(usage_statement.ColumnInt(0));

    if (type == PasswordForm::TYPE_GENERATED) {
      LogTimesUsedStat(base::StringPrintf(
                           "PasswordManager.TimesPasswordUsed.AutoGenerated.%s",
                           custom_passphrase.c_str()),
                       usage_statement.ColumnInt(1));
    } else {
      LogTimesUsedStat(
          base::StringPrintf("PasswordManager.TimesPasswordUsed.UserCreated.%s",
                             custom_passphrase.c_str()),
          usage_statement.ColumnInt(1));
    }
  }

  bool syncing_account_saved = false;
  if (!sync_username.empty()) {
    sql::Statement sync_statement(db_.GetCachedStatement(
        SQL_FROM_HERE,
        "SELECT username_value FROM logins "
        "WHERE signon_realm == ?"));
    sync_statement.BindString(
        0, GaiaUrls::GetInstance()->gaia_url().GetOrigin().spec());

    if (!sync_statement.is_valid())
      return;

    while (sync_statement.Step()) {
      std::string username = sync_statement.ColumnString(0);
      if (gaia::AreEmailsSame(sync_username, username)) {
        syncing_account_saved = true;
        break;
      }
    }
  }
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.SyncingAccountState",
                            2 * sync_username.empty() + syncing_account_saved,
                            4);

  sql::Statement empty_usernames_statement(db_.GetCachedStatement(
      SQL_FROM_HERE, "SELECT COUNT(*) FROM logins "
                     "WHERE blacklisted_by_user=0 AND username_value=''"));
  if (empty_usernames_statement.Step()) {
    int empty_forms = empty_usernames_statement.ColumnInt(0);
    UMA_HISTOGRAM_COUNTS_100("PasswordManager.EmptyUsernames.CountInDatabase",
                             empty_forms);
  }

  sql::Statement standalone_empty_usernames_statement(db_.GetCachedStatement(
      SQL_FROM_HERE, "SELECT COUNT(*) FROM logins a "
                     "WHERE a.blacklisted_by_user=0 AND a.username_value='' "
                     "AND NOT EXISTS (SELECT * FROM logins b "
                     "WHERE b.blacklisted_by_user=0 AND b.username_value!='' "
                     "AND a.signon_realm = b.signon_realm)"));
  if (standalone_empty_usernames_statement.Step()) {
    int num_entries = standalone_empty_usernames_statement.ColumnInt(0);
    UMA_HISTOGRAM_COUNTS_100(
        "PasswordManager.EmptyUsernames.WithoutCorrespondingNonempty",
        num_entries);
  }

  sql::Statement logins_with_schemes_statement(db_.GetUniqueStatement(
      "SELECT signon_realm, origin_url, ssl_valid, blacklisted_by_user "
      "FROM logins;"));

  if (!logins_with_schemes_statement.is_valid())
    return;

  int android_logins = 0;
  int ftp_logins = 0;
  int http_logins = 0;
  int https_logins = 0;
  int other_logins = 0;

  while (logins_with_schemes_statement.Step()) {
    std::string signon_realm = logins_with_schemes_statement.ColumnString(0);
    GURL origin_url = GURL(logins_with_schemes_statement.ColumnString(1));
    bool ssl_valid = !!logins_with_schemes_statement.ColumnInt(2);
    bool blacklisted_by_user = !!logins_with_schemes_statement.ColumnInt(3);
    if (blacklisted_by_user)
      continue;

    if (IsValidAndroidFacetURI(signon_realm)) {
      ++android_logins;
    } else if (origin_url.SchemeIs(url::kHttpsScheme)) {
      ++https_logins;
      metrics_util::LogUMAHistogramBoolean(
          "PasswordManager.UserStoredPasswordWithInvalidSSLCert", !ssl_valid);
    } else if (origin_url.SchemeIs(url::kHttpScheme)) {
      ++http_logins;
    } else if (origin_url.SchemeIs(url::kFtpScheme)) {
      ++ftp_logins;
    } else {
      ++other_logins;
    }
  }

  LogNumberOfAccountsForScheme("Android", android_logins);
  LogNumberOfAccountsForScheme("Ftp", ftp_logins);
  LogNumberOfAccountsForScheme("Http", http_logins);
  LogNumberOfAccountsForScheme("Https", https_logins);
  LogNumberOfAccountsForScheme("Other", other_logins);

  sql::Statement form_based_passwords_statement(
      db_.GetUniqueStatement("SELECT signon_realm, password_value FROM logins "
                             "WHERE blacklisted_by_user = 0 AND scheme = 0"));

  std::map<base::string16, std::vector<std::string>> passwords_to_realms;
  while (form_based_passwords_statement.Step()) {
    std::string signon_realm = form_based_passwords_statement.ColumnString(0);
    base::string16 decrypted_password;
    // Note that CryptProtectData() is non-deterministic, so passwords must be
    // decrypted before checking equality.
    if (!IsValidAndroidFacetURI(signon_realm) &&
        DecryptedString(form_based_passwords_statement.ColumnString(1),
                        &decrypted_password) == ENCRYPTION_RESULT_SUCCESS) {
      passwords_to_realms[decrypted_password].push_back(signon_realm);
    }
  }

  for (const auto& password_to_realms : passwords_to_realms)
    LogPasswordReuseMetrics(password_to_realms.second);
}

PasswordStoreChangeList LoginDatabase::AddLogin(const PasswordForm& form) {
  PasswordStoreChangeList list;
  if (!DoesMatchConstraints(form))
    return list;
  std::string encrypted_password;
  if (EncryptedString(
          clear_password_values_ ? base::string16() : form.password_value,
          &encrypted_password) != ENCRYPTION_RESULT_SUCCESS)
    return list;

  // You *must* change LoginTableColumns if this query changes.
  sql::Statement s(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO logins "
      "(origin_url, action_url, username_element, username_value, "
      " password_element, password_value, submit_element, "
      " signon_realm, ssl_valid, preferred, date_created, blacklisted_by_user, "
      " scheme, password_type, possible_usernames, times_used, form_data, "
      " date_synced, display_name, icon_url,"
      " federation_url, skip_zero_click, generation_upload_status) VALUES "
      "(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
  BindAddStatement(form, encrypted_password, &s);
  db_.set_error_callback(base::Bind(&AddCallback));
  const bool success = s.Run();
  db_.reset_error_callback();
  if (success) {
    list.push_back(PasswordStoreChange(PasswordStoreChange::ADD, form));
    return list;
  }
  // Repeat the same statement but with REPLACE semantic.
  s.Assign(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT OR REPLACE INTO logins "
      "(origin_url, action_url, username_element, username_value, "
      " password_element, password_value, submit_element, "
      " signon_realm, ssl_valid, preferred, date_created, blacklisted_by_user, "
      " scheme, password_type, possible_usernames, times_used, form_data, "
      " date_synced, display_name, icon_url,"
      " federation_url, skip_zero_click, generation_upload_status) VALUES "
      "(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
  BindAddStatement(form, encrypted_password, &s);
  if (s.Run()) {
    list.push_back(PasswordStoreChange(PasswordStoreChange::REMOVE, form));
    list.push_back(PasswordStoreChange(PasswordStoreChange::ADD, form));
  }
  return list;
}

PasswordStoreChangeList LoginDatabase::UpdateLogin(const PasswordForm& form) {
  std::string encrypted_password;
  if (EncryptedString(
          clear_password_values_ ? base::string16() : form.password_value,
          &encrypted_password) != ENCRYPTION_RESULT_SUCCESS)
    return PasswordStoreChangeList();

#if defined(OS_IOS)
  DeleteEncryptedPassword(form);
#endif
  // Replacement is necessary to deal with updating imported credentials. See
  // crbug.com/349138 for details.
  sql::Statement s(db_.GetCachedStatement(SQL_FROM_HERE,
                                          "UPDATE OR REPLACE logins SET "
                                          "action_url = ?, "
                                          "password_value = ?, "
                                          "ssl_valid = ?, "
                                          "preferred = ?, "
                                          "possible_usernames = ?, "
                                          "times_used = ?, "
                                          "submit_element = ?, "
                                          "date_synced = ?, "
                                          "date_created = ?, "
                                          "blacklisted_by_user = ?, "
                                          "scheme = ?, "
                                          "password_type = ?, "
                                          "display_name = ?, "
                                          "icon_url = ?, "
                                          "federation_url = ?, "
                                          "skip_zero_click = ?, "
                                          "generation_upload_status = ? "
                                          "WHERE origin_url = ? AND "
                                          "username_element = ? AND "
                                          "username_value = ? AND "
                                          "password_element = ? AND "
                                          "signon_realm = ?"));
  s.BindString(0, form.action.spec());
  s.BindBlob(1, encrypted_password.data(),
             static_cast<int>(encrypted_password.length()));
  s.BindInt(2, form.ssl_valid);
  s.BindInt(3, form.preferred);
  base::Pickle pickle = SerializeVector(form.other_possible_usernames);
  s.BindBlob(4, pickle.data(), pickle.size());
  s.BindInt(5, form.times_used);
  s.BindString16(6, form.submit_element);
  s.BindInt64(7, form.date_synced.ToInternalValue());
  s.BindInt64(8, form.date_created.ToInternalValue());
  s.BindInt(9, form.blacklisted_by_user);
  s.BindInt(10, form.scheme);
  s.BindInt(11, form.type);
  s.BindString16(12, form.display_name);
  s.BindString(13, form.icon_url.spec());
  // An empty Origin serializes as "null" which would be strange to store here.
  s.BindString(14, form.federation_origin.unique()
                       ? std::string()
                       : form.federation_origin.Serialize());
  s.BindInt(15, form.skip_zero_click);
  s.BindInt(16, form.generation_upload_status);

  // WHERE starts here.
  s.BindString(17, form.origin.spec());
  s.BindString16(18, form.username_element);
  s.BindString16(19, form.username_value);
  s.BindString16(20, form.password_element);
  s.BindString(21, form.signon_realm);

  if (!s.Run())
    return PasswordStoreChangeList();

  PasswordStoreChangeList list;
  if (db_.GetLastChangeCount())
    list.push_back(PasswordStoreChange(PasswordStoreChange::UPDATE, form));

  return list;
}

bool LoginDatabase::RemoveLogin(const PasswordForm& form) {
  if (form.is_public_suffix_match) {
    // TODO(dvadym): Discuss whether we should allow to remove PSL matched
    // credentials.
    return false;
  }
#if defined(OS_IOS)
  DeleteEncryptedPassword(form);
#endif
  // Remove a login by UNIQUE-constrained fields.
  sql::Statement s(db_.GetCachedStatement(SQL_FROM_HERE,
                                          "DELETE FROM logins WHERE "
                                          "origin_url = ? AND "
                                          "username_element = ? AND "
                                          "username_value = ? AND "
                                          "password_element = ? AND "
                                          "submit_element = ? AND "
                                          "signon_realm = ? "));
  s.BindString(0, form.origin.spec());
  s.BindString16(1, form.username_element);
  s.BindString16(2, form.username_value);
  s.BindString16(3, form.password_element);
  s.BindString16(4, form.submit_element);
  s.BindString(5, form.signon_realm);

  return s.Run() && db_.GetLastChangeCount() > 0;
}

bool LoginDatabase::RemoveLoginsCreatedBetween(base::Time delete_begin,
                                               base::Time delete_end) {
#if defined(OS_IOS)
  ScopedVector<autofill::PasswordForm> forms;
  if (GetLoginsCreatedBetween(delete_begin, delete_end, &forms)) {
    for (size_t i = 0; i < forms.size(); i++) {
      DeleteEncryptedPassword(*forms[i]);
    }
  }
#endif

  sql::Statement s(db_.GetCachedStatement(SQL_FROM_HERE,
      "DELETE FROM logins WHERE "
      "date_created >= ? AND date_created < ?"));
  s.BindInt64(0, delete_begin.ToInternalValue());
  s.BindInt64(1, delete_end.is_null() ? std::numeric_limits<int64_t>::max()
                                      : delete_end.ToInternalValue());

  return s.Run();
}

bool LoginDatabase::RemoveLoginsSyncedBetween(base::Time delete_begin,
                                              base::Time delete_end) {
  sql::Statement s(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "DELETE FROM logins WHERE date_synced >= ? AND date_synced < ?"));
  s.BindInt64(0, delete_begin.ToInternalValue());
  s.BindInt64(1,
              delete_end.is_null() ? base::Time::Max().ToInternalValue()
                                   : delete_end.ToInternalValue());

  return s.Run();
}

bool LoginDatabase::GetAutoSignInLogins(
    ScopedVector<autofill::PasswordForm>* forms) const {
  DCHECK(forms);
  sql::Statement s(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT origin_url, action_url, username_element, username_value, "
      "password_element, password_value, submit_element, signon_realm, "
      "ssl_valid, preferred, date_created, blacklisted_by_user, "
      "scheme, password_type, possible_usernames, times_used, form_data, "
      "date_synced, display_name, icon_url, "
      "federation_url, skip_zero_click, generation_upload_status FROM logins "
      "WHERE skip_zero_click = 0 ORDER BY origin_url"));

  return StatementToForms(&s, nullptr, forms);
}

bool LoginDatabase::DisableAutoSignInForAllLogins() {
  sql::Statement s(db_.GetCachedStatement(
      SQL_FROM_HERE, "UPDATE logins SET skip_zero_click = 1;"));

  return s.Run();
}

// static
LoginDatabase::EncryptionResult LoginDatabase::InitPasswordFormFromStatement(
    PasswordForm* form,
    sql::Statement& s) {
  std::string encrypted_password;
  s.ColumnBlobAsString(COLUMN_PASSWORD_VALUE, &encrypted_password);
  base::string16 decrypted_password;
  EncryptionResult encryption_result =
      DecryptedString(encrypted_password, &decrypted_password);
  if (encryption_result != ENCRYPTION_RESULT_SUCCESS) {
    VLOG(0) << "Password decryption failed, encryption_result is "
            << encryption_result;
    return encryption_result;
  }

  std::string tmp = s.ColumnString(COLUMN_ORIGIN_URL);
  form->origin = GURL(tmp);
  tmp = s.ColumnString(COLUMN_ACTION_URL);
  form->action = GURL(tmp);
  form->username_element = s.ColumnString16(COLUMN_USERNAME_ELEMENT);
  form->username_value = s.ColumnString16(COLUMN_USERNAME_VALUE);
  form->password_element = s.ColumnString16(COLUMN_PASSWORD_ELEMENT);
  form->password_value = decrypted_password;
  form->submit_element = s.ColumnString16(COLUMN_SUBMIT_ELEMENT);
  tmp = s.ColumnString(COLUMN_SIGNON_REALM);
  form->signon_realm = tmp;
  form->ssl_valid = (s.ColumnInt(COLUMN_SSL_VALID) > 0);
  form->preferred = (s.ColumnInt(COLUMN_PREFERRED) > 0);
  form->date_created =
      base::Time::FromInternalValue(s.ColumnInt64(COLUMN_DATE_CREATED));
  form->blacklisted_by_user = (s.ColumnInt(COLUMN_BLACKLISTED_BY_USER) > 0);
  int scheme_int = s.ColumnInt(COLUMN_SCHEME);
  DCHECK_LE(0, scheme_int);
  DCHECK_GE(PasswordForm::SCHEME_LAST, scheme_int);
  form->scheme = static_cast<PasswordForm::Scheme>(scheme_int);
  int type_int = s.ColumnInt(COLUMN_PASSWORD_TYPE);
  DCHECK(type_int >= 0 && type_int <= PasswordForm::TYPE_LAST) << type_int;
  form->type = static_cast<PasswordForm::Type>(type_int);
  if (s.ColumnByteLength(COLUMN_POSSIBLE_USERNAMES)) {
    base::Pickle pickle(
        static_cast<const char*>(s.ColumnBlob(COLUMN_POSSIBLE_USERNAMES)),
        s.ColumnByteLength(COLUMN_POSSIBLE_USERNAMES));
    form->other_possible_usernames = DeserializeVector(pickle);
  }
  form->times_used = s.ColumnInt(COLUMN_TIMES_USED);
  if (s.ColumnByteLength(COLUMN_FORM_DATA)) {
    base::Pickle form_data_pickle(
        static_cast<const char*>(s.ColumnBlob(COLUMN_FORM_DATA)),
        s.ColumnByteLength(COLUMN_FORM_DATA));
    base::PickleIterator form_data_iter(form_data_pickle);
    bool success =
        autofill::DeserializeFormData(&form_data_iter, &form->form_data);
    metrics_util::FormDeserializationStatus status =
        success ? metrics_util::LOGIN_DATABASE_SUCCESS
                : metrics_util::LOGIN_DATABASE_FAILURE;
    metrics_util::LogFormDataDeserializationStatus(status);
  }
  form->date_synced =
      base::Time::FromInternalValue(s.ColumnInt64(COLUMN_DATE_SYNCED));
  form->display_name = s.ColumnString16(COLUMN_DISPLAY_NAME);
  form->icon_url = GURL(s.ColumnString(COLUMN_ICON_URL));
  form->federation_origin =
      url::Origin(GURL(s.ColumnString(COLUMN_FEDERATION_URL)));
  form->skip_zero_click = (s.ColumnInt(COLUMN_SKIP_ZERO_CLICK) > 0);
  int generation_upload_status_int =
      s.ColumnInt(COLUMN_GENERATION_UPLOAD_STATUS);
  DCHECK(generation_upload_status_int >= 0 &&
         generation_upload_status_int <= PasswordForm::UNKNOWN_STATUS);
  form->generation_upload_status =
      static_cast<PasswordForm::GenerationUploadStatus>(
          generation_upload_status_int);
  return ENCRYPTION_RESULT_SUCCESS;
}

bool LoginDatabase::GetLogins(
    const PasswordForm& form,
    ScopedVector<autofill::PasswordForm>* forms) const {
  DCHECK(forms);
  // You *must* change LoginTableColumns if this query changes.
  std::string sql_query =
      "SELECT origin_url, action_url, "
      "username_element, username_value, "
      "password_element, password_value, submit_element, "
      "signon_realm, ssl_valid, preferred, date_created, blacklisted_by_user, "
      "scheme, password_type, possible_usernames, times_used, form_data, "
      "date_synced, display_name, icon_url, "
      "federation_url, skip_zero_click, generation_upload_status "
      "FROM logins WHERE signon_realm == ? ";
  const GURL signon_realm(form.signon_realm);
  std::string registered_domain = GetRegistryControlledDomain(signon_realm);
  const bool should_PSL_matching_apply =
      form.scheme == PasswordForm::SCHEME_HTML &&
      ShouldPSLDomainMatchingApply(registered_domain);
  const bool should_federated_apply = form.scheme == PasswordForm::SCHEME_HTML;
  if (should_PSL_matching_apply)
    sql_query += "OR signon_realm REGEXP ? ";
  if (should_federated_apply)
    sql_query += "OR (signon_realm LIKE ? AND password_type == 2) ";

  // TODO(nyquist) Consider usage of GetCachedStatement when
  // http://crbug.com/248608 is fixed.
  sql::Statement s(db_.GetUniqueStatement(sql_query.c_str()));
  s.BindString(0, form.signon_realm);
  int placeholder = 1;

  // PSL matching only applies to HTML forms.
  if (should_PSL_matching_apply) {
    // We are extending the original SQL query with one that includes more
    // possible matches based on public suffix domain matching. Using a regexp
    // here is just an optimization to not have to parse all the stored entries
    // in the |logins| table. The result (scheme, domain and port) is verified
    // further down using GURL. See the functions SchemeMatches,
    // RegistryControlledDomainMatches and PortMatches.
    // We need to escape . in the domain. Since the domain has already been
    // sanitized using GURL, we do not need to escape any other characters.
    base::ReplaceChars(registered_domain, ".", "\\.", &registered_domain);
    std::string scheme = signon_realm.scheme();
    // We need to escape . in the scheme. Since the scheme has already been
    // sanitized using GURL, we do not need to escape any other characters.
    // The scheme soap.beep is an example with '.'.
    base::ReplaceChars(scheme, ".", "\\.", &scheme);
    const std::string port = signon_realm.port();
    // For a signon realm such as http://foo.bar/, this regexp will match
    // domains on the form http://foo.bar/, http://www.foo.bar/,
    // http://www.mobile.foo.bar/. It will not match http://notfoo.bar/.
    // The scheme and port has to be the same as the observed form.
    std::string regexp = "^(" + scheme + ":\\/\\/)([\\w-]+\\.)*" +
                         registered_domain + "(:" + port + ")?\\/$";
    s.BindString(placeholder++, regexp);
  }
  if (should_federated_apply) {
    std::string expression =
        base::StringPrintf("federation://%s/%%", form.origin.host().c_str());
    s.BindString(placeholder++, expression);
  }

  if (!should_PSL_matching_apply && !should_federated_apply) {
    // Otherwise the histogram is reported in StatementToForms.
    UMA_HISTOGRAM_ENUMERATION("PasswordManager.PslDomainMatchTriggering",
                              PSL_DOMAIN_MATCH_NOT_USED,
                              PSL_DOMAIN_MATCH_COUNT);
  }

  return StatementToForms(
      &s, should_PSL_matching_apply || should_federated_apply ? &form : nullptr,
      forms);
}

bool LoginDatabase::GetLoginsCreatedBetween(
    const base::Time begin,
    const base::Time end,
    ScopedVector<autofill::PasswordForm>* forms) const {
  DCHECK(forms);
  sql::Statement s(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT origin_url, action_url, "
      "username_element, username_value, "
      "password_element, password_value, submit_element, "
      "signon_realm, ssl_valid, preferred, date_created, blacklisted_by_user, "
      "scheme, password_type, possible_usernames, times_used, form_data, "
      "date_synced, display_name, icon_url, "
      "federation_url, skip_zero_click, generation_upload_status FROM logins "
      "WHERE date_created >= ? AND date_created < ?"
      "ORDER BY origin_url"));
  s.BindInt64(0, begin.ToInternalValue());
  s.BindInt64(1, end.is_null() ? std::numeric_limits<int64_t>::max()
                               : end.ToInternalValue());

  return StatementToForms(&s, nullptr, forms);
}

bool LoginDatabase::GetLoginsSyncedBetween(
    const base::Time begin,
    const base::Time end,
    ScopedVector<autofill::PasswordForm>* forms) const {
  DCHECK(forms);
  sql::Statement s(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT origin_url, action_url, username_element, username_value, "
      "password_element, password_value, submit_element, signon_realm, "
      "ssl_valid, preferred, date_created, blacklisted_by_user, "
      "scheme, password_type, possible_usernames, times_used, form_data, "
      "date_synced, display_name, icon_url, "
      "federation_url, skip_zero_click, generation_upload_status FROM logins "
      "WHERE date_synced >= ? AND date_synced < ?"
      "ORDER BY origin_url"));
  s.BindInt64(0, begin.ToInternalValue());
  s.BindInt64(1,
              end.is_null() ? base::Time::Max().ToInternalValue()
                            : end.ToInternalValue());

  return StatementToForms(&s, nullptr, forms);
}

bool LoginDatabase::GetAutofillableLogins(
    ScopedVector<autofill::PasswordForm>* forms) const {
  return GetAllLoginsWithBlacklistSetting(false, forms);
}

bool LoginDatabase::GetBlacklistLogins(
    ScopedVector<autofill::PasswordForm>* forms) const {
  return GetAllLoginsWithBlacklistSetting(true, forms);
}

bool LoginDatabase::GetAllLoginsWithBlacklistSetting(
    bool blacklisted,
    ScopedVector<autofill::PasswordForm>* forms) const {
  DCHECK(forms);
  // You *must* change LoginTableColumns if this query changes.
  sql::Statement s(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT origin_url, action_url, "
      "username_element, username_value, "
      "password_element, password_value, submit_element, "
      "signon_realm, ssl_valid, preferred, date_created, blacklisted_by_user, "
      "scheme, password_type, possible_usernames, times_used, form_data, "
      "date_synced, display_name, icon_url, "
      "federation_url, skip_zero_click, generation_upload_status FROM logins "
      "WHERE blacklisted_by_user == ? ORDER BY origin_url"));
  s.BindInt(0, blacklisted ? 1 : 0);

  return StatementToForms(&s, nullptr, forms);
}

bool LoginDatabase::DeleteAndRecreateDatabaseFile() {
  DCHECK(db_.is_open());
  meta_table_.Reset();
  db_.Close();
  sql::Connection::Delete(db_path_);
  return Init();
}

std::string LoginDatabase::GetEncryptedPassword(
    const autofill::PasswordForm& form) const {
  sql::Statement s(
      db_.GetCachedStatement(SQL_FROM_HERE,
                             "SELECT password_value FROM logins WHERE "
                             "origin_url = ? AND "
                             "username_element = ? AND "
                             "username_value = ? AND "
                             "password_element = ? AND "
                             "submit_element = ? AND "
                             "signon_realm = ? "));

  s.BindString(0, form.origin.spec());
  s.BindString16(1, form.username_element);
  s.BindString16(2, form.username_value);
  s.BindString16(3, form.password_element);
  s.BindString16(4, form.submit_element);
  s.BindString(5, form.signon_realm);

  std::string encrypted_password;
  if (s.Step()) {
    s.ColumnBlobAsString(0, &encrypted_password);
  }
  return encrypted_password;
}

// static
bool LoginDatabase::StatementToForms(
    sql::Statement* statement,
    const autofill::PasswordForm* matched_form,
    ScopedVector<autofill::PasswordForm>* forms) {
  PSLDomainMatchMetric psl_domain_match_metric = PSL_DOMAIN_MATCH_NONE;

  forms->clear();
  while (statement->Step()) {
    std::unique_ptr<PasswordForm> new_form(new PasswordForm());
    EncryptionResult result =
        InitPasswordFormFromStatement(new_form.get(), *statement);
    if (result == ENCRYPTION_RESULT_SERVICE_FAILURE)
      return false;
    if (result == ENCRYPTION_RESULT_ITEM_FAILURE)
      continue;
    DCHECK_EQ(ENCRYPTION_RESULT_SUCCESS, result);
    if (matched_form && matched_form->signon_realm != new_form->signon_realm) {
      if (new_form->scheme != PasswordForm::SCHEME_HTML)
        continue;  // Ignore non-HTML matches.

      if (IsPublicSuffixDomainMatch(new_form->signon_realm,
                                    matched_form->signon_realm)) {
        psl_domain_match_metric = PSL_DOMAIN_MATCH_FOUND;
        new_form->is_public_suffix_match = true;
      } else if (!new_form->federation_origin.unique() &&
                 IsFederatedMatch(new_form->signon_realm,
                                  matched_form->origin)) {
      } else {
        continue;
      }
    }
    forms->push_back(std::move(new_form));
  }

  if (matched_form) {
    UMA_HISTOGRAM_ENUMERATION("PasswordManager.PslDomainMatchTriggering",
                              psl_domain_match_metric, PSL_DOMAIN_MATCH_COUNT);
  }

  if (!statement->Succeeded())
    return false;
  return true;
}

}  // namespace password_manager
