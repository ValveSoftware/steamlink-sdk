// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/template_url.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/format_macros.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/google/core/browser/google_util.h"
#include "components/metrics/proto/omnibox_input_type.pb.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/url_formatter/url_formatter.h"
#include "google_apis/google_api_keys.h"
#include "net/base/escape.h"
#include "net/base/mime_util.h"
#include "ui/base/device_form_factor.h"
#include "url/gurl.h"

namespace {

// The TemplateURLRef has any number of terms that need to be replaced. Each of
// the terms is enclosed in braces. If the character preceeding the final
// brace is a ?, it indicates the term is optional and can be replaced with
// an empty string.
const char kStartParameter = '{';
const char kEndParameter = '}';
const char kOptional = '?';

// Known parameters found in the URL.
const char kSearchTermsParameter[] = "searchTerms";
const char kSearchTermsParameterFull[] = "{searchTerms}";
const char kSearchTermsParameterFullEscaped[] = "%7BsearchTerms%7D";

// Same as kSearchTermsParameter, with no escaping.
const char kGoogleUnescapedSearchTermsParameter[] =
    "google:unescapedSearchTerms";
const char kGoogleUnescapedSearchTermsParameterFull[] =
    "{google:unescapedSearchTerms}";

// Display value for kSearchTermsParameter.
const char kDisplaySearchTerms[] = "%s";

// Display value for kGoogleUnescapedSearchTermsParameter.
const char kDisplayUnescapedSearchTerms[] = "%S";

// Used if the count parameter is not optional. Indicates we want 10 search
// results.
const char kDefaultCount[] = "10";

// Used if the output encoding parameter is required.
const char kOutputEncodingType[] = "UTF-8";

// Attempts to encode |terms| and |original_query| in |encoding| and escape
// them.  |terms| may be escaped as path or query depending on |is_in_query|;
// |original_query| is always escaped as query.  Returns whether the encoding
// process succeeded.
bool TryEncoding(const base::string16& terms,
                 const base::string16& original_query,
                 const char* encoding,
                 bool is_in_query,
                 base::string16* escaped_terms,
                 base::string16* escaped_original_query) {
  DCHECK(escaped_terms);
  DCHECK(escaped_original_query);
  std::string encoded_terms;
  if (!base::UTF16ToCodepage(terms, encoding,
      base::OnStringConversionError::SKIP, &encoded_terms))
    return false;
  *escaped_terms = base::UTF8ToUTF16(is_in_query ?
      net::EscapeQueryParamValue(encoded_terms, true) :
      net::EscapePath(encoded_terms));
  if (original_query.empty())
    return true;
  std::string encoded_original_query;
  if (!base::UTF16ToCodepage(original_query, encoding,
      base::OnStringConversionError::SKIP, &encoded_original_query))
    return false;
  *escaped_original_query = base::UTF8ToUTF16(
      net::EscapeQueryParamValue(encoded_original_query, true));
  return true;
}

// Returns true if the search term placeholder is present, and also produces
// the constant prefix/suffix found.
bool TryMatchSearchParam(base::StringPiece text,
                         base::StringPiece pattern,
                         std::string* prefix,
                         std::string* suffix) {
  auto pos = text.find(pattern);
  if (pos == base::StringPiece::npos)
    return false;
  text.substr(0, pos).CopyToString(prefix);
  text.substr(pos + pattern.length()).CopyToString(suffix);
  return true;
}

// Extract query key and host given a list of parameters coming from the URL
// query or ref.
struct SearchTermsKeyResult {
  std::string key;
  std::string value_prefix;
  std::string value_suffix;
  bool found() const { return !key.empty(); }
};
SearchTermsKeyResult FindSearchTermsKey(const std::string& params) {
  SearchTermsKeyResult result;
  if (params.empty())
    return result;
  url::Component query, key, value;
  query.len = static_cast<int>(params.size());
  while (url::ExtractQueryKeyValue(params.c_str(), &query, &key, &value)) {
    if (key.is_nonempty() && value.is_nonempty()) {
      const base::StringPiece value_string(params.c_str() + value.begin,
                                           value.len);
      if (TryMatchSearchParam(value_string, kSearchTermsParameterFull,
                              &result.value_prefix, &result.value_suffix) ||
          TryMatchSearchParam(value_string,
                              kGoogleUnescapedSearchTermsParameterFull,
                              &result.value_prefix, &result.value_suffix)) {
        result.key = params.substr(key.begin, key.len);
        break;
      }
    }
  }
  return result;
}

// Extract the position of the search terms' parameter in the URL path.
bool FindSearchTermsInPath(const std::string& path,
                           url::Component* parameter_position) {
  DCHECK(parameter_position);
  parameter_position->reset();
  const size_t begin = path.find(kSearchTermsParameterFullEscaped);
  if (begin == std::string::npos)
    return false;
  parameter_position->begin = begin;
  parameter_position->len = arraysize(kSearchTermsParameterFullEscaped) - 1;
  return true;
}

bool IsTemplateParameterString(const std::string& param) {
  return (param.length() > 2) && (*(param.begin()) == kStartParameter) &&
      (*(param.rbegin()) == kEndParameter);
}

}  // namespace


// TemplateURLRef::SearchTermsArgs --------------------------------------------

TemplateURLRef::SearchTermsArgs::SearchTermsArgs(
    const base::string16& search_terms)
    : search_terms(search_terms),
      input_type(metrics::OmniboxInputType::INVALID),
      accepted_suggestion(NO_SUGGESTIONS_AVAILABLE),
      cursor_position(base::string16::npos),
      page_classification(metrics::OmniboxEventProto::INVALID_SPEC),
      append_extra_query_params(false),
      force_instant_results(false),
      from_app_list(false),
      contextual_search_params(ContextualSearchParams()) {}

TemplateURLRef::SearchTermsArgs::SearchTermsArgs(const SearchTermsArgs& other) =
    default;

TemplateURLRef::SearchTermsArgs::~SearchTermsArgs() {
}

TemplateURLRef::SearchTermsArgs::ContextualSearchParams::
    ContextualSearchParams()
    : version(-1),
      start(base::string16::npos),
      end(base::string16::npos),
      resolve(true) {
}

TemplateURLRef::SearchTermsArgs::ContextualSearchParams::
    ContextualSearchParams(
        const int version,
        const std::string& selection,
        const std::string& base_page_url,
        const bool resolve)
    : version(version),
      start(base::string16::npos),
      end(base::string16::npos),
      selection(selection),
      base_page_url(base_page_url),
      resolve(resolve) {
}

TemplateURLRef::SearchTermsArgs::ContextualSearchParams::
    ContextualSearchParams(
        const int version,
        const size_t start,
        const size_t end,
        const std::string& selection,
        const std::string& content,
        const std::string& base_page_url,
        const std::string& encoding,
        const bool resolve)
    : version(version),
      start(start),
      end(end),
      selection(selection),
      content(content),
      base_page_url(base_page_url),
      encoding(encoding),
      resolve(resolve) {
}

TemplateURLRef::SearchTermsArgs::ContextualSearchParams::ContextualSearchParams(
    const ContextualSearchParams& other) = default;

TemplateURLRef::SearchTermsArgs::ContextualSearchParams::
    ~ContextualSearchParams() {
}

