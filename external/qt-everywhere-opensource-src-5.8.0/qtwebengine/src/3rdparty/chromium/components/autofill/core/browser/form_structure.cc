// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_structure.h"

#include <stdint.h>

#include <algorithm>
#include <map>
#include <utility>

#include "base/command_line.h"
#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_candidates.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_field.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_predictions.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/form_field_data_predictions.h"
#include "components/rappor/rappor_service.h"
#include "components/rappor/rappor_utils.h"
#include "third_party/re2/src/re2/re2.h"

namespace autofill {
namespace {

const char kClientVersion[] = "6.1.1715.1442/en (GGLL)";
const char kBillingMode[] = "billing";
const char kShippingMode[] = "shipping";

// Strip away >= 5 consecutive digits.
const char kIgnorePatternInFieldName[] = "\\d{5,}";

// A form is considered to have a high prediction mismatch rate if the number of
// mismatches exceeds this threshold.
const int kNumberOfMismatchesThreshold = 3;

// Only removing common name prefixes if we have a minimum number of fields and
// a minimum prefix length. These values are chosen to avoid cases such as two
// fields with "address1" and "address2" and be effective against web frameworks
// which prepend prefixes such as "ctl01$ctl00$MainContentRegion$" on all
// fields.
const int kCommonNamePrefixRemovalFieldThreshold = 3;
const int kMinCommonNamePrefixLength = 16;

// Maximum number of characters in the field label to be encoded in a proto.
const int kMaxFieldLabelNumChars = 200;

// Helper for |EncodeUploadRequest()| that creates a bit field corresponding to
// |available_field_types| and returns the hex representation as a string.
std::string EncodeFieldTypes(const ServerFieldTypeSet& available_field_types) {
  // There are |MAX_VALID_FIELD_TYPE| different field types and 8 bits per byte,
  // so we need ceil(MAX_VALID_FIELD_TYPE / 8) bytes to encode the bit field.
  const size_t kNumBytes = (MAX_VALID_FIELD_TYPE + 0x7) / 8;

  // Pack the types in |available_field_types| into |bit_field|.
  std::vector<uint8_t> bit_field(kNumBytes, 0);
  for (const auto& field_type : available_field_types) {
    // Set the appropriate bit in the field.  The bit we set is the one
    // |field_type| % 8 from the left of the byte.
    const size_t byte = field_type / 8;
    const size_t bit = 0x80 >> (field_type % 8);
    DCHECK(byte < bit_field.size());
    bit_field[byte] |= bit;
  }

  // Discard any trailing zeroes.
  // If there are no available types, we return the empty string.
  size_t data_end = bit_field.size();
  for (; data_end > 0 && !bit_field[data_end - 1]; --data_end) {
  }

  // Print all meaningfull bytes into a string.
  std::string data_presence;
  data_presence.reserve(data_end * 2 + 1);
  for (size_t i = 0; i < data_end; ++i) {
    base::StringAppendF(&data_presence, "%02x", bit_field[i]);
  }

  return data_presence;
}

// Returns |true| iff the |token| is a type hint for a contact field, as
// specified in the implementation section of http://is.gd/whatwg_autocomplete
// Note that "fax" and "pager" are intentionally ignored, as Chrome does not
// support filling either type of information.
bool IsContactTypeHint(const std::string& token) {
  return token == "home" || token == "work" || token == "mobile";
}

// Returns |true| iff the |token| is a type hint appropriate for a field of the
// given |field_type|, as specified in the implementation section of
// http://is.gd/whatwg_autocomplete
bool ContactTypeHintMatchesFieldType(const std::string& token,
                                     HtmlFieldType field_type) {
  // The "home" and "work" type hints are only appropriate for email and phone
  // number field types.
  if (token == "home" || token == "work") {
    return field_type == HTML_TYPE_EMAIL ||
        (field_type >= HTML_TYPE_TEL &&
         field_type <= HTML_TYPE_TEL_LOCAL_SUFFIX);
  }

  // The "mobile" type hint is only appropriate for phone number field types.
  // Note that "fax" and "pager" are intentionally ignored, as Chrome does not
  // support filling either type of information.
  if (token == "mobile") {
    return field_type >= HTML_TYPE_TEL &&
        field_type <= HTML_TYPE_TEL_LOCAL_SUFFIX;
  }

  return false;
}

// Returns the Chrome Autofill-supported field type corresponding to the given
// |autocomplete_attribute_value|, if there is one, in the context of the given
// |field|.  Chrome Autofill supports a subset of the field types listed at
// http://is.gd/whatwg_autocomplete
HtmlFieldType FieldTypeFromAutocompleteAttributeValue(
    const std::string& autocomplete_attribute_value,
    const AutofillField& field) {
  if (autocomplete_attribute_value == "")
    return HTML_TYPE_UNSPECIFIED;

  if (autocomplete_attribute_value == "name")
    return HTML_TYPE_NAME;

  if (autocomplete_attribute_value == "given-name")
    return HTML_TYPE_GIVEN_NAME;

  if (autocomplete_attribute_value == "additional-name") {
    if (field.max_length == 1)
      return HTML_TYPE_ADDITIONAL_NAME_INITIAL;
    else
      return HTML_TYPE_ADDITIONAL_NAME;
  }

  if (autocomplete_attribute_value == "family-name")
    return HTML_TYPE_FAMILY_NAME;

  if (autocomplete_attribute_value == "organization")
    return HTML_TYPE_ORGANIZATION;

  if (autocomplete_attribute_value == "street-address")
    return HTML_TYPE_STREET_ADDRESS;

  if (autocomplete_attribute_value == "address-line1")
    return HTML_TYPE_ADDRESS_LINE1;

  if (autocomplete_attribute_value == "address-line2")
    return HTML_TYPE_ADDRESS_LINE2;

  if (autocomplete_attribute_value == "address-line3")
    return HTML_TYPE_ADDRESS_LINE3;

  // TODO(estade): remove support for "locality" and "region".
  if (autocomplete_attribute_value == "locality")
    return HTML_TYPE_ADDRESS_LEVEL2;

  if (autocomplete_attribute_value == "region")
    return HTML_TYPE_ADDRESS_LEVEL1;

  if (autocomplete_attribute_value == "address-level1")
    return HTML_TYPE_ADDRESS_LEVEL1;

  if (autocomplete_attribute_value == "address-level2")
    return HTML_TYPE_ADDRESS_LEVEL2;

  if (autocomplete_attribute_value == "address-level3")
    return HTML_TYPE_ADDRESS_LEVEL3;

  if (autocomplete_attribute_value == "country")
    return HTML_TYPE_COUNTRY_CODE;

  if (autocomplete_attribute_value == "country-name")
    return HTML_TYPE_COUNTRY_NAME;

  if (autocomplete_attribute_value == "postal-code")
    return HTML_TYPE_POSTAL_CODE;

  // content_switches.h isn't accessible from here, hence we have
  // to copy the string literal. This should be removed soon anyway.
  if (autocomplete_attribute_value == "address" &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          "enable-experimental-web-platform-features")) {
    return HTML_TYPE_FULL_ADDRESS;
  }

