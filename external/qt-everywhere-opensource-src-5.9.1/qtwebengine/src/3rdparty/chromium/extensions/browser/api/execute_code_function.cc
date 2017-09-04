// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_EXECUTE_CODE_FUNCTION_IMPL_H_
#define EXTENSIONS_BROWSER_API_EXECUTE_CODE_FUNCTION_IMPL_H_

#include "extensions/browser/api/execute_code_function.h"

#include "extensions/browser/component_extension_resource_manager.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/file_reader.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/message_bundle.h"
#include "net/base/filename_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

// Error messages
const char kNoCodeOrFileToExecuteError[] = "No source code or file specified.";
const char kMoreThanOneValuesError[] =
    "Code and file should not be specified "
    "at the same time in the second argument.";
const char kBadFileEncodingError[] =
    "Could not load file '*' for content script. It isn't UTF-8 encoded.";
const char kLoadFileError[] = "Failed to load file: \"*\". ";

}

namespace extensions {

using api::extension_types::InjectDetails;

ExecuteCodeFunction::ExecuteCodeFunction() {
}

ExecuteCodeFunction::~ExecuteCodeFunction() {
}

void ExecuteCodeFunction::GetFileURLAndMaybeLocalizeOnFileThread(
    const std::string& extension_id,
    const base::FilePath& extension_path,
    const std::string& extension_default_locale,
    bool might_require_localization,
    std::string* data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::FILE);

  file_url_ = net::FilePathToFileURL(resource_.GetFilePath());

  if (!might_require_localization)
    return;

  bool needs_message_substituion =
      data->find(extensions::MessageBundle::kMessageBegin) != std::string::npos;
  if (!needs_message_substituion)
    return;

  std::unique_ptr<SubstitutionMap> localization_messages(
      file_util::LoadMessageBundleSubstitutionMap(extension_path, extension_id,
                                                  extension_default_locale));

  std::string error;
  MessageBundle::ReplaceMessagesWithExternalDictionary(*localization_messages,
                                                       data, &error);
}

void ExecuteCodeFunction::GetFileURLAndLocalizeComponentResourceOnFileThread(
    std::unique_ptr<std::string> data,
    const std::string& extension_id,
    const base::FilePath& extension_path,
    const std::string& extension_default_locale,
    bool might_require_localization) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::FILE);
  GetFileURLAndMaybeLocalizeOnFileThread(
      extension_id, extension_path, extension_default_locale,
      might_require_localization, data.get());

  bool success = true;
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&ExecuteCodeFunction::DidLoadAndLocalizeFile, this,
                 resource_.relative_path().AsUTF8Unsafe(), success,
                 base::Passed(std::move(data))));
}

void ExecuteCodeFunction::DidLoadAndLocalizeFile(
    const std::string& file,
    bool success,
    std::unique_ptr<std::string> data) {
  if (success) {
    if (!base::IsStringUTF8(*data)) {
      error_ = ErrorUtils::FormatErrorMessage(kBadFileEncodingError, file);
      SendResponse(false);
    } else if (!Execute(*data))
      SendResponse(false);
  } else {
    // TODO(viettrungluu): bug: there's no particular reason the path should be
    // UTF-8, in which case this may fail.
    error_ = ErrorUtils::FormatErrorMessage(kLoadFileError, file);
    SendResponse(false);
  }
}

