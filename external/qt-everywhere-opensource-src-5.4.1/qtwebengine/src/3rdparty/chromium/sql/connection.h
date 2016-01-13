// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_CONNECTION_H_
#define SQL_CONNECTION_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "sql/sql_export.h"

struct sqlite3;
struct sqlite3_stmt;

namespace base {
class FilePath;
}

namespace sql {

class Recovery;
class Statement;

// Uniquely identifies a statement. There are two modes of operation:
//
// - In the most common mode, you will use the source file and line number to
//   identify your statement. This is a convienient way to get uniqueness for
//   a statement that is only used in one place. Use the SQL_FROM_HERE macro
//   to generate a StatementID.
//
// - In the "custom" mode you may use the statement from different places or
//   need to manage it yourself for whatever reason. In this case, you should
//   make up your own unique name and pass it to the StatementID. This name
//   must be a static string, since this object only deals with pointers and
//   assumes the underlying string doesn't change or get deleted.
//
// This object is copyable and assignable using the compiler-generated
// operator= and copy constructor.
class StatementID {
 public:
  // Creates a uniquely named statement with the given file ane line number.
  // Normally you will use SQL_FROM_HERE instead of calling yourself.
  StatementID(const char* file, int line)
      : number_(line),
        str_(file) {
  }

  // Creates a uniquely named statement with the given user-defined name.
  explicit StatementID(const char* unique_name)
      : number_(-1),
        str_(unique_name) {
  }

  // This constructor is unimplemented and will generate a linker error if
  // called. It is intended to try to catch people dynamically generating
  // a statement name that will be deallocated and will cause a crash later.
  // All strings must be static and unchanging!
  explicit StatementID(const std::string& dont_ever_do_this);

  // We need this to insert into our map.
  bool operator<(const StatementID& other) const;

 private:
  int number_;
  const char* str_;
};

#define SQL_FROM_HERE sql::StatementID(__FILE__, __LINE__)

class Connection;

class SQL_EXPORT Connection {
 private:
  class StatementRef;  // Forward declaration, see real one below.

 public:
  // The database is opened by calling Open[InMemory](). Any uncommitted
  // transactions will be rolled back when this object is deleted.
  Connection();
  ~Connection();

  // Pre-init configuration ----------------------------------------------------

  // Sets the page size that will be used when creating a new database. This
  // must be called before Init(), and will only have an effect on new
  // databases.
  //
  // From sqlite.org: "The page size must be a power of two greater than or
  // equal to 512 and less than or equal to SQLITE_MAX_PAGE_SIZE. The maximum
  // value for SQLITE_MAX_PAGE_SIZE is 32768."
  void set_page_size(int page_size) { page_size_ = page_size; }

  // Sets the number of pages that will be cached in memory by sqlite. The
  // total cache size in bytes will be page_size * cache_size. This must be
  // called before Open() to have an effect.
  void set_cache_size(int cache_size) { cache_size_ = cache_size; }

  // Call to put the database in exclusive locking mode. There is no "back to
  // normal" flag because of some additional requirements sqlite puts on this
  // transaction (requires another access to the DB) and because we don't
  // actually need it.
  //
  // Exclusive mode means that the database is not unlocked at the end of each
  // transaction, which means there may be less time spent initializing the
  // next transaction because it doesn't have to re-aquire locks.
  //
  // This must be called before Open() to have an effect.
  void set_exclusive_locking() { exclusive_locking_ = true; }

  // Call to cause Open() to restrict access permissions of the
  // database file to only the owner.
  // TODO(shess): Currently only supported on OS_POSIX, is a noop on
  // other platforms.
  void set_restrict_to_user() { restrict_to_user_ = true; }

  // Set an error-handling callback.  On errors, the error number (and
  // statement, if available) will be passed to the callback.
  //
  // If no callback is set, the default action is to crash in debug
  // mode or return failure in release mode.
  typedef base::Callback<void(int, Statement*)> ErrorCallback;
  void set_error_callback(const ErrorCallback& callback) {
    error_callback_ = callback;
  }
  bool has_error_callback() const {
    return !error_callback_.is_null();
  }
  void reset_error_callback() {
    error_callback_.Reset();
  }

  // Set this tag to enable additional connection-type histogramming
  // for SQLite error codes and database version numbers.
  void set_histogram_tag(const std::string& tag) {
    histogram_tag_ = tag;
  }

