// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/recovery.h"

#include <stddef.h>

#include "base/files/file_path.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "sql/connection.h"
#include "sql/statement.h"
#include "third_party/sqlite/sqlite3.h"
#include "third_party/sqlite/src/src/recover.h"

namespace sql {

namespace {

enum RecoveryEventType {
  // Init() completed successfully.
  RECOVERY_SUCCESS_INIT = 0,

  // Failed to open temporary database to recover into.
  RECOVERY_FAILED_OPEN_TEMPORARY,

  // Failed to initialize recover vtable system.
  RECOVERY_FAILED_VIRTUAL_TABLE_INIT,

  // System SQLite doesn't support vtable.
  RECOVERY_FAILED_VIRTUAL_TABLE_SYSTEM_SQLITE,

  // Failed attempting to enable writable_schema.
  RECOVERY_FAILED_WRITABLE_SCHEMA,

  // Failed to attach the corrupt database to the temporary database.
  RECOVERY_FAILED_ATTACH,

  // Backup() successfully completed.
  RECOVERY_SUCCESS_BACKUP,

  // Failed sqlite3_backup_init().  Error code in Sqlite.RecoveryHandle.
  RECOVERY_FAILED_BACKUP_INIT,

  // Failed sqlite3_backup_step().  Error code in Sqlite.RecoveryStep.
  RECOVERY_FAILED_BACKUP_STEP,

  // AutoRecoverTable() successfully completed.
  RECOVERY_SUCCESS_AUTORECOVER,

  // The target table contained a type which the code is not equipped
  // to handle.  This should only happen if things are fubar.
  RECOVERY_FAILED_AUTORECOVER_UNRECOGNIZED_TYPE,

  // The target table does not exist.
  RECOVERY_FAILED_AUTORECOVER_MISSING_TABLE,

  // The recovery virtual table creation failed.
  RECOVERY_FAILED_AUTORECOVER_CREATE,

  // Copying data from the recovery table to the target table failed.
  RECOVERY_FAILED_AUTORECOVER_INSERT,

  // Dropping the recovery virtual table failed.
  RECOVERY_FAILED_AUTORECOVER_DROP,

  // SetupMeta() successfully completed.
  RECOVERY_SUCCESS_SETUP_META,

  // Failure creating recovery meta table.
  RECOVERY_FAILED_META_CREATE,

  // GetMetaVersionNumber() successfully completed.
  RECOVERY_SUCCESS_META_VERSION,

  // Failed in querying recovery meta table.
  RECOVERY_FAILED_META_QUERY,

  // No version key in recovery meta table.
  RECOVERY_FAILED_META_NO_VERSION,

