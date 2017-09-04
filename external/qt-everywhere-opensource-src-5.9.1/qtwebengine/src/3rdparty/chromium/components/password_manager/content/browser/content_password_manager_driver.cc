// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/browser/content_password_manager_driver.h"

#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/content/browser/bad_message.h"
#include "components/password_manager/content/browser/content_password_manager_driver_factory.h"
#include "components/password_manager/content/browser/visible_password_observer.h"
#include "components/password_manager/core/browser/log_manager.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "net/cert/cert_status_flags.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace password_manager {

ContentPasswordManagerDriver::ContentPasswordManagerDriver(
    content::RenderFrameHost* render_frame_host,
    PasswordManagerClient* client,
    autofill::AutofillClient* autofill_client)
    : render_frame_host_(render_frame_host),
      client_(client),
      password_generation_manager_(client, this),
      password_autofill_manager_(this, autofill_client),
      next_free_key_(0),
      password_manager_binding_(this),
      weak_factory_(this) {
  // Does nothing if a VisiblePasswordObserver has already been created
  // for this WebContents.
  VisiblePasswordObserver::CreateForWebContents(
      content::WebContents::FromRenderFrameHost(render_frame_host_));
}

ContentPasswordManagerDriver::~ContentPasswordManagerDriver() {
}

// static
ContentPasswordManagerDriver*
ContentPasswordManagerDriver::GetForRenderFrameHost(
    content::RenderFrameHost* render_frame_host) {
  ContentPasswordManagerDriverFactory* factory =
      ContentPasswordManagerDriverFactory::FromWebContents(
          content::WebContents::FromRenderFrameHost(render_frame_host));
  return factory ? factory->GetDriverForFrame(render_frame_host) : nullptr;
}

void ContentPasswordManagerDriver::BindRequest(
    autofill::mojom::PasswordManagerDriverRequest request) {
  password_manager_binding_.Bind(std::move(request));
}

void ContentPasswordManagerDriver::BindSensitiveInputVisibilityServiceRequest(
    blink::mojom::SensitiveInputVisibilityServiceRequest request) {
  sensitive_input_visibility_bindings_.AddBinding(this, std::move(request));
}

void ContentPasswordManagerDriver::FillPasswordForm(
    const autofill::PasswordFormFillData& form_data) {
  const int key = next_free_key_++;
  password_autofill_manager_.OnAddPasswordFormMapping(key, form_data);
  GetPasswordAutofillAgent()->FillPasswordForm(key, form_data);
}

void ContentPasswordManagerDriver::AllowPasswordGenerationForForm(
    const autofill::PasswordForm& form) {
  if (!GetPasswordGenerationManager()->IsGenerationEnabled())
    return;
  GetPasswordGenerationAgent()->FormNotBlacklisted(form);
}

void ContentPasswordManagerDriver::FormsEligibleForGenerationFound(
    const std::vector<autofill::PasswordFormGenerationData>& forms) {
  GetPasswordGenerationAgent()->FoundFormsEligibleForGeneration(forms);
}

void ContentPasswordManagerDriver::AutofillDataReceived(
    const std::map<autofill::FormData,
                   autofill::PasswordFormFieldPredictionMap>& predictions) {
  GetPasswordAutofillAgent()->AutofillUsernameAndPasswordDataReceived(
      predictions);
}

void ContentPasswordManagerDriver::GeneratedPasswordAccepted(
    const base::string16& password) {
  GetPasswordGenerationAgent()->GeneratedPasswordAccepted(password);
}

void ContentPasswordManagerDriver::FillSuggestion(
    const base::string16& username,
    const base::string16& password) {
  GetAutofillAgent()->FillPasswordSuggestion(username, password);
}

void ContentPasswordManagerDriver::PreviewSuggestion(
    const base::string16& username,
    const base::string16& password) {
  GetAutofillAgent()->PreviewPasswordSuggestion(username, password);
}

void ContentPasswordManagerDriver::ShowInitialPasswordAccountSuggestions(
    const autofill::PasswordFormFillData& form_data) {
  const int key = next_free_key_++;
  password_autofill_manager_.OnAddPasswordFormMapping(key, form_data);
  GetAutofillAgent()->ShowInitialPasswordAccountSuggestions(key, form_data);
}