  if (autocomplete_attribute_value == "cc-name")
    return HTML_TYPE_CREDIT_CARD_NAME_FULL;

  if (autocomplete_attribute_value == "cc-given-name")
    return HTML_TYPE_CREDIT_CARD_NAME_FIRST;

  if (autocomplete_attribute_value == "cc-family-name")
    return HTML_TYPE_CREDIT_CARD_NAME_LAST;

  if (autocomplete_attribute_value == "cc-number")
    return HTML_TYPE_CREDIT_CARD_NUMBER;

  if (autocomplete_attribute_value == "cc-exp") {
    if (field.max_length == 5)
      return HTML_TYPE_CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR;
    else if (field.max_length == 7)
      return HTML_TYPE_CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR;
    else
      return HTML_TYPE_CREDIT_CARD_EXP;
  }

  if (autocomplete_attribute_value == "cc-exp-month")
    return HTML_TYPE_CREDIT_CARD_EXP_MONTH;

  if (autocomplete_attribute_value == "cc-exp-year") {
    if (field.max_length == 2)
      return HTML_TYPE_CREDIT_CARD_EXP_2_DIGIT_YEAR;
    else if (field.max_length == 4)
      return HTML_TYPE_CREDIT_CARD_EXP_4_DIGIT_YEAR;
    else
      return HTML_TYPE_CREDIT_CARD_EXP_YEAR;
  }

  if (autocomplete_attribute_value == "cc-csc")
    return HTML_TYPE_CREDIT_CARD_VERIFICATION_CODE;

  if (autocomplete_attribute_value == "cc-type")
    return HTML_TYPE_CREDIT_CARD_TYPE;

  if (autocomplete_attribute_value == "transaction-amount")
    return HTML_TYPE_TRANSACTION_AMOUNT;

  if (autocomplete_attribute_value == "transaction-currency")
    return HTML_TYPE_TRANSACTION_CURRENCY;

  if (autocomplete_attribute_value == "tel")
    return HTML_TYPE_TEL;

  if (autocomplete_attribute_value == "tel-country-code")
    return HTML_TYPE_TEL_COUNTRY_CODE;

  if (autocomplete_attribute_value == "tel-national")
    return HTML_TYPE_TEL_NATIONAL;

  if (autocomplete_attribute_value == "tel-area-code")
    return HTML_TYPE_TEL_AREA_CODE;

  if (autocomplete_attribute_value == "tel-local")
    return HTML_TYPE_TEL_LOCAL;

  if (autocomplete_attribute_value == "tel-local-prefix")
    return HTML_TYPE_TEL_LOCAL_PREFIX;

  if (autocomplete_attribute_value == "tel-local-suffix")
    return HTML_TYPE_TEL_LOCAL_SUFFIX;

  if (autocomplete_attribute_value == "tel-extension")
    return HTML_TYPE_TEL_EXTENSION;

  if (autocomplete_attribute_value == "email")
    return HTML_TYPE_EMAIL;

  return HTML_TYPE_UNRECOGNIZED;
}

std::string StripDigitsIfRequired(const base::string16& input) {
  std::string return_string = base::UTF16ToUTF8(input);

  re2::RE2::GlobalReplace(&return_string, re2::RE2(kIgnorePatternInFieldName),
                          std::string());
  return return_string;
}

std::ostream& operator<<(
    std::ostream& out,
    const autofill::AutofillQueryResponseContents& response) {
  out << "upload_required: " << response.upload_required();
  for (const auto& field : response.field()) {
    out << "\nautofill_type: " << field.autofill_type();
  }
  return out;
}

}  // namespace

FormStructure::FormStructure(const FormData& form)
    : form_name_(form.name),
      source_url_(form.origin),
      target_url_(form.action),
      autofill_count_(0),
      active_field_count_(0),
      upload_required_(USE_UPLOAD_RATES),
      has_author_specified_types_(false),
      has_author_specified_sections_(false),
      was_parsed_for_autocomplete_attributes_(false),
      has_password_field_(false),
      is_form_tag_(form.is_form_tag),
      is_formless_checkout_(form.is_formless_checkout),
      all_fields_are_passwords_(true) {
  // Copy the form fields.
  std::map<base::string16, size_t> unique_names;
  for (const FormFieldData& field : form.fields) {
    if (!ShouldSkipField(field)) {
      // Add all supported form fields (including with empty names) to the
      // signature.  This is a requirement for Autofill servers.
      form_signature_field_names_.append("&");
      form_signature_field_names_.append(StripDigitsIfRequired(field.name));

      ++active_field_count_;
    }

    if (field.form_control_type == "password")
      has_password_field_ = true;
    else
      all_fields_are_passwords_ = false;

    // Generate a unique name for this field by appending a counter to the name.
    // Make sure to prepend the counter with a non-numeric digit so that we are
    // guaranteed to avoid collisions.
    base::string16 unique_name =
        field.name + base::ASCIIToUTF16("_") +
        base::SizeTToString16(++unique_names[field.name]);
    fields_.push_back(new AutofillField(field, unique_name));
  }

  // Do further processing on the fields, as needed.
  ProcessExtractedFields();
}

FormStructure::~FormStructure() {}

