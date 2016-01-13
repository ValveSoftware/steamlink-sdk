// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_FILEAPI_FILE_SYSTEM_OPTIONS_H_
#define WEBKIT_BROWSER_FILEAPI_FILE_SYSTEM_OPTIONS_H_

#include <string>
#include <vector>

#include "webkit/browser/webkit_storage_browser_export.h"

namespace leveldb {
class Env;
}

namespace fileapi {

// Provides runtime options that may change FileSystem API behavior.
// This object is copyable.
class WEBKIT_STORAGE_BROWSER_EXPORT FileSystemOptions {
 public:
  enum ProfileMode {
    PROFILE_MODE_NORMAL = 0,
    PROFILE_MODE_INCOGNITO
  };

  // |profile_mode| specifies if the profile (for this filesystem)
  // is running in incognito mode (PROFILE_MODE_INCOGNITO) or no
  // (PROFILE_MODE_NORMAL).
  // |additional_allowed_schemes| specifies schemes that are allowed
  // to access FileSystem API in addition to "http" and "https".
  // Non-NULL |env_override| overrides internal LevelDB environment.
  FileSystemOptions(
      ProfileMode profile_mode,
      const std::vector<std::string>& additional_allowed_schemes,
      leveldb::Env* env_override);

  ~FileSystemOptions();

  // Returns true if it is running in the incognito mode.
  bool is_incognito() const { return profile_mode_ == PROFILE_MODE_INCOGNITO; }

  // Returns the schemes that must be allowed to access FileSystem API
  // in addition to standard "http" and "https".
  // (e.g. If the --allow-file-access-from-files option is given in chrome
  // "file" scheme will also need to be allowed).
  const std::vector<std::string>& additional_allowed_schemes() const {
    return additional_allowed_schemes_;
  }

  leveldb::Env* env_override() const { return env_override_; }

 private:
  const ProfileMode profile_mode_;
  const std::vector<std::string> additional_allowed_schemes_;
  leveldb::Env* env_override_;
};

}  // namespace fileapi

#endif  // WEBKIT_BROWSER_FILEAPI_FILE_SYSTEM_OPTIONS_H_