void ContentPasswordManagerDriver::ClearPreviewedForm() {
  GetAutofillAgent()->ClearPreviewedForm();
}

void ContentPasswordManagerDriver::ForceSavePassword() {
  GetPasswordAutofillAgent()->FindFocusedPasswordForm(
      base::Bind(&ContentPasswordManagerDriver::OnFocusedPasswordFormFound,
                 weak_factory_.GetWeakPtr()));
}

void ContentPasswordManagerDriver::GeneratePassword() {
  GetPasswordGenerationAgent()->UserTriggeredGeneratePassword();
}

void ContentPasswordManagerDriver::SendLoggingAvailability() {
  GetPasswordAutofillAgent()->SetLoggingState(
      client_->GetLogManager()->IsLoggingActive());
}

void ContentPasswordManagerDriver::AllowToRunFormClassifier() {
  GetPasswordGenerationAgent()->AllowToRunFormClassifier();
}

PasswordGenerationManager*
ContentPasswordManagerDriver::GetPasswordGenerationManager() {
  return &password_generation_manager_;
}

PasswordManager* ContentPasswordManagerDriver::GetPasswordManager() {
  return client_->GetPasswordManager();
}

PasswordAutofillManager*
ContentPasswordManagerDriver::GetPasswordAutofillManager() {
  return &password_autofill_manager_;
}

void ContentPasswordManagerDriver::PasswordFormsParsed(
    const std::vector<autofill::PasswordForm>& forms) {
  for (const auto& form : forms)
    if (!CheckChildProcessSecurityPolicy(
            form.origin, BadMessageReason::CPMD_BAD_ORIGIN_FORMS_PARSED))
      return;

  OnPasswordFormsParsedNoRenderCheck(forms);
}

void ContentPasswordManagerDriver::OnPasswordFormsParsedNoRenderCheck(
    const std::vector<autofill::PasswordForm>& forms) {
  GetPasswordManager()->OnPasswordFormsParsed(this, forms);
  GetPasswordGenerationManager()->CheckIfFormClassifierShouldRun();
}

void ContentPasswordManagerDriver::PasswordFormsRendered(
    const std::vector<autofill::PasswordForm>& visible_forms,
    bool did_stop_loading) {
  for (const auto& form : visible_forms)
    if (!CheckChildProcessSecurityPolicy(
            form.origin, BadMessageReason::CPMD_BAD_ORIGIN_FORMS_RENDERED))
      return;
  GetPasswordManager()->OnPasswordFormsRendered(this, visible_forms,
                                                did_stop_loading);
}

void ContentPasswordManagerDriver::PasswordFormSubmitted(
    const autofill::PasswordForm& password_form) {
  if (!CheckChildProcessSecurityPolicy(
          password_form.origin,
          BadMessageReason::CPMD_BAD_ORIGIN_FORM_SUBMITTED))
    return;
  GetPasswordManager()->OnPasswordFormSubmitted(this, password_form);
}

void ContentPasswordManagerDriver::OnFocusedPasswordFormFound(
    const autofill::PasswordForm& password_form) {
  if (!CheckChildProcessSecurityPolicy(
          password_form.origin,
          BadMessageReason::CPMD_BAD_ORIGIN_FOCUSED_PASSWORD_FORM_FOUND))
    return;
  GetPasswordManager()->OnPasswordFormForceSaveRequested(this, password_form);
}

void ContentPasswordManagerDriver::PasswordFieldVisibleInInsecureContext() {
  VisiblePasswordObserver* observer = VisiblePasswordObserver::FromWebContents(
      content::WebContents::FromRenderFrameHost(render_frame_host_));
  observer->RenderFrameHasVisiblePasswordField(render_frame_host_);
}

void ContentPasswordManagerDriver::
    AllPasswordFieldsInInsecureContextInvisible() {
  VisiblePasswordObserver* observer = VisiblePasswordObserver::FromWebContents(
      content::WebContents::FromRenderFrameHost(render_frame_host_));
  observer->RenderFrameHasNoVisiblePasswordFields(render_frame_host_);
}

void ContentPasswordManagerDriver::DidNavigateFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  // Clear page specific data after main frame navigation.
  if (!render_frame_host_->GetParent() && !details.is_in_page) {
    GetPasswordManager()->DidNavigateMainFrame();
    GetPasswordAutofillManager()->DidNavigateMainFrame();
  }
}