// TemplateURLRef -------------------------------------------------------------

TemplateURLRef::TemplateURLRef(const TemplateURL* owner, Type type)
    : owner_(owner),
      type_(type),
      index_in_owner_(0),
      parsed_(false),
      valid_(false),
      supports_replacements_(false),
      search_term_position_in_path_(std::string::npos),
      search_term_key_location_(url::Parsed::QUERY),
      prepopulated_(false) {
  DCHECK(owner_);
  DCHECK_NE(INDEXED, type_);
}

TemplateURLRef::TemplateURLRef(const TemplateURL* owner, size_t index_in_owner)
    : owner_(owner),
      type_(INDEXED),
      index_in_owner_(index_in_owner),
      parsed_(false),
      valid_(false),
      supports_replacements_(false),
      search_term_position_in_path_(std::string::npos),
      search_term_key_location_(url::Parsed::QUERY),
      prepopulated_(false) {
  DCHECK(owner_);
  DCHECK_LT(index_in_owner_, owner_->alternate_urls().size());
}

TemplateURLRef::~TemplateURLRef() {
}

TemplateURLRef::TemplateURLRef(const TemplateURLRef& source) = default;

TemplateURLRef& TemplateURLRef::operator=(const TemplateURLRef& source) =
    default;

std::string TemplateURLRef::GetURL() const {
  switch (type_) {
    case SEARCH:            return owner_->url();
    case SUGGEST:           return owner_->suggestions_url();
    case INSTANT:           return owner_->instant_url();
    case IMAGE:             return owner_->image_url();
    case NEW_TAB:           return owner_->new_tab_url();
    case CONTEXTUAL_SEARCH: return owner_->contextual_search_url();
    case INDEXED:           return owner_->alternate_urls()[index_in_owner_];
    default:       NOTREACHED(); return std::string();  // NOLINT
  }
}

std::string TemplateURLRef::GetPostParamsString() const {
  switch (type_) {
    case INDEXED:
    case SEARCH:            return owner_->search_url_post_params();
    case SUGGEST:           return owner_->suggestions_url_post_params();
    case INSTANT:           return owner_->instant_url_post_params();
    case NEW_TAB:           return std::string();
    case CONTEXTUAL_SEARCH: return std::string();
    case IMAGE:             return owner_->image_url_post_params();
    default:      NOTREACHED(); return std::string();  // NOLINT
  }
}

bool TemplateURLRef::UsesPOSTMethod(
    const SearchTermsData& search_terms_data) const {
  ParseIfNecessary(search_terms_data);
  return !post_params_.empty();
}

bool TemplateURLRef::EncodeFormData(const PostParams& post_params,
                                    PostContent* post_content) const {
  if (post_params.empty())
    return true;
  if (!post_content)
    return false;

  const char kUploadDataMIMEType[] = "multipart/form-data; boundary=";
  // Each name/value pair is stored in a body part which is preceded by a
  // boundary delimiter line.
  std::string boundary = net::GenerateMimeMultipartBoundary();
  // Sets the content MIME type.
  post_content->first = kUploadDataMIMEType;
  post_content->first += boundary;
  // Encodes the post parameters.
  std::string* post_data = &post_content->second;
  post_data->clear();
  for (const auto& param : post_params) {
    DCHECK(!param.name.empty());
    net::AddMultipartValueForUpload(param.name, param.value, boundary,
                                    param.content_type, post_data);
  }
  net::AddMultipartFinalDelimiterForUpload(boundary, post_data);
  return true;
}

bool TemplateURLRef::SupportsReplacement(
    const SearchTermsData& search_terms_data) const {
  ParseIfNecessary(search_terms_data);
  return valid_ && supports_replacements_;
}

std::string TemplateURLRef::ReplaceSearchTerms(
    const SearchTermsArgs& search_terms_args,
    const SearchTermsData& search_terms_data,
    PostContent* post_content) const {
  ParseIfNecessary(search_terms_data);
  if (!valid_)
    return std::string();

  std::string url(HandleReplacements(search_terms_args, search_terms_data,
                                     post_content));

  GURL gurl(url);
  if (!gurl.is_valid())
    return url;

  std::vector<std::string> query_params;
  if (search_terms_args.append_extra_query_params) {
    std::string extra_params(
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kExtraSearchQueryParams));
    if (!extra_params.empty())
      query_params.push_back(extra_params);
  }
  if (!search_terms_args.suggest_query_params.empty())
    query_params.push_back(search_terms_args.suggest_query_params);
  if (!gurl.query().empty())
    query_params.push_back(gurl.query());

  if (query_params.empty())
    return url;

  GURL::Replacements replacements;
  std::string query_str = base::JoinString(query_params, "&");
  replacements.SetQueryStr(query_str);
  return gurl.ReplaceComponents(replacements).possibly_invalid_spec();
}

bool TemplateURLRef::IsValid(const SearchTermsData& search_terms_data) const {
  ParseIfNecessary(search_terms_data);
  return valid_;
}

base::string16 TemplateURLRef::DisplayURL(
    const SearchTermsData& search_terms_data) const {
  ParseIfNecessary(search_terms_data);
  std::string result(GetURL());
  if (valid_ && !replacements_.empty()) {
    base::ReplaceSubstringsAfterOffset(&result, 0,
                                       kSearchTermsParameterFull,
                                       kDisplaySearchTerms);
    base::ReplaceSubstringsAfterOffset(&result, 0,
                                       kGoogleUnescapedSearchTermsParameterFull,
                                       kDisplayUnescapedSearchTerms);
  }
  return base::UTF8ToUTF16(result);
}

// static
std::string TemplateURLRef::DisplayURLToURLRef(
    const base::string16& display_url) {
  std::string result = base::UTF16ToUTF8(display_url);
  base::ReplaceSubstringsAfterOffset(&result, 0,
                                     kDisplaySearchTerms,
                                     kSearchTermsParameterFull);
  base::ReplaceSubstringsAfterOffset(&result, 0,
                                     kDisplayUnescapedSearchTerms,
                                     kGoogleUnescapedSearchTermsParameterFull);
  return result;
}

const std::string& TemplateURLRef::GetHost(
    const SearchTermsData& search_terms_data) const {
  ParseIfNecessary(search_terms_data);
  return host_;
}

const std::string& TemplateURLRef::GetPath(
    const SearchTermsData& search_terms_data) const {
  ParseIfNecessary(search_terms_data);
  return path_;
}

const std::string& TemplateURLRef::GetSearchTermKey(
    const SearchTermsData& search_terms_data) const {
  ParseIfNecessary(search_terms_data);
  return search_term_key_;
}

size_t TemplateURLRef::GetSearchTermPositionInPath(
    const SearchTermsData& search_terms_data) const {
  ParseIfNecessary(search_terms_data);
  return search_term_position_in_path_;
}

url::Parsed::ComponentType TemplateURLRef::GetSearchTermKeyLocation(
    const SearchTermsData& search_terms_data) const {
  ParseIfNecessary(search_terms_data);
  return search_term_key_location_;
}

