// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/typed_url_syncable_service.h"

#include <stddef.h>
#include <utility>

#include "base/auto_reset.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "components/history/core/browser/history_backend.h"
#include "net/base/url_util.h"
#include "sync/protocol/sync.pb.h"
#include "sync/protocol/typed_url_specifics.pb.h"

namespace history {

namespace {

// The server backend can't handle arbitrarily large node sizes, so to keep
// the size under control we limit the visit array.
static const int kMaxTypedUrlVisits = 100;

// There's no limit on how many visits the history DB could have for a given
// typed URL, so we limit how many we fetch from the DB to avoid crashes due to
// running out of memory (http://crbug.com/89793). This value is different
// from kMaxTypedUrlVisits, as some of the visits fetched from the DB may be
// RELOAD visits, which will be stripped.
static const int kMaxVisitsToFetch = 1000;

// This is the threshold at which we start throttling sync updates for typed
// URLs - any URLs with a typed_count >= this threshold will be throttled.
static const int kTypedUrlVisitThrottleThreshold = 10;

// This is the multiple we use when throttling sync updates. If the multiple is
// N, we sync up every Nth update (i.e. when typed_count % N == 0).
static const int kTypedUrlVisitThrottleMultiple = 10;

}  // namespace

// Enforce oldest to newest visit order.
static bool CheckVisitOrdering(const VisitVector& visits) {
  int64_t previous_visit_time = 0;
  for (VisitVector::const_iterator visit = visits.begin();
       visit != visits.end(); ++visit) {
    if (visit != visits.begin()) {
      // We allow duplicate visits here - they shouldn't really be allowed, but
      // they still seem to show up sometimes and we haven't figured out the
      // source, so we just log an error instead of failing an assertion.
      // (http://crbug.com/91473).
      if (previous_visit_time == visit->visit_time.ToInternalValue())
        DVLOG(1) << "Duplicate visit time encountered";
      else if (previous_visit_time > visit->visit_time.ToInternalValue())
        return false;
    }

    previous_visit_time = visit->visit_time.ToInternalValue();
  }
  return true;
}

TypedUrlSyncableService::TypedUrlSyncableService(
    HistoryBackend* history_backend)
    : history_backend_(history_backend),
      processing_syncer_changes_(false),
      num_db_accesses_(0),
      num_db_errors_(0),
      history_backend_observer_(this) {
  DCHECK(history_backend_);
  DCHECK(thread_checker_.CalledOnValidThread());
}

TypedUrlSyncableService::~TypedUrlSyncableService() {
}

syncer::SyncMergeResult TypedUrlSyncableService::MergeDataAndStartSyncing(
    syncer::ModelType type,
    const syncer::SyncDataList& initial_sync_data,
    std::unique_ptr<syncer::SyncChangeProcessor> sync_processor,
    std::unique_ptr<syncer::SyncErrorFactory> error_handler) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!sync_processor_.get());
  DCHECK(sync_processor.get());
  DCHECK(error_handler.get());
  DCHECK_EQ(type, syncer::TYPED_URLS);

  syncer::SyncMergeResult merge_result(type);
  sync_processor_ = std::move(sync_processor);
  sync_error_handler_ = std::move(error_handler);

  ClearErrorStats();

  DVLOG(1) << "Associating TypedUrl: MergeDataAndStartSyncing";

  // Create a mapping of all local data by URLID. These will be narrowed down
  // by CreateOrUpdateUrl() to include only the entries different from sync
  // server data.
  TypedUrlMap new_db_urls;

  // Get all the visits and map the URLRows by URL.
  UrlVisitVectorMap visit_vectors;

  {
    // Get all the typed urls from the history db.
    history::URLRows typed_urls;
    ++num_db_accesses_;
    if (!history_backend_->GetAllTypedURLs(&typed_urls)) {
      ++num_db_errors_;
      merge_result.set_error(sync_error_handler_->CreateAndUploadError(
          FROM_HERE, "Could not get the typed_url entries."));
      return merge_result;
    }

    for (history::URLRows::iterator iter = typed_urls.begin();
         iter != typed_urls.end();) {
      DCHECK_EQ(0U, visit_vectors.count(iter->url()));
      if (!FixupURLAndGetVisits(&(*iter), &(visit_vectors[iter->url()])) ||
          ShouldIgnoreUrl(iter->url()) ||
          ShouldIgnoreVisits(visit_vectors[iter->url()])) {
        // Ignore this URL if we couldn't load the visits or if there's some
        // other problem with it (it was empty, or imported and never visited).
        iter = typed_urls.erase(iter);
      } else {
        // Add url to map.
        new_db_urls[iter->url()] =
            std::make_pair(syncer::SyncChange::ACTION_ADD, *iter);
        ++iter;
      }
    }
  }

  // New sync data organized for different write operations to history backend.
  history::URLRows new_synced_urls;
  history::URLRows updated_synced_urls;
  TypedUrlVisitVector new_synced_visits;

  // List of updates to push to sync.
  syncer::SyncChangeList new_changes;

  // Iterate through initial_sync_data and check for all the urls that
  // sync already knows about. CreateOrUpdateUrl() will remove urls that
  // are the same as the synced ones from |new_db_urls|.
  for (syncer::SyncDataList::const_iterator sync_iter =
           initial_sync_data.begin();
       sync_iter != initial_sync_data.end(); ++sync_iter) {
    // Extract specifics
    const sync_pb::EntitySpecifics& specifics = sync_iter->GetSpecifics();
    const sync_pb::TypedUrlSpecifics& typed_url(specifics.typed_url());

    // Add url to cache of sync state. Note that this is done irrespective of
    // whether the synced url is ignored locally, so that we know what to delete
    // at a later point.
    synced_typed_urls_.insert(GURL(typed_url.url()));

    // Ignore old sync urls that don't have any transition data stored with
    // them, or transition data that does not match the visit data (will be
    // deleted below).
    if (typed_url.visit_transitions_size() == 0 ||
        typed_url.visit_transitions_size() != typed_url.visits_size()) {
      // Generate a debug assertion to help track down http://crbug.com/91473,
      // even though we gracefully handle this case by overwriting this node.
      DCHECK_EQ(typed_url.visits_size(), typed_url.visit_transitions_size());
      DVLOG(1) << "Ignoring obsolete sync url with no visit transition info.";

      // Check if local db has typed visits for the url
      TypedUrlMap::iterator it = new_db_urls.find(GURL(typed_url.url()));
      if (it != new_db_urls.end()) {
        // Overwrite server data with local data
        it->second.first = syncer::SyncChange::ACTION_UPDATE;
      }
      continue;
    }

    CreateOrUpdateUrl(typed_url, &new_db_urls, &visit_vectors, &new_synced_urls,
                      &new_synced_visits, &updated_synced_urls);
  }

  for (TypedUrlMap::iterator i = new_db_urls.begin(); i != new_db_urls.end();
       ++i) {
    std::string tag = i->first.spec();
    AddTypedUrlToChangeList(i->second.first, i->second.second,
                            visit_vectors[i->first], tag, &new_changes);

    // Add url to cache of sync state, if not already cached
    synced_typed_urls_.insert(i->first);
  }

  // Send history changes to the sync server
  merge_result.set_error(
      sync_processor_->ProcessSyncChanges(FROM_HERE, new_changes));

  if (!merge_result.error().IsSet()) {
    WriteToHistoryBackend(&new_synced_urls, &updated_synced_urls, NULL,
                          &new_synced_visits, NULL);
  }

  history_backend_observer_.Add(history_backend_);

  UMA_HISTOGRAM_PERCENTAGE("Sync.TypedUrlMergeAndStartSyncingErrors",
                           GetErrorPercentage());
  ClearErrorStats();

  return merge_result;
}

