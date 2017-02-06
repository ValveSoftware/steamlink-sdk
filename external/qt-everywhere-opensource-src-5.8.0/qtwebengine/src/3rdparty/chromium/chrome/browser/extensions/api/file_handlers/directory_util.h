// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_FILE_HANDLERS_DIRECTORY_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_API_FILE_HANDLERS_DIRECTORY_UTIL_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"

class Profile;

namespace base {
class FilePath;
}  // namespace base

namespace storage {
class FileSystemURL;
}  // namespace storage

namespace extensions {
namespace app_file_handler_util {

class IsDirectoryCollector {
 public:
  typedef base::Callback<void(std::unique_ptr<std::set<base::FilePath>>)>
      CompletionCallback;

  explicit IsDirectoryCollector(Profile* profile);
  virtual ~IsDirectoryCollector();

  // For the given paths obtains a set with which of them are directories.
  // The collector does not support virtual files if OS != CHROMEOS.
  void CollectForEntriesPaths(const std::vector<base::FilePath>& paths,
                              const CompletionCallback& callback);

 private:
  void OnIsDirectoryCollected(size_t index, bool directory);

  Profile* profile_;
  std::vector<base::FilePath> paths_;
  std::unique_ptr<std::set<base::FilePath>> result_;
  size_t left_;
  CompletionCallback callback_;
  base::WeakPtrFactory<IsDirectoryCollector> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(IsDirectoryCollector);
};

}  // namespace app_file_handler_util
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_FILE_HANDLERS_DIRECTORY_UTIL_H_