void FormStructure::DetermineHeuristicTypes() {
  const auto determine_heuristic_types_start_time = base::TimeTicks::Now();

  // First, try to detect field types based on each field's |autocomplete|
  // attribute value.
  if (!was_parsed_for_autocomplete_attributes_)
    ParseFieldTypesFromAutocompleteAttributes();

  // Then if there are enough active fields, and if we are dealing with either a
  // proper <form> or a <form>-less checkout, run the heuristics and server
  // prediction routines.
  if (active_field_count() >= kRequiredFieldsForPredictionRoutines &&
      (is_form_tag_ || is_formless_checkout_)) {
    const FieldCandidatesMap field_type_map =
        FormField::ParseFormFields(fields_.get(), is_form_tag_);
    for (AutofillField* field : fields_) {
      const auto iter = field_type_map.find(field->unique_name());
      if (iter != field_type_map.end())
        field->set_heuristic_type(iter->second.BestHeuristicType());
    }
  }

  UpdateAutofillCount();
  IdentifySections(has_author_specified_sections_);

  if (IsAutofillable()) {
    AutofillMetrics::LogDeveloperEngagementMetric(
        AutofillMetrics::FILLABLE_FORM_PARSED);
    if (has_author_specified_types_) {
      AutofillMetrics::LogDeveloperEngagementMetric(
          AutofillMetrics::FILLABLE_FORM_CONTAINS_TYPE_HINTS);
    }
  }

  AutofillMetrics::LogDetermineHeuristicTypesTiming(
      base::TimeTicks::Now() - determine_heuristic_types_start_time);
}

bool FormStructure::EncodeUploadRequest(
    const ServerFieldTypeSet& available_field_types,
    bool form_was_autofilled,
    const std::string& login_form_signature,
    bool observed_submission,
    AutofillUploadContents* upload) const {
  DCHECK(ShouldBeCrowdsourced());

  // Verify that |available_field_types| agrees with the possible field types we
  // are uploading.
  for (const AutofillField* field : *this) {
    for (const auto& type : field->possible_types()) {
      DCHECK(type == UNKNOWN_TYPE || type == EMPTY_TYPE ||
             available_field_types.count(type));
    }
  }

  upload->set_submission(observed_submission);
  upload->set_client_version(kClientVersion);
  upload->set_form_signature(FormSignature64Bit());
  upload->set_autofill_used(form_was_autofilled);
  upload->set_data_present(EncodeFieldTypes(available_field_types));

  if (IsAutofillFieldMetadataEnabled()) {
    upload->set_action_signature(Hash64Bit(target_url_.host()));
    if (!form_name().empty())
      upload->set_form_name(base::UTF16ToUTF8(form_name()));
  }

  if (!login_form_signature.empty()) {
    uint64_t login_sig;
    if (base::StringToUint64(login_form_signature, &login_sig))
      upload->set_login_form_signature(login_sig);
  }

  if (IsMalformed())
    return false;  // Malformed form, skip it.

  EncodeFormForUpload(upload);
  return true;
}

// static
bool FormStructure::EncodeQueryRequest(
    const std::vector<FormStructure*>& forms,
    std::vector<std::string>* encoded_signatures,
    AutofillQueryContents* query) {
  DCHECK(encoded_signatures);
  encoded_signatures->clear();
  encoded_signatures->reserve(forms.size());

  query->set_client_version(kClientVersion);

  // Some badly formatted web sites repeat forms - detect that and encode only
  // one form as returned data would be the same for all the repeated forms.
  std::set<std::string> processed_forms;
  for (const auto& form : forms) {
    std::string signature(form->FormSignature());
    if (processed_forms.find(signature) != processed_forms.end())
      continue;
    processed_forms.insert(signature);
    if (form->IsMalformed())
      continue;

    form->EncodeFormForQuery(query->add_form());

    encoded_signatures->push_back(signature);
  }

  if (!encoded_signatures->size())
    return false;

  return true;
}

// static
void FormStructure::ParseQueryResponse(std::string payload,
                                       const std::vector<FormStructure*>& forms,
                                       rappor::RapporService* rappor_service) {
  AutofillMetrics::LogServerQueryMetric(
      AutofillMetrics::QUERY_RESPONSE_RECEIVED);

  // Parse the response.
  AutofillQueryResponseContents response;
  if (!response.ParseFromString(payload))
    return;

  VLOG(1) << "Autofill query response was successfully parsed:\n" << response;

  AutofillMetrics::LogServerQueryMetric(AutofillMetrics::QUERY_RESPONSE_PARSED);

  bool heuristics_detected_fillable_field = false;
  bool query_response_overrode_heuristics = false;

  // Copy the field types into the actual form.
  auto current_field = response.field().begin();
  for (FormStructure* form : forms) {
    form->upload_required_ =
        response.upload_required() ? UPLOAD_REQUIRED : UPLOAD_NOT_REQUIRED;

    bool query_response_has_no_server_data = true;
    for (AutofillField* field : form->fields_) {
      if (form->ShouldSkipField(*field))
        continue;

      // In some cases *successful* response does not return all the fields.
      // Quit the update of the types then.
      if (current_field == response.field().end())
        break;

      ServerFieldType field_type =
          static_cast<ServerFieldType>(current_field->autofill_type());
      query_response_has_no_server_data &= field_type == NO_SERVER_DATA;

      // UNKNOWN_TYPE is reserved for use by the client.
      DCHECK_NE(field_type, UNKNOWN_TYPE);

      ServerFieldType heuristic_type = field->heuristic_type();
      if (heuristic_type != UNKNOWN_TYPE)
        heuristics_detected_fillable_field = true;

      field->set_server_type(field_type);
      if (heuristic_type != field->Type().GetStorableType())
        query_response_overrode_heuristics = true;

      ++current_field;
    }

    AutofillMetrics::LogServerResponseHasDataForForm(
        !query_response_has_no_server_data);
    if (query_response_has_no_server_data && form->source_url().is_valid()) {
      rappor::SampleDomainAndRegistryFromGURL(
          rappor_service, "Autofill.QueryResponseHasNoServerDataForForm",
          form->source_url());
    }

    form->UpdateAutofillCount();
    form->IdentifySections(false);
  }

  AutofillMetrics::ServerQueryMetric metric;
  if (query_response_overrode_heuristics) {
    if (heuristics_detected_fillable_field) {
      metric = AutofillMetrics::QUERY_RESPONSE_OVERRODE_LOCAL_HEURISTICS;
    } else {
      metric = AutofillMetrics::QUERY_RESPONSE_WITH_NO_LOCAL_HEURISTICS;
    }
  } else {
    metric = AutofillMetrics::QUERY_RESPONSE_MATCHED_LOCAL_HEURISTICS;
  }
  AutofillMetrics::LogServerQueryMetric(metric);
}

