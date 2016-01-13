// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/test_suite.h"
#include "env_chromium_stdio.h"
#if defined(OS_WIN)
#include "env_chromium_win.h"
#endif
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/env_idb.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"

using namespace leveldb_env;
using namespace leveldb;

#define FPL FILE_PATH_LITERAL

TEST(ErrorEncoding, OnlyAMethod) {
  const MethodID in_method = kSequentialFileRead;
  const Status s = MakeIOError("Somefile.txt", "message", in_method);
  MethodID method;
  int error = -75;
  EXPECT_EQ(METHOD_ONLY,
            ParseMethodAndError(s.ToString().c_str(), &method, &error));
  EXPECT_EQ(in_method, method);
  EXPECT_EQ(-75, error);
}

TEST(ErrorEncoding, FileError) {
  const MethodID in_method = kWritableFileClose;
  const base::File::Error fe = base::File::FILE_ERROR_INVALID_OPERATION;
  const Status s = MakeIOError("Somefile.txt", "message", in_method, fe);
  MethodID method;
  int error;
  EXPECT_EQ(METHOD_AND_PFE,
            ParseMethodAndError(s.ToString().c_str(), &method, &error));
  EXPECT_EQ(in_method, method);
  EXPECT_EQ(fe, error);
}

TEST(ErrorEncoding, Errno) {
  const MethodID in_method = kWritableFileFlush;
  const int some_errno = ENOENT;
  const Status s =
      MakeIOError("Somefile.txt", "message", in_method, some_errno);
  MethodID method;
  int error;
  EXPECT_EQ(METHOD_AND_ERRNO,
            ParseMethodAndError(s.ToString().c_str(), &method, &error));
  EXPECT_EQ(in_method, method);
  EXPECT_EQ(some_errno, error);
}

#if defined(OS_WIN)
TEST(ErrorEncoding, ErrnoWin32) {
  const MethodID in_method = kWritableFileFlush;
  const DWORD some_errno = ERROR_FILE_NOT_FOUND;
  const Status s =
      MakeIOErrorWin("Somefile.txt", "message", in_method, some_errno);
  MethodID method;
  int error;
  EXPECT_EQ(METHOD_AND_ERRNO,
            ParseMethodAndError(s.ToString().c_str(), &method, &error));
  EXPECT_EQ(in_method, method);
  EXPECT_EQ(some_errno, error);
}
#endif

TEST(ErrorEncoding, NoEncodedMessage) {
  Status s = Status::IOError("Some message", "from leveldb itself");
  MethodID method = kRandomAccessFileRead;
  int error = 4;
  EXPECT_EQ(NONE, ParseMethodAndError(s.ToString().c_str(), &method, &error));
  EXPECT_EQ(kRandomAccessFileRead, method);
  EXPECT_EQ(4, error);
}

template <typename T>
class MyEnv : public T {
 public:
  MyEnv() : directory_syncs_(0) {}
  int directory_syncs() { return directory_syncs_; }

 protected:
  virtual void DidSyncDir(const std::string& fname) {
    ++directory_syncs_;
    ChromiumEnv::DidSyncDir(fname);
  }

 private:
  int directory_syncs_;
};

template <typename T>
class ChromiumEnvMultiPlatformTests : public ::testing::Test {
 public:
};

#if defined(OS_WIN)
typedef ::testing::Types<ChromiumEnvStdio, ChromiumEnvWin> ChromiumEnvMultiPlatformTestsTypes;
#else
typedef ::testing::Types<ChromiumEnvStdio> ChromiumEnvMultiPlatformTestsTypes;
#endif
TYPED_TEST_CASE(ChromiumEnvMultiPlatformTests, ChromiumEnvMultiPlatformTestsTypes);

TYPED_TEST(ChromiumEnvMultiPlatformTests, DirectorySyncing) {
  MyEnv<TypeParam> env;

  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  base::FilePath dir_path = dir.path();
  std::string some_data = "some data";
  Slice data = some_data;

  std::string manifest_file_name =
      FilePathToString(dir_path.Append(FILE_PATH_LITERAL("MANIFEST-001")));
  WritableFile* manifest_file_ptr;
  Status s = env.NewWritableFile(manifest_file_name, &manifest_file_ptr);
  EXPECT_TRUE(s.ok());
  scoped_ptr<WritableFile> manifest_file(manifest_file_ptr);
  manifest_file->Append(data);
  EXPECT_EQ(0, env.directory_syncs());
  manifest_file->Append(data);
  EXPECT_EQ(0, env.directory_syncs());

  std::string sst_file_name =
      FilePathToString(dir_path.Append(FILE_PATH_LITERAL("000003.sst")));
  WritableFile* sst_file_ptr;
  s = env.NewWritableFile(sst_file_name, &sst_file_ptr);
  EXPECT_TRUE(s.ok());
  scoped_ptr<WritableFile> sst_file(sst_file_ptr);
  sst_file->Append(data);
  EXPECT_EQ(0, env.directory_syncs());

  manifest_file->Append(data);
  EXPECT_EQ(1, env.directory_syncs());
  manifest_file->Append(data);
  EXPECT_EQ(1, env.directory_syncs());
}

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

TEST(ChromiumEnv, BackupTables) {
  Options options;
  options.create_if_missing = true;
  options.env = IDBEnv();

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

  Env* env = IDBEnv();
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

  Env* env = IDBEnv();
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
