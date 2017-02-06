// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_TYPES_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_TYPES_H_

#include <stddef.h>
#include <stdint.h>

#include <deque>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/stack_container.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "components/favicon_base/favicon_types.h"
#include "components/history/core/browser/history_context.h"
#include "components/history/core/browser/url_row.h"
#include "components/history/core/common/thumbnail_score.h"
#include "components/query_parser/query_parser.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace history {

// Forward declaration for friend statements.
class HistoryBackend;
class PageUsageData;
class URLDatabase;

// Container for a list of URLs.
typedef std::vector<GURL> RedirectList;

typedef int64_t FaviconBitmapID;  // Identifier for a bitmap in a favicon.
typedef int64_t SegmentID;        // URL segments for the most visited view.
typedef int64_t IconMappingID;    // For page url and icon mapping.

// The enumeration of all possible sources of visits is listed below.
// The source will be propagated along with a URL or a visit item
// and eventually be stored in the history database,
// visit_source table specifically.
// Different from page transition types, they describe the origins of visits.
// (Warning): Please don't change any existing values while it is ok to add
// new values when needed.
enum VisitSource {
  SOURCE_SYNCED = 0,         // Synchronized from somewhere else.
  SOURCE_BROWSED = 1,        // User browsed.
  SOURCE_EXTENSION = 2,      // Added by an extension.
  SOURCE_FIREFOX_IMPORTED = 3,
  SOURCE_IE_IMPORTED = 4,
  SOURCE_SAFARI_IMPORTED = 5,
};

typedef int64_t VisitID;
// Structure to hold the mapping between each visit's id and its source.
typedef std::map<VisitID, VisitSource> VisitSourceMap;

// VisitRow -------------------------------------------------------------------

// Holds all information associated with a specific visit. A visit holds time
// and referrer information for one time a URL is visited.
class VisitRow {
 public:
  VisitRow();
  VisitRow(URLID arg_url_id,
           base::Time arg_visit_time,
           VisitID arg_referring_visit,
           ui::PageTransition arg_transition,
           SegmentID arg_segment_id);
  ~VisitRow();

  // ID of this row (visit ID, used a a referrer for other visits).
  VisitID visit_id;

  // Row ID into the URL table of the URL that this page is.
  URLID url_id;

  base::Time visit_time;

  // Indicates another visit that was the referring page for this one.
  // 0 indicates no referrer.
  VisitID referring_visit;

  // A combination of bits from PageTransition.
  ui::PageTransition transition;

  // The segment id (see visitsegment_database.*).
  // If 0, the segment id is null in the table.
  SegmentID segment_id;

  // Record how much time a user has this visit starting from the user
  // opened this visit to the user closed or ended this visit.
  // This includes both active and inactive time as long as
  // the visit was present.
  base::TimeDelta visit_duration;

  // Compares two visits based on dates, for sorting.
  bool operator<(const VisitRow& other) {
    return visit_time < other.visit_time;
  }

  // We allow the implicit copy constuctor and operator=.
};

// We pass around vectors of visits a lot
typedef std::vector<VisitRow> VisitVector;

// The basic information associated with a visit (timestamp, type of visit),
// used by HistoryBackend::AddVisits() to create new visits for a URL.
typedef std::pair<base::Time, ui::PageTransition> VisitInfo;

// PageVisit ------------------------------------------------------------------

// Represents a simplified version of a visit for external users. Normally,
// views are only interested in the time, and not the other information
// associated with a VisitRow.
struct PageVisit {
  URLID page_id;
  base::Time visit_time;
};

// QueryResults ----------------------------------------------------------------

// Encapsulates the results of a history query. It supports an ordered list of
// URLResult objects, plus an efficient way of looking up the index of each time
// a given URL appears in those results.
class QueryResults {
 public:
  typedef std::vector<URLResult*> URLResultVector;

  QueryResults();
  ~QueryResults();

  // Indicates the first time that the query includes results for (queries are
  // clipped at the beginning, so it will always include to the end of the time
  // queried).
  //
  // If the number of results was clipped as a result of the max count, this
  // will be the time of the first query returned. If there were fewer results
  // than we were allowed to return, this represents the first date considered
  // in the query (this will be before the first result if there was time
  // queried with no results).
  //
  // TODO(brettw): bug 1203054: This field is not currently set properly! Do
  // not use until the bug is fixed.
  base::Time first_time_searched() const { return first_time_searched_; }
  void set_first_time_searched(base::Time t) { first_time_searched_ = t; }
  // Note: If you need end_time_searched, it can be added.

