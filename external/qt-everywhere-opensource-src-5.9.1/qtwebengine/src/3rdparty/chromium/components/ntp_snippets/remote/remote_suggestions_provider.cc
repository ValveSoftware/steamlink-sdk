// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/remote/remote_suggestions_provider.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_runner_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/history/core/browser/history_service.h"
#include "components/image_fetcher/image_decoder.h"
#include "components/image_fetcher/image_fetcher.h"
#include "components/ntp_snippets/features.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/ntp_snippets/remote/remote_suggestions_database.h"
#include "components/ntp_snippets/switches.h"
#include "components/ntp_snippets/user_classifier.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/variations_associated_data.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"

namespace ntp_snippets {

namespace {

// Number of snippets requested to the server. Consider replacing sparse UMA
// histograms with COUNTS() if this number increases beyond 50.
const int kMaxSnippetCount = 10;

// Number of archived snippets we keep around in memory.
const int kMaxArchivedSnippetCount = 200;

// Default values for fetching intervals, fallback and wifi.
const double kDefaultFetchingIntervalRareNtpUser[] = {48.0, 24.0};
const double kDefaultFetchingIntervalActiveNtpUser[] = {24.0, 6.0};
const double kDefaultFetchingIntervalActiveSuggestionsConsumer[] = {24.0, 6.0};

// Variation parameters than can override the default fetching intervals.
const char* kFetchingIntervalParamNameRareNtpUser[] = {
    "fetching_interval_hours-fallback-rare_ntp_user",
    "fetching_interval_hours-wifi-rare_ntp_user"};
const char* kFetchingIntervalParamNameActiveNtpUser[] = {
    "fetching_interval_hours-fallback-active_ntp_user",
    "fetching_interval_hours-wifi-active_ntp_user"};
const char* kFetchingIntervalParamNameActiveSuggestionsConsumer[] = {
    "fetching_interval_hours-fallback-active_suggestions_consumer",
    "fetching_interval_hours-wifi-active_suggestions_consumer"};

const int kDefaultExpiryTimeMins = 3 * 24 * 60;

// Keys for storing CategoryContent info in prefs.
const char kCategoryContentId[] = "id";
const char kCategoryContentTitle[] = "title";
const char kCategoryContentProvidedByServer[] = "provided_by_server";
const char kCategoryContentAllowFetchingMore[] = "allow_fetching_more";

// TODO(treib): Remove after M57.
const char kDeprecatedSnippetHostsPref[] = "ntp_snippets.hosts";

base::TimeDelta GetFetchingInterval(bool is_wifi,
                                    UserClassifier::UserClass user_class) {
  double value_hours = 0.0;

  const int index = is_wifi ? 1 : 0;
  const char* param_name = "";
  switch (user_class) {
    case UserClassifier::UserClass::RARE_NTP_USER:
      value_hours = kDefaultFetchingIntervalRareNtpUser[index];
      param_name = kFetchingIntervalParamNameRareNtpUser[index];
      break;
    case UserClassifier::UserClass::ACTIVE_NTP_USER:
      value_hours = kDefaultFetchingIntervalActiveNtpUser[index];
      param_name = kFetchingIntervalParamNameActiveNtpUser[index];
      break;
    case UserClassifier::UserClass::ACTIVE_SUGGESTIONS_CONSUMER:
      value_hours = kDefaultFetchingIntervalActiveSuggestionsConsumer[index];
      param_name = kFetchingIntervalParamNameActiveSuggestionsConsumer[index];
      break;
  }

  // The default value can be overridden by a variation parameter.
  std::string param_value_str = variations::GetVariationParamValueByFeature(
      ntp_snippets::kArticleSuggestionsFeature, param_name);
  if (!param_value_str.empty()) {
    double param_value_hours = 0.0;
    if (base::StringToDouble(param_value_str, &param_value_hours))
      value_hours = param_value_hours;
    else
      LOG(WARNING) << "Invalid value for variation parameter " << param_name;
  }

  return base::TimeDelta::FromSecondsD(value_hours * 3600.0);
}

std::unique_ptr<std::vector<std::string>> GetSnippetIDVector(
    const NTPSnippet::PtrVector& snippets) {
  auto result = base::MakeUnique<std::vector<std::string>>();
  for (const auto& snippet : snippets) {
    result->push_back(snippet->id());
  }
  return result;
}

bool HasIntersection(const std::vector<std::string>& a,
                     const std::set<std::string>& b) {
  for (const std::string& item : a) {
    if (base::ContainsValue(b, item))
      return true;
  }
  return false;
}

void EraseByPrimaryID(NTPSnippet::PtrVector* snippets,
                      const std::vector<std::string>& ids) {
  std::set<std::string> ids_lookup(ids.begin(), ids.end());
  snippets->erase(
      std::remove_if(snippets->begin(), snippets->end(),
                     [&ids_lookup](const std::unique_ptr<NTPSnippet>& snippet) {
                       return base::ContainsValue(ids_lookup, snippet->id());
                     }),
      snippets->end());
}

void EraseMatchingSnippets(NTPSnippet::PtrVector* snippets,
                           const NTPSnippet::PtrVector& compare_against) {
  std::set<std::string> compare_against_ids;
  for (const std::unique_ptr<NTPSnippet>& snippet : compare_against) {
    const std::vector<std::string>& snippet_ids = snippet->GetAllIDs();
    compare_against_ids.insert(snippet_ids.begin(), snippet_ids.end());
  }
  snippets->erase(
      std::remove_if(
          snippets->begin(), snippets->end(),
          [&compare_against_ids](const std::unique_ptr<NTPSnippet>& snippet) {
            return HasIntersection(snippet->GetAllIDs(), compare_against_ids);
          }),
      snippets->end());
}

void RemoveNullPointers(NTPSnippet::PtrVector* snippets) {
  snippets->erase(
      std::remove_if(
          snippets->begin(), snippets->end(),
          [](const std::unique_ptr<NTPSnippet>& snippet) { return !snippet; }),
      snippets->end());
}

void AssignExpiryAndPublishDates(NTPSnippet::PtrVector* snippets) {
  for (std::unique_ptr<NTPSnippet>& snippet : *snippets) {
    if (snippet->publish_date().is_null())
      snippet->set_publish_date(base::Time::Now());
    if (snippet->expiry_date().is_null()) {
      snippet->set_expiry_date(
          snippet->publish_date() +
          base::TimeDelta::FromMinutes(kDefaultExpiryTimeMins));
    }
  }
}

void RemoveIncompleteSnippets(NTPSnippet::PtrVector* snippets) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAddIncompleteSnippets)) {
    return;
  }
  int num_snippets = snippets->size();
  // Remove snippets that do not have all the info we need to display it to
  // the user.
  snippets->erase(
      std::remove_if(snippets->begin(), snippets->end(),
                     [](const std::unique_ptr<NTPSnippet>& snippet) {
                       return !snippet->is_complete();
                     }),
      snippets->end());
  int num_snippets_removed = num_snippets - snippets->size();
  UMA_HISTOGRAM_BOOLEAN("NewTabPage.Snippets.IncompleteSnippetsAfterFetch",
                        num_snippets_removed > 0);
  if (num_snippets_removed > 0) {
    UMA_HISTOGRAM_SPARSE_SLOWLY("NewTabPage.Snippets.NumIncompleteSnippets",
                                num_snippets_removed);
  }
}

