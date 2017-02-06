// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/webrtc/webrtc_identity_store_backend.h"

#include <stddef.h>
#include <stdint.h>

#include <tuple>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/string_util.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_errors.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "url/gurl.h"

namespace content {

static const char kWebRTCIdentityStoreDBName[] = "webrtc_identity_store";

static const base::FilePath::CharType kWebRTCIdentityStoreDirectory[] =
    FILE_PATH_LITERAL("WebRTCIdentityStore");

// Initializes the identity table, returning true on success.
static bool InitDB(sql::Connection* db) {
  if (db->DoesTableExist(kWebRTCIdentityStoreDBName)) {
    if (db->DoesColumnExist(kWebRTCIdentityStoreDBName, "origin") &&
        db->DoesColumnExist(kWebRTCIdentityStoreDBName, "identity_name") &&
        db->DoesColumnExist(kWebRTCIdentityStoreDBName, "common_name") &&
        db->DoesColumnExist(kWebRTCIdentityStoreDBName, "certificate") &&
        db->DoesColumnExist(kWebRTCIdentityStoreDBName, "private_key") &&
        db->DoesColumnExist(kWebRTCIdentityStoreDBName, "creation_time"))
      return true;

    if (!db->Execute("DROP TABLE webrtc_identity_store"))
      return false;
  }

  return db->Execute(
      "CREATE TABLE webrtc_identity_store"
      " ("
      "origin TEXT NOT NULL,"
      "identity_name TEXT NOT NULL,"
      "common_name TEXT NOT NULL,"
      "certificate BLOB NOT NULL,"
      "private_key BLOB NOT NULL,"
      "creation_time INTEGER)");
}

struct WebRTCIdentityStoreBackend::IdentityKey {
  IdentityKey(const GURL& origin, const std::string& identity_name)
      : origin(origin), identity_name(identity_name) {}

  bool operator<(const IdentityKey& other) const {
    return std::tie(origin, identity_name) <
           std::tie(other.origin, other.identity_name);
  }

  GURL origin;
  std::string identity_name;
};

struct WebRTCIdentityStoreBackend::Identity {
  Identity(const std::string& common_name,
           const std::string& certificate,
           const std::string& private_key)
      : common_name(common_name),
        certificate(certificate),
        private_key(private_key),
        creation_time(base::Time::Now().ToInternalValue()) {}

  Identity(const std::string& common_name,
           const std::string& certificate,
           const std::string& private_key,
           int64_t creation_time)
      : common_name(common_name),
        certificate(certificate),
        private_key(private_key),
        creation_time(creation_time) {}

  std::string common_name;
  std::string certificate;
  std::string private_key;
  int64_t creation_time;
};

struct WebRTCIdentityStoreBackend::PendingFindRequest {
  PendingFindRequest(const GURL& origin,
                     const std::string& identity_name,
                     const std::string& common_name,
                     const FindIdentityCallback& callback)
      : origin(origin),
        identity_name(identity_name),
        common_name(common_name),
        callback(callback) {}

  ~PendingFindRequest() {}

