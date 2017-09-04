// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/webdata/token_service_table.h"

#include <map>
#include <string>

#include "base/logging.h"
#include "components/os_crypt/os_crypt.h"
#include "components/webdata/common/web_database.h"
#include "sql/statement.h"

namespace {

WebDatabaseTable::TypeKey GetKey() {
  // We just need a unique constant. Use the address of a static that
  // COMDAT folding won't touch in an optimizing linker.
  static int table_key = 0;
  return reinterpret_cast<void*>(&table_key);
}

}  // namespace

TokenServiceTable* TokenServiceTable::FromWebDatabase(WebDatabase* db) {
  return static_cast<TokenServiceTable*>(db->GetTable(GetKey()));

}

WebDatabaseTable::TypeKey TokenServiceTable::GetTypeKey() const {
  return GetKey();
}

bool TokenServiceTable::CreateTablesIfNecessary() {
  if (!db_->DoesTableExist("token_service")) {
    if (!db_->Execute("CREATE TABLE token_service ("
                      "service VARCHAR PRIMARY KEY NOT NULL,"
                      "encrypted_token BLOB)")) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

bool TokenServiceTable::IsSyncable() {
  return true;
}

bool TokenServiceTable::MigrateToVersion(int version,
                                         bool* update_compatible_version) {
  return true;
}

bool TokenServiceTable::RemoveAllTokens() {
  sql::Statement s(db_->GetUniqueStatement(
      "DELETE FROM token_service"));

  return s.Run();
}

bool TokenServiceTable::RemoveTokenForService(const std::string& service) {
  sql::Statement s(db_->GetUniqueStatement(
      "DELETE FROM token_service WHERE service = ?"));
  s.BindString(0, service);

  return s.Run();
}

bool TokenServiceTable::SetTokenForService(const std::string& service,
                                           const std::string& token) {
  std::string encrypted_token;
  bool encrypted = OSCrypt::EncryptString(token, &encrypted_token);
  if (!encrypted) {
    return false;
  }

  // Don't bother with a cached statement since this will be a relatively
  // infrequent operation.
  sql::Statement s(db_->GetUniqueStatement(
      "INSERT OR REPLACE INTO token_service "
      "(service, encrypted_token) VALUES (?, ?)"));
  s.BindString(0, service);
  s.BindBlob(1, encrypted_token.data(),
             static_cast<int>(encrypted_token.length()));

  return s.Run();
}

bool TokenServiceTable::GetAllTokens(
    std::map<std::string, std::string>* tokens) {
  sql::Statement s(db_->GetUniqueStatement(
      "SELECT service, encrypted_token FROM token_service"));

  if (!s.is_valid())
    return false;

  while (s.Step()) {
    std::string encrypted_token;
    std::string decrypted_token;
    std::string service;
    service = s.ColumnString(0);
    bool entry_ok = !service.empty() &&
                    s.ColumnBlobAsString(1, &encrypted_token);
    if (entry_ok) {
      OSCrypt::DecryptString(encrypted_token, &decrypted_token);
      (*tokens)[service] = decrypted_token;
    } else {
      NOTREACHED();
      return false;
    }
  }
  return true;
}