std::vector<ContentSuggestion> ConvertToContentSuggestions(
    Category category,
    const NTPSnippet::PtrVector& snippets) {
  std::vector<ContentSuggestion> result;
  for (const std::unique_ptr<NTPSnippet>& snippet : snippets) {
    // TODO(sfiera): if a snippet is not going to be displayed, move it
    // directly to content.dismissed on fetch. Otherwise, we might prune
    // other snippets to get down to kMaxSnippetCount, only to hide one of the
    // incomplete ones we kept.
    if (!snippet->is_complete())
      continue;
    ContentSuggestion suggestion(category, snippet->id(),
                                 snippet->best_source().url);
    suggestion.set_amp_url(snippet->best_source().amp_url);
    suggestion.set_title(base::UTF8ToUTF16(snippet->title()));
    suggestion.set_snippet_text(base::UTF8ToUTF16(snippet->snippet()));
    suggestion.set_publish_date(snippet->publish_date());
    suggestion.set_publisher_name(
        base::UTF8ToUTF16(snippet->best_source().publisher_name));
    suggestion.set_score(snippet->score());
    result.emplace_back(std::move(suggestion));
  }
  return result;
}

void CallWithEmptyResults(FetchDoneCallback callback, Status status) {
  if (callback.is_null())
    return;
  callback.Run(status, std::vector<ContentSuggestion>());
}

}  // namespace

RemoteSuggestionsProvider::RemoteSuggestionsProvider(
    Observer* observer,
    CategoryFactory* category_factory,
    PrefService* pref_service,
    const std::string& application_language_code,
    const UserClassifier* user_classifier,
    NTPSnippetsScheduler* scheduler,
    std::unique_ptr<NTPSnippetsFetcher> snippets_fetcher,
    std::unique_ptr<image_fetcher::ImageFetcher> image_fetcher,
    std::unique_ptr<image_fetcher::ImageDecoder> image_decoder,
    std::unique_ptr<RemoteSuggestionsDatabase> database,
    std::unique_ptr<NTPSnippetsStatusService> status_service)
    : ContentSuggestionsProvider(observer, category_factory),
      state_(State::NOT_INITED),
      pref_service_(pref_service),
      articles_category_(
          category_factory->FromKnownCategory(KnownCategories::ARTICLES)),
      application_language_code_(application_language_code),
      user_classifier_(user_classifier),
      scheduler_(scheduler),
      snippets_fetcher_(std::move(snippets_fetcher)),
      image_fetcher_(std::move(image_fetcher)),
      image_decoder_(std::move(image_decoder)),
      database_(std::move(database)),
      snippets_status_service_(std::move(status_service)),
      fetch_when_ready_(false),
      nuke_when_initialized_(false),
      thumbnail_requests_throttler_(
          pref_service,
          RequestThrottler::RequestType::CONTENT_SUGGESTION_THUMBNAIL) {
  pref_service_->ClearPref(kDeprecatedSnippetHostsPref);

  RestoreCategoriesFromPrefs();
  // The articles category always exists. Add it if we didn't get it from prefs.
  // TODO(treib): Rethink this.
  category_contents_.insert(
      std::make_pair(articles_category_,
                     CategoryContent(BuildArticleCategoryInfo(base::nullopt))));
  // Tell the observer about all the categories.
  for (const auto& entry : category_contents_) {
    observer->OnCategoryStatusChanged(this, entry.first, entry.second.status);
  }

  if (database_->IsErrorState()) {
    EnterState(State::ERROR_OCCURRED);
    UpdateAllCategoryStatus(CategoryStatus::LOADING_ERROR);
    return;
  }

  database_->SetErrorCallback(base::Bind(
      &RemoteSuggestionsProvider::OnDatabaseError, base::Unretained(this)));

  // We transition to other states while finalizing the initialization, when the
  // database is done loading.
  database_load_start_ = base::TimeTicks::Now();
  database_->LoadSnippets(base::Bind(
      &RemoteSuggestionsProvider::OnDatabaseLoaded, base::Unretained(this)));
}

RemoteSuggestionsProvider::~RemoteSuggestionsProvider() = default;

// static
void RemoteSuggestionsProvider::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  // TODO(treib): Remove after M57.
  registry->RegisterListPref(kDeprecatedSnippetHostsPref);
  registry->RegisterListPref(prefs::kRemoteSuggestionCategories);
  registry->RegisterInt64Pref(prefs::kSnippetBackgroundFetchingIntervalWifi, 0);
  registry->RegisterInt64Pref(prefs::kSnippetBackgroundFetchingIntervalFallback,
                              0);

  NTPSnippetsStatusService::RegisterProfilePrefs(registry);
}

void RemoteSuggestionsProvider::FetchSnippets(bool interactive_request) {
  if (ready())
    FetchSnippetsFromHosts(std::set<std::string>(), interactive_request);
  else
    fetch_when_ready_ = true;
}