  void set_reached_beginning(bool reached) { reached_beginning_ = reached; }
  bool reached_beginning() { return reached_beginning_; }

  size_t size() const { return results_.size(); }
  bool empty() const { return results_.empty(); }

  URLResult& back() { return *results_.back(); }
  const URLResult& back() const { return *results_.back(); }

  URLResult& operator[](size_t i) { return *results_[i]; }
  const URLResult& operator[](size_t i) const { return *results_[i]; }

  URLResultVector::const_iterator begin() const { return results_.begin(); }
  URLResultVector::const_iterator end() const { return results_.end(); }
  URLResultVector::const_reverse_iterator rbegin() const {
    return results_.rbegin();
  }
  URLResultVector::const_reverse_iterator rend() const {
    return results_.rend();
  }

  // Returns a pointer to the beginning of an array of all matching indices
  // for entries with the given URL. The array will be |*num_matches| long.
  // |num_matches| can be NULL if the caller is not interested in the number of
  // results (commonly it will only be interested in the first one and can test
  // the pointer for NULL).
  //
  // When there is no match, it will return NULL and |*num_matches| will be 0.
  const size_t* MatchesForURL(const GURL& url, size_t* num_matches) const;

  // Swaps the current result with another. This allows ownership to be
  // efficiently transferred without copying.
  void Swap(QueryResults* other);

  // Adds the given result to the map, using swap() on the members to avoid
  // copying (there are a lot of strings and vectors). This means the parameter
  // object will be cleared after this call.
  void AppendURLBySwapping(URLResult* result);

  // Removes all instances of the given URL from the result set.
  void DeleteURL(const GURL& url);

  // Deletes the given range of items in the result set.
  void DeleteRange(size_t begin, size_t end);

 private:
  // Maps the given URL to a list of indices into results_ which identify each
  // time an entry with that URL appears. Normally, each URL will have one or
  // very few indices after it, so we optimize this to use statically allocated
  // memory when possible.
  typedef std::map<GURL, base::StackVector<size_t, 4> > URLToResultIndices;

  // Inserts an entry into the |url_to_results_| map saying that the given URL
  // is at the given index in the results_.
  void AddURLUsageAtIndex(const GURL& url, size_t index);

  // Adds |delta| to each index in url_to_results_ in the range [begin,end]
  // (this is inclusive). This is used when inserting or deleting.
  void AdjustResultMap(size_t begin, size_t end, ptrdiff_t delta);

  base::Time first_time_searched_;

  // Whether the query reaches the beginning of the database.
  bool reached_beginning_;

  // The ordered list of results. The pointers inside this are owned by this
  // QueryResults object.
  ScopedVector<URLResult> results_;

  // Maps URLs to entries in results_.
  URLToResultIndices url_to_results_;

  DISALLOW_COPY_AND_ASSIGN(QueryResults);
};

// QueryOptions ----------------------------------------------------------------

struct QueryOptions {
  QueryOptions();

  // The time range to search for matches in. The beginning is inclusive and
  // the ending is exclusive. Either one (or both) may be null.
  //
  // This will match only the one recent visit of a URL. For text search
  // queries, if the URL was visited in the given time period, but has also
  // been visited more recently than that, it will not be returned. When the
  // text query is empty, this will return the most recent visit within the
  // time range.
  base::Time begin_time;
  base::Time end_time;

  // Sets the query time to the last |days_ago| days to the present time.
  void SetRecentDayRange(int days_ago);

  // The maximum number of results to return. The results will be sorted with
  // the most recent first, so older results may not be returned if there is not
  // enough room. When 0, this will return everything (the default).
  int max_count;

  enum DuplicateHandling {
    // Omit visits for which there is a more recent visit to the same URL.
    // Each URL in the results will appear only once.
    REMOVE_ALL_DUPLICATES,

    // Omit visits for which there is a more recent visit to the same URL on
    // the same day. Each URL will appear no more than once per day, where the
    // day is defined by the local timezone.
    REMOVE_DUPLICATES_PER_DAY,

