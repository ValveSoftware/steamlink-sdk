// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/password_form_conversion_utils.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/password_form.h"
#include "content/public/test/render_view_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFormControlElement.h"
#include "third_party/WebKit/public/web/WebFormElement.h"
#include "third_party/WebKit/public/web/WebInputElement.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

using blink::WebFormControlElement;
using blink::WebFormElement;
using blink::WebFrame;
using blink::WebInputElement;
using blink::WebVector;

namespace autofill {
namespace {

const char kTestFormActionURL[] = "http://cnn.com";

// A builder to produce HTML code for a password form composed of the desired
// number and kinds of username and password fields.
class PasswordFormBuilder {
 public:
  // Creates a builder to start composing a new form. The form will have the
  // specified |action| URL.
  explicit PasswordFormBuilder(const char* action) {
    base::StringAppendF(
        &html_, "<FORM name=\"Test\" action=\"%s\" method=\"post\">", action);
  }

  // Appends a new text-type field at the end of the form, having the specified
  // |name_and_id|, |value|, and |autocomplete| attributes. The |autocomplete|
  // argument can take two special values, namely:
  //  1.) nullptr, causing no autocomplete attribute to be added,
  //  2.) "", causing an empty attribute (i.e. autocomplete="") to be added.
  void AddTextField(const char* name_and_id,
                    const char* value,
                    const char* autocomplete) {
    std::string autocomplete_attribute(autocomplete ?
        base::StringPrintf("autocomplete=\"%s\"", autocomplete) : "");
    base::StringAppendF(
        &html_,
        "<INPUT type=\"text\" name=\"%s\" id=\"%s\" value=\"%s\" %s/>",
        name_and_id, name_and_id, value, autocomplete_attribute.c_str());
  }

  // Appends a new password-type field at the end of the form, having the
  // specified |name_and_id|, |value|, and |autocomplete| attributes. Special
  // values for |autocomplete| are the same as in AddTextField.
  void AddPasswordField(const char* name_and_id,
                        const char* value,
                        const char* autocomplete) {
    std::string autocomplete_attribute(autocomplete ?
        base::StringPrintf("autocomplete=\"%s\"", autocomplete): "");
    base::StringAppendF(
        &html_,
        "<INPUT type=\"password\" name=\"%s\" id=\"%s\" value=\"%s\" %s/>",
        name_and_id, name_and_id, value, autocomplete_attribute.c_str());
  }

  // Appends a disabled text-type field at the end of the form.
  void AddDisabledUsernameField() {
    html_ += "<INPUT type=\"text\" disabled/>";
  }

  // Appends a disabled password-type field at the end of the form.
  void AddDisabledPasswordField() {
    html_ += "<INPUT type=\"password\" disabled/>";
  }

  // Appends a hidden field at the end of the form.
  void AddHiddenField() { html_ += "<INPUT type=\"hidden\"/>"; }

  // Appends a new hidden-type field at the end of the form, having the
  // specified |name_and_id| and |value| attributes.
  void AddHiddenField(const char* name_and_id,
                      const char* value) {
    base::StringAppendF(
        &html_,
        "<INPUT type=\"hidden\" name=\"%s\" id=\"%s\" value=\"%s\" />",
        name_and_id, name_and_id, value);
  }

  // Append a text field with "display: none".
  void AddNonDisplayedTextField(const char* name_and_id,
                                const char* value) {
    // TODO(crbug.com/570628): Add tests with style="visibility: hidden;" too
    // when IsWebNodeVisible in form_autofill_util.cc has changed according to
    // esprehn's TODO in the function. Now tests with visibility attribute fail.
    base::StringAppendF(
            &html_,
            "<INPUT type=\"text\" name=\"%s\" id=\"%s\" value=\"%s\""
             "style=\"display: none;\"/>",
            name_and_id, name_and_id, value);
  }

  // Append a password field with "display: none".
  void AddNonDisplayedPasswordField(const char* name_and_id,
                                    const char* value) {
    base::StringAppendF(
            &html_,
            "<INPUT type=\"password\" name=\"%s\" id=\"%s\" value=\"%s\""
            "style=\"display: none;\"/>",
            name_and_id, name_and_id, value);
  }

  // Appends a new submit-type field at the end of the form with the specified
  // |name|.
  void AddSubmitButton(const char* name) {
    base::StringAppendF(
        &html_,
        "<INPUT type=\"submit\" name=\"%s\" value=\"Submit\"/>",
        name);
  }

  // Returns the HTML code for the form containing the fields that have been
  // added so far.
  std::string ProduceHTML() const {
    return html_ + "</FORM>";
  }

  // Appends a field of |type| without name or id attribute at the end of the
  // form.
  void AddAnonymousInputField(const char* type) {
    std::string type_attribute(type ? base::StringPrintf("type=\"%s\"", type)
                                    : "");
    base::StringAppendF(&html_, "<INPUT %s/>", type_attribute.c_str());
  }

 private:
  std::string html_;

  DISALLOW_COPY_AND_ASSIGN(PasswordFormBuilder);
};

// RenderViewTest-based tests crash on Android
// http://crbug.com/187500
#if defined(OS_ANDROID)
#define MAYBE_PasswordFormConversionUtilsTest \
  DISABLED_PasswordFormConversionUtilsTest
#else
#define MAYBE_PasswordFormConversionUtilsTest PasswordFormConversionUtilsTest
#endif  // defined(OS_ANDROID)

class MAYBE_PasswordFormConversionUtilsTest : public content::RenderViewTest {
 public:
  MAYBE_PasswordFormConversionUtilsTest() : content::RenderViewTest() {}
  ~MAYBE_PasswordFormConversionUtilsTest() override {}

 protected:
  // Loads the given |html|, retrieves the sole WebFormElement from it, and then
  // calls CreatePasswordForm(), passing it the |predictions| to convert it to
  // a PasswordForm. If |with_user_input| == true it's considered that all
  // values in the form elements came from the user input.
  std::unique_ptr<PasswordForm> LoadHTMLAndConvertForm(
      const std::string& html,
      FormsPredictionsMap* predictions,
      bool with_user_input) {
    WebFormElement form;
    LoadWebFormFromHTML(html, &form);

    WebVector<WebFormControlElement> control_elements;
    form.getFormControlElements(control_elements);
    ModifiedValues user_input;
    for (size_t i = 0; i < control_elements.size(); ++i) {
      WebInputElement* input_element = toWebInputElement(&control_elements[i]);
      if (input_element->hasAttribute("set-activated-submit"))
        input_element->setActivatedSubmit(true);
      if (with_user_input)
        user_input[*input_element] = input_element->value();
    }

    return CreatePasswordFormFromWebForm(
        form, with_user_input ? &user_input : nullptr, predictions);
  }

  // Iterates on the form generated by the |html| and adds the fields and type
  // predictions corresponding to |predictions_positions| to |predictions|.
  void SetPredictions(const std::string& html,
                      FormsPredictionsMap* predictions,
                      const std::map<int, PasswordFormFieldPredictionType>&
                          predictions_positions) {
    WebFormElement form;
    LoadWebFormFromHTML(html, &form);

    FormData form_data;
    ASSERT_TRUE(form_util::WebFormElementToFormData(
        form, WebFormControlElement(), form_util::EXTRACT_NONE, &form_data,
        nullptr));

    FormStructure form_structure(form_data);

    int field_index = 0;
    for (std::vector<AutofillField *>::const_iterator
             field = form_structure.begin();
         field != form_structure.end(); ++field, ++field_index) {
      if (predictions_positions.find(field_index) !=
          predictions_positions.end()) {
        (*predictions)[form_data][*(*field)] =
            predictions_positions.find(field_index)->second;
      }
    }
  }

