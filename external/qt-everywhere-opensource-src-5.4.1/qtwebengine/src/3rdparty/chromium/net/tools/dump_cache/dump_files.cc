// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Performs basic inspection of the disk cache files with minimal disruption
// to the actual files (they still may change if an error is detected on the
// files).

#include "net/tools/dump_cache/dump_files.h"

#include <stdio.h>

#include <set>
#include <string>

#include "base/file_util.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/format_macros.h"
#include "base/message_loop/message_loop.h"
#include "net/disk_cache/blockfile/block_files.h"
#include "net/disk_cache/blockfile/disk_format.h"
#include "net/disk_cache/blockfile/mapped_file.h"
#include "net/disk_cache/blockfile/stats.h"
#include "net/disk_cache/blockfile/storage_block-inl.h"
#include "net/disk_cache/blockfile/storage_block.h"

namespace {

const base::FilePath::CharType kIndexName[] = FILE_PATH_LITERAL("index");

// Reads the |header_size| bytes from the beginning of file |name|.
bool ReadHeader(const base::FilePath& name, char* header, int header_size) {
  base::File file(name, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    printf("Unable to open file %s\n", name.MaybeAsASCII().c_str());
    return false;
  }

  int read = file.Read(0, header, header_size);
  if (read != header_size) {
    printf("Unable to read file %s\n", name.MaybeAsASCII().c_str());
    return false;
  }
  return true;
}

int GetMajorVersionFromFile(const base::FilePath& name) {
  disk_cache::IndexHeader header;
  if (!ReadHeader(name, reinterpret_cast<char*>(&header), sizeof(header)))
    return 0;

  return header.version >> 16;
}

// Dumps the contents of the Stats record.
void DumpStats(const base::FilePath& path, disk_cache::CacheAddr addr) {
  // We need a message loop, although we really don't run any task.
  base::MessageLoopForIO loop;

  disk_cache::BlockFiles block_files(path);
  if (!block_files.Init(false)) {
    printf("Unable to init block files\n");
    return;
  }

  disk_cache::Addr address(addr);
  disk_cache::MappedFile* file = block_files.GetFile(address);
  if (!file)
    return;

  size_t length = (2 + disk_cache::Stats::kDataSizesLength) * sizeof(int32) +
                  disk_cache::Stats::MAX_COUNTER * sizeof(int64);

  size_t offset = address.start_block() * address.BlockSize() +
                  disk_cache::kBlockHeaderSize;

  scoped_ptr<int32[]> buffer(new int32[length]);
  if (!file->Read(buffer.get(), length, offset))
    return;

  printf("Stats:\nSignatrure: 0x%x\n", buffer[0]);
  printf("Total size: %d\n", buffer[1]);
  for (int i = 0; i < disk_cache::Stats::kDataSizesLength; i++)
    printf("Size(%d): %d\n", i, buffer[i + 2]);

  int64* counters = reinterpret_cast<int64*>(
                        buffer.get() + 2 + disk_cache::Stats::kDataSizesLength);
  for (int i = 0; i < disk_cache::Stats::MAX_COUNTER; i++)
    printf("Count(%d): %" PRId64 "\n", i, *counters++);
  printf("-------------------------\n\n");
}

// Dumps the contents of the Index-file header.
void DumpIndexHeader(const base::FilePath& name,
                     disk_cache::CacheAddr* stats_addr) {
  disk_cache::IndexHeader header;
  if (!ReadHeader(name, reinterpret_cast<char*>(&header), sizeof(header)))
    return;

  printf("Index file:\n");
  printf("magic: %x\n", header.magic);
  printf("version: %d.%d\n", header.version >> 16, header.version & 0xffff);
  printf("entries: %d\n", header.num_entries);
  printf("total bytes: %d\n", header.num_bytes);
  printf("last file number: %d\n", header.last_file);
  printf("current id: %d\n", header.this_id);
  printf("table length: %d\n", header.table_len);
  printf("last crash: %d\n", header.crash);
  printf("experiment: %d\n", header.experiment);
  printf("stats: %x\n", header.stats);
  for (int i = 0; i < 5; i++) {
    printf("head %d: 0x%x\n", i, header.lru.heads[i]);
    printf("tail %d: 0x%x\n", i, header.lru.tails[i]);
    printf("size %d: 0x%x\n", i, header.lru.sizes[i]);
  }
  printf("transaction: 0x%x\n", header.lru.transaction);
  printf("operation: %d\n", header.lru.operation);
  printf("operation list: %d\n", header.lru.operation_list);
  printf("-------------------------\n\n");

  *stats_addr = header.stats;
}

// Dumps the contents of a block-file header.
void DumpBlockHeader(const base::FilePath& name) {
  disk_cache::BlockFileHeader header;
  if (!ReadHeader(name, reinterpret_cast<char*>(&header), sizeof(header)))
    return;

  printf("Block file: %s\n", name.BaseName().MaybeAsASCII().c_str());
  printf("magic: %x\n", header.magic);
  printf("version: %d.%d\n", header.version >> 16, header.version & 0xffff);
  printf("file id: %d\n", header.this_file);
  printf("next file id: %d\n", header.next_file);
  printf("entry size: %d\n", header.entry_size);
  printf("current entries: %d\n", header.num_entries);
  printf("max entries: %d\n", header.max_entries);
  printf("updating: %d\n", header.updating);
  printf("empty sz 1: %d\n", header.empty[0]);
  printf("empty sz 2: %d\n", header.empty[1]);
  printf("empty sz 3: %d\n", header.empty[2]);
  printf("empty sz 4: %d\n", header.empty[3]);
  printf("user 0: 0x%x\n", header.user[0]);
  printf("user 1: 0x%x\n", header.user[1]);
  printf("user 2: 0x%x\n", header.user[2]);
  printf("user 3: 0x%x\n", header.user[3]);
  printf("-------------------------\n\n");
}

// Simple class that interacts with the set of cache files.
class CacheDumper {
 public:
  explicit CacheDumper(const base::FilePath& path)
      : path_(path),
        block_files_(path),
        index_(NULL),
        current_hash_(0),
        next_addr_(0) {
  }

