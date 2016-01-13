// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_DATA_MEMOIZING_STORE_H_
#define CONTENT_BROWSER_RENDERER_DATA_MEMOIZING_STORE_H_

#include <map>

#include "base/bind.h"
#include "base/synchronization/lock.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"

namespace content {

// RendererDataMemoizingStore is a thread-safe container that retains reference
// counted objects that are associated with one or more render processes.
// Objects are identified by an int and only a single reference to a given
// object is retained. RendererDataMemoizingStore watches for render process
// termination and releases objects that are no longer associated with any
// render process.
//
// TODO(jcampan): Rather than watching for render process termination, we should
//                instead be listening to events such as resource cached/
//                removed from cache, and remove the items when we know they
//                are not used anymore.
template <typename T>
class RendererDataMemoizingStore : public RenderProcessHostObserver {
 public:
  RendererDataMemoizingStore() : next_item_id_(1) {
  }

  ~RendererDataMemoizingStore() {
    DCHECK_EQ(0U, id_to_item_.size()) << "Failed to outlive render processes";
  }

  // Store adds |item| to this collection, associates it with the given render
  // process id and returns an opaque identifier for it. If |item| is already
  // known, the same identifier will be returned.
  int Store(T* item, int process_id) {
    DCHECK(item);
    base::AutoLock auto_lock(lock_);

    int item_id;

    // Do we already know this item?
    typename ReverseItemMap::iterator item_iter = item_to_id_.find(item);
    if (item_iter == item_to_id_.end()) {
      item_id = next_item_id_++;
      // We use 0 as an invalid item_id value.  In the unlikely event that
      // next_item_id_ wraps around, we reset it to 1.
      if (next_item_id_ == 0)
        next_item_id_ = 1;
      id_to_item_[item_id] = item;
      item_to_id_[item] = item_id;
    } else {
      item_id = item_iter->second;
    }

    // Let's update process_id_to_item_id_.
    std::pair<IDMap::iterator, IDMap::iterator> process_ids =
        process_id_to_item_id_.equal_range(process_id);
    bool already_watching_process = (process_ids.first != process_ids.second);
    if (std::find_if(process_ids.first, process_ids.second,
                     MatchSecond<int>(item_id)) == process_ids.second) {
      process_id_to_item_id_.insert(std::make_pair(process_id, item_id));
    }

    // And item_id_to_process_id_.
    std::pair<IDMap::iterator, IDMap::iterator> item_ids =
        item_id_to_process_id_.equal_range(item_id);
    if (std::find_if(item_ids.first, item_ids.second,
                     MatchSecond<int>(process_id)) == item_ids.second) {
      item_id_to_process_id_.insert(std::make_pair(item_id, process_id));
    }

    // If we're not doing so already, keep an eye for the process host deletion.
    if (!already_watching_process) {
      if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
        StartObservingProcess(process_id);
      } else {
        BrowserThread::PostTask(
            BrowserThread::UI,
            FROM_HERE,
            base::Bind(&RendererDataMemoizingStore::StartObservingProcess,
                       base::Unretained(this),
                       process_id));
      }
    }

    DCHECK(item_id);
    return item_id;
  }

  // Retrieve fetches a previously Stored() item, identified by |item_id|.
  // If |item_id| is recognized, |item| will be updated and Retrieve() will
  // return true, it will otherwise return false.
  bool Retrieve(int item_id, scoped_refptr<T>* item) {
    base::AutoLock auto_lock(lock_);

    typename ItemMap::iterator iter = id_to_item_.find(item_id);
    if (iter == id_to_item_.end())
      return false;
    if (item)
      *item = iter->second;
    return true;
  }

 private:
  typedef std::multimap<int, int> IDMap;
  typedef std::map<int, scoped_refptr<T> > ItemMap;
  typedef std::map<T*, int, typename T::LessThan> ReverseItemMap;

  template <typename M>
  struct MatchSecond {
    explicit MatchSecond(const M& t) : value(t) {}

    template <typename Pair>
    bool operator()(const Pair& p) const {
      return (value == p.second);
    }

    M value;
  };

  void StartObservingProcess(int process_id) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    RenderProcessHost* host = RenderProcessHost::FromID(process_id);
    if (!host) {
      // We lost the race to observe the host before it was destroyed. Since
      // this function was called because we're managing objects tied to that
      // (now destroyed) RenderProcessHost, let's clean up.
      RemoveRenderProcessItems(process_id);
      return;
    }

    host->AddObserver(this);
  }

  // Remove the item specified by |item_id| from id_to_item_ and item_to_id_.
  // NOTE: the caller (RemoveRenderProcessItems) must hold lock_.
  void RemoveInternal(int item_id) {
    typename ItemMap::iterator item_iter = id_to_item_.find(item_id);
    DCHECK(item_iter != id_to_item_.end());

    typename ReverseItemMap::iterator id_iter =
        item_to_id_.find(item_iter->second.get());
    DCHECK(id_iter != item_to_id_.end());
    item_to_id_.erase(id_iter);

    id_to_item_.erase(item_iter);
  }

  void RenderProcessHostDestroyed(RenderProcessHost* host) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    RemoveRenderProcessItems(host->GetID());
  }

  // Removes all the items associated with the specified process from the store.
  void RemoveRenderProcessItems(int process_id) {
    base::AutoLock auto_lock(lock_);

    // We iterate through all the item ids for that process.
    std::pair<IDMap::iterator, IDMap::iterator> process_ids =
        process_id_to_item_id_.equal_range(process_id);
    for (IDMap::iterator ids_iter = process_ids.first;
         ids_iter != process_ids.second; ++ids_iter) {
      int item_id = ids_iter->second;
      // Find all the processes referring to this item id in
      // item_id_to_process_id_, then locate the process being removed within
      // that range.
      std::pair<IDMap::iterator, IDMap::iterator> item_ids =
          item_id_to_process_id_.equal_range(item_id);
      IDMap::iterator proc_iter = std::find_if(
          item_ids.first, item_ids.second, MatchSecond<int>(process_id));
      DCHECK(proc_iter != item_ids.second);

      // Before removing, determine if no other processes refer to the current
      // item id. If |proc_iter| (the current process) is the lower bound of
      // processes containing the current item id and if |next_proc_iter| is the
      // upper bound (the first process that does not), then only one process,
      // the one being removed, refers to the item id.
      IDMap::iterator next_proc_iter = proc_iter;
      ++next_proc_iter;
      bool last_process_for_item_id =
          (proc_iter == item_ids.first && next_proc_iter == item_ids.second);
      item_id_to_process_id_.erase(proc_iter);

      if (last_process_for_item_id) {
        // The current item id is not referenced by any other processes, so
        // remove it from id_to_item_ and item_to_id_.
        RemoveInternal(item_id);
      }
    }
    if (process_ids.first != process_ids.second)
      process_id_to_item_id_.erase(process_ids.first, process_ids.second);
  }

  IDMap process_id_to_item_id_;
  IDMap item_id_to_process_id_;
  ItemMap id_to_item_;
  ReverseItemMap item_to_id_;

  int next_item_id_;

  // This lock protects: process_id_to_item_id_, item_id_to_process_id_,
  //                     id_to_item_, and item_to_id_.
  base::Lock lock_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_DATA_MEMOIZING_STORE_H_
