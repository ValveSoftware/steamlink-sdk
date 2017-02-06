// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/password_form_conversion_utils.h"

#include <stddef.h>

#include <algorithm>
#include <string>

#include "base/i18n/case_conversion.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_form_field_prediction_map.h"
#include "google_apis/gaia/gaia_urls.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFormControlElement.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebInputElement.h"
#include "third_party/re2/src/re2/re2.h"

using blink::WebDocument;
using blink::WebFormControlElement;
using blink::WebFormElement;
using blink::WebFrame;
using blink::WebInputElement;
using blink::WebString;

namespace autofill {
namespace {

// PasswordForms can be constructed for both WebFormElements and for collections
// of WebInputElements that are not in a WebFormElement. This intermediate
// aggregating structure is provided so GetPasswordForm() only has one
// view of the underlying data, regardless of its origin.
struct SyntheticForm {
  SyntheticForm();
  ~SyntheticForm();

  std::vector<blink::WebElement> fieldsets;
  // Contains control elements of the represented form, including not fillable
  // ones.
  std::vector<blink::WebFormControlElement> control_elements;
  blink::WebDocument document;
  blink::WebString action;

 private:
  DISALLOW_COPY_AND_ASSIGN(SyntheticForm);
};

SyntheticForm::SyntheticForm() {}
SyntheticForm::~SyntheticForm() {}

// Layout classification of password forms
// A layout sequence of a form is the sequence of it's non-password and password
// input fields, represented by "N" and "P", respectively. A form like this
// <form>
//   <input type='text' ...>
//   <input type='hidden' ...>
//   <input type='password' ...>
//   <input type='submit' ...>
// </form>
// has the layout sequence "NP" -- "N" for the first field, and "P" for the
// third. The second and fourth fields are ignored, because they are not text
// fields.
//
// The code below classifies the layout (see PasswordForm::Layout) of a form
// based on its layout sequence. This is done by assigning layouts regular
// expressions over the alphabet {N, P}. LAYOUT_OTHER is implicitly the type
// corresponding to all layout sequences not matching any other layout.
//
// LAYOUT_LOGIN_AND_SIGNUP is classified by NPN+P.*. This corresponds to a form
// which starts with a login section (NP) and continues with a sign-up section
// (N+P.*). The aim is to distinguish such forms from change password-forms
// (N*PPP?.*) and forms which use password fields to store private but
// non-password data (could look like, e.g., PN+P.*).
const char kLoginAndSignupRegex[] =
    "NP"   // Login section.
    "N+P"  // Sign-up section.
    ".*";  // Anything beyond that.

const char kAutocompleteUsername[] = "username";
const char kAutocompleteCurrentPassword[] = "current-password";
const char kAutocompleteNewPassword[] = "new-password";

re2::RE2* CreateMatcher(void* instance, const char* pattern) {
  re2::RE2::Options options;
  options.set_case_sensitive(false);
  // Use placement new to initialize the instance in the preallocated space.
  // The "(instance)" is very important to force POD type initialization.
  re2::RE2* matcher = new (instance) re2::RE2(pattern, options);
  DCHECK(matcher->ok());
  return matcher;
}

struct LoginAndSignupLazyInstanceTraits
    : public base::DefaultLazyInstanceTraits<re2::RE2> {
  static re2::RE2* New(void* instance) {
    return CreateMatcher(instance, kLoginAndSignupRegex);
  }
};

base::LazyInstance<re2::RE2, LoginAndSignupLazyInstanceTraits>
    g_login_and_signup_matcher = LAZY_INSTANCE_INITIALIZER;

// Given the sequence of non-password and password text input fields of a form,
// represented as a string of Ns (non-password) and Ps (password), computes the
// layout type of that form.
PasswordForm::Layout SequenceToLayout(base::StringPiece layout_sequence) {
  if (re2::RE2::FullMatch(
          re2::StringPiece(layout_sequence.data(),
                           base::checked_cast<int>(layout_sequence.size())),
          g_login_and_signup_matcher.Get()))
    return PasswordForm::Layout::LAYOUT_LOGIN_AND_SIGNUP;
  return PasswordForm::Layout::LAYOUT_OTHER;
}

void PopulateSyntheticFormFromWebForm(const WebFormElement& web_form,
                                      SyntheticForm* synthetic_form) {
  // TODO(vabr): The fact that we are actually passing all form fields, not just
  // autofillable ones (cause of http://crbug.com/537396, see also
  // http://crbug.com/543006) is not tested yet, due to difficulties to fake
  // test frame origin to match GAIA login page. Once this code gets moved to
  // browser, we need to add tests for this as well.
  blink::WebVector<blink::WebFormControlElement> web_control_elements;
  web_form.getFormControlElements(web_control_elements);
  synthetic_form->control_elements.assign(web_control_elements.begin(),
                                          web_control_elements.end());
  synthetic_form->action = web_form.action();
  synthetic_form->document = web_form.document();
}

// Helper function that removes |username_element.value()| from the vector
// |other_possible_usernames|, if the value presents in the vector.
void ExcludeUsernameFromOtherUsernamesList(
    const WebInputElement& username_element,
    std::vector<base::string16>* other_possible_usernames) {
  other_possible_usernames->erase(
      std::remove(other_possible_usernames->begin(),
                  other_possible_usernames->end(),
                  username_element.value()),
      other_possible_usernames->end());
}

// Helper to determine which password is the main (current) one, and which is
// the new password (e.g., on a sign-up or change password form), if any.
bool LocateSpecificPasswords(std::vector<WebInputElement> passwords,
                             WebInputElement* current_password,
                             WebInputElement* new_password) {
  DCHECK(current_password && current_password->isNull());
  DCHECK(new_password && new_password->isNull());

  // First, look for elements marked with either autocomplete='current-password'
  // or 'new-password' -- if we find any, take the hint, and treat the first of
  // each kind as the element we are looking for.
  for (const WebInputElement& it : passwords) {
    if (HasAutocompleteAttributeValue(it, kAutocompleteCurrentPassword) &&
        current_password->isNull()) {
      *current_password = it;
    } else if (HasAutocompleteAttributeValue(it, kAutocompleteNewPassword) &&
               new_password->isNull()) {
      *new_password = it;
    }
  }

  // If we have seen an element with either of autocomplete attributes above,
  // take that as a signal that the page author must have intentionally left the
  // rest of the password fields unmarked. Perhaps they are used for other
  // purposes, e.g., PINs, OTPs, and the like. So we skip all the heuristics we
  // normally do, and ignore the rest of the password fields.
  if (!current_password->isNull() || !new_password->isNull())
    return true;

  if (passwords.empty())
    return false;

  switch (passwords.size()) {
    case 1:
      // Single password, easy.
      *current_password = passwords[0];
      break;
    case 2:
      if (!passwords[0].value().isEmpty() &&
          passwords[0].value() == passwords[1].value()) {
        // Two identical non-empty passwords: assume we are seeing a new
        // password with a confirmation. This can be either a sign-up form or a
        // password change form that does not ask for the old password.
        *new_password = passwords[0];
      } else {
        // Assume first is old password, second is new (no choice but to guess).
        // This case also includes empty passwords in order to allow filling of
        // password change forms (that also could autofill for sign up form, but
        // we can't do anything with this using only client side information).
        *current_password = passwords[0];
        *new_password = passwords[1];
      }
      break;
    default:
      if (!passwords[0].value().isEmpty() &&
          passwords[0].value() == passwords[1].value() &&
          passwords[0].value() == passwords[2].value()) {
        // All three passwords are the same and non-empty? This does not make
        // any sense, give up.
        return false;
      } else if (passwords[1].value() == passwords[2].value()) {
        // New password is the duplicated one, and comes second; or empty form
        // with 3 password fields, in which case we will assume this layout.
        *current_password = passwords[0];
        *new_password = passwords[1];
      } else if (passwords[0].value() == passwords[1].value()) {
        // It is strange that the new password comes first, but trust more which
        // fields are duplicated than the ordering of fields. Assume that
        // any password fields after the new password contain sensitive
        // information that isn't actually a password (security hint, SSN, etc.)
        *new_password = passwords[0];
      } else {
        // Three different passwords, or first and last match with middle
        // different. No idea which is which, so no luck.
        return false;
      }
  }
  return true;
}

// Checks the |form_predictions| map to see if there is a key associated with
// the |prediction_type| value. Assigns the key to |prediction_field| and
// returns true if it is found.
bool MapContainsPrediction(
    const std::map<WebInputElement, PasswordFormFieldPredictionType>&
        form_predictions,
    PasswordFormFieldPredictionType prediction_type,
    WebInputElement* prediction_field) {
  for (auto it = form_predictions.begin(); it != form_predictions.end(); ++it) {
    if (it->second == prediction_type) {
      (*prediction_field) = it->first;
      return true;
    }
  }
  return false;
}

void FindPredictedElements(
    const SyntheticForm& form,
    const FormData& form_data,
    const FormsPredictionsMap& form_predictions,
    std::map<WebInputElement, PasswordFormFieldPredictionType>*
        predicted_elements) {
  // Matching only requires that action and name of the form match to allow
  // the username to be updated even if the form is changed after page load.
  // See https://crbug.com/476092 for more details.
  auto predictions_it = form_predictions.begin();
  for (; predictions_it != form_predictions.end(); ++predictions_it) {
    if (predictions_it->first.action == form_data.action &&
        predictions_it->first.name == form_data.name) {
      break;
    }
  }

  if (predictions_it == form_predictions.end())
    return;

  std::vector<blink::WebFormControlElement> autofillable_elements =
      form_util::ExtractAutofillableElementsFromSet(form.control_elements);
  const PasswordFormFieldPredictionMap& field_predictions =
      predictions_it->second;
  for (const auto& prediction : field_predictions) {
    const FormFieldData& target_field = prediction.first;
    const PasswordFormFieldPredictionType& type = prediction.second;
    for (const auto& control_element : autofillable_elements)  {
      if (control_element.nameForAutofill() == target_field.name) {
        const WebInputElement* input_element =
            toWebInputElement(&control_element);

        // TODO(sebsg): Investigate why this guard is necessary, see
        // https://crbug.com/517490 for more details.
        if (input_element) {
          (*predicted_elements)[*input_element] = type;
        }
        break;
      }
    }
  }
}

// TODO(crbug.com/543085): Move the reauthentication recognition code to the
// browser.
const char kPasswordSiteUrlRegex[] =
    "passwords(?:-[a-z-]+\\.corp)?\\.google\\.com";

struct PasswordSiteUrlLazyInstanceTraits
    : public base::DefaultLazyInstanceTraits<re2::RE2> {
  static re2::RE2* New(void* instance) {
    return CreateMatcher(instance, kPasswordSiteUrlRegex);
  }
};

base::LazyInstance<re2::RE2, PasswordSiteUrlLazyInstanceTraits>
    g_password_site_matcher = LAZY_INSTANCE_INITIALIZER;

// Returns the |input_field| name if its non-empty; otherwise a |dummy_name|.
base::string16 FieldName(const WebInputElement& input_field,
                         const char dummy_name[]) {
  base::string16 field_name = input_field.nameForAutofill();
  return field_name.empty() ? base::ASCIIToUTF16(dummy_name) : field_name;
}

// Helper function that checks the presence of visible password and username
// fields in |form.control_elements|.
// Iff a visible password found, then |*found_visible_password| is set to true.
// Iff a visible password found AND there is a visible username before it, then
// |*found_visible_username_before_visible_password| is set to true.
void FoundVisiblePasswordAndVisibleUsernameBeforePassword(
    const SyntheticForm& form,
    bool* found_visible_password,
    bool* found_visible_username_before_visible_password) {
  DCHECK(found_visible_password);
  DCHECK(found_visible_username_before_visible_password);
  *found_visible_password = false;
  *found_visible_username_before_visible_password = false;

  bool found_visible_username = false;
  for (auto& control_element : form.control_elements) {
    const WebInputElement* input_element = toWebInputElement(&control_element);
    if (!input_element || !input_element->isEnabled() ||
        !input_element->isTextField())
      continue;

    if (!form_util::IsWebNodeVisible(*input_element))
      continue;

    if (input_element->isPasswordField()) {
      *found_visible_password = true;
      *found_visible_username_before_visible_password = found_visible_username;
      break;
    } else {
      found_visible_username = true;
    }
  }
}

// Get information about a login form encapsulated in a PasswordForm struct.
// If an element of |form| has an entry in |nonscript_modified_values|, the
// associated string is used instead of the element's value to create
// the PasswordForm.
bool GetPasswordForm(const SyntheticForm& form,
                     PasswordForm* password_form,
                     const ModifiedValues* nonscript_modified_values,
                     const FormsPredictionsMap* form_predictions) {
  WebInputElement latest_input_element;
  WebInputElement username_element;
  password_form->username_marked_by_site = false;
  std::vector<WebInputElement> passwords;
  std::map<blink::WebInputElement, blink::WebInputElement>
      last_text_input_before_password;
  std::vector<base::string16> other_possible_usernames;

  // Bail if this is a GAIA passwords site reauthentication form, so that
  // the form will be ignored.
  // TODO(msramek): Move this logic to the browser, and disable filling only
  // for the sync credential and if passwords are being synced.
  if (IsGaiaReauthenticationForm(GURL(form.document.url()).GetOrigin(),
                                 form.control_elements)) {
    return false;
  }

  std::map<WebInputElement, PasswordFormFieldPredictionType> predicted_elements;
  if (form_predictions) {
    FindPredictedElements(form, password_form->form_data, *form_predictions,
                          &predicted_elements);
  }

  // Check the presence of visible password and username fields.
  // If there is a visible password field, then ignore invisible password
  // fields. If there is a visible username before visible password, then ignore
  // invisible username fields.
  // If there is no visible password field, don't ignore any elements (i.e. use
  // the latest username field just before selected password field).
  bool ignore_invisible_passwords = false;
  bool ignore_invisible_usernames = false;
  FoundVisiblePasswordAndVisibleUsernameBeforePassword(
      form, &ignore_invisible_passwords, &ignore_invisible_usernames);
  std::string layout_sequence;
  layout_sequence.reserve(form.control_elements.size());
  size_t number_of_non_empty_text_non_password_fields = 0;
  for (size_t i = 0; i < form.control_elements.size(); ++i) {
    WebFormControlElement control_element = form.control_elements[i];

    WebInputElement* input_element = toWebInputElement(&control_element);
    if (!input_element || !input_element->isEnabled())
      continue;

    bool element_is_invisible = !form_util::IsWebNodeVisible(*input_element);
    if (input_element->isTextField()) {
      if (input_element->isPasswordField()) {
        if (element_is_invisible && ignore_invisible_passwords)
          continue;
        layout_sequence.push_back('P');
      } else {
        if (nonscript_modified_values &&
            nonscript_modified_values->find(*input_element) !=
                nonscript_modified_values->end())
          ++number_of_non_empty_text_non_password_fields;
        if (element_is_invisible && ignore_invisible_usernames)
          continue;
        layout_sequence.push_back('N');
      }
    }

    bool password_marked_by_autocomplete_attribute =
        HasAutocompleteAttributeValue(*input_element,
                                      kAutocompleteCurrentPassword) ||
        HasAutocompleteAttributeValue(*input_element, kAutocompleteNewPassword);

    // If the password field is readonly, the page is likely using a virtual
    // keyboard and bypassing the password field value (see
    // http://crbug.com/475488). There is nothing Chrome can do to fill
    // passwords for now. Continue processing in case when the password field
    // was made readonly by JavaScript before submission. We can do this by
    // checking whether password element was updated not from JavaScript.
    if (input_element->isPasswordField() &&
        (!input_element->isReadOnly() ||
         (nonscript_modified_values &&
          nonscript_modified_values->find(*input_element) !=
              nonscript_modified_values->end()) ||
         password_marked_by_autocomplete_attribute)) {

      // We add the field to the list of password fields if it was not flagged
      // as a special NOT_PASSWORD prediction by Autofill. The NOT_PASSWORD
      // mechanism exists because some webpages use the type "password" for
      // fields which Autofill knows shouldn't be treated as passwords by the
      // Password Manager. This is ultimately bypassed if the field has
      // autocomplete attributes.
      auto possible_password_element_iterator =
          predicted_elements.find(*input_element);
      if (password_marked_by_autocomplete_attribute ||
          possible_password_element_iterator == predicted_elements.end() ||
          possible_password_element_iterator->second !=
              PREDICTION_NOT_PASSWORD) {
        passwords.push_back(*input_element);
        last_text_input_before_password[*input_element] = latest_input_element;
      }
    }

    // Various input types such as text, url, email can be a username field.
    if (input_element->isTextField() && !input_element->isPasswordField()) {
      if (HasAutocompleteAttributeValue(*input_element,
                                        kAutocompleteUsername)) {
        if (password_form->username_marked_by_site) {
          // A second or subsequent element marked with autocomplete='username'.
          // This makes us less confident that we have understood the form. We
          // will stick to our choice that the first such element was the real
          // username, but will start collecting other_possible_usernames from
          // the extra elements marked with autocomplete='username'. Note that
          // unlike username_element, other_possible_usernames is used only for
          // autofill, not for form identification, and blank autofill entries
          // are not useful, so we do not collect empty strings.
          if (!input_element->value().isEmpty())
            other_possible_usernames.push_back(input_element->value());
        } else {
          // The first element marked with autocomplete='username'. Take the
          // hint and treat it as the username (overruling the tentative choice
          // we might have made before). Furthermore, drop all other possible
          // usernames we have accrued so far: they come from fields not marked
          // with the autocomplete attribute, making them unlikely alternatives.
          username_element = *input_element;
          password_form->username_marked_by_site = true;
          other_possible_usernames.clear();
        }
      } else {
        if (password_form->username_marked_by_site) {
          // Having seen elements with autocomplete='username', elements without
          // this attribute are no longer interesting. No-op.
        } else {
          // No elements marked with autocomplete='username' so far whatsoever.
          // If we have not yet selected a username element even provisionally,
          // then remember this element for the case when the next field turns
          // out to be a password. Save a non-empty username as a possible
          // alternative, at least for now.
          if (username_element.isNull())
            latest_input_element = *input_element;
          if (!input_element->value().isEmpty())
            other_possible_usernames.push_back(input_element->value());
        }
      }
    }
  }

  WebInputElement password;
  WebInputElement new_password;
  if (!LocateSpecificPasswords(passwords, &password, &new_password))
    return false;

  DCHECK_EQ(passwords.size(), last_text_input_before_password.size());
  if (username_element.isNull()) {
    if (!password.isNull())
      username_element = last_text_input_before_password[password];
    if (username_element.isNull() && !new_password.isNull())
      username_element = last_text_input_before_password[new_password];
    if (!username_element.isNull())
      ExcludeUsernameFromOtherUsernamesList(username_element,
                                            &other_possible_usernames);
  }

  password_form->layout = SequenceToLayout(layout_sequence);

  WebInputElement predicted_username_element;
  bool map_has_username_prediction = MapContainsPrediction(
      predicted_elements, PREDICTION_USERNAME, &predicted_username_element);

  // Let server predictions override the selection of the username field. This
  // allows instant adjusting without changing Chromium code.
  auto username_element_iterator = predicted_elements.find(username_element);
  if (map_has_username_prediction &&
      (username_element_iterator == predicted_elements.end() ||
       username_element_iterator->second != PREDICTION_USERNAME)) {
    ExcludeUsernameFromOtherUsernamesList(predicted_username_element,
                                          &other_possible_usernames);
    if (!username_element.isNull()) {
      other_possible_usernames.push_back(username_element.value());
    }
    username_element = predicted_username_element;
    password_form->was_parsed_using_autofill_predictions = true;
  }

  if (!username_element.isNull()) {
    password_form->username_element =
        FieldName(username_element, "anonymous_username");
    base::string16 username_value = username_element.value();
    if (nonscript_modified_values != nullptr) {
      auto username_iterator =
        nonscript_modified_values->find(username_element);
      if (username_iterator != nonscript_modified_values->end()) {
        base::string16 typed_username_value = username_iterator->second;
        if (!base::StartsWith(
                base::i18n::ToLower(username_value),
                base::i18n::ToLower(typed_username_value),
                base::CompareCase::SENSITIVE)) {
          // We check that |username_value| was not obtained by autofilling
          // |typed_username_value|. In case when it was, |typed_username_value|
          // is incomplete, so we should leave autofilled value.
          username_value = typed_username_value;
        }
      }
    }
    password_form->username_value = username_value;
  }

  password_form->origin =
      form_util::GetCanonicalOriginForDocument(form.document);
  GURL::Replacements rep;
  rep.SetPathStr("");
  password_form->signon_realm =
      password_form->origin.ReplaceComponents(rep).spec();
  password_form->other_possible_usernames.swap(other_possible_usernames);

  if (!password.isNull()) {
    password_form->password_element = FieldName(password, "anonymous_password");
    blink::WebString password_value = password.value();
    if (nonscript_modified_values != nullptr) {
      auto password_iterator = nonscript_modified_values->find(password);
      if (password_iterator != nonscript_modified_values->end())
        password_value = password_iterator->second;
    }
    password_form->password_value = password_value;
  }
  if (!new_password.isNull()) {
    password_form->new_password_element =
        FieldName(new_password, "anonymous_new_password");
    password_form->new_password_value = new_password.value();
    password_form->new_password_value_is_default =
        new_password.getAttribute("value") == new_password.value();
    if (HasAutocompleteAttributeValue(new_password, kAutocompleteNewPassword))
      password_form->new_password_marked_by_site = true;
  }

  if (username_element.isNull()) {
    // To get a better idea on how password forms without a username field
    // look like, report the total number of text and password fields.
    UMA_HISTOGRAM_COUNTS_100(
        "PasswordManager.EmptyUsernames.TextAndPasswordFieldCount",
        layout_sequence.size());
    // For comparison, also report the number of password fields.
    UMA_HISTOGRAM_COUNTS_100(
        "PasswordManager.EmptyUsernames.PasswordFieldCount",
        std::count(layout_sequence.begin(), layout_sequence.end(), 'P'));
  }

  password_form->scheme = PasswordForm::SCHEME_HTML;
  password_form->ssl_valid = false;
  password_form->preferred = false;
  password_form->blacklisted_by_user = false;
  password_form->type = PasswordForm::TYPE_MANUAL;

  // The password form is considered that it looks like SignUp form if it has
  // more than 1 text field with user input or it has a new password field and
  // no current password field.
  password_form->does_look_like_signup_form =
      number_of_non_empty_text_non_password_fields > 1 ||
      (number_of_non_empty_text_non_password_fields == 1 &&
       password_form->password_element.empty() &&
       !password_form->new_password_element.empty());
  return true;
}

}  // namespace

bool IsGaiaReauthenticationForm(
    const GURL& origin,
    const std::vector<blink::WebFormControlElement>& control_elements) {
  if (origin != GaiaUrls::GetInstance()->gaia_url().GetOrigin())
    return false;

  bool has_rart_field = false;
  bool has_continue_field = false;

  for (const blink::WebFormControlElement& element : control_elements) {
    // We're only interested in the presence
    // of <input type="hidden" /> elements.
    CR_DEFINE_STATIC_LOCAL(WebString, kHidden, ("hidden"));
    const blink::WebInputElement* input = blink::toWebInputElement(&element);
    if (!input || input->formControlType() != kHidden)
      continue;

    // There must be a hidden input named "rart".
    if (input->formControlName() == "rart")
      has_rart_field = true;

    // There must be a hidden input named "continue", whose value points
    // to a password (or password testing) site.
    if (input->formControlName() == "continue" &&
        re2::RE2::PartialMatch(input->value().utf8(),
                               g_password_site_matcher.Get())) {
      has_continue_field = true;
    }
  }

  return has_rart_field && has_continue_field;
}

std::unique_ptr<PasswordForm> CreatePasswordFormFromWebForm(
    const WebFormElement& web_form,
    const ModifiedValues* nonscript_modified_values,
    const FormsPredictionsMap* form_predictions) {
  if (web_form.isNull())
    return std::unique_ptr<PasswordForm>();

  std::unique_ptr<PasswordForm> password_form(new PasswordForm());
  password_form->action = form_util::GetCanonicalActionForForm(web_form);
  if (!password_form->action.is_valid())
    return std::unique_ptr<PasswordForm>();

  SyntheticForm synthetic_form;
  PopulateSyntheticFormFromWebForm(web_form, &synthetic_form);

  WebFormElementToFormData(web_form, blink::WebFormControlElement(),
                           form_util::EXTRACT_NONE, &password_form->form_data,
                           NULL /* FormFieldData */);

  if (!GetPasswordForm(synthetic_form, password_form.get(),
                       nonscript_modified_values, form_predictions))
    return std::unique_ptr<PasswordForm>();

  return password_form;
}

std::unique_ptr<PasswordForm> CreatePasswordFormFromUnownedInputElements(
    const WebFrame& frame,
    const ModifiedValues* nonscript_modified_values,
    const FormsPredictionsMap* form_predictions) {
  SyntheticForm synthetic_form;
  synthetic_form.control_elements = form_util::GetUnownedFormFieldElements(
      frame.document().all(), &synthetic_form.fieldsets);
  synthetic_form.document = frame.document();

  if (synthetic_form.control_elements.empty())
    return std::unique_ptr<PasswordForm>();

  std::unique_ptr<PasswordForm> password_form(new PasswordForm());
  UnownedPasswordFormElementsAndFieldSetsToFormData(
      synthetic_form.fieldsets, synthetic_form.control_elements, nullptr,
      frame.document(), form_util::EXTRACT_NONE, &password_form->form_data,
      nullptr /* FormFieldData */);
  if (!GetPasswordForm(synthetic_form, password_form.get(),
                       nonscript_modified_values, form_predictions))
    return std::unique_ptr<PasswordForm>();

  // No actual action on the form, so use the the origin as the action.
  password_form->action = password_form->origin;

  return password_form;
}

bool HasAutocompleteAttributeValue(const blink::WebInputElement& element,
                                   const char* value_in_lowercase) {
  return base::LowerCaseEqualsASCII(
      base::StringPiece16(element.getAttribute("autocomplete")),
      value_in_lowercase);
}

}  // namespace autofill