  // Record a sparse UMA histogram sample under
  // |name|+"."+|histogram_tag_|.  If |histogram_tag_| is empty, no
  // histogram is recorded.
  void AddTaggedHistogram(const std::string& name, size_t sample) const;

  // Run "PRAGMA integrity_check" and post each line of
  // results into |messages|.  Returns the success of running the
  // statement - per the SQLite documentation, if no errors are found the
  // call should succeed, and a single value "ok" should be in messages.
  bool FullIntegrityCheck(std::vector<std::string>* messages);

  // Runs "PRAGMA quick_check" and, unlike the FullIntegrityCheck method,
  // interprets the results returning true if the the statement executes
  // without error and results in a single "ok" value.
  bool QuickIntegrityCheck() WARN_UNUSED_RESULT;

  // Initialization ------------------------------------------------------------

  // Initializes the SQL connection for the given file, returning true if the
  // file could be opened. You can call this or OpenInMemory.
  bool Open(const base::FilePath& path) WARN_UNUSED_RESULT;

  // Initializes the SQL connection for a temporary in-memory database. There
  // will be no associated file on disk, and the initial database will be
  // empty. You can call this or Open.
  bool OpenInMemory() WARN_UNUSED_RESULT;

  // Create a temporary on-disk database.  The database will be
  // deleted after close.  This kind of database is similar to
  // OpenInMemory() for small databases, but can page to disk if the
  // database becomes large.
  bool OpenTemporary() WARN_UNUSED_RESULT;

  // Returns true if the database has been successfully opened.
  bool is_open() const { return !!db_; }

  // Closes the database. This is automatically performed on destruction for
  // you, but this allows you to close the database early. You must not call
  // any other functions after closing it. It is permissable to call Close on
  // an uninitialized or already-closed database.
  void Close();

  // Reads the first <cache-size>*<page-size> bytes of the file to prime the
  // filesystem cache.  This can be more efficient than faulting pages
  // individually.  Since this involves blocking I/O, it should only be used if
  // the caller will immediately read a substantial amount of data from the
  // database.
  //
  // TODO(shess): Design a set of histograms or an experiment to inform this
  // decision.  Preloading should almost always improve later performance
  // numbers for this database simply because it pulls operations forward, but
  // if the data isn't actually used soon then preloading just slows down
  // everything else.
  void Preload();

  // Try to trim the cache memory used by the database.  If |aggressively| is
  // true, this function will try to free all of the cache memory it can. If
  // |aggressively| is false, this function will try to cut cache memory
  // usage by half.
  void TrimMemory(bool aggressively);

  // Raze the database to the ground.  This approximates creating a
  // fresh database from scratch, within the constraints of SQLite's
  // locking protocol (locks and open handles can make doing this with
  // filesystem operations problematic).  Returns true if the database
  // was razed.
  //
  // false is returned if the database is locked by some other
  // process.  RazeWithTimeout() may be used if appropriate.
  //
  // NOTE(shess): Raze() will DCHECK in the following situations:
  // - database is not open.
  // - the connection has a transaction open.
  // - a SQLite issue occurs which is structural in nature (like the
  //   statements used are broken).
  // Since Raze() is expected to be called in unexpected situations,
  // these all return false, since it is unlikely that the caller
  // could fix them.
  //
  // The database's page size is taken from |page_size_|.  The
  // existing database's |auto_vacuum| setting is lost (the
  // possibility of corruption makes it unreliable to pull it from the
  // existing database).  To re-enable on the empty database requires
  // running "PRAGMA auto_vacuum = 1;" then "VACUUM".
  //
  // NOTE(shess): For Android, SQLITE_DEFAULT_AUTOVACUUM is set to 1,
  // so Raze() sets auto_vacuum to 1.
  //
  // TODO(shess): Raze() needs a connection so cannot clear SQLITE_NOTADB.
  // TODO(shess): Bake auto_vacuum into Connection's API so it can
  // just pick up the default.
  bool Raze();
  bool RazeWithTimout(base::TimeDelta timeout);

  // Breaks all outstanding transactions (as initiated by
  // BeginTransaction()), closes the SQLite database, and poisons the
  // object so that all future operations against the Connection (or
  // its Statements) fail safely, without side effects.
  //
  // This is intended as an alternative to Close() in error callbacks.
  // Close() should still be called at some point.
  void Poison();

  // Raze() the database and Poison() the handle.  Returns the return
  // value from Raze().
  // TODO(shess): Rename to RazeAndPoison().
  bool RazeAndClose();