void ContentPasswordManagerDriver::InPageNavigation(
    const autofill::PasswordForm& password_form) {
  if (!CheckChildProcessSecurityPolicy(
          password_form.origin,
          BadMessageReason::CPMD_BAD_ORIGIN_IN_PAGE_NAVIGATION))
    return;
  GetPasswordManager()->OnInPageNavigation(this, password_form);
}

void ContentPasswordManagerDriver::PresaveGeneratedPassword(
    const autofill::PasswordForm& password_form) {
  if (!CheckChildProcessSecurityPolicy(
          password_form.origin,
          BadMessageReason::CPMD_BAD_ORIGIN_PRESAVE_GENERATED_PASSWORD))
    return;
  GetPasswordManager()->OnPresaveGeneratedPassword(password_form);
}

void ContentPasswordManagerDriver::PasswordNoLongerGenerated(
    const autofill::PasswordForm& password_form) {
  if (!CheckChildProcessSecurityPolicy(
          password_form.origin,
          BadMessageReason::CPMD_BAD_ORIGIN_PASSWORD_NO_LONGER_GENERATED))
    return;
  GetPasswordManager()->SetHasGeneratedPasswordForForm(this, password_form,
                                                       false);
}

void ContentPasswordManagerDriver::SaveGenerationFieldDetectedByClassifier(
    const autofill::PasswordForm& password_form,
    const base::string16& generation_field) {
  if (!CheckChildProcessSecurityPolicy(
          password_form.origin,
          BadMessageReason::
              CPMD_BAD_ORIGIN_SAVE_GENERATION_FIELD_DETECTED_BY_CLASSIFIER))
    return;
  GetPasswordManager()->SaveGenerationFieldDetectedByClassifier(
      password_form, generation_field);
}

void ContentPasswordManagerDriver::ShowPasswordSuggestions(
    int key,
    base::i18n::TextDirection text_direction,
    const base::string16& typed_username,
    int options,
    const gfx::RectF& bounds) {
  password_autofill_manager_.OnShowPasswordSuggestions(
      key, text_direction, typed_username, options, bounds);
}

void ContentPasswordManagerDriver::PasswordAutofillAgentConstructed() {
  SendLoggingAvailability();
}

void ContentPasswordManagerDriver::RecordSavePasswordProgress(
    const std::string& log) {
  client_->GetLogManager()->LogSavePasswordProgress(log);
}

bool ContentPasswordManagerDriver::CheckChildProcessSecurityPolicy(
    const GURL& url,
    BadMessageReason reason) {
  content::ChildProcessSecurityPolicy* policy =
      content::ChildProcessSecurityPolicy::GetInstance();
  if (!policy->CanAccessDataForOrigin(render_frame_host_->GetProcess()->GetID(),
                                      url)) {
    bad_message::ReceivedBadMessage(render_frame_host_->GetProcess(), reason);
    return false;
  }

  return true;
}

const autofill::mojom::AutofillAgentPtr&
ContentPasswordManagerDriver::GetAutofillAgent() {
  autofill::ContentAutofillDriver* autofill_driver =
      autofill::ContentAutofillDriver::GetForRenderFrameHost(
          render_frame_host_);
  DCHECK(autofill_driver);
  return autofill_driver->GetAutofillAgent();
}

const autofill::mojom::PasswordAutofillAgentPtr&
ContentPasswordManagerDriver::GetPasswordAutofillAgent() {
  if (!password_autofill_agent_) {
    autofill::mojom::PasswordAutofillAgentRequest request =
        mojo::GetProxy(&password_autofill_agent_);
    // Some test codes may have no initialized remote interfaces.
    if (render_frame_host_->GetRemoteInterfaces()) {
      render_frame_host_->GetRemoteInterfaces()->GetInterface(
          std::move(request));
    }
  }

  return password_autofill_agent_;
}

const autofill::mojom::PasswordGenerationAgentPtr&
ContentPasswordManagerDriver::GetPasswordGenerationAgent() {
  if (!password_gen_agent_) {
    render_frame_host_->GetRemoteInterfaces()->GetInterface(
        mojo::GetProxy(&password_gen_agent_));
  }

  return password_gen_agent_;
}

}  // namespace password_manager
