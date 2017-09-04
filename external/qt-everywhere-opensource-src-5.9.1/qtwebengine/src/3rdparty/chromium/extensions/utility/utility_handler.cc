// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/utility/utility_handler.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/i18n/rtl.h"
#include "content/public/utility/utility_thread.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_l10n_util.h"
#include "extensions/common/extension_utility_messages.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/manifest.h"
#include "extensions/common/update_manifest.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "extensions/utility/unpacker.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/zlib/google/zip.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_switches.h"

namespace extensions {

namespace {

bool Send(IPC::Message* message) {
  return content::UtilityThread::Get()->Send(message);
}

void ReleaseProcessIfNeeded() {
  content::UtilityThread::Get()->ReleaseProcessIfNeeded();
}

const char kExtensionHandlerUnzipError[] =
    "Could not unzip extension for install.";

}  // namespace

UtilityHandler::UtilityHandler() {
}

UtilityHandler::~UtilityHandler() {
}

// static
void UtilityHandler::UtilityThreadStarted() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  std::string lang = command_line->GetSwitchValueASCII(switches::kLang);
  if (!lang.empty())
    extension_l10n_util::SetProcessLocale(lang);
}

bool UtilityHandler::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(UtilityHandler, message)
    IPC_MESSAGE_HANDLER(ExtensionUtilityMsg_ParseUpdateManifest,
                        OnParseUpdateManifest)
    IPC_MESSAGE_HANDLER(ExtensionUtilityMsg_UnzipToDir, OnUnzipToDir)
    IPC_MESSAGE_HANDLER(ExtensionUtilityMsg_UnpackExtension, OnUnpackExtension)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void UtilityHandler::OnParseUpdateManifest(const std::string& xml) {
  UpdateManifest manifest;
  if (!manifest.Parse(xml)) {
    Send(new ExtensionUtilityHostMsg_ParseUpdateManifest_Failed(
        manifest.errors()));
  } else {
    Send(new ExtensionUtilityHostMsg_ParseUpdateManifest_Succeeded(
        manifest.results()));
  }
  ReleaseProcessIfNeeded();
}

void UtilityHandler::OnUnzipToDir(const base::FilePath& zip_path,
                                  const base::FilePath& dir) {
  // First extract only the manifest to determine the extension type.
  if (!zip::UnzipWithFilterCallback(zip_path, dir,
                                    base::Bind(&Unpacker::IsManifestFile))) {
    Send(new ExtensionUtilityHostMsg_UnzipToDir_Failed(
        std::string(kExtensionHandlerUnzipError)));
    ReleaseProcessIfNeeded();
    return;
  }

  // Load the manifest.
  std::string error;
  std::unique_ptr<base::DictionaryValue> dict =
      Unpacker::ReadManifest(dir, &error);
  if (!dict.get()) {
    Send(new ExtensionUtilityHostMsg_UnzipToDir_Failed(
        std::string(kExtensionHandlerUnzipError)));
    ReleaseProcessIfNeeded();
    return;
  }

  Manifest manifest(Manifest::INTERNAL, std::move(dict));
  base::Callback<bool(const base::FilePath&)> filetype_filter_cb =
      base::Bind(&Unpacker::ShouldExtractFile, manifest.is_theme());

  // TODO(crbug.com/645263): This silently ignores blocked file types.
  //                         Add install warnings.
  if (!zip::UnzipWithFilterCallback(zip_path, dir, filetype_filter_cb)) {
    Send(new ExtensionUtilityHostMsg_UnzipToDir_Failed(
        std::string(kExtensionHandlerUnzipError)));
  } else {
    Send(new ExtensionUtilityHostMsg_UnzipToDir_Succeeded(dir));
  }
  ReleaseProcessIfNeeded();
}

void UtilityHandler::OnUnpackExtension(const base::FilePath& directory_path,
                                       const std::string& extension_id,
                                       int location,
                                       int creation_flags) {
  CHECK_GT(location, Manifest::INVALID_LOCATION);
  CHECK_LT(location, Manifest::NUM_LOCATIONS);
  DCHECK(ExtensionsClient::Get());
  content::UtilityThread::Get()->EnsureBlinkInitialized();
  Unpacker unpacker(directory_path.DirName(), directory_path, extension_id,
                    static_cast<Manifest::Location>(location), creation_flags);
  if (unpacker.Run()) {
    Send(new ExtensionUtilityHostMsg_UnpackExtension_Succeeded(
        *unpacker.parsed_manifest()));
  } else {
    Send(new ExtensionUtilityHostMsg_UnpackExtension_Failed(
        unpacker.error_message()));
  }
  ReleaseProcessIfNeeded();
}


}  // namespace extensions