void RemoteSuggestionsProvider::FetchSnippetsFromHosts(
    const std::set<std::string>& hosts,
    bool interactive_request) {
  // TODO(tschumann): FetchSnippets() and FetchSnippetsFromHost() implement the
  // fetch logic when called by the background fetcher or interactive "reload"
  // requests. Fetch() is right now only called for the fetch-more use case.
  // The names are confusing and we need to clean them up.
  if (!ready())
    return;
  MarkEmptyCategoriesAsLoading();

  NTPSnippetsFetcher::Params params =
      BuildFetchParams(/*exclude_archived_suggestions=*/true);
  params.hosts = hosts;
  params.interactive_request = interactive_request;
  snippets_fetcher_->FetchSnippets(
      params, base::BindOnce(&RemoteSuggestionsProvider::OnFetchFinished,
                             base::Unretained(this)));
}

void RemoteSuggestionsProvider::Fetch(
    const Category& category,
    const std::set<std::string>& known_suggestion_ids,
    const FetchDoneCallback& callback) {
  if (!ready()) {
    CallWithEmptyResults(callback,
                         Status(StatusCode::TEMPORARY_ERROR,
                                "RemoteSuggestionsProvider is not ready!"));
    return;
  }
  NTPSnippetsFetcher::Params params =
      BuildFetchParams(/*exclude_archived_suggestions=*/false);
  params.excluded_ids.insert(known_suggestion_ids.begin(),
                             known_suggestion_ids.end());
  params.interactive_request = true;
  params.exclusive_category = category;

  // TODO(tschumann): NTPSnippetsFetcher does not support concurrent requests
  // yet. If a background fetch happens while we fetch-more data, the callback
  // will never get called.
  snippets_fetcher_->FetchSnippets(
      params, base::BindOnce(&RemoteSuggestionsProvider::OnFetchMoreFinished,
                             base::Unretained(this), callback));
}

// Builds default fetcher params.
NTPSnippetsFetcher::Params RemoteSuggestionsProvider::BuildFetchParams(
    bool exclude_archived_suggestions) const {
  NTPSnippetsFetcher::Params result;
  result.language_code = application_language_code_;
  result.count_to_fetch = kMaxSnippetCount;
  for (const auto& map_entry : category_contents_) {
    const CategoryContent& content = map_entry.second;
    for (const auto& dismissed_snippet : content.dismissed) {
      result.excluded_ids.insert(dismissed_snippet->id());
    }
    if (exclude_archived_suggestions) {
      for (const auto& archived_snippet : content.archived) {
        result.excluded_ids.insert(archived_snippet->id());
      }
    }
  }
  return result;
}

void RemoteSuggestionsProvider::MarkEmptyCategoriesAsLoading() {
  for (const auto& item : category_contents_) {
    Category category = item.first;
    const CategoryContent& content = item.second;
    if (content.snippets.empty())
      UpdateCategoryStatus(category, CategoryStatus::AVAILABLE_LOADING);
  }
}

void RemoteSuggestionsProvider::RescheduleFetching(bool force) {
  // The scheduler only exists on Android so far, it's null on other platforms.
  if (!scheduler_)
    return;

  if (ready()) {
    base::TimeDelta old_interval_wifi = base::TimeDelta::FromInternalValue(
        pref_service_->GetInt64(prefs::kSnippetBackgroundFetchingIntervalWifi));
    base::TimeDelta old_interval_fallback =
        base::TimeDelta::FromInternalValue(pref_service_->GetInt64(
            prefs::kSnippetBackgroundFetchingIntervalFallback));
    UserClassifier::UserClass user_class = user_classifier_->GetUserClass();
    base::TimeDelta interval_wifi =
        GetFetchingInterval(/*is_wifi=*/true, user_class);
    base::TimeDelta interval_fallback =
        GetFetchingInterval(/*is_wifi=*/false, user_class);
    if (force || interval_wifi != old_interval_wifi ||
        interval_fallback != old_interval_fallback) {
      scheduler_->Schedule(interval_wifi, interval_fallback);
      pref_service_->SetInt64(prefs::kSnippetBackgroundFetchingIntervalWifi,
                              interval_wifi.ToInternalValue());
      pref_service_->SetInt64(prefs::kSnippetBackgroundFetchingIntervalFallback,
                              interval_fallback.ToInternalValue());
    }
  } else {
    // If we're NOT_INITED, we don't know whether to schedule or unschedule.
    // If |force| is false, all is well: We'll reschedule on the next state
    // change anyway. If it's true, then unschedule here, to make sure that the
    // next reschedule actually happens.
    if (state_ != State::NOT_INITED || force) {
      scheduler_->Unschedule();
      pref_service_->ClearPref(prefs::kSnippetBackgroundFetchingIntervalWifi);
      pref_service_->ClearPref(
          prefs::kSnippetBackgroundFetchingIntervalFallback);
    }
  }
}

CategoryStatus RemoteSuggestionsProvider::GetCategoryStatus(Category category) {
  auto content_it = category_contents_.find(category);
  DCHECK(content_it != category_contents_.end());
  return content_it->second.status;
}

CategoryInfo RemoteSuggestionsProvider::GetCategoryInfo(Category category) {
  auto content_it = category_contents_.find(category);
  DCHECK(content_it != category_contents_.end());
  return content_it->second.info;
}

void RemoteSuggestionsProvider::DismissSuggestion(
    const ContentSuggestion::ID& suggestion_id) {
  if (!ready())
    return;

  auto content_it = category_contents_.find(suggestion_id.category());
  DCHECK(content_it != category_contents_.end());
  CategoryContent* content = &content_it->second;
  DismissSuggestionFromCategoryContent(content,
                                       suggestion_id.id_within_category());
}

void RemoteSuggestionsProvider::FetchSuggestionImage(
    const ContentSuggestion::ID& suggestion_id,
    const ImageFetchedCallback& callback) {
  database_->LoadImage(
      suggestion_id.id_within_category(),
      base::Bind(&RemoteSuggestionsProvider::OnSnippetImageFetchedFromDatabase,
                 base::Unretained(this), callback, suggestion_id));
}

void RemoteSuggestionsProvider::ClearHistory(
    base::Time begin,
    base::Time end,
    const base::Callback<bool(const GURL& url)>& filter) {
  // Both time range and the filter are ignored and all suggestions are removed,
  // because it is not known which history entries were used for the suggestions
  // personalization.
  if (!ready())
    nuke_when_initialized_ = true;
  else
    NukeAllSnippets();
}

