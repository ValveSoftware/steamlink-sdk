// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/util.h"

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/memory/scoped_vector.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service.h"

base::string16 GetDefaultSearchEngineName(TemplateURLService* service) {
  DCHECK(service);
  const TemplateURL* const default_provider =
      service->GetDefaultSearchProvider();
  if (!default_provider) {
    // TODO(cpu): bug 1187517. It is possible to have no default provider.
    // returning an empty string is a stopgap measure for the crash
    // http://code.google.com/p/chromium/issues/detail?id=2573
    return base::string16();
  }
  return default_provider->short_name();
}

GURL GetDefaultSearchURLForSearchTerms(TemplateURLService* service,
                                       const base::string16& terms) {
  DCHECK(service);
  const TemplateURL* default_provider = service->GetDefaultSearchProvider();
  if (!default_provider)
    return GURL();
  const TemplateURLRef& search_url = default_provider->url_ref();
  DCHECK(search_url.SupportsReplacement(service->search_terms_data()));
  TemplateURLRef::SearchTermsArgs search_terms_args(terms);
  search_terms_args.append_extra_query_params = true;
  return GURL(search_url.ReplaceSearchTerms(search_terms_args,
                                            service->search_terms_data()));
}

void RemoveDuplicatePrepopulateIDs(
    KeywordWebDataService* service,
    const ScopedVector<TemplateURLData>& prepopulated_urls,
    TemplateURL* default_search_provider,
    TemplateURLService::TemplateURLVector* template_urls,
    const SearchTermsData& search_terms_data,
    std::set<std::string>* removed_keyword_guids) {
  DCHECK(template_urls);

  // For convenience construct an ID->TemplateURL* map from |prepopulated_urls|.
  typedef std::map<int, TemplateURLData*> PrepopulatedURLMap;
  PrepopulatedURLMap prepopulated_url_map;
  for (std::vector<TemplateURLData*>::const_iterator i(
           prepopulated_urls.begin());
       i != prepopulated_urls.end();
       ++i)
    prepopulated_url_map[(*i)->prepopulate_id] = *i;

  // Separate |template_urls| into prepopulated and non-prepopulated groups.
  typedef std::multimap<int, TemplateURL*> UncheckedURLMap;
  UncheckedURLMap unchecked_urls;
  TemplateURLService::TemplateURLVector checked_urls;
  for (TemplateURLService::TemplateURLVector::iterator i(
       template_urls->begin()); i != template_urls->end(); ++i) {
    TemplateURL* turl = *i;
    int prepopulate_id = turl->prepopulate_id();
    if (prepopulate_id)
      unchecked_urls.insert(std::make_pair(prepopulate_id, turl));
    else
      checked_urls.push_back(turl);
  }

  // For each group of prepopulated URLs with one ID, find the best URL to use
  // and add it to the (initially all non-prepopulated) URLs we've already OKed.
  // Delete the others from the service and from memory.
  while (!unchecked_urls.empty()) {
    // Find the best URL.
    int prepopulate_id = unchecked_urls.begin()->first;
    PrepopulatedURLMap::const_iterator prepopulated_url =
        prepopulated_url_map.find(prepopulate_id);
    UncheckedURLMap::iterator end = unchecked_urls.upper_bound(prepopulate_id);
    UncheckedURLMap::iterator best = unchecked_urls.begin();
    bool matched_keyword = false;
    for (UncheckedURLMap::iterator i = unchecked_urls.begin(); i != end; ++i) {
      // If the user-selected DSE is a prepopulated engine its properties will
      // either come from the prepopulation origin or from the user preferences
      // file (see DefaultSearchManager). Those properties will end up
      // overwriting whatever we load now anyway. If we are eliminating
      // duplicates, then, we err on the side of keeping the thing that looks
      // more like the value we will end up with in the end.
      if (default_search_provider &&
          (default_search_provider->prepopulate_id() ==
              i->second->prepopulate_id()) &&
          default_search_provider->HasSameKeywordAs(i->second->data(),
                                                    search_terms_data)) {
        best = i;
        break;
      }

      // Otherwise, a URL is best if it matches the prepopulated data's keyword;
      // if none match, just fall back to using the one with the lowest ID.
      if (matched_keyword)
        continue;
      if ((prepopulated_url != prepopulated_url_map.end()) &&
          i->second->HasSameKeywordAs(*prepopulated_url->second,
                                      search_terms_data)) {
        best = i;
        matched_keyword = true;
      } else if (i->second->id() < best->second->id()) {
        best = i;
      }
    }

    // Add the best URL to the checked group and delete the rest.
    checked_urls.push_back(best->second);
    for (UncheckedURLMap::iterator i = unchecked_urls.begin(); i != end; ++i) {
      if (i == best)
        continue;
      if (service) {
        service->RemoveKeyword(i->second->id());
        if (removed_keyword_guids)
          removed_keyword_guids->insert(i->second->sync_guid());
      }
      delete i->second;
    }

    // Done with this group.
    unchecked_urls.erase(unchecked_urls.begin(), end);
  }

  // Return the checked URLs.
  template_urls->swap(checked_urls);
}