base::string16 TemplateURLRef::SearchTermToString16(
    const std::string& term) const {
  const std::vector<std::string>& encodings = owner_->input_encodings();
  base::string16 result;

  net::UnescapeRule::Type unescape_rules =
      net::UnescapeRule::SPACES | net::UnescapeRule::PATH_SEPARATORS |
      net::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS;
  if (search_term_key_location_ != url::Parsed::PATH)
    unescape_rules |= net::UnescapeRule::REPLACE_PLUS_WITH_SPACE;

  std::string unescaped = net::UnescapeURLComponent(term, unescape_rules);
  for (size_t i = 0; i < encodings.size(); ++i) {
    if (base::CodepageToUTF16(unescaped, encodings[i].c_str(),
                              base::OnStringConversionError::FAIL, &result))
      return result;
  }

  // Always fall back on UTF-8 if it works.
  if (base::CodepageToUTF16(unescaped, base::kCodepageUTF8,
                            base::OnStringConversionError::FAIL, &result))
    return result;

  // When nothing worked, just use the escaped text. We have no idea what the
  // encoding is. We need to substitute spaces for pluses ourselves since we're
  // not sending it through an unescaper.
  result = base::UTF8ToUTF16(term);
  if (unescape_rules & net::UnescapeRule::REPLACE_PLUS_WITH_SPACE)
    std::replace(result.begin(), result.end(), '+', ' ');
  return result;
}

bool TemplateURLRef::HasGoogleBaseURLs(
    const SearchTermsData& search_terms_data) const {
  ParseIfNecessary(search_terms_data);
  for (size_t i = 0; i < replacements_.size(); ++i) {
    if ((replacements_[i].type == GOOGLE_BASE_URL) ||
        (replacements_[i].type == GOOGLE_BASE_SUGGEST_URL))
      return true;
  }
  return false;
}

bool TemplateURLRef::ExtractSearchTermsFromURL(
    const GURL& url,
    base::string16* search_terms,
    const SearchTermsData& search_terms_data,
    url::Parsed::ComponentType* search_terms_component,
    url::Component* search_terms_position) const {
  DCHECK(search_terms);
  search_terms->clear();

  ParseIfNecessary(search_terms_data);

  // We need a search term in the template URL to extract something.
  if (search_term_key_.empty() &&
      (search_term_key_location_ != url::Parsed::PATH))
    return false;

  // Host, port, and path must match.
  if ((url.host() != host_) ||
      (url.port() != port_) ||
      ((url.path() != path_) &&
          (search_term_key_location_ != url::Parsed::PATH))) {
    return false;
  }

  std::string source;
  url::Component position;

  if (search_term_key_location_ == url::Parsed::PATH) {
    source = url.path();

    // Characters in the path before and after search terms must match.
    if (source.length() < path_.length())
      return false;
    position.begin = search_term_position_in_path_;
    position.len = source.length() - path_.length();
    if (source.substr(0, position.begin) + source.substr(position.end()) !=
        path_)
      return false;
  } else {
    DCHECK(search_term_key_location_ == url::Parsed::QUERY ||
           search_term_key_location_ == url::Parsed::REF);
    source = (search_term_key_location_ == url::Parsed::QUERY) ?
        url.query() : url.ref();

    url::Component query, key, value;
    query.len = static_cast<int>(source.size());
    bool key_found = false;
    while (url::ExtractQueryKeyValue(source.c_str(), &query, &key, &value)) {
      if (key.is_nonempty()) {
        if (source.substr(key.begin, key.len) == search_term_key_) {
          // Fail if search term key is found twice.
          if (key_found)
            return false;

          // If the query parameter does not contain the expected prefix and
          // suffix, then this is not a match.
          base::StringPiece search_term =
              base::StringPiece(source).substr(value.begin, value.len);
          if (!search_term.starts_with(search_term_value_prefix_) ||
              !search_term.ends_with(search_term_value_suffix_))
            continue;

          key_found = true;
          position =
              url::MakeRange(value.begin + search_term_value_prefix_.size(),
                             value.end() - search_term_value_suffix_.size());
        }
      }
    }
    if (!key_found)
      return false;
  }

  // Extract the search term.
  *search_terms =
      SearchTermToString16(source.substr(position.begin, position.len));
  if (search_terms_component)
    *search_terms_component = search_term_key_location_;
  if (search_terms_position)
    *search_terms_position = position;
  return true;
}

void TemplateURLRef::InvalidateCachedValues() const {
  supports_replacements_ = valid_ = parsed_ = false;
  host_.clear();
  port_.clear();
  path_.clear();
  search_term_key_.clear();
  search_term_position_in_path_ = std::string::npos;
  search_term_key_location_ = url::Parsed::QUERY;
  replacements_.clear();
  post_params_.clear();
}