  bool Init();

  // Reads an entry from disk. Return false when all entries have been already
  // returned.
  bool GetEntry(disk_cache::EntryStore* entry);

  // Loads a specific block from the block files.
  bool LoadEntry(disk_cache::CacheAddr addr, disk_cache::EntryStore* entry);
  bool LoadRankings(disk_cache::CacheAddr addr,
                    disk_cache::RankingsNode* rankings);

 private:
  base::FilePath path_;
  disk_cache::BlockFiles block_files_;
  scoped_refptr<disk_cache::MappedFile> index_file_;
  disk_cache::Index* index_;
  int current_hash_;
  disk_cache::CacheAddr next_addr_;
  std::set<disk_cache::CacheAddr> dumped_entries_;
  DISALLOW_COPY_AND_ASSIGN(CacheDumper);
};

bool CacheDumper::Init() {
  if (!block_files_.Init(false)) {
    printf("Unable to init block files\n");
    return false;
  }

  base::FilePath index_name(path_.Append(kIndexName));
  index_file_ = new disk_cache::MappedFile;
  index_ = reinterpret_cast<disk_cache::Index*>(
      index_file_->Init(index_name, 0));
  if (!index_) {
    printf("Unable to map index\n");
    return false;
  }

  return true;
}

bool CacheDumper::GetEntry(disk_cache::EntryStore* entry) {
  if (dumped_entries_.find(next_addr_) != dumped_entries_.end()) {
    printf("Loop detected\n");
    next_addr_ = 0;
    current_hash_++;
  }

  if (next_addr_) {
    if (LoadEntry(next_addr_, entry))
      return true;

    printf("Unable to load entry at address 0x%x\n", next_addr_);
    next_addr_ = 0;
    current_hash_++;
  }

  for (int i = current_hash_; i < index_->header.table_len; i++) {
    // Yes, we'll crash if the table is shorter than expected, but only after
    // dumping every entry that we can find.
    if (index_->table[i]) {
      current_hash_ = i;
      if (LoadEntry(index_->table[i], entry))
        return true;

      printf("Unable to load entry at address 0x%x\n", index_->table[i]);
    }
  }
  return false;
}

bool CacheDumper::LoadEntry(disk_cache::CacheAddr addr,
                            disk_cache::EntryStore* entry) {
  disk_cache::Addr address(addr);
  disk_cache::MappedFile* file = block_files_.GetFile(address);
  if (!file)
    return false;

  disk_cache::StorageBlock<disk_cache::EntryStore> entry_block(file, address);
  if (!entry_block.Load())
    return false;

  memcpy(entry, entry_block.Data(), sizeof(*entry));
  printf("Entry at 0x%x\n", addr);

  // Prepare for the next entry to load.
  next_addr_ = entry->next;
  if (next_addr_) {
    dumped_entries_.insert(addr);
  } else {
    current_hash_++;
    dumped_entries_.clear();
  }
  return true;
}

bool CacheDumper::LoadRankings(disk_cache::CacheAddr addr,
                               disk_cache::RankingsNode* rankings) {
  disk_cache::Addr address(addr);
  disk_cache::MappedFile* file = block_files_.GetFile(address);
  if (!file)
    return false;

  disk_cache::StorageBlock<disk_cache::RankingsNode> rank_block(file, address);
  if (!rank_block.Load())
    return false;

  memcpy(rankings, rank_block.Data(), sizeof(*rankings));
  printf("Rankings at 0x%x\n", addr);
  return true;
}

void DumpEntry(const disk_cache::EntryStore& entry) {
  std::string key;
  if (!entry.long_key) {
    key = entry.key;
    if (key.size() > 50)
      key.resize(50);
  }

  printf("hash: 0x%x\n", entry.hash);
  printf("next entry: 0x%x\n", entry.next);
  printf("rankings: 0x%x\n", entry.rankings_node);
  printf("key length: %d\n", entry.key_len);
  printf("key: \"%s\"\n", key.c_str());
  printf("key addr: 0x%x\n", entry.long_key);
  printf("reuse count: %d\n", entry.reuse_count);
  printf("refetch count: %d\n", entry.refetch_count);
  printf("state: %d\n", entry.state);
  for (int i = 0; i < 4; i++) {
    printf("data size %d: %d\n", i, entry.data_size[i]);
    printf("data addr %d: 0x%x\n", i, entry.data_addr[i]);
  }
  printf("----------\n\n");
}

void DumpRankings(const disk_cache::RankingsNode& rankings) {
  printf("next: 0x%x\n", rankings.next);
  printf("prev: 0x%x\n", rankings.prev);
  printf("entry: 0x%x\n", rankings.contents);
  printf("dirty: %d\n", rankings.dirty);
  printf("hash: 0x%x\n", rankings.self_hash);
  printf("----------\n\n");
}

}  // namespace.