  GURL origin;
  std::string identity_name;
  std::string common_name;
  FindIdentityCallback callback;
};

// The class encapsulates the database operations. All members except ctor and
// dtor should be accessed on the DB thread.
// It can be created/destroyed on any thread.
class WebRTCIdentityStoreBackend::SqlLiteStorage
    : public base::RefCountedThreadSafe<SqlLiteStorage> {
 public:
  SqlLiteStorage(base::TimeDelta validity_period,
                 const base::FilePath& path,
                 storage::SpecialStoragePolicy* policy)
      : validity_period_(validity_period), special_storage_policy_(policy) {
    if (!path.empty())
      path_ = path.Append(kWebRTCIdentityStoreDirectory);
  }

  void Load(IdentityMap* out_map);
  void Close();
  void AddIdentity(const GURL& origin,
                   const std::string& identity_name,
                   const Identity& identity);
  void DeleteIdentity(const GURL& origin,
                      const std::string& identity_name,
                      const Identity& identity);
  void DeleteBetween(base::Time delete_begin, base::Time delete_end);

  void SetValidityPeriodForTesting(base::TimeDelta validity_period) {
    DCHECK_CURRENTLY_ON(BrowserThread::DB);
    DCHECK(!db_.get());
    validity_period_ = validity_period;
  }

 private:
  friend class base::RefCountedThreadSafe<SqlLiteStorage>;

  enum OperationType {
    ADD_IDENTITY,
    DELETE_IDENTITY
  };
  struct PendingOperation {
    PendingOperation(OperationType type,
                     const GURL& origin,
                     const std::string& identity_name,
                     const Identity& identity)
        : type(type),
          origin(origin),
          identity_name(identity_name),
          identity(identity) {}

    OperationType type;
    GURL origin;
    std::string identity_name;
    Identity identity;
  };
  typedef ScopedVector<PendingOperation> PendingOperationList;

  virtual ~SqlLiteStorage() {}
  void OnDatabaseError(int error, sql::Statement* stmt);
  void BatchOperation(OperationType type,
                      const GURL& origin,
                      const std::string& identity_name,
                      const Identity& identity);
  void Commit();

  base::TimeDelta validity_period_;
  // The file path of the DB. Empty if temporary.
  base::FilePath path_;
  scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy_;
  std::unique_ptr<sql::Connection> db_;
  // Batched DB operations pending to commit.
  PendingOperationList pending_operations_;

  DISALLOW_COPY_AND_ASSIGN(SqlLiteStorage);
};

WebRTCIdentityStoreBackend::WebRTCIdentityStoreBackend(
    const base::FilePath& path,
    storage::SpecialStoragePolicy* policy,
    base::TimeDelta validity_period)
    : validity_period_(validity_period),
      state_(NOT_STARTED),
      sql_lite_storage_(new SqlLiteStorage(validity_period, path, policy)) {
}

bool WebRTCIdentityStoreBackend::FindIdentity(
    const GURL& origin,
    const std::string& identity_name,
    const std::string& common_name,
    const FindIdentityCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (state_ == CLOSED)
    return false;

  if (state_ != LOADED) {
    // Queues the request to wait for the DB to load.
    pending_find_requests_.push_back(
        new PendingFindRequest(origin, identity_name, common_name, callback));
    if (state_ == LOADING)
      return true;

    DCHECK_EQ(state_, NOT_STARTED);

    // Kick off loading the DB.
    std::unique_ptr<IdentityMap> out_map(new IdentityMap());
    base::Closure task(
        base::Bind(&SqlLiteStorage::Load, sql_lite_storage_, out_map.get()));
    // |out_map| will be NULL after this call.
    if (BrowserThread::PostTaskAndReply(
            BrowserThread::DB,
            FROM_HERE,
            task,
            base::Bind(&WebRTCIdentityStoreBackend::OnLoaded,
                       this,
                       base::Passed(&out_map)))) {
      state_ = LOADING;
      return true;
    }
    // If it fails to post task, falls back to ERR_FILE_NOT_FOUND.
  }

  IdentityKey key(origin, identity_name);
  IdentityMap::iterator iter = identities_.find(key);
  if (iter != identities_.end() && iter->second.common_name == common_name) {
    base::TimeDelta age = base::Time::Now() - base::Time::FromInternalValue(
                                                  iter->second.creation_time);
    if (age < validity_period_) {
      // Identity found.
      return BrowserThread::PostTask(BrowserThread::IO,
                                     FROM_HERE,
                                     base::Bind(callback,
                                                net::OK,
                                                iter->second.certificate,
                                                iter->second.private_key));
    }
    // Removes the expired identity from the in-memory cache. The copy in the
    // database will be removed on the next load.
    identities_.erase(iter);
  }

  return BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(callback, net::ERR_FILE_NOT_FOUND, "", ""));
}

