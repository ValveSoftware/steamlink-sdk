// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/event_filter.h"

#include <string>
#include <utility>

#include "components/url_matcher/url_matcher_factory.h"
#include "ipc/ipc_message.h"

using url_matcher::URLMatcher;
using url_matcher::URLMatcherConditionSet;
using url_matcher::URLMatcherFactory;

namespace extensions {

EventFilter::EventMatcherEntry::EventMatcherEntry(
    std::unique_ptr<EventMatcher> event_matcher,
    URLMatcher* url_matcher,
    const URLMatcherConditionSet::Vector& condition_sets)
    : event_matcher_(std::move(event_matcher)), url_matcher_(url_matcher) {
  for (URLMatcherConditionSet::Vector::const_iterator it =
       condition_sets.begin(); it != condition_sets.end(); it++)
    condition_set_ids_.push_back((*it)->id());
  url_matcher_->AddConditionSets(condition_sets);
}

EventFilter::EventMatcherEntry::~EventMatcherEntry() {
  url_matcher_->RemoveConditionSets(condition_set_ids_);
}

void EventFilter::EventMatcherEntry::DontRemoveConditionSetsInDestructor() {
  condition_set_ids_.clear();
}

EventFilter::EventFilter()
    : next_id_(0),
      next_condition_set_id_(0) {
}

EventFilter::~EventFilter() {
  // Normally when an event matcher entry is removed from event_matchers_ it
  // will remove its condition sets from url_matcher_, but as url_matcher_ is
  // being destroyed anyway there is no need to do that step here.
  for (EventMatcherMultiMap::iterator it = event_matchers_.begin();
       it != event_matchers_.end(); it++) {
    for (EventMatcherMap::iterator it2 = it->second.begin();
         it2 != it->second.end(); it2++) {
      it2->second->DontRemoveConditionSetsInDestructor();
    }
  }
}

EventFilter::MatcherID EventFilter::AddEventMatcher(
    const std::string& event_name,
    std::unique_ptr<EventMatcher> matcher) {
  MatcherID id = next_id_++;
  URLMatcherConditionSet::Vector condition_sets;
  if (!CreateConditionSets(id, matcher.get(), &condition_sets))
    return -1;

  for (URLMatcherConditionSet::Vector::iterator it = condition_sets.begin();
       it != condition_sets.end(); it++) {
    condition_set_id_to_event_matcher_id_.insert(
        std::make_pair((*it)->id(), id));
  }
  id_to_event_name_[id] = event_name;
  event_matchers_[event_name][id] = linked_ptr<EventMatcherEntry>(
      new EventMatcherEntry(std::move(matcher), &url_matcher_, condition_sets));
  return id;
}

EventMatcher* EventFilter::GetEventMatcher(MatcherID id) {
  DCHECK(id_to_event_name_.find(id) != id_to_event_name_.end());
  const std::string& event_name = id_to_event_name_[id];
  return event_matchers_[event_name][id]->event_matcher();
}

const std::string& EventFilter::GetEventName(MatcherID id) {
  DCHECK(id_to_event_name_.find(id) != id_to_event_name_.end());
  return id_to_event_name_[id];
}

bool EventFilter::CreateConditionSets(
    MatcherID id,
    EventMatcher* matcher,
    URLMatcherConditionSet::Vector* condition_sets) {
  if (matcher->GetURLFilterCount() == 0) {
    // If there are no URL filters then we want to match all events, so create a
    // URLFilter from an empty dictionary.
    base::DictionaryValue empty_dict;
    return AddDictionaryAsConditionSet(&empty_dict, condition_sets);
  }
  for (int i = 0; i < matcher->GetURLFilterCount(); i++) {
    base::DictionaryValue* url_filter;
    if (!matcher->GetURLFilter(i, &url_filter))
      return false;
    if (!AddDictionaryAsConditionSet(url_filter, condition_sets))
      return false;
  }
  return true;
}

bool EventFilter::AddDictionaryAsConditionSet(
    base::DictionaryValue* url_filter,
    URLMatcherConditionSet::Vector* condition_sets) {
  std::string error;
  URLMatcherConditionSet::ID condition_set_id = next_condition_set_id_++;
  condition_sets->push_back(URLMatcherFactory::CreateFromURLFilterDictionary(
      url_matcher_.condition_factory(),
      url_filter,
      condition_set_id,
      &error));
  if (!error.empty()) {
    LOG(ERROR) << "CreateFromURLFilterDictionary failed: " << error;
    url_matcher_.ClearUnusedConditionSets();
    condition_sets->clear();
    return false;
  }
  return true;
}

std::string EventFilter::RemoveEventMatcher(MatcherID id) {
  std::map<MatcherID, std::string>::iterator it = id_to_event_name_.find(id);
  std::string event_name = it->second;
  // EventMatcherEntry's destructor causes the condition set ids to be removed
  // from url_matcher_.
  event_matchers_[event_name].erase(id);
  id_to_event_name_.erase(it);
  return event_name;
}

std::set<EventFilter::MatcherID> EventFilter::MatchEvent(
    const std::string& event_name, const EventFilteringInfo& event_info,
    int routing_id) {
  std::set<MatcherID> matchers;

  EventMatcherMultiMap::iterator it = event_matchers_.find(event_name);
  if (it == event_matchers_.end())
    return matchers;

  EventMatcherMap& matcher_map = it->second;
  GURL url_to_match_against = event_info.has_url() ? event_info.url() : GURL();
  std::set<URLMatcherConditionSet::ID> matching_condition_set_ids =
      url_matcher_.MatchURL(url_to_match_against);
  for (std::set<URLMatcherConditionSet::ID>::iterator it =
       matching_condition_set_ids.begin();
       it != matching_condition_set_ids.end(); it++) {
    std::map<URLMatcherConditionSet::ID, MatcherID>::iterator matcher_id =
        condition_set_id_to_event_matcher_id_.find(*it);
    if (matcher_id == condition_set_id_to_event_matcher_id_.end()) {
      NOTREACHED() << "id not found in condition set map (" << (*it) << ")";
      continue;
    }
    MatcherID id = matcher_id->second;
    EventMatcherMap::iterator matcher_entry = matcher_map.find(id);
    if (matcher_entry == matcher_map.end()) {
      // Matcher must be for a different event.
      continue;
    }
    const EventMatcher* event_matcher = matcher_entry->second->event_matcher();
    // The context that installed the event listener should be the same context
    // as the one where the event listener is called.
    if ((routing_id != MSG_ROUTING_NONE) &&
        (event_matcher->GetRoutingID() != routing_id)) {
      continue;
    }
    if (event_matcher->MatchNonURLCriteria(event_info)) {
      CHECK(!event_matcher->HasURLFilters() || event_info.has_url());
      matchers.insert(id);
    }
  }

  return matchers;
}

int EventFilter::GetMatcherCountForEvent(const std::string& name) {
  EventMatcherMultiMap::const_iterator it = event_matchers_.find(name);
  if (it == event_matchers_.end())
    return 0;

  return it->second.size();
}

}  // namespace extensions