void TypedUrlSyncableService::StopSyncing(syncer::ModelType type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(type, syncer::TYPED_URLS);

  // Clear cache of server state.
  synced_typed_urls_.clear();

  history_backend_observer_.RemoveAll();

  ClearErrorStats();

  sync_processor_.reset();
  sync_error_handler_.reset();
}

syncer::SyncDataList TypedUrlSyncableService::GetAllSyncData(
    syncer::ModelType type) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  syncer::SyncDataList list;

  // TODO(sync): Add implementation

  return list;
}

syncer::SyncError TypedUrlSyncableService::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::vector<GURL> pending_deleted_urls;
  history::URLRows new_synced_urls;
  history::URLRows updated_synced_urls;
  TypedUrlVisitVector new_synced_visits;
  history::VisitVector deleted_visits;

  for (syncer::SyncChangeList::const_iterator it = change_list.begin();
       it != change_list.end(); ++it) {
    const sync_pb::EntitySpecifics& specifics = it->sync_data().GetSpecifics();
    DCHECK(specifics.has_typed_url())
        << "Typed URL delete change does not have necessary specifics.";
    GURL url(specifics.typed_url().url());

    if (syncer::SyncChange::ACTION_DELETE == it->change_type()) {
      pending_deleted_urls.push_back(url);
      if (synced_typed_urls_.find(url) != synced_typed_urls_.end()) {
        // Delete typed url from cache.
        synced_typed_urls_.erase(url);
      }
      continue;
    }

    // Ensure cache of server state is up to date.
    synced_typed_urls_.insert(url);

    if (ShouldIgnoreUrl(url))
      continue;

    const sync_pb::TypedUrlSpecifics& typed_url(specifics.typed_url());
    DCHECK(typed_url.visits_size());
    sync_pb::TypedUrlSpecifics filtered_url = FilterExpiredVisits(typed_url);
    if (filtered_url.visits_size() == 0)
      continue;

    UpdateFromSyncDB(filtered_url, &new_synced_visits, &deleted_visits,
                     &updated_synced_urls, &new_synced_urls);
  }

  WriteToHistoryBackend(&new_synced_urls, &updated_synced_urls,
                        &pending_deleted_urls, &new_synced_visits,
                        &deleted_visits);

  return syncer::SyncError();
}

