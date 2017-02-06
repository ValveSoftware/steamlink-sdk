// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_storage.h"

#include <stddef.h>
#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_string_value_serializer.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/bookmarks/browser/bookmark_codec.h"
#include "components/bookmarks/browser/bookmark_index.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_constants.h"

using base::TimeTicks;

namespace bookmarks {

namespace {

// Extension used for backup files (copy of main file created during startup).
const base::FilePath::CharType kBackupExtension[] = FILE_PATH_LITERAL("bak");

// How often we save.
const int kSaveDelayMS = 2500;

void BackupCallback(const base::FilePath& path) {
  base::FilePath backup_path = path.ReplaceExtension(kBackupExtension);
  base::CopyFile(path, backup_path);
}

// Adds node to the model's index, recursing through all children as well.
void AddBookmarksToIndex(BookmarkLoadDetails* details,
                         BookmarkNode* node) {
  if (node->is_url()) {
    if (node->url().is_valid())
      details->index()->Add(node);
  } else {
    for (int i = 0; i < node->child_count(); ++i)
      AddBookmarksToIndex(details, node->GetChild(i));
  }
}

void LoadCallback(const base::FilePath& path,
                  const base::WeakPtr<BookmarkStorage>& storage,
                  std::unique_ptr<BookmarkLoadDetails> details,
                  base::SequencedTaskRunner* task_runner) {
  bool load_index = false;
  bool bookmark_file_exists = base::PathExists(path);
  if (bookmark_file_exists) {
    JSONFileValueDeserializer deserializer(path);
    std::unique_ptr<base::Value> root = deserializer.Deserialize(NULL, NULL);

    if (root.get()) {
      // Building the index can take a while, so we do it on the background
      // thread.
      int64_t max_node_id = 0;
      BookmarkCodec codec;
      TimeTicks start_time = TimeTicks::Now();
      codec.Decode(details->bb_node(), details->other_folder_node(),
                   details->mobile_folder_node(), &max_node_id, *root.get());
      details->set_max_id(std::max(max_node_id, details->max_id()));
      details->set_computed_checksum(codec.computed_checksum());
      details->set_stored_checksum(codec.stored_checksum());
      details->set_ids_reassigned(codec.ids_reassigned());
      details->set_model_meta_info_map(codec.model_meta_info_map());
      details->set_model_sync_transaction_version(
          codec.model_sync_transaction_version());
      UMA_HISTOGRAM_TIMES("Bookmarks.DecodeTime",
                          TimeTicks::Now() - start_time);

      load_index = true;
    }
  }

  // Load any extra root nodes now, after the IDs have been potentially
  // reassigned.
  details->LoadExtraNodes();

  // Load the index if there are any bookmarks in the extra nodes.
  const BookmarkPermanentNodeList& extra_nodes = details->extra_nodes();
  for (size_t i = 0; i < extra_nodes.size(); ++i) {
    if (!extra_nodes[i]->empty()) {
      load_index = true;
      break;
    }
  }

  if (load_index) {
    TimeTicks start_time = TimeTicks::Now();
    AddBookmarksToIndex(details.get(), details->bb_node());
    AddBookmarksToIndex(details.get(), details->other_folder_node());
    AddBookmarksToIndex(details.get(), details->mobile_folder_node());
    for (size_t i = 0; i < extra_nodes.size(); ++i)
      AddBookmarksToIndex(details.get(), extra_nodes[i]);
    UMA_HISTOGRAM_TIMES("Bookmarks.CreateBookmarkIndexTime",
                        TimeTicks::Now() - start_time);
  }

  task_runner->PostTask(FROM_HERE,
                        base::Bind(&BookmarkStorage::OnLoadFinished, storage,
                                   base::Passed(&details)));
}

}  // namespace

// BookmarkLoadDetails ---------------------------------------------------------

BookmarkLoadDetails::BookmarkLoadDetails(
    BookmarkPermanentNode* bb_node,
    BookmarkPermanentNode* other_folder_node,
    BookmarkPermanentNode* mobile_folder_node,
    const LoadExtraCallback& load_extra_callback,
    BookmarkIndex* index,
    int64_t max_id)
    : bb_node_(bb_node),
      other_folder_node_(other_folder_node),
      mobile_folder_node_(mobile_folder_node),
      load_extra_callback_(load_extra_callback),
      index_(index),
      model_sync_transaction_version_(
          BookmarkNode::kInvalidSyncTransactionVersion),
      max_id_(max_id),
      ids_reassigned_(false) {}

BookmarkLoadDetails::~BookmarkLoadDetails() {
}

void BookmarkLoadDetails::LoadExtraNodes() {
  if (!load_extra_callback_.is_null())
    extra_nodes_ = load_extra_callback_.Run(&max_id_);
}

// BookmarkStorage -------------------------------------------------------------

BookmarkStorage::BookmarkStorage(
    BookmarkModel* model,
    const base::FilePath& profile_path,
    base::SequencedTaskRunner* sequenced_task_runner)
    : model_(model),
      writer_(profile_path.Append(kBookmarksFileName),
              sequenced_task_runner,
              base::TimeDelta::FromMilliseconds(kSaveDelayMS)),
      sequenced_task_runner_(sequenced_task_runner),
      weak_factory_(this) {
}

BookmarkStorage::~BookmarkStorage() {
  if (writer_.HasPendingWrite())
    writer_.DoScheduledWrite();
}

void BookmarkStorage::LoadBookmarks(
    std::unique_ptr<BookmarkLoadDetails> details,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner) {
  sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&LoadCallback, writer_.path(), weak_factory_.GetWeakPtr(),
                 base::Passed(&details), base::RetainedRef(task_runner)));
}

void BookmarkStorage::ScheduleSave() {
  switch (backup_state_) {
    case BACKUP_NONE:
      backup_state_ = BACKUP_DISPATCHED;
      sequenced_task_runner_->PostTaskAndReply(
          FROM_HERE, base::Bind(&BackupCallback, writer_.path()),
          base::Bind(&BookmarkStorage::OnBackupFinished,
                     weak_factory_.GetWeakPtr()));
      return;
    case BACKUP_DISPATCHED:
      // Currently doing a backup which will call this function when done.
      return;
    case BACKUP_ATTEMPTED:
      writer_.ScheduleWrite(this);
      return;
  }
  NOTREACHED();
}

void BookmarkStorage::OnBackupFinished() {
  backup_state_ = BACKUP_ATTEMPTED;
  ScheduleSave();
}

void BookmarkStorage::BookmarkModelDeleted() {
  // We need to save now as otherwise by the time SaveNow is invoked
  // the model is gone.
  if (writer_.HasPendingWrite())
    SaveNow();
  model_ = NULL;
}

bool BookmarkStorage::SerializeData(std::string* output) {
  BookmarkCodec codec;
  std::unique_ptr<base::Value> value(codec.Encode(model_));
  JSONStringValueSerializer serializer(output);
  serializer.set_pretty_print(true);
  return serializer.Serialize(*(value.get()));
}

void BookmarkStorage::OnLoadFinished(
    std::unique_ptr<BookmarkLoadDetails> details) {
  if (!model_)
    return;

  model_->DoneLoading(std::move(details));
}

bool BookmarkStorage::SaveNow() {
  if (!model_ || !model_->loaded()) {
    // We should only get here if we have a valid model and it's finished
    // loading.
    NOTREACHED();
    return false;
  }

  std::unique_ptr<std::string> data(new std::string);
  if (!SerializeData(data.get()))
    return false;
  writer_.WriteNow(std::move(data));
  return true;
}

}  // namespace bookmarks
