// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spelling_service_client.h"

#include <stddef.h>

#include <algorithm>

#include "base/json/json_reader.h"
#include "base/json/string_escape.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/spellcheck_common.h"
#include "chrome/common/spellcheck_result.h"
#ifndef TOOLKIT_QT
#include "components/data_use_measurement/core/data_use_user_data.h"
#endif
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/google_api_keys.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_fetcher.h"
#include "url/gurl.h"

namespace {

// The URL for requesting spell checking and sending user feedback.
const char kSpellingServiceURL[] = "https://www.googleapis.com/rpc";

// The location of spellcheck suggestions in JSON response from spelling
// service.
const char kMisspellingsPath[] = "result.spellingCheckResponse.misspellings";

// The location of error messages in JSON response from spelling service.
const char kErrorPath[] = "error";

// Languages currently supported by SPELLCHECK.
const char* const kValidLanguages[] = {"en", "es", "fi", "da"};

}  // namespace

SpellingServiceClient::SpellingServiceClient() {
}

SpellingServiceClient::~SpellingServiceClient() {
  STLDeleteContainerPairPointers(spellcheck_fetchers_.begin(),
                                 spellcheck_fetchers_.end());
}

bool SpellingServiceClient::RequestTextCheck(
    content::BrowserContext* context,
    ServiceType type,
    const base::string16& text,
    const TextCheckCompleteCallback& callback) {
  DCHECK(type == SUGGEST || type == SPELLCHECK);
  if (!context || !IsAvailable(context, type)) {
    callback.Run(false, text, std::vector<SpellCheckResult>());
    return false;
  }

  const PrefService* pref = user_prefs::UserPrefs::Get(context);
  DCHECK(pref);

  std::string dictionary;
  pref->GetList(prefs::kSpellCheckDictionaries)->GetString(0, &dictionary);

  std::string language_code;
  std::string country_code;
  chrome::spellcheck_common::GetISOLanguageCountryCodeFromLocale(
      dictionary, &language_code, &country_code);

  // Replace typographical apostrophes with typewriter apostrophes, so that
  // server word breaker behaves correctly.
  const base::char16 kApostrophe = 0x27;
  const base::char16 kRightSingleQuotationMark = 0x2019;
  base::string16 text_copy = text;
  std::replace(text_copy.begin(), text_copy.end(), kRightSingleQuotationMark,
               kApostrophe);

  // Format the JSON request to be sent to the Spelling service.
  std::string encoded_text = base::GetQuotedJSONString(text_copy);

  static const char kSpellingRequest[] =
      "{"
      "\"method\":\"spelling.check\","
      "\"apiVersion\":\"v%d\","
      "\"params\":{"
      "\"text\":%s,"
      "\"language\":\"%s\","
      "\"originCountry\":\"%s\","
      "\"key\":%s"
      "}"
      "}";
  std::string api_key = base::GetQuotedJSONString(google_apis::GetAPIKey());
  std::string request = base::StringPrintf(
      kSpellingRequest,
      type,
      encoded_text.c_str(),
      language_code.c_str(),
      country_code.c_str(),
      api_key.c_str());

  GURL url = GURL(kSpellingServiceURL);
  net::URLFetcher* fetcher = CreateURLFetcher(url).release();
#ifndef TOOLKIT_QT
  data_use_measurement::DataUseUserData::AttachToFetcher(
      fetcher, data_use_measurement::DataUseUserData::SPELL_CHECKER);
#endif
  fetcher->SetRequestContext(
      content::BrowserContext::GetDefaultStoragePartition(context)->
          GetURLRequestContext());
  fetcher->SetUploadData("application/json", request);
  fetcher->SetLoadFlags(
      net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SAVE_COOKIES);
  spellcheck_fetchers_[fetcher] = new TextCheckCallbackData(callback, text);
  fetcher->Start();
  return true;
}

bool SpellingServiceClient::IsAvailable(
    content::BrowserContext* context,
    ServiceType type) {
  const PrefService* pref = user_prefs::UserPrefs::Get(context);
  DCHECK(pref);
  // If prefs don't allow spellchecking, if the context is off the record, or if
  // multilingual spellchecking is enabled the spelling service should be
  // unavailable.
  if (!pref->GetBoolean(prefs::kEnableContinuousSpellcheck) ||
      !pref->GetBoolean(prefs::kSpellCheckUseSpellingService) ||
      context->IsOffTheRecord())
    return false;

  // If the locale for spelling has not been set, the user has not decided to
  // use spellcheck so we don't do anything remote (suggest or spelling).
  std::string locale;
  pref->GetList(prefs::kSpellCheckDictionaries)->GetString(0, &locale);
  if (locale.empty())
    return false;

  // Finally, if all options are available, we only enable only SUGGEST
  // if SPELLCHECK is not available for our language because SPELLCHECK results
  // are a superset of SUGGEST results.
  for (const char* language : kValidLanguages) {
    if (!locale.compare(0, 2, language))
      return type == SPELLCHECK;
  }

  // Only SUGGEST is allowed.
  return type == SUGGEST;
}

