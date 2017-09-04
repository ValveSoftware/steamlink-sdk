// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/common/autofill_types_struct_traits.h"

#include "base/i18n/rtl.h"
#include "ipc/ipc_message_utils.h"
#include "url/mojo/origin_struct_traits.h"
#include "url/mojo/url_gurl_struct_traits.h"

using namespace autofill;

namespace mojo {

// static
mojom::CheckStatus
EnumTraits<mojom::CheckStatus, FormFieldData::CheckStatus>::ToMojom(
    FormFieldData::CheckStatus input) {
  switch (input) {
    case FormFieldData::CheckStatus::NOT_CHECKABLE:
      return mojom::CheckStatus::NOT_CHECKABLE;
    case FormFieldData::CheckStatus::CHECKABLE_BUT_UNCHECKED:
      return mojom::CheckStatus::CHECKABLE_BUT_UNCHECKED;
    case FormFieldData::CheckStatus::CHECKED:
      return mojom::CheckStatus::CHECKED;
  }

  NOTREACHED();
  return mojom::CheckStatus::NOT_CHECKABLE;
}

// static
bool EnumTraits<mojom::CheckStatus, FormFieldData::CheckStatus>::FromMojom(
    mojom::CheckStatus input,
    FormFieldData::CheckStatus* output) {
  switch (input) {
    case mojom::CheckStatus::NOT_CHECKABLE:
      *output = FormFieldData::CheckStatus::NOT_CHECKABLE;
      return true;
    case mojom::CheckStatus::CHECKABLE_BUT_UNCHECKED:
      *output = FormFieldData::CheckStatus::CHECKABLE_BUT_UNCHECKED;
      return true;
    case mojom::CheckStatus::CHECKED:
      *output = FormFieldData::CheckStatus::CHECKED;
      return true;
  }

  NOTREACHED();
  return false;
}

// static
mojom::RoleAttribute
EnumTraits<mojom::RoleAttribute, FormFieldData::RoleAttribute>::ToMojom(
    FormFieldData::RoleAttribute input) {
  switch (input) {
    case FormFieldData::RoleAttribute::ROLE_ATTRIBUTE_PRESENTATION:
      return mojom::RoleAttribute::ROLE_ATTRIBUTE_PRESENTATION;
    case FormFieldData::RoleAttribute::ROLE_ATTRIBUTE_OTHER:
      return mojom::RoleAttribute::ROLE_ATTRIBUTE_OTHER;
  }

  NOTREACHED();
  return mojom::RoleAttribute::ROLE_ATTRIBUTE_OTHER;
}

// static
bool EnumTraits<mojom::RoleAttribute, FormFieldData::RoleAttribute>::FromMojom(
    mojom::RoleAttribute input,
    FormFieldData::RoleAttribute* output) {
  switch (input) {
    case mojom::RoleAttribute::ROLE_ATTRIBUTE_PRESENTATION:
      *output = FormFieldData::RoleAttribute::ROLE_ATTRIBUTE_PRESENTATION;
      return true;
    case mojom::RoleAttribute::ROLE_ATTRIBUTE_OTHER:
      *output = FormFieldData::RoleAttribute::ROLE_ATTRIBUTE_OTHER;
      return true;
  }

  NOTREACHED();
  return false;
}

// static
mojom::TextDirection
EnumTraits<mojom::TextDirection, base::i18n::TextDirection>::ToMojom(
    base::i18n::TextDirection input) {
  switch (input) {
    case base::i18n::TextDirection::UNKNOWN_DIRECTION:
      return mojom::TextDirection::UNKNOWN_DIRECTION;
    case base::i18n::TextDirection::RIGHT_TO_LEFT:
      return mojom::TextDirection::RIGHT_TO_LEFT;
    case base::i18n::TextDirection::LEFT_TO_RIGHT:
      return mojom::TextDirection::LEFT_TO_RIGHT;
    case base::i18n::TextDirection::TEXT_DIRECTION_NUM_DIRECTIONS:
      return mojom::TextDirection::TEXT_DIRECTION_NUM_DIRECTIONS;
  }

  NOTREACHED();
  return mojom::TextDirection::UNKNOWN_DIRECTION;
}

// static
bool EnumTraits<mojom::TextDirection, base::i18n::TextDirection>::FromMojom(
    mojom::TextDirection input,
    base::i18n::TextDirection* output) {
  switch (input) {
    case mojom::TextDirection::UNKNOWN_DIRECTION:
      *output = base::i18n::TextDirection::UNKNOWN_DIRECTION;
      return true;
    case mojom::TextDirection::RIGHT_TO_LEFT:
      *output = base::i18n::TextDirection::RIGHT_TO_LEFT;
      return true;
    case mojom::TextDirection::LEFT_TO_RIGHT:
      *output = base::i18n::TextDirection::LEFT_TO_RIGHT;
      return true;
    case mojom::TextDirection::TEXT_DIRECTION_NUM_DIRECTIONS:
      *output = base::i18n::TextDirection::TEXT_DIRECTION_NUM_DIRECTIONS;
      return true;
  }

  NOTREACHED();
  return false;
}

// static
mojom::GenerationUploadStatus EnumTraits<mojom::GenerationUploadStatus,
                                         PasswordForm::GenerationUploadStatus>::
    ToMojom(PasswordForm::GenerationUploadStatus input) {
  switch (input) {
    case PasswordForm::GenerationUploadStatus::NO_SIGNAL_SENT:
      return mojom::GenerationUploadStatus::NO_SIGNAL_SENT;
    case PasswordForm::GenerationUploadStatus::POSITIVE_SIGNAL_SENT:
      return mojom::GenerationUploadStatus::POSITIVE_SIGNAL_SENT;
    case PasswordForm::GenerationUploadStatus::NEGATIVE_SIGNAL_SENT:
      return mojom::GenerationUploadStatus::NEGATIVE_SIGNAL_SENT;
    case PasswordForm::GenerationUploadStatus::UNKNOWN_STATUS:
      return mojom::GenerationUploadStatus::UNKNOWN_STATUS;
  }

  NOTREACHED();
  return mojom::GenerationUploadStatus::UNKNOWN_STATUS;
}

// static
bool EnumTraits<mojom::GenerationUploadStatus,
                PasswordForm::GenerationUploadStatus>::
    FromMojom(mojom::GenerationUploadStatus input,
              PasswordForm::GenerationUploadStatus* output) {
  switch (input) {
    case mojom::GenerationUploadStatus::NO_SIGNAL_SENT:
      *output = PasswordForm::GenerationUploadStatus::NO_SIGNAL_SENT;
      return true;
    case mojom::GenerationUploadStatus::POSITIVE_SIGNAL_SENT:
      *output = PasswordForm::GenerationUploadStatus::POSITIVE_SIGNAL_SENT;
      return true;
    case mojom::GenerationUploadStatus::NEGATIVE_SIGNAL_SENT:
      *output = PasswordForm::GenerationUploadStatus::NEGATIVE_SIGNAL_SENT;
      return true;
    case mojom::GenerationUploadStatus::UNKNOWN_STATUS:
      *output = PasswordForm::GenerationUploadStatus::UNKNOWN_STATUS;
      return true;
  }

  NOTREACHED();
  return false;
}

// static
mojom::PasswordFormLayout
EnumTraits<mojom::PasswordFormLayout, PasswordForm::Layout>::ToMojom(
    PasswordForm::Layout input) {
  switch (input) {
    case PasswordForm::Layout::LAYOUT_OTHER:
      return mojom::PasswordFormLayout::LAYOUT_OTHER;
    case PasswordForm::Layout::LAYOUT_LOGIN_AND_SIGNUP:
      return mojom::PasswordFormLayout::LAYOUT_LOGIN_AND_SIGNUP;
  }

  NOTREACHED();
  return mojom::PasswordFormLayout::LAYOUT_OTHER;
}

// static
bool EnumTraits<mojom::PasswordFormLayout, PasswordForm::Layout>::FromMojom(
    mojom::PasswordFormLayout input,
    PasswordForm::Layout* output) {
  switch (input) {
    case mojom::PasswordFormLayout::LAYOUT_OTHER:
      *output = PasswordForm::Layout::LAYOUT_OTHER;
      return true;
    case mojom::PasswordFormLayout::LAYOUT_LOGIN_AND_SIGNUP:
      *output = PasswordForm::Layout::LAYOUT_LOGIN_AND_SIGNUP;
      return true;
  }

  NOTREACHED();
  return false;
}

// static
mojom::PasswordFormType
EnumTraits<mojom::PasswordFormType, PasswordForm::Type>::ToMojom(
    PasswordForm::Type input) {
  switch (input) {
    case PasswordForm::Type::TYPE_MANUAL:
      return mojom::PasswordFormType::TYPE_MANUAL;
    case PasswordForm::Type::TYPE_GENERATED:
      return mojom::PasswordFormType::TYPE_GENERATED;
    case PasswordForm::Type::TYPE_API:
      return mojom::PasswordFormType::TYPE_API;
  }

  NOTREACHED();
  return mojom::PasswordFormType::TYPE_MANUAL;
}

// static
bool EnumTraits<mojom::PasswordFormType, PasswordForm::Type>::FromMojom(
    mojom::PasswordFormType input,
    PasswordForm::Type* output) {
  switch (input) {
    case mojom::PasswordFormType::TYPE_MANUAL:
      *output = PasswordForm::Type::TYPE_MANUAL;
      return true;
    case mojom::PasswordFormType::TYPE_GENERATED:
      *output = PasswordForm::Type::TYPE_GENERATED;
      return true;
    case mojom::PasswordFormType::TYPE_API:
      *output = PasswordForm::Type::TYPE_API;
      return true;
  }

  NOTREACHED();
  return false;
}

// static
mojom::PasswordFormScheme
EnumTraits<mojom::PasswordFormScheme, PasswordForm::Scheme>::ToMojom(
    PasswordForm::Scheme input) {
  switch (input) {
    case PasswordForm::Scheme::SCHEME_HTML:
      return mojom::PasswordFormScheme::SCHEME_HTML;
    case PasswordForm::Scheme::SCHEME_BASIC:
      return mojom::PasswordFormScheme::SCHEME_BASIC;
    case PasswordForm::Scheme::SCHEME_DIGEST:
      return mojom::PasswordFormScheme::SCHEME_DIGEST;
    case PasswordForm::Scheme::SCHEME_OTHER:
      return mojom::PasswordFormScheme::SCHEME_OTHER;
    case PasswordForm::Scheme::SCHEME_USERNAME_ONLY:
      return mojom::PasswordFormScheme::SCHEME_USERNAME_ONLY;
  }

  NOTREACHED();
  return mojom::PasswordFormScheme::SCHEME_OTHER;
}

// static
bool EnumTraits<mojom::PasswordFormScheme, PasswordForm::Scheme>::FromMojom(
    mojom::PasswordFormScheme input,
    PasswordForm::Scheme* output) {
  switch (input) {
    case mojom::PasswordFormScheme::SCHEME_HTML:
      *output = PasswordForm::Scheme::SCHEME_HTML;
      return true;
    case mojom::PasswordFormScheme::SCHEME_BASIC:
      *output = PasswordForm::Scheme::SCHEME_BASIC;
      return true;
    case mojom::PasswordFormScheme::SCHEME_DIGEST:
      *output = PasswordForm::Scheme::SCHEME_DIGEST;
      return true;
    case mojom::PasswordFormScheme::SCHEME_OTHER:
      *output = PasswordForm::Scheme::SCHEME_OTHER;
      return true;
    case mojom::PasswordFormScheme::SCHEME_USERNAME_ONLY:
      *output = PasswordForm::Scheme::SCHEME_USERNAME_ONLY;
      return true;
  }

  NOTREACHED();
  return false;
}

// static
mojom::PasswordFormFieldPredictionType EnumTraits<
    mojom::PasswordFormFieldPredictionType,
    PasswordFormFieldPredictionType>::ToMojom(PasswordFormFieldPredictionType
                                                  input) {
  switch (input) {
    case PasswordFormFieldPredictionType::PREDICTION_USERNAME:
      return mojom::PasswordFormFieldPredictionType::PREDICTION_USERNAME;
    case PasswordFormFieldPredictionType::PREDICTION_CURRENT_PASSWORD:
      return mojom::PasswordFormFieldPredictionType::
          PREDICTION_CURRENT_PASSWORD;
    case PasswordFormFieldPredictionType::PREDICTION_NEW_PASSWORD:
      return mojom::PasswordFormFieldPredictionType::PREDICTION_NEW_PASSWORD;
    case PasswordFormFieldPredictionType::PREDICTION_NOT_PASSWORD:
      return mojom::PasswordFormFieldPredictionType::PREDICTION_NOT_PASSWORD;
  }

  NOTREACHED();
  return mojom::PasswordFormFieldPredictionType::PREDICTION_NOT_PASSWORD;
}

// static
bool EnumTraits<mojom::PasswordFormFieldPredictionType,
                PasswordFormFieldPredictionType>::
    FromMojom(mojom::PasswordFormFieldPredictionType input,
              PasswordFormFieldPredictionType* output) {
  switch (input) {
    case mojom::PasswordFormFieldPredictionType::PREDICTION_USERNAME:
      *output = PasswordFormFieldPredictionType::PREDICTION_USERNAME;
      return true;
    case mojom::PasswordFormFieldPredictionType::PREDICTION_CURRENT_PASSWORD:
      *output = PasswordFormFieldPredictionType::PREDICTION_CURRENT_PASSWORD;
      return true;
    case mojom::PasswordFormFieldPredictionType::PREDICTION_NEW_PASSWORD:
      *output = PasswordFormFieldPredictionType::PREDICTION_NEW_PASSWORD;
      return true;
    case mojom::PasswordFormFieldPredictionType::PREDICTION_NOT_PASSWORD:
      *output = PasswordFormFieldPredictionType::PREDICTION_NOT_PASSWORD;
      return true;
  }

  NOTREACHED();
  return false;
}

// static
bool StructTraits<mojom::FormFieldDataDataView, FormFieldData>::Read(
    mojom::FormFieldDataDataView data,
    FormFieldData* out) {
  if (!data.ReadLabel(&out->label))
    return false;
  if (!data.ReadName(&out->name))
    return false;
  if (!data.ReadValue(&out->value))
    return false;

  if (!data.ReadFormControlType(&out->form_control_type))
    return false;
  if (!data.ReadAutocompleteAttribute(&out->autocomplete_attribute))
    return false;

  if (!data.ReadPlaceholder(&out->placeholder))
    return false;

  if (!data.ReadCssClasses(&out->css_classes))
    return false;

  out->properties_mask = data.properties_mask();

  out->max_length = data.max_length();
  out->is_autofilled = data.is_autofilled();

  if (!data.ReadCheckStatus(&out->check_status))
    return false;

  out->is_focusable = data.is_focusable();
  out->should_autocomplete = data.should_autocomplete();

  if (!data.ReadRole(&out->role))
    return false;

  if (!data.ReadTextDirection(&out->text_direction))
    return false;

  if (!data.ReadOptionValues(&out->option_values))
    return false;
  if (!data.ReadOptionContents(&out->option_contents))
    return false;

  return true;
}

// static
bool StructTraits<mojom::FormDataDataView, FormData>::Read(
    mojom::FormDataDataView data,
    FormData* out) {
  if (!data.ReadName(&out->name))
    return false;
  if (!data.ReadOrigin(&out->origin))
    return false;
  if (!data.ReadAction(&out->action))
    return false;

  out->is_form_tag = data.is_form_tag();
  out->is_formless_checkout = data.is_formless_checkout();

  if (!data.ReadFields(&out->fields))
    return false;

  return true;
}

// static
bool StructTraits<mojom::FormFieldDataPredictionsDataView,
                  FormFieldDataPredictions>::
    Read(mojom::FormFieldDataPredictionsDataView data,
         FormFieldDataPredictions* out) {
  if (!data.ReadField(&out->field))
    return false;
  if (!data.ReadSignature(&out->signature))
    return false;
  if (!data.ReadHeuristicType(&out->heuristic_type))
    return false;
  if (!data.ReadServerType(&out->server_type))
    return false;
  if (!data.ReadOverallType(&out->overall_type))
    return false;
  if (!data.ReadParseableName(&out->parseable_name))
    return false;

  return true;
}

// static
bool StructTraits<mojom::FormDataPredictionsDataView, FormDataPredictions>::
    Read(mojom::FormDataPredictionsDataView data, FormDataPredictions* out) {
  if (!data.ReadData(&out->data))
    return false;
  if (!data.ReadSignature(&out->signature))
    return false;
  if (!data.ReadFields(&out->fields))
    return false;

  return true;
}

// static
bool StructTraits<mojom::PasswordAndRealmDataView, PasswordAndRealm>::Read(
    mojom::PasswordAndRealmDataView data,
    PasswordAndRealm* out) {
  if (!data.ReadPassword(&out->password))
    return false;
  if (!data.ReadRealm(&out->realm))
    return false;

  return true;
}

// static
bool StructTraits<
    mojom::UsernamesCollectionKeyDataView,
    UsernamesCollectionKey>::Read(mojom::UsernamesCollectionKeyDataView data,
                                  UsernamesCollectionKey* out) {
  if (!data.ReadUsername(&out->username))
    return false;
  if (!data.ReadPassword(&out->password))
    return false;
  if (!data.ReadRealm(&out->realm))
    return false;

  return true;
}

// static
void* StructTraits<mojom::PasswordFormFillDataDataView, PasswordFormFillData>::
    SetUpContext(const PasswordFormFillData& r) {
  // Extracts keys vector and values vector from the map, saves them as a pair.
  auto* pair = new UsernamesCollectionKeysValuesPair();
  for (const auto& i : r.other_possible_usernames) {
    pair->first.push_back(i.first);
    pair->second.push_back(i.second);
  }

  return pair;
}

// static
void StructTraits<mojom::PasswordFormFillDataDataView, PasswordFormFillData>::
    TearDownContext(const PasswordFormFillData& r, void* context) {
  delete static_cast<UsernamesCollectionKeysValuesPair*>(context);
}

// static
bool StructTraits<mojom::PasswordFormFillDataDataView, PasswordFormFillData>::
    Read(mojom::PasswordFormFillDataDataView data, PasswordFormFillData* out) {
  if (!data.ReadName(&out->name) || !data.ReadOrigin(&out->origin) ||
      !data.ReadAction(&out->action) ||
      !data.ReadUsernameField(&out->username_field) ||
      !data.ReadPasswordField(&out->password_field) ||
      !data.ReadPreferredRealm(&out->preferred_realm) ||
      !data.ReadAdditionalLogins(&out->additional_logins))
    return false;

  // Combines keys vector and values vector to the map.
  std::vector<UsernamesCollectionKey> keys;
  if (!data.ReadOtherPossibleUsernamesKeys(&keys))
    return false;
  std::vector<std::vector<base::string16>> values;
  if (!data.ReadOtherPossibleUsernamesValues(&values))
    return false;
  if (keys.size() != values.size())
    return false;
  out->other_possible_usernames.clear();
  for (size_t i = 0; i < keys.size(); ++i)
    out->other_possible_usernames.insert({keys[i], values[i]});

  out->wait_for_username = data.wait_for_username();
  out->is_possible_change_password_form =
      data.is_possible_change_password_form();

  return true;
}

// static
bool StructTraits<mojom::PasswordFormGenerationDataDataView,
                  PasswordFormGenerationData>::
    Read(mojom::PasswordFormGenerationDataDataView data,
         PasswordFormGenerationData* out) {
  out->form_signature = data.form_signature();
  out->field_signature = data.field_signature();
  return true;
}

// static
bool StructTraits<mojom::PasswordFormDataView, PasswordForm>::Read(
    mojom::PasswordFormDataView data,
    PasswordForm* out) {
  if (!data.ReadScheme(&out->scheme) ||
      !data.ReadSignonRealm(&out->signon_realm) ||
      !data.ReadOriginWithPath(&out->origin) ||
      !data.ReadAction(&out->action) ||
      !data.ReadAffiliatedWebRealm(&out->affiliated_web_realm) ||
      !data.ReadSubmitElement(&out->submit_element) ||
      !data.ReadUsernameElement(&out->username_element))
    return false;

  out->username_marked_by_site = data.username_marked_by_site();

  if (!data.ReadUsernameValue(&out->username_value) ||
      !data.ReadOtherPossibleUsernames(&out->other_possible_usernames) ||
      !data.ReadPasswordElement(&out->password_element) ||
      !data.ReadPasswordValue(&out->password_value))
    return false;

  out->password_value_is_default = data.password_value_is_default();

  if (!data.ReadNewPasswordElement(&out->new_password_element) ||
      !data.ReadNewPasswordValue(&out->new_password_value))
    return false;

  out->new_password_value_is_default = data.new_password_value_is_default();
  out->new_password_marked_by_site = data.new_password_marked_by_site();
  out->preferred = data.preferred();

  if (!data.ReadDateCreated(&out->date_created) ||
      !data.ReadDateSynced(&out->date_synced))
    return false;

  out->blacklisted_by_user = data.blacklisted_by_user();

  if (!data.ReadType(&out->type))
    return false;

  out->times_used = data.times_used();

  if (!data.ReadFormData(&out->form_data) ||
      !data.ReadGenerationUploadStatus(&out->generation_upload_status) ||
      !data.ReadDisplayName(&out->display_name) ||
      !data.ReadIconUrl(&out->icon_url) ||
      !data.ReadFederationOrigin(&out->federation_origin))
    return false;

  out->skip_zero_click = data.skip_zero_click();

  if (!data.ReadLayout(&out->layout))
    return false;

  out->was_parsed_using_autofill_predictions =
      data.was_parsed_using_autofill_predictions();
  out->is_public_suffix_match = data.is_public_suffix_match();
  out->is_affiliation_based_match = data.is_affiliation_based_match();
  out->does_look_like_signup_form = data.does_look_like_signup_form();

  return true;
}

// static
void* StructTraits<mojom::PasswordFormFieldPredictionMapDataView,
                   PasswordFormFieldPredictionMap>::
    SetUpContext(const PasswordFormFieldPredictionMap& r) {
  // Extracts keys vector and values vector from the map, saves them as a pair.
  auto* pair = new KeysValuesPair();
  for (const auto& i : r) {
    pair->first.push_back(i.first);
    pair->second.push_back(i.second);
  }

  return pair;
}

// static
void StructTraits<mojom::PasswordFormFieldPredictionMapDataView,
                  PasswordFormFieldPredictionMap>::
    TearDownContext(const PasswordFormFieldPredictionMap& r, void* context) {
  delete static_cast<KeysValuesPair*>(context);
}

// static
bool StructTraits<mojom::PasswordFormFieldPredictionMapDataView,
                  PasswordFormFieldPredictionMap>::
    Read(mojom::PasswordFormFieldPredictionMapDataView data,
         PasswordFormFieldPredictionMap* out) {
  // Combines keys vector and values vector to the map.
  std::vector<FormFieldData> keys;
  if (!data.ReadKeys(&keys))
    return false;
  std::vector<PasswordFormFieldPredictionType> values;
  if (!data.ReadValues(&values))
    return false;
  if (keys.size() != values.size())
    return false;
  out->clear();
  for (size_t i = 0; i < keys.size(); ++i)
    out->insert({keys[i], values[i]});

  return true;
}

// static
void* StructTraits<mojom::FormsPredictionsMapDataView,
                   FormsPredictionsMap>::SetUpContext(const FormsPredictionsMap&
                                                          r) {
  // Extracts keys vector and values vector from the map, saves them as a pair.
  auto* pair = new KeysValuesPair();
  for (const auto& i : r) {
    pair->first.push_back(i.first);
    pair->second.push_back(i.second);
  }

  return pair;
}

// static
void StructTraits<mojom::FormsPredictionsMapDataView, FormsPredictionsMap>::
    TearDownContext(const FormsPredictionsMap& r, void* context) {
  delete static_cast<KeysValuesPair*>(context);
}

// static
bool StructTraits<mojom::FormsPredictionsMapDataView, FormsPredictionsMap>::
    Read(mojom::FormsPredictionsMapDataView data, FormsPredictionsMap* out) {
  // Combines keys vector and values vector to the map.
  std::vector<FormData> keys;
  if (!data.ReadKeys(&keys))
    return false;
  std::vector<PasswordFormFieldPredictionMap> values;
  if (!data.ReadValues(&values))
    return false;
  if (keys.size() != values.size())
    return false;
  out->clear();
  for (size_t i = 0; i < keys.size(); ++i)
    out->insert({keys[i], values[i]});

  return true;
}

}  // namespace mojo