    // Return all visits without deduping.
    KEEP_ALL_DUPLICATES
  };

  // Allows the caller to specify how duplicate URLs in the result set should
  // be handled. The default is REMOVE_DUPLICATES.
  DuplicateHandling duplicate_policy;

  // Allows the caller to specify the matching algorithm for text queries.
  query_parser::MatchingAlgorithm matching_algorithm;

  // Helpers to get the effective parameters values, since a value of 0 means
  // "unspecified".
  int EffectiveMaxCount() const;
  int64_t EffectiveBeginTime() const;
  int64_t EffectiveEndTime() const;
};

// QueryURLResult -------------------------------------------------------------

// QueryURLResult encapsulates the result of a call to HistoryBackend::QueryURL.
struct QueryURLResult {
  QueryURLResult();
  ~QueryURLResult();

  // Indicates whether the call to HistoryBackend::QueryURL was successfull
  // or not. If false, then both |row| and |visits| fields are undefined.
  bool success;
  URLRow row;
  VisitVector visits;
};

// VisibleVisitCountToHostResult ----------------------------------------------

// VisibleVisitCountToHostResult encapsulates the result of a call to
// HistoryBackend::GetVisibleVisitCountToHost.
struct VisibleVisitCountToHostResult {
  // Indicates whether the call to HistoryBackend::GetVisibleVisitCountToHost
  // was successful or not. If false, then both |count| and |first_visit| are
  // undefined.
  bool success;
  int count;
  base::Time first_visit;
};

// MostVisitedURL --------------------------------------------------------------

// Holds the per-URL information of the most visited query.
struct MostVisitedURL {
  MostVisitedURL();
  MostVisitedURL(const GURL& url, const base::string16& title);
  MostVisitedURL(const GURL& url,
                 const base::string16& title,
                 const base::Time& last_forced_time);
  MostVisitedURL(const MostVisitedURL& other);
  ~MostVisitedURL();

  GURL url;
  base::string16 title;

  // If this is a URL for which we want to force a thumbnail, records the last
  // time it was forced so we can evict it when more recent URLs are requested.
  // If it's not a forced thumbnail, keep a time of 0.
  base::Time last_forced_time;

  RedirectList redirects;

  bool operator==(const MostVisitedURL& other) {
    return url == other.url;
  }
};

// FilteredURL -----------------------------------------------------------------

// Holds the per-URL information of the filterd url query.
struct FilteredURL {
  struct ExtendedInfo {
    ExtendedInfo();
    // The absolute number of visits.
    unsigned int total_visits;
    // The number of visits, as seen by the Most Visited NTP pane.
    unsigned int visits;
    // The total number of seconds that the page was open.
    int64_t duration_opened;
    // The time when the page was last visited.
    base::Time last_visit_time;
  };

  FilteredURL();
  explicit FilteredURL(const PageUsageData& data);
  ~FilteredURL();

  GURL url;
  base::string16 title;
  double score;
  ExtendedInfo extended_info;
};

// Navigation -----------------------------------------------------------------

// Marshalling structure for AddPage.
struct HistoryAddPageArgs {
  // The default constructor is equivalent to:
  //
  //   HistoryAddPageArgs(
  //       GURL(), base::Time(), NULL, 0, GURL(),
  //       RedirectList(), ui::PAGE_TRANSITION_LINK,
  //       SOURCE_BROWSED, false)
  HistoryAddPageArgs();
  HistoryAddPageArgs(const GURL& url,
                     base::Time time,
                     ContextID context_id,
                     int nav_entry_id,
                     const GURL& referrer,
                     const RedirectList& redirects,
                     ui::PageTransition transition,
                     VisitSource source,
                     bool did_replace_entry);
  HistoryAddPageArgs(const HistoryAddPageArgs& other);
  ~HistoryAddPageArgs();

  GURL url;
  base::Time time;
  ContextID context_id;
  int nav_entry_id;
  GURL referrer;
  RedirectList redirects;
  ui::PageTransition transition;
  VisitSource visit_source;
  bool did_replace_entry;
};

// TopSites -------------------------------------------------------------------

typedef std::vector<MostVisitedURL> MostVisitedURLList;
typedef std::vector<FilteredURL> FilteredURLList;

// Used by TopSites to store the thumbnails.
struct Images {
  Images();
  Images(const Images& other);
  ~Images();