// Returns the TemplateURL with id specified from the list of TemplateURLs.
// If not found, returns NULL.
TemplateURL* GetTemplateURLByID(
    const TemplateURLService::TemplateURLVector& template_urls,
    int64_t id) {
  for (TemplateURLService::TemplateURLVector::const_iterator i(
       template_urls.begin()); i != template_urls.end(); ++i) {
    if ((*i)->id() == id) {
      return *i;
    }
  }
  return NULL;
}

TemplateURL* FindURLByPrepopulateID(
    const TemplateURLService::TemplateURLVector& template_urls,
    int prepopulate_id) {
  for (std::vector<TemplateURL*>::const_iterator i = template_urls.begin();
       i < template_urls.end(); ++i) {
    if ((*i)->prepopulate_id() == prepopulate_id)
      return *i;
  }
  return NULL;
}

void MergeIntoPrepopulatedEngineData(const TemplateURL* original_turl,
                                     TemplateURLData* prepopulated_url) {
  DCHECK_EQ(original_turl->prepopulate_id(), prepopulated_url->prepopulate_id);
  if (!original_turl->safe_for_autoreplace()) {
    prepopulated_url->safe_for_autoreplace = false;
    prepopulated_url->SetKeyword(original_turl->keyword());
    prepopulated_url->SetShortName(original_turl->short_name());
  }
  prepopulated_url->id = original_turl->id();
  prepopulated_url->sync_guid = original_turl->sync_guid();
  prepopulated_url->date_created = original_turl->date_created();
  prepopulated_url->last_modified = original_turl->last_modified();
}

ActionsFromPrepopulateData::ActionsFromPrepopulateData() {}

ActionsFromPrepopulateData::ActionsFromPrepopulateData(
    const ActionsFromPrepopulateData& other) = default;

ActionsFromPrepopulateData::~ActionsFromPrepopulateData() {}

