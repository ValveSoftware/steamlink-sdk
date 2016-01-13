// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/discardable_memory_ashmem_allocator.h"

#include <sys/mman.h>
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <utility>

#include "base/basictypes.h"
#include "base/containers/hash_tables.h"
#include "base/file_util.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/memory/scoped_vector.h"
#include "third_party/ashmem/ashmem.h"

// The allocator consists of three parts (classes):
// - DiscardableMemoryAshmemAllocator: entry point of all allocations (through
// its Allocate() method) that are dispatched to the AshmemRegion instances
// (which it owns).
// - AshmemRegion: manages allocations and destructions inside a single large
// (e.g. 32 MBytes) ashmem region.
// - DiscardableAshmemChunk: class mimicking the DiscardableMemory interface
// whose instances are returned to the client.

namespace base {
namespace {

// Only tolerate fragmentation in used chunks *caused by the client* (as opposed
// to the allocator when a free chunk is reused). The client can cause such
// fragmentation by e.g. requesting 4097 bytes. This size would be rounded up to
// 8192 by the allocator which would cause 4095 bytes of fragmentation (which is
// currently the maximum allowed). If the client requests 4096 bytes and a free
// chunk of 8192 bytes is available then the free chunk gets splitted into two
// pieces to minimize fragmentation (since 8192 - 4096 = 4096 which is greater
// than 4095).
// TODO(pliard): tune this if splitting chunks too often leads to performance
// issues.
const size_t kMaxChunkFragmentationBytes = 4096 - 1;

const size_t kMinAshmemRegionSize = 32 * 1024 * 1024;

// Returns 0 if the provided size is too high to be aligned.
size_t AlignToNextPage(size_t size) {
  const size_t kPageSize = 4096;
  DCHECK_EQ(static_cast<int>(kPageSize), getpagesize());
  if (size > std::numeric_limits<size_t>::max() - kPageSize + 1)
    return 0;
  const size_t mask = ~(kPageSize - 1);
  return (size + kPageSize - 1) & mask;
}

bool CreateAshmemRegion(const char* name,
                        size_t size,
                        int* out_fd,
                        void** out_address) {
  base::ScopedFD fd(ashmem_create_region(name, size));
  if (!fd.is_valid()) {
    DLOG(ERROR) << "ashmem_create_region() failed";
    return false;
  }

  const int err = ashmem_set_prot_region(fd.get(), PROT_READ | PROT_WRITE);
  if (err < 0) {
    DLOG(ERROR) << "Error " << err << " when setting protection of ashmem";
    return false;
  }

  // There is a problem using MAP_PRIVATE here. As we are constantly calling
  // Lock() and Unlock(), data could get lost if they are not written to the
  // underlying file when Unlock() gets called.
  void* const address = mmap(
      NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd.get(), 0);
  if (address == MAP_FAILED) {
    DPLOG(ERROR) << "Failed to map memory.";
    return false;
  }

  *out_fd = fd.release();
  *out_address = address;
  return true;
}

bool CloseAshmemRegion(int fd, size_t size, void* address) {
  if (munmap(address, size) == -1) {
    DPLOG(ERROR) << "Failed to unmap memory.";
    close(fd);
    return false;
  }
  return close(fd) == 0;
}

bool LockAshmemRegion(int fd, size_t off, size_t size) {
  return ashmem_pin_region(fd, off, size) != ASHMEM_WAS_PURGED;
}

bool UnlockAshmemRegion(int fd, size_t off, size_t size) {
  const int failed = ashmem_unpin_region(fd, off, size);
  if (failed)
    DLOG(ERROR) << "Failed to unpin memory.";
  return !failed;
}

}  // namespace

namespace internal {

class AshmemRegion {
 public:
  // Note that |allocator| must outlive |this|.
  static scoped_ptr<AshmemRegion> Create(
      size_t size,
      const std::string& name,
      DiscardableMemoryAshmemAllocator* allocator) {
    DCHECK_EQ(size, AlignToNextPage(size));
    int fd;
    void* base;
    if (!CreateAshmemRegion(name.c_str(), size, &fd, &base))
      return scoped_ptr<AshmemRegion>();
    return make_scoped_ptr(new AshmemRegion(fd, size, base, allocator));
  }