// static
std::vector<FormDataPredictions> FormStructure::GetFieldTypePredictions(
    const std::vector<FormStructure*>& form_structures) {
  std::vector<FormDataPredictions> forms;
  forms.reserve(form_structures.size());
  for (const FormStructure* form_structure : form_structures) {
    FormDataPredictions form;
    form.data.name = form_structure->form_name_;
    form.data.origin = form_structure->source_url_;
    form.data.action = form_structure->target_url_;
    form.data.is_form_tag = form_structure->is_form_tag_;
    form.signature = form_structure->FormSignature();

    for (const AutofillField* field : form_structure->fields_) {
      form.data.fields.push_back(FormFieldData(*field));

      FormFieldDataPredictions annotated_field;
      annotated_field.signature = field->FieldSignature();
      annotated_field.heuristic_type =
          AutofillType(field->heuristic_type()).ToString();
      annotated_field.server_type =
          AutofillType(field->server_type()).ToString();
      annotated_field.overall_type = field->Type().ToString();
      annotated_field.parseable_name =
          base::UTF16ToUTF8(field->parseable_name());
      form.fields.push_back(annotated_field);
    }

    forms.push_back(form);
  }
  return forms;
}

// static
bool FormStructure::IsAutofillFieldMetadataEnabled() {
  const std::string group_name =
      base::FieldTrialList::FindFullName("AutofillFieldMetadata");
  return base::StartsWith(group_name, "Enabled", base::CompareCase::SENSITIVE);
}

std::string FormStructure::FormSignature() const {
  return base::Uint64ToString(FormSignature64Bit());
}

bool FormStructure::IsAutofillable() const {
  if (autofill_count() < kRequiredFieldsForPredictionRoutines)
    return false;

  return ShouldBeParsed();
}

void FormStructure::UpdateAutofillCount() {
  autofill_count_ = 0;
  for (const AutofillField* field : *this) {
    if (field && field->IsFieldFillable())
      ++autofill_count_;
  }
}

bool FormStructure::ShouldBeParsed() const {
  if (active_field_count() < kRequiredFieldsForPredictionRoutines &&
      (!all_fields_are_passwords() ||
       active_field_count() < kRequiredFieldsForFormsWithOnlyPasswordFields) &&
      !has_author_specified_types_) {
    return false;
  }

  // Rule out http(s)://*/search?...
  //  e.g. http://www.google.com/search?q=...
  //       http://search.yahoo.com/search?p=...
  if (target_url_.path() == "/search")
    return false;

  bool has_text_field = false;
  for (const AutofillField* it : *this) {
    has_text_field |= it->form_control_type != "select-one";
  }

  return has_text_field;
}

bool FormStructure::ShouldBeCrowdsourced() const {
  return (has_password_field_ ||
          active_field_count() >= kRequiredFieldsForPredictionRoutines) &&
         ShouldBeParsed();
}

void FormStructure::UpdateFromCache(const FormStructure& cached_form) {
  // Map from field signatures to cached fields.
  std::map<std::string, const AutofillField*> cached_fields;
  for (size_t i = 0; i < cached_form.field_count(); ++i) {
    const AutofillField* field = cached_form.field(i);
    cached_fields[field->FieldSignature()] = field;
  }

  for (AutofillField* field : *this) {
    std::map<std::string, const AutofillField*>::const_iterator
        cached_field = cached_fields.find(field->FieldSignature());
    if (cached_field != cached_fields.end()) {
      if (field->form_control_type != "select-one" &&
          field->value == cached_field->second->value) {
        // From the perspective of learning user data, text fields containing
        // default values are equivalent to empty fields.
        field->value = base::string16();
      }

      // Transfer attributes of the cached AutofillField to the newly created
      // AutofillField.
      field->set_heuristic_type(cached_field->second->heuristic_type());
      field->set_server_type(cached_field->second->server_type());
      field->SetHtmlType(cached_field->second->html_type(),
                         cached_field->second->html_mode());
      field->set_previously_autofilled(
          cached_field->second->previously_autofilled());
      field->set_section(cached_field->second->section());
    }
  }

  UpdateAutofillCount();

  // The form signature should match between query and upload requests to the
  // server. On many websites, form elements are dynamically added, removed, or
  // rearranged via JavaScript between page load and form submission, so we
  // copy over the |form_signature_field_names_| corresponding to the query
  // request.
  DCHECK_EQ(cached_form.form_name_, form_name_);
  DCHECK_EQ(cached_form.source_url_, source_url_);
  DCHECK_EQ(cached_form.target_url_, target_url_);
  form_signature_field_names_ = cached_form.form_signature_field_names_;
}

