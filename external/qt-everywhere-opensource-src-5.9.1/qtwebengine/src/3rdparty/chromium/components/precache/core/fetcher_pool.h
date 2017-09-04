// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRECACHE_CORE_FETCHER_POOL_H_
#define COMPONENTS_PRECACHE_CORE_FETCHER_POOL_H_

#include <memory>
#include <unordered_map>

#include "base/gtest_prod_util.h"
#include "base/logging.h"

namespace precache {

// FetcherPool that accepts a limited number of elements.
//
// FetcherPool is particularly suited for having multiple URLFetchers running
// in parallel.
//
// It doesn't enqueue the elements above a defined capacity. The callsite must
// check for IsAvailable before calling Start.
//
// Example usage:
//   std::list<GURL> pending_urls = ...;
//   FetcherPool<net::URLFetcher> pool(max_parallel_fetches);
//   std::function<void()> start_next_batch =
//       [&pending_urls, &pool]() {
//         while (!pending_urls.empty() && pool.IsAvailable()) {
//           pool.Add(CreateAndStartUrlFetcher(pending_urls.front()));
//           pending_urls.pop_front();
//         }
//       };
//   // The URLFetcherDelegate of the created URLFetchers MUST call
//   // pool.Release(url_fetcher) and start_next_batch() as part of
//   // OnURLFetchComplete.
//   start_next_batch();
template <typename T>
class FetcherPool {
 public:
  explicit FetcherPool(size_t max_size) : max_size_(max_size){};
  virtual ~FetcherPool(){};

  // Takes ownership and adds the given |element| to the pool.
  // The element will live until its deletion.
  void Add(std::unique_ptr<T> element) {
    DCHECK(IsAvailable()) << "FetcherPool size exceeded. "
                             "Did you check IsAvailable?";
    DCHECK(element) << "The element cannot be null.";
    DCHECK(elements_.find(element.get()) == elements_.end())
        << "The pool already contains the given element.";
    elements_[element.get()] = std::move(element);
  }

  // Deletes the given |element| from the pool.
  void Delete(const T& element) {
    DCHECK(elements_.find(&element) != elements_.end())
        << "The pool doesn't contain the given element.";
    elements_.erase(&element);
  }

  // Deletes all the elements in the pool.
  void DeleteAll() { elements_.clear(); }

  // Returns true iff the pool is empty.
  bool IsEmpty() const { return elements_.empty(); }

  // Returns true iff the pool can accept a new element.
  bool IsAvailable() const { return max_size_ > elements_.size(); }

  const std::unordered_map<const T*, std::unique_ptr<T>>& elements() const {
    return elements_;
  }

  // Returns the maximum size of the pool.
  size_t max_size() const { return max_size_; }

 private:
  const size_t max_size_;
  std::unordered_map<const T*, std::unique_ptr<T>> elements_;

  DISALLOW_COPY_AND_ASSIGN(FetcherPool);
};

}  // namespace precache

#endif  // COMPONENTS_PRECACHE_CORE_FETCHER_POOL_H_
