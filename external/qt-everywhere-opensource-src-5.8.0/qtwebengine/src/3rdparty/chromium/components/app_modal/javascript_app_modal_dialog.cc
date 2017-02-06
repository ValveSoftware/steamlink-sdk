// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/app_modal/javascript_app_modal_dialog.h"

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/app_modal/javascript_dialog_manager.h"
#include "components/app_modal/javascript_native_dialog_factory.h"
#include "ui/gfx/text_elider.h"

namespace app_modal {
namespace {

// Control maximum sizes of various texts passed to us from javascript.
#if defined(OS_POSIX) && !defined(OS_MACOSX)
// Two-dimensional eliding.  Reformat the text of the message dialog
// inserting line breaks because otherwise a single long line can overflow
// the message dialog (and crash/hang the GTK, depending on the version).
const int kMessageTextMaxRows = 32;
const int kMessageTextMaxCols = 132;
const int kDefaultPromptMaxRows = 24;
const int kDefaultPromptMaxCols = 132;
void EnforceMaxTextSize(const base::string16& in_string,
                        base::string16* out_string) {
  gfx::ElideRectangleString(in_string, kMessageTextMaxRows,
                           kMessageTextMaxCols, false, out_string);
}
void EnforceMaxPromptSize(const base::string16& in_string,
                          base::string16* out_string) {
  gfx::ElideRectangleString(in_string, kDefaultPromptMaxRows,
                           kDefaultPromptMaxCols, false, out_string);
}
#else
// One-dimensional eliding.  Trust the window system to break the string
// appropriately, but limit its overall length to something reasonable.
const size_t kMessageTextMaxSize = 2000;
const size_t kDefaultPromptMaxSize = 2000;
void EnforceMaxTextSize(const base::string16& in_string,
                        base::string16* out_string) {
  gfx::ElideString(in_string, kMessageTextMaxSize, out_string);
}
void EnforceMaxPromptSize(const base::string16& in_string,
                          base::string16* out_string) {
  gfx::ElideString(in_string, kDefaultPromptMaxSize, out_string);
}
#endif

}  // namespace

ChromeJavaScriptDialogExtraData::ChromeJavaScriptDialogExtraData()
    : has_already_shown_a_dialog_(false),
      suppress_javascript_messages_(false),
      suppressed_dialog_count_(0) {}

JavaScriptAppModalDialog::JavaScriptAppModalDialog(
    content::WebContents* web_contents,
    ExtraDataMap* extra_data_map,
    const base::string16& title,
    content::JavaScriptMessageType javascript_message_type,
    const base::string16& message_text,
    const base::string16& default_prompt_text,
    bool display_suppress_checkbox,
    bool is_before_unload_dialog,
    bool is_reload,
    const content::JavaScriptDialogManager::DialogClosedCallback& callback)
    : AppModalDialog(web_contents, title),
      extra_data_map_(extra_data_map),
      javascript_message_type_(javascript_message_type),
      display_suppress_checkbox_(display_suppress_checkbox),
      is_before_unload_dialog_(is_before_unload_dialog),
      is_reload_(is_reload),
      callback_(callback),
      use_override_prompt_text_(false),
      creation_time_(base::TimeTicks::Now()) {
  EnforceMaxTextSize(message_text, &message_text_);
  EnforceMaxPromptSize(default_prompt_text, &default_prompt_text_);
}

JavaScriptAppModalDialog::~JavaScriptAppModalDialog() {
}

NativeAppModalDialog* JavaScriptAppModalDialog::CreateNativeDialog() {
  return JavaScriptDialogManager::GetInstance()
      ->native_dialog_factory()
      ->CreateNativeJavaScriptDialog(this);
}

bool JavaScriptAppModalDialog::IsJavaScriptModalDialog() {
  return true;
}

void JavaScriptAppModalDialog::Invalidate() {
  if (!IsValid())
    return;

  AppModalDialog::Invalidate();
  CallDialogClosedCallback(false, base::string16());
  if (native_dialog())
    CloseModalDialog();
}

void JavaScriptAppModalDialog::OnCancel(bool suppress_js_messages) {
  // We need to do this before WM_DESTROY (WindowClosing()) as any parent frame
  // will receive its activation messages before this dialog receives
  // WM_DESTROY. The parent frame would then try to activate any modal dialogs
  // that were still open in the ModalDialogQueue, which would send activation
  // back to this one. The framework should be improved to handle this, so this
  // is a temporary workaround.
  CompleteDialog();

  NotifyDelegate(false, base::string16(), suppress_js_messages);
}

void JavaScriptAppModalDialog::OnAccept(const base::string16& prompt_text,
                                        bool suppress_js_messages) {
  base::string16 prompt_text_to_use = prompt_text;
  // This is only for testing.
  if (use_override_prompt_text_)
    prompt_text_to_use = override_prompt_text_;

  CompleteDialog();
  NotifyDelegate(true, prompt_text_to_use, suppress_js_messages);
}

void JavaScriptAppModalDialog::OnClose() {
  NotifyDelegate(false, base::string16(), false);
}

void JavaScriptAppModalDialog::SetOverridePromptText(
    const base::string16& override_prompt_text) {
  override_prompt_text_ = override_prompt_text;
  use_override_prompt_text_ = true;
}

void JavaScriptAppModalDialog::NotifyDelegate(bool success,
                                              const base::string16& user_input,
                                              bool suppress_js_messages) {
  if (!IsValid())
    return;

  CallDialogClosedCallback(success, user_input);

  // The close callback above may delete web_contents_, thus removing the extra
  // data from the map owned by ::JavaScriptDialogManager. Make sure
  // to only use the data if still present. http://crbug.com/236476
  ExtraDataMap::iterator extra_data = extra_data_map_->find(web_contents());
  if (extra_data != extra_data_map_->end()) {
    extra_data->second.has_already_shown_a_dialog_ = true;
    extra_data->second.suppress_javascript_messages_ = suppress_js_messages;
  }

  // On Views, we can end up coming through this code path twice :(.
  // See crbug.com/63732.
  AppModalDialog::Invalidate();
}

void JavaScriptAppModalDialog::CallDialogClosedCallback(bool success,
    const base::string16& user_input) {
  // TODO(joenotcharles): Both the callers of this function also check IsValid
  // and call AppModalDialog::Invalidate, but in different orders. If the
  // difference is not significant, more common code could be moved here.
  UMA_HISTOGRAM_MEDIUM_TIMES(
      "JSDialogs.FineTiming.TimeBetweenDialogCreatedAndSameDialogClosed",
      base::TimeTicks::Now() - creation_time_);
  if (!callback_.is_null()) {
    callback_.Run(success, user_input);
    callback_.Reset();
  }
}

}  // namespace app_modal