void WebRTCIdentityStoreBackend::AddIdentity(const GURL& origin,
                                             const std::string& identity_name,
                                             const std::string& common_name,
                                             const std::string& certificate,
                                             const std::string& private_key) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (state_ == CLOSED)
    return;

  // If there is an existing identity for the same origin and identity_name,
  // delete it.
  IdentityKey key(origin, identity_name);
  Identity identity(common_name, certificate, private_key);

  if (identities_.find(key) != identities_.end()) {
    if (!BrowserThread::PostTask(BrowserThread::DB,
                                 FROM_HERE,
                                 base::Bind(&SqlLiteStorage::DeleteIdentity,
                                            sql_lite_storage_,
                                            origin,
                                            identity_name,
                                            identities_.find(key)->second)))
      return;
  }
  identities_.insert(std::pair<IdentityKey, Identity>(key, identity));

  BrowserThread::PostTask(BrowserThread::DB,
                          FROM_HERE,
                          base::Bind(&SqlLiteStorage::AddIdentity,
                                     sql_lite_storage_,
                                     origin,
                                     identity_name,
                                     identity));
}

void WebRTCIdentityStoreBackend::Close() {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&WebRTCIdentityStoreBackend::Close, this));
    return;
  }

  if (state_ == CLOSED)
    return;

  state_ = CLOSED;
  BrowserThread::PostTask(
      BrowserThread::DB,
      FROM_HERE,
      base::Bind(&SqlLiteStorage::Close, sql_lite_storage_));
}

void WebRTCIdentityStoreBackend::DeleteBetween(base::Time delete_begin,
                                               base::Time delete_end,
                                               const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (state_ == CLOSED)
    return;

  // Delete the in-memory cache.
  IdentityMap::iterator it = identities_.begin();
  while (it != identities_.end()) {
    if (it->second.creation_time >= delete_begin.ToInternalValue() &&
        it->second.creation_time <= delete_end.ToInternalValue()) {
      identities_.erase(it++);
    } else {
      ++it;
    }
  }
  BrowserThread::PostTaskAndReply(BrowserThread::DB,
                                  FROM_HERE,
                                  base::Bind(&SqlLiteStorage::DeleteBetween,
                                             sql_lite_storage_,
                                             delete_begin,
                                             delete_end),
                                  callback);
}

void WebRTCIdentityStoreBackend::SetValidityPeriodForTesting(
    base::TimeDelta validity_period) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  validity_period_ = validity_period;
  BrowserThread::PostTask(
      BrowserThread::DB,
      FROM_HERE,
      base::Bind(&SqlLiteStorage::SetValidityPeriodForTesting,
                 sql_lite_storage_,
                 validity_period));
}

WebRTCIdentityStoreBackend::~WebRTCIdentityStoreBackend() {}

void WebRTCIdentityStoreBackend::OnLoaded(
    std::unique_ptr<IdentityMap> out_map) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (state_ != LOADING)
    return;

  DVLOG(3) << "WebRTC identity store has loaded.";

  state_ = LOADED;
  identities_.swap(*out_map);

  for (size_t i = 0; i < pending_find_requests_.size(); ++i) {
    FindIdentity(pending_find_requests_[i]->origin,
                 pending_find_requests_[i]->identity_name,
                 pending_find_requests_[i]->common_name,
                 pending_find_requests_[i]->callback);
    delete pending_find_requests_[i];
  }
  pending_find_requests_.clear();
}

//
// Implementation of SqlLiteStorage.
//