  ~AshmemRegion() {
    const bool result = CloseAshmemRegion(fd_, size_, base_);
    DCHECK(result);
    DCHECK(!highest_allocated_chunk_);
  }

  // Returns a new instance of DiscardableAshmemChunk whose size is greater or
  // equal than |actual_size| (which is expected to be greater or equal than
  // |client_requested_size|).
  // Allocation works as follows:
  // 1) Reuse a previously freed chunk and return it if it succeeded. See
  // ReuseFreeChunk_Locked() below for more information.
  // 2) If no free chunk could be reused and the region is not big enough for
  // the requested size then NULL is returned.
  // 3) If there is enough room in the ashmem region then a new chunk is
  // returned. This new chunk starts at |offset_| which is the end of the
  // previously highest chunk in the region.
  scoped_ptr<DiscardableAshmemChunk> Allocate_Locked(
      size_t client_requested_size,
      size_t actual_size) {
    DCHECK_LE(client_requested_size, actual_size);
    allocator_->lock_.AssertAcquired();

    // Check that the |highest_allocated_chunk_| field doesn't contain a stale
    // pointer. It should point to either a free chunk or a used chunk.
    DCHECK(!highest_allocated_chunk_ ||
           address_to_free_chunk_map_.find(highest_allocated_chunk_) !=
               address_to_free_chunk_map_.end() ||
           used_to_previous_chunk_map_.find(highest_allocated_chunk_) !=
               used_to_previous_chunk_map_.end());

    scoped_ptr<DiscardableAshmemChunk> memory = ReuseFreeChunk_Locked(
        client_requested_size, actual_size);
    if (memory)
      return memory.Pass();

    if (size_ - offset_ < actual_size) {
      // This region does not have enough space left to hold the requested size.
      return scoped_ptr<DiscardableAshmemChunk>();
    }

    void* const address = static_cast<char*>(base_) + offset_;
    memory.reset(
        new DiscardableAshmemChunk(this, fd_, address, offset_, actual_size));

    used_to_previous_chunk_map_.insert(
        std::make_pair(address, highest_allocated_chunk_));
    highest_allocated_chunk_ = address;
    offset_ += actual_size;
    DCHECK_LE(offset_, size_);
    return memory.Pass();
  }

  void OnChunkDeletion(void* chunk, size_t size) {
    AutoLock auto_lock(allocator_->lock_);
    MergeAndAddFreeChunk_Locked(chunk, size);
    // Note that |this| might be deleted beyond this point.
  }

 private:
  struct FreeChunk {
    FreeChunk() : previous_chunk(NULL), start(NULL), size(0) {}

    explicit FreeChunk(size_t size)
        : previous_chunk(NULL),
          start(NULL),
          size(size) {
    }

    FreeChunk(void* previous_chunk, void* start, size_t size)
        : previous_chunk(previous_chunk),
          start(start),
          size(size) {
      DCHECK_LT(previous_chunk, start);
    }

    void* const previous_chunk;
    void* const start;
    const size_t size;

    bool is_null() const { return !start; }

    bool operator<(const FreeChunk& other) const {
      return size < other.size;
    }
  };

  // Note that |allocator| must outlive |this|.
  AshmemRegion(int fd,
               size_t size,
               void* base,
               DiscardableMemoryAshmemAllocator* allocator)
      : fd_(fd),
        size_(size),
        base_(base),
        allocator_(allocator),
        highest_allocated_chunk_(NULL),
        offset_(0) {
    DCHECK_GE(fd_, 0);
    DCHECK_GE(size, kMinAshmemRegionSize);
    DCHECK(base);
    DCHECK(allocator);
  }

