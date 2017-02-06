// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/lazy_instance.h"
#include "base/test/test_suite.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"

#define FPL FILE_PATH_LITERAL

using leveldb::DB;
using leveldb::Env;
using leveldb::Options;
using leveldb::ReadOptions;
using leveldb::Slice;
using leveldb::Status;
using leveldb::WritableFile;
using leveldb::WriteOptions;
using leveldb_env::ChromiumEnv;
using leveldb_env::MethodID;

TEST(ErrorEncoding, OnlyAMethod) {
  const MethodID in_method = leveldb_env::kSequentialFileRead;
  const Status s = MakeIOError("Somefile.txt", "message", in_method);
  MethodID method;
  base::File::Error error = base::File::FILE_ERROR_MAX;
  EXPECT_EQ(leveldb_env::METHOD_ONLY, ParseMethodAndError(s, &method, &error));
  EXPECT_EQ(in_method, method);
  EXPECT_EQ(base::File::FILE_ERROR_MAX, error);
}

TEST(ErrorEncoding, FileError) {
  const MethodID in_method = leveldb_env::kWritableFileClose;
  const base::File::Error fe = base::File::FILE_ERROR_INVALID_OPERATION;
  const Status s = MakeIOError("Somefile.txt", "message", in_method, fe);
  MethodID method;
  base::File::Error error;
  EXPECT_EQ(leveldb_env::METHOD_AND_BFE,
            ParseMethodAndError(s, &method, &error));
  EXPECT_EQ(in_method, method);
  EXPECT_EQ(fe, error);
}

TEST(ErrorEncoding, NoEncodedMessage) {
  Status s = Status::IOError("Some message", "from leveldb itself");
  MethodID method = leveldb_env::kRandomAccessFileRead;
  base::File::Error error = base::File::FILE_ERROR_MAX;
  EXPECT_EQ(leveldb_env::NONE, ParseMethodAndError(s, &method, &error));
  EXPECT_EQ(leveldb_env::kRandomAccessFileRead, method);
  EXPECT_EQ(base::File::FILE_ERROR_MAX, error);
}

template <typename T>
class ChromiumEnvMultiPlatformTests : public ::testing::Test {
 public:
};

typedef ::testing::Types<ChromiumEnv> ChromiumEnvMultiPlatformTestsTypes;
TYPED_TEST_CASE(ChromiumEnvMultiPlatformTests,
                ChromiumEnvMultiPlatformTestsTypes);

int CountFilesWithExtension(const base::FilePath& dir,
                            const base::FilePath::StringType& extension) {
  int matching_files = 0;
  base::FileEnumerator dir_reader(
      dir, false, base::FileEnumerator::FILES);
  for (base::FilePath fname = dir_reader.Next(); !fname.empty();
       fname = dir_reader.Next()) {
    if (fname.MatchesExtension(extension))
      matching_files++;
  }
  return matching_files;
}

bool GetFirstLDBFile(const base::FilePath& dir, base::FilePath* ldb_file) {
  base::FileEnumerator dir_reader(
      dir, false, base::FileEnumerator::FILES);
  for (base::FilePath fname = dir_reader.Next(); !fname.empty();
       fname = dir_reader.Next()) {
    if (fname.MatchesExtension(FPL(".ldb"))) {
      *ldb_file = fname;
      return true;
    }
  }
  return false;
}

class BackupEnv : public ChromiumEnv {
 public:
  BackupEnv() : ChromiumEnv("BackupEnv", true /* backup tables */) {}
};

base::LazyInstance<BackupEnv>::Leaky backup_env = LAZY_INSTANCE_INITIALIZER;

TEST(ChromiumEnv, BackupTables) {
  Options options;
  options.create_if_missing = true;
  options.env = backup_env.Pointer();

  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  base::FilePath dir = scoped_temp_dir.path();

  DB* db;
  Status status = DB::Open(options, dir.AsUTF8Unsafe(), &db);
  EXPECT_TRUE(status.ok()) << status.ToString();
  status = db->Put(WriteOptions(), "key", "value");
  EXPECT_TRUE(status.ok()) << status.ToString();
  Slice a = "a";
  Slice z = "z";
  db->CompactRange(&a, &z);
  int ldb_files = CountFilesWithExtension(dir, FPL(".ldb"));
  int bak_files = CountFilesWithExtension(dir, FPL(".bak"));
  EXPECT_GT(ldb_files, 0);
  EXPECT_EQ(ldb_files, bak_files);
  base::FilePath ldb_file;
  EXPECT_TRUE(GetFirstLDBFile(dir, &ldb_file));
  delete db;
  EXPECT_TRUE(base::DeleteFile(ldb_file, false));
  EXPECT_EQ(ldb_files - 1, CountFilesWithExtension(dir, FPL(".ldb")));

  // The ldb file deleted above should be restored in Open.
  status = leveldb::DB::Open(options, dir.AsUTF8Unsafe(), &db);
  EXPECT_TRUE(status.ok()) << status.ToString();
  std::string value;
  status = db->Get(ReadOptions(), "key", &value);
  EXPECT_TRUE(status.ok()) << status.ToString();
  EXPECT_EQ("value", value);
  delete db;

  // Ensure that deleting an ldb file also deletes its backup.
  int orig_ldb_files = CountFilesWithExtension(dir, FPL(".ldb"));
  EXPECT_GT(ldb_files, 0);
  EXPECT_EQ(ldb_files, bak_files);
  EXPECT_TRUE(GetFirstLDBFile(dir, &ldb_file));
  options.env->DeleteFile(ldb_file.AsUTF8Unsafe());
  ldb_files = CountFilesWithExtension(dir, FPL(".ldb"));
  bak_files = CountFilesWithExtension(dir, FPL(".bak"));
  EXPECT_EQ(orig_ldb_files - 1, ldb_files);
  EXPECT_EQ(bak_files, ldb_files);
}

TEST(ChromiumEnv, GetChildrenEmptyDir) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  base::FilePath dir = scoped_temp_dir.path();

  Env* env = Env::Default();
  std::vector<std::string> result;
  leveldb::Status status = env->GetChildren(dir.AsUTF8Unsafe(), &result);
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(0U, result.size());
}

TEST(ChromiumEnv, GetChildrenPriorResults) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  base::FilePath dir = scoped_temp_dir.path();

  base::FilePath new_file_dir = dir.Append(FPL("tmp_file"));
  FILE* f = fopen(new_file_dir.AsUTF8Unsafe().c_str(), "w");
  if (f) {
    fputs("Temp file contents", f);
    fclose(f);
  }

  Env* env = Env::Default();
  std::vector<std::string> result;
  leveldb::Status status = env->GetChildren(dir.AsUTF8Unsafe(), &result);
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(1U, result.size());

  // And a second time should also return one result
  status = env->GetChildren(dir.AsUTF8Unsafe(), &result);
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(1U, result.size());
}

int main(int argc, char** argv) { return base::TestSuite(argc, argv).Run(); }
