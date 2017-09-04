// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_COMMON_AUTOFILL_TYPES_STRUCT_TRAITS_H_
#define COMPONENTS_AUTOFILL_CONTENT_COMMON_AUTOFILL_TYPES_STRUCT_TRAITS_H_

#include <utility>

#include "base/strings/string16.h"
#include "components/autofill/content/common/autofill_types.mojom.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_predictions.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/form_field_data_predictions.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_form_field_prediction_map.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/autofill/core/common/password_form_generation_data.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct EnumTraits<autofill::mojom::CheckStatus,
                  autofill::FormFieldData::CheckStatus> {
  static autofill::mojom::CheckStatus ToMojom(
      autofill::FormFieldData::CheckStatus input);
  static bool FromMojom(autofill::mojom::CheckStatus input,
                        autofill::FormFieldData::CheckStatus* output);
};

template <>
struct EnumTraits<autofill::mojom::RoleAttribute,
                  autofill::FormFieldData::RoleAttribute> {
  static autofill::mojom::RoleAttribute ToMojom(
      autofill::FormFieldData::RoleAttribute input);
  static bool FromMojom(autofill::mojom::RoleAttribute input,
                        autofill::FormFieldData::RoleAttribute* output);
};

template <>
struct EnumTraits<autofill::mojom::TextDirection, base::i18n::TextDirection> {
  static autofill::mojom::TextDirection ToMojom(
      base::i18n::TextDirection input);
  static bool FromMojom(autofill::mojom::TextDirection input,
                        base::i18n::TextDirection* output);
};

template <>
struct EnumTraits<autofill::mojom::GenerationUploadStatus,
                  autofill::PasswordForm::GenerationUploadStatus> {
  static autofill::mojom::GenerationUploadStatus ToMojom(
      autofill::PasswordForm::GenerationUploadStatus input);
  static bool FromMojom(autofill::mojom::GenerationUploadStatus input,
                        autofill::PasswordForm::GenerationUploadStatus* output);
};

template <>
struct EnumTraits<autofill::mojom::PasswordFormLayout,
                  autofill::PasswordForm::Layout> {
  static autofill::mojom::PasswordFormLayout ToMojom(
      autofill::PasswordForm::Layout input);
  static bool FromMojom(autofill::mojom::PasswordFormLayout input,
                        autofill::PasswordForm::Layout* output);
};

template <>
struct EnumTraits<autofill::mojom::PasswordFormType,
                  autofill::PasswordForm::Type> {
  static autofill::mojom::PasswordFormType ToMojom(
      autofill::PasswordForm::Type input);
  static bool FromMojom(autofill::mojom::PasswordFormType input,
                        autofill::PasswordForm::Type* output);
};

template <>
struct EnumTraits<autofill::mojom::PasswordFormScheme,
                  autofill::PasswordForm::Scheme> {
  static autofill::mojom::PasswordFormScheme ToMojom(
      autofill::PasswordForm::Scheme input);
  static bool FromMojom(autofill::mojom::PasswordFormScheme input,
                        autofill::PasswordForm::Scheme* output);
};

template <>
struct EnumTraits<autofill::mojom::PasswordFormFieldPredictionType,
                  autofill::PasswordFormFieldPredictionType> {
  static autofill::mojom::PasswordFormFieldPredictionType ToMojom(
      autofill::PasswordFormFieldPredictionType input);
  static bool FromMojom(autofill::mojom::PasswordFormFieldPredictionType input,
                        autofill::PasswordFormFieldPredictionType* output);
};

