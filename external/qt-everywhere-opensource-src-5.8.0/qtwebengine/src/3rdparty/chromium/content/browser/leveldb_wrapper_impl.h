// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LEVELDB_WRAPPER_IMPL_H_
#define CONTENT_BROWSER_LEVELDB_WRAPPER_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "content/common/leveldb_wrapper.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"

namespace content {

// This is a wrapper around a leveldb::mojom::LevelDBDatabase. Multiple
// interface
// pointers can be bound to the same object. The wrapper adds a couple of
// features not found directly in leveldb.
// 1) Adds the given prefix, if any, to all keys. This allows the sharing of one
//    database across many, possibly untrusted, consumers and ensuring that they
//    can't access each other's values.
// 2) Enforces a max_size constraint.
// 3) Informs observers when values scoped by prefix are modified.
// 4) Throttles requests to avoid overwhelming the disk.
class LevelDBWrapperImpl : public mojom::LevelDBWrapper {
 public:
  // |no_bindings_callback| will be called when this object has no more
  // bindings.
  LevelDBWrapperImpl(leveldb::mojom::LevelDBDatabase* database,
                     const std::string& prefix,
                     size_t max_size,
                     base::TimeDelta default_commit_delay,
                     int max_bytes_per_hour,
                     int max_commits_per_hour,
                     const base::Closure& no_bindings_callback);
  ~LevelDBWrapperImpl() override;

  void Bind(mojom::LevelDBWrapperRequest request);
  void AddObserver(mojom::LevelDBObserverPtr observer);

  // Commence aggressive flushing. This should be called early during startup,
  // before any localStorage writing. Currently scheduled writes will not be
  // rescheduled and will be flushed at the scheduled time after which
  // aggressive flushing will commence.
  static void EnableAggressiveCommitDelay();

 private:
  using ValueMap = std::map<mojo::Array<uint8_t>, mojo::Array<uint8_t>>;

  // Used to rate limit commits.
  class RateLimiter {
   public:
    RateLimiter(size_t desired_rate, base::TimeDelta time_quantum);

    void add_samples(size_t samples) { samples_ += samples;  }

    // Computes the total time needed to process the total samples seen
    // at the desired rate.
    base::TimeDelta ComputeTimeNeeded() const;

    // Given the elapsed time since the start of the rate limiting session,
    // computes the delay needed to mimic having processed the total samples
    // seen at the desired rate.
    base::TimeDelta ComputeDelayNeeded(
        const base::TimeDelta elapsed_time) const;

   private:
    float rate_;
    float samples_;
    base::TimeDelta time_quantum_;
  };

  struct CommitBatch {
    bool clear_all_first;
    ValueMap changed_values;

    CommitBatch();
    ~CommitBatch();
    size_t GetDataSize() const;
  };

  // LevelDBWrapperImpl:
  void Put(mojo::Array<uint8_t> key,
           mojo::Array<uint8_t> value,
           const mojo::String& source,
           const PutCallback& callback) override;
  void Delete(mojo::Array<uint8_t> key,
              const mojo::String& source,
              const DeleteCallback& callback) override;
  void DeleteAll(const mojo::String& source,
                 const DeleteAllCallback& callback) override;
  void Get(mojo::Array<uint8_t> key, const GetCallback& callback) override;
  void GetAll(const mojo::String& source,
              const GetAllCallback& callback) override;

  void OnConnectionError();
  void LoadMap(const base::Closure& completion_callback);
  void OnLoadComplete(leveldb::mojom::DatabaseError status,
                      mojo::Array<leveldb::mojom::KeyValuePtr> data);
  void CreateCommitBatchIfNeeded();
  void StartCommitTimer();
  base::TimeDelta ComputeCommitDelay() const;
  void CommitChanges();
  void OnCommitComplete(leveldb::mojom::DatabaseError error);

  std::string prefix_;
  mojo::BindingSet<mojom::LevelDBWrapper> bindings_;
  mojo::InterfacePtrSet<mojom::LevelDBObserver> observers_;
  base::Closure no_bindings_callback_;
  leveldb::mojom::LevelDBDatabase* database_;
  std::unique_ptr<ValueMap> map_;
  std::vector<base::Closure> on_load_complete_tasks_;
  size_t bytes_used_;
  size_t max_size_;
  base::TimeTicks start_time_;
  base::TimeDelta default_commit_delay_;
  RateLimiter data_rate_limiter_;
  RateLimiter commit_rate_limiter_;
  int commit_batches_in_flight_ = 0;
  std::unique_ptr<CommitBatch> commit_batch_;
  base::WeakPtrFactory<LevelDBWrapperImpl> weak_ptr_factory_;

  static bool s_aggressive_flushing_enabled_;

  DISALLOW_COPY_AND_ASSIGN(LevelDBWrapperImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LEVELDB_WRAPPER_IMPL_H_