void TypedUrlSyncableService::OnURLsModified(
    history::HistoryBackend* history_backend,
    const history::URLRows& changed_urls) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (processing_syncer_changes_)
    return;  // These are changes originating from us, ignore.
  if (!sync_processor_.get())
    return;  // Sync processor not yet initialized, don't sync.

  // Create SyncChangeList.
  syncer::SyncChangeList changes;

  for (const auto& row : changed_urls) {
    // Only care if the modified URL is typed.
    if (row.typed_count() >= 0) {
      // If there were any errors updating the sync node, just ignore them and
      // continue on to process the next URL.
      CreateOrUpdateSyncNode(row, &changes);
    }
  }

  // Send SyncChangeList to server if there are any changes.
  if (changes.size() > 0)
    sync_processor_->ProcessSyncChanges(FROM_HERE, changes);
}

void TypedUrlSyncableService::OnURLVisited(
    history::HistoryBackend* history_backend,
    ui::PageTransition transition,
    const history::URLRow& row,
    const history::RedirectList& redirects,
    base::Time visit_time) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (processing_syncer_changes_)
    return;  // These are changes originating from us, ignore.
  if (!sync_processor_.get())
    return;  // Sync processor not yet initialized, don't sync.
  if (!ShouldSyncVisit(row.typed_count(), transition))
    return;

  // Create SyncChangeList.
  syncer::SyncChangeList changes;

  CreateOrUpdateSyncNode(row, &changes);

  // Send SyncChangeList to server if there are any changes.
  if (changes.size() > 0)
    sync_processor_->ProcessSyncChanges(FROM_HERE, changes);
}

void TypedUrlSyncableService::OnURLsDeleted(
    history::HistoryBackend* history_backend,
    bool all_history,
    bool expired,
    const history::URLRows& deleted_rows,
    const std::set<GURL>& favicon_urls) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (processing_syncer_changes_)
    return;  // These are changes originating from us, ignore.
  if (!sync_processor_.get())
    return;  // Sync processor not yet initialized, don't sync.

  // Ignore URLs expired due to old age (we don't want to sync them as deletions
  // to avoid extra traffic up to the server, and also to make sure that a
  // client with a bad clock setting won't go on an expiration rampage and
  // delete all history from every client). The server will gracefully age out
  // the sync DB entries when they've been idle for long enough.
  if (expired)
    return;

  // Create SyncChangeList.
  syncer::SyncChangeList changes;

  if (all_history) {
    // Delete all synced typed urls.
    for (const auto& url : synced_typed_urls_) {
      VisitVector visits;
      URLRow row(url);
      AddTypedUrlToChangeList(syncer::SyncChange::ACTION_DELETE, row, visits,
                              url.spec(), &changes);
    }
    // Clear cache of server state.
    synced_typed_urls_.clear();
  } else {
    // Delete rows.
    for (const auto& row : deleted_rows) {
      // Add specifics to change list for all synced urls that were deleted.
      if (synced_typed_urls_.find(row.url()) != synced_typed_urls_.end()) {
        VisitVector visits;
        AddTypedUrlToChangeList(syncer::SyncChange::ACTION_DELETE, row, visits,
                                row.url().spec(), &changes);
        // Delete typed url from cache.
        synced_typed_urls_.erase(row.url());
      }
    }
  }

  // Send SyncChangeList to server if there are any changes.
  if (changes.size() > 0)
    sync_processor_->ProcessSyncChanges(FROM_HERE, changes);
}

