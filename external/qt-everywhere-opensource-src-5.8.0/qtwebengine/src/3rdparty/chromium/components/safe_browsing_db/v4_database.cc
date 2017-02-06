// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/callback.h"
#include "base/debug/leak_annotations.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "components/safe_browsing_db/v4_database.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace safe_browsing {

// static
V4StoreFactory* V4Database::factory_ = NULL;

// static
void V4Database::Create(
    const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
    const base::FilePath& base_path,
    const StoreFileNameMap& store_file_name_map,
    NewDatabaseReadyCallback new_db_callback) {
  DCHECK(base_path.IsAbsolute());
  DCHECK(!store_file_name_map.empty());

  const scoped_refptr<base::SingleThreadTaskRunner>& callback_task_runner =
      base::MessageLoop::current()->task_runner();
  db_task_runner->PostTask(
      FROM_HERE,
      base::Bind(&V4Database::CreateOnTaskRunner, db_task_runner, base_path,
                 store_file_name_map, callback_task_runner, new_db_callback));
}

// static
void V4Database::CreateOnTaskRunner(
    const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
    const base::FilePath& base_path,
    const StoreFileNameMap& store_file_name_map,
    const scoped_refptr<base::SingleThreadTaskRunner>& callback_task_runner,
    NewDatabaseReadyCallback new_db_callback) {
  DCHECK(db_task_runner->RunsTasksOnCurrentThread());

  if (!factory_) {
    factory_ = new V4StoreFactory();
    ANNOTATE_LEAKING_OBJECT_PTR(factory_);
  }

  if (!base::CreateDirectory(base_path)) {
    NOTREACHED();
  }

  std::unique_ptr<StoreMap> store_map = base::MakeUnique<StoreMap>();
  for (const auto& store_info : store_file_name_map) {
    const UpdateListIdentifier& update_list_identifier = store_info.first;
    const base::FilePath store_path = base_path.AppendASCII(store_info.second);
    (*store_map)[update_list_identifier].reset(
        factory_->CreateV4Store(db_task_runner, store_path));
  }
  std::unique_ptr<V4Database> v4_database(
      new V4Database(db_task_runner, std::move(store_map)));

  // Database is done loading, pass it to the new_db_callback on the caller's
  // thread.
  callback_task_runner->PostTask(
      FROM_HERE, base::Bind(new_db_callback, base::Passed(&v4_database)));
}

V4Database::V4Database(
    const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
    std::unique_ptr<StoreMap> store_map)
    : db_task_runner_(db_task_runner),
      store_map_(std::move(store_map)),
      pending_store_updates_(0) {
  DCHECK(db_task_runner->RunsTasksOnCurrentThread());
  // TODO(vakh): Implement skeleton
}

// static
void V4Database::Destroy(std::unique_ptr<V4Database> v4_database) {
  V4Database* v4_database_raw = v4_database.release();
  if (v4_database_raw) {
    v4_database_raw->db_task_runner_->DeleteSoon(FROM_HERE, v4_database_raw);
  }
}

V4Database::~V4Database() {
  DCHECK(db_task_runner_->RunsTasksOnCurrentThread());
}

void V4Database::ApplyUpdate(
    std::unique_ptr<ParsedServerResponse> parsed_server_response,
    DatabaseUpdatedCallback db_updated_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!pending_store_updates_);
  DCHECK(db_updated_callback_.is_null());

  db_updated_callback_ = db_updated_callback;

  // Post the V4Store update task on the task runner but get the callback on the
  // current thread.
  const scoped_refptr<base::SingleThreadTaskRunner>& current_task_runner =
      base::MessageLoop::current()->task_runner();
  for (std::unique_ptr<ListUpdateResponse>& response :
       *parsed_server_response) {
    UpdateListIdentifier identifier(*response);
    StoreMap::const_iterator iter = store_map_->find(identifier);
    if (iter != store_map_->end()) {
      const std::unique_ptr<V4Store>& old_store = iter->second;
      if (old_store->state() != response->new_client_state()) {
        // A different state implies there are updates to process.
        pending_store_updates_++;
        UpdatedStoreReadyCallback store_ready_callback = base::Bind(
            &V4Database::UpdatedStoreReady, base::Unretained(this), identifier);
        db_task_runner_->PostTask(
            FROM_HERE,
            base::Bind(&V4Store::ApplyUpdate, base::Unretained(old_store.get()),
                       base::Passed(std::move(response)), current_task_runner,
                       store_ready_callback));
      }
    } else {
      NOTREACHED() << "Got update for unexpected identifier: " << identifier;
    }
  }

  if (!pending_store_updates_) {
    current_task_runner->PostTask(FROM_HERE, db_updated_callback_);
    db_updated_callback_.Reset();
  }
}

void V4Database::UpdatedStoreReady(UpdateListIdentifier identifier,
                                   std::unique_ptr<V4Store> new_store) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(pending_store_updates_);
  (*store_map_)[identifier] = std::move(new_store);

  pending_store_updates_--;
  if (!pending_store_updates_) {
    db_updated_callback_.Run();
    db_updated_callback_.Reset();
  }
}

bool V4Database::ResetDatabase() {
  DCHECK(db_task_runner_->RunsTasksOnCurrentThread());
  bool reset_success = true;
  for (const auto& store_map_iter : *store_map_) {
    if (!store_map_iter.second->Reset()) {
      reset_success = false;
    }
  }
  return reset_success;
}

std::unique_ptr<StoreStateMap> V4Database::GetStoreStateMap() {
  std::unique_ptr<StoreStateMap> store_state_map =
      base::MakeUnique<StoreStateMap>();
  for (const auto& store_map_iter : *store_map_) {
    (*store_state_map)[store_map_iter.first] = store_map_iter.second->state();
  }
  return store_state_map;
}

}  // namespace safe_browsing
