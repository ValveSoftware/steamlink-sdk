// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/remote/remote_suggestions_database.h"

#include <utility>

#include "base/files/file_path.h"
#include "components/leveldb_proto/proto_database_impl.h"
#include "components/ntp_snippets/remote/proto/ntp_snippets.pb.h"

using leveldb_proto::ProtoDatabaseImpl;

namespace {
// Statistics are logged to UMA with this string as part of histogram name. They
// can all be found under LevelDB.*.NTPSnippets. Changing this needs to
// synchronize with histograms.xml, AND will also become incompatible with older
// browsers still reporting the previous values.
const char kDatabaseUMAClientName[] = "NTPSnippets";
const char kImageDatabaseUMAClientName[] = "NTPSnippetImages";

const char kSnippetDatabaseFolder[] = "snippets";
const char kImageDatabaseFolder[] = "images";
}  // namespace

namespace ntp_snippets {

RemoteSuggestionsDatabase::RemoteSuggestionsDatabase(
    const base::FilePath& database_dir,
    scoped_refptr<base::SequencedTaskRunner> file_task_runner)
    : database_(new ProtoDatabaseImpl<SnippetProto>(file_task_runner)),
      database_initialized_(false),
      image_database_(
          new ProtoDatabaseImpl<SnippetImageProto>(file_task_runner)),
      image_database_initialized_(false),
      weak_ptr_factory_(this) {
  base::FilePath snippet_dir = database_dir.AppendASCII(kSnippetDatabaseFolder);
  database_->Init(kDatabaseUMAClientName, snippet_dir,
                  base::Bind(&RemoteSuggestionsDatabase::OnDatabaseInited,
                             weak_ptr_factory_.GetWeakPtr()));

  base::FilePath image_dir = database_dir.AppendASCII(kImageDatabaseFolder);
  image_database_->Init(
      kImageDatabaseUMAClientName, image_dir,
      base::Bind(&RemoteSuggestionsDatabase::OnImageDatabaseInited,
                 weak_ptr_factory_.GetWeakPtr()));
}

RemoteSuggestionsDatabase::~RemoteSuggestionsDatabase() = default;

bool RemoteSuggestionsDatabase::IsInitialized() const {
  return !IsErrorState() && database_initialized_ &&
         image_database_initialized_;
}

bool RemoteSuggestionsDatabase::IsErrorState() const {
  return !database_ || !image_database_;
}

void RemoteSuggestionsDatabase::SetErrorCallback(
    const base::Closure& error_callback) {
  error_callback_ = error_callback;
}

void RemoteSuggestionsDatabase::LoadSnippets(const SnippetsCallback& callback) {
  if (IsInitialized())
    LoadSnippetsImpl(callback);
  else
    pending_snippets_callbacks_.emplace_back(callback);
}

void RemoteSuggestionsDatabase::SaveSnippet(const NTPSnippet& snippet) {
  std::unique_ptr<KeyEntryVector> entries_to_save(new KeyEntryVector());
  // OnDatabaseLoaded relies on the detail that the primary snippet id goes
  // first in the protocol representation.
  DCHECK_EQ(snippet.ToProto().ids(0), snippet.id());
  entries_to_save->emplace_back(snippet.id(), snippet.ToProto());
  SaveSnippetsImpl(std::move(entries_to_save));
}

void RemoteSuggestionsDatabase::SaveSnippets(
    const NTPSnippet::PtrVector& snippets) {
  std::unique_ptr<KeyEntryVector> entries_to_save(new KeyEntryVector());
  for (const std::unique_ptr<NTPSnippet>& snippet : snippets) {
    // OnDatabaseLoaded relies on the detail that the primary snippet id goes
    // first in the protocol representation.
    DCHECK_EQ(snippet->ToProto().ids(0), snippet->id());
    entries_to_save->emplace_back(snippet->id(), snippet->ToProto());
  }
  SaveSnippetsImpl(std::move(entries_to_save));
}

void RemoteSuggestionsDatabase::DeleteSnippet(const std::string& snippet_id) {
  DeleteSnippets(base::MakeUnique<std::vector<std::string>>(1, snippet_id));
}

void RemoteSuggestionsDatabase::DeleteSnippets(
    std::unique_ptr<std::vector<std::string>> snippet_ids) {
  DCHECK(IsInitialized());

  std::unique_ptr<KeyEntryVector> entries_to_save(new KeyEntryVector());
  database_->UpdateEntries(
      std::move(entries_to_save), std::move(snippet_ids),
      base::Bind(&RemoteSuggestionsDatabase::OnDatabaseSaved,
                 weak_ptr_factory_.GetWeakPtr()));
}

void RemoteSuggestionsDatabase::LoadImage(
    const std::string& snippet_id,
    const SnippetImageCallback& callback) {
  if (IsInitialized())
    LoadImageImpl(snippet_id, callback);
  else
    pending_image_callbacks_.emplace_back(snippet_id, callback);
}

void RemoteSuggestionsDatabase::SaveImage(const std::string& snippet_id,
                                          const std::string& image_data) {
  DCHECK(IsInitialized());

  SnippetImageProto image_proto;
  image_proto.set_data(image_data);

  std::unique_ptr<ImageKeyEntryVector> entries_to_save(
      new ImageKeyEntryVector());
  entries_to_save->emplace_back(snippet_id, std::move(image_proto));

  image_database_->UpdateEntries(
      std::move(entries_to_save), base::MakeUnique<std::vector<std::string>>(),
      base::Bind(&RemoteSuggestionsDatabase::OnImageDatabaseSaved,
                 weak_ptr_factory_.GetWeakPtr()));
}

void RemoteSuggestionsDatabase::DeleteImage(const std::string& snippet_id) {
  DeleteImages(base::MakeUnique<std::vector<std::string>>(1, snippet_id));
}

void RemoteSuggestionsDatabase::DeleteImages(
    std::unique_ptr<std::vector<std::string>> snippet_ids) {
  DCHECK(IsInitialized());
  image_database_->UpdateEntries(
      base::MakeUnique<ImageKeyEntryVector>(), std::move(snippet_ids),
      base::Bind(&RemoteSuggestionsDatabase::OnImageDatabaseSaved,
                 weak_ptr_factory_.GetWeakPtr()));
}

void RemoteSuggestionsDatabase::GarbageCollectImages(
    std::unique_ptr<std::set<std::string>> alive_snippet_ids) {
  DCHECK(image_database_initialized_);
  image_database_->LoadKeys(
      base::Bind(&RemoteSuggestionsDatabase::DeleteUnreferencedImages,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(std::move(alive_snippet_ids))));
}

void RemoteSuggestionsDatabase::OnDatabaseInited(bool success) {
  DCHECK(!database_initialized_);
  if (!success) {
    DVLOG(1) << "RemoteSuggestionsDatabase init failed.";
    OnDatabaseError();
    return;
  }
  database_initialized_ = true;
  if (IsInitialized())
    ProcessPendingLoads();
}

void RemoteSuggestionsDatabase::OnDatabaseLoaded(
    const SnippetsCallback& callback,
    bool success,
    std::unique_ptr<std::vector<SnippetProto>> entries) {
  if (!success) {
    DVLOG(1) << "RemoteSuggestionsDatabase load failed.";
    OnDatabaseError();
    return;
  }

  std::unique_ptr<std::vector<std::string>> keys_to_remove(
      new std::vector<std::string>());

  NTPSnippet::PtrVector snippets;
  for (const SnippetProto& proto : *entries) {
    std::unique_ptr<NTPSnippet> snippet = NTPSnippet::CreateFromProto(proto);
    if (snippet) {
      snippets.emplace_back(std::move(snippet));
    } else {
      if (proto.ids_size() > 0) {
        LOG(WARNING) << "Invalid proto from DB " << proto.ids(0);
        keys_to_remove->emplace_back(proto.ids(0));
      } else {
        LOG(WARNING)
            << "Loaded proto without ID from the DB. Cannot clean this up.";
      }
    }
  }

  callback.Run(std::move(snippets));

  // If any of the snippet protos couldn't be converted to actual snippets,
  // clean them up now.
  if (!keys_to_remove->empty())
    DeleteSnippets(std::move(keys_to_remove));
}

void RemoteSuggestionsDatabase::OnDatabaseSaved(bool success) {
  if (!success) {
    DVLOG(1) << "RemoteSuggestionsDatabase save failed.";
    OnDatabaseError();
  }
}

void RemoteSuggestionsDatabase::OnImageDatabaseInited(bool success) {
  DCHECK(!image_database_initialized_);
  if (!success) {
    DVLOG(1) << "RemoteSuggestionsDatabase init failed.";
    OnDatabaseError();
    return;
  }
  image_database_initialized_ = true;
  if (IsInitialized())
    ProcessPendingLoads();
}

void RemoteSuggestionsDatabase::OnImageDatabaseLoaded(
    const SnippetImageCallback& callback,
    bool success,
    std::unique_ptr<SnippetImageProto> entry) {
  if (!success) {
    DVLOG(1) << "RemoteSuggestionsDatabase load failed.";
    OnDatabaseError();
    return;
  }

  if (!entry) {
    callback.Run(std::string());
    return;
  }

  std::unique_ptr<std::string> data(entry->release_data());
  callback.Run(std::move(*data));
}

void RemoteSuggestionsDatabase::OnImageDatabaseSaved(bool success) {
  if (!success) {
    DVLOG(1) << "RemoteSuggestionsDatabase save failed.";
    OnDatabaseError();
  }
}

void RemoteSuggestionsDatabase::OnDatabaseError() {
  database_.reset();
  image_database_.reset();
  if (!error_callback_.is_null())
    error_callback_.Run();
}

void RemoteSuggestionsDatabase::ProcessPendingLoads() {
  DCHECK(IsInitialized());

  for (const auto& callback : pending_snippets_callbacks_)
    LoadSnippetsImpl(callback);
  pending_snippets_callbacks_.clear();

  for (const auto& id_callback : pending_image_callbacks_)
    LoadImageImpl(id_callback.first, id_callback.second);
  pending_image_callbacks_.clear();
}

void RemoteSuggestionsDatabase::LoadSnippetsImpl(
    const SnippetsCallback& callback) {
  DCHECK(IsInitialized());
  database_->LoadEntries(
      base::Bind(&RemoteSuggestionsDatabase::OnDatabaseLoaded,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void RemoteSuggestionsDatabase::SaveSnippetsImpl(
    std::unique_ptr<KeyEntryVector> entries_to_save) {
  DCHECK(IsInitialized());

  std::unique_ptr<std::vector<std::string>> keys_to_remove(
      new std::vector<std::string>());
  database_->UpdateEntries(
      std::move(entries_to_save), std::move(keys_to_remove),
      base::Bind(&RemoteSuggestionsDatabase::OnDatabaseSaved,
                 weak_ptr_factory_.GetWeakPtr()));
}

void RemoteSuggestionsDatabase::LoadImageImpl(
    const std::string& snippet_id,
    const SnippetImageCallback& callback) {
  DCHECK(IsInitialized());
  image_database_->GetEntry(
      snippet_id, base::Bind(&RemoteSuggestionsDatabase::OnImageDatabaseLoaded,
                             weak_ptr_factory_.GetWeakPtr(), callback));
}

void RemoteSuggestionsDatabase::DeleteUnreferencedImages(
    std::unique_ptr<std::set<std::string>> references,
    bool load_keys_success,
    std::unique_ptr<std::vector<std::string>> image_keys) {
  if (!load_keys_success) {
    DVLOG(1) << "RemoteSuggestionsDatabase garbage collection failed.";
    OnDatabaseError();
    return;
  }
  auto keys_to_remove = base::MakeUnique<std::vector<std::string>>();
  for (const std::string& key : *image_keys) {
    if (references->count(key) == 0) {
      keys_to_remove->emplace_back(key);
    }
  }
  DeleteImages(std::move(keys_to_remove));
}

}  // namespace ntp_snippets