void FormStructure::LogQualityMetrics(const base::TimeTicks& load_time,
                                      const base::TimeTicks& interaction_time,
                                      const base::TimeTicks& submission_time,
                                      rappor::RapporService* rappor_service,
                                      bool did_show_suggestions,
                                      bool observed_submission) const {
  size_t num_detected_field_types = 0;
  size_t num_server_mismatches = 0;
  size_t num_heuristic_mismatches = 0;
  size_t num_edited_autofilled_fields = 0;
  bool did_autofill_all_possible_fields = true;
  bool did_autofill_some_possible_fields = false;
  for (size_t i = 0; i < field_count(); ++i) {
    const AutofillField* field = this->field(i);

    // No further logging for password fields.  Those are primarily related to a
    // different feature code path, and so make more sense to track outside of
    // this metric.
    if (field->form_control_type == "password")
      continue;

    // We count fields that were autofilled but later modified, regardless of
    // whether the data now in the field is recognized.
    if (field->previously_autofilled())
      num_edited_autofilled_fields++;

    // No further logging for empty fields nor for fields where the entered data
    // does not appear to already exist in the user's stored Autofill data.
    const ServerFieldTypeSet& field_types = field->possible_types();
    DCHECK(!field_types.empty());
    if (field_types.count(EMPTY_TYPE) || field_types.count(UNKNOWN_TYPE))
      continue;

    ++num_detected_field_types;
    if (field->is_autofilled)
      did_autofill_some_possible_fields = true;
    else
      did_autofill_all_possible_fields = false;

    // Collapse field types that Chrome treats as identical, e.g. home and
    // billing address fields.
    ServerFieldTypeSet collapsed_field_types;
    for (const auto& it : field_types) {
      // Since we currently only support US phone numbers, the (city code + main
      // digits) number is almost always identical to the whole phone number.
      // TODO(isherman): Improve this logic once we add support for
      // international numbers.
      if (it == PHONE_HOME_CITY_AND_NUMBER)
        collapsed_field_types.insert(PHONE_HOME_WHOLE_NUMBER);
      else
        collapsed_field_types.insert(AutofillType(it).GetStorableType());
    }

    // Capture the field's type, if it is unambiguous.
    ServerFieldType field_type = UNKNOWN_TYPE;
    if (collapsed_field_types.size() == 1)
      field_type = *collapsed_field_types.begin();

    ServerFieldType heuristic_type =
        AutofillType(field->heuristic_type()).GetStorableType();
    ServerFieldType server_type =
        AutofillType(field->server_type()).GetStorableType();
    ServerFieldType predicted_type = field->Type().GetStorableType();

    // Log heuristic, server, and overall type quality metrics, independently of
    // whether the field was autofilled.
    const AutofillMetrics::QualityMetricType metric_type =
        observed_submission ? AutofillMetrics::TYPE_SUBMISSION
                            : AutofillMetrics::TYPE_NO_SUBMISSION;
    if (heuristic_type == UNKNOWN_TYPE) {
      AutofillMetrics::LogHeuristicTypePrediction(AutofillMetrics::TYPE_UNKNOWN,
                                                  field_type, metric_type);
    } else if (field_types.count(heuristic_type)) {
      AutofillMetrics::LogHeuristicTypePrediction(AutofillMetrics::TYPE_MATCH,
                                                  field_type, metric_type);
    } else {
      ++num_heuristic_mismatches;
      AutofillMetrics::LogHeuristicTypePrediction(
          AutofillMetrics::TYPE_MISMATCH, field_type, metric_type);
    }

    if (server_type == NO_SERVER_DATA) {
      AutofillMetrics::LogServerTypePrediction(AutofillMetrics::TYPE_UNKNOWN,
                                               field_type, metric_type);
    } else if (field_types.count(server_type)) {
      AutofillMetrics::LogServerTypePrediction(AutofillMetrics::TYPE_MATCH,
                                               field_type, metric_type);
    } else {
      ++num_server_mismatches;
      AutofillMetrics::LogServerTypePrediction(AutofillMetrics::TYPE_MISMATCH,
                                               field_type, metric_type);
    }

    if (predicted_type == UNKNOWN_TYPE) {
      AutofillMetrics::LogOverallTypePrediction(AutofillMetrics::TYPE_UNKNOWN,
                                                field_type, metric_type);
    } else if (field_types.count(predicted_type)) {
      AutofillMetrics::LogOverallTypePrediction(AutofillMetrics::TYPE_MATCH,
                                                field_type, metric_type);
    } else {
      AutofillMetrics::LogOverallTypePrediction(AutofillMetrics::TYPE_MISMATCH,
                                                field_type, metric_type);
    }
  }

  AutofillMetrics::LogNumberOfEditedAutofilledFields(
      num_edited_autofilled_fields, observed_submission);

  // We log "submission" and duration metrics if we are here after observing a
  // submission event.
  if (observed_submission) {
    if (num_detected_field_types < kRequiredFieldsForPredictionRoutines) {
      AutofillMetrics::LogAutofillFormSubmittedState(
          AutofillMetrics::NON_FILLABLE_FORM_OR_NEW_DATA);
    } else {
      if (did_autofill_all_possible_fields) {
        AutofillMetrics::LogAutofillFormSubmittedState(
            AutofillMetrics::FILLABLE_FORM_AUTOFILLED_ALL);
      } else if (did_autofill_some_possible_fields) {
        AutofillMetrics::LogAutofillFormSubmittedState(
            AutofillMetrics::FILLABLE_FORM_AUTOFILLED_SOME);
      } else if (!did_show_suggestions) {
        AutofillMetrics::LogAutofillFormSubmittedState(
            AutofillMetrics::
                FILLABLE_FORM_AUTOFILLED_NONE_DID_NOT_SHOW_SUGGESTIONS);
      } else {
        AutofillMetrics::LogAutofillFormSubmittedState(
            AutofillMetrics::
                FILLABLE_FORM_AUTOFILLED_NONE_DID_SHOW_SUGGESTIONS);
      }

      // Log some RAPPOR metrics for problematic cases.
      if (num_server_mismatches >= kNumberOfMismatchesThreshold) {
        rappor::SampleDomainAndRegistryFromGURL(
            rappor_service, "Autofill.HighNumberOfServerMismatches",
            source_url_);
      }
      if (num_heuristic_mismatches >= kNumberOfMismatchesThreshold) {
        rappor::SampleDomainAndRegistryFromGURL(
            rappor_service, "Autofill.HighNumberOfHeuristicMismatches",
            source_url_);
      }

      // Unlike the other times, the |submission_time| should always be
      // available.
      DCHECK(!submission_time.is_null());

      // The |load_time| might be unset, in the case that the form was
      // dynamically
      // added to the DOM.
      if (!load_time.is_null()) {
        // Submission should always chronologically follow form load.
        DCHECK(submission_time > load_time);
        base::TimeDelta elapsed = submission_time - load_time;
        if (did_autofill_some_possible_fields)
          AutofillMetrics::LogFormFillDurationFromLoadWithAutofill(elapsed);
        else
          AutofillMetrics::LogFormFillDurationFromLoadWithoutAutofill(elapsed);
      }

      // The |interaction_time| might be unset, in the case that the user
      // submitted a blank form.
      if (!interaction_time.is_null()) {
        // Submission should always chronologically follow interaction.
        DCHECK(submission_time > interaction_time);
        base::TimeDelta elapsed = submission_time - interaction_time;
        if (did_autofill_some_possible_fields) {
          AutofillMetrics::LogFormFillDurationFromInteractionWithAutofill(
              elapsed);
        } else {
          AutofillMetrics::LogFormFillDurationFromInteractionWithoutAutofill(
              elapsed);
        }
      }
    }
  }
}