// This is invoked when the version of the prepopulate data changes.
// If |removed_keyword_guids| is not NULL, the Sync GUID of each item removed
// from the DB will be added to it.  Note that this function will take
// ownership of |prepopulated_urls| and will clear the vector.
void MergeEnginesFromPrepopulateData(
    KeywordWebDataService* service,
    ScopedVector<TemplateURLData>* prepopulated_urls,
    size_t default_search_index,
    TemplateURLService::TemplateURLVector* template_urls,
    TemplateURL* default_search_provider,
    std::set<std::string>* removed_keyword_guids) {
  DCHECK(prepopulated_urls);
  DCHECK(template_urls);

  ActionsFromPrepopulateData actions(CreateActionsFromCurrentPrepopulateData(
      prepopulated_urls, *template_urls, default_search_provider));

  // Remove items.
  for (std::vector<TemplateURL*>::iterator i = actions.removed_engines.begin();
       i < actions.removed_engines.end(); ++i) {
    std::unique_ptr<TemplateURL> template_url(*i);
    TemplateURLService::TemplateURLVector::iterator j = std::find(
        template_urls->begin(), template_urls->end(), template_url.get());
    DCHECK(j != template_urls->end());
    DCHECK(!default_search_provider ||
           (*j)->prepopulate_id() != default_search_provider->prepopulate_id());
    template_urls->erase(j);
    if (service) {
      service->RemoveKeyword(template_url->id());
      if (removed_keyword_guids)
        removed_keyword_guids->insert(template_url->sync_guid());
    }
  }

  // Edit items.
  for (EditedEngines::iterator i(actions.edited_engines.begin());
       i < actions.edited_engines.end(); ++i) {
    TemplateURLData& data = i->second;
    std::unique_ptr<TemplateURL> existing_url(i->first);
    if (service)
      service->UpdateKeyword(data);

    // Replace the entry in |template_urls| with the updated one.
    TemplateURLService::TemplateURLVector::iterator j = std::find(
        template_urls->begin(), template_urls->end(), existing_url.get());
    *j = new TemplateURL(data);
  }

  // Add items.
  for (std::vector<TemplateURLData>::const_iterator it =
           actions.added_engines.begin();
       it != actions.added_engines.end();
       ++it) {
    template_urls->push_back(new TemplateURL(*it));
  }
}

ActionsFromPrepopulateData CreateActionsFromCurrentPrepopulateData(
    ScopedVector<TemplateURLData>* prepopulated_urls,
    const TemplateURLService::TemplateURLVector& existing_urls,
    const TemplateURL* default_search_provider) {
  // Create a map to hold all provided |template_urls| that originally came from
  // prepopulate data (i.e. have a non-zero prepopulate_id()).
  typedef std::map<int, TemplateURL*> IDMap;
  IDMap id_to_turl;
  for (TemplateURLService::TemplateURLVector::const_iterator i(
       existing_urls.begin()); i != existing_urls.end(); ++i) {
    int prepopulate_id = (*i)->prepopulate_id();
    if (prepopulate_id > 0)
      id_to_turl[prepopulate_id] = *i;
  }

  // For each current prepopulated URL, check whether |template_urls| contained
  // a matching prepopulated URL.  If so, update the passed-in URL to match the
  // current data.  (If the passed-in URL was user-edited, we persist the user's
  // name and keyword.)  If not, add the prepopulated URL.
  ActionsFromPrepopulateData actions;
  for (size_t i = 0; i < prepopulated_urls->size(); ++i) {
    // We take ownership of |prepopulated_urls[i]|.
    std::unique_ptr<TemplateURLData> prepopulated_url((*prepopulated_urls)[i]);
    const int prepopulated_id = prepopulated_url->prepopulate_id;
    DCHECK_NE(0, prepopulated_id);

    IDMap::iterator existing_url_iter(id_to_turl.find(prepopulated_id));
    if (existing_url_iter != id_to_turl.end()) {
      // Update the data store with the new prepopulated data. Preserve user
      // edits to the name and keyword.
      TemplateURL* existing_url(existing_url_iter->second);
      id_to_turl.erase(existing_url_iter);
      MergeIntoPrepopulatedEngineData(existing_url, prepopulated_url.get());
      // Update last_modified to ensure that if this entry is later merged with
      // entries from Sync, the conflict resolution logic knows that this was
      // updated and propagates the new values to the server.
      prepopulated_url->last_modified = base::Time::Now();
      actions.edited_engines.push_back(
          std::make_pair(existing_url, *prepopulated_url));
    } else {
      actions.added_engines.push_back(*prepopulated_url);
    }
  }
  // The above loop takes ownership of all the contents of prepopulated_urls.
  // Clear the pointers.
  prepopulated_urls->weak_erase(prepopulated_urls->begin(),
                                prepopulated_urls->end());

  // The block above removed all the URLs from the |id_to_turl| map that were
  // found in the prepopulate data.  Any remaining URLs that haven't been
  // user-edited or made default can be removed from the data store.
  // We assume that this entry is equivalent to the DSE if its prepopulate ID
  // and keyword both match. If the prepopulate ID _does_ match all properties
  // will be replaced with those from |default_search_provider| anyway.
  for (IDMap::iterator i(id_to_turl.begin()); i != id_to_turl.end(); ++i) {
    TemplateURL* template_url = i->second;
    if ((template_url->safe_for_autoreplace()) &&
        (!default_search_provider ||
         (template_url->prepopulate_id() !=
             default_search_provider->prepopulate_id()) ||
         (template_url->keyword() != default_search_provider->keyword())))
      actions.removed_engines.push_back(template_url);
  }

  return actions;
}

