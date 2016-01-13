// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/hpack_header_table.h"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/macros.h"
#include "net/spdy/hpack_constants.h"
#include "net/spdy/hpack_entry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

using base::StringPiece;
using std::distance;
using std::string;

namespace test {

class HpackHeaderTablePeer {
 public:
  explicit HpackHeaderTablePeer(HpackHeaderTable* table)
      : table_(table) {}

  const HpackHeaderTable::EntryTable& dynamic_entries() {
    return table_->dynamic_entries_;
  }
  const HpackHeaderTable::EntryTable& static_entries() {
    return table_->static_entries_;
  }
  const HpackHeaderTable::OrderedEntrySet& index() {
    return table_->index_;
  }
  std::vector<HpackEntry*> EvictionSet(StringPiece name, StringPiece value) {
    HpackHeaderTable::EntryTable::iterator begin, end;
    table_->EvictionSet(name, value, &begin, &end);
    std::vector<HpackEntry*> result;
    for (; begin != end; ++begin) {
      result.push_back(&(*begin));
    }
    return result;
  }
  size_t total_insertions() {
    return table_->total_insertions_;
  }
  size_t dynamic_entries_count() {
    return table_->dynamic_entries_.size();
  }
  size_t EvictionCountForEntry(StringPiece name, StringPiece value) {
    return table_->EvictionCountForEntry(name, value);
  }
  size_t EvictionCountToReclaim(size_t reclaim_size) {
    return table_->EvictionCountToReclaim(reclaim_size);
  }
  void Evict(size_t count) {
    return table_->Evict(count);
  }

  void AddStaticEntry(StringPiece name, StringPiece value) {
    table_->static_entries_.push_back(
        HpackEntry(name, value, true, table_->total_insertions_++));
  }

  void AddDynamicEntry(StringPiece name, StringPiece value) {
    table_->dynamic_entries_.push_back(
        HpackEntry(name, value, false, table_->total_insertions_++));
  }

 private:
  HpackHeaderTable* table_;
};

}  // namespace test

namespace {

class HpackHeaderTableTest : public ::testing::Test {
 protected:
  typedef std::vector<HpackEntry> HpackEntryVector;

  HpackHeaderTableTest()
      : table_(),
        peer_(&table_),
        name_("header-name"),
        value_("header value") {}

  // Returns an entry whose Size() is equal to the given one.
  static HpackEntry MakeEntryOfSize(uint32 size) {
    EXPECT_GE(size, HpackEntry::kSizeOverhead);
    string name((size - HpackEntry::kSizeOverhead) / 2, 'n');
    string value(size - HpackEntry::kSizeOverhead - name.size(), 'v');
    HpackEntry entry(name, value);
    EXPECT_EQ(size, entry.Size());
    return entry;
  }

  // Returns a vector of entries whose total size is equal to the given
  // one.
  static HpackEntryVector MakeEntriesOfTotalSize(uint32 total_size) {
    EXPECT_GE(total_size, HpackEntry::kSizeOverhead);
    uint32 entry_size = HpackEntry::kSizeOverhead;
    uint32 remaining_size = total_size;
    HpackEntryVector entries;
    while (remaining_size > 0) {
      EXPECT_LE(entry_size, remaining_size);
      entries.push_back(MakeEntryOfSize(entry_size));
      remaining_size -= entry_size;
      entry_size = std::min(remaining_size, entry_size + 32);
    }
    return entries;
  }

  // Adds the given vector of entries to the given header table,
  // expecting no eviction to happen.
  void AddEntriesExpectNoEviction(const HpackEntryVector& entries) {
    for (HpackEntryVector::const_iterator it = entries.begin();
         it != entries.end(); ++it) {
      HpackHeaderTable::EntryTable::iterator begin, end;

      table_.EvictionSet(it->name(), it->value(), &begin, &end);
      EXPECT_EQ(0, distance(begin, end));

      HpackEntry* entry = table_.TryAddEntry(it->name(), it->value());
      EXPECT_NE(entry, static_cast<HpackEntry*>(NULL));
    }

    for (size_t i = 0; i != entries.size(); ++i) {
      size_t index = entries.size() - i;
      HpackEntry* entry = table_.GetByIndex(index);
      EXPECT_EQ(entries[i].name(), entry->name());
      EXPECT_EQ(entries[i].value(), entry->value());
      EXPECT_EQ(index, table_.IndexOf(entry));
    }
  }

  HpackEntry StaticEntry() {
    peer_.AddStaticEntry(name_, value_);
    return peer_.static_entries().back();
  }
  HpackEntry DynamicEntry() {
    peer_.AddDynamicEntry(name_, value_);
    return peer_.dynamic_entries().back();
  }

