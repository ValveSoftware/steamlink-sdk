// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_PASSWORD_AUTOFILL_AGENT_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_PASSWORD_AUTOFILL_AGENT_H_

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "components/autofill/content/renderer/password_form_conversion_utils.h"
#include "components/autofill/core/common/form_data_predictions.h"
#include "components/autofill/core/common/password_form_field_prediction_map.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_view_observer.h"
#include "third_party/WebKit/public/web/WebInputElement.h"

namespace blink {
class WebInputElement;
class WebKeyboardEvent;
class WebSecurityOrigin;
}

namespace autofill {

class RendererSavePasswordProgressLogger;

// This class is responsible for filling password forms.
class PasswordAutofillAgent : public content::RenderFrameObserver {
 public:
  explicit PasswordAutofillAgent(content::RenderFrame* render_frame);
  ~PasswordAutofillAgent() override;

  // WebFrameClient editor related calls forwarded by AutofillAgent.
  // If they return true, it indicates the event was consumed and should not
  // be used for any other autofill activity.
  bool TextFieldDidEndEditing(const blink::WebInputElement& element);
  bool TextDidChangeInTextField(const blink::WebInputElement& element);

  // Function that should be called whenever the value of |element| changes due
  // to user input. This is separate from TextDidChangeInTextField() as that
  // function may trigger UI and should only be called when other UI won't be
  // shown.
  void UpdateStateForTextChange(const blink::WebInputElement& element);

  // Fills the username and password fields of this form with the given values.
  // Returns true if the fields were filled, false otherwise.
  bool FillSuggestion(const blink::WebFormControlElement& node,
                      const blink::WebString& username,
                      const blink::WebString& password);

  // Previews the username and password fields of this form with the given
  // values. Returns true if the fields were previewed, false otherwise.
  bool PreviewSuggestion(const blink::WebFormControlElement& node,
                         const blink::WebString& username,
                         const blink::WebString& password);

  // Clears the preview for the username and password fields, restoring both to
  // their previous filled state. Return false if no login information was
  // found for the form.
  bool DidClearAutofillSelection(
      const blink::WebFormControlElement& control_element);

  // Shows an Autofill popup with username suggestions for |element|. If
  // |show_all| is |true|, will show all possible suggestions for that element,
  // otherwise shows suggestions based on current value of |element|.
  // If |generation_popup_showing| is true, this function will return false
  // as both UIs should not be shown at the same time. This function should
  // still be called in this situation so that UMA stats can be logged.
  // Returns true if any suggestions were shown, false otherwise.
  bool ShowSuggestions(const blink::WebInputElement& element,
                       bool show_all,
                       bool generation_popup_showing);

  // Called when new form controls are inserted.
  void OnDynamicFormsSeen();

  // Called when an AJAX has succesfully completed. Used to determine if
  // a form has been submitted by AJAX without navigation.
  void AJAXSucceeded();

  // Called when the user first interacts with the page after a load. This is a
  // signal to make autofilled values of password input elements accessible to
  // JavaScript.
  void FirstUserGestureObserved();

  // Given password form data |form_data| and a supplied key |key| for
  // referencing the password info, returns a set of WebInputElements in
  // |elements|, which must be non-null, that the password manager has values
  // for filling. Also takes an optional logger |logger| for logging password
  // autofill behavior.
  void GetFillableElementFromFormData(
      int key,
      const PasswordFormFillData& form_data,
      RendererSavePasswordProgressLogger* logger,
      std::vector<blink::WebInputElement>* elements);

  bool logging_state_active() const { return logging_state_active_; }

 protected:
  virtual bool OriginCanAccessPasswordManager(
      const blink::WebSecurityOrigin& origin);

 private:
  // Ways to restrict which passwords are saved in ProvisionallySavePassword.
  enum ProvisionallySaveRestriction {
    RESTRICTION_NONE,
    RESTRICTION_NON_EMPTY_PASSWORD
  };

  struct PasswordInfo {
    blink::WebInputElement password_field;
    PasswordFormFillData fill_data;
    // The user manually edited the password more recently than the username was
    // changed.
    bool password_was_edited_last = false;
    // The user accepted a suggestion from a dropdown on a password field.
    bool password_field_suggestion_was_accepted = false;
    // The key under which PasswordAutofillManager can find info for filling.
    int key = -1;
  };
  typedef std::map<blink::WebInputElement, PasswordInfo>
      WebInputToPasswordInfoMap;
  typedef std::map<blink::WebElement, int> WebElementToPasswordInfoKeyMap;
  typedef std::map<blink::WebInputElement, blink::WebInputElement>
      PasswordToLoginMap;

  // This class keeps track of autofilled password input elements and makes sure
  // the autofilled password value is not accessible to JavaScript code until
  // the user interacts with the page.
  class PasswordValueGatekeeper {
   public:
    PasswordValueGatekeeper();
    ~PasswordValueGatekeeper();