bool TemplateURLRef::ParseParameter(size_t start,
                                    size_t end,
                                    std::string* url,
                                    Replacements* replacements) const {
  DCHECK(start != std::string::npos &&
         end != std::string::npos && end > start);
  size_t length = end - start - 1;
  bool optional = false;
  // Make a copy of |url| that can be referenced in StringPieces below. |url| is
  // modified, so that can't be used in StringPiece.
  const std::string original_url(*url);
  if (original_url[end - 1] == kOptional) {
    optional = true;
    length--;
  }

  const base::StringPiece parameter(original_url.begin() + start + 1,
                                    original_url.begin() + start + 1 + length);
  const base::StringPiece full_parameter(original_url.begin() + start,
                                         original_url.begin() + end + 1);
  // Remove the parameter from the string.  For parameters who replacement is
  // constant and already known, just replace them directly.  For other cases,
  // like parameters whose values may change over time, use |replacements|.
  url->erase(start, end - start + 1);
  if (parameter == kSearchTermsParameter) {
    replacements->push_back(Replacement(SEARCH_TERMS, start));
  } else if (parameter == "count") {
    if (!optional)
      url->insert(start, kDefaultCount);
  } else if (parameter == "google:assistedQueryStats") {
    replacements->push_back(Replacement(GOOGLE_ASSISTED_QUERY_STATS, start));
  } else if (parameter == "google:baseURL") {
    replacements->push_back(Replacement(GOOGLE_BASE_URL, start));
  } else if (parameter == "google:baseSuggestURL") {
    replacements->push_back(Replacement(GOOGLE_BASE_SUGGEST_URL, start));
  } else if (parameter == "google:currentPageUrl") {
    replacements->push_back(Replacement(GOOGLE_CURRENT_PAGE_URL, start));
  } else if (parameter == "google:cursorPosition") {
    replacements->push_back(Replacement(GOOGLE_CURSOR_POSITION, start));
  } else if (parameter == "google:forceInstantResults") {
    replacements->push_back(Replacement(GOOGLE_FORCE_INSTANT_RESULTS, start));
  } else if (parameter == "google:imageOriginalHeight") {
    replacements->push_back(
        Replacement(TemplateURLRef::GOOGLE_IMAGE_ORIGINAL_HEIGHT, start));
  } else if (parameter == "google:imageOriginalWidth") {
    replacements->push_back(
        Replacement(TemplateURLRef::GOOGLE_IMAGE_ORIGINAL_WIDTH, start));
  } else if (parameter == "google:imageSearchSource") {
    replacements->push_back(
        Replacement(TemplateURLRef::GOOGLE_IMAGE_SEARCH_SOURCE, start));
  } else if (parameter == "google:imageThumbnail") {
    replacements->push_back(
        Replacement(TemplateURLRef::GOOGLE_IMAGE_THUMBNAIL, start));
  } else if (parameter == "google:imageURL") {
    replacements->push_back(Replacement(TemplateURLRef::GOOGLE_IMAGE_URL,
                                        start));
  } else if (parameter == "google:inputType") {
    replacements->push_back(Replacement(TemplateURLRef::GOOGLE_INPUT_TYPE,
                                        start));
  } else if (parameter == "google:instantExtendedEnabledParameter") {
    replacements->push_back(Replacement(GOOGLE_INSTANT_EXTENDED_ENABLED,
                                        start));
  } else if (parameter == "google:instantExtendedEnabledKey") {
    url->insert(start, google_util::kInstantExtendedAPIParam);
  } else if (parameter == "google:iOSSearchLanguage") {
    replacements->push_back(Replacement(GOOGLE_IOS_SEARCH_LANGUAGE, start));
  } else if (parameter == "google:contextualSearchVersion") {
    replacements->push_back(
        Replacement(GOOGLE_CONTEXTUAL_SEARCH_VERSION, start));
  } else if (parameter == "google:contextualSearchContextData") {
    replacements->push_back(
        Replacement(GOOGLE_CONTEXTUAL_SEARCH_CONTEXT_DATA, start));
  } else if (parameter == "google:originalQueryForSuggestion") {
    replacements->push_back(Replacement(GOOGLE_ORIGINAL_QUERY_FOR_SUGGESTION,
                                        start));
  } else if (parameter == "google:pageClassification") {
    replacements->push_back(Replacement(GOOGLE_PAGE_CLASSIFICATION, start));
  } else if (parameter == "google:prefetchQuery") {
    replacements->push_back(Replacement(GOOGLE_PREFETCH_QUERY, start));
  } else if (parameter == "google:RLZ") {
    replacements->push_back(Replacement(GOOGLE_RLZ, start));
  } else if (parameter == "google:searchClient") {
    replacements->push_back(Replacement(GOOGLE_SEARCH_CLIENT, start));
  } else if (parameter == "google:searchFieldtrialParameter") {
    replacements->push_back(Replacement(GOOGLE_SEARCH_FIELDTRIAL_GROUP, start));
  } else if (parameter == "google:searchVersion") {
    replacements->push_back(Replacement(GOOGLE_SEARCH_VERSION, start));
  } else if (parameter == "google:sessionToken") {
    replacements->push_back(Replacement(GOOGLE_SESSION_TOKEN, start));
  } else if (parameter == "google:sourceId") {
#if defined(OS_ANDROID) || defined(OS_IOS)
    url->insert(start, "sourceid=chrome-mobile&");
#else
    url->insert(start, "sourceid=chrome&");
#endif
  } else if (parameter == "google:suggestAPIKeyParameter") {
    url->insert(start,
                net::EscapeQueryParamValue(google_apis::GetAPIKey(), false));
  } else if (parameter == "google:suggestClient") {
    replacements->push_back(Replacement(GOOGLE_SUGGEST_CLIENT, start));
  } else if (parameter == "google:suggestRid") {
    replacements->push_back(Replacement(GOOGLE_SUGGEST_REQUEST_ID, start));
  } else if (parameter == kGoogleUnescapedSearchTermsParameter) {
    replacements->push_back(Replacement(GOOGLE_UNESCAPED_SEARCH_TERMS, start));
  } else if (parameter == "yandex:searchPath") {
    switch (ui::GetDeviceFormFactor()) {
      case ui::DEVICE_FORM_FACTOR_DESKTOP:
        url->insert(start, "search/");
        break;
      case ui::DEVICE_FORM_FACTOR_PHONE:
        url->insert(start, "search/touch/");
        break;
      case ui::DEVICE_FORM_FACTOR_TABLET:
        url->insert(start, "search/pad/");
        break;
    }
  } else if (parameter == "inputEncoding") {
    replacements->push_back(Replacement(ENCODING, start));
  } else if (parameter == "language") {
    replacements->push_back(Replacement(LANGUAGE, start));
  } else if (parameter == "outputEncoding") {
    if (!optional)
      url->insert(start, kOutputEncodingType);
  } else if ((parameter == "startIndex") || (parameter == "startPage")) {
    // We don't support these.
    if (!optional)
      url->insert(start, "1");
  } else if (!prepopulated_) {
    // If it's a prepopulated URL, we know that it's safe to remove unknown
    // parameters, so just ignore this and return true below. Otherwise it could
    // be some garbage but can also be a javascript block. Put it back.
    url->insert(start, full_parameter.data(), full_parameter.size());
    return false;
  }
  return true;
}

