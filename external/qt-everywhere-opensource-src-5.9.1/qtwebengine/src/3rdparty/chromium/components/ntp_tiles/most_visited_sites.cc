// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/most_visited_sites.h"

#include <algorithm>
#include <set>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "components/history/core/browser/top_sites.h"
#include "components/ntp_tiles/constants.h"
#include "components/ntp_tiles/field_trial.h"
#include "components/ntp_tiles/icon_cacher.h"
#include "components/ntp_tiles/metrics.h"
#include "components/ntp_tiles/pref_names.h"
#include "components/ntp_tiles/switches.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

using history::TopSites;
using suggestions::ChromeSuggestion;
using suggestions::SuggestionsProfile;
using suggestions::SuggestionsService;

namespace ntp_tiles {

namespace {

const base::Feature kDisplaySuggestionsServiceTiles{
    "DisplaySuggestionsServiceTiles", base::FEATURE_ENABLED_BY_DEFAULT};

// Determine whether we need any tiles from PopularSites to fill up a grid of
// |num_tiles| tiles.
bool NeedPopularSites(const PrefService* prefs, int num_tiles) {
  return prefs->GetInteger(prefs::kNumPersonalTiles) < num_tiles;
}

bool AreURLsEquivalent(const GURL& url1, const GURL& url2) {
  return url1.host_piece() == url2.host_piece() &&
         url1.path_piece() == url2.path_piece();
}

}  // namespace

MostVisitedSites::MostVisitedSites(PrefService* prefs,
                                   scoped_refptr<history::TopSites> top_sites,
                                   SuggestionsService* suggestions,
                                   std::unique_ptr<PopularSites> popular_sites,
                                   std::unique_ptr<IconCacher> icon_cacher,
                                   MostVisitedSitesSupervisor* supervisor)
    : prefs_(prefs),
      top_sites_(top_sites),
      suggestions_service_(suggestions),
      popular_sites_(std::move(popular_sites)),
      icon_cacher_(std::move(icon_cacher)),
      supervisor_(supervisor),
      observer_(nullptr),
      num_sites_(0),
      waiting_for_most_visited_sites_(true),
      waiting_for_popular_sites_(true),
      recorded_impressions_(false),
      top_sites_observer_(this),
      mv_source_(NTPTileSource::SUGGESTIONS_SERVICE),
      weak_ptr_factory_(this) {
  DCHECK(prefs_);
  // top_sites_ can be null in tests.
  // TODO(sfiera): have iOS use a dummy TopSites in its tests.
  DCHECK(suggestions_service_);
  if (supervisor_)
    supervisor_->SetObserver(this);
}

MostVisitedSites::~MostVisitedSites() {
  if (supervisor_)
    supervisor_->SetObserver(nullptr);
}

void MostVisitedSites::SetMostVisitedURLsObserver(Observer* observer,
                                                  int num_sites) {
  DCHECK(observer);
  observer_ = observer;
  num_sites_ = num_sites;

  // The order for this condition is important, ShouldShowPopularSite() should
  // always be called last to keep metrics as relevant as possible.
  if (popular_sites_ && NeedPopularSites(prefs_, num_sites_) &&
      ShouldShowPopularSites()) {
    popular_sites_->StartFetch(
        false, base::Bind(&MostVisitedSites::OnPopularSitesAvailable,
                          base::Unretained(this)));
  } else {
    waiting_for_popular_sites_ = false;
  }

  if (top_sites_) {
    // TopSites updates itself after a delay. To ensure up-to-date results,
    // force an update now.
    top_sites_->SyncWithHistory();

    // Register as TopSitesObserver so that we can update ourselves when the
    // TopSites changes.
    top_sites_observer_.Add(top_sites_.get());
  }

  suggestions_subscription_ = suggestions_service_->AddCallback(
      base::Bind(&MostVisitedSites::OnSuggestionsProfileAvailable,
                 base::Unretained(this)));

  // Immediately build the current set of tiles, getting suggestions from the
  // SuggestionsService's cache or, if that is empty, sites from TopSites.
  BuildCurrentTiles();
  // Also start a request for fresh suggestions.
  suggestions_service_->FetchSuggestionsData();
}

void MostVisitedSites::AddOrRemoveBlacklistedUrl(const GURL& url,
                                                 bool add_url) {
  if (top_sites_) {
    // Always blacklist in the local TopSites.
    if (add_url)
      top_sites_->AddBlacklistedURL(url);
    else
      top_sites_->RemoveBlacklistedURL(url);
  }

  // Only blacklist in the server-side suggestions service if it's active.
  if (mv_source_ == NTPTileSource::SUGGESTIONS_SERVICE) {
    if (add_url)
      suggestions_service_->BlacklistURL(url);
    else
      suggestions_service_->UndoBlacklistURL(url);
  }
}

void MostVisitedSites::OnBlockedSitesChanged() {
  BuildCurrentTiles();
}

// static
void MostVisitedSites::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(prefs::kNumPersonalTiles, 0);
}