  HpackHeaderTable table_;
  test::HpackHeaderTablePeer peer_;
  string name_, value_;
};

TEST_F(HpackHeaderTableTest, StaticTableInitialization) {
  EXPECT_EQ(0u, table_.size());
  EXPECT_EQ(kDefaultHeaderTableSizeSetting, table_.max_size());
  EXPECT_EQ(kDefaultHeaderTableSizeSetting, table_.settings_size_bound());

  EXPECT_EQ(0u, peer_.dynamic_entries_count());
  EXPECT_EQ(0u, table_.reference_set().size());
  EXPECT_EQ(peer_.static_entries().size(), peer_.total_insertions());

  // Static entries have been populated and inserted into the table & index.
  EXPECT_NE(0u, peer_.static_entries().size());
  EXPECT_EQ(peer_.index().size(), peer_.static_entries().size());
  for (size_t i = 0; i != peer_.static_entries().size(); ++i) {
    const HpackEntry* entry = &peer_.static_entries()[i];

    EXPECT_TRUE(entry->IsStatic());
    EXPECT_EQ(entry, table_.GetByIndex(i + 1));
    EXPECT_EQ(entry, table_.GetByNameAndValue(entry->name(), entry->value()));
  }
}

TEST_F(HpackHeaderTableTest, BasicDynamicEntryInsertionAndEviction) {
  size_t static_count = peer_.total_insertions();
  HpackEntry* first_static_entry = table_.GetByIndex(1);

  EXPECT_EQ(1u, table_.IndexOf(first_static_entry));

  HpackEntry* entry = table_.TryAddEntry("header-key", "Header Value");
  EXPECT_EQ("header-key", entry->name());
  EXPECT_EQ("Header Value", entry->value());
  EXPECT_FALSE(entry->IsStatic());

  // Table counts were updated appropriately.
  EXPECT_EQ(entry->Size(), table_.size());
  EXPECT_EQ(1u, peer_.dynamic_entries_count());
  EXPECT_EQ(peer_.dynamic_entries().size(), peer_.dynamic_entries_count());
  EXPECT_EQ(static_count + 1, peer_.total_insertions());
  EXPECT_EQ(static_count + 1, peer_.index().size());

  // Index() of entries reflects the insertion.
  EXPECT_EQ(1u, table_.IndexOf(entry));
  EXPECT_EQ(2u, table_.IndexOf(first_static_entry));
  EXPECT_EQ(entry, table_.GetByIndex(1));
  EXPECT_EQ(first_static_entry, table_.GetByIndex(2));

  // Evict |entry|. Table counts are again updated appropriately.
  peer_.Evict(1);
  EXPECT_EQ(0u, table_.size());
  EXPECT_EQ(0u, peer_.dynamic_entries_count());
  EXPECT_EQ(peer_.dynamic_entries().size(), peer_.dynamic_entries_count());
  EXPECT_EQ(static_count + 1, peer_.total_insertions());
  EXPECT_EQ(static_count, peer_.index().size());

  // Index() of |first_static_entry| reflects the eviction.
  EXPECT_EQ(1u, table_.IndexOf(first_static_entry));
  EXPECT_EQ(first_static_entry, table_.GetByIndex(1));
}

TEST_F(HpackHeaderTableTest, EntryIndexing) {
  HpackEntry* first_static_entry = table_.GetByIndex(1);

  // Static entries are queryable by name & value.
  EXPECT_EQ(first_static_entry, table_.GetByName(first_static_entry->name()));
  EXPECT_EQ(first_static_entry, table_.GetByNameAndValue(
        first_static_entry->name(), first_static_entry->value()));

  // Create a mix of entries which duplicate names, and names & values of both
  // dynamic and static entries.
  HpackEntry* entry1 = table_.TryAddEntry(first_static_entry->name(),
                                          first_static_entry->value());
  HpackEntry* entry2 = table_.TryAddEntry(first_static_entry->name(),
                                          "Value Four");
  HpackEntry* entry3 = table_.TryAddEntry("key-1", "Value One");
  HpackEntry* entry4 = table_.TryAddEntry("key-2", "Value Three");
  HpackEntry* entry5 = table_.TryAddEntry("key-1", "Value Two");
  HpackEntry* entry6 = table_.TryAddEntry("key-2", "Value Three");
  HpackEntry* entry7 = table_.TryAddEntry("key-2", "Value Four");

  // Entries are queryable under their current index.
  EXPECT_EQ(entry7, table_.GetByIndex(1));
  EXPECT_EQ(entry6, table_.GetByIndex(2));
  EXPECT_EQ(entry5, table_.GetByIndex(3));
  EXPECT_EQ(entry4, table_.GetByIndex(4));
  EXPECT_EQ(entry3, table_.GetByIndex(5));
  EXPECT_EQ(entry2, table_.GetByIndex(6));
  EXPECT_EQ(entry1, table_.GetByIndex(7));
  EXPECT_EQ(first_static_entry, table_.GetByIndex(8));

  // Querying by name returns the lowest-value matching entry.
  EXPECT_EQ(entry3, table_.GetByName("key-1"));
  EXPECT_EQ(entry7, table_.GetByName("key-2"));
  EXPECT_EQ(entry2->name(),
            table_.GetByName(first_static_entry->name())->name());
  EXPECT_EQ(NULL, table_.GetByName("not-present"));

  // Querying by name & value returns the lowest-index matching entry.
  EXPECT_EQ(entry3, table_.GetByNameAndValue("key-1", "Value One"));
  EXPECT_EQ(entry5, table_.GetByNameAndValue("key-1", "Value Two"));
  EXPECT_EQ(entry6, table_.GetByNameAndValue("key-2", "Value Three"));
  EXPECT_EQ(entry7, table_.GetByNameAndValue("key-2", "Value Four"));
  EXPECT_EQ(entry1, table_.GetByNameAndValue(first_static_entry->name(),
                                             first_static_entry->value()));
  EXPECT_EQ(entry2, table_.GetByNameAndValue(first_static_entry->name(),
                                             "Value Four"));
  EXPECT_EQ(NULL, table_.GetByNameAndValue("key-1", "Not Present"));
  EXPECT_EQ(NULL, table_.GetByNameAndValue("not-present", "Value One"));

  // Evict |entry1|. Queries for its name & value now return the static entry.
  // |entry2| remains queryable.
  peer_.Evict(1);
  EXPECT_EQ(first_static_entry,
      table_.GetByNameAndValue(first_static_entry->name(),
                               first_static_entry->value()));
  EXPECT_EQ(entry2, table_.GetByNameAndValue(first_static_entry->name(),
                                             "Value Four"));

  // Evict |entry2|. Queries by its name & value are not found.
  peer_.Evict(1);
  EXPECT_EQ(NULL, table_.GetByNameAndValue(first_static_entry->name(),
                                           "Value Four"));
}

TEST_F(HpackHeaderTableTest, SetSizes) {
  string key = "key", value = "value";
  HpackEntry* entry1 = table_.TryAddEntry(key, value);
  HpackEntry* entry2 = table_.TryAddEntry(key, value);
  HpackEntry* entry3 = table_.TryAddEntry(key, value);

  // Set exactly large enough. No Evictions.
  size_t max_size = entry1->Size() + entry2->Size() + entry3->Size();
  table_.SetMaxSize(max_size);
  EXPECT_EQ(3u, peer_.dynamic_entries().size());

  // Set just too small. One eviction.
  max_size = entry1->Size() + entry2->Size() + entry3->Size() - 1;
  table_.SetMaxSize(max_size);
  EXPECT_EQ(2u, peer_.dynamic_entries().size());

  // Changing SETTINGS_HEADER_TABLE_SIZE doesn't affect table_.max_size(),
  // iff SETTINGS_HEADER_TABLE_SIZE >= |max_size|.
  EXPECT_EQ(kDefaultHeaderTableSizeSetting, table_.settings_size_bound());
  table_.SetSettingsHeaderTableSize(kDefaultHeaderTableSizeSetting*2);
  EXPECT_EQ(max_size, table_.max_size());
  table_.SetSettingsHeaderTableSize(max_size + 1);
  EXPECT_EQ(max_size, table_.max_size());
  EXPECT_EQ(2u, peer_.dynamic_entries().size());

  // SETTINGS_HEADER_TABLE_SIZE upper-bounds |table_.max_size()|,
  // and will force evictions.
  max_size = entry3->Size() - 1;
  table_.SetSettingsHeaderTableSize(max_size);
  EXPECT_EQ(max_size, table_.max_size());
  EXPECT_EQ(max_size, table_.settings_size_bound());
  EXPECT_EQ(0u, peer_.dynamic_entries().size());
}

TEST_F(HpackHeaderTableTest, ToggleReferenceSet) {
  HpackEntry* entry1 = table_.TryAddEntry("key-1", "Value One");
  HpackEntry* entry2 = table_.TryAddEntry("key-2", "Value Two");

  // Entries must be explictly toggled after creation.
  EXPECT_EQ(0u, table_.reference_set().size());

  // Add |entry1|.
  EXPECT_TRUE(table_.Toggle(entry1));
  EXPECT_EQ(1u, table_.reference_set().size());
  EXPECT_EQ(1u, table_.reference_set().count(entry1));
  EXPECT_EQ(0u, table_.reference_set().count(entry2));

  // Add |entry2|.
  EXPECT_TRUE(table_.Toggle(entry2));
  EXPECT_EQ(2u, table_.reference_set().size());
  EXPECT_EQ(1u, table_.reference_set().count(entry1));
  EXPECT_EQ(1u, table_.reference_set().count(entry2));

  // Remove |entry2|.
  EXPECT_FALSE(table_.Toggle(entry2));
  EXPECT_EQ(1u, table_.reference_set().size());
  EXPECT_EQ(0u, table_.reference_set().count(entry2));

  // Evict |entry1|. Implicit removal from reference set.
  peer_.Evict(1);
  EXPECT_EQ(0u, table_.reference_set().size());
}

TEST_F(HpackHeaderTableTest, ClearReferenceSet) {
  HpackEntry* entry1 = table_.TryAddEntry("key-1", "Value One");
  EXPECT_TRUE(table_.Toggle(entry1));
  entry1->set_state(123);

  // |entry1| state is cleared, and removed from the reference set.
  table_.ClearReferenceSet();
  EXPECT_EQ(0u, entry1->state());
  EXPECT_EQ(0u, table_.reference_set().size());
}

TEST_F(HpackHeaderTableTest, EvictionCountForEntry) {
  string key = "key", value = "value";
  HpackEntry* entry1 = table_.TryAddEntry(key, value);
  HpackEntry* entry2 = table_.TryAddEntry(key, value);
  size_t entry3_size = HpackEntry::Size(key, value);

  // Just enough capacity for third entry.
  table_.SetMaxSize(entry1->Size() + entry2->Size() + entry3_size);
  EXPECT_EQ(0u, peer_.EvictionCountForEntry(key, value));
  EXPECT_EQ(1u, peer_.EvictionCountForEntry(key, value + "x"));

  // No extra capacity. Third entry would force evictions.
  table_.SetMaxSize(entry1->Size() + entry2->Size());
  EXPECT_EQ(1u, peer_.EvictionCountForEntry(key, value));
  EXPECT_EQ(2u, peer_.EvictionCountForEntry(key, value + "x"));
}

TEST_F(HpackHeaderTableTest, EvictionCountToReclaim) {
  string key = "key", value = "value";
  HpackEntry* entry1 = table_.TryAddEntry(key, value);
  HpackEntry* entry2 = table_.TryAddEntry(key, value);

  EXPECT_EQ(1u, peer_.EvictionCountToReclaim(1));
  EXPECT_EQ(1u, peer_.EvictionCountToReclaim(entry1->Size()));
  EXPECT_EQ(2u, peer_.EvictionCountToReclaim(entry1->Size() + 1));
  EXPECT_EQ(2u, peer_.EvictionCountToReclaim(entry1->Size() + entry2->Size()));
}

// Fill a header table with entries. Make sure the entries are in
// reverse order in the header table.
TEST_F(HpackHeaderTableTest, TryAddEntryBasic) {
  EXPECT_EQ(0u, table_.size());
  EXPECT_EQ(table_.settings_size_bound(), table_.max_size());

  HpackEntryVector entries = MakeEntriesOfTotalSize(table_.max_size());

  // Most of the checks are in AddEntriesExpectNoEviction().
  AddEntriesExpectNoEviction(entries);
  EXPECT_EQ(table_.max_size(), table_.size());
  EXPECT_EQ(table_.settings_size_bound(), table_.size());
}

// Fill a header table with entries, and then ramp the table's max
// size down to evict an entry one at a time. Make sure the eviction
// happens as expected.
TEST_F(HpackHeaderTableTest, SetMaxSize) {
  HpackEntryVector entries = MakeEntriesOfTotalSize(
      kDefaultHeaderTableSizeSetting / 2);
  AddEntriesExpectNoEviction(entries);

  for (HpackEntryVector::iterator it = entries.begin();
       it != entries.end(); ++it) {
    size_t expected_count = distance(it, entries.end());
    EXPECT_EQ(expected_count, peer_.dynamic_entries().size());

    table_.SetMaxSize(table_.size() + 1);
    EXPECT_EQ(expected_count, peer_.dynamic_entries().size());

    table_.SetMaxSize(table_.size());
    EXPECT_EQ(expected_count, peer_.dynamic_entries().size());

    --expected_count;
    table_.SetMaxSize(table_.size() - 1);
    EXPECT_EQ(expected_count, peer_.dynamic_entries().size());
  }
  EXPECT_EQ(0u, table_.size());
}

// Fill a header table with entries, and then add an entry just big
// enough to cause eviction of all but one entry. Make sure the
// eviction happens as expected and the long entry is inserted into
// the table.
TEST_F(HpackHeaderTableTest, TryAddEntryEviction) {
  HpackEntryVector entries = MakeEntriesOfTotalSize(table_.max_size());
  AddEntriesExpectNoEviction(entries);

  HpackEntry* survivor_entry = table_.GetByIndex(1);
  HpackEntry long_entry =
      MakeEntryOfSize(table_.max_size() - survivor_entry->Size());

  // All entries but the first are to be evicted.
  EXPECT_EQ(peer_.dynamic_entries().size() - 1, peer_.EvictionSet(
      long_entry.name(), long_entry.value()).size());

  HpackEntry* new_entry = table_.TryAddEntry(long_entry.name(),
                                             long_entry.value());
  EXPECT_EQ(1u, table_.IndexOf(new_entry));
  EXPECT_EQ(2u, peer_.dynamic_entries().size());
  EXPECT_EQ(table_.GetByIndex(2), survivor_entry);
  EXPECT_EQ(table_.GetByIndex(1), new_entry);
}

// Fill a header table with entries, and then add an entry bigger than
// the entire table. Make sure no entry remains in the table.
TEST_F(HpackHeaderTableTest, TryAddTooLargeEntry) {
  HpackEntryVector entries = MakeEntriesOfTotalSize(table_.max_size());
  AddEntriesExpectNoEviction(entries);

  HpackEntry long_entry = MakeEntryOfSize(table_.max_size() + 1);

  // All entries are to be evicted.
  EXPECT_EQ(peer_.dynamic_entries().size(), peer_.EvictionSet(
      long_entry.name(), long_entry.value()).size());

  HpackEntry* new_entry = table_.TryAddEntry(long_entry.name(),
                                             long_entry.value());
  EXPECT_EQ(new_entry, static_cast<HpackEntry*>(NULL));
  EXPECT_EQ(0u, peer_.dynamic_entries().size());
}

TEST_F(HpackHeaderTableTest, ComparatorNameOrdering) {
  HpackEntry entry1(StaticEntry());
  name_[0]--;
  HpackEntry entry2(StaticEntry());

  HpackHeaderTable::EntryComparator comparator(&table_);
  EXPECT_FALSE(comparator(&entry1, &entry2));
  EXPECT_TRUE(comparator(&entry2, &entry1));
}

TEST_F(HpackHeaderTableTest, ComparatorValueOrdering) {
  HpackEntry entry1(StaticEntry());
  value_[0]--;
  HpackEntry entry2(StaticEntry());

  HpackHeaderTable::EntryComparator comparator(&table_);
  EXPECT_FALSE(comparator(&entry1, &entry2));
  EXPECT_TRUE(comparator(&entry2, &entry1));
}

TEST_F(HpackHeaderTableTest, ComparatorIndexOrdering) {
  HpackEntry entry1(StaticEntry());
  HpackEntry entry2(StaticEntry());

  HpackHeaderTable::EntryComparator comparator(&table_);
  EXPECT_TRUE(comparator(&entry1, &entry2));
  EXPECT_FALSE(comparator(&entry2, &entry1));

  HpackEntry entry3(DynamicEntry());
  HpackEntry entry4(DynamicEntry());

  // |entry4| has lower index than |entry3|.
  EXPECT_TRUE(comparator(&entry4, &entry3));
  EXPECT_FALSE(comparator(&entry3, &entry4));

  // |entry3| has lower index than |entry1|.
  EXPECT_TRUE(comparator(&entry3, &entry1));
  EXPECT_FALSE(comparator(&entry1, &entry3));

  // |entry1| & |entry2| ordering is preserved, though each Index() has changed.
  EXPECT_TRUE(comparator(&entry1, &entry2));
  EXPECT_FALSE(comparator(&entry2, &entry1));
}

TEST_F(HpackHeaderTableTest, ComparatorEqualityOrdering) {
  HpackEntry entry1(StaticEntry());
  HpackEntry entry2(DynamicEntry());

  HpackHeaderTable::EntryComparator comparator(&table_);
  EXPECT_FALSE(comparator(&entry1, &entry1));
  EXPECT_FALSE(comparator(&entry2, &entry2));
}

}  // namespace

}  // namespace net
