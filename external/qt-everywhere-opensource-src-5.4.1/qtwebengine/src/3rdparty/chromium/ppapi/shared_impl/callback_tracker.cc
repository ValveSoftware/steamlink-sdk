// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/callback_tracker.h"

#include <algorithm>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/shared_impl/tracked_callback.h"

namespace ppapi {

// CallbackTracker -------------------------------------------------------------

CallbackTracker::CallbackTracker() {}

void CallbackTracker::AbortAll() {
  // Iterate over a copy since |Abort()| calls |Remove()| (indirectly).
  // TODO(viettrungluu): This obviously isn't so efficient.
  CallbackSetMap pending_callbacks_copy = pending_callbacks_;
  for (CallbackSetMap::iterator it1 = pending_callbacks_copy.begin();
       it1 != pending_callbacks_copy.end();
       ++it1) {
    for (CallbackSet::iterator it2 = it1->second.begin();
         it2 != it1->second.end();
         ++it2) {
      (*it2)->Abort();
    }
  }
}

void CallbackTracker::PostAbortForResource(PP_Resource resource_id) {
  CHECK(resource_id != 0);
  CallbackSetMap::iterator it1 = pending_callbacks_.find(resource_id);
  if (it1 == pending_callbacks_.end())
    return;
  for (CallbackSet::iterator it2 = it1->second.begin();
       it2 != it1->second.end();
       ++it2) {
    // Post the abort.
    (*it2)->PostAbort();
  }
}

CallbackTracker::~CallbackTracker() {
  // All callbacks must be aborted before destruction.
  CHECK_EQ(0u, pending_callbacks_.size());
}

void CallbackTracker::Add(
    const scoped_refptr<TrackedCallback>& tracked_callback) {
  PP_Resource resource_id = tracked_callback->resource_id();
  DCHECK(pending_callbacks_[resource_id].find(tracked_callback) ==
         pending_callbacks_[resource_id].end());
  pending_callbacks_[resource_id].insert(tracked_callback);
}

void CallbackTracker::Remove(
    const scoped_refptr<TrackedCallback>& tracked_callback) {
  CallbackSetMap::iterator map_it =
      pending_callbacks_.find(tracked_callback->resource_id());
  DCHECK(map_it != pending_callbacks_.end());
  CallbackSet::iterator it = map_it->second.find(tracked_callback);
  DCHECK(it != map_it->second.end());
  map_it->second.erase(it);

  // If there are no pending callbacks left for this ID, get rid of the entry.
  if (map_it->second.empty())
    pending_callbacks_.erase(map_it);
}

}  // namespace ppapi