bool ExecuteCodeFunction::Execute(const std::string& code_string) {
  ScriptExecutor* executor = GetScriptExecutor();
  if (!executor)
    return false;

  if (!extension() && !IsWebView())
    return false;

  ScriptExecutor::ScriptType script_type = ScriptExecutor::JAVASCRIPT;
  if (ShouldInsertCSS())
    script_type = ScriptExecutor::CSS;

  ScriptExecutor::FrameScope frame_scope =
      details_->all_frames.get() && *details_->all_frames
          ? ScriptExecutor::INCLUDE_SUB_FRAMES
          : ScriptExecutor::SINGLE_FRAME;

  int frame_id = details_->frame_id.get() ? *details_->frame_id
                                          : ExtensionApiFrameIdMap::kTopFrameId;

  ScriptExecutor::MatchAboutBlank match_about_blank =
      details_->match_about_blank.get() && *details_->match_about_blank
          ? ScriptExecutor::MATCH_ABOUT_BLANK
          : ScriptExecutor::DONT_MATCH_ABOUT_BLANK;

  UserScript::RunLocation run_at = UserScript::UNDEFINED;
  switch (details_->run_at) {
    case api::extension_types::RUN_AT_NONE:
    case api::extension_types::RUN_AT_DOCUMENT_IDLE:
      run_at = UserScript::DOCUMENT_IDLE;
      break;
    case api::extension_types::RUN_AT_DOCUMENT_START:
      run_at = UserScript::DOCUMENT_START;
      break;
    case api::extension_types::RUN_AT_DOCUMENT_END:
      run_at = UserScript::DOCUMENT_END;
      break;
  }
  CHECK_NE(UserScript::UNDEFINED, run_at);

  executor->ExecuteScript(
      host_id_, script_type, code_string, frame_scope, frame_id,
      match_about_blank, run_at, ScriptExecutor::ISOLATED_WORLD,
      IsWebView() ? ScriptExecutor::WEB_VIEW_PROCESS
                  : ScriptExecutor::DEFAULT_PROCESS,
      GetWebViewSrc(), file_url_, user_gesture(),
      has_callback() ? ScriptExecutor::JSON_SERIALIZED_RESULT
                     : ScriptExecutor::NO_RESULT,
      base::Bind(&ExecuteCodeFunction::OnExecuteCodeFinished, this));
  return true;
}

bool ExecuteCodeFunction::HasPermission() {
  return true;
}

bool ExecuteCodeFunction::RunAsync() {
  EXTENSION_FUNCTION_VALIDATE(Init());

  if (!details_->code.get() && !details_->file.get()) {
    error_ = kNoCodeOrFileToExecuteError;
    return false;
  }
  if (details_->code.get() && details_->file.get()) {
    error_ = kMoreThanOneValuesError;
    return false;
  }

  if (!CanExecuteScriptOnPage())
    return false;

  if (details_->code.get())
    return Execute(*details_->code);

  if (!details_->file.get())
    return false;

  return LoadFile(*details_->file);
}

bool ExecuteCodeFunction::LoadFile(const std::string& file) {
  resource_ = extension()->GetResource(file);

  if (resource_.extension_root().empty() || resource_.relative_path().empty()) {
    error_ = kNoCodeOrFileToExecuteError;
    return false;
  }

  const std::string& extension_id = extension()->id();
  base::FilePath extension_path = extension()->path();
  std::string extension_default_locale;
  extension()->manifest()->GetString(manifest_keys::kDefaultLocale,
                                     &extension_default_locale);
  // TODO(lazyboy): |extension_id| should not be empty(), turn this into a
  // DCHECK.
  bool might_require_localization = ShouldInsertCSS() && !extension_id.empty();

  int resource_id;
  const ComponentExtensionResourceManager*
      component_extension_resource_manager =
          ExtensionsBrowserClient::Get()
              ->GetComponentExtensionResourceManager();
  if (component_extension_resource_manager &&
      component_extension_resource_manager->IsComponentExtensionResource(
          resource_.extension_root(),
          resource_.relative_path(),
          &resource_id)) {
    base::StringPiece resource =
        ResourceBundle::GetSharedInstance().GetRawDataResource(resource_id);
    std::unique_ptr<std::string> data(
        new std::string(resource.data(), resource.size()));
    content::BrowserThread::PostTask(
        content::BrowserThread::FILE, FROM_HERE,
        base::Bind(&ExecuteCodeFunction::
                       GetFileURLAndLocalizeComponentResourceOnFileThread,
                   this, base::Passed(std::move(data)), extension_id,
                   extension_path, extension_default_locale,
                   might_require_localization));
  } else {
    FileReader::OptionalFileThreadTaskCallback get_file_and_l10n_callback =
        base::Bind(&ExecuteCodeFunction::GetFileURLAndMaybeLocalizeOnFileThread,
                   this, extension_id, extension_path, extension_default_locale,
                   might_require_localization);

    scoped_refptr<FileReader> file_reader(new FileReader(
        resource_, get_file_and_l10n_callback,
        base::Bind(&ExecuteCodeFunction::DidLoadAndLocalizeFile, this,
                   resource_.relative_path().AsUTF8Unsafe())));
    file_reader->Start();
  }

  return true;
}

void ExecuteCodeFunction::OnExecuteCodeFinished(const std::string& error,
                                                const GURL& on_url,
                                                const base::ListValue& result) {
  if (!error.empty())
    SetError(error);

  SendResponse(error.empty());
}

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_EXECUTE_CODE_FUNCTION_IMPL_H_