  // Tries to reuse a previously freed chunk by doing a closest size match.
  scoped_ptr<DiscardableAshmemChunk> ReuseFreeChunk_Locked(
      size_t client_requested_size,
      size_t actual_size) {
    allocator_->lock_.AssertAcquired();
    const FreeChunk reused_chunk = RemoveFreeChunkFromIterator_Locked(
        free_chunks_.lower_bound(FreeChunk(actual_size)));
    if (reused_chunk.is_null())
      return scoped_ptr<DiscardableAshmemChunk>();

    used_to_previous_chunk_map_.insert(
        std::make_pair(reused_chunk.start, reused_chunk.previous_chunk));
    size_t reused_chunk_size = reused_chunk.size;
    // |client_requested_size| is used below rather than |actual_size| to
    // reflect the amount of bytes that would not be usable by the client (i.e.
    // wasted). Using |actual_size| instead would not allow us to detect
    // fragmentation caused by the client if he did misaligned allocations.
    DCHECK_GE(reused_chunk.size, client_requested_size);
    const size_t fragmentation_bytes =
        reused_chunk.size - client_requested_size;

    if (fragmentation_bytes > kMaxChunkFragmentationBytes) {
      // Split the free chunk being recycled so that its unused tail doesn't get
      // reused (i.e. locked) which would prevent it from being evicted under
      // memory pressure.
      reused_chunk_size = actual_size;
      void* const new_chunk_start =
          static_cast<char*>(reused_chunk.start) + actual_size;
      if (reused_chunk.start == highest_allocated_chunk_) {
        // We also need to update the pointer to the highest allocated chunk in
        // case we are splitting the highest chunk.
        highest_allocated_chunk_ = new_chunk_start;
      }
      DCHECK_GT(reused_chunk.size, actual_size);
      const size_t new_chunk_size = reused_chunk.size - actual_size;
      // Note that merging is not needed here since there can't be contiguous
      // free chunks at this point.
      AddFreeChunk_Locked(
          FreeChunk(reused_chunk.start, new_chunk_start, new_chunk_size));
    }

    const size_t offset =
        static_cast<char*>(reused_chunk.start) - static_cast<char*>(base_);
    LockAshmemRegion(fd_, offset, reused_chunk_size);
    scoped_ptr<DiscardableAshmemChunk> memory(
        new DiscardableAshmemChunk(
            this, fd_, reused_chunk.start, offset, reused_chunk_size));
    return memory.Pass();
  }

  // Makes the chunk identified with the provided arguments free and possibly
  // merges this chunk with the previous and next contiguous ones.
  // If the provided chunk is the only one used (and going to be freed) in the
  // region then the internal ashmem region is closed so that the underlying
  // physical pages are immediately released.
  // Note that free chunks are unlocked therefore they can be reclaimed by the
  // kernel if needed (under memory pressure) but they are not immediately
  // released unfortunately since madvise(MADV_REMOVE) and
  // fallocate(FALLOC_FL_PUNCH_HOLE) don't seem to work on ashmem. This might
  // change in versions of kernel >=3.5 though. The fact that free chunks are
  // not immediately released is the reason why we are trying to minimize
  // fragmentation in order not to cause "artificial" memory pressure.
  void MergeAndAddFreeChunk_Locked(void* chunk, size_t size) {
    allocator_->lock_.AssertAcquired();
    size_t new_free_chunk_size = size;
    // Merge with the previous chunk.
    void* first_free_chunk = chunk;
    DCHECK(!used_to_previous_chunk_map_.empty());
    const hash_map<void*, void*>::iterator previous_chunk_it =
        used_to_previous_chunk_map_.find(chunk);
    DCHECK(previous_chunk_it != used_to_previous_chunk_map_.end());
    void* previous_chunk = previous_chunk_it->second;
    used_to_previous_chunk_map_.erase(previous_chunk_it);

    if (previous_chunk) {
      const FreeChunk free_chunk = RemoveFreeChunk_Locked(previous_chunk);
      if (!free_chunk.is_null()) {
        new_free_chunk_size += free_chunk.size;
        first_free_chunk = previous_chunk;
        if (chunk == highest_allocated_chunk_)
          highest_allocated_chunk_ = previous_chunk;

        // There should not be more contiguous previous free chunks.
        previous_chunk = free_chunk.previous_chunk;
        DCHECK(!address_to_free_chunk_map_.count(previous_chunk));
      }
    }

    // Merge with the next chunk if free and present.
    void* next_chunk = static_cast<char*>(chunk) + size;
    const FreeChunk next_free_chunk = RemoveFreeChunk_Locked(next_chunk);
    if (!next_free_chunk.is_null()) {
      new_free_chunk_size += next_free_chunk.size;
      if (next_free_chunk.start == highest_allocated_chunk_)
        highest_allocated_chunk_ = first_free_chunk;

      // Same as above.
      DCHECK(!address_to_free_chunk_map_.count(static_cast<char*>(next_chunk) +
                                               next_free_chunk.size));
    }

    const bool whole_ashmem_region_is_free =
        used_to_previous_chunk_map_.empty();
    if (!whole_ashmem_region_is_free) {
      AddFreeChunk_Locked(
          FreeChunk(previous_chunk, first_free_chunk, new_free_chunk_size));
      return;
    }

    // The whole ashmem region is free thus it can be deleted.
    DCHECK_EQ(base_, first_free_chunk);
    DCHECK_EQ(base_, highest_allocated_chunk_);
    DCHECK(free_chunks_.empty());
    DCHECK(address_to_free_chunk_map_.empty());
    DCHECK(used_to_previous_chunk_map_.empty());
    highest_allocated_chunk_ = NULL;
    allocator_->DeleteAshmemRegion_Locked(this);  // Deletes |this|.
  }

