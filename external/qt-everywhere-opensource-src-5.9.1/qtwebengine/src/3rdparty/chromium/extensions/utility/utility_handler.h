// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_UTILITY_UTILITY_HANDLER_
#define EXTENSIONS_UTILITY_UTILITY_HANDLER_

#include <string>

#include "base/callback.h"
#include "base/macros.h"

namespace base {
class FilePath;
}

namespace IPC {
class Message;
}

namespace extensions {

// A handler for extensions-related IPC from within utility processes.
class UtilityHandler {
 public:
  UtilityHandler();
  ~UtilityHandler();

  static void UtilityThreadStarted();

  bool OnMessageReceived(const IPC::Message& message);

 private:
  // IPC message handlers.
  void OnParseUpdateManifest(const std::string& xml);
  void OnUnzipToDir(const base::FilePath& zip_path, const base::FilePath& dir);
  void OnUnpackExtension(const base::FilePath& directory_path,
                         const std::string& extension_id,
                         int location,
                         int creation_flags);

  DISALLOW_COPY_AND_ASSIGN(UtilityHandler);
};

}  // namespace extensions

#endif  // EXTENSIONS_UTILITY_UTILITY_HANDLER_