void RemoteSuggestionsProvider::ClearCachedSuggestions(Category category) {
  if (!initialized())
    return;

  auto content_it = category_contents_.find(category);
  if (content_it == category_contents_.end()) {
    return;
  }
  CategoryContent* content = &content_it->second;
  if (content->snippets.empty())
    return;

  database_->DeleteSnippets(GetSnippetIDVector(content->snippets));
  database_->DeleteImages(GetSnippetIDVector(content->snippets));
  content->snippets.clear();

  if (IsCategoryStatusAvailable(content->status))
    NotifyNewSuggestions(category, *content);
}

void RemoteSuggestionsProvider::GetDismissedSuggestionsForDebugging(
    Category category,
    const DismissedSuggestionsCallback& callback) {
  auto content_it = category_contents_.find(category);
  DCHECK(content_it != category_contents_.end());
  callback.Run(
      ConvertToContentSuggestions(category, content_it->second.dismissed));
}

void RemoteSuggestionsProvider::ClearDismissedSuggestionsForDebugging(
    Category category) {
  auto content_it = category_contents_.find(category);
  DCHECK(content_it != category_contents_.end());
  CategoryContent* content = &content_it->second;

  if (!initialized())
    return;

  if (content->dismissed.empty())
    return;

  database_->DeleteSnippets(GetSnippetIDVector(content->dismissed));
  // The image got already deleted when the suggestion was dismissed.

  content->dismissed.clear();
}

// static
int RemoteSuggestionsProvider::GetMaxSnippetCountForTesting() {
  return kMaxSnippetCount;
}

////////////////////////////////////////////////////////////////////////////////
// Private methods

GURL RemoteSuggestionsProvider::FindSnippetImageUrl(
    const ContentSuggestion::ID& suggestion_id) const {
  DCHECK(base::ContainsKey(category_contents_, suggestion_id.category()));

  const CategoryContent& content =
      category_contents_.at(suggestion_id.category());
  const NTPSnippet* snippet =
      content.FindSnippet(suggestion_id.id_within_category());
  if (!snippet)
    return GURL();
  return snippet->salient_image_url();
}

// image_fetcher::ImageFetcherDelegate implementation.
void RemoteSuggestionsProvider::OnImageDataFetched(
    const std::string& id_within_category,
    const std::string& image_data) {
  if (image_data.empty())
    return;

  // Only save the image if the corresponding snippet still exists.
  bool found = false;
  for (const auto& entry : category_contents_) {
    if (entry.second.FindSnippet(id_within_category)) {
      found = true;
      break;
    }
  }
  if (!found)
    return;

  // Only cache the data in the DB, the actual serving is done in the callback
  // provided to |image_fetcher_| (OnSnippetImageDecodedFromNetwork()).
  database_->SaveImage(id_within_category, image_data);
}

void RemoteSuggestionsProvider::OnDatabaseLoaded(
    NTPSnippet::PtrVector snippets) {
  if (state_ == State::ERROR_OCCURRED)
    return;
  DCHECK(state_ == State::NOT_INITED);
  DCHECK(base::ContainsKey(category_contents_, articles_category_));

  base::TimeDelta database_load_time =
      base::TimeTicks::Now() - database_load_start_;
  UMA_HISTOGRAM_MEDIUM_TIMES("NewTabPage.Snippets.DatabaseLoadTime",
                             database_load_time);

  NTPSnippet::PtrVector to_delete;
  for (std::unique_ptr<NTPSnippet>& snippet : snippets) {
    Category snippet_category =
        category_factory()->FromRemoteCategory(snippet->remote_category_id());
    auto content_it = category_contents_.find(snippet_category);
    // We should already know about the category.
    if (content_it == category_contents_.end()) {
      DLOG(WARNING) << "Loaded a suggestion for unknown category "
                    << snippet_category << " from the DB; deleting";
      to_delete.emplace_back(std::move(snippet));
      continue;
    }
    CategoryContent* content = &content_it->second;
    if (snippet->is_dismissed())
      content->dismissed.emplace_back(std::move(snippet));
    else
      content->snippets.emplace_back(std::move(snippet));
  }
  if (!to_delete.empty()) {
    database_->DeleteSnippets(GetSnippetIDVector(to_delete));
    database_->DeleteImages(GetSnippetIDVector(to_delete));
  }

  // Sort the suggestions in each category.
  // TODO(treib): Persist the actual order in the DB somehow? crbug.com/654409
  for (auto& entry : category_contents_) {
    CategoryContent* content = &entry.second;
    std::sort(content->snippets.begin(), content->snippets.end(),
              [](const std::unique_ptr<NTPSnippet>& lhs,
                 const std::unique_ptr<NTPSnippet>& rhs) {
                return lhs->score() > rhs->score();
              });
  }

  // TODO(tschumann): If I move ClearExpiredDismissedSnippets() to the beginning
  // of the function, it essentially does nothing but tests are still green. Fix
  // this!
  ClearExpiredDismissedSnippets();
  ClearOrphanedImages();
  FinishInitialization();
}

void RemoteSuggestionsProvider::OnDatabaseError() {
  EnterState(State::ERROR_OCCURRED);
  UpdateAllCategoryStatus(CategoryStatus::LOADING_ERROR);
}