  void AddFreeChunk_Locked(const FreeChunk& free_chunk) {
    allocator_->lock_.AssertAcquired();
    const std::multiset<FreeChunk>::iterator it = free_chunks_.insert(
        free_chunk);
    address_to_free_chunk_map_.insert(std::make_pair(free_chunk.start, it));
    // Update the next used contiguous chunk, if any, since its previous chunk
    // may have changed due to free chunks merging/splitting.
    void* const next_used_contiguous_chunk =
        static_cast<char*>(free_chunk.start) + free_chunk.size;
    hash_map<void*, void*>::iterator previous_it =
        used_to_previous_chunk_map_.find(next_used_contiguous_chunk);
    if (previous_it != used_to_previous_chunk_map_.end())
      previous_it->second = free_chunk.start;
  }

  // Finds and removes the free chunk, if any, whose start address is
  // |chunk_start|. Returns a copy of the unlinked free chunk or a free chunk
  // whose content is null if it was not found.
  FreeChunk RemoveFreeChunk_Locked(void* chunk_start) {
    allocator_->lock_.AssertAcquired();
    const hash_map<
        void*, std::multiset<FreeChunk>::iterator>::iterator it =
            address_to_free_chunk_map_.find(chunk_start);
    if (it == address_to_free_chunk_map_.end())
      return FreeChunk();
    return RemoveFreeChunkFromIterator_Locked(it->second);
  }

  // Same as above but takes an iterator in.
  FreeChunk RemoveFreeChunkFromIterator_Locked(
      std::multiset<FreeChunk>::iterator free_chunk_it) {
    allocator_->lock_.AssertAcquired();
    if (free_chunk_it == free_chunks_.end())
      return FreeChunk();
    DCHECK(free_chunk_it != free_chunks_.end());
    const FreeChunk free_chunk(*free_chunk_it);
    address_to_free_chunk_map_.erase(free_chunk_it->start);
    free_chunks_.erase(free_chunk_it);
    return free_chunk;
  }

  const int fd_;
  const size_t size_;
  void* const base_;
  DiscardableMemoryAshmemAllocator* const allocator_;
  // Points to the chunk with the highest address in the region. This pointer
  // needs to be carefully updated when chunks are merged/split.
  void* highest_allocated_chunk_;
  // Points to the end of |highest_allocated_chunk_|.
  size_t offset_;
  // Allows free chunks recycling (lookup, insertion and removal) in O(log N).
  // Note that FreeChunk values are indexed by their size and also note that
  // multiple free chunks can have the same size (which is why multiset<> is
  // used instead of e.g. set<>).
  std::multiset<FreeChunk> free_chunks_;
  // Used while merging free contiguous chunks to erase free chunks (from their
  // start address) in constant time. Note that multiset<>::{insert,erase}()
  // don't invalidate iterators (except the one for the element being removed
  // obviously).
  hash_map<
      void*, std::multiset<FreeChunk>::iterator> address_to_free_chunk_map_;
  // Maps the address of *used* chunks to the address of their previous
  // contiguous chunk.
  hash_map<void*, void*> used_to_previous_chunk_map_;

