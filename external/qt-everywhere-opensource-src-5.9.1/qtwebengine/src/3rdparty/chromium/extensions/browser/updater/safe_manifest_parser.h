// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_UPDATER_SAFE_MANIFEST_PARSER_H_
#define EXTENSIONS_BROWSER_UPDATER_SAFE_MANIFEST_PARSER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "content/public/browser/utility_process_host_client.h"
#include "extensions/browser/updater/manifest_fetch_data.h"
#include "extensions/common/update_manifest.h"

namespace extensions {

// Utility class to handle doing xml parsing in a sandboxed utility process.
class SafeManifestParser : public content::UtilityProcessHostClient {
 public:
  // Callback that is invoked when the manifest results are ready.
  typedef base::Callback<void(const UpdateManifest::Results*)> ResultsCallback;

  SafeManifestParser(const std::string& xml,
                     const ResultsCallback& results_callback);

  // Posts a task over to the IO loop to start the parsing of xml_ in a
  // utility process.
  void Start();

 private:
  ~SafeManifestParser() override;

  // Creates the sandboxed utility process and tells it to start parsing.
  void ParseInSandbox();

  // content::UtilityProcessHostClient implementation.
  bool OnMessageReceived(const IPC::Message& message) override;

  void OnParseUpdateManifestSucceeded(const UpdateManifest::Results& results);
  void OnParseUpdateManifestFailed(const std::string& error_message);

  const std::string xml_;

  // Should be accessed only on UI thread.
  ResultsCallback results_callback_;

  DISALLOW_COPY_AND_ASSIGN(SafeManifestParser);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_UPDATER_SAFE_MANIFEST_PARSER_H_