void RemoteSuggestionsProvider::OnFetchMoreFinished(
    FetchDoneCallback fetching_callback,
    NTPSnippetsFetcher::OptionalFetchedCategories fetched_categories) {
  if (!fetched_categories) {
    // TODO(fhorschig): Disambiguate the kind of error that led here.
    CallWithEmptyResults(fetching_callback,
                         Status(StatusCode::PERMANENT_ERROR,
                                "The NTPSnippetsFetcher did not "
                                "complete the fetching successfully."));
    return;
  }
  if (fetched_categories->size() != 1u) {
    LOG(DFATAL) << "Requested one exclusive category but received "
                << fetched_categories->size() << " categories.";
    CallWithEmptyResults(fetching_callback,
                         Status(StatusCode::PERMANENT_ERROR,
                                "RemoteSuggestionsProvider received more "
                                "categories than requested."));
    return;
  }
  auto& fetched_category = (*fetched_categories)[0];
  Category category = fetched_category.category;
  CategoryContent* existing_content =
      UpdateCategoryInfo(category, fetched_category.info);
  SanitizeReceivedSnippets(existing_content->dismissed,
                           &fetched_category.snippets);
  // We compute the result now before modifying |fetched_category.snippets|.
  // However, we wait with notifying the caller until the end of the method when
  // all state is updated.
  std::vector<ContentSuggestion> result =
      ConvertToContentSuggestions(category, fetched_category.snippets);

  // Fill up the newly fetched snippets with existing ones, store them, and
  // notify observers about new data.
  while (fetched_category.snippets.size() <
             static_cast<size_t>(kMaxSnippetCount) &&
         !existing_content->snippets.empty()) {
    fetched_category.snippets.emplace(
        fetched_category.snippets.begin(),
        std::move(existing_content->snippets.back()));
    existing_content->snippets.pop_back();
  }
  std::vector<std::string> to_dismiss =
      *GetSnippetIDVector(existing_content->snippets);
  for (const auto& id : to_dismiss) {
    DismissSuggestionFromCategoryContent(existing_content, id);
  }
  DCHECK(existing_content->snippets.empty());

  IntegrateSnippets(existing_content, std::move(fetched_category.snippets));

  // TODO(tschumann): We should properly honor the existing category state,
  // e.g. to make sure we don't serve results after the sign-out. Revisit this
  // once the snippets fetcher supports concurrent requests. We can then see if
  // Nuke should also cancel outstanding requests or we want to check the
  // status.
  UpdateCategoryStatus(category, CategoryStatus::AVAILABLE);
  // Notify callers and observers.
  fetching_callback.Run(Status(StatusCode::SUCCESS), std::move(result));
  NotifyNewSuggestions(category, *existing_content);
}

void RemoteSuggestionsProvider::OnFetchFinished(
    NTPSnippetsFetcher::OptionalFetchedCategories fetched_categories) {
  if (!ready()) {
    // TODO(tschumann): What happens if this was a user-triggered, interactive
    // request? Is the UI waiting indefinitely now?
    return;
  }

  // Mark all categories as not provided by the server in the latest fetch. The
  // ones we got will be marked again below.
  for (auto& item : category_contents_) {
    CategoryContent* content = &item.second;
    content->included_in_last_server_response = false;
  }

  // Clear up expired dismissed snippets before we use them to filter new ones.
  ClearExpiredDismissedSnippets();

  // If snippets were fetched successfully, update our |category_contents_| from
  // each category provided by the server.
  if (fetched_categories) {
    // TODO(treib): Reorder |category_contents_| to match the order we received
    // from the server. crbug.com/653816
    for (NTPSnippetsFetcher::FetchedCategory& fetched_category :
         *fetched_categories) {
      // TODO(tschumann): Remove this histogram once we only talk to the content
      // suggestions cloud backend.
      if (fetched_category.category == articles_category_) {
        UMA_HISTOGRAM_SPARSE_SLOWLY(
            "NewTabPage.Snippets.NumArticlesFetched",
            std::min(fetched_category.snippets.size(),
                     static_cast<size_t>(kMaxSnippetCount + 1)));
      }
      CategoryContent* content =
          UpdateCategoryInfo(fetched_category.category, fetched_category.info);
      content->included_in_last_server_response = true;
      SanitizeReceivedSnippets(content->dismissed, &fetched_category.snippets);
      IntegrateSnippets(content, std::move(fetched_category.snippets));
    }
  }

  // TODO(tschumann): The snippets fetcher needs to signal errors so that we
  // know why we received no data. If an error occured, none of the following
  // should take place.

  // We might have gotten new categories (or updated the titles of existing
  // ones), so update the pref.
  StoreCategoriesToPrefs();

  for (const auto& item : category_contents_) {
    Category category = item.first;
    UpdateCategoryStatus(category, CategoryStatus::AVAILABLE);
    // TODO(sfiera): notify only when a category changed above.
    NotifyNewSuggestions(category, item.second);
  }

  // TODO(sfiera): equivalent metrics for non-articles.
  auto content_it = category_contents_.find(articles_category_);
  DCHECK(content_it != category_contents_.end());
  const CategoryContent& content = content_it->second;
  UMA_HISTOGRAM_SPARSE_SLOWLY("NewTabPage.Snippets.NumArticles",
                              content.snippets.size());
  if (content.snippets.empty() && !content.dismissed.empty()) {
    UMA_HISTOGRAM_COUNTS("NewTabPage.Snippets.NumArticlesZeroDueToDiscarded",
                         content.dismissed.size());
  }

  // Reschedule after a successful fetch. This resets all currently scheduled
  // fetches, to make sure the fallback interval triggers only if no wifi fetch
  // succeeded, and also that we don't do a background fetch immediately after
  // a user-initiated one.
  if (fetched_categories)
    RescheduleFetching(true);
}

void RemoteSuggestionsProvider::ArchiveSnippets(
    CategoryContent* content,
    NTPSnippet::PtrVector* to_archive) {
  // Archive previous snippets - move them at the beginning of the list.
  content->archived.insert(content->archived.begin(),
                           std::make_move_iterator(to_archive->begin()),
                           std::make_move_iterator(to_archive->end()));
  to_archive->clear();

  // If there are more archived snippets than we want to keep, delete the
  // oldest ones by their fetch time (which are always in the back).
  if (content->archived.size() > kMaxArchivedSnippetCount) {
    NTPSnippet::PtrVector to_delete(
        std::make_move_iterator(content->archived.begin() +
                                kMaxArchivedSnippetCount),
        std::make_move_iterator(content->archived.end()));
    content->archived.resize(kMaxArchivedSnippetCount);
    database_->DeleteImages(GetSnippetIDVector(to_delete));
  }
}

void RemoteSuggestionsProvider::SanitizeReceivedSnippets(
    const NTPSnippet::PtrVector& dismissed,
    NTPSnippet::PtrVector* snippets) {
  DCHECK(ready());
  EraseMatchingSnippets(snippets, dismissed);
  AssignExpiryAndPublishDates(snippets);
  RemoveIncompleteSnippets(snippets);
}

