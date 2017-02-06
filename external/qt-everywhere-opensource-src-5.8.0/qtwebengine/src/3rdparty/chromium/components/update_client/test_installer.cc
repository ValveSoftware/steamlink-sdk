// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/test_installer.h"

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/values.h"

namespace update_client {

TestInstaller::TestInstaller() : error_(0), install_count_(0) {
}

void TestInstaller::OnUpdateError(int error) {
  error_ = error;
}

bool TestInstaller::Install(const base::DictionaryValue& manifest,
                            const base::FilePath& unpack_path) {
  ++install_count_;
  return base::DeleteFile(unpack_path, true);
}

bool TestInstaller::GetInstalledFile(const std::string& file,
                                     base::FilePath* installed_file) {
  return false;
}

TestInstaller::~TestInstaller() {
}

bool TestInstaller::Uninstall() {
  return false;
}

ReadOnlyTestInstaller::ReadOnlyTestInstaller(const base::FilePath& install_dir)
    : install_directory_(install_dir) {
}

ReadOnlyTestInstaller::~ReadOnlyTestInstaller() {
}

bool ReadOnlyTestInstaller::GetInstalledFile(const std::string& file,
                                             base::FilePath* installed_file) {
  *installed_file = install_directory_.AppendASCII(file);
  return true;
}

VersionedTestInstaller::VersionedTestInstaller() {
  base::CreateNewTempDirectory(FILE_PATH_LITERAL("TEST_"), &install_directory_);
}

VersionedTestInstaller::~VersionedTestInstaller() {
  base::DeleteFile(install_directory_, true);
}

bool VersionedTestInstaller::Install(const base::DictionaryValue& manifest,
                                     const base::FilePath& unpack_path) {
  std::string version_string;
  manifest.GetStringASCII("version", &version_string);
  Version version(version_string.c_str());

  base::FilePath path;
  path = install_directory_.AppendASCII(version.GetString());
  base::CreateDirectory(path.DirName());
  if (!base::Move(unpack_path, path))
    return false;
  current_version_ = version;
  ++install_count_;
  return true;
}

bool VersionedTestInstaller::GetInstalledFile(const std::string& file,
                                              base::FilePath* installed_file) {
  base::FilePath path;
  path = install_directory_.AppendASCII(current_version_.GetString());
  *installed_file = path.Append(base::FilePath::FromUTF8Unsafe(file));
  return true;
}

}  // namespace update_client