  // Delete the underlying database files associated with |path|.
  // This should be used on a database which has no existing
  // connections.  If any other connections are open to the same
  // database, this could cause odd results or corruption (for
  // instance if a hot journal is deleted but the associated database
  // is not).
  //
  // Returns true if the database file and associated journals no
  // longer exist, false otherwise.  If the database has never
  // existed, this will return true.
  static bool Delete(const base::FilePath& path);

  // Transactions --------------------------------------------------------------

  // Transaction management. We maintain a virtual transaction stack to emulate
  // nested transactions since sqlite can't do nested transactions. The
  // limitation is you can't roll back a sub transaction: if any transaction
  // fails, all transactions open will also be rolled back. Any nested
  // transactions after one has rolled back will return fail for Begin(). If
  // Begin() fails, you must not call Commit or Rollback().
  //
  // Normally you should use sql::Transaction to manage a transaction, which
  // will scope it to a C++ context.
  bool BeginTransaction();
  void RollbackTransaction();
  bool CommitTransaction();

  // Rollback all outstanding transactions.  Use with care, there may
  // be scoped transactions on the stack.
  void RollbackAllTransactions();

  // Returns the current transaction nesting, which will be 0 if there are
  // no open transactions.
  int transaction_nesting() const { return transaction_nesting_; }

  // Attached databases---------------------------------------------------------

  // SQLite supports attaching multiple database files to a single
  // handle.  Attach the database in |other_db_path| to the current
  // handle under |attachment_point|.  |attachment_point| should only
  // contain characters from [a-zA-Z0-9_].
  //
  // Note that calling attach or detach with an open transaction is an
  // error.
  bool AttachDatabase(const base::FilePath& other_db_path,
                      const char* attachment_point);
  bool DetachDatabase(const char* attachment_point);

  // Statements ----------------------------------------------------------------

  // Executes the given SQL string, returning true on success. This is
  // normally used for simple, 1-off statements that don't take any bound
  // parameters and don't return any data (e.g. CREATE TABLE).
  //
  // This will DCHECK if the |sql| contains errors.
  //
  // Do not use ignore_result() to ignore all errors.  Use
  // ExecuteAndReturnErrorCode() and ignore only specific errors.
  bool Execute(const char* sql) WARN_UNUSED_RESULT;

  // Like Execute(), but returns the error code given by SQLite.
  int ExecuteAndReturnErrorCode(const char* sql) WARN_UNUSED_RESULT;

  // Returns true if we have a statement with the given identifier already
  // cached. This is normally not necessary to call, but can be useful if the
  // caller has to dynamically build up SQL to avoid doing so if it's already
  // cached.
  bool HasCachedStatement(const StatementID& id) const;

  // Returns a statement for the given SQL using the statement cache. It can
  // take a nontrivial amount of work to parse and compile a statement, so
  // keeping commonly-used ones around for future use is important for
  // performance.
  //
  // If the |sql| has an error, an invalid, inert StatementRef is returned (and
  // the code will crash in debug). The caller must deal with this eventuality,
  // either by checking validity of the |sql| before calling, by correctly
  // handling the return of an inert statement, or both.
  //
  // The StatementID and the SQL must always correspond to one-another. The
  // ID is the lookup into the cache, so crazy things will happen if you use
  // different SQL with the same ID.
  //
  // You will normally use the SQL_FROM_HERE macro to generate a statement
  // ID associated with the current line of code. This gives uniqueness without
  // you having to manage unique names. See StatementID above for more.
  //
  // Example:
  //   sql::Statement stmt(connection_.GetCachedStatement(
  //       SQL_FROM_HERE, "SELECT * FROM foo"));
  //   if (!stmt)
  //     return false;  // Error creating statement.
  scoped_refptr<StatementRef> GetCachedStatement(const StatementID& id,
                                                 const char* sql);

  // Used to check a |sql| statement for syntactic validity. If the statement is
  // valid SQL, returns true.
  bool IsSQLValid(const char* sql);

  // Returns a non-cached statement for the given SQL. Use this for SQL that
  // is only executed once or only rarely (there is overhead associated with
  // keeping a statement cached).
  //
  // See GetCachedStatement above for examples and error information.
  scoped_refptr<StatementRef> GetUniqueStatement(const char* sql);

  // Info querying -------------------------------------------------------------

  // Returns true if the given table exists.
  bool DoesTableExist(const char* table_name) const;