void RemoteSuggestionsProvider::IntegrateSnippets(
    CategoryContent* content,
    NTPSnippet::PtrVector new_snippets) {
  DCHECK(ready());

  // Do not touch the current set of snippets if the newly fetched one is empty.
  // TODO(tschumann): This should go. If we get empty results we should update
  // accordingly and remove the old one (only of course if this was not received
  // through a fetch-more).
  if (new_snippets.empty())
    return;

  // It's entirely possible that the newly fetched snippets contain articles
  // that have been present before.
  // We need to make sure to only delete and archive snippets that don't
  // appear with the same ID in the new suggestions (it's fine for additional
  // IDs though).
  EraseByPrimaryID(&content->snippets, *GetSnippetIDVector(new_snippets));
  // Do not delete the thumbnail images as they are still handy on open NTPs.
  database_->DeleteSnippets(GetSnippetIDVector(content->snippets));
  // Note, that ArchiveSnippets will clear |content->snippets|.
  ArchiveSnippets(content, &content->snippets);

  database_->SaveSnippets(new_snippets);

  content->snippets = std::move(new_snippets);
}

void RemoteSuggestionsProvider::DismissSuggestionFromCategoryContent(
    CategoryContent* content,
    const std::string& id_within_category) {
  auto it = std::find_if(
      content->snippets.begin(), content->snippets.end(),
      [&id_within_category](const std::unique_ptr<NTPSnippet>& snippet) {
        return snippet->id() == id_within_category;
      });
  if (it == content->snippets.end())
    return;

  (*it)->set_dismissed(true);

  database_->SaveSnippet(**it);

  content->dismissed.push_back(std::move(*it));
  content->snippets.erase(it);
}

void RemoteSuggestionsProvider::ClearExpiredDismissedSnippets() {
  std::vector<Category> categories_to_erase;

  const base::Time now = base::Time::Now();

  for (auto& item : category_contents_) {
    Category category = item.first;
    CategoryContent* content = &item.second;

    NTPSnippet::PtrVector to_delete;
    // Move expired dismissed snippets over into |to_delete|.
    for (std::unique_ptr<NTPSnippet>& snippet : content->dismissed) {
      if (snippet->expiry_date() <= now)
        to_delete.emplace_back(std::move(snippet));
    }
    RemoveNullPointers(&content->dismissed);

    // Delete the images.
    database_->DeleteImages(GetSnippetIDVector(to_delete));
    // Delete the removed article suggestions from the DB.
    database_->DeleteSnippets(GetSnippetIDVector(to_delete));

    if (content->snippets.empty() && content->dismissed.empty() &&
        category != articles_category_ &&
        !content->included_in_last_server_response) {
      categories_to_erase.push_back(category);
    }
  }

  for (Category category : categories_to_erase) {
    UpdateCategoryStatus(category, CategoryStatus::NOT_PROVIDED);
    category_contents_.erase(category);
  }

  StoreCategoriesToPrefs();
}

void RemoteSuggestionsProvider::ClearOrphanedImages() {
  auto alive_snippets = base::MakeUnique<std::set<std::string>>();
  for (const auto& entry : category_contents_) {
    const CategoryContent& content = entry.second;
    for (const auto& snippet_ptr : content.snippets) {
      alive_snippets->insert(snippet_ptr->id());
    }
    for (const auto& snippet_ptr : content.dismissed) {
      alive_snippets->insert(snippet_ptr->id());
    }
  }
  database_->GarbageCollectImages(std::move(alive_snippets));
}

void RemoteSuggestionsProvider::NukeAllSnippets() {
  std::vector<Category> categories_to_erase;

  // Empty the ARTICLES category and remove all others, since they may or may
  // not be personalized.
  for (const auto& item : category_contents_) {
    Category category = item.first;

    ClearCachedSuggestions(category);
    ClearDismissedSuggestionsForDebugging(category);

    UpdateCategoryStatus(category, CategoryStatus::NOT_PROVIDED);

    // Remove the category entirely; it may or may not reappear.
    if (category != articles_category_)
      categories_to_erase.push_back(category);
  }

  for (Category category : categories_to_erase) {
    category_contents_.erase(category);
  }

  StoreCategoriesToPrefs();
}

void RemoteSuggestionsProvider::OnSnippetImageFetchedFromDatabase(
    const ImageFetchedCallback& callback,
    const ContentSuggestion::ID& suggestion_id,
    std::string data) {
  // |image_decoder_| is null in tests.
  if (image_decoder_ && !data.empty()) {
    image_decoder_->DecodeImage(
        data, base::Bind(
                  &RemoteSuggestionsProvider::OnSnippetImageDecodedFromDatabase,
                  base::Unretained(this), callback, suggestion_id));
    return;
  }

  // Fetching from the DB failed; start a network fetch.
  FetchSnippetImageFromNetwork(suggestion_id, callback);
}

void RemoteSuggestionsProvider::OnSnippetImageDecodedFromDatabase(
    const ImageFetchedCallback& callback,
    const ContentSuggestion::ID& suggestion_id,
    const gfx::Image& image) {
  if (!image.IsEmpty()) {
    callback.Run(image);
    return;
  }

  // If decoding the image failed, delete the DB entry.
  database_->DeleteImage(suggestion_id.id_within_category());

  FetchSnippetImageFromNetwork(suggestion_id, callback);
}

void RemoteSuggestionsProvider::FetchSnippetImageFromNetwork(
    const ContentSuggestion::ID& suggestion_id,
    const ImageFetchedCallback& callback) {
  if (!base::ContainsKey(category_contents_, suggestion_id.category())) {
    OnSnippetImageDecodedFromNetwork(
        callback, suggestion_id.id_within_category(), gfx::Image());
    return;
  }

  GURL image_url = FindSnippetImageUrl(suggestion_id);

  if (image_url.is_empty() ||
      !thumbnail_requests_throttler_.DemandQuotaForRequest(
          /*interactive_request=*/true)) {
    // Return an empty image. Directly, this is never synchronous with the
    // original FetchSuggestionImage() call - an asynchronous database query has
    // happened in the meantime.
    OnSnippetImageDecodedFromNetwork(
        callback, suggestion_id.id_within_category(), gfx::Image());
    return;
  }

  image_fetcher_->StartOrQueueNetworkRequest(
      suggestion_id.id_within_category(), image_url,
      base::Bind(&RemoteSuggestionsProvider::OnSnippetImageDecodedFromNetwork,
                 base::Unretained(this), callback));
}

