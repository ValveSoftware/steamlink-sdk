// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_ENV_MOJO_H_
#define COMPONENTS_LEVELDB_ENV_MOJO_H_

#include "components/filesystem/public/interfaces/directory.mojom.h"
#include "components/leveldb/leveldb_mojo_proxy.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/include/leveldb/env.h"

namespace leveldb {

// An implementation of the leveldb operating system interaction code which
// proxies to a specified mojo:filesystem directory. Most of these methods are
// synchronous and block on responses from the filesystem service. That's fine
// since, for the most part, they merely open files or check for a file's
// existence.
class MojoEnv : public leveldb_env::ChromiumEnv {
 public:
  MojoEnv(scoped_refptr<LevelDBMojoProxy> file_thread,
          LevelDBMojoProxy::OpaqueDir* dir);
  ~MojoEnv() override;

  // Overridden from leveldb_env::EnvChromium:
  Status NewSequentialFile(const std::string& fname,
                           SequentialFile** result) override;
  Status NewRandomAccessFile(const std::string& fname,
                             RandomAccessFile** result) override;
  Status NewWritableFile(const std::string& fname,
                         WritableFile** result) override;
  Status NewAppendableFile(const std::string& fname,
                           WritableFile** result) override;
  bool FileExists(const std::string& fname) override;
  Status GetChildren(const std::string& dir,
                     std::vector<std::string>* result) override;
  Status DeleteFile(const std::string& fname) override;
  Status CreateDir(const std::string& dirname) override;
  Status DeleteDir(const std::string& dirname) override;
  Status GetFileSize(const std::string& fname, uint64_t* file_size) override;
  Status RenameFile(const std::string& src, const std::string& target) override;
  Status LockFile(const std::string& fname, FileLock** lock) override;
  Status UnlockFile(FileLock* lock) override;
  Status GetTestDirectory(std::string* path) override;
  Status NewLogger(const std::string& fname, Logger** result) override;

  // For reference, we specifically don't override Schedule(), StartThread(),
  // NowMicros() or SleepForMicroseconds() and use the EnvChromium versions.

 private:
  scoped_refptr<LevelDBMojoProxy> thread_;
  LevelDBMojoProxy::OpaqueDir* dir_;

  DISALLOW_COPY_AND_ASSIGN(MojoEnv);
};

}  // namespace leveldb

#endif  // COMPONENTS_LEVELDB_ENV_MOJO_H_