  // Returns true if the given index exists.
  bool DoesIndexExist(const char* index_name) const;

  // Returns true if a column with the given name exists in the given table.
  bool DoesColumnExist(const char* table_name, const char* column_name) const;

  // Returns sqlite's internal ID for the last inserted row. Valid only
  // immediately after an insert.
  int64 GetLastInsertRowId() const;

  // Returns sqlite's count of the number of rows modified by the last
  // statement executed. Will be 0 if no statement has executed or the database
  // is closed.
  int GetLastChangeCount() const;

  // Errors --------------------------------------------------------------------

  // Returns the error code associated with the last sqlite operation.
  int GetErrorCode() const;

  // Returns the errno associated with GetErrorCode().  See
  // SQLITE_LAST_ERRNO in SQLite documentation.
  int GetLastErrno() const;

  // Returns a pointer to a statically allocated string associated with the
  // last sqlite operation.
  const char* GetErrorMessage() const;

  // Return a reproducible representation of the schema equivalent to
  // running the following statement at a sqlite3 command-line:
  //   SELECT type, name, tbl_name, sql FROM sqlite_master ORDER BY 1, 2, 3, 4;
  std::string GetSchema() const;

  // Clients which provide an error_callback don't see the
  // error-handling at the end of OnSqliteError().  Expose to allow
  // those clients to work appropriately with ScopedErrorIgnorer in
  // tests.
  static bool ShouldIgnoreSqliteError(int error);

 private:
  // For recovery module.
  friend class Recovery;

  // Allow test-support code to set/reset error ignorer.
  friend class ScopedErrorIgnorer;

  // Statement accesses StatementRef which we don't want to expose to everybody
  // (they should go through Statement).
  friend class Statement;

  // Internal initialize function used by both Init and InitInMemory. The file
  // name is always 8 bits since we want to use the 8-bit version of
  // sqlite3_open. The string can also be sqlite's special ":memory:" string.
  //
  // |retry_flag| controls retrying the open if the error callback
  // addressed errors using RazeAndClose().
  enum Retry {
    NO_RETRY = 0,
    RETRY_ON_POISON
  };
  bool OpenInternal(const std::string& file_name, Retry retry_flag);

  // Internal close function used by Close() and RazeAndClose().
  // |forced| indicates that orderly-shutdown checks should not apply.
  void CloseInternal(bool forced);

  // Check whether the current thread is allowed to make IO calls, but only
  // if database wasn't open in memory. Function is inlined to be a no-op in
  // official build.
  void AssertIOAllowed() {
    if (!in_memory_)
      base::ThreadRestrictions::AssertIOAllowed();
  }

  // Internal helper for DoesTableExist and DoesIndexExist.
  bool DoesTableOrIndexExist(const char* name, const char* type) const;

  // Accessors for global error-ignorer, for injecting behavior during tests.
  // See test/scoped_error_ignorer.h.
  typedef base::Callback<bool(int)> ErrorIgnorerCallback;
  static ErrorIgnorerCallback* current_ignorer_cb_;
  static void SetErrorIgnorer(ErrorIgnorerCallback* ignorer);
  static void ResetErrorIgnorer();

  // A StatementRef is a refcounted wrapper around a sqlite statement pointer.
  // Refcounting allows us to give these statements out to sql::Statement
  // objects while also optionally maintaining a cache of compiled statements
  // by just keeping a refptr to these objects.
  //
  // A statement ref can be valid, in which case it can be used, or invalid to
  // indicate that the statement hasn't been created yet, has an error, or has
  // been destroyed.
  //
  // The Connection may revoke a StatementRef in some error cases, so callers
  // should always check validity before using.
  class SQL_EXPORT StatementRef : public base::RefCounted<StatementRef> {
   public:
    // |connection| is the sql::Connection instance associated with
    // the statement, and is used for tracking outstanding statements
    // and for error handling.  Set to NULL for invalid or untracked
    // refs.  |stmt| is the actual statement, and should only be NULL
    // to create an invalid ref.  |was_valid| indicates whether the
    // statement should be considered valid for diagnistic purposes.
    // |was_valid| can be true for NULL |stmt| if the connection has
    // been forcibly closed by an error handler.
    StatementRef(Connection* connection, sqlite3_stmt* stmt, bool was_valid);

    // When true, the statement can be used.
    bool is_valid() const { return !!stmt_; }