std::string TemplateURLRef::ParseURL(const std::string& url,
                                     Replacements* replacements,
                                     PostParams* post_params,
                                     bool* valid) const {
  *valid = false;
  std::string parsed_url = url;
  for (size_t last = 0; last != std::string::npos; ) {
    last = parsed_url.find(kStartParameter, last);
    if (last != std::string::npos) {
      size_t template_end = parsed_url.find(kEndParameter, last);
      if (template_end != std::string::npos) {
        // Since we allow Javascript in the URL, {} pairs could be nested. Match
        // only leaf pairs with supported parameters.
        size_t next_template_start = parsed_url.find(kStartParameter, last + 1);
        if (next_template_start == std::string::npos ||
            next_template_start > template_end) {
          // If successful, ParseParameter erases from the string as such no
          // need to update |last|. If failed, move |last| to the end of pair.
          if (!ParseParameter(last, template_end, &parsed_url, replacements)) {
            // |template_end| + 1 may be beyond the end of the string.
            last = template_end;
          }
        } else {
          last = next_template_start;
        }
      } else {
        // Open brace without a closing brace, return.
        return std::string();
      }
    }
  }

  // Handles the post parameters.
  const std::string& post_params_string = GetPostParamsString();
  if (!post_params_string.empty()) {
    for (const base::StringPiece& cur : base::SplitStringPiece(
             post_params_string, ",",
             base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
      // The '=' delimiter is required and the name must be not empty.
      std::vector<std::string> parts = base::SplitString(
          cur, "=", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
      if ((parts.size() != 2U) || parts[0].empty())
        return std::string();

      std::string& value = parts[1];
      size_t replacements_size = replacements->size();
      if (IsTemplateParameterString(value))
        ParseParameter(0, value.length() - 1, &value, replacements);
      PostParam param = { parts[0], value };
      post_params->push_back(param);
      // If there was a replacement added, points its index to last added
      // PostParam.
      if (replacements->size() > replacements_size) {
        DCHECK_EQ(replacements_size + 1, replacements->size());
        Replacement* r = &replacements->back();
        r->is_post_param = true;
        r->index = post_params->size() - 1;
      }
    }
    DCHECK(!post_params->empty());
  }

  *valid = true;
  return parsed_url;
}

void TemplateURLRef::ParseIfNecessary(
    const SearchTermsData& search_terms_data) const {
  if (!parsed_) {
    InvalidateCachedValues();
    parsed_ = true;
    parsed_url_ = ParseURL(GetURL(), &replacements_, &post_params_, &valid_);
    supports_replacements_ = false;
    if (valid_) {
      bool has_only_one_search_term = false;
      for (Replacements::const_iterator i = replacements_.begin();
           i != replacements_.end(); ++i) {
        if ((i->type == SEARCH_TERMS) ||
            (i->type == GOOGLE_UNESCAPED_SEARCH_TERMS)) {
          if (has_only_one_search_term) {
            has_only_one_search_term = false;
            break;
          }
          has_only_one_search_term = true;
          supports_replacements_ = true;
        }
      }
      // Only parse the host/key if there is one search term. Technically there
      // could be more than one term, but it's uncommon; so we punt.
      if (has_only_one_search_term)
        ParseHostAndSearchTermKey(search_terms_data);
    }
  }
}

void TemplateURLRef::ParseHostAndSearchTermKey(
    const SearchTermsData& search_terms_data) const {
  std::string url_string(GetURL());
  base::ReplaceSubstringsAfterOffset(
      &url_string, 0, "{google:baseURL}",
      search_terms_data.GoogleBaseURLValue());
  base::ReplaceSubstringsAfterOffset(
      &url_string, 0, "{google:baseSuggestURL}",
      search_terms_data.GoogleBaseSuggestURLValue());

  GURL url(url_string);
  if (!url.is_valid())
    return;

  auto query_result = FindSearchTermsKey(url.query());
  auto ref_result = FindSearchTermsKey(url.ref());
  url::Component parameter_position;
  const bool in_query = query_result.found();
  const bool in_ref = ref_result.found();
  const bool in_path = FindSearchTermsInPath(url.path(), &parameter_position);
  if (in_query ? (in_ref || in_path) : (in_ref == in_path))
    return;  // No key or multiple keys found.  We only handle having one key.

  host_ = url.host();
  port_ = url.port();
  path_ = url.path();
  if (in_query) {
    search_term_key_ = query_result.key;
    search_term_key_location_ = url::Parsed::QUERY;
    search_term_value_prefix_ = query_result.value_prefix;
    search_term_value_suffix_ = query_result.value_suffix;
  } else if (in_ref) {
    search_term_key_ = ref_result.key;
    search_term_key_location_ = url::Parsed::REF;
    search_term_value_prefix_ = ref_result.value_prefix;
    search_term_value_suffix_ = ref_result.value_suffix;
  } else {
    DCHECK(in_path);
    DCHECK_GE(parameter_position.begin, 1);  // Path must start with '/'.
    search_term_key_location_ = url::Parsed::PATH;
    search_term_position_in_path_ = parameter_position.begin;
    // Remove the "{searchTerms}" itself from |path_|.
    path_.erase(parameter_position.begin, parameter_position.len);
  }
}

void TemplateURLRef::HandleReplacement(const std::string& name,
                                       const std::string& value,
                                       const Replacement& replacement,
                                       std::string* url) const {
  size_t pos = replacement.index;
  if (replacement.is_post_param) {
    DCHECK_LT(pos, post_params_.size());
    DCHECK(!post_params_[pos].name.empty());
    post_params_[pos].value = value;
  } else {
    url->insert(pos, name.empty() ? value : (name + "=" + value + "&"));
  }
}

std::string TemplateURLRef::HandleReplacements(
    const SearchTermsArgs& search_terms_args,
    const SearchTermsData& search_terms_data,
    PostContent* post_content) const {
  if (replacements_.empty()) {
    if (!post_params_.empty())
      EncodeFormData(post_params_, post_content);
    return parsed_url_;
  }

  // Determine if the search terms are in the query or before. We're escaping
  // space as '+' in the former case and as '%20' in the latter case.
  bool is_in_query = true;
  for (Replacements::iterator i = replacements_.begin();
       i != replacements_.end(); ++i) {
    if (i->type == SEARCH_TERMS) {
      base::string16::size_type query_start = parsed_url_.find('?');
      is_in_query = query_start != base::string16::npos &&
          (static_cast<base::string16::size_type>(i->index) > query_start);
      break;
    }
  }

  std::string input_encoding;
  base::string16 encoded_terms;
  base::string16 encoded_original_query;
  owner_->EncodeSearchTerms(search_terms_args, is_in_query, &input_encoding,
                            &encoded_terms, &encoded_original_query);

  std::string url = parsed_url_;

  // replacements_ is ordered in ascending order, as such we need to iterate
  // from the back.
  for (Replacements::reverse_iterator i = replacements_.rbegin();
       i != replacements_.rend(); ++i) {
    switch (i->type) {
      case ENCODING:
        HandleReplacement(std::string(), input_encoding, *i, &url);
        break;

      case GOOGLE_ASSISTED_QUERY_STATS:
        DCHECK(!i->is_post_param);
        if (!search_terms_args.assisted_query_stats.empty()) {
          // Get the base URL without substituting AQS to avoid infinite
          // recursion.  We need the URL to find out if it meets all
          // AQS requirements (e.g. HTTPS protocol check).
          // See TemplateURLRef::SearchTermsArgs for more details.
          SearchTermsArgs search_terms_args_without_aqs(search_terms_args);
          search_terms_args_without_aqs.assisted_query_stats.clear();
          GURL base_url(ReplaceSearchTerms(
              search_terms_args_without_aqs, search_terms_data, NULL));
          if (base_url.SchemeIsCryptographic()) {
            HandleReplacement(
                "aqs", search_terms_args.assisted_query_stats, *i, &url);
          }
        }
        break;

      case GOOGLE_BASE_URL:
        DCHECK(!i->is_post_param);
        HandleReplacement(
            std::string(), search_terms_data.GoogleBaseURLValue(), *i, &url);
        break;

      case GOOGLE_BASE_SUGGEST_URL:
        DCHECK(!i->is_post_param);
        HandleReplacement(
            std::string(), search_terms_data.GoogleBaseSuggestURLValue(), *i,
            &url);
        break;

      case GOOGLE_CURRENT_PAGE_URL:
        DCHECK(!i->is_post_param);
        if (!search_terms_args.current_page_url.empty()) {
          const std::string& escaped_current_page_url =
              net::EscapeQueryParamValue(search_terms_args.current_page_url,
                                         true);
          HandleReplacement("url", escaped_current_page_url, *i, &url);
        }
        break;

      case GOOGLE_CURSOR_POSITION:
        DCHECK(!i->is_post_param);
        if (search_terms_args.cursor_position != base::string16::npos)
          HandleReplacement(
              "cp",
              base::StringPrintf("%" PRIuS, search_terms_args.cursor_position),
              *i,
              &url);
        break;

      case GOOGLE_FORCE_INSTANT_RESULTS:
        DCHECK(!i->is_post_param);
        HandleReplacement(std::string(),
                          search_terms_data.ForceInstantResultsParam(
                              search_terms_args.force_instant_results),
                          *i,
                          &url);
        break;

      case GOOGLE_INPUT_TYPE:
        DCHECK(!i->is_post_param);
        HandleReplacement(
            "oit", base::IntToString(search_terms_args.input_type), *i, &url);
        break;

      case GOOGLE_INSTANT_EXTENDED_ENABLED:
        DCHECK(!i->is_post_param);
        HandleReplacement(std::string(),
                          search_terms_data.InstantExtendedEnabledParam(
                              type_ == SEARCH),
                          *i,
                          &url);
        break;

      case GOOGLE_CONTEXTUAL_SEARCH_VERSION:
        if (search_terms_args.contextual_search_params.version >= 0) {
          HandleReplacement(
              "ctxs",
              base::IntToString(
                  search_terms_args.contextual_search_params.version),
              *i,
              &url);
        }
        break;

      case GOOGLE_CONTEXTUAL_SEARCH_CONTEXT_DATA: {
        DCHECK(!i->is_post_param);
        std::string context_data;

        const SearchTermsArgs::ContextualSearchParams& params =
            search_terms_args.contextual_search_params;

        if (params.start != std::string::npos) {
          context_data.append("ctxs_start=" +
                              base::SizeTToString(params.start) + "&");
        }

        if (params.end != std::string::npos) {
          context_data.append("ctxs_end=" +
                              base::SizeTToString(params.end) + "&");
        }

        if (!params.selection.empty())
          context_data.append("q=" + params.selection + "&");

        if (!params.content.empty())
          context_data.append("ctxs_content=" + params.content + "&");

        if (!params.base_page_url.empty())
          context_data.append("ctxsl_url=" + params.base_page_url + "&");

        if (!params.encoding.empty()) {
          context_data.append("ctxs_encoding=" + params.encoding + "&");
        }

        context_data.append(
            params.resolve ? "ctxsl_resolve=1" : "ctxsl_resolve=0");

        HandleReplacement(std::string(), context_data, *i, &url);
        break;
      }

      case GOOGLE_ORIGINAL_QUERY_FOR_SUGGESTION:
        DCHECK(!i->is_post_param);
        if (search_terms_args.accepted_suggestion >= 0 ||
            !search_terms_args.assisted_query_stats.empty()) {
          HandleReplacement(
              "oq", base::UTF16ToUTF8(encoded_original_query), *i, &url);
        }
        break;

      case GOOGLE_PAGE_CLASSIFICATION:
        if (search_terms_args.page_classification !=
            metrics::OmniboxEventProto::INVALID_SPEC) {
          HandleReplacement(
              "pgcl", base::IntToString(search_terms_args.page_classification),
              *i, &url);
        }
        break;

      case GOOGLE_PREFETCH_QUERY: {
        const std::string& query = search_terms_args.prefetch_query;
        const std::string& type = search_terms_args.prefetch_query_type;
        if (!query.empty() && !type.empty()) {
          HandleReplacement(
              std::string(), "pfq=" + query + "&qha=" + type + "&", *i, &url);
        }
        break;
      }

      case GOOGLE_RLZ: {
        DCHECK(!i->is_post_param);
        // On platforms that don't have RLZ, we still want this branch
        // to happen so that we replace the RLZ template with the
        // empty string.  (If we don't handle this case, we hit a
        // NOTREACHED below.)
        base::string16 rlz_string = search_terms_data.GetRlzParameterValue(
            search_terms_args.from_app_list);
        if (!rlz_string.empty()) {
          HandleReplacement("rlz", base::UTF16ToUTF8(rlz_string), *i, &url);
        }
        break;
      }

      case GOOGLE_SEARCH_CLIENT: {
        DCHECK(!i->is_post_param);
        std::string client = search_terms_data.GetSearchClient();
        if (!client.empty())
          HandleReplacement("client", client, *i, &url);
        break;
      }

      case GOOGLE_SEARCH_FIELDTRIAL_GROUP:
        // We are not currently running any fieldtrials that modulate the search
        // url.  If we do, then we'd have some conditional insert such as:
        // url.insert(i->index, used_www ? "gcx=w&" : "gcx=c&");
        break;

      case GOOGLE_SEARCH_VERSION:
        HandleReplacement("gs_rn", "42", *i, &url);
        break;

      case GOOGLE_SESSION_TOKEN: {
        std::string token = search_terms_args.session_token;
        if (!token.empty())
          HandleReplacement("psi", token, *i, &url);
        break;
      }

      case GOOGLE_SUGGEST_CLIENT:
        HandleReplacement(
            std::string(), search_terms_data.GetSuggestClient(), *i, &url);
        break;

      case GOOGLE_SUGGEST_REQUEST_ID:
        HandleReplacement(
            std::string(), search_terms_data.GetSuggestRequestIdentifier(), *i,
            &url);
        break;

      case GOOGLE_UNESCAPED_SEARCH_TERMS: {
        std::string unescaped_terms;
        base::UTF16ToCodepage(search_terms_args.search_terms,
                              input_encoding.c_str(),
                              base::OnStringConversionError::SKIP,
                              &unescaped_terms);
        HandleReplacement(std::string(), unescaped_terms, *i, &url);
        break;
      }

      case LANGUAGE:
        HandleReplacement(
            std::string(), search_terms_data.GetApplicationLocale(), *i, &url);
        break;

      case SEARCH_TERMS:
        HandleReplacement(
            std::string(), base::UTF16ToUTF8(encoded_terms), *i, &url);
        break;

      case GOOGLE_IMAGE_THUMBNAIL:
        HandleReplacement(
            std::string(), search_terms_args.image_thumbnail_content, *i, &url);
        post_params_[i->index].content_type = "image/jpeg";
        break;

      case GOOGLE_IMAGE_URL:
        if (search_terms_args.image_url.is_valid()) {
          HandleReplacement(
              std::string(), search_terms_args.image_url.spec(), *i, &url);
        }
        break;

      case GOOGLE_IMAGE_ORIGINAL_WIDTH:
        if (!search_terms_args.image_original_size.IsEmpty()) {
          HandleReplacement(
              std::string(),
              base::IntToString(search_terms_args.image_original_size.width()),
              *i, &url);
        }
        break;

      case GOOGLE_IMAGE_ORIGINAL_HEIGHT:
        if (!search_terms_args.image_original_size.IsEmpty()) {
          HandleReplacement(
              std::string(),
              base::IntToString(search_terms_args.image_original_size.height()),
              *i, &url);
        }
        break;

      case GOOGLE_IMAGE_SEARCH_SOURCE:
        HandleReplacement(
            std::string(), search_terms_data.GoogleImageSearchSource(), *i,
            &url);
        break;

      case GOOGLE_IOS_SEARCH_LANGUAGE:
#if defined(OS_IOS)
        HandleReplacement("hl", search_terms_data.GetApplicationLocale(), *i,
                          &url);
#endif
        break;

      default:
        NOTREACHED();
        break;
    }
  }

  if (!post_params_.empty())
    EncodeFormData(post_params_, post_content);

  return url;
}


// TemplateURL ----------------------------------------------------------------

TemplateURL::AssociatedExtensionInfo::AssociatedExtensionInfo(
    Type type,
    const std::string& extension_id)
    : type(type),
      extension_id(extension_id),
      wants_to_be_default_engine(false) {
  DCHECK_NE(NORMAL, type);
}

TemplateURL::AssociatedExtensionInfo::~AssociatedExtensionInfo() {
}

TemplateURL::TemplateURL(const TemplateURLData& data)
    : data_(data),
      url_ref_(nullptr),
      suggestions_url_ref_(this,
                           TemplateURLRef::SUGGEST),
      instant_url_ref_(this,
                       TemplateURLRef::INSTANT),
      image_url_ref_(this, TemplateURLRef::IMAGE),
      new_tab_url_ref_(this, TemplateURLRef::NEW_TAB),
      contextual_search_url_ref_(this, TemplateURLRef::CONTEXTUAL_SEARCH),
      engine_type_(SEARCH_ENGINE_UNKNOWN) {
  ResizeURLRefVector();
  SetPrepopulateId(data_.prepopulate_id);

  if (data_.search_terms_replacement_key ==
      "{google:instantExtendedEnabledKey}") {
    data_.search_terms_replacement_key = google_util::kInstantExtendedAPIParam;
  }
}

TemplateURL::~TemplateURL() {
}

// static
base::string16 TemplateURL::GenerateKeyword(const GURL& url) {
  DCHECK(url.is_valid());
  // Strip "www." off the front of the keyword; otherwise the keyword won't work
  // properly.  See http://code.google.com/p/chromium/issues/detail?id=6984 .
  // |url|'s hostname may be IDN-encoded. Before generating |keyword| from it,
  // convert to Unicode, so it won't look like a confusing punycode string.
  base::string16 keyword = url_formatter::StripWWW(
      url_formatter::IDNToUnicode(url.host()));
  // Special case: if the host was exactly "www." (not sure this can happen but
  // perhaps with some weird intranet and custom DNS server?), ensure we at
  // least don't return the empty string.
  return keyword.empty() ? base::ASCIIToUTF16("www") : keyword;
}

// static
GURL TemplateURL::GenerateFaviconURL(const GURL& url) {
  DCHECK(url.is_valid());
  GURL::Replacements rep;

  const char favicon_path[] = "/favicon.ico";
  int favicon_path_len = arraysize(favicon_path) - 1;

  rep.SetPath(favicon_path, url::Component(0, favicon_path_len));
  rep.ClearUsername();
  rep.ClearPassword();
  rep.ClearQuery();
  rep.ClearRef();
  return url.ReplaceComponents(rep);
}

// static
bool TemplateURL::MatchesData(const TemplateURL* t_url,
                              const TemplateURLData* data,
                              const SearchTermsData& search_terms_data) {
  if (!t_url || !data)
    return !t_url && !data;

  return (t_url->short_name() == data->short_name()) &&
      t_url->HasSameKeywordAs(*data, search_terms_data) &&
      (t_url->url() == data->url()) &&
      (t_url->suggestions_url() == data->suggestions_url) &&
      (t_url->instant_url() == data->instant_url) &&
      (t_url->image_url() == data->image_url) &&
      (t_url->new_tab_url() == data->new_tab_url) &&
      (t_url->search_url_post_params() == data->search_url_post_params) &&
      (t_url->suggestions_url_post_params() ==
          data->suggestions_url_post_params) &&
      (t_url->instant_url_post_params() == data->instant_url_post_params) &&
      (t_url->image_url_post_params() == data->image_url_post_params) &&
      (t_url->favicon_url() == data->favicon_url) &&
      (t_url->safe_for_autoreplace() == data->safe_for_autoreplace) &&
      (t_url->show_in_default_list() == data->show_in_default_list) &&
      (t_url->input_encodings() == data->input_encodings) &&
      (t_url->alternate_urls() == data->alternate_urls) &&
      (t_url->search_terms_replacement_key() ==
          data->search_terms_replacement_key);
}

base::string16 TemplateURL::AdjustedShortNameForLocaleDirection() const {
  base::string16 bidi_safe_short_name = data_.short_name();
  base::i18n::AdjustStringForLocaleDirection(&bidi_safe_short_name);
  return bidi_safe_short_name;
}

bool TemplateURL::ShowInDefaultList(
    const SearchTermsData& search_terms_data) const {
  return data_.show_in_default_list &&
      url_ref_->SupportsReplacement(search_terms_data);
}

bool TemplateURL::SupportsReplacement(
    const SearchTermsData& search_terms_data) const {
  return url_ref_->SupportsReplacement(search_terms_data);
}

bool TemplateURL::HasGoogleBaseURLs(
    const SearchTermsData& search_terms_data) const {
  for (const TemplateURLRef& ref : url_refs_) {
    if (ref.HasGoogleBaseURLs(search_terms_data))
      return true;
  }
  return suggestions_url_ref_.HasGoogleBaseURLs(search_terms_data) ||
      instant_url_ref_.HasGoogleBaseURLs(search_terms_data) ||
      image_url_ref_.HasGoogleBaseURLs(search_terms_data) ||
      new_tab_url_ref_.HasGoogleBaseURLs(search_terms_data) ||
      contextual_search_url_ref_.HasGoogleBaseURLs(search_terms_data);
}

bool TemplateURL::IsGoogleSearchURLWithReplaceableKeyword(
    const SearchTermsData& search_terms_data) const {
  return (GetType() == NORMAL) &&
      url_ref_->HasGoogleBaseURLs(search_terms_data) &&
      google_util::IsGoogleHostname(base::UTF16ToUTF8(data_.keyword()),
                                    google_util::DISALLOW_SUBDOMAIN);
}

bool TemplateURL::HasSameKeywordAs(
    const TemplateURLData& other,
    const SearchTermsData& search_terms_data) const {
  return (data_.keyword() == other.keyword()) ||
      (IsGoogleSearchURLWithReplaceableKeyword(search_terms_data) &&
       TemplateURL(other).IsGoogleSearchURLWithReplaceableKeyword(
           search_terms_data));
}

TemplateURL::Type TemplateURL::GetType() const {
  return extension_info_ ? extension_info_->type : NORMAL;
}

std::string TemplateURL::GetExtensionId() const {
  DCHECK(extension_info_);
  return extension_info_->extension_id;
}

SearchEngineType TemplateURL::GetEngineType(
    const SearchTermsData& search_terms_data) const {
  if (engine_type_ == SEARCH_ENGINE_UNKNOWN) {
    const GURL url = GenerateSearchURL(search_terms_data);
    engine_type_ = url.is_valid() ?
        TemplateURLPrepopulateData::GetEngineType(url) : SEARCH_ENGINE_OTHER;
    DCHECK_NE(SEARCH_ENGINE_UNKNOWN, engine_type_);
  }
  return engine_type_;
}

bool TemplateURL::ExtractSearchTermsFromURL(
    const GURL& url,
    const SearchTermsData& search_terms_data,
    base::string16* search_terms) const {
  return FindSearchTermsInURL(url, search_terms_data, search_terms, NULL, NULL);
}

bool TemplateURL::IsSearchURL(const GURL& url,
                              const SearchTermsData& search_terms_data) const {
  base::string16 search_terms;
  return ExtractSearchTermsFromURL(url, search_terms_data, &search_terms) &&
      !search_terms.empty();
}

bool TemplateURL::HasSearchTermsReplacementKey(const GURL& url) const {
  // Look for the key both in the query and the ref.
  std::string params[] = {url.query(), url.ref()};

  for (int i = 0; i < 2; ++i) {
    url::Component query, key, value;
    query.len = static_cast<int>(params[i].size());
    while (url::ExtractQueryKeyValue(params[i].c_str(), &query, &key, &value)) {
      if (key.is_nonempty() &&
          params[i].substr(key.begin, key.len) ==
              search_terms_replacement_key()) {
        return true;
      }
    }
  }
  return false;
}

bool TemplateURL::ReplaceSearchTermsInURL(
    const GURL& url,
    const TemplateURLRef::SearchTermsArgs& search_terms_args,
    const SearchTermsData& search_terms_data,
    GURL* result) {
  // TODO(beaudoin): Use AQS from |search_terms_args| too.
  url::Parsed::ComponentType search_term_component;
  url::Component search_terms_position;
  base::string16 search_terms;
  if (!FindSearchTermsInURL(url, search_terms_data, &search_terms,
                            &search_term_component, &search_terms_position)) {
    return false;
  }
  DCHECK(search_terms_position.is_nonempty());

  // Query and ref are encoded in the same way.
  const bool is_in_query = (search_term_component != url::Parsed::PATH);

  std::string input_encoding;
  base::string16 encoded_terms;
  base::string16 encoded_original_query;
  EncodeSearchTerms(search_terms_args, is_in_query, &input_encoding,
                    &encoded_terms, &encoded_original_query);

  std::string old_params;
  if (search_term_component == url::Parsed::QUERY) {
    old_params = url.query();
  } else if (search_term_component == url::Parsed::REF) {
    old_params = url.ref();
  } else {
    DCHECK_EQ(search_term_component, url::Parsed::PATH);
    old_params = url.path();
  }

  std::string new_params(old_params, 0, search_terms_position.begin);
  new_params += base::UTF16ToUTF8(encoded_terms);
  new_params += old_params.substr(search_terms_position.end());
  GURL::Replacements replacements;

  if (search_term_component == url::Parsed::QUERY) {
    replacements.SetQueryStr(new_params);
  } else if (search_term_component == url::Parsed::REF) {
    replacements.SetRefStr(new_params);
  } else {
    DCHECK_EQ(search_term_component, url::Parsed::PATH);
    replacements.SetPathStr(new_params);
  }

  *result = url.ReplaceComponents(replacements);
  return true;
}

void TemplateURL::EncodeSearchTerms(
    const TemplateURLRef::SearchTermsArgs& search_terms_args,
    bool is_in_query,
    std::string* input_encoding,
    base::string16* encoded_terms,
    base::string16* encoded_original_query) const {

  std::vector<std::string> encodings(input_encodings());
  if (std::find(encodings.begin(), encodings.end(), "UTF-8") == encodings.end())
    encodings.push_back("UTF-8");
  for (std::vector<std::string>::const_iterator i(encodings.begin());
       i != encodings.end(); ++i) {
    if (TryEncoding(search_terms_args.search_terms,
                    search_terms_args.original_query, i->c_str(),
                    is_in_query, encoded_terms, encoded_original_query)) {
      *input_encoding = *i;
      return;
    }
  }
  NOTREACHED();
}

GURL TemplateURL::GenerateSearchURL(
    const SearchTermsData& search_terms_data) const {
  if (!url_ref_->IsValid(search_terms_data))
    return GURL();

  if (!url_ref_->SupportsReplacement(search_terms_data))
    return GURL(url());

  // Use something obscure for the search terms argument so that in the rare
  // case the term replaces the URL it's unlikely another keyword would have the
  // same url.
  // TODO(jnd): Add additional parameters to get post data when the search URL
  // has post parameters.
  return GURL(url_ref_->ReplaceSearchTerms(
      TemplateURLRef::SearchTermsArgs(
          base::ASCIIToUTF16("blah.blah.blah.blah.blah")),
      search_terms_data, NULL));
}

void TemplateURL::CopyFrom(const TemplateURL& other) {
  if (this == &other)
    return;

  data_ = other.data_;
  ResizeURLRefVector();
  InvalidateCachedValues();
  SetPrepopulateId(other.data_.prepopulate_id);
}

void TemplateURL::SetURL(const std::string& url) {
  data_.SetURL(url);
  engine_type_ = SEARCH_ENGINE_UNKNOWN;
  url_ref_->InvalidateCachedValues();
}

void TemplateURL::SetPrepopulateId(int id) {
  data_.prepopulate_id = id;
  const bool prepopulated = id > 0;
  for (TemplateURLRef& ref : url_refs_)
    ref.prepopulated_ = prepopulated;
  suggestions_url_ref_.prepopulated_ = prepopulated;
  instant_url_ref_.prepopulated_ = prepopulated;
  image_url_ref_.prepopulated_ = prepopulated;
  new_tab_url_ref_.prepopulated_ = prepopulated;
  contextual_search_url_ref_.prepopulated_ = prepopulated;
}

void TemplateURL::ResetKeywordIfNecessary(
    const SearchTermsData& search_terms_data,
    bool force) {
  if (IsGoogleSearchURLWithReplaceableKeyword(search_terms_data) || force) {
    DCHECK(GetType() != OMNIBOX_API_EXTENSION);
    GURL url(GenerateSearchURL(search_terms_data));
    if (url.is_valid())
      data_.SetKeyword(GenerateKeyword(url));
  }
}

void TemplateURL::InvalidateCachedValues() const {
  for (const TemplateURLRef& ref : url_refs_)
    ref.InvalidateCachedValues();
  suggestions_url_ref_.InvalidateCachedValues();
  instant_url_ref_.InvalidateCachedValues();
  image_url_ref_.InvalidateCachedValues();
  new_tab_url_ref_.InvalidateCachedValues();
  contextual_search_url_ref_.InvalidateCachedValues();
}

void TemplateURL::ResizeURLRefVector() {
  const size_t new_size = data_.alternate_urls.size() + 1;
  if (url_refs_.size() == new_size)
    return;

  // See comment on TemplateURL::ExtractSearchTermsFromURL() for understanding
  // the order of TemplateURLRefs in the |url_refs_| vector.
  url_refs_.clear();
  url_refs_.reserve(new_size);
  for (size_t i = 0; i != data_.alternate_urls.size(); ++i)
    url_refs_.emplace_back(this, i);
  url_refs_.emplace_back(this, TemplateURLRef::SEARCH);
  url_ref_ = &url_refs_.back();
}

bool TemplateURL::FindSearchTermsInURL(
    const GURL& url,
    const SearchTermsData& search_terms_data,
    base::string16* search_terms,
    url::Parsed::ComponentType* search_term_component,
    url::Component* search_terms_position) const {
  DCHECK(search_terms);
  search_terms->clear();

  // Try to match with every pattern.
  for (const TemplateURLRef& ref : url_refs_) {
    if (ref.ExtractSearchTermsFromURL(url, search_terms, search_terms_data,
        search_term_component, search_terms_position)) {
      // If ExtractSearchTermsFromURL() returns true and |search_terms| is empty
      // it means the pattern matched but no search terms were present. In this
      // case we fail immediately without looking for matches in subsequent
      // patterns. This means that given patterns
      //    [ "http://foo/#q={searchTerms}", "http://foo/?q={searchTerms}" ],
      // calling ExtractSearchTermsFromURL() on "http://foo/?q=bar#q=' would
      // return false. This is important for at least Google, where such URLs
      // are invalid.
      return !search_terms->empty();
    }
  }
  return false;
}