void TypedUrlSyncableService::CreateOrUpdateUrl(
    const sync_pb::TypedUrlSpecifics& typed_url,
    TypedUrlMap* loaded_data,
    UrlVisitVectorMap* visit_vectors,
    history::URLRows* new_synced_urls,
    TypedUrlVisitVector* new_synced_visits,
    history::URLRows* updated_synced_urls) {
  DCHECK(typed_url.visits_size() != 0);
  DCHECK_EQ(typed_url.visits_size(), typed_url.visit_transitions_size());

  // Ignore empty urls.
  if (typed_url.url().empty()) {
    DVLOG(1) << "Ignoring empty URL in sync DB";
    return;
  }
  // Now, get rid of the expired visits. If there are no un-expired visits
  // left, ignore this url - any local data should just replace it.
  sync_pb::TypedUrlSpecifics sync_url = FilterExpiredVisits(typed_url);
  if (sync_url.visits_size() == 0) {
    DVLOG(1) << "Ignoring expired URL in sync DB: " << sync_url.url();
    return;
  }

  // Check if local db already has the url from sync.
  TypedUrlMap::iterator it = loaded_data->find(GURL(sync_url.url()));
  if (it == loaded_data->end()) {
    // There are no matching typed urls from the local db, check for untyped
    history::URLRow untyped_url(GURL(sync_url.url()));

    // The URL may still exist in the local db if it is an untyped url.
    // An untyped url will transition to a typed url after receiving visits
    // from sync, and sync should receive any visits already existing locally
    // for the url, so the full list of visits is consistent.
    bool is_existing_url =
        history_backend_->GetURL(untyped_url.url(), &untyped_url);
    if (is_existing_url) {
      // Add a new entry to |loaded_data|, and set the iterator to it.
      history::VisitVector untyped_visits;
      if (!FixupURLAndGetVisits(&untyped_url, &untyped_visits)) {
        // Couldn't load the visits for this URL due to some kind of DB error.
        // Don't bother writing this URL to the history DB (if we ignore the
        // error and continue, we might end up duplicating existing visits).
        DLOG(ERROR) << "Could not load visits for url: " << untyped_url.url();
        return;
      }
      (*visit_vectors)[untyped_url.url()] = untyped_visits;

      // Store row info that will be used to update sync's visits.
      (*loaded_data)[untyped_url.url()] =
          std::pair<syncer::SyncChange::SyncChangeType, history::URLRow>(
              syncer::SyncChange::ACTION_UPDATE, untyped_url);

      // Set iterator |it| to point to this entry.
      it = loaded_data->find(untyped_url.url());
      DCHECK(it != loaded_data->end());
      // Continue with merge below.
    } else {
      // The url is new to the local history DB.
      // Create new db entry for url.
      history::URLRow new_url(GURL(sync_url.url()));
      UpdateURLRowFromTypedUrlSpecifics(sync_url, &new_url);
      new_synced_urls->push_back(new_url);

      // Add entries for url visits.
      std::vector<history::VisitInfo> added_visits;
      size_t visit_count = sync_url.visits_size();

      for (size_t index = 0; index < visit_count; ++index) {
        base::Time visit_time =
            base::Time::FromInternalValue(sync_url.visits(index));
        ui::PageTransition transition =
            ui::PageTransitionFromInt(sync_url.visit_transitions(index));
        added_visits.push_back(history::VisitInfo(visit_time, transition));
      }
      new_synced_visits->push_back(
          std::pair<GURL, std::vector<history::VisitInfo>>(new_url.url(),
                                                           added_visits));
      return;
    }
  }

  // Same URL exists in sync data and in history data - compare the
  // entries to see if there's any difference.
  history::VisitVector& visits = (*visit_vectors)[it->first];
  std::vector<history::VisitInfo> added_visits;

  // Empty URLs should be filtered out by ShouldIgnoreUrl() previously.
  DCHECK(!it->second.second.url().spec().empty());

  // Initialize fields in |new_url| to the same values as the fields in
  // the existing URLRow in the history DB. This is needed because we
  // overwrite the existing value in WriteToHistoryBackend(), but some of
  // the values in that structure are not synced (like typed_count).
  history::URLRow new_url(it->second.second);

  MergeResult difference =
      MergeUrls(sync_url, it->second.second, &visits, &new_url, &added_visits);

  if (difference != DIFF_NONE) {
    it->second.second = new_url;
    if (difference & DIFF_UPDATE_NODE) {
      // Edit map entry to reflect update to sync.
      it->second.first = syncer::SyncChange::ACTION_UPDATE;
      // We don't want to resurrect old visits that have been aged out by
      // other clients, so remove all visits that are older than the
      // earliest existing visit in the sync node.
      //
      // TODO(sync): This logic should be unnecessary now that filtering of
      // expired visits is performed separately. Non-expired visits older than
      // the earliest existing sync visits should still be synced, so this
      // logic should be removed.
      if (sync_url.visits_size() > 0) {
        base::Time earliest_visit =
            base::Time::FromInternalValue(sync_url.visits(0));
        for (history::VisitVector::iterator i = visits.begin();
             i != visits.end() && i->visit_time < earliest_visit;) {
          i = visits.erase(i);
        }
        // Should never be possible to delete all the items, since the
        // visit vector contains newer local visits it will keep and/or the
        // visits in typed_url.visits newer than older local visits.
        DCHECK(visits.size() > 0);
      }
      DCHECK_EQ(new_url.last_visit().ToInternalValue(),
                visits.back().visit_time.ToInternalValue());
    }
    if (difference & DIFF_LOCAL_ROW_CHANGED) {
      // Add entry to updated_synced_urls to update the local db.
      DCHECK_EQ(it->second.second.id(), new_url.id());
      updated_synced_urls->push_back(new_url);
    }
    if (difference & DIFF_LOCAL_VISITS_ADDED) {
      // Add entry with new visits to new_synced_visits to update the local db.
      new_synced_visits->push_back(
          std::pair<GURL, std::vector<history::VisitInfo>>(it->first,
                                                           added_visits));
    }
  } else {
    // No difference in urls, erase from map
    loaded_data->erase(it);
  }
}

sync_pb::TypedUrlSpecifics TypedUrlSyncableService::FilterExpiredVisits(
    const sync_pb::TypedUrlSpecifics& source) {
  // Make a copy of the source, then regenerate the visits.
  sync_pb::TypedUrlSpecifics specifics(source);
  specifics.clear_visits();
  specifics.clear_visit_transitions();
  for (int i = 0; i < source.visits_size(); ++i) {
    base::Time time = base::Time::FromInternalValue(source.visits(i));
    if (!history_backend_->IsExpiredVisitTime(time)) {
      specifics.add_visits(source.visits(i));
      specifics.add_visit_transitions(source.visit_transitions(i));
    }
  }
  DCHECK(specifics.visits_size() == specifics.visit_transitions_size());
  return specifics;
}