    // When true, the statement is either currently valid, or was
    // previously valid but the connection was forcibly closed.  Used
    // for diagnostic checks.
    bool was_valid() const { return was_valid_; }

    // If we've not been linked to a connection, this will be NULL.
    // TODO(shess): connection_ can be NULL in case of GetUntrackedStatement(),
    // which prevents Statement::OnError() from forwarding errors.
    Connection* connection() const { return connection_; }

    // Returns the sqlite statement if any. If the statement is not active,
    // this will return NULL.
    sqlite3_stmt* stmt() const { return stmt_; }

    // Destroys the compiled statement and marks it NULL. The statement will
    // no longer be active.  |forced| is used to indicate if orderly-shutdown
    // checks should apply (see Connection::RazeAndClose()).
    void Close(bool forced);

    // Check whether the current thread is allowed to make IO calls, but only
    // if database wasn't open in memory.
    void AssertIOAllowed() { if (connection_) connection_->AssertIOAllowed(); }

   private:
    friend class base::RefCounted<StatementRef>;

    ~StatementRef();

    Connection* connection_;
    sqlite3_stmt* stmt_;
    bool was_valid_;

    DISALLOW_COPY_AND_ASSIGN(StatementRef);
  };
  friend class StatementRef;

  // Executes a rollback statement, ignoring all transaction state. Used
  // internally in the transaction management code.
  void DoRollback();

  // Called by a StatementRef when it's being created or destroyed. See
  // open_statements_ below.
  void StatementRefCreated(StatementRef* ref);
  void StatementRefDeleted(StatementRef* ref);

  // Called when a sqlite function returns an error, which is passed
  // as |err|.  The return value is the error code to be reflected
  // back to client code.  |stmt| is non-NULL if the error relates to
  // an sql::Statement instance.  |sql| is non-NULL if the error
  // relates to non-statement sql code (Execute, for instance).  Both
  // can be NULL, but both should never be set.
  // NOTE(shess): Originally, the return value was intended to allow
  // error handlers to transparently convert errors into success.
  // Unfortunately, transactions are not generally restartable, so
  // this did not work out.
  int OnSqliteError(int err, Statement* stmt, const char* sql);

  // Like |Execute()|, but retries if the database is locked.
  bool ExecuteWithTimeout(const char* sql, base::TimeDelta ms_timeout)
      WARN_UNUSED_RESULT;

  // Internal helper for const functions.  Like GetUniqueStatement(),
  // except the statement is not entered into open_statements_,
  // allowing this function to be const.  Open statements can block
  // closing the database, so only use in cases where the last ref is
  // released before close could be called (which should always be the
  // case for const functions).
  scoped_refptr<StatementRef> GetUntrackedStatement(const char* sql) const;

  bool IntegrityCheckHelper(
      const char* pragma_sql,
      std::vector<std::string>* messages) WARN_UNUSED_RESULT;

  // The actual sqlite database. Will be NULL before Init has been called or if
  // Init resulted in an error.
  sqlite3* db_;

  // Parameters we'll configure in sqlite before doing anything else. Zero means
  // use the default value.
  int page_size_;
  int cache_size_;
  bool exclusive_locking_;
  bool restrict_to_user_;

  // All cached statements. Keeping a reference to these statements means that
  // they'll remain active.
  typedef std::map<StatementID, scoped_refptr<StatementRef> >
      CachedStatementMap;
  CachedStatementMap statement_cache_;

  // A list of all StatementRefs we've given out. Each ref must register with
  // us when it's created or destroyed. This allows us to potentially close
  // any open statements when we encounter an error.
  typedef std::set<StatementRef*> StatementRefSet;
  StatementRefSet open_statements_;

  // Number of currently-nested transactions.
  int transaction_nesting_;

  // True if any of the currently nested transactions have been rolled back.
  // When we get to the outermost transaction, this will determine if we do
  // a rollback instead of a commit.
  bool needs_rollback_;

  // True if database is open with OpenInMemory(), False if database is open
  // with Open().
  bool in_memory_;

  // |true| if the connection was closed using RazeAndClose().  Used
  // to enable diagnostics to distinguish calls to never-opened
  // databases (incorrect use of the API) from calls to once-valid
  // databases.
  bool poisoned_;

  ErrorCallback error_callback_;

  // Tag for auxiliary histograms.
  std::string histogram_tag_;

  DISALLOW_COPY_AND_ASSIGN(Connection);
};

}  // namespace sql

#endif  // SQL_CONNECTION_H_