void RemoteSuggestionsProvider::OnSnippetImageDecodedFromNetwork(
    const ImageFetchedCallback& callback,
    const std::string& id_within_category,
    const gfx::Image& image) {
  callback.Run(image);
}

void RemoteSuggestionsProvider::EnterStateReady() {
  if (nuke_when_initialized_) {
    NukeAllSnippets();
    nuke_when_initialized_ = false;
  }

  auto article_category_it = category_contents_.find(articles_category_);
  DCHECK(article_category_it != category_contents_.end());
  if (article_category_it->second.snippets.empty() || fetch_when_ready_) {
    // TODO(jkrcal): Fetching snippets automatically upon creation of this
    // lazily created service can cause troubles, e.g. in unit tests where
    // network I/O is not allowed.
    // Either add a DCHECK here that we actually are allowed to do network I/O
    // or change the logic so that some explicit call is always needed for the
    // network request.
    FetchSnippets(/*interactive_request=*/false);
    fetch_when_ready_ = false;
  }

  for (const auto& item : category_contents_) {
    Category category = item.first;
    const CategoryContent& content = item.second;
    // FetchSnippets has set the status to |AVAILABLE_LOADING| if relevant,
    // otherwise we transition to |AVAILABLE| here.
    if (content.status != CategoryStatus::AVAILABLE_LOADING)
      UpdateCategoryStatus(category, CategoryStatus::AVAILABLE);
  }
}

void RemoteSuggestionsProvider::EnterStateDisabled() {
  NukeAllSnippets();
}

void RemoteSuggestionsProvider::EnterStateError() {
  snippets_status_service_.reset();
}

void RemoteSuggestionsProvider::FinishInitialization() {
  if (nuke_when_initialized_) {
    // We nuke here in addition to EnterStateReady, so that it happens even if
    // we enter the DISABLED state below.
    NukeAllSnippets();
    nuke_when_initialized_ = false;
  }

  // |image_fetcher_| can be null in tests.
  if (image_fetcher_) {
    image_fetcher_->SetImageFetcherDelegate(this);
    image_fetcher_->SetDataUseServiceName(
        data_use_measurement::DataUseUserData::NTP_SNIPPETS);
  }

  // Note: Initializing the status service will run the callback right away with
  // the current state.
  snippets_status_service_->Init(
      base::Bind(&RemoteSuggestionsProvider::OnSnippetsStatusChanged,
                 base::Unretained(this)));

  // Always notify here even if we got nothing from the database, because we
  // don't know how long the fetch will take or if it will even complete.
  for (const auto& item : category_contents_) {
    Category category = item.first;
    const CategoryContent& content = item.second;
    // Note: We might be in a non-available status here, e.g. DISABLED due to
    // enterprise policy.
    if (IsCategoryStatusAvailable(content.status))
      NotifyNewSuggestions(category, content);
  }
}

void RemoteSuggestionsProvider::OnSnippetsStatusChanged(
    SnippetsStatus old_snippets_status,
    SnippetsStatus new_snippets_status) {
  switch (new_snippets_status) {
    case SnippetsStatus::ENABLED_AND_SIGNED_IN:
      if (old_snippets_status == SnippetsStatus::ENABLED_AND_SIGNED_OUT) {
        DCHECK(state_ == State::READY);
        // Clear nonpersonalized suggestions.
        NukeAllSnippets();
        // Fetch personalized ones.
        FetchSnippets(/*interactive_request=*/true);
      } else {
        // Do not change the status. That will be done in EnterStateReady().
        EnterState(State::READY);
      }
      break;

    case SnippetsStatus::ENABLED_AND_SIGNED_OUT:
      if (old_snippets_status == SnippetsStatus::ENABLED_AND_SIGNED_IN) {
        DCHECK(state_ == State::READY);
        // Clear personalized suggestions.
        NukeAllSnippets();
        // Fetch nonpersonalized ones.
        FetchSnippets(/*interactive_request=*/true);
      } else {
        // Do not change the status. That will be done in EnterStateReady().
        EnterState(State::READY);
      }
      break;

    case SnippetsStatus::EXPLICITLY_DISABLED:
      EnterState(State::DISABLED);
      UpdateAllCategoryStatus(CategoryStatus::CATEGORY_EXPLICITLY_DISABLED);
      break;

    case SnippetsStatus::SIGNED_OUT_AND_DISABLED:
      EnterState(State::DISABLED);
      UpdateAllCategoryStatus(CategoryStatus::SIGNED_OUT);
      break;
  }
}

void RemoteSuggestionsProvider::EnterState(State state) {
  if (state == state_)
    return;

  UMA_HISTOGRAM_ENUMERATION("NewTabPage.Snippets.EnteredState",
                            static_cast<int>(state),
                            static_cast<int>(State::COUNT));

  switch (state) {
    case State::NOT_INITED:
      // Initial state, it should not be possible to get back there.
      NOTREACHED();
      break;

    case State::READY:
      DCHECK(state_ == State::NOT_INITED || state_ == State::DISABLED);

      DVLOG(1) << "Entering state: READY";
      state_ = State::READY;
      EnterStateReady();
      break;

    case State::DISABLED:
      DCHECK(state_ == State::NOT_INITED || state_ == State::READY);

      DVLOG(1) << "Entering state: DISABLED";
      state_ = State::DISABLED;
      EnterStateDisabled();
      break;

    case State::ERROR_OCCURRED:
      DVLOG(1) << "Entering state: ERROR_OCCURRED";
      state_ = State::ERROR_OCCURRED;
      EnterStateError();
      break;

    case State::COUNT:
      NOTREACHED();
      break;
  }

  // Schedule or un-schedule background fetching after each state change.
  RescheduleFetching(false);
}

void RemoteSuggestionsProvider::NotifyNewSuggestions(
    Category category,
    const CategoryContent& content) {
  DCHECK(IsCategoryStatusAvailable(content.status));

  std::vector<ContentSuggestion> result =
      ConvertToContentSuggestions(category, content.snippets);

  DVLOG(1) << "NotifyNewSuggestions(): " << result.size()
           << " items in category " << category;
  observer()->OnNewSuggestions(this, category, std::move(result));
}