    // Call this for every autofilled password field, so that the gatekeeper
    // protects the value accordingly.
    void RegisterElement(blink::WebInputElement* element);

    // Call this to notify the gatekeeper that the user interacted with the
    // page.
    void OnUserGesture();

    // Call this to reset the gatekeeper on a new page navigation.
    void Reset();

   private:
    // Make the value of |element| accessible to JavaScript code.
    void ShowValue(blink::WebInputElement* element);

    bool was_user_gesture_seen_;
    std::vector<blink::WebInputElement> elements_;

    DISALLOW_COPY_AND_ASSIGN(PasswordValueGatekeeper);
  };

  // RenderFrameObserver:
  bool OnMessageReceived(const IPC::Message& message) override;
  void DidFinishDocumentLoad() override;
  void DidFinishLoad() override;
  void FrameDetached() override;
  void FrameWillClose() override;
  void DidStartProvisionalLoad() override;
  void DidCommitProvisionalLoad(bool is_new_navigation,
                                bool is_same_page_navigation) override;
  void WillSendSubmitEvent(const blink::WebFormElement& form) override;
  void WillSubmitForm(const blink::WebFormElement& form) override;
  void OnDestruct() override;

  // RenderView IPC handlers:
  void OnFillPasswordForm(int key, const PasswordFormFillData& form_data);
  void OnSetLoggingState(bool active);
  void OnAutofillUsernameAndPasswordDataReceived(
      const FormsPredictionsMap& predictions);
  void OnFindFocusedPasswordForm();

  // Scans the given frame for password forms and sends them up to the browser.
  // If |only_visible| is true, only forms visible in the layout are sent.
  void SendPasswordForms(bool only_visible);

  // Instructs the browser to show a pop-up suggesting which credentials could
  // be filled. |show_in_password_field| should indicate whether the pop-up is
  // to be shown on the password field instead of on the username field. If the
  // username exists, it should be passed as |user_input|. If there is no
  // username, pass the password field in |user_input|. In the latter case, no
  // username value will be shown in the pop-up.
  bool ShowSuggestionPopup(const PasswordInfo& password_info,
                           const blink::WebInputElement& user_input,
                           bool show_all,
                           bool show_on_password_field);

  // Finds the PasswordInfo, username and password fields that corresponds to
  // the passed in |element|. |element| can refer either to a username element
  // or a password element. If a PasswordInfo was found, returns |true| and also
  // assigns the corresponding username, password elements and PasswordInfo into
  // |username_element|, |password_element| and |pasword_info|, respectively.
  // Note, that |username_element->isNull()| can be true if |element| is a
  // password.
  bool FindPasswordInfoForElement(const blink::WebInputElement& element,
                                  blink::WebInputElement* username_element,
                                  blink::WebInputElement* password_element,
                                  PasswordInfo** password_info);

  // Invoked when the frame is closing.
  void FrameClosing();

  // Clears the preview for the username and password fields, restoring both to
  // their previous filled state.
  void ClearPreview(blink::WebInputElement* username,
                    blink::WebInputElement* password);

  // Saves |password_form| in |provisionally_saved_form_|, as long as it
  // satisfies |restriction|.
  void ProvisionallySavePassword(std::unique_ptr<PasswordForm> password_form,
                                 ProvisionallySaveRestriction restriction);

  // Returns true if |provisionally_saved_form_| has enough information that
  // it is likely filled out.
  bool ProvisionallySavedPasswordIsValid();

  // Helper function called when in-page navigation completed
  void OnSamePageNavigationCompleted();

  // The logins we have filled so far with their associated info.
  WebInputToPasswordInfoMap web_input_to_password_info_;
  // A (sort-of) reverse map to |login_to_password_info_|.
  PasswordToLoginMap password_to_username_;

  // Set if the user might be submitting a password form on the current page,
  // but the submit may still fail (i.e. doesn't pass JavaScript validation).
  std::unique_ptr<PasswordForm> provisionally_saved_form_;

  // Contains the most recent text that user typed or PasswordManager autofilled
  // in input elements. Used for storing username/password before JavaScript
  // changes them.
  ModifiedValues nonscript_modified_values_;

  PasswordValueGatekeeper gatekeeper_;

  // True indicates that user debug information should be logged.
  bool logging_state_active_;

  // True indicates that the username field was autofilled, false otherwise.
  bool was_username_autofilled_;
  // True indicates that the password field was autofilled, false otherwise.
  bool was_password_autofilled_;

  // Records the username typed before suggestions preview.
  base::string16 username_query_prefix_;

  // Contains server predictions for username, password and/or new password
  // fields for individual forms.
  FormsPredictionsMap form_predictions_;

  DISALLOW_COPY_AND_ASSIGN(PasswordAutofillAgent);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_PASSWORD_AUTOFILL_AGENT_H_
