// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/strings/stringprintf.h"
#include "components/safe_browsing_db/v4_store.h"
#include "components/safe_browsing_db/v4_store.pb.h"

namespace safe_browsing {

namespace {
const uint32_t kFileMagic = 0x600D71FE;

const uint32_t kFileVersion = 9;

void RecordStoreReadResult(StoreReadResult result) {
  UMA_HISTOGRAM_ENUMERATION("SafeBrowsing.V4StoreReadResult", result,
                            STORE_READ_RESULT_MAX);
}

void RecordStoreWriteResult(StoreWriteResult result) {
  UMA_HISTOGRAM_ENUMERATION("SafeBrowsing.V4StoreWriteResult", result,
                            STORE_WRITE_RESULT_MAX);
}

// Returns the name of the temporary file used to buffer data for
// |filename|.  Exported for unit tests.
const base::FilePath TemporaryFileForFilename(const base::FilePath& filename) {
  return base::FilePath(filename.value() + FILE_PATH_LITERAL("_new"));
}

}  // namespace

std::ostream& operator<<(std::ostream& os, const V4Store& store) {
  os << store.DebugString();
  return os;
}

V4Store* V4StoreFactory::CreateV4Store(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const base::FilePath& store_path) {
  V4Store* new_store = new V4Store(task_runner, store_path);
  new_store->Initialize();
  return new_store;
}

void V4Store::Initialize() {
  // If a state already exists, don't re-initilize.
  DCHECK(state_.empty());

  StoreReadResult store_read_result = ReadFromDisk();
  RecordStoreReadResult(store_read_result);
}

V4Store::V4Store(const scoped_refptr<base::SequencedTaskRunner>& task_runner,
                 const base::FilePath& store_path)
    : store_path_(store_path), task_runner_(task_runner) {}

V4Store::~V4Store() {}

std::string V4Store::DebugString() const {
  std::string state_base64;
  base::Base64Encode(state_, &state_base64);

  return base::StringPrintf("path: %s; state: %s", store_path_.value().c_str(),
                            state_base64.c_str());
}

bool V4Store::Reset() {
  // TODO(vakh): Implement skeleton.
  state_ = "";
  return true;
}

void V4Store::ApplyUpdate(
    std::unique_ptr<ListUpdateResponse> response,
    const scoped_refptr<base::SingleThreadTaskRunner>& callback_task_runner,
    UpdatedStoreReadyCallback callback) {
  std::unique_ptr<V4Store> new_store(
      new V4Store(this->task_runner_, this->store_path_));

  new_store->state_ = response->new_client_state();
  // TODO(vakh):
  // 1. Merge the old store and the new update in new_store.
  // 2. Create a ListUpdateResponse containing RICE encoded hash-prefixes and
  // response_type as FULL_UPDATE, and write that to disk.
  // 3. Remove this if condition after completing 1. and 2.
  if (response->has_response_type() &&
      response->response_type() == ListUpdateResponse::FULL_UPDATE) {
    StoreWriteResult result = new_store->WriteToDisk(std::move(response));
    RecordStoreWriteResult(result);
  }

  // new_store is done updating, pass it to the callback.
  callback_task_runner->PostTask(
      FROM_HERE, base::Bind(callback, base::Passed(&new_store)));
}

StoreReadResult V4Store::ReadFromDisk() {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  std::string contents;
  bool read_success = base::ReadFileToString(store_path_, &contents);
  if (!read_success) {
    return FILE_UNREADABLE_FAILURE;
  }

  if (contents.empty()) {
    return FILE_EMPTY_FAILURE;
  }

  V4StoreFileFormat file_format;
  if (!file_format.ParseFromString(contents)) {
    return PROTO_PARSING_FAILURE;
  }

  if (file_format.magic_number() != kFileMagic) {
    DVLOG(1) << "Unexpected magic number found in file: "
             << file_format.magic_number();
    return UNEXPECTED_MAGIC_NUMBER_FAILURE;
  }

  UMA_HISTOGRAM_SPARSE_SLOWLY("SafeBrowsing.V4StoreVersionRead",
                              file_format.version_number());
  if (file_format.version_number() != kFileVersion) {
    DVLOG(1) << "File version incompatible: " << file_format.version_number()
             << "; expected: " << kFileVersion;
    return FILE_VERSION_INCOMPATIBLE_FAILURE;
  }

  if (!file_format.has_list_update_response()) {
    return HASH_PREFIX_INFO_MISSING_FAILURE;
  }

  ListUpdateResponse list_update_response = file_format.list_update_response();
  state_ = list_update_response.new_client_state();
  // TODO(vakh): Do more with what's read from the disk.

  return READ_SUCCESS;
}

StoreWriteResult V4Store::WriteToDisk(
    std::unique_ptr<ListUpdateResponse> response) const {
  // Do not write partial updates to the disk.
  // After merging the updates, the ListUpdateResponse passed to this method
  // should be a FULL_UPDATE.
  if (!response->has_response_type() ||
      response->response_type() != ListUpdateResponse::FULL_UPDATE) {
    DVLOG(1) << "response->has_response_type(): "
             << response->has_response_type();
    DVLOG(1) << "response->response_type(): " << response->response_type();
    return INVALID_RESPONSE_TYPE_FAILURE;
  }

  // Attempt writing to a temporary file first and at the end, swap the files.
  const base::FilePath new_filename = TemporaryFileForFilename(store_path_);

  V4StoreFileFormat file_format;
  file_format.set_magic_number(kFileMagic);
  file_format.set_version_number(kFileVersion);
  ListUpdateResponse* response_to_write =
      file_format.mutable_list_update_response();
  response_to_write->Swap(response.get());
  std::string file_format_string;
  file_format.SerializeToString(&file_format_string);
  size_t written = base::WriteFile(new_filename, file_format_string.data(),
                                   file_format_string.size());
  DCHECK_EQ(file_format_string.size(), written);

  if (!base::Move(new_filename, store_path_)) {
    DVLOG(1) << "store_path_: " << store_path_.value();
    return UNABLE_TO_RENAME_FAILURE;
  }

  return WRITE_SUCCESS;
}

}  // namespace safe_browsing
