// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_DB_V4_STORE_H_
#define COMPONENTS_SAFE_BROWSING_DB_V4_STORE_H_

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "components/safe_browsing_db/v4_protocol_manager_util.h"

namespace safe_browsing {

class V4Store;

typedef base::Callback<void(std::unique_ptr<V4Store>)>
    UpdatedStoreReadyCallback;

// Enumerate different failure events while parsing the file read from disk for
// histogramming purposes.  DO NOT CHANGE THE ORDERING OF THESE VALUES.
enum StoreReadResult {
  // No errors.
  READ_SUCCESS = 0,

  // Reserved for errors in parsing this enum.
  UNEXPECTED_READ_FAILURE = 1,

  // The contents of the file could not be read.
  FILE_UNREADABLE_FAILURE = 2,

  // The file was found to be empty.
  FILE_EMPTY_FAILURE = 3,

  // The contents of the file could not be interpreted as a valid
  // V4StoreFileFormat proto.
  PROTO_PARSING_FAILURE = 4,

  // The magic number didn't match. We're most likely trying to read a file
  // that doesn't contain hash-prefixes.
  UNEXPECTED_MAGIC_NUMBER_FAILURE = 5,

  // The version of the file is different from expected and Chromium doesn't
  // know how to interpret this version of the file.
  FILE_VERSION_INCOMPATIBLE_FAILURE = 6,

  // The rest of the file could not be parsed as a ListUpdateResponse protobuf.
  // This can happen if the machine crashed before the file was fully written to
  // disk or if there was disk corruption.
  HASH_PREFIX_INFO_MISSING_FAILURE = 7,

  // Memory space for histograms is determined by the max.  ALWAYS
  // ADD NEW VALUES BEFORE THIS ONE.
  STORE_READ_RESULT_MAX
};

// Enumerate different failure events while writing the file to disk after
// applying updates for histogramming purposes.
// DO NOT CHANGE THE ORDERING OF THESE VALUES.
enum StoreWriteResult {
  // No errors.
  WRITE_SUCCESS = 0,

  // Reserved for errors in parsing this enum.
  UNEXPECTED_WRITE_FAILURE = 1,

  // The proto being written to disk wasn't a FULL_UPDATE proto.
  INVALID_RESPONSE_TYPE_FAILURE = 2,

  // Number of bytes written to disk was different from the size of the proto.
  UNEXPECTED_BYTES_WRITTEN_FAILURE = 3,

  // Renaming the temporary file to store file failed.
  UNABLE_TO_RENAME_FAILURE = 4,

  // Memory space for histograms is determined by the max.  ALWAYS
  // ADD NEW VALUES BEFORE THIS ONE.
  STORE_WRITE_RESULT_MAX
};

// Factory for creating V4Store. Tests implement this factory to create fake
// stores for testing.
class V4StoreFactory {
 public:
  virtual ~V4StoreFactory() {}
  virtual V4Store* CreateV4Store(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      const base::FilePath& store_path);
};

class V4Store {
 public:
  // The |task_runner| is used to ensure that the operations in this file are
  // performed on the correct thread. |store_path| specifies the location on
  // disk for this file. The constructor doesn't read the store file from disk.
  V4Store(const scoped_refptr<base::SequencedTaskRunner>& task_runner,
          const base::FilePath& store_path);
  virtual ~V4Store();

  const std::string& state() const { return state_; }

  const base::FilePath& store_path() const { return store_path_; }

  void ApplyUpdate(std::unique_ptr<ListUpdateResponse> response,
                   const scoped_refptr<base::SingleThreadTaskRunner>&,
                   UpdatedStoreReadyCallback);

  std::string DebugString() const;

  // Reads the store file from disk and populates the in-memory representation
  // of the hash prefixes.
  void Initialize();

  // Reset internal state and delete the backing file.
  virtual bool Reset();

 private:
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestReadFromEmptyFile);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestReadFromAbsentFile);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestReadFromInvalidContentsFile);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestReadFromUnexpectedMagicFile);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestReadFromLowVersionFile);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestReadFromNoHashPrefixInfoFile);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestReadFromNoHashPrefixesFile);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestWriteNoResponseType);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestWritePartialResponseType);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestWriteFullResponseType);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestReadFromFileWithUnknownProto);

  // Reads the state of the store from the file on disk and returns the reason
  // for the failure or reports success.
  StoreReadResult ReadFromDisk();

  // Writes the FULL_UPDATE |response| to disk as a V4StoreFileFormat proto.
  StoreWriteResult WriteToDisk(
      std::unique_ptr<ListUpdateResponse> response) const;

  // The state of the store as returned by the PVer4 server in the last applied
  // update response.
  std::string state_;
  const base::FilePath store_path_;
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

std::ostream& operator<<(std::ostream& os, const V4Store& store);

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_DB_V4_STORE_H_