void FormStructure::LogQualityMetricsBasedOnAutocomplete() const {
  for (const AutofillField* field : fields_) {
    if (field->html_type() != HTML_TYPE_UNSPECIFIED &&
        field->html_type() != HTML_TYPE_UNRECOGNIZED) {
      // The type inferred by the autocomplete attribute.
      AutofillType type(field->html_type(), field->html_mode());
      ServerFieldType actual_field_type = type.GetStorableType();

      const AutofillMetrics::QualityMetricType metric_type =
          AutofillMetrics::TYPE_AUTOCOMPLETE_BASED;
      // Log the quality of our heuristics predictions.
      if (field->heuristic_type() == UNKNOWN_TYPE) {
        AutofillMetrics::LogHeuristicTypePrediction(
            AutofillMetrics::TYPE_UNKNOWN, actual_field_type, metric_type);
      } else if (field->heuristic_type() == actual_field_type) {
        AutofillMetrics::LogHeuristicTypePrediction(
            AutofillMetrics::TYPE_MATCH, actual_field_type, metric_type);
      } else {
        AutofillMetrics::LogHeuristicTypePrediction(
            AutofillMetrics::TYPE_MISMATCH, actual_field_type, metric_type);
      }

      // Log the quality of our server predictions.
      if (field->server_type() == NO_SERVER_DATA) {
        AutofillMetrics::LogServerTypePrediction(
            AutofillMetrics::TYPE_UNKNOWN, actual_field_type, metric_type);
      } else if (field->server_type() == actual_field_type) {
        AutofillMetrics::LogServerTypePrediction(
            AutofillMetrics::TYPE_MATCH, actual_field_type, metric_type);
      } else {
        AutofillMetrics::LogServerTypePrediction(
            AutofillMetrics::TYPE_MISMATCH, actual_field_type, metric_type);
      }
    }
  }
}

void FormStructure::ParseFieldTypesFromAutocompleteAttributes() {
  const std::string kDefaultSection = "-default";

  has_author_specified_types_ = false;
  has_author_specified_sections_ = false;
  for (AutofillField* field : fields_) {
    // To prevent potential section name collisions, add a default suffix for
    // other fields.  Without this, 'autocomplete' attribute values
    // "section--shipping street-address" and "shipping street-address" would be
    // parsed identically, given the section handling code below.  We do this
    // before any validation so that fields with invalid attributes still end up
    // in the default section.  These default section names will be overridden
    // by subsequent heuristic parsing steps if there are no author-specified
    // section names.
    field->set_section(kDefaultSection);

    // Canonicalize the attribute value by trimming whitespace, collapsing
    // non-space characters (e.g. tab) to spaces, and converting to lowercase.
    std::string autocomplete_attribute =
        base::CollapseWhitespaceASCII(field->autocomplete_attribute, false);
    autocomplete_attribute = base::ToLowerASCII(autocomplete_attribute);

    // The autocomplete attribute is overloaded: it can specify either a field
    // type hint or whether autocomplete should be enabled at all.  Ignore the
    // latter type of attribute value.
    if (autocomplete_attribute.empty() || autocomplete_attribute == "on" ||
        autocomplete_attribute == "off") {
      continue;
    }

    // Any other value, even it is invalid, is considered to be a type hint.
    // This allows a website's author to specify an attribute like
    // autocomplete="other" on a field to disable all Autofill heuristics for
    // the form.
    has_author_specified_types_ = true;

    // Tokenize the attribute value.  Per the spec, the tokens are parsed in
    // reverse order.
    std::vector<std::string> tokens =
        base::SplitString(autocomplete_attribute, " ", base::KEEP_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY);

    // The final token must be the field type.
    // If it is not one of the known types, abort.
    DCHECK(!tokens.empty());
    std::string field_type_token = tokens.back();
    tokens.pop_back();
    HtmlFieldType field_type =
        FieldTypeFromAutocompleteAttributeValue(field_type_token, *field);
    if (field_type == HTML_TYPE_UNSPECIFIED)
      continue;

    // The preceding token, if any, may be a type hint.
    if (!tokens.empty() && IsContactTypeHint(tokens.back())) {
      // If it is, it must match the field type; otherwise, abort.
      // Note that an invalid token invalidates the entire attribute value, even
      // if the other tokens are valid.
      if (!ContactTypeHintMatchesFieldType(tokens.back(), field_type))
        continue;

      // Chrome Autofill ignores these type hints.
      tokens.pop_back();
    }

    // The preceding token, if any, may be a fixed string that is either
    // "shipping" or "billing".  Chrome Autofill treats these as implicit
    // section name suffixes.
    DCHECK_EQ(kDefaultSection, field->section());
    std::string section = field->section();
    HtmlFieldMode mode = HTML_MODE_NONE;
    if (!tokens.empty()) {
      if (tokens.back() == kShippingMode)
        mode = HTML_MODE_SHIPPING;
      else if (tokens.back() == kBillingMode)
        mode = HTML_MODE_BILLING;
    }

    if (mode != HTML_MODE_NONE) {
      section = "-" + tokens.back();
      tokens.pop_back();
    }

    // The preceding token, if any, may be a named section.
    const std::string kSectionPrefix = "section-";
    if (!tokens.empty() && base::StartsWith(tokens.back(), kSectionPrefix,
                                            base::CompareCase::SENSITIVE)) {
      // Prepend this section name to the suffix set in the preceding block.
      section = tokens.back().substr(kSectionPrefix.size()) + section;
      tokens.pop_back();
    }

    // No other tokens are allowed.  If there are any remaining, abort.
    if (!tokens.empty())
      continue;

    if (section != kDefaultSection) {
      has_author_specified_sections_ = true;
      field->set_section(section);
    }

    // No errors encountered while parsing!
    // Update the |field|'s type based on what was parsed from the attribute.
    field->SetHtmlType(field_type, mode);
  }

  was_parsed_for_autocomplete_attributes_ = true;
}