// -----------------------------------------------------------------------

int GetMajorVersion(const base::FilePath& input_path) {
  base::FilePath index_name(input_path.Append(kIndexName));

  int version = GetMajorVersionFromFile(index_name);
  if (!version)
    return 0;

  base::FilePath data_name(input_path.Append(FILE_PATH_LITERAL("data_0")));
  if (version != GetMajorVersionFromFile(data_name))
    return 0;

  data_name = input_path.Append(FILE_PATH_LITERAL("data_1"));
  if (version != GetMajorVersionFromFile(data_name))
    return 0;

  data_name = input_path.Append(FILE_PATH_LITERAL("data_2"));
  if (version != GetMajorVersionFromFile(data_name))
    return 0;

  data_name = input_path.Append(FILE_PATH_LITERAL("data_3"));
  if (version != GetMajorVersionFromFile(data_name))
    return 0;

  return version;
}

// Dumps the headers of all files.
int DumpHeaders(const base::FilePath& input_path) {
  base::FilePath index_name(input_path.Append(kIndexName));
  disk_cache::CacheAddr stats_addr = 0;
  DumpIndexHeader(index_name, &stats_addr);

  base::FileEnumerator iter(input_path, false,
                            base::FileEnumerator::FILES,
                            FILE_PATH_LITERAL("data_*"));
  for (base::FilePath file = iter.Next(); !file.empty(); file = iter.Next())
    DumpBlockHeader(file);

  DumpStats(input_path, stats_addr);
  return 0;
}

// Dumps all entries from the cache.
int DumpContents(const base::FilePath& input_path) {
  DumpHeaders(input_path);

  // We need a message loop, although we really don't run any task.
  base::MessageLoopForIO loop;
  CacheDumper dumper(input_path);
  if (!dumper.Init())
    return -1;

  disk_cache::EntryStore entry;
  while (dumper.GetEntry(&entry)) {
    DumpEntry(entry);
    disk_cache::RankingsNode rankings;
    if (dumper.LoadRankings(entry.rankings_node, &rankings))
      DumpRankings(rankings);
  }

  printf("Done.\n");

  return 0;
}