  DISALLOW_COPY_AND_ASSIGN(AshmemRegion);
};

DiscardableAshmemChunk::~DiscardableAshmemChunk() {
  if (locked_)
    UnlockAshmemRegion(fd_, offset_, size_);
  ashmem_region_->OnChunkDeletion(address_, size_);
}

bool DiscardableAshmemChunk::Lock() {
  DCHECK(!locked_);
  locked_ = true;
  return LockAshmemRegion(fd_, offset_, size_);
}

void DiscardableAshmemChunk::Unlock() {
  DCHECK(locked_);
  locked_ = false;
  UnlockAshmemRegion(fd_, offset_, size_);
}

void* DiscardableAshmemChunk::Memory() const {
  return address_;
}

// Note that |ashmem_region| must outlive |this|.
DiscardableAshmemChunk::DiscardableAshmemChunk(AshmemRegion* ashmem_region,
                                               int fd,
                                               void* address,
                                               size_t offset,
                                               size_t size)
    : ashmem_region_(ashmem_region),
      fd_(fd),
      address_(address),
      offset_(offset),
      size_(size),
      locked_(true) {
}

DiscardableMemoryAshmemAllocator::DiscardableMemoryAshmemAllocator(
    const std::string& name,
    size_t ashmem_region_size)
    : name_(name),
      ashmem_region_size_(
          std::max(kMinAshmemRegionSize, AlignToNextPage(ashmem_region_size))),
      last_ashmem_region_size_(0) {
  DCHECK_GE(ashmem_region_size_, kMinAshmemRegionSize);
}

DiscardableMemoryAshmemAllocator::~DiscardableMemoryAshmemAllocator() {
  DCHECK(ashmem_regions_.empty());
}

scoped_ptr<DiscardableAshmemChunk> DiscardableMemoryAshmemAllocator::Allocate(
    size_t size) {
  const size_t aligned_size = AlignToNextPage(size);
  if (!aligned_size)
    return scoped_ptr<DiscardableAshmemChunk>();
  // TODO(pliard): make this function less naive by e.g. moving the free chunks
  // multiset to the allocator itself in order to decrease even more
  // fragmentation/speedup allocation. Note that there should not be more than a
  // couple (=5) of AshmemRegion instances in practice though.
  AutoLock auto_lock(lock_);
  DCHECK_LE(ashmem_regions_.size(), 5U);
  for (ScopedVector<AshmemRegion>::iterator it = ashmem_regions_.begin();
       it != ashmem_regions_.end(); ++it) {
    scoped_ptr<DiscardableAshmemChunk> memory(
        (*it)->Allocate_Locked(size, aligned_size));
    if (memory)
      return memory.Pass();
  }
  // The creation of the (large) ashmem region might fail if the address space
  // is too fragmented. In case creation fails the allocator retries by
  // repetitively dividing the size by 2.
  const size_t min_region_size = std::max(kMinAshmemRegionSize, aligned_size);
  for (size_t region_size = std::max(ashmem_region_size_, aligned_size);
       region_size >= min_region_size;
       region_size = AlignToNextPage(region_size / 2)) {
    scoped_ptr<AshmemRegion> new_region(
        AshmemRegion::Create(region_size, name_.c_str(), this));
    if (!new_region)
      continue;
    last_ashmem_region_size_ = region_size;
    ashmem_regions_.push_back(new_region.release());
    return ashmem_regions_.back()->Allocate_Locked(size, aligned_size);
  }
  // TODO(pliard): consider adding an histogram to see how often this happens.
  return scoped_ptr<DiscardableAshmemChunk>();
}

size_t DiscardableMemoryAshmemAllocator::last_ashmem_region_size() const {
  AutoLock auto_lock(lock_);
  return last_ashmem_region_size_;
}

void DiscardableMemoryAshmemAllocator::DeleteAshmemRegion_Locked(
    AshmemRegion* region) {
  lock_.AssertAcquired();
  // Note that there should not be more than a couple of ashmem region instances
  // in |ashmem_regions_|.
  DCHECK_LE(ashmem_regions_.size(), 5U);
  const ScopedVector<AshmemRegion>::iterator it = std::find(
      ashmem_regions_.begin(), ashmem_regions_.end(), region);
  DCHECK_NE(ashmem_regions_.end(), it);
  std::swap(*it, ashmem_regions_.back());
  ashmem_regions_.pop_back();
}

}  // namespace internal
}  // namespace base
