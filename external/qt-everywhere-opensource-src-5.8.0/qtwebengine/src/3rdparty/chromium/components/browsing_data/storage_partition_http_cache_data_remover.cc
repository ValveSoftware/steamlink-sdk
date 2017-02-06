// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/storage_partition_http_cache_data_remover.h"

#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/browsing_data/conditional_cache_deletion_helper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/sdch_manager.h"
#include "net/disk_cache/blockfile/backend_impl.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/memory/mem_backend_impl.h"
#include "net/disk_cache/simple/simple_backend_impl.h"
#include "net/http/http_cache.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

using content::BrowserThread;

namespace browsing_data {

StoragePartitionHttpCacheDataRemover::StoragePartitionHttpCacheDataRemover(
    base::Callback<bool(const GURL&)> url_predicate,
    base::Time delete_begin,
    base::Time delete_end,
    net::URLRequestContextGetter* main_context_getter,
    net::URLRequestContextGetter* media_context_getter)
    : url_predicate_(url_predicate),
      delete_begin_(delete_begin),
      delete_end_(delete_end),
      main_context_getter_(main_context_getter),
      media_context_getter_(media_context_getter),
      next_cache_state_(STATE_NONE),
      cache_(nullptr),
      calculation_result_(0) {
}

StoragePartitionHttpCacheDataRemover::~StoragePartitionHttpCacheDataRemover() {
}

// static.
StoragePartitionHttpCacheDataRemover*
StoragePartitionHttpCacheDataRemover::CreateForRange(
    content::StoragePartition* storage_partition,
    base::Time delete_begin,
    base::Time delete_end) {
  return new StoragePartitionHttpCacheDataRemover(
      base::Callback<bool(const GURL&)>(),  // Null callback.
      delete_begin, delete_end,
      storage_partition->GetURLRequestContext(),
      storage_partition->GetMediaURLRequestContext());
}

// static.
StoragePartitionHttpCacheDataRemover*
StoragePartitionHttpCacheDataRemover::CreateForURLsAndRange(
    content::StoragePartition* storage_partition,
    const base::Callback<bool(const GURL&)>& url_predicate,
    base::Time delete_begin,
    base::Time delete_end) {
  return new StoragePartitionHttpCacheDataRemover(
      url_predicate, delete_begin, delete_end,
      storage_partition->GetURLRequestContext(),
      storage_partition->GetMediaURLRequestContext());
}

void StoragePartitionHttpCacheDataRemover::Remove(
    const base::Closure& done_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!done_callback.is_null());
  done_callback_ = done_callback;

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(
          &StoragePartitionHttpCacheDataRemover::ClearHttpCacheOnIOThread,
          base::Unretained(this)));
}

void StoragePartitionHttpCacheDataRemover::Count(
    const net::Int64CompletionCallback& result_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!result_callback.is_null());
  result_callback_ = result_callback;
  calculation_result_ = 0;

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(
          &StoragePartitionHttpCacheDataRemover::CountHttpCacheOnIOThread,
          base::Unretained(this)));
}

void StoragePartitionHttpCacheDataRemover::ClearHttpCacheOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  next_cache_state_ = STATE_NONE;
  DCHECK_EQ(STATE_NONE, next_cache_state_);
  DCHECK(main_context_getter_.get());
  DCHECK(media_context_getter_.get());

  next_cache_state_ = STATE_CREATE_MAIN;
  DoClearCache(net::OK);
}

void StoragePartitionHttpCacheDataRemover::CountHttpCacheOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  next_cache_state_ = STATE_NONE;
  DCHECK_EQ(STATE_NONE, next_cache_state_);
  DCHECK(main_context_getter_.get());
  DCHECK(media_context_getter_.get());

  next_cache_state_ = STATE_CREATE_MAIN;
  DoCountCache(net::OK);
}

void StoragePartitionHttpCacheDataRemover::ClearedHttpCache() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  done_callback_.Run();
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

void StoragePartitionHttpCacheDataRemover::CountedHttpCache() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  result_callback_.Run(calculation_result_);
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

