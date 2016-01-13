// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/file_system_util.h"

#include "base/logging.h"

namespace ppapi {

fileapi::FileSystemType PepperFileSystemTypeToFileSystemType(
    PP_FileSystemType type) {
  switch (type) {
    case PP_FILESYSTEMTYPE_LOCALTEMPORARY:
      return fileapi::kFileSystemTypeTemporary;
    case PP_FILESYSTEMTYPE_LOCALPERSISTENT:
      return fileapi::kFileSystemTypePersistent;
    case PP_FILESYSTEMTYPE_EXTERNAL:
      return fileapi::kFileSystemTypeExternal;
    default:
      return fileapi::kFileSystemTypeUnknown;
  }
}

bool FileSystemTypeIsValid(PP_FileSystemType type) {
  return (type == PP_FILESYSTEMTYPE_LOCALPERSISTENT ||
          type == PP_FILESYSTEMTYPE_LOCALTEMPORARY ||
          type == PP_FILESYSTEMTYPE_EXTERNAL ||
          type == PP_FILESYSTEMTYPE_ISOLATED);
}

bool FileSystemTypeHasQuota(PP_FileSystemType type) {
  return (type == PP_FILESYSTEMTYPE_LOCALTEMPORARY ||
          type == PP_FILESYSTEMTYPE_LOCALPERSISTENT);
}

std::string IsolatedFileSystemTypeToRootName(
    PP_IsolatedFileSystemType_Private type) {
  switch (type) {
    case PP_ISOLATEDFILESYSTEMTYPE_PRIVATE_CRX:
      return "crxfs";
    case PP_ISOLATEDFILESYSTEMTYPE_PRIVATE_PLUGINPRIVATE:
      return "pluginprivate";
    default:
      NOTREACHED() << type;
      return std::string();
  }
}

}  // namespace ppapi
