// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, so no include guard.

#include <string>
#include <tuple>

#include "extensions/common/update_manifest.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/ipc/skia/gfx_skia_param_traits.h"
#include "url/ipc/url_param_traits.h"

#define IPC_MESSAGE_START ExtensionUtilityMsgStart

#ifndef EXTENSIONS_COMMON_EXTENSION_UTILITY_MESSAGES_H_
#define EXTENSIONS_COMMON_EXTENSION_UTILITY_MESSAGES_H_

using DecodedImages = std::vector<std::tuple<SkBitmap, base::FilePath>>;

#endif  //  EXTENSIONS_COMMON_EXTENSION_UTILITY_MESSAGES_H_

IPC_STRUCT_TRAITS_BEGIN(UpdateManifest::Result)
  IPC_STRUCT_TRAITS_MEMBER(extension_id)
  IPC_STRUCT_TRAITS_MEMBER(version)
  IPC_STRUCT_TRAITS_MEMBER(browser_min_version)
  IPC_STRUCT_TRAITS_MEMBER(package_hash)
  IPC_STRUCT_TRAITS_MEMBER(crx_url)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(UpdateManifest::Results)
  IPC_STRUCT_TRAITS_MEMBER(list)
  IPC_STRUCT_TRAITS_MEMBER(daystart_elapsed_seconds)
IPC_STRUCT_TRAITS_END()

//------------------------------------------------------------------------------
// Utility process messages:
// These are messages from the browser to the utility process.

// Tell the utility process to parse the given xml document.
IPC_MESSAGE_CONTROL1(ExtensionUtilityMsg_ParseUpdateManifest,
                     std::string /* xml document contents */)

// Tell the utility process to unzip the zipfile at a given path into a
// directory at the second given path.
IPC_MESSAGE_CONTROL2(ExtensionUtilityMsg_UnzipToDir,
                     base::FilePath /* zip_file */,
                     base::FilePath /* dir */)

// Tells the utility process to validate and sanitize the extension in a
// directory.
IPC_MESSAGE_CONTROL4(ExtensionUtilityMsg_UnpackExtension,
                     base::FilePath /* directory_path */,
                     std::string /* extension_id */,
                     int /* Manifest::Location */,
                     int /* InitFromValue flags */)

//------------------------------------------------------------------------------
// Utility process host messages:
// These are messages from the utility process to the browser.

// Reply when the utility process has succeeded in parsing an update manifest
// xml document.
IPC_MESSAGE_CONTROL1(ExtensionUtilityHostMsg_ParseUpdateManifest_Succeeded,
                     UpdateManifest::Results /* updates */)

// Reply when an error occurred parsing the update manifest. |error_message|
// is a description of what went wrong suitable for logging.
IPC_MESSAGE_CONTROL1(ExtensionUtilityHostMsg_ParseUpdateManifest_Failed,
                     std::string /* error_message, if any */)

// Reply when the utility process is done unzipping a file. |unpacked_path|
// is the directory which contains the unzipped contents.
IPC_MESSAGE_CONTROL1(ExtensionUtilityHostMsg_UnzipToDir_Succeeded,
                     base::FilePath /* unpacked_path */)

// Reply when the utility process failed to unzip a file. |error| contains
// an error string to be reported to the user.
IPC_MESSAGE_CONTROL1(ExtensionUtilityHostMsg_UnzipToDir_Failed,
                     std::string /* error */)

// Reply when the utility process is done unpacking an extension.  |manifest|
// is the parsed manifest.json file.
// The unpacker should also have written out files containing the decoded
// images and message catalogs from the extension. The data is written into a
// DecodedImages struct into a file named kDecodedImagesFilename in the
// directory that was passed in. This is done because the data is too large to
// pass over IPC.
IPC_MESSAGE_CONTROL1(ExtensionUtilityHostMsg_UnpackExtension_Succeeded,
                     base::DictionaryValue /* manifest */)

// Reply when the utility process has failed while unpacking an extension.
// |error_message| is a user-displayable explanation of what went wrong.
IPC_MESSAGE_CONTROL1(ExtensionUtilityHostMsg_UnpackExtension_Failed,
                     base::string16 /* error_message, if any */)
