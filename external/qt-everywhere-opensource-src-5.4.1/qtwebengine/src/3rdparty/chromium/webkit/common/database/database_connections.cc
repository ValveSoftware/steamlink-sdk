// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/common/database/database_connections.h"

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"

namespace webkit_database {

DatabaseConnections::DatabaseConnections() {
}

DatabaseConnections::~DatabaseConnections() {
  DCHECK(connections_.empty());
}

bool DatabaseConnections::IsEmpty() const {
  return connections_.empty();
}

bool DatabaseConnections::IsDatabaseOpened(
    const std::string& origin_identifier,
    const base::string16& database_name) const {
  OriginConnections::const_iterator origin_it =
      connections_.find(origin_identifier);
  if (origin_it == connections_.end())
    return false;
  const DBConnections& origin_connections = origin_it->second;
  return (origin_connections.find(database_name) != origin_connections.end());
}

bool DatabaseConnections::IsOriginUsed(
    const std::string& origin_identifier) const {
  return (connections_.find(origin_identifier) != connections_.end());
}

bool DatabaseConnections::AddConnection(
    const std::string& origin_identifier,
    const base::string16& database_name) {
  int& count = connections_[origin_identifier][database_name].first;
  return ++count == 1;
}

bool DatabaseConnections::RemoveConnection(
    const std::string& origin_identifier,
    const base::string16& database_name) {
  return RemoveConnectionsHelper(origin_identifier, database_name, 1);
}

void DatabaseConnections::RemoveAllConnections() {
  connections_.clear();
}

void DatabaseConnections::RemoveConnections(
    const DatabaseConnections& connections,
    std::vector<std::pair<std::string, base::string16> >* closed_dbs) {
  for (OriginConnections::const_iterator origin_it =
           connections.connections_.begin();
       origin_it != connections.connections_.end();
       origin_it++) {
    const DBConnections& db_connections = origin_it->second;
    for (DBConnections::const_iterator db_it = db_connections.begin();
         db_it != db_connections.end(); db_it++) {
      if (RemoveConnectionsHelper(origin_it->first, db_it->first,
                                  db_it->second.first))
        closed_dbs->push_back(std::make_pair(origin_it->first, db_it->first));
    }
  }
}

int64 DatabaseConnections::GetOpenDatabaseSize(
    const std::string& origin_identifier,
    const base::string16& database_name) const {
  DCHECK(IsDatabaseOpened(origin_identifier, database_name));
  return connections_[origin_identifier][database_name].second;
}

void DatabaseConnections::SetOpenDatabaseSize(
    const std::string& origin_identifier,
    const base::string16& database_name,
    int64 size) {
  DCHECK(IsDatabaseOpened(origin_identifier, database_name));
  connections_[origin_identifier][database_name].second = size;
}

void DatabaseConnections::ListConnections(
    std::vector<std::pair<std::string, base::string16> > *list) const {
  for (OriginConnections::const_iterator origin_it =
           connections_.begin();
       origin_it != connections_.end();
       origin_it++) {
    const DBConnections& db_connections = origin_it->second;
    for (DBConnections::const_iterator db_it = db_connections.begin();
         db_it != db_connections.end(); db_it++) {
      list->push_back(std::make_pair(origin_it->first, db_it->first));
    }
  }
}

bool DatabaseConnections::RemoveConnectionsHelper(
    const std::string& origin_identifier,
    const base::string16& database_name,
    int num_connections) {
  OriginConnections::iterator origin_iterator =
      connections_.find(origin_identifier);
  DCHECK(origin_iterator != connections_.end());
  DBConnections& db_connections = origin_iterator->second;
  int& count = db_connections[database_name].first;
  DCHECK(count >= num_connections);
  count -= num_connections;
  if (count)
    return false;
  db_connections.erase(database_name);
  if (db_connections.empty())
    connections_.erase(origin_iterator);
  return true;
}

DatabaseConnectionsWrapper::DatabaseConnectionsWrapper()
    : waiting_for_dbs_to_close_(false),
      main_thread_(base::MessageLoopProxy::current()) {
}

DatabaseConnectionsWrapper::~DatabaseConnectionsWrapper() {
}

void DatabaseConnectionsWrapper::WaitForAllDatabasesToClose() {
  // We assume that new databases won't be open while we're waiting.
  DCHECK(main_thread_->BelongsToCurrentThread());
  if (HasOpenConnections()) {
    base::AutoReset<bool> auto_reset(&waiting_for_dbs_to_close_, true);
    base::MessageLoop::ScopedNestableTaskAllower allow(
        base::MessageLoop::current());
    base::MessageLoop::current()->Run();
  }
}

bool DatabaseConnectionsWrapper::HasOpenConnections() {
  DCHECK(main_thread_->BelongsToCurrentThread());
  base::AutoLock auto_lock(open_connections_lock_);
  return !open_connections_.IsEmpty();
}

void DatabaseConnectionsWrapper::AddOpenConnection(
    const std::string& origin_identifier,
    const base::string16& database_name) {
  // We add to the collection immediately on any thread.
  base::AutoLock auto_lock(open_connections_lock_);
  open_connections_.AddConnection(origin_identifier, database_name);
}

void DatabaseConnectionsWrapper::RemoveOpenConnection(
    const std::string& origin_identifier,
    const base::string16& database_name) {
  // But only remove from the collection on the main thread
  // so we can handle the waiting_for_dbs_to_close_ case.
  if (!main_thread_->BelongsToCurrentThread()) {
    main_thread_->PostTask(
        FROM_HERE,
        base::Bind(&DatabaseConnectionsWrapper::RemoveOpenConnection, this,
                   origin_identifier, database_name));
    return;
  }
  base::AutoLock auto_lock(open_connections_lock_);
  open_connections_.RemoveConnection(origin_identifier, database_name);
  if (waiting_for_dbs_to_close_ && open_connections_.IsEmpty())
    base::MessageLoop::current()->Quit();
}

}  // namespace webkit_database
