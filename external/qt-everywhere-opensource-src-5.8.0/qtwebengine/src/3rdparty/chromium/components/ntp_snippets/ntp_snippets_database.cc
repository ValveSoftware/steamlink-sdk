// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/ntp_snippets_database.h"

#include <utility>

#include "base/files/file_path.h"
#include "components/leveldb_proto/proto_database_impl.h"
#include "components/ntp_snippets/proto/ntp_snippets.pb.h"

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
}

namespace ntp_snippets {

NTPSnippetsDatabase::NTPSnippetsDatabase(
    const base::FilePath& database_dir,
    scoped_refptr<base::SequencedTaskRunner> file_task_runner)
    : database_(
          new ProtoDatabaseImpl<SnippetProto>(file_task_runner)),
      database_initialized_(false),
      image_database_(
          new ProtoDatabaseImpl<SnippetImageProto>(file_task_runner)),
      image_database_initialized_(false),
      weak_ptr_factory_(this) {
  base::FilePath snippet_dir = database_dir.AppendASCII(kSnippetDatabaseFolder);
  database_->Init(kDatabaseUMAClientName, snippet_dir,
                  base::Bind(&NTPSnippetsDatabase::OnDatabaseInited,
                             weak_ptr_factory_.GetWeakPtr()));

  base::FilePath image_dir = database_dir.AppendASCII(kImageDatabaseFolder);
  image_database_->Init(kImageDatabaseUMAClientName, image_dir,
                        base::Bind(&NTPSnippetsDatabase::OnImageDatabaseInited,
                                   weak_ptr_factory_.GetWeakPtr()));
}

NTPSnippetsDatabase::~NTPSnippetsDatabase() {}

bool NTPSnippetsDatabase::IsInitialized() const {
  return !IsErrorState() && database_initialized_ &&
      image_database_initialized_;
}

bool NTPSnippetsDatabase::IsErrorState() const {
  return !database_ || !image_database_;
}

void NTPSnippetsDatabase::SetErrorCallback(
    const base::Closure& error_callback) {
  error_callback_ = error_callback;
}

void NTPSnippetsDatabase::LoadSnippets(const SnippetsCallback& callback) {
  if (IsInitialized())
    LoadSnippetsImpl(callback);
  else
    pending_snippets_callbacks_.emplace_back(callback);
}

void NTPSnippetsDatabase::SaveSnippet(const NTPSnippet& snippet) {
  std::unique_ptr<KeyEntryVector> entries_to_save(new KeyEntryVector());
  entries_to_save->emplace_back(snippet.id(), snippet.ToProto());
  SaveSnippetsImpl(std::move(entries_to_save));
}

void NTPSnippetsDatabase::SaveSnippets(const NTPSnippet::PtrVector& snippets) {
  std::unique_ptr<KeyEntryVector> entries_to_save(new KeyEntryVector());
  for (const std::unique_ptr<NTPSnippet>& snippet : snippets)
    entries_to_save->emplace_back(snippet->id(), snippet->ToProto());
  SaveSnippetsImpl(std::move(entries_to_save));
}

void NTPSnippetsDatabase::DeleteSnippet(const std::string& snippet_id) {
  DeleteSnippetsImpl(
      base::WrapUnique(new std::vector<std::string>(1, snippet_id)));
}

void NTPSnippetsDatabase::DeleteSnippets(
    const NTPSnippet::PtrVector& snippets) {
  std::unique_ptr<std::vector<std::string>> keys_to_remove(
      new std::vector<std::string>());
  for (const std::unique_ptr<NTPSnippet>& snippet : snippets)
    keys_to_remove->emplace_back(snippet->id());
  DeleteSnippetsImpl(std::move(keys_to_remove));
}

void NTPSnippetsDatabase::LoadImage(const std::string& snippet_id,
                                    const SnippetImageCallback& callback) {
  if (IsInitialized())
    LoadImageImpl(snippet_id, callback);
  else
    pending_image_callbacks_.emplace_back(snippet_id, callback);
}

void NTPSnippetsDatabase::SaveImage(const std::string& snippet_id,
                                    const std::string& image_data) {
  DCHECK(IsInitialized());

  SnippetImageProto image_proto;
  image_proto.set_data(image_data);

  std::unique_ptr<ImageKeyEntryVector> entries_to_save(
      new ImageKeyEntryVector());
  entries_to_save->emplace_back(snippet_id, std::move(image_proto));

  image_database_->UpdateEntries(
      std::move(entries_to_save),
      base::WrapUnique(new std::vector<std::string>()),
      base::Bind(&NTPSnippetsDatabase::OnImageDatabaseSaved,
                 weak_ptr_factory_.GetWeakPtr()));
}

void NTPSnippetsDatabase::DeleteImage(const std::string& snippet_id) {
  DeleteImagesImpl(
      base::WrapUnique(new std::vector<std::string>(1, snippet_id)));
}

void NTPSnippetsDatabase::OnDatabaseInited(bool success) {
  DCHECK(!database_initialized_);
  if (!success) {
    DVLOG(1) << "NTPSnippetsDatabase init failed.";
    OnDatabaseError();
    return;
  }
  database_initialized_ = true;
  if (IsInitialized())
    ProcessPendingLoads();
}

void NTPSnippetsDatabase::OnDatabaseLoaded(
    const SnippetsCallback& callback,
    bool success,
    std::unique_ptr<std::vector<SnippetProto>> entries) {
  if (!success) {
    DVLOG(1) << "NTPSnippetsDatabase load failed.";
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
      LOG(WARNING) << "Invalid proto from DB " << proto.id();
      keys_to_remove->emplace_back(proto.id());
    }
  }