  // Loads the given |html| and retrieves the sole WebFormElement from it.
  void LoadWebFormFromHTML(const std::string& html, WebFormElement* form) {
    LoadHTML(html.c_str());

    WebFrame* frame = GetMainFrame();
    ASSERT_NE(nullptr, frame);

    WebVector<WebFormElement> forms;
    frame->document().forms(forms);
    ASSERT_EQ(1U, forms.size());

    *form = forms[0];
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MAYBE_PasswordFormConversionUtilsTest);
};

}  // namespace

TEST_F(MAYBE_PasswordFormConversionUtilsTest, BasicFormAttributes) {
  PasswordFormBuilder builder(kTestFormActionURL);
  builder.AddTextField("username", "johnsmith", nullptr);
  builder.AddSubmitButton("inactive_submit");
  builder.AddSubmitButton("active_submit");
  builder.AddSubmitButton("inactive_submit2");
  builder.AddPasswordField("password", "secret", nullptr);
  std::string html = builder.ProduceHTML();

  std::unique_ptr<PasswordForm> password_form =
      LoadHTMLAndConvertForm(html, nullptr, false);
  ASSERT_TRUE(password_form);

  EXPECT_EQ("data:", password_form->signon_realm);
  EXPECT_EQ(GURL(kTestFormActionURL), password_form->action);
  EXPECT_EQ(base::UTF8ToUTF16("username"), password_form->username_element);
  EXPECT_EQ(base::UTF8ToUTF16("johnsmith"), password_form->username_value);
  EXPECT_EQ(base::UTF8ToUTF16("password"), password_form->password_element);
  EXPECT_EQ(base::UTF8ToUTF16("secret"), password_form->password_value);
  EXPECT_EQ(PasswordForm::SCHEME_HTML, password_form->scheme);
  EXPECT_FALSE(password_form->ssl_valid);
  EXPECT_FALSE(password_form->preferred);
  EXPECT_FALSE(password_form->blacklisted_by_user);
  EXPECT_EQ(PasswordForm::TYPE_MANUAL, password_form->type);
}

TEST_F(MAYBE_PasswordFormConversionUtilsTest, DisabledFieldsAreIgnored) {
  PasswordFormBuilder builder(kTestFormActionURL);
  builder.AddTextField("username", "johnsmith", nullptr);
  builder.AddDisabledUsernameField();
  builder.AddDisabledPasswordField();
  builder.AddPasswordField("password", "secret", nullptr);
  builder.AddSubmitButton("submit");
  std::string html = builder.ProduceHTML();

  std::unique_ptr<PasswordForm> password_form =
      LoadHTMLAndConvertForm(html, nullptr, false);
  ASSERT_TRUE(password_form);
  EXPECT_EQ(base::UTF8ToUTF16("username"), password_form->username_element);
  EXPECT_EQ(base::UTF8ToUTF16("johnsmith"), password_form->username_value);
  EXPECT_EQ(base::UTF8ToUTF16("password"), password_form->password_element);
  EXPECT_EQ(base::UTF8ToUTF16("secret"), password_form->password_value);
}

TEST_F(MAYBE_PasswordFormConversionUtilsTest, IdentifyingUsernameFields) {
  // Each test case consists of a set of parameters to be plugged into the
  // PasswordFormBuilder below, plus the corresponding expectations.
  struct TestCase {
    const char* autocomplete[3];
    const char* expected_username_element;
    const char* expected_username_value;
    const char* expected_other_possible_usernames;
  } cases[] = {
      // When no elements are marked with autocomplete='username', the text-type
      // input field before the first password element should get selected as
      // the username, and the rest should be marked as alternatives.
      {{nullptr, nullptr, nullptr}, "username2", "William", "John+Smith"},
      // When a sole element is marked with autocomplete='username', it should
      // be treated as the username for sure, with no other_possible_usernames.
      {{"username", nullptr, nullptr}, "username1", "John", ""},
      {{nullptr, "username", nullptr}, "username2", "William", ""},
      {{nullptr, nullptr, "username"}, "username3", "Smith", ""},
      // When >=2 elements have the attribute, the first should be selected as
      // the username, and the rest should go to other_possible_usernames.
      {{"username", "username", nullptr}, "username1", "John", "William"},
      {{nullptr, "username", "username"}, "username2", "William", "Smith"},
      {{"username", nullptr, "username"}, "username1", "John", "Smith"},
      {{"username", "username", "username"},
       "username1",
       "John",
       "William+Smith"},
      // When there is an empty autocomplete attribute (i.e. autocomplete=""),
      // it should have the same effect as having no attribute whatsoever.
      {{"", "", ""}, "username2", "William", "John+Smith"},
      {{"", "", "username"}, "username3", "Smith", ""},
      {{"username", "", "username"}, "username1", "John", "Smith"},
      // It should not matter if attribute values are upper or mixed case.
      {{"USERNAME", nullptr, "uSeRNaMe"}, "username1", "John", "Smith"},
      {{"uSeRNaMe", nullptr, "USERNAME"}, "username1", "John", "Smith"}};

  for (size_t i = 0; i < arraysize(cases); ++i) {
    for (size_t nonempty_username_fields = 0; nonempty_username_fields < 2;
         ++nonempty_username_fields) {
      SCOPED_TRACE(testing::Message()
                   << "Iteration " << i << " "
                   << (nonempty_username_fields ? "nonempty" : "empty"));

      // Repeat each test once with empty, and once with non-empty usernames.
      // In the former case, no empty other_possible_usernames should be saved.
      const char* names[3];
      if (nonempty_username_fields) {
        names[0] = "John";
        names[1] = "William";
        names[2] = "Smith";
      } else {
        names[0] = names[1] = names[2] = "";
      }

      PasswordFormBuilder builder(kTestFormActionURL);
      builder.AddTextField("username1", names[0], cases[i].autocomplete[0]);
      builder.AddTextField("username2", names[1], cases[i].autocomplete[1]);
      builder.AddPasswordField("password", "secret", nullptr);
      builder.AddTextField("username3", names[2], cases[i].autocomplete[2]);
      builder.AddPasswordField("password2", "othersecret", nullptr);
      builder.AddSubmitButton("submit");
      std::string html = builder.ProduceHTML();

      std::unique_ptr<PasswordForm> password_form =
          LoadHTMLAndConvertForm(html, nullptr, false);
      ASSERT_TRUE(password_form);

      EXPECT_EQ(base::UTF8ToUTF16(cases[i].expected_username_element),
                password_form->username_element);

      if (nonempty_username_fields) {
        EXPECT_EQ(base::UTF8ToUTF16(cases[i].expected_username_value),
                  password_form->username_value);
        EXPECT_EQ(base::UTF8ToUTF16(cases[i].expected_other_possible_usernames),
                  base::JoinString(password_form->other_possible_usernames,
                                   base::ASCIIToUTF16("+")));
      } else {
        EXPECT_TRUE(password_form->username_value.empty());
        EXPECT_TRUE(password_form->other_possible_usernames.empty());
      }

      // Do a basic sanity check that we are still having a password field.
      EXPECT_EQ(base::UTF8ToUTF16("password"), password_form->password_element);
      EXPECT_EQ(base::UTF8ToUTF16("secret"), password_form->password_value);
    }
  }
}

TEST_F(MAYBE_PasswordFormConversionUtilsTest, IdentifyingTwoPasswordFields) {
  // Each test case consists of a set of parameters to be plugged into the
  // PasswordFormBuilder below, plus the corresponding expectations.
  struct TestCase {
    const char* password_values[2];
    const char* expected_password_element;
    const char* expected_password_value;
    const char* expected_new_password_element;
    const char* expected_new_password_value;
  } cases[] = {
      // Two non-empty fields with the same value should be treated as a new
      // password field plus a confirmation field for the new password.
      {{"alpha", "alpha"}, "", "", "password1", "alpha"},
      // The same goes if the fields are yet empty: we speculate that we will
      // identify them as new password fields once they are filled out, and we
      // want to keep our abstract interpretation of the form less flaky.
      {{"", ""}, "password1", "", "password2", ""},
      // Two different values should be treated as a password change form, one
      // that also asks for the current password, but only once for the new.
      {{"alpha", ""}, "password1", "alpha", "password2", ""},
      {{"", "beta"}, "password1", "", "password2", "beta"},
      {{"alpha", "beta"}, "password1", "alpha", "password2", "beta"}};

  for (size_t i = 0; i < arraysize(cases); ++i) {
    SCOPED_TRACE(testing::Message() << "Iteration " << i);

    PasswordFormBuilder builder(kTestFormActionURL);
    builder.AddTextField("username1", "William", nullptr);
    builder.AddPasswordField("password1", cases[i].password_values[0], nullptr);
    builder.AddTextField("username2", "Smith", nullptr);
    builder.AddPasswordField("password2", cases[i].password_values[1], nullptr);
    builder.AddSubmitButton("submit");
    std::string html = builder.ProduceHTML();

    std::unique_ptr<PasswordForm> password_form =
        LoadHTMLAndConvertForm(html, nullptr, false);
    ASSERT_TRUE(password_form);

    EXPECT_EQ(base::UTF8ToUTF16(cases[i].expected_password_element),
              password_form->password_element);
    EXPECT_EQ(base::UTF8ToUTF16(cases[i].expected_password_value),
              password_form->password_value);
    EXPECT_EQ(base::UTF8ToUTF16(cases[i].expected_new_password_element),
              password_form->new_password_element);
    EXPECT_EQ(base::UTF8ToUTF16(cases[i].expected_new_password_value),
              password_form->new_password_value);

    // Do a basic sanity check that we are still selecting the right username.
    EXPECT_EQ(base::UTF8ToUTF16("username1"), password_form->username_element);
    EXPECT_EQ(base::UTF8ToUTF16("William"), password_form->username_value);
    EXPECT_THAT(password_form->other_possible_usernames,
                testing::ElementsAre(base::UTF8ToUTF16("Smith")));
  }
}

TEST_F(MAYBE_PasswordFormConversionUtilsTest, IdentifyingThreePasswordFields) {
  // Each test case consists of a set of parameters to be plugged into the
  // PasswordFormBuilder below, plus the corresponding expectations.
  struct TestCase {
    const char* password_values[3];
    const char* expected_password_element;
    const char* expected_password_value;
    const char* expected_new_password_element;
    const char* expected_new_password_value;
  } cases[] = {
      // Two fields with the same value, and one different: we should treat this
      // as a password change form with confirmation for the new password. Note
      // that we only recognize (current + new + new) and (new + new + current)
      // without autocomplete attributes.
      {{"alpha", "", ""}, "password1", "alpha", "password2", ""},
      {{"", "beta", "beta"}, "password1", "", "password2", "beta"},
      {{"alpha", "beta", "beta"}, "password1", "alpha", "password2", "beta"},
      // If confirmed password comes first, assume that the third password
      // field is related to security question, SSN, or credit card and ignore
      // it.
      {{"beta", "beta", "alpha"}, "", "", "password1", "beta"},
      // If the fields are yet empty, we speculate that we will identify them as
      // (current + new + new) once they are filled out, so we should classify
      // them the same for now to keep our abstract interpretation less flaky.
      {{"", "", ""}, "password1", "", "password2", ""}};
      // Note: In all other cases, we give up and consider the form invalid.
      // This is tested in InvalidFormDueToConfusingPasswordFields.

  for (size_t i = 0; i < arraysize(cases); ++i) {
    SCOPED_TRACE(testing::Message() << "Iteration " << i);

    PasswordFormBuilder builder(kTestFormActionURL);
    builder.AddTextField("username1", "William", nullptr);
    builder.AddPasswordField("password1", cases[i].password_values[0], nullptr);
    builder.AddPasswordField("password2", cases[i].password_values[1], nullptr);
    builder.AddTextField("username2", "Smith", nullptr);
    builder.AddPasswordField("password3", cases[i].password_values[2], nullptr);
    builder.AddSubmitButton("submit");
    std::string html = builder.ProduceHTML();

    std::unique_ptr<PasswordForm> password_form =
        LoadHTMLAndConvertForm(html, nullptr, false);
    ASSERT_TRUE(password_form);

    EXPECT_EQ(base::UTF8ToUTF16(cases[i].expected_password_element),
              password_form->password_element);
    EXPECT_EQ(base::UTF8ToUTF16(cases[i].expected_password_value),
              password_form->password_value);
    EXPECT_EQ(base::UTF8ToUTF16(cases[i].expected_new_password_element),
              password_form->new_password_element);
    EXPECT_EQ(base::UTF8ToUTF16(cases[i].expected_new_password_value),
              password_form->new_password_value);

    // Do a basic sanity check that we are still selecting the right username.
    EXPECT_EQ(base::UTF8ToUTF16("username1"), password_form->username_element);
    EXPECT_EQ(base::UTF8ToUTF16("William"), password_form->username_value);
    EXPECT_THAT(password_form->other_possible_usernames,
                testing::ElementsAre(base::UTF8ToUTF16("Smith")));
  }
}

TEST_F(MAYBE_PasswordFormConversionUtilsTest,
       IdentifyingPasswordFieldsWithAutocompleteAttributes) {
  // Each test case consists of a set of parameters to be plugged into the
  // PasswordFormBuilder below, plus the corresponding expectations.
  struct TestCase {
    const char* autocomplete[3];
    const char* expected_password_element;
    const char* expected_password_value;
    const char* expected_new_password_element;
    const char* expected_new_password_value;
    bool expected_new_password_marked_by_site;
    const char* expected_username_element;
    const char* expected_username_value;
  } cases[] = {
      // When there are elements marked with autocomplete='current-password',
      // but no elements with 'new-password', we should treat the first of the
      // former kind as the current password, and ignore all other password
      // fields, assuming they are not intentionally not marked. They might be
      // for other purposes, such as PINs, OTPs, and the like. Actual values in
      // the password fields should be ignored in all cases below.
      // Username is the element just before the first 'current-password' (even
      // if 'new-password' comes earlier). If no 'current-password', then the
      // element just before the first 'new-passwords'.
      {{"current-password", nullptr, nullptr},
       "password1",
       "alpha",
       "",
       "",
       false,
       "username1",
       "William"},
      {{nullptr, "current-password", nullptr},
       "password2",
       "beta",
       "",
       "",
       false,
       "username2",
       "Smith"},
      {{nullptr, nullptr, "current-password"},
       "password3",
       "gamma",
       "",
       "",
       false,
       "username2",
       "Smith"},
      {{nullptr, "current-password", "current-password"},
       "password2",
       "beta",
       "",
       "",
       false,
       "username2",
       "Smith"},
      {{"current-password", nullptr, "current-password"},
       "password1",
       "alpha",
       "",
       "",
       false,
       "username1",
       "William"},
      {{"current-password", "current-password", nullptr},
       "password1",
       "alpha",
       "",
       "",
       false,
       "username1",
       "William"},
      {{"current-password", "current-password", "current-password"},
       "password1",
       "alpha",
       "",
       "",
       false,
       "username1",
       "William"},
      // The same goes vice versa for autocomplete='new-password'.
      {{"new-password", nullptr, nullptr},
       "",
       "",
       "password1",
       "alpha",
       true,
       "username1",
       "William"},
      {{nullptr, "new-password", nullptr},
       "",
       "",
       "password2",
       "beta",
       true,
       "username2",
       "Smith"},
      {{nullptr, nullptr, "new-password"},
       "",
       "",
       "password3",
       "gamma",
       true,
       "username2",
       "Smith"},
      {{nullptr, "new-password", "new-password"},
       "",
       "",
       "password2",
       "beta",
       true,
       "username2",
       "Smith"},
      {{"new-password", nullptr, "new-password"},
       "",
       "",
       "password1",
       "alpha",
       true,
       "username1",
       "William"},
      {{"new-password", "new-password", nullptr},
       "",
       "",
       "password1",
       "alpha",
       true,
       "username1",
       "William"},
      {{"new-password", "new-password", "new-password"},
       "",
       "",
       "password1",
       "alpha",
       true,
       "username1",
       "William"},
      // When there is one element marked with autocomplete='current-password',
      // and one with 'new-password', just comply. Ignore the unmarked password
      // field(s) for the same reason as above.
      {{"current-password", "new-password", nullptr},
       "password1",
       "alpha",
       "password2",
       "beta",
       true,
       "username1",
       "William"},
      {{"current-password", nullptr, "new-password"},
       "password1",
       "alpha",
       "password3",
       "gamma",
       true,
       "username1",
       "William"},
      {{nullptr, "current-password", "new-password"},
       "password2",
       "beta",
       "password3",
       "gamma",
       true,
       "username2",
       "Smith"},
      {{"new-password", "current-password", nullptr},
       "password2",
       "beta",
       "password1",
       "alpha",
       true,
       "username2",
       "Smith"},
      {{"new-password", nullptr, "current-password"},
       "password3",
       "gamma",
       "password1",
       "alpha",
       true,
       "username2",
       "Smith"},
      {{nullptr, "new-password", "current-password"},
       "password3",
       "gamma",
       "password2",
       "beta",
       true,
       "username2",
       "Smith"},
      // In case of duplicated elements of either kind, go with the first one of
      // its kind.
      {{"current-password", "current-password", "new-password"},
       "password1",
       "alpha",
       "password3",
       "gamma",
       true,
       "username1",
       "William"},
      {{"current-password", "new-password", "current-password"},
       "password1",
       "alpha",
       "password2",
       "beta",
       true,
       "username1",
       "William"},
      {{"new-password", "current-password", "current-password"},
       "password2",
       "beta",
       "password1",
       "alpha",
       true,
       "username2",
       "Smith"},
      {{"current-password", "new-password", "new-password"},
       "password1",
       "alpha",
       "password2",
       "beta",
       true,
       "username1",
       "William"},
      {{"new-password", "current-password", "new-password"},
       "password2",
       "beta",
       "password1",
       "alpha",
       true,
       "username2",
       "Smith"},
      {{"new-password", "new-password", "current-password"},
       "password3",
       "gamma",
       "password1",
       "alpha",
       true,
       "username2",
       "Smith"},
      // When there is an empty autocomplete attribute (i.e. autocomplete=""),
      // it should have the same effect as having no attribute whatsoever.
      {{"current-password", "", ""},
       "password1",
       "alpha",
       "",
       "",
       false,
       "username1",
       "William"},
      {{"", "", "new-password"},
       "",
       "",
       "password3",
       "gamma",
       true,
       "username2",
       "Smith"},
      {{"", "new-password", ""},
       "",
       "",
       "password2",
       "beta",
       true,
       "username2",
       "Smith"},
      {{"", "current-password", "current-password"},
       "password2",
       "beta",
       "",
       "",
       false,
       "username2",
       "Smith"},
      {{"new-password", "", "new-password"},
       "",
       "",
       "password1",
       "alpha",
       true,
       "username1",
       "William"},
      {{"new-password", "", "current-password"},
       "password3",
       "gamma",
       "password1",
       "alpha",
       true,
       "username2",
       "Smith"},
      // It should not matter if attribute values are upper or mixed case.
      {{nullptr, "current-password", nullptr},
       "password2",
       "beta",
       "",
       "",
       false,
       "username2",
       "Smith"},
      {{nullptr, "CURRENT-PASSWORD", nullptr},
       "password2",
       "beta",
       "",
       "",
       false,
       "username2",
       "Smith"},
      {{nullptr, "new-password", nullptr},
       "",
       "",
       "password2",
       "beta",
       true,
       "username2",
       "Smith"},
      {{nullptr, "nEw-PaSsWoRd", nullptr},
       "",
       "",
       "password2",
       "beta",
       true,
       "username2",
       "Smith"}};

  for (size_t i = 0; i < arraysize(cases); ++i) {
    SCOPED_TRACE(testing::Message() << "Iteration " << i);

    PasswordFormBuilder builder(kTestFormActionURL);
    builder.AddPasswordField("pin1", "123456", nullptr);
    builder.AddPasswordField("pin2", "789101", nullptr);
    builder.AddTextField("username1", "William", nullptr);
    builder.AddPasswordField("password1", "alpha", cases[i].autocomplete[0]);
    builder.AddTextField("username2", "Smith", nullptr);
    builder.AddPasswordField("password2", "beta", cases[i].autocomplete[1]);
    builder.AddPasswordField("password3", "gamma", cases[i].autocomplete[2]);
    builder.AddSubmitButton("submit");
    std::string html = builder.ProduceHTML();

    std::unique_ptr<PasswordForm> password_form =
        LoadHTMLAndConvertForm(html, nullptr, false);
    ASSERT_TRUE(password_form);

    // In the absence of username autocomplete attributes, the username should
    // be the text input field just before 'current-password' or before
    // 'new-password', if there is no 'current-password'.
    EXPECT_EQ(base::UTF8ToUTF16(cases[i].expected_username_element),
              password_form->username_element);
    EXPECT_EQ(base::UTF8ToUTF16(cases[i].expected_username_value),
              password_form->username_value);
    if (strcmp(cases[i].expected_username_value, "William") == 0) {
      EXPECT_THAT(password_form->other_possible_usernames,
                  testing::ElementsAre(base::UTF8ToUTF16("Smith")));
    } else {
      EXPECT_THAT(password_form->other_possible_usernames,
                  testing::ElementsAre(base::UTF8ToUTF16("William")));
    }
    EXPECT_EQ(base::UTF8ToUTF16(cases[i].expected_password_element),
              password_form->password_element);
    EXPECT_EQ(base::UTF8ToUTF16(cases[i].expected_password_value),
              password_form->password_value);
    EXPECT_EQ(base::UTF8ToUTF16(cases[i].expected_new_password_element),
              password_form->new_password_element);
    EXPECT_EQ(base::UTF8ToUTF16(cases[i].expected_new_password_value),
              password_form->new_password_value);
    EXPECT_EQ(cases[i].expected_new_password_marked_by_site,
              password_form->new_password_marked_by_site);
  }
}

TEST_F(MAYBE_PasswordFormConversionUtilsTest, IgnoreNonDisplayedTextFields) {
  PasswordFormBuilder builder(kTestFormActionURL);

  builder.AddNonDisplayedTextField("nondisplayed1", "nodispalyed_value1");
  builder.AddTextField("username", "johnsmith", nullptr);
  builder.AddNonDisplayedTextField("nondisplayed2", "nodispalyed_value2");
  builder.AddPasswordField("password", "secret", nullptr);
  builder.AddPasswordField("password", "secret", nullptr);
  builder.AddSubmitButton("submit");
  std::string html = builder.ProduceHTML();

  std::unique_ptr<PasswordForm> password_form =
      LoadHTMLAndConvertForm(html, nullptr, false);
  ASSERT_TRUE(password_form);
  EXPECT_EQ(base::UTF8ToUTF16("username"), password_form->username_element);
  EXPECT_EQ(base::UTF8ToUTF16("johnsmith"), password_form->username_value);
  EXPECT_EQ(base::UTF8ToUTF16(""), password_form->password_element);
  EXPECT_EQ(base::UTF8ToUTF16("password"), password_form->new_password_element);
  EXPECT_EQ(base::UTF8ToUTF16("secret"), password_form->new_password_value);
}

TEST_F(MAYBE_PasswordFormConversionUtilsTest, IgnoreNonDisplayedLoginPairs) {
  PasswordFormBuilder builder(kTestFormActionURL);

  builder.AddNonDisplayedTextField("nondisplayed1", "nodispalyed_value1");
  builder.AddNonDisplayedPasswordField("nondisplayed2", "nodispalyed_value2");
  builder.AddTextField("username", "johnsmith", nullptr);
  builder.AddNonDisplayedTextField("nondisplayed3", "nodispalyed_value3");
  builder.AddNonDisplayedPasswordField("nondisplayed4", "nodispalyed_value4");
  builder.AddPasswordField("password", "secret", nullptr);
  builder.AddPasswordField("password", "secret", nullptr);
  builder.AddSubmitButton("submit");
  std::string html = builder.ProduceHTML();

  std::unique_ptr<PasswordForm> password_form =
      LoadHTMLAndConvertForm(html, nullptr, false);
  ASSERT_TRUE(password_form);
  EXPECT_EQ(base::UTF8ToUTF16("username"), password_form->username_element);
  EXPECT_EQ(base::UTF8ToUTF16("johnsmith"), password_form->username_value);
  EXPECT_EQ(base::UTF8ToUTF16(""), password_form->password_element);
  EXPECT_EQ(base::UTF8ToUTF16("password"), password_form->new_password_element);
  EXPECT_EQ(base::UTF8ToUTF16("secret"), password_form->new_password_value);
}

TEST_F(MAYBE_PasswordFormConversionUtilsTest, OnlyNonDisplayedLoginPair) {
  PasswordFormBuilder builder(kTestFormActionURL);

  builder.AddNonDisplayedTextField("username", "William");
  builder.AddNonDisplayedPasswordField("password", "secret");
  builder.AddSubmitButton("submit");
  std::string html = builder.ProduceHTML();

  std::unique_ptr<PasswordForm> password_form =
      LoadHTMLAndConvertForm(html, nullptr, false);
  ASSERT_TRUE(password_form);
  EXPECT_EQ(base::UTF8ToUTF16("username"),
            password_form->username_element);
  EXPECT_EQ(base::UTF8ToUTF16("William"),
            password_form->username_value);
  EXPECT_EQ(base::UTF8ToUTF16("password"),
            password_form->password_element);
  EXPECT_EQ(base::UTF8ToUTF16("secret"),
            password_form->password_value);
}

TEST_F(MAYBE_PasswordFormConversionUtilsTest,
       VisiblePasswordAndInvisibleUsername) {
  PasswordFormBuilder builder(kTestFormActionURL);

  builder.AddNonDisplayedTextField("username", "William");
  builder.AddPasswordField("password", "secret", nullptr);
  builder.AddSubmitButton("submit");
  std::string html = builder.ProduceHTML();

  std::unique_ptr<PasswordForm> password_form =
      LoadHTMLAndConvertForm(html, nullptr, false);
  ASSERT_TRUE(password_form);
  EXPECT_EQ(base::UTF8ToUTF16("username"), password_form->username_element);
  EXPECT_EQ(base::UTF8ToUTF16("William"), password_form->username_value);
  EXPECT_EQ(base::UTF8ToUTF16("password"), password_form->password_element);
  EXPECT_EQ(base::UTF8ToUTF16("secret"), password_form->password_value);
}

TEST_F(MAYBE_PasswordFormConversionUtilsTest,
       InvisiblePassword_LatestUsernameIsVisible) {
  PasswordFormBuilder builder(kTestFormActionURL);

  builder.AddNonDisplayedTextField("search", "query");
  builder.AddTextField("username", "William", nullptr);
  builder.AddNonDisplayedPasswordField("password", "secret");
  builder.AddSubmitButton("submit");
  std::string html = builder.ProduceHTML();

  std::unique_ptr<PasswordForm> password_form =
      LoadHTMLAndConvertForm(html, nullptr, false);
  ASSERT_TRUE(password_form);
  EXPECT_EQ(base::UTF8ToUTF16("username"), password_form->username_element);
  EXPECT_EQ(base::UTF8ToUTF16("William"), password_form->username_value);
  EXPECT_EQ(base::UTF8ToUTF16("password"), password_form->password_element);
  EXPECT_EQ(base::UTF8ToUTF16("secret"), password_form->password_value);
}

TEST_F(MAYBE_PasswordFormConversionUtilsTest,
       InvisiblePassword_LatestUsernameIsInvisible) {
  PasswordFormBuilder builder(kTestFormActionURL);

  builder.AddTextField("search", "query", nullptr);
  builder.AddNonDisplayedTextField("username", "William");
  builder.AddNonDisplayedPasswordField("password", "secret");
  builder.AddSubmitButton("submit");
  std::string html = builder.ProduceHTML();

  std::unique_ptr<PasswordForm> password_form =
      LoadHTMLAndConvertForm(html, nullptr, false);
  ASSERT_TRUE(password_form);
  EXPECT_EQ(base::UTF8ToUTF16("username"), password_form->username_element);
  EXPECT_EQ(base::UTF8ToUTF16("William"), password_form->username_value);
  EXPECT_EQ(base::UTF8ToUTF16("password"), password_form->password_element);
  EXPECT_EQ(base::UTF8ToUTF16("secret"), password_form->password_value);
}

TEST_F(MAYBE_PasswordFormConversionUtilsTest, InvalidFormDueToBadActionURL) {
  PasswordFormBuilder builder("invalid_target");
  builder.AddTextField("username", "JohnSmith", nullptr);
  builder.AddSubmitButton("submit");
  builder.AddPasswordField("password", "secret", nullptr);
  std::string html = builder.ProduceHTML();

  std::unique_ptr<PasswordForm> password_form =
      LoadHTMLAndConvertForm(html, nullptr, false);
  EXPECT_FALSE(password_form);
}

TEST_F(MAYBE_PasswordFormConversionUtilsTest,
       InvalidFormDueToNoPasswordFields) {
  PasswordFormBuilder builder(kTestFormActionURL);
  builder.AddTextField("username1", "John", nullptr);
  builder.AddTextField("username2", "Smith", nullptr);
  builder.AddSubmitButton("submit");
  std::string html = builder.ProduceHTML();

  std::unique_ptr<PasswordForm> password_form =
      LoadHTMLAndConvertForm(html, nullptr, false);
  EXPECT_FALSE(password_form);
}

TEST_F(MAYBE_PasswordFormConversionUtilsTest,
       InvalidFormsDueToConfusingPasswordFields) {
  // Each test case consists of a set of parameters to be plugged into the
  // PasswordFormBuilder below.
  const char* cases[][3] = {
      // No autocomplete attributes to guide us, and we see:
      //  * three password values that are all different,
      //  * three password values that are all the same;
      //  * three password values with the first and last matching.
      // In any case, we should just give up on this form.
      {"alpha", "beta", "gamma"},
      {"alpha", "alpha", "alpha"},
      {"alpha", "beta", "alpha"}};

  for (size_t i = 0; i < arraysize(cases); ++i) {
    SCOPED_TRACE(testing::Message() << "Iteration " << i);

    PasswordFormBuilder builder(kTestFormActionURL);
    builder.AddTextField("username1", "John", nullptr);
    builder.AddPasswordField("password1", cases[i][0], nullptr);
    builder.AddPasswordField("password2", cases[i][1], nullptr);
    builder.AddPasswordField("password3", cases[i][2], nullptr);
    builder.AddSubmitButton("submit");
    std::string html = builder.ProduceHTML();

    std::unique_ptr<PasswordForm> password_form =
        LoadHTMLAndConvertForm(html, nullptr, false);
    EXPECT_FALSE(password_form);
  }
}

TEST_F(MAYBE_PasswordFormConversionUtilsTest,
       InvalidFormDueToTooManyPasswordFieldsWithoutAutocompleteAttributes) {
  PasswordFormBuilder builder(kTestFormActionURL);
  builder.AddTextField("username1", "John", nullptr);
  builder.AddPasswordField("password1", "alpha", nullptr);
  builder.AddPasswordField("password2", "alpha", nullptr);
  builder.AddPasswordField("password3", "alpha", nullptr);
  builder.AddPasswordField("password4", "alpha", nullptr);
  builder.AddSubmitButton("submit");
  std::string html = builder.ProduceHTML();

  std::unique_ptr<PasswordForm> password_form =
      LoadHTMLAndConvertForm(html, nullptr, false);
  EXPECT_FALSE(password_form);
}

TEST_F(MAYBE_PasswordFormConversionUtilsTest, LayoutClassificationLogin) {
  PasswordFormBuilder builder(kTestFormActionURL);
  builder.AddHiddenField();
  builder.AddTextField("username", "", nullptr);
  builder.AddPasswordField("password", "", nullptr);
  builder.AddSubmitButton("submit");
  std::string login_html = builder.ProduceHTML();

  std::unique_ptr<PasswordForm> login_form =
      LoadHTMLAndConvertForm(login_html, nullptr, false);
  ASSERT_TRUE(login_form);
  EXPECT_EQ(PasswordForm::Layout::LAYOUT_OTHER, login_form->layout);
}

TEST_F(MAYBE_PasswordFormConversionUtilsTest, LayoutClassificationSignup) {
  PasswordFormBuilder builder(kTestFormActionURL);
  builder.AddTextField("someotherfield", "", nullptr);
  builder.AddTextField("username", "", nullptr);
  builder.AddPasswordField("new_password", "", nullptr);
  builder.AddHiddenField();
  builder.AddPasswordField("new_password2", "", nullptr);
  builder.AddSubmitButton("submit");
  std::string signup_html = builder.ProduceHTML();

  std::unique_ptr<PasswordForm> signup_form =
      LoadHTMLAndConvertForm(signup_html, nullptr, false);
  ASSERT_TRUE(signup_form);
  EXPECT_EQ(PasswordForm::Layout::LAYOUT_OTHER, signup_form->layout);
}

TEST_F(MAYBE_PasswordFormConversionUtilsTest, LayoutClassificationChange) {
  PasswordFormBuilder builder(kTestFormActionURL);
  builder.AddTextField("username", "", nullptr);
  builder.AddPasswordField("old_password", "", nullptr);
  builder.AddHiddenField();
  builder.AddPasswordField("new_password", "", nullptr);
  builder.AddPasswordField("new_password2", "", nullptr);
  builder.AddSubmitButton("submit");
  std::string change_html = builder.ProduceHTML();

  std::unique_ptr<PasswordForm> change_form =
      LoadHTMLAndConvertForm(change_html, nullptr, false);
  ASSERT_TRUE(change_form);
  EXPECT_EQ(PasswordForm::Layout::LAYOUT_OTHER, change_form->layout);
}

TEST_F(MAYBE_PasswordFormConversionUtilsTest,
       LayoutClassificationLoginPlusSignup_A) {
  PasswordFormBuilder builder(kTestFormActionURL);
  builder.AddTextField("username", "", nullptr);
  builder.AddHiddenField();
  builder.AddPasswordField("password", "", nullptr);
  builder.AddTextField("username2", "", nullptr);
  builder.AddTextField("someotherfield", "", nullptr);
  builder.AddPasswordField("new_password", "", nullptr);
  builder.AddPasswordField("new_password2", "", nullptr);
  builder.AddHiddenField();
  builder.AddSubmitButton("submit");
  std::string login_plus_signup_html = builder.ProduceHTML();

  std::unique_ptr<PasswordForm> login_plus_signup_form =
      LoadHTMLAndConvertForm(login_plus_signup_html, nullptr, false);
  ASSERT_TRUE(login_plus_signup_form);
  EXPECT_EQ(PasswordForm::Layout::LAYOUT_LOGIN_AND_SIGNUP,
            login_plus_signup_form->layout);
}

TEST_F(MAYBE_PasswordFormConversionUtilsTest,
       LayoutClassificationLoginPlusSignup_B) {
  PasswordFormBuilder builder(kTestFormActionURL);
  builder.AddTextField("username", "", nullptr);
  builder.AddHiddenField();
  builder.AddPasswordField("password", "", nullptr);
  builder.AddTextField("username2", "", nullptr);
  builder.AddTextField("someotherfield", "", nullptr);
  builder.AddPasswordField("new_password", "", nullptr);
  builder.AddTextField("someotherfield2", "", nullptr);
  builder.AddHiddenField();
  builder.AddSubmitButton("submit");
  std::string login_plus_signup_html = builder.ProduceHTML();

  std::unique_ptr<PasswordForm> login_plus_signup_form =
      LoadHTMLAndConvertForm(login_plus_signup_html, nullptr, false);
  ASSERT_TRUE(login_plus_signup_form);
  EXPECT_EQ(PasswordForm::Layout::LAYOUT_LOGIN_AND_SIGNUP,
            login_plus_signup_form->layout);
}

TEST_F(MAYBE_PasswordFormConversionUtilsTest,
       CreditCardNumberWithTypePasswordForm) {
  PasswordFormBuilder builder(kTestFormActionURL);
  builder.AddTextField("Credit card owner name", "John Smith", nullptr);
  builder.AddPasswordField("Credit card number", "0000 0000 0000 0000",
                           nullptr);
  builder.AddTextField("cvc", "000", nullptr);
  builder.AddSubmitButton("submit");
  std::string html = builder.ProduceHTML();

  std::map<int, PasswordFormFieldPredictionType> predictions_positions;
  predictions_positions[1] = PREDICTION_NOT_PASSWORD;

  FormsPredictionsMap predictions;
  SetPredictions(html, &predictions, predictions_positions);

  std::unique_ptr<PasswordForm> password_form =
      LoadHTMLAndConvertForm(html, &predictions, false);
  EXPECT_FALSE(password_form);
}

TEST_F(MAYBE_PasswordFormConversionUtilsTest,
       CreditCardVerificationNumberWithTypePasswordForm) {
  PasswordFormBuilder builder(kTestFormActionURL);
  builder.AddTextField("Credit card owner name", "John Smith", nullptr);
  builder.AddTextField("Credit card number", "0000 0000 0000 0000", nullptr);
  builder.AddPasswordField("cvc", "000", nullptr);
  builder.AddSubmitButton("submit");
  std::string html = builder.ProduceHTML();

  std::map<int, PasswordFormFieldPredictionType> predictions_positions;
  predictions_positions[2] = PREDICTION_NOT_PASSWORD;

  FormsPredictionsMap predictions;
  SetPredictions(html, &predictions, predictions_positions);

  std::unique_ptr<PasswordForm> password_form =
      LoadHTMLAndConvertForm(html, &predictions, false);
  EXPECT_FALSE(password_form);
}

TEST_F(MAYBE_PasswordFormConversionUtilsTest,
       CreditCardNumberWithTypePasswordFormWithAutocomplete) {
  PasswordFormBuilder builder(kTestFormActionURL);
  builder.AddTextField("Credit card owner name", "John Smith", nullptr);
  builder.AddPasswordField("Credit card number", "0000 0000 0000 0000",
                           "current-password");
  builder.AddTextField("cvc", "000", nullptr);
  builder.AddSubmitButton("submit");
  std::string html = builder.ProduceHTML();

  std::map<int, PasswordFormFieldPredictionType> predictions_positions;
  predictions_positions[1] = PREDICTION_NOT_PASSWORD;

  FormsPredictionsMap predictions;
  SetPredictions(html, &predictions, predictions_positions);

  std::unique_ptr<PasswordForm> password_form =
      LoadHTMLAndConvertForm(html, &predictions, false);
  EXPECT_TRUE(password_form);
}

TEST_F(MAYBE_PasswordFormConversionUtilsTest,
       CreditCardVerificationNumberWithTypePasswordFormWithAutocomplete) {
  PasswordFormBuilder builder(kTestFormActionURL);
  builder.AddTextField("Credit card owner name", "John Smith", nullptr);
  builder.AddTextField("Credit card number", "0000 0000 0000 0000", nullptr);
  builder.AddPasswordField("cvc", "000", "new-password");
  builder.AddSubmitButton("submit");
  std::string html = builder.ProduceHTML();

  std::map<int, PasswordFormFieldPredictionType> predictions_positions;
  predictions_positions[2] = PREDICTION_NOT_PASSWORD;

  FormsPredictionsMap predictions;
  SetPredictions(html, &predictions, predictions_positions);

  std::unique_ptr<PasswordForm> password_form =
      LoadHTMLAndConvertForm(html, &predictions, false);
  EXPECT_TRUE(password_form);
}

TEST_F(MAYBE_PasswordFormConversionUtilsTest, IsGaiaReauthFormIgnored) {
  struct TestCase {
    const char* origin;
    struct KeyValue {
      KeyValue() : name(nullptr), value(nullptr) {}
      KeyValue(const char* new_name, const char* new_value)
          : name(new_name), value(new_value) {}
      const char* name;
      const char* value;
    } hidden_fields[2];
    bool expected_form_is_reauth;
  } cases[] = {
      // A common password form is parsed successfully.
      {"https://example.com",
       {TestCase::KeyValue(), TestCase::KeyValue()},
       false},
      // A common password form, even if it appears on a GAIA reauth url,
      // is parsed successfully.
      {"https://accounts.google.com",
       {TestCase::KeyValue(), TestCase::KeyValue()},
       false},
      // Not a transactional reauth.
      {"https://accounts.google.com",
       {TestCase::KeyValue("continue", "https://passwords.google.com/settings"),
        TestCase::KeyValue()},
       false},
      // A reauth form that is not for a password site is parsed successfuly.
      {"https://accounts.google.com",
       {TestCase::KeyValue("continue", "https://mail.google.com"),
        TestCase::KeyValue("rart", "")},
       false},
      // A reauth form for a password site is recognised as such.
      {"https://accounts.google.com",
       {TestCase::KeyValue("continue", "https://passwords.google.com"),
        TestCase::KeyValue("rart", "")},
       true},
      // Path, params or fragment in "continue" should not have influence.
      {"https://accounts.google.com",
       {TestCase::KeyValue("continue",
                           "https://passwords.google.com/path?param=val#frag"),
        TestCase::KeyValue("rart", "")},
       true},
      // Password site is inaccesible via HTTP, but because of HSTS the
      // following link should still continue to https://passwords.google.com.
      {"https://accounts.google.com",
       {TestCase::KeyValue("continue", "http://passwords.google.com"),
        TestCase::KeyValue("rart", "")},
       true},
      // Make sure testing sites are disabled as well.
      {"https://accounts.google.com",
       {TestCase::KeyValue(
            "continue",
            "https://passwords-ac-testing.corp.google.com/settings"),
        TestCase::KeyValue("rart", "")},
       true},
      // Specifying default port doesn't change anything.
      {"https://accounts.google.com",
       {TestCase::KeyValue("continue", "passwords.google.com:443"),
        TestCase::KeyValue("rart", "")},
       true},
      // Encoded URL is considered the same.
      {"https://accounts.google.com",
       {TestCase::KeyValue("continue",
                           "https://passwords.%67oogle.com/settings"),
        TestCase::KeyValue("rart", "")},
       true},
      // Fully qualified domain should work as well.
      {"https://accounts.google.com",
       {TestCase::KeyValue("continue",
                           "https://passwords.google.com./settings"),
        TestCase::KeyValue("rart", "")},
       true},
      // A correctly looking form, but on a different page.
      {"https://google.com",
       {TestCase::KeyValue("continue", "https://passwords.google.com"),
        TestCase::KeyValue("rart", "")},
       false},
  };

  for (TestCase& test_case : cases) {
    SCOPED_TRACE(testing::Message("origin=")
                 << test_case.origin
                 << ", hidden_fields[0]=" << test_case.hidden_fields[0].name
                 << "/" << test_case.hidden_fields[0].value
                 << ", hidden_fields[1]=" << test_case.hidden_fields[1].name
                 << "/" << test_case.hidden_fields[1].value
                 << ", expected_form_is_reauth="
                 << test_case.expected_form_is_reauth);
    std::unique_ptr<PasswordFormBuilder> builder(new PasswordFormBuilder(""));
    builder->AddTextField("username", "", nullptr);
    builder->AddPasswordField("password", "", nullptr);
    for (TestCase::KeyValue& hidden_field : test_case.hidden_fields) {
      if (hidden_field.name)
        builder->AddHiddenField(hidden_field.name, hidden_field.value);
    }
    std::string html = builder->ProduceHTML();
    WebFormElement form;
    LoadWebFormFromHTML(html, &form);
    std::vector<WebFormControlElement> control_elements;
    WebVector<blink::WebFormControlElement> web_control_elements;
    form.getFormControlElements(web_control_elements);
    control_elements.assign(web_control_elements.begin(),
                            web_control_elements.end());
    EXPECT_EQ(test_case.expected_form_is_reauth,
              IsGaiaReauthenticationForm(GURL(test_case.origin).GetOrigin(),
                                         control_elements));
  }
}

TEST_F(MAYBE_PasswordFormConversionUtilsTest,
       IdentifyingFieldsWithoutNameOrIdAttributes) {
  const char* kEmpty = nullptr;
  const struct {
    const char* username_fieldname;
    const char* password_fieldname;
    const char* new_password_fieldname;
    const char* expected_username_element;
    const char* expected_password_element;
    const char* expected_new_password_element;
  } test_cases[] = {
      {"username", "password", "new_password", "username", "password",
       "new_password"},
      {"username", "password", kEmpty, "username", "password",
       "anonymous_new_password"},
      {"username", kEmpty, kEmpty, "username", "anonymous_password",
       "anonymous_new_password"},
      {kEmpty, kEmpty, kEmpty, "anonymous_username", "anonymous_password",
       "anonymous_new_password"},
  };

  for (size_t i = 0; i < arraysize(test_cases); ++i) {
    SCOPED_TRACE(testing::Message()
                 << "Iteration " << i << ", expected_username "
                 << test_cases[i].expected_username_element
                 << ", expected_password"
                 << test_cases[i].expected_password_element
                 << ", expected_new_password "
                 << test_cases[i].expected_new_password_element);

    PasswordFormBuilder builder(kTestFormActionURL);
    if (test_cases[i].username_fieldname == kEmpty) {
      builder.AddAnonymousInputField("text");
    } else {
      builder.AddTextField(test_cases[i].username_fieldname, "", kEmpty);
    }

    if (test_cases[i].password_fieldname == kEmpty) {
      builder.AddAnonymousInputField("password");
    } else {
      builder.AddPasswordField(test_cases[i].password_fieldname, "", kEmpty);
    }

    if (test_cases[i].new_password_fieldname == kEmpty) {
      builder.AddAnonymousInputField("password");
    } else {
      builder.AddPasswordField(test_cases[i].new_password_fieldname, "",
                               kEmpty);
    }
    std::string html = builder.ProduceHTML();

    std::unique_ptr<PasswordForm> password_form =
        LoadHTMLAndConvertForm(html, nullptr, false);
    EXPECT_TRUE(password_form);

    EXPECT_EQ(base::UTF8ToUTF16(test_cases[i].expected_username_element),
              password_form->username_element);
    EXPECT_EQ(base::UTF8ToUTF16(test_cases[i].expected_password_element),
              password_form->password_element);
    EXPECT_EQ(base::UTF8ToUTF16(test_cases[i].expected_new_password_element),
              password_form->new_password_element);
  }
}

TEST_F(MAYBE_PasswordFormConversionUtilsTest,
       ProbablySignUpFormTwoTextOnePassword) {
  PasswordFormBuilder builder(kTestFormActionURL);
  builder.AddTextField("email", "johnsmith@gmail.com", nullptr);
  builder.AddTextField("username", "johnsmith", nullptr);
  builder.AddPasswordField("password", "secret", nullptr);
  std::string html = builder.ProduceHTML();

  // No user input, not considered as SignUp.
  std::unique_ptr<PasswordForm> password_form =
      LoadHTMLAndConvertForm(html, nullptr, false);
  ASSERT_TRUE(password_form);
  EXPECT_FALSE(password_form->does_look_like_signup_form);

  // With user input, considered as SignUp.
  password_form = LoadHTMLAndConvertForm(html, nullptr, true);
  ASSERT_TRUE(password_form);
  EXPECT_TRUE(password_form->does_look_like_signup_form);
}

TEST_F(MAYBE_PasswordFormConversionUtilsTest,
       ProbablySignUpFormOneTextNewAndConfirmPassword) {
  PasswordFormBuilder builder(kTestFormActionURL);
  builder.AddTextField("username", "johnsmith", nullptr);
  builder.AddPasswordField("new_password", "secret", nullptr);
  builder.AddPasswordField("confirm_password", "secret", nullptr);
  std::string html = builder.ProduceHTML();

  // No user input, not considered as SignUp.
  std::unique_ptr<PasswordForm> password_form =
      LoadHTMLAndConvertForm(html, nullptr, false);
  ASSERT_TRUE(password_form);
  EXPECT_FALSE(password_form->does_look_like_signup_form);

  // With user input, considered as SignUp.
  password_form = LoadHTMLAndConvertForm(html, nullptr, true);
  ASSERT_TRUE(password_form);
  EXPECT_TRUE(password_form->does_look_like_signup_form);
}

TEST_F(MAYBE_PasswordFormConversionUtilsTest,
       NotProbablySignUpFormOneTextCurrentAndNewPassword) {
  PasswordFormBuilder builder(kTestFormActionURL);
  builder.AddTextField("username", "johnsmith", nullptr);
  builder.AddPasswordField("password", "secret", nullptr);
  builder.AddPasswordField("new_password", "new_secret", nullptr);
  std::string html = builder.ProduceHTML();

  std::unique_ptr<PasswordForm> password_form =
      LoadHTMLAndConvertForm(html, nullptr, true);
  ASSERT_TRUE(password_form);
  EXPECT_FALSE(password_form->does_look_like_signup_form);
}

TEST_F(MAYBE_PasswordFormConversionUtilsTest,
       NotProbablySignUpFormForSignInForm) {
  PasswordFormBuilder builder(kTestFormActionURL);
  builder.AddTextField("username", "johnsmith", nullptr);
  builder.AddPasswordField("password", "secret", nullptr);
  std::string html = builder.ProduceHTML();

  std::unique_ptr<PasswordForm> password_form =
      LoadHTMLAndConvertForm(html, nullptr, true);
  ASSERT_TRUE(password_form);
  EXPECT_FALSE(password_form->does_look_like_signup_form);
}

}  // namespace autofill