template <>
struct StructTraits<autofill::mojom::FormFieldDataDataView,
                    autofill::FormFieldData> {
  static const base::string16& label(const autofill::FormFieldData& r) {
    return r.label;
  }

  static const base::string16& name(const autofill::FormFieldData& r) {
    return r.name;
  }

  static const base::string16& value(const autofill::FormFieldData& r) {
    return r.value;
  }

  static const std::string& form_control_type(
      const autofill::FormFieldData& r) {
    return r.form_control_type;
  }

  static const std::string& autocomplete_attribute(
      const autofill::FormFieldData& r) {
    return r.autocomplete_attribute;
  }

  static const base::string16& placeholder(const autofill::FormFieldData& r) {
    return r.placeholder;
  }

  static const base::string16& css_classes(const autofill::FormFieldData& r) {
    return r.css_classes;
  }

  static uint32_t properties_mask(const autofill::FormFieldData& r) {
    return r.properties_mask;
  }

  static uint64_t max_length(const autofill::FormFieldData& r) {
    return r.max_length;
  }

  static bool is_autofilled(const autofill::FormFieldData& r) {
    return r.is_autofilled;
  }

  static autofill::FormFieldData::CheckStatus check_status(
      const autofill::FormFieldData& r) {
    return r.check_status;
  }

  static bool is_focusable(const autofill::FormFieldData& r) {
    return r.is_focusable;
  }

  static bool should_autocomplete(const autofill::FormFieldData& r) {
    return r.should_autocomplete;
  }

  static autofill::FormFieldData::RoleAttribute role(
      const autofill::FormFieldData& r) {
    return r.role;
  }

  static base::i18n::TextDirection text_direction(
      const autofill::FormFieldData& r) {
    return r.text_direction;
  }

  static const std::vector<base::string16>& option_values(
      const autofill::FormFieldData& r) {
    return r.option_values;
  }

  static const std::vector<base::string16>& option_contents(
      const autofill::FormFieldData& r) {
    return r.option_contents;
  }

  static bool Read(autofill::mojom::FormFieldDataDataView data,
                   autofill::FormFieldData* out);
};

template <>
struct StructTraits<autofill::mojom::FormDataDataView, autofill::FormData> {
  static const base::string16& name(const autofill::FormData& r) {
    return r.name;
  }

  static const GURL& origin(const autofill::FormData& r) { return r.origin; }

  static const GURL& action(const autofill::FormData& r) { return r.action; }

  static bool is_form_tag(const autofill::FormData& r) { return r.is_form_tag; }

  static bool is_formless_checkout(const autofill::FormData& r) {
    return r.is_formless_checkout;
  }

  static const std::vector<autofill::FormFieldData>& fields(
      const autofill::FormData& r) {
    return r.fields;
  }

  static bool Read(autofill::mojom::FormDataDataView data,
                   autofill::FormData* out);
};

template <>
struct StructTraits<autofill::mojom::FormFieldDataPredictionsDataView,
                    autofill::FormFieldDataPredictions> {
  static const autofill::FormFieldData& field(
      const autofill::FormFieldDataPredictions& r) {
    return r.field;
  }

  static const std::string& signature(
      const autofill::FormFieldDataPredictions& r) {
    return r.signature;
  }

  static const std::string& heuristic_type(
      const autofill::FormFieldDataPredictions& r) {
    return r.heuristic_type;
  }

  static const std::string& server_type(
      const autofill::FormFieldDataPredictions& r) {
    return r.server_type;
  }

  static const std::string& overall_type(
      const autofill::FormFieldDataPredictions& r) {
    return r.overall_type;
  }

  static const std::string& parseable_name(
      const autofill::FormFieldDataPredictions& r) {
    return r.parseable_name;
  }

  static bool Read(autofill::mojom::FormFieldDataPredictionsDataView data,
                   autofill::FormFieldDataPredictions* out);
};

template <>
struct StructTraits<autofill::mojom::FormDataPredictionsDataView,
                    autofill::FormDataPredictions> {
  static const autofill::FormData& data(
      const autofill::FormDataPredictions& r) {
    return r.data;
  }

  static const std::string& signature(const autofill::FormDataPredictions& r) {
    return r.signature;
  }

  static const std::vector<autofill::FormFieldDataPredictions>& fields(
      const autofill::FormDataPredictions& r) {
    return r.fields;
  }

  static bool Read(autofill::mojom::FormDataPredictionsDataView data,
                   autofill::FormDataPredictions* out);
};

template <>
struct StructTraits<autofill::mojom::PasswordAndRealmDataView,
                    autofill::PasswordAndRealm> {
  static const base::string16& password(const autofill::PasswordAndRealm& r) {
    return r.password;
  }

  static const std::string& realm(const autofill::PasswordAndRealm& r) {
    return r.realm;
  }

  static bool Read(autofill::mojom::PasswordAndRealmDataView data,
                   autofill::PasswordAndRealm* out);
};

template <>
struct StructTraits<autofill::mojom::UsernamesCollectionKeyDataView,
                    autofill::UsernamesCollectionKey> {
  static const base::string16& username(
      const autofill::UsernamesCollectionKey& r) {
    return r.username;
  }

  static const base::string16& password(
      const autofill::UsernamesCollectionKey& r) {
    return r.password;
  }

  static const std::string& realm(const autofill::UsernamesCollectionKey& r) {
    return r.realm;
  }

  static bool Read(autofill::mojom::UsernamesCollectionKeyDataView data,
                   autofill::UsernamesCollectionKey* out);
};

