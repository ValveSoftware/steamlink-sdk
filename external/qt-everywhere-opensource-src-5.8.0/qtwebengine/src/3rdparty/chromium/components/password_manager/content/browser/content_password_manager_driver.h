// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_CONTENT_PASSWORD_MANAGER_DRIVER_H_
#define COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_CONTENT_PASSWORD_MANAGER_DRIVER_H_

#include <map>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/autofill/core/common/password_form_field_prediction_map.h"
#include "components/autofill/core/common/password_form_generation_data.h"
#include "components/password_manager/core/browser/password_autofill_manager.h"
#include "components/password_manager/core/browser/password_generation_manager.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_driver.h"

namespace autofill {
class AutofillManager;
struct PasswordForm;
}

namespace content {
struct FrameNavigateParams;
struct LoadCommittedDetails;
class RenderFrameHost;
class WebContents;
}

namespace IPC {
class Message;
}

namespace password_manager {
enum class BadMessageReason;

// There is one ContentPasswordManagerDriver per RenderFrameHost.
// The lifetime is managed by the ContentPasswordManagerDriverFactory.
class ContentPasswordManagerDriver : public PasswordManagerDriver {
 public:
  ContentPasswordManagerDriver(content::RenderFrameHost* render_frame_host,
                               PasswordManagerClient* client,
                               autofill::AutofillClient* autofill_client);
  ~ContentPasswordManagerDriver() override;

  // Gets the driver for |render_frame_host|.
  static ContentPasswordManagerDriver* GetForRenderFrameHost(
      content::RenderFrameHost* render_frame_host);

  // PasswordManagerDriver implementation.
  void FillPasswordForm(
      const autofill::PasswordFormFillData& form_data) override;
  void AllowPasswordGenerationForForm(
      const autofill::PasswordForm& form) override;
  void FormsEligibleForGenerationFound(
      const std::vector<autofill::PasswordFormGenerationData>& forms) override;
  void AutofillDataReceived(
      const std::map<autofill::FormData,
                     autofill::PasswordFormFieldPredictionMap>& predictions)
      override;
  void GeneratedPasswordAccepted(const base::string16& password) override;
  void FillSuggestion(const base::string16& username,
                      const base::string16& password) override;
  void PreviewSuggestion(const base::string16& username,
                         const base::string16& password) override;
  void ShowInitialPasswordAccountSuggestions(
      const autofill::PasswordFormFillData& form_data) override;
  void ClearPreviewedForm() override;
  void ForceSavePassword() override;
  void GeneratePassword() override;
  void SendLoggingAvailability() override;
  void AllowToRunFormClassifier() override;

  PasswordGenerationManager* GetPasswordGenerationManager() override;
  PasswordManager* GetPasswordManager() override;
  PasswordAutofillManager* GetPasswordAutofillManager() override;

  bool HandleMessage(const IPC::Message& message);
  void DidNavigateFrame(const content::LoadCommittedDetails& details,
                        const content::FrameNavigateParams& params);

  // Pass-throughs to PasswordManager.
  void OnPasswordFormsParsed(const std::vector<autofill::PasswordForm>& forms);
  void OnPasswordFormsParsedNoRenderCheck(
      const std::vector<autofill::PasswordForm>& forms);
  void OnPasswordFormsRendered(
      const std::vector<autofill::PasswordForm>& visible_forms,
      bool did_stop_loading);
  void OnPasswordFormSubmitted(const autofill::PasswordForm& password_form);
  void OnInPageNavigation(const autofill::PasswordForm& password_form);
  void OnPresaveGeneratedPassword(const autofill::PasswordForm& password_form);
  void OnPasswordNoLongerGenerated(const autofill::PasswordForm& password_form);
  void OnFocusedPasswordFormFound(const autofill::PasswordForm& password_form);
  void OnSaveGenerationFieldDetectedByClassifier(
      const autofill::PasswordForm& password_form,
      const base::string16& generation_field);

 private:
  bool CheckChildProcessSecurityPolicy(const GURL& url,
                                       BadMessageReason reason);

  content::RenderFrameHost* render_frame_host_;
  PasswordManagerClient* client_;
  PasswordGenerationManager password_generation_manager_;
  PasswordAutofillManager password_autofill_manager_;

  // Every instance of PasswordFormFillData created by |*this| and sent to
  // PasswordAutofillManager and PasswordAutofillAgent is given an ID, so that
  // the latter two classes can reference to the same instance without sending
  // it to each other over IPC. The counter below is used to generate new IDs.
  int next_free_key_;

  DISALLOW_COPY_AND_ASSIGN(ContentPasswordManagerDriver);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_CONTENT_PASSWORD_MANAGER_DRIVER_H_