  // Always keep this at the end.
  RECOVERY_EVENT_MAX,
};

void RecordRecoveryEvent(RecoveryEventType recovery_event) {
  UMA_HISTOGRAM_ENUMERATION("Sqlite.RecoveryEvents",
                            recovery_event, RECOVERY_EVENT_MAX);
}

}  // namespace

// static
bool Recovery::FullRecoverySupported() {
  // TODO(shess): See comment in Init().
  return true;
}

// static
std::unique_ptr<Recovery> Recovery::Begin(Connection* connection,
                                          const base::FilePath& db_path) {
  // Recovery is likely to be used in error handling.  Since recovery changes
  // the state of the handle, protect against multiple layers attempting the
  // same recovery.
  if (!connection->is_open()) {
    // Warn about API mis-use.
    DLOG_IF(FATAL, !connection->poisoned_)
        << "Illegal to recover with closed database";
    return std::unique_ptr<Recovery>();
  }

  std::unique_ptr<Recovery> r(new Recovery(connection));
  if (!r->Init(db_path)) {
    // TODO(shess): Should Init() failure result in Raze()?
    r->Shutdown(POISON);
    return std::unique_ptr<Recovery>();
  }

  return r;
}

// static
bool Recovery::Recovered(std::unique_ptr<Recovery> r) {
  return r->Backup();
}

// static
void Recovery::Unrecoverable(std::unique_ptr<Recovery> r) {
  CHECK(r->db_);
  // ~Recovery() will RAZE_AND_POISON.
}

// static
void Recovery::Rollback(std::unique_ptr<Recovery> r) {
  // TODO(shess): HISTOGRAM to track?  Or just have people crash out?
  // Crash and dump?
  r->Shutdown(POISON);
}

Recovery::Recovery(Connection* connection)
    : db_(connection),
      recover_db_() {
  // Result should keep the page size specified earlier.
  if (db_->page_size_)
    recover_db_.set_page_size(db_->page_size_);

  // Files with I/O errors cannot be safely memory-mapped.
  recover_db_.set_mmap_disabled();

  // TODO(shess): This may not handle cases where the default page
  // size is used, but the default has changed.  I do not think this
  // has ever happened.  This could be handled by using "PRAGMA
  // page_size", at the cost of potential additional failure cases.
}

Recovery::~Recovery() {
  Shutdown(RAZE_AND_POISON);
}

bool Recovery::Init(const base::FilePath& db_path) {
  // Prevent the possibility of re-entering this code due to errors
  // which happen while executing this code.
  DCHECK(!db_->has_error_callback());

  // Break any outstanding transactions on the original database to
  // prevent deadlocks reading through the attached version.
  // TODO(shess): A client may legitimately wish to recover from
  // within the transaction context, because it would potentially
  // preserve any in-flight changes.  Unfortunately, any attach-based
  // system could not handle that.  A system which manually queried
  // one database and stored to the other possibly could, but would be
  // more complicated.
  db_->RollbackAllTransactions();

  // Disable exclusive locking mode so that the attached database can
  // access things.  The locking_mode change is not active until the
  // next database access, so immediately force an access.  Enabling
  // writable_schema allows processing through certain kinds of
  // corruption.
  // TODO(shess): It would be better to just close the handle, but it
  // is necessary for the final backup which rewrites things.  It
  // might be reasonable to close then re-open the handle.
  ignore_result(db_->Execute("PRAGMA writable_schema=1"));
  ignore_result(db_->Execute("PRAGMA locking_mode=NORMAL"));
  ignore_result(db_->Execute("SELECT COUNT(*) FROM sqlite_master"));

  // TODO(shess): If this is a common failure case, it might be
  // possible to fall back to a memory database.  But it probably
  // implies that the SQLite tmpdir logic is busted, which could cause
  // a variety of other random issues in our code.
  if (!recover_db_.OpenTemporary()) {
    RecordRecoveryEvent(RECOVERY_FAILED_OPEN_TEMPORARY);
    return false;
  }

  // Enable the recover virtual table for this connection.
  int rc = recoverVtableInit(recover_db_.db_);
  if (rc != SQLITE_OK) {
    RecordRecoveryEvent(RECOVERY_FAILED_VIRTUAL_TABLE_INIT);
    LOG(ERROR) << "Failed to initialize recover module: "
               << recover_db_.GetErrorMessage();
    return false;
  }

  // Turn on |SQLITE_RecoveryMode| for the handle, which allows
  // reading certain broken databases.
  if (!recover_db_.Execute("PRAGMA writable_schema=1")) {
    RecordRecoveryEvent(RECOVERY_FAILED_WRITABLE_SCHEMA);
    return false;
  }

  if (!recover_db_.AttachDatabase(db_path, "corrupt")) {
    RecordRecoveryEvent(RECOVERY_FAILED_ATTACH);
    return false;
  }

  RecordRecoveryEvent(RECOVERY_SUCCESS_INIT);
  return true;
}

bool Recovery::Backup() {
  CHECK(db_);
  CHECK(recover_db_.is_open());

  // TODO(shess): Some of the failure cases here may need further
  // exploration.  Just as elsewhere, persistent problems probably
  // need to be razed, while anything which might succeed on a future
  // run probably should be allowed to try.  But since Raze() uses the
  // same approach, even that wouldn't work when this code fails.
  //
  // The documentation for the backup system indicate a relatively
  // small number of errors are expected:
  // SQLITE_BUSY - cannot lock the destination database.  This should
  //               only happen if someone has another handle to the
  //               database, Chromium generally doesn't do that.
  // SQLITE_LOCKED - someone locked the source database.  Should be
  //                 impossible (perhaps anti-virus could?).
  // SQLITE_READONLY - destination is read-only.
  // SQLITE_IOERR - since source database is temporary, probably
  //                indicates that the destination contains blocks
  //                throwing errors, or gross filesystem errors.
  // SQLITE_NOMEM - out of memory, should be transient.
  //
  // AFAICT, SQLITE_BUSY and SQLITE_NOMEM could perhaps be considered
  // transient, with SQLITE_LOCKED being unclear.
  //
  // SQLITE_READONLY and SQLITE_IOERR are probably persistent, with a
  // strong chance that Raze() would not resolve them.  If Delete()
  // deletes the database file, the code could then re-open the file
  // and attempt the backup again.
  //
  // For now, this code attempts a best effort and records histograms
  // to inform future development.

  // Backup the original db from the recovered db.
  const char* kMain = "main";
  sqlite3_backup* backup = sqlite3_backup_init(db_->db_, kMain,
                                               recover_db_.db_, kMain);
  if (!backup) {
    RecordRecoveryEvent(RECOVERY_FAILED_BACKUP_INIT);

    // Error code is in the destination database handle.
    int err = sqlite3_extended_errcode(db_->db_);
    UMA_HISTOGRAM_SPARSE_SLOWLY("Sqlite.RecoveryHandle", err);
    LOG(ERROR) << "sqlite3_backup_init() failed: "
               << sqlite3_errmsg(db_->db_);

    return false;
  }

  // -1 backs up the entire database.
  int rc = sqlite3_backup_step(backup, -1);
  int pages = sqlite3_backup_pagecount(backup);
  // TODO(shess): sqlite3_backup_finish() appears to allow returning a
  // different value from sqlite3_backup_step().  Circle back and
  // figure out if that can usefully inform the decision of whether to
  // retry or not.
  sqlite3_backup_finish(backup);
  DCHECK_GT(pages, 0);

  if (rc != SQLITE_DONE) {
    RecordRecoveryEvent(RECOVERY_FAILED_BACKUP_STEP);
    UMA_HISTOGRAM_SPARSE_SLOWLY("Sqlite.RecoveryStep", rc);
    LOG(ERROR) << "sqlite3_backup_step() failed: "
               << sqlite3_errmsg(db_->db_);
  }

  // The destination database was locked.  Give up, but leave the data
  // in place.  Maybe it won't be locked next time.
  if (rc == SQLITE_BUSY || rc == SQLITE_LOCKED) {
    Shutdown(POISON);
    return false;
  }

  // Running out of memory should be transient, retry later.
  if (rc == SQLITE_NOMEM) {
    Shutdown(POISON);
    return false;
  }

  // TODO(shess): For now, leave the original database alone, pending
  // results from Sqlite.RecoveryStep.  Some errors should probably
  // route to RAZE_AND_POISON.
  if (rc != SQLITE_DONE) {
    Shutdown(POISON);
    return false;
  }

  // Clean up the recovery db, and terminate the main database
  // connection.
  RecordRecoveryEvent(RECOVERY_SUCCESS_BACKUP);
  Shutdown(POISON);
  return true;
}

void Recovery::Shutdown(Recovery::Disposition raze) {
  if (!db_)
    return;

  recover_db_.Close();
  if (raze == RAZE_AND_POISON) {
    db_->RazeAndClose();
  } else if (raze == POISON) {
    db_->Poison();
  }
  db_ = NULL;
}

bool Recovery::AutoRecoverTable(const char* table_name,
                                size_t* rows_recovered) {
  // Query the info for the recovered table in database [main].
  std::string query(
      base::StringPrintf("PRAGMA main.table_info(%s)", table_name));
  Statement s(db()->GetUniqueStatement(query.c_str()));

  // The columns of the recover virtual table.
  std::vector<std::string> create_column_decls;

  // The columns to select from the recover virtual table when copying
  // to the recovered table.
  std::vector<std::string> insert_columns;

  // If PRIMARY KEY is a single INTEGER column, then it is an alias
  // for ROWID.  The primary key can be compound, so this can only be
  // determined after processing all column data and tracking what is
  // seen.  |pk_column_count| counts the columns in the primary key.
  // |rowid_decl| stores the ROWID version of the last INTEGER column
  // seen, which is at |rowid_ofs| in |create_column_decls|.
  size_t pk_column_count = 0;
  size_t rowid_ofs = 0;  // Only valid if rowid_decl is set.
  std::string rowid_decl;  // ROWID version of column |rowid_ofs|.

  while (s.Step()) {
    const std::string column_name(s.ColumnString(1));
    const std::string column_type(s.ColumnString(2));
    const int default_type = s.ColumnType(4);
    const bool default_is_null = (default_type == COLUMN_TYPE_NULL);
    const int pk_column = s.ColumnInt(5);

    // http://www.sqlite.org/pragma.html#pragma_table_info documents column 5 as
    // the 1-based index of the column in the primary key, otherwise 0.
    if (pk_column > 0)
      ++pk_column_count;

    // Construct column declaration as "name type [optional constraint]".
    std::string column_decl = column_name;

    // SQLite's affinity detection is documented at:
    // http://www.sqlite.org/datatype3.html#affname
    // The gist of it is that CHAR, TEXT, and INT use substring matches.
    // TODO(shess): It would be nice to unit test the type handling,
    // but it is not obvious to me how to write a test which would
    // fail appropriately when something was broken.  It would have to
    // somehow use data which would allow detecting the various type
    // coercions which happen.  If STRICT could be enabled, type
    // mismatches could be detected by which rows are filtered.
    if (column_type.find("INT") != std::string::npos) {
      if (pk_column == 1) {
        rowid_ofs = create_column_decls.size();
        rowid_decl = column_name + " ROWID";
      }
      column_decl += " INTEGER";
    } else if (column_type.find("CHAR") != std::string::npos ||
               column_type.find("TEXT") != std::string::npos) {
      column_decl += " TEXT";
    } else if (column_type == "BLOB") {
      column_decl += " BLOB";
    } else if (column_type.find("DOUB") != std::string::npos) {
      column_decl += " FLOAT";
    } else {
      // TODO(shess): AFAICT, there remain:
      // - contains("CLOB") -> TEXT
      // - contains("REAL") -> FLOAT
      // - contains("FLOA") -> FLOAT
      // - other -> "NUMERIC"
      // Just code those in as they come up.
      NOTREACHED() << " Unsupported type " << column_type;
      RecordRecoveryEvent(RECOVERY_FAILED_AUTORECOVER_UNRECOGNIZED_TYPE);
      return false;
    }

    create_column_decls.push_back(column_decl);

    // Per the NOTE in the header file, convert NULL values to the
    // DEFAULT.  All columns could be IFNULL(column_name,default), but
    // the NULL case would require special handling either way.
    if (default_is_null) {
      insert_columns.push_back(column_name);
    } else {
      // The default value appears to be pre-quoted, as if it is
      // literally from the sqlite_master CREATE statement.
      std::string default_value = s.ColumnString(4);
      insert_columns.push_back(base::StringPrintf(
          "IFNULL(%s,%s)", column_name.c_str(), default_value.c_str()));
    }
  }

  // Receiving no column information implies that the table doesn't exist.
  if (create_column_decls.empty()) {
    RecordRecoveryEvent(RECOVERY_FAILED_AUTORECOVER_MISSING_TABLE);
    return false;
  }

  // If the PRIMARY KEY was a single INTEGER column, convert it to ROWID.
  if (pk_column_count == 1 && !rowid_decl.empty())
    create_column_decls[rowid_ofs] = rowid_decl;

  std::string recover_create(base::StringPrintf(
      "CREATE VIRTUAL TABLE temp.recover_%s USING recover(corrupt.%s, %s)",
      table_name,
      table_name,
      base::JoinString(create_column_decls, ",").c_str()));

  // INSERT OR IGNORE means that it will drop rows resulting from constraint
  // violations.  INSERT OR REPLACE only handles UNIQUE constraint violations.
  std::string recover_insert(base::StringPrintf(
      "INSERT OR IGNORE INTO main.%s SELECT %s FROM temp.recover_%s",
      table_name,
      base::JoinString(insert_columns, ",").c_str(),
      table_name));

  std::string recover_drop(base::StringPrintf(
      "DROP TABLE temp.recover_%s", table_name));

  if (!db()->Execute(recover_create.c_str())) {
    RecordRecoveryEvent(RECOVERY_FAILED_AUTORECOVER_CREATE);
    return false;
  }

  if (!db()->Execute(recover_insert.c_str())) {
    RecordRecoveryEvent(RECOVERY_FAILED_AUTORECOVER_INSERT);
    ignore_result(db()->Execute(recover_drop.c_str()));
    return false;
  }

  *rows_recovered = db()->GetLastChangeCount();

  // TODO(shess): Is leaving the recover table around a breaker?
  if (!db()->Execute(recover_drop.c_str())) {
    RecordRecoveryEvent(RECOVERY_FAILED_AUTORECOVER_DROP);
    return false;
  }
  RecordRecoveryEvent(RECOVERY_SUCCESS_AUTORECOVER);
  return true;
}

bool Recovery::SetupMeta() {
  const char kCreateSql[] =
      "CREATE VIRTUAL TABLE temp.recover_meta USING recover"
      "("
      "corrupt.meta,"
      "key TEXT NOT NULL,"
      "value ANY"  // Whatever is stored.
      ")";
  if (!db()->Execute(kCreateSql)) {
    RecordRecoveryEvent(RECOVERY_FAILED_META_CREATE);
    return false;
  }
  RecordRecoveryEvent(RECOVERY_SUCCESS_SETUP_META);
  return true;
}

bool Recovery::GetMetaVersionNumber(int* version) {
  DCHECK(version);
  // TODO(shess): DCHECK(db()->DoesTableExist("temp.recover_meta"));
  // Unfortunately, DoesTableExist() queries sqlite_master, not
  // sqlite_temp_master.

  const char kVersionSql[] =
      "SELECT value FROM temp.recover_meta WHERE key = 'version'";
  sql::Statement recovery_version(db()->GetUniqueStatement(kVersionSql));
  if (!recovery_version.Step()) {
    if (!recovery_version.Succeeded()) {
      RecordRecoveryEvent(RECOVERY_FAILED_META_QUERY);
    } else {
      RecordRecoveryEvent(RECOVERY_FAILED_META_NO_VERSION);
    }
    return false;
  }

  RecordRecoveryEvent(RECOVERY_SUCCESS_META_VERSION);
  *version = recovery_version.ColumnInt(0);
  return true;
}

}  // namespace sql