void WebRTCIdentityStoreBackend::SqlLiteStorage::Load(IdentityMap* out_map) {
  DCHECK_CURRENTLY_ON(BrowserThread::DB);
  DCHECK(!db_.get());

  // Ensure the parent directory for storing certs is created before reading
  // from it.
  const base::FilePath dir = path_.DirName();
  if (!base::PathExists(dir) && !base::CreateDirectory(dir)) {
    DVLOG(2) << "Unable to open DB file path.";
    return;
  }

  db_.reset(new sql::Connection());

  db_->set_error_callback(base::Bind(&SqlLiteStorage::OnDatabaseError, this));

  if (!db_->Open(path_)) {
    DVLOG(2) << "Unable to open DB.";
    db_.reset();
    return;
  }

  if (!InitDB(db_.get())) {
    DVLOG(2) << "Unable to init DB.";
    db_.reset();
    return;
  }

  db_->Preload();

  // Delete expired identities.
  DeleteBetween(base::Time(), base::Time::Now() - validity_period_);

  // Slurp all the identities into the out_map.
  sql::Statement stmt(db_->GetUniqueStatement(
      "SELECT origin, identity_name, common_name, "
      "certificate, private_key, creation_time "
      "FROM webrtc_identity_store"));
  CHECK(stmt.is_valid());

  while (stmt.Step()) {
    IdentityKey key(GURL(stmt.ColumnString(0)), stmt.ColumnString(1));
    std::string common_name(stmt.ColumnString(2));
    std::string cert, private_key;
    stmt.ColumnBlobAsString(3, &cert);
    stmt.ColumnBlobAsString(4, &private_key);
    int64_t creation_time = stmt.ColumnInt64(5);
    std::pair<IdentityMap::iterator, bool> result =
        out_map->insert(std::pair<IdentityKey, Identity>(
            key, Identity(common_name, cert, private_key, creation_time)));
    DCHECK(result.second);
  }
}

void WebRTCIdentityStoreBackend::SqlLiteStorage::Close() {
  DCHECK_CURRENTLY_ON(BrowserThread::DB);
  Commit();
  db_.reset();
}

void WebRTCIdentityStoreBackend::SqlLiteStorage::AddIdentity(
    const GURL& origin,
    const std::string& identity_name,
    const Identity& identity) {
  DCHECK_CURRENTLY_ON(BrowserThread::DB);
  if (!db_.get())
    return;

  // Do not add for session only origins.
  if (special_storage_policy_.get() &&
      !special_storage_policy_->IsStorageProtected(origin) &&
      special_storage_policy_->IsStorageSessionOnly(origin)) {
    return;
  }
  BatchOperation(ADD_IDENTITY, origin, identity_name, identity);
}

void WebRTCIdentityStoreBackend::SqlLiteStorage::DeleteIdentity(
    const GURL& origin,
    const std::string& identity_name,
    const Identity& identity) {
  DCHECK_CURRENTLY_ON(BrowserThread::DB);
  if (!db_.get())
    return;
  BatchOperation(DELETE_IDENTITY, origin, identity_name, identity);
}

void WebRTCIdentityStoreBackend::SqlLiteStorage::DeleteBetween(
    base::Time delete_begin,
    base::Time delete_end) {
  DCHECK_CURRENTLY_ON(BrowserThread::DB);
  if (!db_.get())
    return;

  // Commit pending operations first.
  Commit();

  sql::Statement del_stmt(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "DELETE FROM webrtc_identity_store"
      " WHERE creation_time >= ? AND creation_time <= ?"));
  CHECK(del_stmt.is_valid());

  del_stmt.BindInt64(0, delete_begin.ToInternalValue());
  del_stmt.BindInt64(1, delete_end.ToInternalValue());

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin()) {
    DVLOG(2) << "Failed to begin the transaction.";
    return;
  }

  if (!del_stmt.Run()) {
    DVLOG(2) << "Failed to run the delete statement.";
    return;
  }

  if (!transaction.Commit())
    DVLOG(2) << "Failed to commit the transaction.";
}

void WebRTCIdentityStoreBackend::SqlLiteStorage::OnDatabaseError(
    int error,
    sql::Statement* stmt) {
  DCHECK_CURRENTLY_ON(BrowserThread::DB);

  db_->RazeAndClose();
  // It's not safe to reset |db_| here.
}