// static
TypedUrlSyncableService::MergeResult TypedUrlSyncableService::MergeUrls(
    const sync_pb::TypedUrlSpecifics& sync_url,
    const history::URLRow& url,
    history::VisitVector* visits,
    history::URLRow* new_url,
    std::vector<history::VisitInfo>* new_visits) {
  DCHECK(new_url);
  DCHECK(!sync_url.url().compare(url.url().spec()));
  DCHECK(!sync_url.url().compare(new_url->url().spec()));
  DCHECK(visits->size());
  DCHECK_GT(sync_url.visits_size(), 0);
  CHECK_EQ(sync_url.visits_size(), sync_url.visit_transitions_size());

  // Convert these values only once.
  base::string16 sync_url_title(base::UTF8ToUTF16(sync_url.title()));
  base::Time sync_url_last_visit = base::Time::FromInternalValue(
      sync_url.visits(sync_url.visits_size() - 1));

  // This is a bitfield representing what we'll need to update with the output
  // value.
  MergeResult different = DIFF_NONE;

  // Check if the non-incremented values changed.
  if ((sync_url_title.compare(url.title()) != 0) ||
      (sync_url.hidden() != url.hidden())) {
    // Use the values from the most recent visit.
    if (sync_url_last_visit >= url.last_visit()) {
      new_url->set_title(sync_url_title);
      new_url->set_hidden(sync_url.hidden());
      different |= DIFF_LOCAL_ROW_CHANGED;
    } else {
      new_url->set_title(url.title());
      new_url->set_hidden(url.hidden());
      different |= DIFF_UPDATE_NODE;
    }
  } else {
    // No difference.
    new_url->set_title(url.title());
    new_url->set_hidden(url.hidden());
  }

  size_t sync_url_num_visits = sync_url.visits_size();
  size_t history_num_visits = visits->size();
  size_t sync_url_visit_index = 0;
  size_t history_visit_index = 0;
  base::Time earliest_history_time = (*visits)[0].visit_time;
  // Walk through the two sets of visits and figure out if any new visits were
  // added on either side.
  while (sync_url_visit_index < sync_url_num_visits ||
         history_visit_index < history_num_visits) {
    // Time objects are initialized to "earliest possible time".
    base::Time sync_url_time, history_time;
    if (sync_url_visit_index < sync_url_num_visits)
      sync_url_time =
          base::Time::FromInternalValue(sync_url.visits(sync_url_visit_index));
    if (history_visit_index < history_num_visits)
      history_time = (*visits)[history_visit_index].visit_time;
    if (sync_url_visit_index >= sync_url_num_visits ||
        (history_visit_index < history_num_visits &&
         sync_url_time > history_time)) {
      // We found a visit in the history DB that doesn't exist in the sync DB,
      // so mark the sync_url as modified so the caller will update the sync
      // node.
      different |= DIFF_UPDATE_NODE;
      ++history_visit_index;
    } else if (history_visit_index >= history_num_visits ||
               sync_url_time < history_time) {
      // Found a visit in the sync node that doesn't exist in the history DB, so
      // add it to our list of new visits and set the appropriate flag so the
      // caller will update the history DB.
      // If the sync_url visit is older than any existing visit in the history
      // DB, don't re-add it - this keeps us from resurrecting visits that were
      // aged out locally.
      //
      // TODO(sync): This extra check should be unnecessary now that filtering
      // expired visits is performed separately. Non-expired visits older than
      // the earliest existing history visits should still be synced, so this
      // check should be removed.
      if (sync_url_time > earliest_history_time) {
        different |= DIFF_LOCAL_VISITS_ADDED;
        new_visits->push_back(history::VisitInfo(
            sync_url_time, ui::PageTransitionFromInt(sync_url.visit_transitions(
                               sync_url_visit_index))));
      }
      // This visit is added to visits below.
      ++sync_url_visit_index;
    } else {
      // Same (already synced) entry found in both DBs - no need to do anything.
      ++sync_url_visit_index;
      ++history_visit_index;
    }
  }

  DCHECK(CheckVisitOrdering(*visits));
  if (different & DIFF_LOCAL_VISITS_ADDED) {
    // If the server does not have the same visits as the local db, then the
    // new visits from the server need to be added to the vector containing
    // local visits. These visits will be passed to the server.
    // Insert new visits into the appropriate place in the visits vector.
    history::VisitVector::iterator visit_ix = visits->begin();
    for (std::vector<history::VisitInfo>::iterator new_visit =
             new_visits->begin();
         new_visit != new_visits->end(); ++new_visit) {
      while (visit_ix != visits->end() &&
             new_visit->first > visit_ix->visit_time) {
        ++visit_ix;
      }
      visit_ix =
          visits->insert(visit_ix, history::VisitRow(url.id(), new_visit->first,
                                                     0, new_visit->second, 0));
      ++visit_ix;
    }
  }
  DCHECK(CheckVisitOrdering(*visits));

  new_url->set_last_visit(visits->back().visit_time);
  return different;
}