void GetSearchProvidersUsingKeywordResult(
    const WDTypedResult& result,
    KeywordWebDataService* service,
    PrefService* prefs,
    TemplateURLService::TemplateURLVector* template_urls,
    TemplateURL* default_search_provider,
    const SearchTermsData& search_terms_data,
    int* new_resource_keyword_version,
    std::set<std::string>* removed_keyword_guids) {
  DCHECK(template_urls);
  DCHECK(template_urls->empty());
  DCHECK_EQ(KEYWORDS_RESULT, result.GetType());
  DCHECK(new_resource_keyword_version);

  WDKeywordsResult keyword_result = reinterpret_cast<
      const WDResult<WDKeywordsResult>*>(&result)->GetValue();

  for (KeywordTable::Keywords::iterator i(keyword_result.keywords.begin());
       i != keyword_result.keywords.end(); ++i) {
    // Fix any duplicate encodings in the local database.  Note that we don't
    // adjust the last_modified time of this keyword; this way, we won't later
    // overwrite any changes on the sync server that happened to this keyword
    // since the last time we synced.  Instead, we also run a de-duping pass on
    // the server-provided data in
    // TemplateURLService::CreateTemplateURLFromTemplateURLAndSyncData() and
    // update the server with the merged, de-duped results at that time.  We
    // still fix here, though, to correct problems in clients that have disabled
    // search engine sync, since in that case that code will never be reached.
    if (DeDupeEncodings(&i->input_encodings) && service)
      service->UpdateKeyword(*i);
    template_urls->push_back(new TemplateURL(*i));
  }

  *new_resource_keyword_version = keyword_result.builtin_keyword_version;
  GetSearchProvidersUsingLoadedEngines(service, prefs, template_urls,
                                       default_search_provider,
                                       search_terms_data,
                                       new_resource_keyword_version,
                                       removed_keyword_guids);
}

void GetSearchProvidersUsingLoadedEngines(
    KeywordWebDataService* service,
    PrefService* prefs,
    TemplateURLService::TemplateURLVector* template_urls,
    TemplateURL* default_search_provider,
    const SearchTermsData& search_terms_data,
    int* resource_keyword_version,
    std::set<std::string>* removed_keyword_guids) {
  DCHECK(template_urls);
  DCHECK(resource_keyword_version);
  size_t default_search_index;
  ScopedVector<TemplateURLData> prepopulated_urls =
      TemplateURLPrepopulateData::GetPrepopulatedEngines(prefs,
                                                         &default_search_index);
  RemoveDuplicatePrepopulateIDs(service, prepopulated_urls,
                                default_search_provider, template_urls,
                                search_terms_data, removed_keyword_guids);

  const int prepopulate_resource_keyword_version =
      TemplateURLPrepopulateData::GetDataVersion(prefs);
  if (*resource_keyword_version < prepopulate_resource_keyword_version) {
    MergeEnginesFromPrepopulateData(
        service, &prepopulated_urls, default_search_index, template_urls,
        default_search_provider, removed_keyword_guids);
    *resource_keyword_version = prepopulate_resource_keyword_version;
  } else {
    *resource_keyword_version = 0;
  }
}

bool DeDupeEncodings(std::vector<std::string>* encodings) {
  std::vector<std::string> deduped_encodings;
  std::set<std::string> encoding_set;
  for (std::vector<std::string>::const_iterator i(encodings->begin());
       i != encodings->end(); ++i) {
    if (encoding_set.insert(*i).second)
      deduped_encodings.push_back(*i);
  }
  encodings->swap(deduped_encodings);
  return encodings->size() != deduped_encodings.size();
}