void WebRTCIdentityStoreBackend::SqlLiteStorage::BatchOperation(
    OperationType type,
    const GURL& origin,
    const std::string& identity_name,
    const Identity& identity) {
  DCHECK_CURRENTLY_ON(BrowserThread::DB);
  // Commit every 30 seconds.
  static const base::TimeDelta kCommitInterval(
      base::TimeDelta::FromSeconds(30));
  // Commit right away if we have more than 512 outstanding operations.
  static const size_t kCommitAfterBatchSize = 512;

  // We do a full copy of the cert here, and hopefully just here.
  std::unique_ptr<PendingOperation> operation(
      new PendingOperation(type, origin, identity_name, identity));

  pending_operations_.push_back(operation.release());

  if (pending_operations_.size() == 1) {
    // We've gotten our first entry for this batch, fire off the timer.
    BrowserThread::PostDelayedTask(BrowserThread::DB,
                                   FROM_HERE,
                                   base::Bind(&SqlLiteStorage::Commit, this),
                                   kCommitInterval);
  } else if (pending_operations_.size() >= kCommitAfterBatchSize) {
    // We've reached a big enough batch, fire off a commit now.
    BrowserThread::PostTask(BrowserThread::DB,
                            FROM_HERE,
                            base::Bind(&SqlLiteStorage::Commit, this));
  }
}

void WebRTCIdentityStoreBackend::SqlLiteStorage::Commit() {
  DCHECK_CURRENTLY_ON(BrowserThread::DB);
  // Maybe an old timer fired or we are already Close()'ed.
  if (!db_.get() || pending_operations_.empty())
    return;

  sql::Statement add_stmt(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO webrtc_identity_store "
      "(origin, identity_name, common_name, certificate,"
      " private_key, creation_time) VALUES"
      " (?,?,?,?,?,?)"));

  CHECK(add_stmt.is_valid());

  sql::Statement del_stmt(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "DELETE FROM webrtc_identity_store WHERE origin=? AND identity_name=?"));

  CHECK(del_stmt.is_valid());

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin()) {
    DVLOG(2) << "Failed to begin the transaction.";
    return;
  }

  // Swaps |pending_operations_| into a temporary list to make sure
  // |pending_operations_| is always cleared in case of DB errors.
  PendingOperationList pending_operations_copy;
  pending_operations_.swap(pending_operations_copy);

  for (PendingOperationList::const_iterator it =
           pending_operations_copy.begin();
       it != pending_operations_copy.end();
       ++it) {
    switch ((*it)->type) {
      case ADD_IDENTITY: {
        add_stmt.Reset(true);
        add_stmt.BindString(0, (*it)->origin.spec());
        add_stmt.BindString(1, (*it)->identity_name);
        add_stmt.BindString(2, (*it)->identity.common_name);
        const std::string& cert = (*it)->identity.certificate;
        add_stmt.BindBlob(3, cert.data(), cert.size());
        const std::string& private_key = (*it)->identity.private_key;
        add_stmt.BindBlob(4, private_key.data(), private_key.size());
        add_stmt.BindInt64(5, (*it)->identity.creation_time);
        if (!add_stmt.Run()) {
          DVLOG(2) << "Failed to add the identity to DB.";
          return;
        }
        break;
      }
      case DELETE_IDENTITY:
        del_stmt.Reset(true);
        del_stmt.BindString(0, (*it)->origin.spec());
        del_stmt.BindString(1, (*it)->identity_name);
        if (!del_stmt.Run()) {
          DVLOG(2) << "Failed to delete the identity from DB.";
          return;
        }
        break;

      default:
        NOTREACHED();
        break;
    }
  }

  if (!transaction.Commit())
    DVLOG(2) << "Failed to commit the transaction.";
}

}  // namespace content
