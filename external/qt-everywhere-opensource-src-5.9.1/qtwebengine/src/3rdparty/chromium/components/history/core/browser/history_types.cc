// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/history_types.h"

#include <limits>

#include "base/logging.h"
#include "base/stl_util.h"
#include "components/history/core/browser/page_usage_data.h"

namespace history {

// VisitRow --------------------------------------------------------------------

VisitRow::VisitRow()
    : visit_id(0),
      url_id(0),
      referring_visit(0),
      transition(ui::PAGE_TRANSITION_LINK),
      segment_id(0) {
}

VisitRow::VisitRow(URLID arg_url_id,
                   base::Time arg_visit_time,
                   VisitID arg_referring_visit,
                   ui::PageTransition arg_transition,
                   SegmentID arg_segment_id)
    : visit_id(0),
      url_id(arg_url_id),
      visit_time(arg_visit_time),
      referring_visit(arg_referring_visit),
      transition(arg_transition),
      segment_id(arg_segment_id) {
}

VisitRow::~VisitRow() {
}

// QueryResults ----------------------------------------------------------------

QueryResults::QueryResults() : reached_beginning_(false) {
}

QueryResults::~QueryResults() {}

const size_t* QueryResults::MatchesForURL(const GURL& url,
                                          size_t* num_matches) const {
  URLToResultIndices::const_iterator found = url_to_results_.find(url);
  if (found == url_to_results_.end()) {
    if (num_matches)
      *num_matches = 0;
    return NULL;
  }

  // All entries in the map should have at least one index, otherwise it
  // shouldn't be in the map.
  DCHECK(!found->second->empty());
  if (num_matches)
    *num_matches = found->second->size();
  return &found->second->front();
}

void QueryResults::Swap(QueryResults* other) {
  std::swap(first_time_searched_, other->first_time_searched_);
  std::swap(reached_beginning_, other->reached_beginning_);
  results_.swap(other->results_);
  url_to_results_.swap(other->url_to_results_);
}

void QueryResults::AppendURLBySwapping(URLResult* result) {
  URLResult* new_result = new URLResult;
  new_result->SwapResult(result);

  results_.push_back(new_result);
  AddURLUsageAtIndex(new_result->url(), results_.size() - 1);
}

void QueryResults::DeleteURL(const GURL& url) {
  // Delete all instances of this URL. We re-query each time since each
  // mutation will cause the indices to change.
  while (const size_t* match_indices = MatchesForURL(url, NULL))
    DeleteRange(*match_indices, *match_indices);
}

void QueryResults::DeleteRange(size_t begin, size_t end) {
  DCHECK(begin <= end && begin < size() && end < size());

  // First delete the pointers in the given range and store all the URLs that
  // were modified. We will delete references to these later.
  std::set<GURL> urls_modified;
  for (size_t i = begin; i <= end; i++) {
    urls_modified.insert(results_[i]->url());
  }

  // Now just delete that range in the vector en masse (the STL ending is
  // exclusive, while ours is inclusive, hence the +1).
  results_.erase(results_.begin() + begin, results_.begin() + end + 1);

  // Delete the indicies referencing the deleted entries.
  for (std::set<GURL>::const_iterator url = urls_modified.begin();
       url != urls_modified.end(); ++url) {
    URLToResultIndices::iterator found = url_to_results_.find(*url);
    if (found == url_to_results_.end()) {
      NOTREACHED();
      continue;
    }

    // Need a signed loop type since we do -- which may take us to -1.
    for (int match = 0; match < static_cast<int>(found->second->size());
         match++) {
      if (found->second[match] >= begin && found->second[match] <= end) {
        // Remove this referece from the list.
        found->second->erase(found->second->begin() + match);
        match--;
      }
    }

    // Clear out an empty lists if we just made one.
    if (found->second->empty())
      url_to_results_.erase(found);
  }

  // Shift all other indices over to account for the removed ones.
  AdjustResultMap(end + 1, std::numeric_limits<size_t>::max(),
                  -static_cast<ptrdiff_t>(end - begin + 1));
}

void QueryResults::AddURLUsageAtIndex(const GURL& url, size_t index) {
  URLToResultIndices::iterator found = url_to_results_.find(url);
  if (found != url_to_results_.end()) {
    // The URL is already in the list, so we can just append the new index.
    found->second->push_back(index);
    return;
  }

  // Need to add a new entry for this URL.
  base::StackVector<size_t, 4> new_list;
  new_list->push_back(index);
  url_to_results_[url] = new_list;
}

void QueryResults::AdjustResultMap(size_t begin, size_t end, ptrdiff_t delta) {
  for (URLToResultIndices::iterator i = url_to_results_.begin();
       i != url_to_results_.end(); ++i) {
    for (size_t match = 0; match < i->second->size(); match++) {
      size_t match_index = i->second[match];
      if (match_index >= begin && match_index <= end)
        i->second[match] += delta;
    }
  }
}

// QueryOptions ----------------------------------------------------------------

QueryOptions::QueryOptions()
    : max_count(0),
      duplicate_policy(QueryOptions::REMOVE_ALL_DUPLICATES),
      matching_algorithm(query_parser::MatchingAlgorithm::DEFAULT) {}

void QueryOptions::SetRecentDayRange(int days_ago) {
  end_time = base::Time::Now();
  begin_time = end_time - base::TimeDelta::FromDays(days_ago);
}

int64_t QueryOptions::EffectiveBeginTime() const {
  return begin_time.ToInternalValue();
}

int64_t QueryOptions::EffectiveEndTime() const {
  return end_time.is_null() ? std::numeric_limits<int64_t>::max()
                            : end_time.ToInternalValue();
}

int QueryOptions::EffectiveMaxCount() const {
  return max_count ? max_count : std::numeric_limits<int>::max();
}

// QueryURLResult -------------------------------------------------------------

QueryURLResult::QueryURLResult() : success(false) {
}

QueryURLResult::~QueryURLResult() {
}

// MostVisitedURL --------------------------------------------------------------

MostVisitedURL::MostVisitedURL() {}

MostVisitedURL::MostVisitedURL(const GURL& url,
                               const base::string16& title)
    : url(url),
      title(title) {
}

MostVisitedURL::MostVisitedURL(const GURL& url,
                               const base::string16& title,
                               const base::Time& last_forced_time)
    : url(url),
      title(title),
      last_forced_time(last_forced_time) {
}

MostVisitedURL::MostVisitedURL(const MostVisitedURL& other) = default;

MostVisitedURL::~MostVisitedURL() {}

// FilteredURL -----------------------------------------------------------------

FilteredURL::FilteredURL() : score(0.0) {}

FilteredURL::FilteredURL(const PageUsageData& page_data)
    : url(page_data.GetURL()),
      title(page_data.GetTitle()),
      score(page_data.GetScore()) {
}

FilteredURL::~FilteredURL() {}

// FilteredURL::ExtendedInfo ---------------------------------------------------

FilteredURL::ExtendedInfo::ExtendedInfo()
    : total_visits(0),
      visits(0),
      duration_opened(0) {
}

// Images ---------------------------------------------------------------------

Images::Images() {}

Images::Images(const Images& other) = default;

Images::~Images() {}

// TopSitesDelta --------------------------------------------------------------

TopSitesDelta::TopSitesDelta() {}

TopSitesDelta::TopSitesDelta(const TopSitesDelta& other) = default;

TopSitesDelta::~TopSitesDelta() {}

// HistoryAddPageArgs ---------------------------------------------------------

HistoryAddPageArgs::HistoryAddPageArgs()
    : HistoryAddPageArgs(GURL(),
                         base::Time(),
                         nullptr,
                         0,
                         GURL(),
                         RedirectList(),
                         ui::PAGE_TRANSITION_LINK,
                         SOURCE_BROWSED,
                         false,
                         true) {
}

HistoryAddPageArgs::HistoryAddPageArgs(const GURL& url,
                                       base::Time time,
                                       ContextID context_id,
                                       int nav_entry_id,
                                       const GURL& referrer,
                                       const RedirectList& redirects,
                                       ui::PageTransition transition,
                                       VisitSource source,
                                       bool did_replace_entry,
                                       bool consider_for_ntp_most_visited)
    : url(url),
      time(time),
      context_id(context_id),
      nav_entry_id(nav_entry_id),
      referrer(referrer),
      redirects(redirects),
      transition(transition),
      visit_source(source),
      did_replace_entry(did_replace_entry),
      consider_for_ntp_most_visited(consider_for_ntp_most_visited) {
}

HistoryAddPageArgs::HistoryAddPageArgs(const HistoryAddPageArgs& other) =
    default;

HistoryAddPageArgs::~HistoryAddPageArgs() {}

// ThumbnailMigration ---------------------------------------------------------

ThumbnailMigration::ThumbnailMigration() {}

ThumbnailMigration::~ThumbnailMigration() {}

// MostVisitedThumbnails ------------------------------------------------------

MostVisitedThumbnails::MostVisitedThumbnails() {}

MostVisitedThumbnails::~MostVisitedThumbnails() {}

// IconMapping ----------------------------------------------------------------

IconMapping::IconMapping()
    : mapping_id(0), icon_id(0), icon_type(favicon_base::INVALID_ICON) {}

IconMapping::~IconMapping() {}

// FaviconBitmapIDSize ---------------------------------------------------------

FaviconBitmapIDSize::FaviconBitmapIDSize()
    : bitmap_id(0) {
}

FaviconBitmapIDSize::~FaviconBitmapIDSize() {
}

// FaviconBitmap --------------------------------------------------------------

FaviconBitmap::FaviconBitmap()
    : bitmap_id(0),
      icon_id(0) {
}

FaviconBitmap::FaviconBitmap(const FaviconBitmap& other) = default;

FaviconBitmap::~FaviconBitmap() {
}

// ExpireHistoryArgs ----------------------------------------------------------

ExpireHistoryArgs::ExpireHistoryArgs() {
}

ExpireHistoryArgs::ExpireHistoryArgs(const ExpireHistoryArgs& other) = default;

ExpireHistoryArgs::~ExpireHistoryArgs() {
}

void ExpireHistoryArgs::SetTimeRangeForOneDay(base::Time time) {
  begin_time = time.LocalMidnight();

  // Due to DST, leap seconds, etc., the next day at midnight may be more than
  // 24 hours away, so add 36 hours and round back down to midnight.
  end_time = (begin_time + base::TimeDelta::FromHours(36)).LocalMidnight();
}

}  // namespace history