bool FormStructure::FillFields(
    const std::vector<ServerFieldType>& types,
    const InputFieldComparator& matches,
    const base::Callback<base::string16(const AutofillType&)>& get_info,
    const std::string& address_language_code,
    const std::string& app_locale) {
  bool filled_something = false;
  for (size_t i = 0; i < field_count(); ++i) {
    for (size_t j = 0; j < types.size(); ++j) {
      if (matches.Run(types[j], *field(i))) {
        AutofillField::FillFormField(*field(i), get_info.Run(field(i)->Type()),
                                     address_language_code, app_locale,
                                     field(i));
        filled_something = true;
        break;
      }
    }
  }
  return filled_something;
}

std::set<base::string16> FormStructure::PossibleValues(ServerFieldType type) {
  std::set<base::string16> values;
  AutofillType target_type(type);
  for (const AutofillField* field : fields_) {
    if (field->Type().GetStorableType() != target_type.GetStorableType() ||
        field->Type().group() != target_type.group()) {
      continue;
    }

    // No option values; anything goes.
    if (field->option_values.empty()) {
      values.clear();
      break;
    }

    for (const base::string16& val : field->option_values) {
      if (!val.empty())
        values.insert(base::i18n::ToUpper(val));
    }

    for (const base::string16& content : field->option_contents) {
      if (!content.empty())
        values.insert(base::i18n::ToUpper(content));
    }
  }

  return values;
}

base::string16 FormStructure::GetUniqueValue(HtmlFieldType type) const {
  base::string16 value;
  for (const AutofillField* field : fields_) {
    if (field->html_type() != type)
      continue;

    // More than one value found; abort rather than choosing one arbitrarily.
    if (!value.empty() && !field->value.empty()) {
      value.clear();
      break;
    }

    value = field->value;
  }

  return value;
}

const AutofillField* FormStructure::field(size_t index) const {
  if (index >= fields_.size()) {
    NOTREACHED();
    return NULL;
  }

  return fields_[index];
}

AutofillField* FormStructure::field(size_t index) {
  return const_cast<AutofillField*>(
      static_cast<const FormStructure*>(this)->field(index));
}

size_t FormStructure::field_count() const {
  return fields_.size();
}

size_t FormStructure::active_field_count() const {
  return active_field_count_;
}

FormData FormStructure::ToFormData() const {
  FormData data;
  data.name = form_name_;
  data.origin = source_url_;
  data.action = target_url_;

  for (size_t i = 0; i < fields_.size(); ++i) {
    data.fields.push_back(FormFieldData(*fields_[i]));
  }

  return data;
}

bool FormStructure::operator==(const FormData& form) const {
  // TODO(jhawkins): Is this enough to differentiate a form?
  if (form_name_ == form.name && source_url_ == form.origin &&
      target_url_ == form.action) {
    return true;
  }

  // TODO(jhawkins): Compare field names, IDs and labels once we have labels
  // set up.

  return false;
}

bool FormStructure::operator!=(const FormData& form) const {
  return !operator==(form);
}

void FormStructure::EncodeFormForQuery(
    AutofillQueryContents::Form* query_form) const {
  DCHECK(!IsMalformed());

  query_form->set_signature(FormSignature64Bit());
  for (const AutofillField* field : fields_) {
    if (ShouldSkipField(*field))
      continue;

    AutofillQueryContents::Form::Field* added_field = query_form->add_field();
    unsigned sig = 0;

    // The signature is a required field. If it can't be parsed, the proto would
    // not serialize.
    if (!base::StringToUint(field->FieldSignature(), &sig))
      continue;
    added_field->set_signature(sig);

    if (IsAutofillFieldMetadataEnabled()) {
      added_field->set_type(field->form_control_type);

      if (!field->name.empty())
        added_field->set_name(base::UTF16ToUTF8(field->name));

      if (!field->label.empty()) {
        std::string truncated;
        base::TruncateUTF8ToByteSize(base::UTF16ToUTF8(field->label),
                                     kMaxFieldLabelNumChars, &truncated);
        added_field->set_label(truncated);
      }
    }
  }
}

void FormStructure::EncodeFormForUpload(AutofillUploadContents* upload) const {
  DCHECK(!IsMalformed());

  for (const AutofillField* field : fields_) {
    // Don't upload checkable fields.
    if (IsCheckable(field->check_status))
      continue;

    const ServerFieldTypeSet& types = field->possible_types();
    for (const auto& field_type : types) {
      // Add the same field elements as the query and a few more below.
      if (ShouldSkipField(*field))
        continue;

      AutofillUploadContents::Field* added_field = upload->add_field();
      added_field->set_autofill_type(field_type);
      if (field->generation_type())
        added_field->set_generation_type(field->generation_type());

      unsigned sig = 0;
      // The signature is a required field. If it can't be parsed, the proto
      // would not serialize.
      if (!base::StringToUint(field->FieldSignature(), &sig))
        continue;
      added_field->set_signature(sig);

      if (IsAutofillFieldMetadataEnabled()) {
        added_field->set_type(field->form_control_type);

        if (!field->name.empty())
          added_field->set_name(base::UTF16ToUTF8(field->name));

        if (!field->label.empty()) {
          std::string truncated;
          base::TruncateUTF8ToByteSize(base::UTF16ToUTF8(field->label),
                                       kMaxFieldLabelNumChars, &truncated);
          added_field->set_label(truncated);
        }

        if (!field->autocomplete_attribute.empty())
          added_field->set_autocomplete(field->autocomplete_attribute);

        if (!field->css_classes.empty())
          added_field->set_css_classes(base::UTF16ToUTF8(field->css_classes));
      }
    }
  }
}

uint64_t FormStructure::Hash64Bit(const std::string& str) {
  std::string hash_bin = base::SHA1HashString(str);
  DCHECK_EQ(base::kSHA1Length, hash_bin.length());

  uint64_t hash64 = (((static_cast<uint64_t>(hash_bin[0])) & 0xFF) << 56) |
                    (((static_cast<uint64_t>(hash_bin[1])) & 0xFF) << 48) |
                    (((static_cast<uint64_t>(hash_bin[2])) & 0xFF) << 40) |
                    (((static_cast<uint64_t>(hash_bin[3])) & 0xFF) << 32) |
                    (((static_cast<uint64_t>(hash_bin[4])) & 0xFF) << 24) |
                    (((static_cast<uint64_t>(hash_bin[5])) & 0xFF) << 16) |
                    (((static_cast<uint64_t>(hash_bin[6])) & 0xFF) << 8) |
                    ((static_cast<uint64_t>(hash_bin[7])) & 0xFF);

  return hash64;
}