void TypedUrlSyncableService::WriteToHistoryBackend(
    const history::URLRows* new_urls,
    const history::URLRows* updated_urls,
    const std::vector<GURL>* deleted_urls,
    const TypedUrlVisitVector* new_visits,
    const history::VisitVector* deleted_visits) {
  // Set flag to stop accepting history change notifications from backend
  base::AutoReset<bool> processing_changes(&processing_syncer_changes_, true);

  if (deleted_urls && !deleted_urls->empty())
    history_backend_->DeleteURLs(*deleted_urls);

  if (new_urls) {
    history_backend_->AddPagesWithDetails(*new_urls, history::SOURCE_SYNCED);
  }
  if (updated_urls) {
    ++num_db_accesses_;
    // This is an existing entry in the URL database. We don't verify the
    // visit_count or typed_count values here, because either one (or both)
    // could be zero in the case of bookmarks, or in the case of a URL
    // transitioning from non-typed to typed as a result of this sync.
    // In the field we sometimes run into errors on specific URLs. It's OK
    // to just continue on (we can try writing again on the next model
    // association).
    size_t num_successful_updates = history_backend_->UpdateURLs(*updated_urls);
    num_db_errors_ += updated_urls->size() - num_successful_updates;
  }
  if (new_visits) {
    for (TypedUrlVisitVector::const_iterator visits = new_visits->begin();
         visits != new_visits->end(); ++visits) {
      // If there are no visits to add, just skip this.
      if (visits->second.empty())
        continue;
      ++num_db_accesses_;
      if (!history_backend_->AddVisits(visits->first, visits->second,
                                       history::SOURCE_SYNCED)) {
        ++num_db_errors_;
        DLOG(ERROR) << "Could not add visits.";
      }
    }
  }
  if (deleted_visits) {
    ++num_db_accesses_;
    if (!history_backend_->RemoveVisits(*deleted_visits)) {
      ++num_db_errors_;
      DLOG(ERROR) << "Could not remove visits.";
      // This is bad news, since it means we may end up resurrecting history
      // entries on the next reload. It's unavoidable so we'll just keep on
      // syncing.
    }
  }
}

void TypedUrlSyncableService::GetSyncedUrls(std::set<GURL>* urls) const {
  urls->insert(synced_typed_urls_.begin(), synced_typed_urls_.end());
}

void TypedUrlSyncableService::ClearErrorStats() {
  num_db_accesses_ = 0;
  num_db_errors_ = 0;
}

int TypedUrlSyncableService::GetErrorPercentage() const {
  return num_db_accesses_ ? (100 * num_db_errors_ / num_db_accesses_) : 0;
}

bool TypedUrlSyncableService::ShouldIgnoreUrl(const GURL& url) {
  // Ignore empty URLs. Not sure how this can happen (maybe import from other
  // busted browsers, or misuse of the history API, or just plain bugs) but we
  // can't deal with them.
  if (url.spec().empty())
    return true;

  // Ignore local file URLs.
  if (url.SchemeIsFile())
    return true;

  // Ignore localhost URLs.
  if (net::IsLocalhost(url.host()))
    return true;

  return false;
}

bool TypedUrlSyncableService::ShouldIgnoreVisits(
    const history::VisitVector& visits) {
  // We ignore URLs that were imported, but have never been visited by
  // chromium.
  static const int kFirstImportedSource = history::SOURCE_FIREFOX_IMPORTED;
  history::VisitSourceMap map;
  if (!history_backend_->GetVisitsSource(visits, &map))
    return false;  // If we can't read the visit, assume it's not imported.

  // Walk the list of visits and look for a non-imported item.
  for (history::VisitVector::const_iterator it = visits.begin();
       it != visits.end(); ++it) {
    if (map.count(it->visit_id) == 0 ||
        map[it->visit_id] < kFirstImportedSource) {
      return false;
    }
  }
  // We only saw imported visits, so tell the caller to ignore them.
  return true;
}

bool TypedUrlSyncableService::ShouldSyncVisit(int typed_count,
                                              ui::PageTransition transition) {
  // Just use an ad-hoc criteria to determine whether to ignore this
  // notification. For most users, the distribution of visits is roughly a bell
  // curve with a long tail - there are lots of URLs with < 5 visits so we want
  // to make sure we sync up every visit to ensure the proper ordering of
  // suggestions. But there are relatively few URLs with > 10 visits, and those
  // tend to be more broadly distributed such that there's no need to sync up
  // every visit to preserve their relative ordering.
  return (ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_TYPED) &&
          typed_count >= 0 &&
          (typed_count < kTypedUrlVisitThrottleThreshold ||
           (typed_count % kTypedUrlVisitThrottleMultiple) == 0));
}

bool TypedUrlSyncableService::CreateOrUpdateSyncNode(
    URLRow url,
    syncer::SyncChangeList* changes) {
  DCHECK_GE(url.typed_count(), 0);

  if (ShouldIgnoreUrl(url.url()))
    return true;

  // Get the visits for this node.
  VisitVector visit_vector;
  if (!FixupURLAndGetVisits(&url, &visit_vector)) {
    DLOG(ERROR) << "Could not load visits for url: " << url.url();
    return false;
  }

  if (std::find_if(visit_vector.begin(), visit_vector.end(),
                   [](const history::VisitRow& visit) {
                     return ui::PageTransitionCoreTypeIs(
                         visit.transition, ui::PAGE_TRANSITION_TYPED);
                   }) == visit_vector.end())
    // This URL has no TYPED visits, don't sync it
    return false;

  DCHECK(!visit_vector.empty());

  std::string title = url.url().spec();
  syncer::SyncChange::SyncChangeType change_type;

  // If server already has URL, then send a sync update, else add it.
  change_type = (synced_typed_urls_.find(url.url()) != synced_typed_urls_.end())
                    ? syncer::SyncChange::ACTION_UPDATE
                    : syncer::SyncChange::ACTION_ADD;

  // Ensure cache of server state is up to date.
  synced_typed_urls_.insert(url.url());

  AddTypedUrlToChangeList(change_type, url, visit_vector, title, changes);

  return true;
}

