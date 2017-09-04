// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_TILES_MOST_VISITED_SITES_H_
#define COMPONENTS_NTP_TILES_MOST_VISITED_SITES_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/strings/string16.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/top_sites_observer.h"
#include "components/ntp_tiles/ntp_tile.h"
#include "components/ntp_tiles/popular_sites.h"
#include "components/suggestions/proto/suggestions.pb.h"
#include "components/suggestions/suggestions_service.h"
#include "url/gurl.h"

namespace history {
class TopSites;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace ntp_tiles {

class IconCacher;

// Shim interface for SupervisedUserService.
class MostVisitedSitesSupervisor {
 public:
  struct Whitelist {
    base::string16 title;
    GURL entry_point;
    base::FilePath large_icon_path;
  };

  class Observer {
   public:
    virtual void OnBlockedSitesChanged() = 0;

   protected:
    ~Observer() {}
  };

  // Pass non-null to set observer, or null to remove observer.
  // If setting observer, there must not yet be an observer set.
  // If removing observer, there must already be one to remove.
  // Does not take ownership. Observer must outlive this object.
  virtual void SetObserver(Observer* new_observer) = 0;

  // If true, |url| should not be shown on the NTP.
  virtual bool IsBlocked(const GURL& url) = 0;

  // Explicitly-specified sites to show on NTP.
  virtual std::vector<Whitelist> whitelists() = 0;

  // If true, be conservative about suggesting sites from outside sources.
  virtual bool IsChildProfile() = 0;

 protected:
  virtual ~MostVisitedSitesSupervisor() {}
};

// Tracks the list of most visited sites and their thumbnails.
class MostVisitedSites : public history::TopSitesObserver,
                         public MostVisitedSitesSupervisor::Observer {
 public:
  using PopularSitesVector = std::vector<PopularSites::Site>;

  // The observer to be notified when the list of most visited sites changes.
  class Observer {
   public:
    virtual void OnMostVisitedURLsAvailable(const NTPTilesVector& tiles) = 0;
    virtual void OnIconMadeAvailable(const GURL& site_url) = 0;

   protected:
    virtual ~Observer() {}
  };

  // Construct a MostVisitedSites instance.
  //
  // |prefs| and |suggestions| are required and may not be null. |top_sites|,
  // |popular_sites|, and |supervisor| are optional and if null the associated
  // features will be disabled.
  MostVisitedSites(PrefService* prefs,
                   scoped_refptr<history::TopSites> top_sites,
                   suggestions::SuggestionsService* suggestions,
                   std::unique_ptr<PopularSites> popular_sites,
                   std::unique_ptr<IconCacher> icon_cacher,
                   MostVisitedSitesSupervisor* supervisor);

  ~MostVisitedSites() override;

  // Does not take ownership of |observer|, which must outlive this object and
  // must not be null.
  void SetMostVisitedURLsObserver(Observer* observer, int num_sites);

  void AddOrRemoveBlacklistedUrl(const GURL& url, bool add_url);

  // MostVisitedSitesSupervisor::Observer implementation.
  void OnBlockedSitesChanged() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 private:
  friend class MostVisitedSitesTest;

  void BuildCurrentTiles();

  // Initialize the query to Top Sites. Called if the SuggestionsService
  // returned no data.
  void InitiateTopSitesQuery();

  // If there's a whitelist entry point for the URL, return the large icon path.
  base::FilePath GetWhitelistLargeIconPath(const GURL& url);

  // Callback for when data is available from TopSites.
  void OnMostVisitedURLsAvailable(
      const history::MostVisitedURLList& visited_list);

  // Callback for when data is available from the SuggestionsService.
  void OnSuggestionsProfileAvailable(
      const suggestions::SuggestionsProfile& suggestions_profile);

  // Takes the personal suggestions and creates whitelist entry point
  // suggestions if necessary.
  NTPTilesVector CreateWhitelistEntryPointTiles(
      const NTPTilesVector& personal_tiles);

  // Takes the personal and whitelist tiles and creates popular tiles if
  // necessary.
  NTPTilesVector CreatePopularSitesTiles(const NTPTilesVector& personal_tiles,
                                         const NTPTilesVector& whitelist_tiles);

  // Takes the personal tiles, creates and merges in whitelist and popular tiles
  // if appropriate, and saves the new tiles.
  void SaveNewTiles(NTPTilesVector personal_tiles);

  // Workhorse for SaveNewTiles above. Implemented as a separate static method
  // for ease of testing.
  static NTPTilesVector MergeTiles(NTPTilesVector personal_tiles,
                                   NTPTilesVector whitelist_tiles,
                                   NTPTilesVector popular_tiles);

  // Notifies the observer about the availability of tiles.
  // Also records impressions UMA if not done already.
  void NotifyMostVisitedURLsObserver();

  void OnPopularSitesAvailable(bool success);

  void OnIconMadeAvailable(const GURL& site_url, bool newly_available);

  // history::TopSitesObserver implementation.
  void TopSitesLoaded(history::TopSites* top_sites) override;
  void TopSitesChanged(history::TopSites* top_sites,
                       ChangeReason change_reason) override;

  PrefService* prefs_;
  scoped_refptr<history::TopSites> top_sites_;
  suggestions::SuggestionsService* suggestions_service_;
  std::unique_ptr<PopularSites> const popular_sites_;
  std::unique_ptr<IconCacher> const icon_cacher_;
  MostVisitedSitesSupervisor* supervisor_;

  Observer* observer_;

  // The maximum number of most visited sites to return.
  int num_sites_;

  // True if we are still waiting for an initial set of most visited sites (from
  // either TopSites or the SuggestionsService).
  bool waiting_for_most_visited_sites_;

  // True if we are still waiting for the set of popular sites. Immediately set
  // to false if popular sites are disabled, or are not required.
  bool waiting_for_popular_sites_;

  // True if we have recorded impression metrics. They are recorded once both
  // the previous flags are false.
  bool recorded_impressions_;

  std::unique_ptr<
      suggestions::SuggestionsService::ResponseCallbackList::Subscription>
      suggestions_subscription_;

  ScopedObserver<history::TopSites, history::TopSitesObserver>
      top_sites_observer_;

  // The main source of personal tiles - either TOP_SITES or SUGGESTIONS_SEVICE.
  NTPTileSource mv_source_;

  NTPTilesVector current_tiles_;

  // For callbacks may be run after destruction.
  base::WeakPtrFactory<MostVisitedSites> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MostVisitedSites);
};

}  // namespace ntp_tiles

#endif  // COMPONENTS_NTP_TILES_MOST_VISITED_SITES_H_