bool SpellingServiceClient::ParseResponse(
    const std::string& data,
    std::vector<SpellCheckResult>* results) {
  // When this JSON-RPC call finishes successfully, the Spelling service returns
  // an JSON object listed below.
  // * result - an envelope object representing the result from the APIARY
  //   server, which is the JSON-API front-end for the Spelling service. This
  //   object consists of the following variable:
  //   - spellingCheckResponse (SpellingCheckResponse).
  // * SpellingCheckResponse - an object representing the result from the
  //   Spelling service. This object consists of the following variable:
  //   - misspellings (optional array of Misspelling)
  // * Misspelling - an object representing a misspelling region and its
  //   suggestions. This object consists of the following variables:
  //   - charStart (number) - the beginning of the misspelled region;
  //   - charLength (number) - the length of the misspelled region;
  //   - suggestions (array of string) - the suggestions for the misspelling
  //     text, and;
  //   - canAutoCorrect (optional boolean) - whether we can use the first
  //     suggestion for auto-correction.
  // For example, the Spelling service returns the following JSON when we send a
  // spelling request for "duck goes quisk" as of 16 August, 2011.
  // {
  //   "result": {
  //     "spellingCheckResponse": {
  //       "misspellings": [{
  //           "charStart": 10,
  //           "charLength": 5,
  //           "suggestions": [{ "suggestion": "quack" }],
  //           "canAutoCorrect": false
  //       }]
  //     }
  //   }
  // }
  // If the service is not available, the Spelling service returns JSON with an
  // error.
  // {
  //   "error": {
  //     "code": 400,
  //     "message": "Bad Request",
  //     "data": [...]
  //   }
  // }
  std::unique_ptr<base::DictionaryValue> value(
      static_cast<base::DictionaryValue*>(
          base::JSONReader::Read(data, base::JSON_ALLOW_TRAILING_COMMAS)
              .release()));
  if (!value.get() || !value->IsType(base::Value::TYPE_DICTIONARY))
    return false;

  // Check for errors from spelling service.
  base::DictionaryValue* error = NULL;
  if (value->GetDictionary(kErrorPath, &error))
    return false;

  // Retrieve the array of Misspelling objects. When the input text does not
  // have misspelled words, it returns an empty JSON. (In this case, its HTTP
  // status is 200.) We just return true for this case.
  base::ListValue* misspellings = NULL;
  if (!value->GetList(kMisspellingsPath, &misspellings))
    return true;

  for (size_t i = 0; i < misspellings->GetSize(); ++i) {
    // Retrieve the i-th misspelling region and put it to the given vector. When
    // the Spelling service sends two or more suggestions, we read only the
    // first one because SpellCheckResult can store only one suggestion.
    base::DictionaryValue* misspelling = NULL;
    if (!misspellings->GetDictionary(i, &misspelling))
      return false;

    int start = 0;
    int length = 0;
    base::ListValue* suggestions = NULL;
    if (!misspelling->GetInteger("charStart", &start) ||
        !misspelling->GetInteger("charLength", &length) ||
        !misspelling->GetList("suggestions", &suggestions)) {
      return false;
    }

    base::DictionaryValue* suggestion = NULL;
    base::string16 replacement;
    if (!suggestions->GetDictionary(0, &suggestion) ||
        !suggestion->GetString("suggestion", &replacement)) {
      return false;
    }
    SpellCheckResult result(
        SpellCheckResult::SPELLING, start, length, replacement);
    results->push_back(result);
  }
  return true;
}

SpellingServiceClient::TextCheckCallbackData::TextCheckCallbackData(
    TextCheckCompleteCallback callback,
    base::string16 text)
      : callback(callback),
        text(text) {
}

SpellingServiceClient::TextCheckCallbackData::~TextCheckCallbackData() {
}

void SpellingServiceClient::OnURLFetchComplete(
    const net::URLFetcher* source) {
  DCHECK(spellcheck_fetchers_[source]);
  std::unique_ptr<const net::URLFetcher> fetcher(source);
  std::unique_ptr<TextCheckCallbackData> callback_data(
      spellcheck_fetchers_[fetcher.get()]);
  bool success = false;
  std::vector<SpellCheckResult> results;
  if (fetcher->GetResponseCode() / 100 == 2) {
    std::string data;
    fetcher->GetResponseAsString(&data);
    success = ParseResponse(data, &results);
  }
  spellcheck_fetchers_.erase(fetcher.get());

  // The callback may release the last (transitive) dependency on |this|. It
  // MUST be the last function called.
  callback_data->callback.Run(success, callback_data->text, results);
}

std::unique_ptr<net::URLFetcher> SpellingServiceClient::CreateURLFetcher(
    const GURL& url) {
  return net::URLFetcher::Create(url, net::URLFetcher::POST, this);
}