// The expected state sequence is STATE_NONE --> STATE_CREATE_MAIN -->
// STATE_PROCESS_MAIN --> STATE_CREATE_MEDIA --> STATE_PROCESS_MEDIA -->
// STATE_DONE, and any errors are ignored.
void StoragePartitionHttpCacheDataRemover::DoClearCache(int rv) {
  DCHECK_NE(STATE_NONE, next_cache_state_);

  while (rv != net::ERR_IO_PENDING && next_cache_state_ != STATE_NONE) {
    switch (next_cache_state_) {
      case STATE_CREATE_MAIN:
      case STATE_CREATE_MEDIA: {
        // Get a pointer to the cache.
        net::URLRequestContextGetter* getter =
            (next_cache_state_ == STATE_CREATE_MAIN)
                ? main_context_getter_.get()
                : media_context_getter_.get();
        net::HttpCache* http_cache = getter->GetURLRequestContext()
                                         ->http_transaction_factory()
                                         ->GetCache();

        next_cache_state_ = (next_cache_state_ == STATE_CREATE_MAIN)
                                ? STATE_PROCESS_MAIN
                                : STATE_PROCESS_MEDIA;

        // Clear QUIC server information from memory and the disk cache.
        http_cache->GetSession()
            ->quic_stream_factory()
            ->ClearCachedStatesInCryptoConfig();

        // Clear SDCH dictionary state.
        net::SdchManager* sdch_manager =
            getter->GetURLRequestContext()->sdch_manager();
        // The test is probably overkill, since chrome should always have an
        // SdchManager.  But in general the URLRequestContext  is *not*
        // guaranteed to have an SdchManager, so checking is wise.
        if (sdch_manager)
          sdch_manager->ClearData();

        rv = http_cache->GetBackend(
            &cache_,
            base::Bind(&StoragePartitionHttpCacheDataRemover::DoClearCache,
                       base::Unretained(this)));
        break;
      }
      case STATE_PROCESS_MAIN:
      case STATE_PROCESS_MEDIA: {
        next_cache_state_ = (next_cache_state_ == STATE_PROCESS_MAIN)
                                ? STATE_CREATE_MEDIA
                                : STATE_DONE;

        // |cache_| can be null if it cannot be initialized.
        if (cache_) {
          if (!url_predicate_.is_null()) {
            rv = (new ConditionalCacheDeletionHelper(
                cache_,
                ConditionalCacheDeletionHelper::CreateURLAndTimeCondition(
                    url_predicate_,
                    delete_begin_,
                    delete_end_)))->DeleteAndDestroySelfWhenFinished(
                        base::Bind(
                            &StoragePartitionHttpCacheDataRemover::DoClearCache,
                            base::Unretained(this)));
          } else if (delete_begin_.is_null() && delete_end_.is_max()) {
            rv = cache_->DoomAllEntries(base::Bind(
                &StoragePartitionHttpCacheDataRemover::DoClearCache,
                base::Unretained(this)));
          } else {
            rv = cache_->DoomEntriesBetween(
                delete_begin_, delete_end_,
                base::Bind(
                    &StoragePartitionHttpCacheDataRemover::DoClearCache,
                    base::Unretained(this)));
          }
          cache_ = NULL;
        }
        break;
      }
      case STATE_DONE: {
        cache_ = NULL;
        next_cache_state_ = STATE_NONE;

        // Notify the UI thread that we are done.
        BrowserThread::PostTask(
            BrowserThread::UI, FROM_HERE,
            base::Bind(&StoragePartitionHttpCacheDataRemover::ClearedHttpCache,
                       base::Unretained(this)));
        return;
      }
      default: {
        NOTREACHED() << "bad state";
        next_cache_state_ = STATE_NONE;  // Stop looping.
        return;
      }
    }
  }
}

// The expected state sequence is STATE_NONE --> STATE_CREATE_MAIN -->
// STATE_PROCESS_MAIN --> STATE_CREATE_MEDIA --> STATE_PROCESS_MEDIA -->
// STATE_DONE. On error, we jump directly to STATE_DONE.
void StoragePartitionHttpCacheDataRemover::DoCountCache(int rv) {
  DCHECK_NE(STATE_NONE, next_cache_state_);

  while (rv != net::ERR_IO_PENDING && next_cache_state_ != STATE_NONE) {
    // On error, finish and return the error code. A valid result value might
    // be of two types - either net::OK from the CREATE states, or the result
    // of calculation from the PROCESS states. Since net::OK == 0, it is valid
    // to simply add the value to the final calculation result.
    if (rv < 0) {
      calculation_result_ = rv;
      next_cache_state_ = STATE_DONE;
    } else {
      DCHECK_EQ(0, net::OK);
      calculation_result_ += rv;
    }

    switch (next_cache_state_) {
      case STATE_CREATE_MAIN:
      case STATE_CREATE_MEDIA: {
        // Get a pointer to the cache.
        net::URLRequestContextGetter* getter =
            (next_cache_state_ == STATE_CREATE_MAIN)
                ? main_context_getter_.get()
                : media_context_getter_.get();
        net::HttpCache* http_cache = getter->GetURLRequestContext()
                                         ->http_transaction_factory()
                                         ->GetCache();

        next_cache_state_ = (next_cache_state_ == STATE_CREATE_MAIN)
                                ? STATE_PROCESS_MAIN
                                : STATE_PROCESS_MEDIA;

        rv = http_cache->GetBackend(
            &cache_,
            base::Bind(&StoragePartitionHttpCacheDataRemover::DoCountCache,
                       base::Unretained(this)));
        break;
      }
      case STATE_PROCESS_MAIN:
      case STATE_PROCESS_MEDIA: {
        next_cache_state_ = (next_cache_state_ == STATE_PROCESS_MAIN)
                                ? STATE_CREATE_MEDIA
                                : STATE_DONE;

        // |cache_| can be null if it cannot be initialized.
        if (cache_) {
          if (delete_begin_.is_null() && delete_end_.is_max()) {
            rv = cache_->CalculateSizeOfAllEntries(
                base::Bind(
                    &StoragePartitionHttpCacheDataRemover::DoCountCache,
                    base::Unretained(this)));
          } else {
            // TODO(msramek): Implement this when we need it.
            DoCountCache(net::ERR_NOT_IMPLEMENTED);
          }
          cache_ = NULL;
        }
        break;
      }
      case STATE_DONE: {
        cache_ = NULL;
        next_cache_state_ = STATE_NONE;

        // Notify the UI thread that we are done.
        BrowserThread::PostTask(
            BrowserThread::UI, FROM_HERE,
            base::Bind(&StoragePartitionHttpCacheDataRemover::CountedHttpCache,
                       base::Unretained(this)));
        return;
      }
      default: {
        NOTREACHED() << "bad state";
        next_cache_state_ = STATE_NONE;  // Stop looping.
        return;
      }
    }
  }
}

}  // namespace browsing_data