template <>
struct StructTraits<autofill::mojom::PasswordFormFillDataDataView,
                    autofill::PasswordFormFillData> {
  using UsernamesCollectionKeysValuesPair =
      std::pair<std::vector<autofill::UsernamesCollectionKey>,
                std::vector<std::vector<base::string16>>>;

  static void* SetUpContext(const autofill::PasswordFormFillData& r);

  static void TearDownContext(const autofill::PasswordFormFillData& r,
                              void* context);

  static const base::string16& name(const autofill::PasswordFormFillData& r) {
    return r.name;
  }

  static const GURL& origin(const autofill::PasswordFormFillData& r) {
    return r.origin;
  }

  static const GURL& action(const autofill::PasswordFormFillData& r) {
    return r.action;
  }

  static const autofill::FormFieldData& username_field(
      const autofill::PasswordFormFillData& r) {
    return r.username_field;
  }

  static const autofill::FormFieldData& password_field(
      const autofill::PasswordFormFillData& r) {
    return r.password_field;
  }

  static const std::string& preferred_realm(
      const autofill::PasswordFormFillData& r) {
    return r.preferred_realm;
  }

  static const std::map<base::string16, autofill::PasswordAndRealm>&
  additional_logins(const autofill::PasswordFormFillData& r) {
    return r.additional_logins;
  }

  static const std::vector<autofill::UsernamesCollectionKey>&
  other_possible_usernames_keys(const autofill::PasswordFormFillData& r,
                                void* context) {
    return static_cast<UsernamesCollectionKeysValuesPair*>(context)->first;
  }

  static const std::vector<std::vector<base::string16>>&
  other_possible_usernames_values(const autofill::PasswordFormFillData& r,
                                  void* context) {
    return static_cast<UsernamesCollectionKeysValuesPair*>(context)->second;
  }

  static bool wait_for_username(const autofill::PasswordFormFillData& r) {
    return r.wait_for_username;
  }

  static bool is_possible_change_password_form(
      const autofill::PasswordFormFillData& r) {
    return r.is_possible_change_password_form;
  }

  static bool Read(autofill::mojom::PasswordFormFillDataDataView data,
                   autofill::PasswordFormFillData* out);
};

template <>
struct StructTraits<autofill::mojom::PasswordFormGenerationDataDataView,
                    autofill::PasswordFormGenerationData> {
  static uint64_t form_signature(
      const autofill::PasswordFormGenerationData& r) {
    return r.form_signature;
  }

  static uint32_t field_signature(
      const autofill::PasswordFormGenerationData& r) {
    return r.field_signature;
  }

  static bool Read(autofill::mojom::PasswordFormGenerationDataDataView data,
                   autofill::PasswordFormGenerationData* out);
};

