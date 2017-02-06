// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_PROTO_TESTING_FAKE_DB_H_
#define COMPONENTS_LEVELDB_PROTO_TESTING_FAKE_DB_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "components/leveldb_proto/proto_database.h"

namespace leveldb_proto {
namespace test {

template <typename T>
class FakeDB : public ProtoDatabase<T> {
  using Callback = base::Callback<void(bool)>;

 public:
  using EntryMap = typename base::hash_map<std::string, T>;

  explicit FakeDB(EntryMap* db);
  ~FakeDB() override;

  // ProtoDatabase implementation.
  void Init(const char* client_name,
            const base::FilePath& database_dir,
            const typename ProtoDatabase<T>::InitCallback& callback) override;
  void UpdateEntries(
      std::unique_ptr<typename ProtoDatabase<T>::KeyEntryVector>
          entries_to_save,
      std::unique_ptr<std::vector<std::string>> keys_to_remove,
      const typename ProtoDatabase<T>::UpdateCallback& callback) override;
  void LoadEntries(
      const typename ProtoDatabase<T>::LoadCallback& callback) override;
  void GetEntry(
      const std::string& key,
      const typename ProtoDatabase<T>::GetCallback& callback) override;
  void Destroy(
      const typename ProtoDatabase<T>::DestroyCallback& callback) override;

  base::FilePath& GetDirectory();

  void InitCallback(bool success);

  void LoadCallback(bool success);

  void GetCallback(bool success);

  void UpdateCallback(bool success);

  static base::FilePath DirectoryForTestDB();

 private:
  static void RunLoadCallback(
      const typename ProtoDatabase<T>::LoadCallback& callback,
      std::unique_ptr<typename std::vector<T>> entries,
      bool success);

  static void RunGetCallback(
      const typename ProtoDatabase<T>::GetCallback& callback,
      std::unique_ptr<T> entry,
      bool success);

  base::FilePath dir_;
  EntryMap* db_;

  Callback init_callback_;
  Callback load_callback_;
  Callback get_callback_;
  Callback update_callback_;
};

template <typename T>
FakeDB<T>::FakeDB(EntryMap* db)
    : db_(db) {}

template <typename T>
FakeDB<T>::~FakeDB() {}

template <typename T>
void FakeDB<T>::Init(const char* client_name,
                     const base::FilePath& database_dir,
                     const typename ProtoDatabase<T>::InitCallback& callback) {
  dir_ = database_dir;
  init_callback_ = callback;
}

template <typename T>
void FakeDB<T>::UpdateEntries(
    std::unique_ptr<typename ProtoDatabase<T>::KeyEntryVector> entries_to_save,
    std::unique_ptr<std::vector<std::string>> keys_to_remove,
    const typename ProtoDatabase<T>::UpdateCallback& callback) {
  for (const auto& pair : *entries_to_save)
    (*db_)[pair.first] = pair.second;

  for (const auto& key : *keys_to_remove)
    db_->erase(key);

  update_callback_ = callback;
}

template <typename T>
void FakeDB<T>::LoadEntries(
    const typename ProtoDatabase<T>::LoadCallback& callback) {
  std::unique_ptr<std::vector<T>> entries(new std::vector<T>());
  for (const auto& pair : *db_)
    entries->push_back(pair.second);

  load_callback_ =
      base::Bind(RunLoadCallback, callback, base::Passed(&entries));
}

template <typename T>
void FakeDB<T>::GetEntry(
    const std::string& key,
    const typename ProtoDatabase<T>::GetCallback& callback) {
  std::unique_ptr<T> entry;
  auto it = db_->find(key);
  if (it != db_->end())
    entry.reset(new T(it->second));

  get_callback_ = base::Bind(RunGetCallback, callback, base::Passed(&entry));
}

template <typename T>
void FakeDB<T>::Destroy(
    const typename ProtoDatabase<T>::DestroyCallback& callback) {
}

template <typename T>
base::FilePath& FakeDB<T>::GetDirectory() {
  return dir_;
}

template <typename T>
void FakeDB<T>::InitCallback(bool success) {
  init_callback_.Run(success);
  init_callback_.Reset();
}

template <typename T>
void FakeDB<T>::LoadCallback(bool success) {
  load_callback_.Run(success);
  load_callback_.Reset();
}

template <typename T>
void FakeDB<T>::GetCallback(bool success) {
  get_callback_.Run(success);
  get_callback_.Reset();
}

template <typename T>
void FakeDB<T>::UpdateCallback(bool success) {
  update_callback_.Run(success);
  update_callback_.Reset();
}

// static
template <typename T>
void FakeDB<T>::RunLoadCallback(
    const typename ProtoDatabase<T>::LoadCallback& callback,
    std::unique_ptr<typename std::vector<T>> entries,
    bool success) {
  callback.Run(success, std::move(entries));
}

// static
template <typename T>
void FakeDB<T>::RunGetCallback(
    const typename ProtoDatabase<T>::GetCallback& callback,
    std::unique_ptr<T> entry,
    bool success) {
  callback.Run(success, std::move(entry));
}

// static
template <typename T>
base::FilePath FakeDB<T>::DirectoryForTestDB() {
  return base::FilePath(FILE_PATH_LITERAL("/fake/path"));
}

}  // namespace test
}  // namespace leveldb_proto

#endif  // COMPONENTS_LEVELDB_PROTO_TESTING_FAKE_DB_H_