void TypedUrlSyncableService::AddTypedUrlToChangeList(
    syncer::SyncChange::SyncChangeType change_type,
    const URLRow& row,
    const VisitVector& visits,
    std::string title,
    syncer::SyncChangeList* change_list) {
  sync_pb::EntitySpecifics entity_specifics;
  sync_pb::TypedUrlSpecifics* typed_url = entity_specifics.mutable_typed_url();
  std::string tag = row.url().spec();

  if (change_type == syncer::SyncChange::ACTION_DELETE) {
    typed_url->set_url(tag);
  } else {
    WriteToTypedUrlSpecifics(row, visits, typed_url);
  }

  change_list->push_back(syncer::SyncChange(
      FROM_HERE, change_type,
      syncer::SyncData::CreateLocalData(tag, title, entity_specifics)));
}

void TypedUrlSyncableService::WriteToTypedUrlSpecifics(
    const URLRow& url,
    const VisitVector& visits,
    sync_pb::TypedUrlSpecifics* typed_url) {
  DCHECK(!url.last_visit().is_null());
  DCHECK(!visits.empty());
  DCHECK_EQ(url.last_visit().ToInternalValue(),
            visits.back().visit_time.ToInternalValue());

  typed_url->set_url(url.url().spec());
  typed_url->set_title(base::UTF16ToUTF8(url.title()));
  typed_url->set_hidden(url.hidden());

  DCHECK(CheckVisitOrdering(visits));

  bool only_typed = false;
  int skip_count = 0;

  if (visits.size() > static_cast<size_t>(kMaxTypedUrlVisits)) {
    int typed_count = 0;
    int total = 0;
    // Walk the passed-in visit vector and count the # of typed visits.
    for (VisitVector::const_iterator visit = visits.begin();
         visit != visits.end(); ++visit) {
      // We ignore reload visits.
      if (PageTransitionCoreTypeIs(visit->transition,
                                   ui::PAGE_TRANSITION_RELOAD)) {
        continue;
      }
      ++total;
      if (PageTransitionCoreTypeIs(visit->transition,
                                   ui::PAGE_TRANSITION_TYPED)) {
        ++typed_count;
      }
    }
    // We should have at least one typed visit. This can sometimes happen if
    // the history DB has an inaccurate count for some reason (there's been
    // bugs in the history code in the past which has left users in the wild
    // with incorrect counts - http://crbug.com/84258).
    DCHECK(typed_count > 0);

    if (typed_count > kMaxTypedUrlVisits) {
      only_typed = true;
      skip_count = typed_count - kMaxTypedUrlVisits;
    } else if (total > kMaxTypedUrlVisits) {
      skip_count = total - kMaxTypedUrlVisits;
    }
  }

  for (VisitVector::const_iterator visit = visits.begin();
       visit != visits.end(); ++visit) {
    // Skip reload visits.
    if (PageTransitionCoreTypeIs(visit->transition, ui::PAGE_TRANSITION_RELOAD))
      continue;

    // If we only have room for typed visits, then only add typed visits.
    if (only_typed &&
        !PageTransitionCoreTypeIs(visit->transition,
                                  ui::PAGE_TRANSITION_TYPED)) {
      continue;
    }

    if (skip_count > 0) {
      // We have too many entries to fit, so we need to skip the oldest ones.
      // Only skip typed URLs if there are too many typed URLs to fit.
      if (only_typed ||
          !PageTransitionCoreTypeIs(visit->transition,
                                    ui::PAGE_TRANSITION_TYPED)) {
        --skip_count;
        continue;
      }
    }
    typed_url->add_visits(visit->visit_time.ToInternalValue());
    typed_url->add_visit_transitions(visit->transition);
  }
  DCHECK_EQ(skip_count, 0);

  if (typed_url->visits_size() == 0) {
    // If we get here, it's because we don't actually have any TYPED visits
    // even though the visit's typed_count > 0 (corrupted typed_count). So
    // let's go ahead and add a RELOAD visit at the most recent visit since
    // it's not legal to have an empty visit array (yet another workaround
    // for http://crbug.com/84258).
    typed_url->add_visits(url.last_visit().ToInternalValue());
    typed_url->add_visit_transitions(ui::PAGE_TRANSITION_RELOAD);
  }
  CHECK_GT(typed_url->visits_size(), 0);
  CHECK_LE(typed_url->visits_size(), kMaxTypedUrlVisits);
  CHECK_EQ(typed_url->visits_size(), typed_url->visit_transitions_size());
}

// static
void TypedUrlSyncableService::UpdateURLRowFromTypedUrlSpecifics(
    const sync_pb::TypedUrlSpecifics& typed_url,
    history::URLRow* new_url) {
  DCHECK_GT(typed_url.visits_size(), 0);
  CHECK_EQ(typed_url.visit_transitions_size(), typed_url.visits_size());
  new_url->set_title(base::UTF8ToUTF16(typed_url.title()));
  new_url->set_hidden(typed_url.hidden());
  // Only provide the initial value for the last_visit field - after that, let
  // the history code update the last_visit field on its own.
  if (new_url->last_visit().is_null()) {
    new_url->set_last_visit(base::Time::FromInternalValue(
        typed_url.visits(typed_url.visits_size() - 1)));
  }
}