  callback.Run(std::move(snippets));

  // If any of the snippet protos couldn't be converted to actual snippets,
  // clean them up now.
  if (!keys_to_remove->empty())
    DeleteSnippetsImpl(std::move(keys_to_remove));
}

void NTPSnippetsDatabase::OnDatabaseSaved(bool success) {
  if (!success) {
    DVLOG(1) << "NTPSnippetsDatabase save failed.";
    OnDatabaseError();
  }
}

void NTPSnippetsDatabase::OnImageDatabaseInited(bool success) {
  DCHECK(!image_database_initialized_);
  if (!success) {
    DVLOG(1) << "NTPSnippetsDatabase init failed.";
    OnDatabaseError();
    return;
  }
  image_database_initialized_ = true;
  if (IsInitialized())
    ProcessPendingLoads();
}

void NTPSnippetsDatabase::OnImageDatabaseLoaded(
    const SnippetImageCallback& callback,
    bool success,
    std::unique_ptr<SnippetImageProto> entry) {
  if (!success) {
    DVLOG(1) << "NTPSnippetsDatabase load failed.";
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

void NTPSnippetsDatabase::OnImageDatabaseSaved(bool success) {
  if (!success) {
    DVLOG(1) << "NTPSnippetsDatabase save failed.";
    OnDatabaseError();
  }
}

void NTPSnippetsDatabase::OnDatabaseError() {
  database_.reset();
  image_database_.reset();
  if (!error_callback_.is_null())
    error_callback_.Run();
}

void NTPSnippetsDatabase::ProcessPendingLoads() {
  DCHECK(IsInitialized());

  for (const auto& callback : pending_snippets_callbacks_)
    LoadSnippetsImpl(callback);
  pending_snippets_callbacks_.clear();

  for (const auto& id_callback : pending_image_callbacks_)
    LoadImageImpl(id_callback.first, id_callback.second);
  pending_image_callbacks_.clear();
}

void NTPSnippetsDatabase::LoadSnippetsImpl(const SnippetsCallback& callback) {
  DCHECK(IsInitialized());
  database_->LoadEntries(base::Bind(&NTPSnippetsDatabase::OnDatabaseLoaded,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    callback));
}

void NTPSnippetsDatabase::SaveSnippetsImpl(
    std::unique_ptr<KeyEntryVector> entries_to_save) {
  DCHECK(IsInitialized());

  std::unique_ptr<std::vector<std::string>> keys_to_remove(
      new std::vector<std::string>());
  database_->UpdateEntries(std::move(entries_to_save),
                           std::move(keys_to_remove),
                           base::Bind(&NTPSnippetsDatabase::OnDatabaseSaved,
                                      weak_ptr_factory_.GetWeakPtr()));
}

void NTPSnippetsDatabase::DeleteSnippetsImpl(
    std::unique_ptr<std::vector<std::string>> keys_to_remove) {
  DCHECK(IsInitialized());

  DeleteImagesImpl(
      base::WrapUnique(new std::vector<std::string>(*keys_to_remove)));

  std::unique_ptr<KeyEntryVector> entries_to_save(new KeyEntryVector());
  database_->UpdateEntries(std::move(entries_to_save),
                           std::move(keys_to_remove),
                           base::Bind(&NTPSnippetsDatabase::OnDatabaseSaved,
                                      weak_ptr_factory_.GetWeakPtr()));
}

void NTPSnippetsDatabase::LoadImageImpl(const std::string& snippet_id,
                                        const SnippetImageCallback& callback) {
  DCHECK(IsInitialized());
  image_database_->GetEntry(
      snippet_id,
      base::Bind(&NTPSnippetsDatabase::OnImageDatabaseLoaded,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void NTPSnippetsDatabase::DeleteImagesImpl(
    std::unique_ptr<std::vector<std::string>> keys_to_remove) {
  DCHECK(IsInitialized());

  image_database_->UpdateEntries(
      base::WrapUnique(new ImageKeyEntryVector()),
      std::move(keys_to_remove),
      base::Bind(&NTPSnippetsDatabase::OnImageDatabaseSaved,
                 weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace ntp_snippets
