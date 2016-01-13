// Copyright (c) 2013 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef THIRD_PARTY_LEVELDATABASE_ENV_CHROMIUM_WIN_H_
#define THIRD_PARTY_LEVELDATABASE_ENV_CHROMIUM_WIN_H_

#include "env_chromium.h"

namespace leveldb_env {

leveldb::Status MakeIOErrorWin(leveldb::Slice filename,
                                 const std::string& message,
                                 MethodID method,
                                 DWORD err);

class ChromiumWritableFileWin : public leveldb::WritableFile {
 public:
  ChromiumWritableFileWin(const std::string& fname,
                            HANDLE f,
                            const UMALogger* uma_logger,
                            WriteTracker* tracker,
                            bool make_backup);
  virtual ~ChromiumWritableFileWin();
  virtual leveldb::Status Append(const leveldb::Slice& data);
  virtual leveldb::Status Close();
  virtual leveldb::Status Flush();
  virtual leveldb::Status Sync();

 private:
  enum Type {
    kManifest,
    kTable,
    kOther
  };
  leveldb::Status SyncParent();

  std::string filename_;
  HANDLE file_;
  const UMALogger* uma_logger_;
  WriteTracker* tracker_;
  Type file_type_;
  std::string parent_dir_;
  bool make_backup_;
};

class ChromiumEnvWin : public ChromiumEnv {
 public:
  ChromiumEnvWin();
  virtual ~ChromiumEnvWin();

  virtual leveldb::Status NewSequentialFile(const std::string& fname,
                                            leveldb::SequentialFile** result);
  virtual leveldb::Status NewRandomAccessFile(
      const std::string& fname,
      leveldb::RandomAccessFile** result);
  virtual leveldb::Status NewWritableFile(const std::string& fname,
                                          leveldb::WritableFile** result);
  virtual leveldb::Status NewLogger(const std::string& fname,
                                    leveldb::Logger** result);

 protected:
  virtual base::File::Error GetDirectoryEntries(
      const base::FilePath& dir_param,
      std::vector<base::FilePath>* result) const;

 private:
  // BGThread() is the body of the background thread
  void BGThread();
  static void BGThreadWrapper(void* arg) {
    reinterpret_cast<ChromiumEnvWin*>(arg)->BGThread();
  }
  void RecordOpenFilesLimit(const std::string& type);
  virtual void RecordOSError(MethodID method, DWORD err) const;
};

}  // namespace leveldb_env

#endif