  scoped_refptr<base::RefCountedMemory> thumbnail;
  ThumbnailScore thumbnail_score;

  // TODO(brettw): this will eventually store the favicon.
  // scoped_refptr<base::RefCountedBytes> favicon;
};

struct MostVisitedURLWithRank {
  MostVisitedURL url;
  int rank;
};

typedef std::vector<MostVisitedURLWithRank> MostVisitedURLWithRankList;

struct TopSitesDelta {
  TopSitesDelta();
  TopSitesDelta(const TopSitesDelta& other);
  ~TopSitesDelta();

  MostVisitedURLList deleted;
  MostVisitedURLWithRankList added;
  MostVisitedURLWithRankList moved;
};

typedef std::map<GURL, scoped_refptr<base::RefCountedBytes> > URLToThumbnailMap;

// Used when migrating most visited thumbnails out of history and into topsites.
struct ThumbnailMigration {
  ThumbnailMigration();
  ~ThumbnailMigration();

  MostVisitedURLList most_visited;
  URLToThumbnailMap url_to_thumbnail_map;
};

typedef std::map<GURL, Images> URLToImagesMap;

class MostVisitedThumbnails
    : public base::RefCountedThreadSafe<MostVisitedThumbnails> {
 public:
  MostVisitedThumbnails();

  MostVisitedURLList most_visited;
  URLToImagesMap url_to_images_map;

 private:
  friend class base::RefCountedThreadSafe<MostVisitedThumbnails>;
  virtual ~MostVisitedThumbnails();

  DISALLOW_COPY_AND_ASSIGN(MostVisitedThumbnails);
};

// Map from host to visit count, sorted by visit count descending.
typedef std::vector<std::pair<std::string, int>> TopHostsList;

// Map from origins to a count of matching URLs and the last visited time to any
// URL under that origin.
typedef std::map<GURL, std::pair<int, base::Time>> OriginCountAndLastVisitMap;

// Statistics -----------------------------------------------------------------

// HistoryCountResult encapsulates the result of a call to
// HistoryBackend::GetHistoryCount.
struct HistoryCountResult {
  // Indicates whether the call to HistoryBackend::GetHistoryCount was
  // successful or not. If false, then |count| is undefined.
  bool success;
  int count;
};

// Favicons -------------------------------------------------------------------

// Used for the mapping between the page and icon.
struct IconMapping {
  IconMapping();
  ~IconMapping();

  // The unique id of the mapping.
  IconMappingID mapping_id;

  // The url of a web page.
  GURL page_url;

  // The unique id of the icon.
  favicon_base::FaviconID icon_id;

  // The url of the icon.
  GURL icon_url;

  // The type of icon.
  favicon_base::IconType icon_type;
};

// Defines a favicon bitmap and its associated pixel size.
struct FaviconBitmapIDSize {
  FaviconBitmapIDSize();
  ~FaviconBitmapIDSize();

  // The unique id of the favicon bitmap.
  FaviconBitmapID bitmap_id;

  // The pixel dimensions of the associated bitmap.
  gfx::Size pixel_size;
};

// Defines a favicon bitmap stored in the history backend.
struct FaviconBitmap {
  FaviconBitmap();
  FaviconBitmap(const FaviconBitmap& other);
  ~FaviconBitmap();

  // The unique id of the bitmap.
  FaviconBitmapID bitmap_id;

  // The id of the favicon to which the bitmap belongs to.
  favicon_base::FaviconID icon_id;

  // Time at which |bitmap_data| was last updated.
  base::Time last_updated;

  // Time at which |bitmap_data| was last requested.
  base::Time last_requested;

  // The bits of the bitmap.
  scoped_refptr<base::RefCountedMemory> bitmap_data;

  // The pixel dimensions of bitmap_data.
  gfx::Size pixel_size;
};

struct ExpireHistoryArgs {
  ExpireHistoryArgs();
  ExpireHistoryArgs(const ExpireHistoryArgs& other);
  ~ExpireHistoryArgs();

  // Sets |begin_time| and |end_time| to the beginning and end of the day (in
  // local time) on which |time| occurs.
  void SetTimeRangeForOneDay(base::Time time);

  std::set<GURL> urls;
  base::Time begin_time;
  base::Time end_time;
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_TYPES_H_