void RemoteSuggestionsProvider::UpdateCategoryStatus(Category category,
                                                     CategoryStatus status) {
  auto content_it = category_contents_.find(category);
  DCHECK(content_it != category_contents_.end());
  CategoryContent& content = content_it->second;

  if (status == content.status)
    return;

  DVLOG(1) << "UpdateCategoryStatus(): " << category.id() << ": "
           << static_cast<int>(content.status) << " -> "
           << static_cast<int>(status);
  content.status = status;
  observer()->OnCategoryStatusChanged(this, category, content.status);
}

void RemoteSuggestionsProvider::UpdateAllCategoryStatus(CategoryStatus status) {
  for (const auto& category : category_contents_) {
    UpdateCategoryStatus(category.first, status);
  }
}

namespace {

template <typename T>
typename T::const_iterator FindSnippetInContainer(
    const T& container,
    const std::string& id_within_category) {
  return std::find_if(
      container.begin(), container.end(),
      [&id_within_category](const std::unique_ptr<NTPSnippet>& snippet) {
        return snippet->id() == id_within_category;
      });
}

}  // namespace

const NTPSnippet* RemoteSuggestionsProvider::CategoryContent::FindSnippet(
    const std::string& id_within_category) const {
  // Search for the snippet in current and archived snippets.
  auto it = FindSnippetInContainer(snippets, id_within_category);
  if (it != snippets.end()) {
    return it->get();
  }
  auto archived_it = FindSnippetInContainer(archived, id_within_category);
  if (archived_it != archived.end()) {
    return archived_it->get();
  }
  auto dismissed_it = FindSnippetInContainer(dismissed, id_within_category);
  if (dismissed_it != dismissed.end()) {
    return dismissed_it->get();
  }
  return nullptr;
}

RemoteSuggestionsProvider::CategoryContent*
RemoteSuggestionsProvider::UpdateCategoryInfo(Category category,
                                              const CategoryInfo& info) {
  auto content_it = category_contents_.find(category);
  if (content_it == category_contents_.end()) {
    content_it = category_contents_
                     .insert(std::make_pair(category, CategoryContent(info)))
                     .first;
  } else {
    content_it->second.info = info;
  }
  return &content_it->second;
}

void RemoteSuggestionsProvider::RestoreCategoriesFromPrefs() {
  // This must only be called at startup, before there are any categories.
  DCHECK(category_contents_.empty());

  const base::ListValue* list =
      pref_service_->GetList(prefs::kRemoteSuggestionCategories);
  for (const std::unique_ptr<base::Value>& entry : *list) {
    const base::DictionaryValue* dict = nullptr;
    if (!entry->GetAsDictionary(&dict)) {
      DLOG(WARNING) << "Invalid category pref value: " << *entry;
      continue;
    }
    int id = 0;
    if (!dict->GetInteger(kCategoryContentId, &id)) {
      DLOG(WARNING) << "Invalid category pref value, missing '"
                    << kCategoryContentId << "': " << *entry;
      continue;
    }
    base::string16 title;
    if (!dict->GetString(kCategoryContentTitle, &title)) {
      DLOG(WARNING) << "Invalid category pref value, missing '"
                    << kCategoryContentTitle << "': " << *entry;
      continue;
    }
    bool included_in_last_server_response = false;
    if (!dict->GetBoolean(kCategoryContentProvidedByServer,
                          &included_in_last_server_response)) {
      DLOG(WARNING) << "Invalid category pref value, missing '"
                    << kCategoryContentProvidedByServer << "': " << *entry;
      continue;
    }
    bool allow_fetching_more_results = false;
    // This wasn't always around, so it's okay if it's missing.
    dict->GetBoolean(kCategoryContentAllowFetchingMore,
                     &allow_fetching_more_results);

    Category category = category_factory()->FromIDValue(id);
    // TODO(tschumann): The following has a bad smell that category
    // serialization / deserialization should not be done inside this
    // class. We should move that into a central place that also knows how to
    // parse data we received from remote backends.
    CategoryInfo info =
        category == articles_category_
            ? BuildArticleCategoryInfo(title)
            : BuildRemoteCategoryInfo(title, allow_fetching_more_results);
    CategoryContent* content = UpdateCategoryInfo(category, info);
    content->included_in_last_server_response =
        included_in_last_server_response;
  }
}

void RemoteSuggestionsProvider::StoreCategoriesToPrefs() {
  // Collect all the CategoryContents.
  std::vector<std::pair<Category, const CategoryContent*>> to_store;
  for (const auto& entry : category_contents_)
    to_store.emplace_back(entry.first, &entry.second);
  // Sort them into the proper category order.
  std::sort(to_store.begin(), to_store.end(),
            [this](const std::pair<Category, const CategoryContent*>& left,
                   const std::pair<Category, const CategoryContent*>& right) {
              return category_factory()->CompareCategories(left.first,
                                                           right.first);
            });
  // Convert the relevant info into a base::ListValue for storage.
  base::ListValue list;
  for (const auto& entry : to_store) {
    const Category& category = entry.first;
    const CategoryContent& content = *entry.second;
    auto dict = base::MakeUnique<base::DictionaryValue>();
    dict->SetInteger(kCategoryContentId, category.id());
    // TODO(tschumann): Persist other properties of the CategoryInfo.
    dict->SetString(kCategoryContentTitle, content.info.title());
    dict->SetBoolean(kCategoryContentProvidedByServer,
                     content.included_in_last_server_response);
    dict->SetBoolean(kCategoryContentAllowFetchingMore,
                     content.info.has_more_action());
    list.Append(std::move(dict));
  }
  // Finally, store the result in the pref service.
  pref_service_->Set(prefs::kRemoteSuggestionCategories, list);
}

RemoteSuggestionsProvider::CategoryContent::CategoryContent(
    const CategoryInfo& info)
    : info(info) {}

RemoteSuggestionsProvider::CategoryContent::CategoryContent(CategoryContent&&) =
    default;

RemoteSuggestionsProvider::CategoryContent::~CategoryContent() = default;

RemoteSuggestionsProvider::CategoryContent&
RemoteSuggestionsProvider::CategoryContent::operator=(CategoryContent&&) =
    default;

}  // namespace ntp_snippets