void MostVisitedSites::BuildCurrentTiles() {
  // Get the current suggestions from cache. If the cache is empty, this will
  // fall back to TopSites.
  OnSuggestionsProfileAvailable(
      suggestions_service_->GetSuggestionsDataFromCache());
}

void MostVisitedSites::InitiateTopSitesQuery() {
  if (!top_sites_)
    return;
  top_sites_->GetMostVisitedURLs(
      base::Bind(&MostVisitedSites::OnMostVisitedURLsAvailable,
                 weak_ptr_factory_.GetWeakPtr()),
      false);
}

base::FilePath MostVisitedSites::GetWhitelistLargeIconPath(const GURL& url) {
  if (supervisor_) {
    for (const auto& whitelist : supervisor_->whitelists()) {
      if (AreURLsEquivalent(whitelist.entry_point, url))
        return whitelist.large_icon_path;
    }
  }
  return base::FilePath();
}

void MostVisitedSites::OnMostVisitedURLsAvailable(
    const history::MostVisitedURLList& visited_list) {
  NTPTilesVector tiles;
  size_t num_tiles =
      std::min(visited_list.size(), static_cast<size_t>(num_sites_));
  for (size_t i = 0; i < num_tiles; ++i) {
    const history::MostVisitedURL& visited = visited_list[i];
    if (visited.url.is_empty()) {
      num_tiles = i;
      break;  // This is the signal that there are no more real visited sites.
    }
    if (supervisor_ && supervisor_->IsBlocked(visited.url))
      continue;

    NTPTile tile;
    tile.title = visited.title;
    tile.url = visited.url;
    tile.source = NTPTileSource::TOP_SITES;
    tile.whitelist_icon_path = GetWhitelistLargeIconPath(visited.url);

    tiles.push_back(std::move(tile));
  }

  waiting_for_most_visited_sites_ = false;
  mv_source_ = NTPTileSource::TOP_SITES;
  SaveNewTiles(std::move(tiles));
  NotifyMostVisitedURLsObserver();
}

void MostVisitedSites::OnSuggestionsProfileAvailable(
    const SuggestionsProfile& suggestions_profile) {
  int num_tiles = suggestions_profile.suggestions_size();
  // With no server suggestions, fall back to local TopSites.
  if (num_tiles == 0 ||
      !base::FeatureList::IsEnabled(kDisplaySuggestionsServiceTiles)) {
    InitiateTopSitesQuery();
    return;
  }
  if (num_sites_ < num_tiles)
    num_tiles = num_sites_;

  NTPTilesVector tiles;
  for (int i = 0; i < num_tiles; ++i) {
    const ChromeSuggestion& suggestion_pb = suggestions_profile.suggestions(i);
    GURL url(suggestion_pb.url());
    if (supervisor_ && supervisor_->IsBlocked(url))
      continue;

    NTPTile tile;
    tile.title = base::UTF8ToUTF16(suggestion_pb.title());
    tile.url = url;
    tile.source = NTPTileSource::SUGGESTIONS_SERVICE;
    tile.whitelist_icon_path = GetWhitelistLargeIconPath(url);

    tiles.push_back(std::move(tile));
  }

  waiting_for_most_visited_sites_ = false;
  mv_source_ = NTPTileSource::SUGGESTIONS_SERVICE;
  SaveNewTiles(std::move(tiles));
  NotifyMostVisitedURLsObserver();
}

NTPTilesVector MostVisitedSites::CreateWhitelistEntryPointTiles(
    const NTPTilesVector& personal_tiles) {
  if (!supervisor_) {
    return NTPTilesVector();
  }

  size_t num_personal_tiles = personal_tiles.size();
  DCHECK_LE(num_personal_tiles, static_cast<size_t>(num_sites_));

  size_t num_whitelist_tiles = num_sites_ - num_personal_tiles;
  NTPTilesVector whitelist_tiles;

  std::set<std::string> personal_hosts;
  for (const auto& tile : personal_tiles)
    personal_hosts.insert(tile.url.host());

  for (const auto& whitelist : supervisor_->whitelists()) {
    // Skip blacklisted sites.
    if (top_sites_ && top_sites_->IsBlacklisted(whitelist.entry_point))
      continue;

    // Skip tiles already present.
    if (personal_hosts.find(whitelist.entry_point.host()) !=
        personal_hosts.end())
      continue;

    // Skip whitelist entry points that are manually blocked.
    if (supervisor_->IsBlocked(whitelist.entry_point))
      continue;

    NTPTile tile;
    tile.title = whitelist.title;
    tile.url = whitelist.entry_point;
    tile.source = NTPTileSource::WHITELIST;
    tile.whitelist_icon_path = whitelist.large_icon_path;

    whitelist_tiles.push_back(std::move(tile));
    if (whitelist_tiles.size() >= num_whitelist_tiles)
      break;
  }

  return whitelist_tiles;
}

