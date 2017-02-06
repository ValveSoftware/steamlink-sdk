// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_EXECUTE_CODE_FUNCTION_H_
#define EXTENSIONS_BROWSER_API_EXECUTE_CODE_FUNCTION_H_

#include "extensions/browser/extension_function.h"
#include "extensions/browser/script_executor.h"
#include "extensions/common/api/extension_types.h"
#include "extensions/common/host_id.h"

namespace extensions {

// Base class for javascript code injection.
// This is used by both chrome.webview.executeScript and
// chrome.tabs.executeScript.
class ExecuteCodeFunction : public AsyncExtensionFunction {
 public:
  ExecuteCodeFunction();

 protected:
  ~ExecuteCodeFunction() override;

  // ExtensionFunction implementation.
  bool HasPermission() override;
  bool RunAsync() override;

  // Initialize |details_| if it hasn't already been.
  virtual bool Init() = 0;
  virtual bool ShouldInsertCSS() const = 0;
  virtual bool CanExecuteScriptOnPage() = 0;
  virtual ScriptExecutor* GetScriptExecutor() = 0;
  virtual bool IsWebView() const = 0;
  virtual const GURL& GetWebViewSrc() const = 0;
  virtual void OnExecuteCodeFinished(const std::string& error,
                                     const GURL& on_url,
                                     const base::ListValue& result);

  virtual bool LoadFile(const std::string& file);

  // Called when contents from the loaded file have been localized.
  void DidLoadAndLocalizeFile(const std::string& file,
                              bool success,
                              const std::string& data);

  const HostID& host_id() const { return host_id_; }
  void set_host_id(HostID host_id) {
    host_id_ = host_id;
  }

  // The injection details.
  std::unique_ptr<api::extension_types::InjectDetails> details_;

 private:
  // Called when contents from the file whose path is specified in JSON
  // arguments has been loaded.
  void DidLoadFile(bool success, const std::string& data);

  // Runs on FILE thread. Loads message bundles for the extension and
  // localizes the CSS data. Calls back DidLoadAndLocalizeFile on the UI thread.
  void GetFileURLAndLocalizeCSS(ScriptExecutor::ScriptType script_type,
                                const std::string& data,
                                const std::string& extension_id,
                                const base::FilePath& extension_path,
                                const std::string& extension_default_locale);

  // Run in UI thread.  Code string contains the code to be executed. Returns
  // true on success. If true is returned, this does an AddRef.
  bool Execute(const std::string& code_string);

  // Contains extension resource built from path of file which is
  // specified in JSON arguments.
  ExtensionResource resource_;

  // The URL of the file being injected into the page.
  GURL file_url_;

  // The ID of the injection host.
  HostID host_id_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_EXECUTE_CODE_FUNCTION_H_
