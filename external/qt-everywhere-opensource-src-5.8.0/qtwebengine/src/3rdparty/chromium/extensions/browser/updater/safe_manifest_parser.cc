// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/updater/safe_manifest_parser.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/utility_process_host.h"
#include "content/public/common/content_switches.h"
#include "extensions/common/extension_utility_messages.h"
#include "ipc/ipc_message_macros.h"
#include "grit/extensions_strings.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;

namespace extensions {

SafeManifestParser::SafeManifestParser(const std::string& xml,
                                       const ResultsCallback& results_callback)
    : xml_(xml), results_callback_(results_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void SafeManifestParser::Start() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!BrowserThread::PostTask(
          BrowserThread::IO,
          FROM_HERE,
          base::Bind(&SafeManifestParser::ParseInSandbox, this))) {
    NOTREACHED();
  }
}

SafeManifestParser::~SafeManifestParser() {
  // If we're using UtilityProcessHost, we may not be destroyed on
  // the UI or IO thread.
}

void SafeManifestParser::ParseInSandbox() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  content::UtilityProcessHost* host = content::UtilityProcessHost::Create(
      this,
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI).get());
  host->SetName(
      l10n_util::GetStringUTF16(IDS_UTILITY_PROCESS_MANIFEST_PARSER_NAME));
  host->Send(new ExtensionUtilityMsg_ParseUpdateManifest(xml_));
}

bool SafeManifestParser::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SafeManifestParser, message)
    IPC_MESSAGE_HANDLER(ExtensionUtilityHostMsg_ParseUpdateManifest_Succeeded,
                        OnParseUpdateManifestSucceeded)
    IPC_MESSAGE_HANDLER(ExtensionUtilityHostMsg_ParseUpdateManifest_Failed,
                        OnParseUpdateManifestFailed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void SafeManifestParser::OnParseUpdateManifestSucceeded(
    const UpdateManifest::Results& results) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  results_callback_.Run(&results);
}

void SafeManifestParser::OnParseUpdateManifestFailed(
    const std::string& error_message) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  LOG(WARNING) << "Error parsing update manifest:\n" << error_message;
  results_callback_.Run(NULL);
}

}  // namespace extensions
