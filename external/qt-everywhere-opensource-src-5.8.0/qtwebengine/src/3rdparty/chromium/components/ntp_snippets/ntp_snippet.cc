// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/ntp_snippet.h"

#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/ntp_snippets/content_suggestion.h"
#include "components/ntp_snippets/content_suggestion_category.h"
#include "components/ntp_snippets/content_suggestions_provider_type.h"
#include "components/ntp_snippets/proto/ntp_snippets.pb.h"

namespace {

// dict.Get() specialization for base::Time values
bool GetTimeValue(const base::DictionaryValue& dict,
                  const std::string& key,
                  base::Time* time) {
  std::string time_value;
  return dict.GetString(key, &time_value) &&
         base::Time::FromString(time_value.c_str(), time);
}

// dict.Get() specialization for GURL values
bool GetURLValue(const base::DictionaryValue& dict,
                 const std::string& key,
                 GURL* url) {
  std::string spec;
  if (!dict.GetString(key, &spec)) {
    return false;
  }
  *url = GURL(spec);
  return url->is_valid();
}

}  // namespace

namespace ntp_snippets {

NTPSnippet::NTPSnippet(const std::string& id)
    : id_(id), score_(0), is_discarded_(false), best_source_index_(0) {}

NTPSnippet::~NTPSnippet() {}

// static
std::unique_ptr<NTPSnippet> NTPSnippet::CreateFromChromeReaderDictionary(
    const base::DictionaryValue& dict) {
  const base::DictionaryValue* content = nullptr;
  if (!dict.GetDictionary("contentInfo", &content))
    return nullptr;

  // Need at least the id.
  std::string id;
  if (!content->GetString("url", &id) || id.empty())
    return nullptr;

  std::unique_ptr<NTPSnippet> snippet(new NTPSnippet(id));

  std::string title;
  if (content->GetString("title", &title))
    snippet->set_title(title);
  std::string salient_image_url;
  if (content->GetString("thumbnailUrl", &salient_image_url))
    snippet->set_salient_image_url(GURL(salient_image_url));
  std::string snippet_str;
  if (content->GetString("snippet", &snippet_str))
    snippet->set_snippet(snippet_str);
  // The creation and expiry timestamps are uint64s which are stored as strings.
  std::string creation_timestamp_str;
  if (content->GetString("creationTimestampSec", &creation_timestamp_str))
    snippet->set_publish_date(TimeFromJsonString(creation_timestamp_str));
  std::string expiry_timestamp_str;
  if (content->GetString("expiryTimestampSec", &expiry_timestamp_str))
    snippet->set_expiry_date(TimeFromJsonString(expiry_timestamp_str));

  const base::ListValue* corpus_infos_list = nullptr;
  if (!content->GetList("sourceCorpusInfo", &corpus_infos_list)) {
    DLOG(WARNING) << "No sources found for article " << title;
    return nullptr;
  }

  for (const auto& value : *corpus_infos_list) {
    const base::DictionaryValue* dict_value = nullptr;
    if (!value->GetAsDictionary(&dict_value)) {
      DLOG(WARNING) << "Invalid source info for article " << id;
      continue;
    }

    std::string corpus_id_str;
    GURL corpus_id;
    if (dict_value->GetString("corpusId", &corpus_id_str))
      corpus_id = GURL(corpus_id_str);

    if (!corpus_id.is_valid()) {
      // We must at least have a valid source URL.
      DLOG(WARNING) << "Invalid article url " << corpus_id_str;
      continue;
    }

    const base::DictionaryValue* publisher_data = nullptr;
    std::string site_title;
    if (dict_value->GetDictionary("publisherData", &publisher_data)) {
      if (!publisher_data->GetString("sourceName", &site_title)) {
        // It's possible but not desirable to have no publisher data.
        DLOG(WARNING) << "No publisher name for article " << corpus_id.spec();
      }
    } else {
      DLOG(WARNING) << "No publisher data for article " << corpus_id.spec();
    }

    std::string amp_url_str;
    GURL amp_url;
    // Expected to not have AMP url sometimes.
    if (dict_value->GetString("ampUrl", &amp_url_str)) {
      amp_url = GURL(amp_url_str);
      DLOG_IF(WARNING, !amp_url.is_valid()) << "Invalid AMP url "
                                            << amp_url_str;
    }
    SnippetSource source(corpus_id, site_title,
                         amp_url.is_valid() ? amp_url : GURL());
    snippet->add_source(source);
  }

  if (snippet->sources_.empty()) {
    DLOG(WARNING) << "No sources found for article " << id;
    return nullptr;
  }

  snippet->FindBestSource();

  double score;
  if (dict.GetDouble("score", &score))
    snippet->set_score(score);

  return snippet;
}

// static
std::unique_ptr<NTPSnippet> NTPSnippet::CreateFromContentSuggestionsDictionary(
    const base::DictionaryValue& dict) {
  const base::ListValue* id_list;
  std::string id;
  if (!(dict.GetList("id", &id_list) &&
        id_list->GetString(0, &id))) {  // TODO(sfiera): multiple IDs
    return nullptr;
  }

  auto snippet = base::MakeUnique<NTPSnippet>(id);
  snippet->sources_.emplace_back(GURL(), std::string(), GURL());
  auto source = &snippet->sources_.back();
  snippet->best_source_index_ = 0;

  if (!(dict.GetString("title", &snippet->title_) &&
        dict.GetString("summaryText", &snippet->snippet_) &&
        GetTimeValue(dict, "publishTime", &snippet->publish_date_) &&
        GetTimeValue(dict, "expirationTime", &snippet->expiry_date_) &&
        GetURLValue(dict, "imageUrl", &snippet->salient_image_url_) &&
        dict.GetString("publisherName", &source->publisher_name) &&
        GetURLValue(dict, "fullPageUrl", &source->url))) {
    return nullptr;
  }
  GetURLValue(dict, "ampUrl", &source->amp_url);  // May fail; OK.
  // TODO(sfiera): also favicon URL.

  snippet->score_ = 0.0;  // TODO(sfiera): put score in protocol.

  return snippet;
}

// static
std::unique_ptr<NTPSnippet> NTPSnippet::CreateFromProto(
    const SnippetProto& proto) {
  // Need at least the id.
  if (!proto.has_id() || proto.id().empty())
    return nullptr;

  std::unique_ptr<NTPSnippet> snippet(new NTPSnippet(proto.id()));

  snippet->set_title(proto.title());
  snippet->set_snippet(proto.snippet());
  snippet->set_salient_image_url(GURL(proto.salient_image_url()));
  snippet->set_publish_date(
      base::Time::FromInternalValue(proto.publish_date()));
  snippet->set_expiry_date(base::Time::FromInternalValue(proto.expiry_date()));
  snippet->set_score(proto.score());
  snippet->set_discarded(proto.discarded());

  for (int i = 0; i < proto.sources_size(); ++i) {
    const SnippetSourceProto& source_proto = proto.sources(i);
    GURL url(source_proto.url());
    if (!url.is_valid()) {
      // We must at least have a valid source URL.
      DLOG(WARNING) << "Invalid article url " << source_proto.url();
      continue;
    }
    std::string publisher_name = source_proto.publisher_name();
    GURL amp_url;
    if (source_proto.has_amp_url()) {
      amp_url = GURL(source_proto.amp_url());
      DLOG_IF(WARNING, !amp_url.is_valid()) << "Invalid AMP URL "
                                            << source_proto.amp_url();
    }

    snippet->add_source(SnippetSource(url, publisher_name, amp_url));
  }

  if (snippet->sources_.empty()) {
    DLOG(WARNING) << "No sources found for article " << snippet->id();
    return nullptr;
  }

  snippet->FindBestSource();

  return snippet;
}

SnippetProto NTPSnippet::ToProto() const {
  SnippetProto result;

  result.set_id(id_);
  if (!title_.empty())
    result.set_title(title_);
  if (!snippet_.empty())
    result.set_snippet(snippet_);
  if (salient_image_url_.is_valid())
    result.set_salient_image_url(salient_image_url_.spec());
  if (!publish_date_.is_null())
    result.set_publish_date(publish_date_.ToInternalValue());
  if (!expiry_date_.is_null())
    result.set_expiry_date(expiry_date_.ToInternalValue());
  result.set_score(score_);
  result.set_discarded(is_discarded_);

  for (const SnippetSource& source : sources_) {
    SnippetSourceProto* source_proto = result.add_sources();
    source_proto->set_url(source.url.spec());
    if (!source.publisher_name.empty())
      source_proto->set_publisher_name(source.publisher_name);
    if (source.amp_url.is_valid())
      source_proto->set_amp_url(source.amp_url.spec());
  }

  return result;
}

std::unique_ptr<ContentSuggestion> NTPSnippet::ToContentSuggestion() const {
  std::unique_ptr<ContentSuggestion> result(new ContentSuggestion(
      id_, ContentSuggestionsProviderType::ARTICLES,
      ContentSuggestionCategory::ARTICLE, best_source().url));
  result->set_amp_url(best_source().amp_url);
  result->set_title(title_);
  result->set_snippet_text(snippet_);
  result->set_publish_date(publish_date_);
  result->set_publisher_name(best_source().publisher_name);
  result->set_score(score_);
  return result;
}

// static
base::Time NTPSnippet::TimeFromJsonString(const std::string& timestamp_str) {
  int64_t timestamp;
  if (!base::StringToInt64(timestamp_str, &timestamp)) {
    // Even if there's an error in the conversion, some garbage data may still
    // be written to the output var, so reset it.
    timestamp = 0;
  }
  return base::Time::UnixEpoch() + base::TimeDelta::FromSeconds(timestamp);
}

// static
std::string NTPSnippet::TimeToJsonString(const base::Time& time) {
  return base::Int64ToString((time - base::Time::UnixEpoch()).InSeconds());
}

void NTPSnippet::FindBestSource() {
  // The same article can be hosted by multiple sources, e.g. nytimes.com,
  // cnn.com, etc. We need to parse the list of sources for this article and
  // find the best match. In order of preference:
  //  1 A source that has URL, publisher name, AMP URL
  //  2) A source that has URL, publisher name
  //  3) A source that has URL and AMP URL, or URL only (since we won't show
  //  the snippet to users if the article does not have a publisher name, it
  //  doesn't matter whether the snippet has the AMP URL or not)
  best_source_index_ = 0;
  for (size_t i = 0; i < sources_.size(); ++i) {
    const SnippetSource& source = sources_[i];
    if (!source.publisher_name.empty()) {
      best_source_index_ = i;
      if (!source.amp_url.is_empty()) {
        // This is the best possible source, stop looking.
        break;
      }
    }
  }
}

}  // namespace ntp_snippets