bool TypedUrlSyncableService::FixupURLAndGetVisits(URLRow* url,
                                                   VisitVector* visits) {
  ++num_db_accesses_;
  CHECK(history_backend_);
  if (!history_backend_->GetMostRecentVisitsForURL(url->id(), kMaxVisitsToFetch,
                                                   visits)) {
    ++num_db_errors_;
    return false;
  }

  // Sometimes (due to a bug elsewhere in the history or sync code, or due to
  // a crash between adding a URL to the history database and updating the
  // visit DB) the visit vector for a URL can be empty. If this happens, just
  // create a new visit whose timestamp is the same as the last_visit time.
  // This is a workaround for http://crbug.com/84258.
  if (visits->empty()) {
    DVLOG(1) << "Found empty visits for URL: " << url->url();
    if (url->last_visit().is_null()) {
      // If modified URL is bookmarked, history backend treats it as modified
      // even if all its visits are deleted. Return false to stop further
      // processing because sync expects valid visit time for modified entry.
      return false;
    }

    VisitRow visit(url->id(), url->last_visit(), 0, ui::PAGE_TRANSITION_TYPED,
                   0);
    visits->push_back(visit);
  }

  // GetMostRecentVisitsForURL() returns the data in the opposite order that
  // we need it, so reverse it.
  std::reverse(visits->begin(), visits->end());

  // Sometimes, the last_visit field in the URL doesn't match the timestamp of
  // the last visit in our visit array (they come from different tables, so
  // crashes/bugs can cause them to mismatch), so just set it here.
  url->set_last_visit(visits->back().visit_time);
  DCHECK(CheckVisitOrdering(*visits));
  return true;
}

void TypedUrlSyncableService::UpdateFromSyncDB(
    const sync_pb::TypedUrlSpecifics& typed_url,
    TypedUrlVisitVector* visits_to_add,
    history::VisitVector* visits_to_remove,
    history::URLRows* updated_urls,
    history::URLRows* new_urls) {
  history::URLRow new_url(GURL(typed_url.url()));
  history::VisitVector existing_visits;
  bool existing_url = history_backend_->GetURL(new_url.url(), &new_url);
  if (existing_url) {
    // This URL already exists locally - fetch the visits so we can
    // merge them below.
    if (!FixupURLAndGetVisits(&new_url, &existing_visits)) {
      // Couldn't load the visits for this URL due to some kind of DB error.
      // Don't bother writing this URL to the history DB (if we ignore the
      // error and continue, we might end up duplicating existing visits).
      DLOG(ERROR) << "Could not load visits for url: " << new_url.url();
      return;
    }
  }
  visits_to_add->push_back(std::pair<GURL, std::vector<history::VisitInfo>>(
      new_url.url(), std::vector<history::VisitInfo>()));

  // Update the URL with information from the typed URL.
  UpdateURLRowFromTypedUrlSpecifics(typed_url, &new_url);

  // Figure out which visits we need to add.
  DiffVisits(existing_visits, typed_url, &visits_to_add->back().second,
             visits_to_remove);

  if (existing_url) {
    updated_urls->push_back(new_url);
  } else {
    new_urls->push_back(new_url);
  }
}

// static
void TypedUrlSyncableService::DiffVisits(
    const history::VisitVector& history_visits,
    const sync_pb::TypedUrlSpecifics& sync_specifics,
    std::vector<history::VisitInfo>* new_visits,
    history::VisitVector* removed_visits) {
  DCHECK(new_visits);
  size_t old_visit_count = history_visits.size();
  size_t new_visit_count = sync_specifics.visits_size();
  size_t old_index = 0;
  size_t new_index = 0;
  while (old_index < old_visit_count && new_index < new_visit_count) {
    base::Time new_visit_time =
        base::Time::FromInternalValue(sync_specifics.visits(new_index));
    if (history_visits[old_index].visit_time < new_visit_time) {
      if (new_index > 0 && removed_visits) {
        // If there are visits missing from the start of the node, that
        // means that they were probably clipped off due to our code that
        // limits the size of the sync nodes - don't delete them from our
        // local history.
        removed_visits->push_back(history_visits[old_index]);
      }
      ++old_index;
    } else if (history_visits[old_index].visit_time > new_visit_time) {
      new_visits->push_back(history::VisitInfo(
          new_visit_time, ui::PageTransitionFromInt(
                              sync_specifics.visit_transitions(new_index))));
      ++new_index;
    } else {
      ++old_index;
      ++new_index;
    }
  }

  if (removed_visits) {
    for (; old_index < old_visit_count; ++old_index) {
      removed_visits->push_back(history_visits[old_index]);
    }
  }

  for (; new_index < new_visit_count; ++new_index) {
    new_visits->push_back(history::VisitInfo(
        base::Time::FromInternalValue(sync_specifics.visits(new_index)),
        ui::PageTransitionFromInt(
            sync_specifics.visit_transitions(new_index))));
  }
}

}  // namespace history