uint64_t FormStructure::FormSignature64Bit() const {
  std::string scheme(target_url_.scheme());
  std::string host(target_url_.host());

  // If target host or scheme is empty, set scheme and host of source url.
  // This is done to match the Toolbar's behavior.
  if (scheme.empty() || host.empty()) {
    scheme = source_url_.scheme();
    host = source_url_.host();
  }

  std::string form_string = scheme + "://" + host + "&" +
                            base::UTF16ToUTF8(form_name_) +
                            form_signature_field_names_;

  return Hash64Bit(form_string);
}

bool FormStructure::IsMalformed() const {
  if (!field_count())  // Nothing to add.
    return true;

  // Some badly formatted web sites repeat fields - limit number of fields to
  // 48, which is far larger than any valid form and proto still fits into 2K.
  // Do not send requests for forms with more than this many fields, as they are
  // near certainly not valid/auto-fillable.
  const size_t kMaxFieldsOnTheForm = 48;
  if (field_count() > kMaxFieldsOnTheForm)
    return true;
  return false;
}

void FormStructure::IdentifySections(bool has_author_specified_sections) {
  if (fields_.empty())
    return;

  if (!has_author_specified_sections) {
    // Name sections after the first field in the section.
    base::string16 current_section = fields_.front()->unique_name();

    // Keep track of the types we've seen in this section.
    std::set<ServerFieldType> seen_types;
    ServerFieldType previous_type = UNKNOWN_TYPE;

    for (AutofillField* field : fields_) {
      const ServerFieldType current_type = field->Type().GetStorableType();

      bool already_saw_current_type = seen_types.count(current_type) > 0;

      // Forms often ask for multiple phone numbers -- e.g. both a daytime and
      // evening phone number.  Our phone number detection is also generally a
      // little off.  Hence, ignore this field type as a signal here.
      if (AutofillType(current_type).group() == PHONE_HOME)
        already_saw_current_type = false;

      // Ignore non-focusable field and presentation role fields while inferring
      // boundaries between sections.
      bool ignored_field = !field->is_focusable ||
          field->role == FormFieldData::ROLE_ATTRIBUTE_PRESENTATION;
      if (ignored_field)
        already_saw_current_type = false;

      // Some forms have adjacent fields of the same type.  Two common examples:
      //  * Forms with two email fields, where the second is meant to "confirm"
      //    the first.
      //  * Forms with a <select> menu for states in some countries, and a
      //    freeform <input> field for states in other countries.  (Usually,
      //    only one of these two will be visible for any given choice of
      //    country.)
      // Generally, adjacent fields of the same type belong in the same logical
      // section.
      if (current_type == previous_type)
        already_saw_current_type = false;

      if (current_type != UNKNOWN_TYPE && already_saw_current_type) {
        // We reached the end of a section, so start a new section.
        seen_types.clear();
        current_section = field->unique_name();
      }

      // Only consider a type "seen" if it was not ignored. Some forms have
      // sections for different locales, only one of which is enabled at a
      // time. Each section may duplicate some information (e.g. postal code)
      // and we don't want that to cause section splits.
      // Also only set |previous_type| when the field was not ignored. This
      // prevents ignored fields from breaking up fields that are otherwise
      // adjacent.
      if (!ignored_field) {
        seen_types.insert(current_type);
        previous_type = current_type;
      }

      field->set_section(base::UTF16ToUTF8(current_section));
    }
  }

  // Ensure that credit card and address fields are in separate sections.
  // This simplifies the section-aware logic in autofill_manager.cc.
  for (AutofillField* field : fields_) {
    FieldTypeGroup field_type_group = field->Type().group();
    if (field_type_group == CREDIT_CARD)
      field->set_section(field->section() + "-cc");
    else
      field->set_section(field->section() + "-default");
  }
}

bool FormStructure::ShouldSkipField(const FormFieldData& field) const {
  return IsCheckable(field.check_status);
}

void FormStructure::ProcessExtractedFields() {
  // Update the field name parsed by heuristics if several criteria are met.
  // Several fields must be present in the form.
  if (field_count() < kCommonNamePrefixRemovalFieldThreshold)
    return;

  // Find the longest common prefix within all the field names.
  std::vector<base::string16> names;
  names.reserve(field_count());
  for (const AutofillField* field : *this)
    names.push_back(field->name);

  const base::string16 longest_prefix = FindLongestCommonPrefix(names);
  if (longest_prefix.size() < kMinCommonNamePrefixLength)
    return;

  // The name without the prefix will be used for heuristics parsing.
  for (AutofillField* field : *this) {
    if (field->name.size() > longest_prefix.size()) {
      field->set_parseable_name(
          field->name.substr(longest_prefix.size(), field->name.size()));
    }
  }
}

// static
base::string16 FormStructure::FindLongestCommonPrefix(
    const std::vector<base::string16>& strings) {
  if (strings.empty())
    return base::string16();

  std::vector<base::string16> filtered_strings;

  // Any strings less than kMinCommonNamePrefixLength are neither modified
  // nor considered when processing for a common prefix.
  std::copy_if(
      strings.begin(), strings.end(), std::back_inserter(filtered_strings),
      [](base::string16 s) { return s.size() >= kMinCommonNamePrefixLength; });

  if (filtered_strings.empty())
    return base::string16();

  // Go through each character of the first string until there is a mismatch at
  // the same position in any other string. Adapted from http://goo.gl/YGukMM.
  for (size_t prefix_len = 0; prefix_len < filtered_strings[0].size();
       prefix_len++) {
    for (size_t i = 1; i < filtered_strings.size(); i++) {
      if (prefix_len >= filtered_strings[i].size() ||
          filtered_strings[i].at(prefix_len) !=
              filtered_strings[0].at(prefix_len)) {
        // Mismatch found.
        return filtered_strings[i].substr(0, prefix_len);
      }
    }
  }
  return filtered_strings[0];
}

}  // namespace autofill