template <>
struct StructTraits<autofill::mojom::PasswordFormDataView,
                    autofill::PasswordForm> {
  static autofill::PasswordForm::Scheme scheme(
      const autofill::PasswordForm& r) {
    return r.scheme;
  }

  static const std::string& signon_realm(const autofill::PasswordForm& r) {
    return r.signon_realm;
  }

  static const GURL& origin_with_path(const autofill::PasswordForm& r) {
    return r.origin;
  }

  static const GURL& action(const autofill::PasswordForm& r) {
    return r.action;
  }

  static const std::string& affiliated_web_realm(
      const autofill::PasswordForm& r) {
    return r.affiliated_web_realm;
  }

  static const base::string16& submit_element(const autofill::PasswordForm& r) {
    return r.submit_element;
  }

  static const base::string16& username_element(
      const autofill::PasswordForm& r) {
    return r.username_element;
  }

  static bool username_marked_by_site(const autofill::PasswordForm& r) {
    return r.username_marked_by_site;
  }

  static const base::string16& username_value(const autofill::PasswordForm& r) {
    return r.username_value;
  }

  static const std::vector<base::string16>& other_possible_usernames(
      const autofill::PasswordForm& r) {
    return r.other_possible_usernames;
  }

  static const base::string16& password_element(
      const autofill::PasswordForm& r) {
    return r.password_element;
  }

  static const base::string16& password_value(const autofill::PasswordForm& r) {
    return r.password_value;
  }

  static bool password_value_is_default(const autofill::PasswordForm& r) {
    return r.password_value_is_default;
  }

  static const base::string16& new_password_element(
      const autofill::PasswordForm& r) {
    return r.new_password_element;
  }

  static const base::string16& new_password_value(
      const autofill::PasswordForm& r) {
    return r.new_password_value;
  }

  static bool new_password_value_is_default(const autofill::PasswordForm& r) {
    return r.new_password_value_is_default;
  }

  static bool new_password_marked_by_site(const autofill::PasswordForm& r) {
    return r.new_password_marked_by_site;
  }

  static bool preferred(const autofill::PasswordForm& r) { return r.preferred; }

  static const base::Time& date_created(const autofill::PasswordForm& r) {
    return r.date_created;
  }

  static const base::Time& date_synced(const autofill::PasswordForm& r) {
    return r.date_synced;
  }

  static bool blacklisted_by_user(const autofill::PasswordForm& r) {
    return r.blacklisted_by_user;
  }

  static autofill::PasswordForm::Type type(const autofill::PasswordForm& r) {
    return r.type;
  }

  static int32_t times_used(const autofill::PasswordForm& r) {
    return r.times_used;
  }

  static const autofill::FormData& form_data(const autofill::PasswordForm& r) {
    return r.form_data;
  }

  static autofill::PasswordForm::GenerationUploadStatus
  generation_upload_status(const autofill::PasswordForm& r) {
    return r.generation_upload_status;
  }

  static const base::string16& display_name(const autofill::PasswordForm& r) {
    return r.display_name;
  }

  static const GURL& icon_url(const autofill::PasswordForm& r) {
    return r.icon_url;
  }

  static const url::Origin& federation_origin(const autofill::PasswordForm& r) {
    return r.federation_origin;
  }

  static bool skip_zero_click(const autofill::PasswordForm& r) {
    return r.skip_zero_click;
  }

  static autofill::PasswordForm::Layout layout(
      const autofill::PasswordForm& r) {
    return r.layout;
  }

  static bool was_parsed_using_autofill_predictions(
      const autofill::PasswordForm& r) {
    return r.was_parsed_using_autofill_predictions;
  }

  static bool is_public_suffix_match(const autofill::PasswordForm& r) {
    return r.is_public_suffix_match;
  }

  static bool is_affiliation_based_match(const autofill::PasswordForm& r) {
    return r.is_affiliation_based_match;
  }

  static bool does_look_like_signup_form(const autofill::PasswordForm& r) {
    return r.does_look_like_signup_form;
  }

  static bool Read(autofill::mojom::PasswordFormDataView data,
                   autofill::PasswordForm* out);
};

template <>
struct StructTraits<autofill::mojom::PasswordFormFieldPredictionMapDataView,
                    autofill::PasswordFormFieldPredictionMap> {
  using KeysValuesPair =
      std::pair<std::vector<autofill::FormFieldData>,
                std::vector<autofill::PasswordFormFieldPredictionType>>;

  static void* SetUpContext(const autofill::PasswordFormFieldPredictionMap& r);

  static void TearDownContext(const autofill::PasswordFormFieldPredictionMap& r,
                              void* context);

  static const std::vector<autofill::FormFieldData>& keys(
      const autofill::PasswordFormFieldPredictionMap& r,
      void* context) {
    return static_cast<KeysValuesPair*>(context)->first;
  }

  static const std::vector<autofill::PasswordFormFieldPredictionType>& values(
      const autofill::PasswordFormFieldPredictionMap& r,
      void* context) {
    return static_cast<KeysValuesPair*>(context)->second;
  }

  static bool Read(autofill::mojom::PasswordFormFieldPredictionMapDataView data,
                   autofill::PasswordFormFieldPredictionMap* out);
};

template <>
struct StructTraits<autofill::mojom::FormsPredictionsMapDataView,
                    autofill::FormsPredictionsMap> {
  using KeysValuesPair =
      std::pair<std::vector<autofill::FormData>,
                std::vector<autofill::PasswordFormFieldPredictionMap>>;

  static void* SetUpContext(const autofill::FormsPredictionsMap& r);

  static void TearDownContext(const autofill::FormsPredictionsMap& r,
                              void* context);

  static const std::vector<autofill::FormData>& keys(
      const autofill::FormsPredictionsMap& r,
      void* context) {
    return static_cast<KeysValuesPair*>(context)->first;
  }

  static const std::vector<autofill::PasswordFormFieldPredictionMap>& values(
      const autofill::FormsPredictionsMap& r,
      void* context) {
    return static_cast<KeysValuesPair*>(context)->second;
  }

  static bool Read(autofill::mojom::FormsPredictionsMapDataView data,
                   autofill::FormsPredictionsMap* out);
};

}  // namespace mojo

#endif  // COMPONENTS_AUTOFILL_CONTENT_COMMON_AUTOFILL_TYPES_STRUCT_TRAITS_H_