NTPTilesVector MostVisitedSites::CreatePopularSitesTiles(
    const NTPTilesVector& personal_tiles,
    const NTPTilesVector& whitelist_tiles) {
  // For child accounts popular sites tiles will not be added.
  if (supervisor_ && supervisor_->IsChildProfile())
    return NTPTilesVector();

  size_t num_tiles = personal_tiles.size() + whitelist_tiles.size();
  DCHECK_LE(num_tiles, static_cast<size_t>(num_sites_));

  // Collect non-blacklisted popular suggestions, skipping those already present
  // in the personal suggestions.
  size_t num_popular_sites_tiles = num_sites_ - num_tiles;
  NTPTilesVector popular_sites_tiles;

  if (num_popular_sites_tiles > 0 && popular_sites_) {
    std::set<std::string> hosts;
    for (const auto& tile : personal_tiles)
      hosts.insert(tile.url.host());
    for (const auto& tile : whitelist_tiles)
      hosts.insert(tile.url.host());
    for (const PopularSites::Site& popular_site : popular_sites_->sites()) {
      // Skip blacklisted sites.
      if (top_sites_ && top_sites_->IsBlacklisted(popular_site.url))
        continue;
      std::string host = popular_site.url.host();
      // Skip tiles already present in personal or whitelists.
      if (hosts.find(host) != hosts.end())
        continue;

      NTPTile tile;
      tile.title = popular_site.title;
      tile.url = GURL(popular_site.url);
      tile.source = NTPTileSource::POPULAR;

      popular_sites_tiles.push_back(std::move(tile));
      icon_cacher_->StartFetch(
          popular_site, base::Bind(&MostVisitedSites::OnIconMadeAvailable,
                                   base::Unretained(this), popular_site.url));
      if (popular_sites_tiles.size() >= num_popular_sites_tiles)
        break;
    }
  }
  return popular_sites_tiles;
}

void MostVisitedSites::SaveNewTiles(NTPTilesVector personal_tiles) {
  NTPTilesVector whitelist_tiles =
      CreateWhitelistEntryPointTiles(personal_tiles);
  NTPTilesVector popular_sites_tiles =
      CreatePopularSitesTiles(personal_tiles, whitelist_tiles);

  size_t num_actual_tiles = personal_tiles.size() + whitelist_tiles.size() +
                            popular_sites_tiles.size();
  DCHECK_LE(num_actual_tiles, static_cast<size_t>(num_sites_));

  current_tiles_ =
      MergeTiles(std::move(personal_tiles), std::move(whitelist_tiles),
                 std::move(popular_sites_tiles));
  DCHECK_EQ(num_actual_tiles, current_tiles_.size());

  int num_personal_tiles = 0;
  for (const auto& tile : current_tiles_) {
    if (tile.source != NTPTileSource::POPULAR)
      num_personal_tiles++;
  }
  prefs_->SetInteger(prefs::kNumPersonalTiles, num_personal_tiles);
}

// static
NTPTilesVector MostVisitedSites::MergeTiles(NTPTilesVector personal_tiles,
                                            NTPTilesVector whitelist_tiles,
                                            NTPTilesVector popular_tiles) {
  NTPTilesVector merged_tiles;
  std::move(personal_tiles.begin(), personal_tiles.end(),
            std::back_inserter(merged_tiles));
  std::move(whitelist_tiles.begin(), whitelist_tiles.end(),
            std::back_inserter(merged_tiles));
  std::move(popular_tiles.begin(), popular_tiles.end(),
            std::back_inserter(merged_tiles));
  return merged_tiles;
}

void MostVisitedSites::NotifyMostVisitedURLsObserver() {
  if (!waiting_for_most_visited_sites_ && !waiting_for_popular_sites_ &&
      !recorded_impressions_) {
    // TODO(treib): Move this out of here. crbug.com/514752
    int num_tiles = static_cast<int>(current_tiles_.size());
    for (int i = 0; i < num_tiles; i++)
      metrics::RecordTileImpression(i, current_tiles_[i].source);
    metrics::RecordPageImpression(num_tiles);
    recorded_impressions_ = true;
  }

  if (!observer_)
    return;

  observer_->OnMostVisitedURLsAvailable(current_tiles_);
}

void MostVisitedSites::OnPopularSitesAvailable(bool success) {
  waiting_for_popular_sites_ = false;

  if (!success) {
    LOG(WARNING) << "Download of popular sites failed";
    return;
  }

  // Re-build the tile list. Once done, this will notify the observer.
  BuildCurrentTiles();
}

void MostVisitedSites::OnIconMadeAvailable(const GURL& site_url,
                                           bool newly_available) {
  if (newly_available)
    observer_->OnIconMadeAvailable(site_url);
}

void MostVisitedSites::TopSitesLoaded(TopSites* top_sites) {}

void MostVisitedSites::TopSitesChanged(TopSites* top_sites,
                                       ChangeReason change_reason) {
  if (mv_source_ == NTPTileSource::TOP_SITES) {
    // The displayed tiles are invalidated.
    InitiateTopSitesQuery();
  }
}

}  // namespace ntp_tiles
