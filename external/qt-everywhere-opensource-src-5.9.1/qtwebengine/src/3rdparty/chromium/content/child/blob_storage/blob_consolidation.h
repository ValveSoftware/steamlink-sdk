// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_BLOB_STORAGE_BLOB_CONSOLIDATION_H_
#define CONTENT_CHILD_BLOB_STORAGE_BLOB_CONSOLIDATION_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "storage/common/data_element.h"
#include "third_party/WebKit/public/platform/WebThreadSafeData.h"

namespace content {

// This class facilitates the consolidation of memory items in blobs. No memory
// is copied to store items in this object. Instead, the memory is copied into
// the external char* array given to the ReadMemory method.
// Features:
// * Add_Item methods for building the blob.
// * consolidated_items for getting the consolidated items. This is
//   used to describe the blob to the browser.
// * total_memory to get the total memory size of the blob.
// * ReadMemory for reading arbitrary memory from any consolidated item.
//
// NOTE: this class does not do memory accounting or garbage collecting. The
// memory for the blob sticks around until this class is destructed.
class CONTENT_EXPORT BlobConsolidation
    : public base::RefCountedThreadSafe<BlobConsolidation> {
 public:
  enum class ReadStatus {
    ERROR_UNKNOWN,
    ERROR_WRONG_TYPE,
    ERROR_OUT_OF_BOUNDS,
    CANCELLED_BY_VISITOR,
    OK
  };

  struct ConsolidatedItem {
    ConsolidatedItem();
    ConsolidatedItem(storage::DataElement::Type type,
                     uint64_t offset,
                     uint64_t length);
    ConsolidatedItem(const ConsolidatedItem& other);
    ~ConsolidatedItem();

    storage::DataElement::Type type;
    uint64_t offset;
    uint64_t length;

    base::FilePath path;                // For TYPE_FILE.
    GURL filesystem_url;                // For TYPE_FILE_FILESYSTEM.
    double expected_modification_time;  // For TYPE_FILE, TYPE_FILE_FILESYSTEM.
    std::string blob_uuid;              // For TYPE_BLOB.
    // Only populated if len(items) > 1.  Used for binary search.
    // Since the offset of the first item is always 0, we exclude this.
    std::vector<size_t> offsets;                 // For TYPE_BYTES.
    std::vector<blink::WebThreadSafeData> data;  // For TYPE_BYTES.
  };

  using MemoryVisitor =
      base::Callback<bool(const char* memory, size_t memory_size)>;

  BlobConsolidation();

  void AddDataItem(const blink::WebThreadSafeData& data);
  void AddFileItem(const base::FilePath& path,
                   uint64_t offset,
                   uint64_t length,
                   double expected_modification_time);
  void AddBlobItem(const std::string& uuid, uint64_t offset, uint64_t length);
  void AddFileSystemItem(const GURL& url,
                         uint64_t offset,
                         uint64_t length,
                         double expected_modification_time);

  // These are the resulting consolidated items, constructed from the Add*
  // methods. This configuration is used to describe the data to the browser,
  // even though one consolidated memory items can contain multiple data parts.
  const std::vector<ConsolidatedItem>& consolidated_items() const {
    return consolidated_items_;
  }

  // These are all of the blobs referenced in the construction of this blob.
  const std::set<std::string> referenced_blobs() const {
    return referenced_blobs_;
  }

  size_t total_memory() const { return total_memory_; }

  // This method calls the |visitor| callback with the given memory item,
  // offset, and size. Since items are consolidated, |visitor| can be called
  // multiple times. The return value of |visitor| determines if we should
  // continue reading memory, where 'false' triggers an early return and we
  // return EARLY_ABORT. |visitor| is guaranteed to be called only during this
  // method call.
  // Returns:
  // * ReadStatus::ERROR_UNKNOWN if the state or arguments are invalid,
  // * ReadStatus::ERROR_WRONG_TYPE if the item at the index isn't memory,
  // * ReadStatus::ERROR_OUT_OF_BOUNDS if index, offset, or size are invalid,
  // * ReadStatus::CANCELLED_BY_VISITOR if the visitor returns false before
  //   completion, and
  // * ReadStatus::DONE if the memory has been successfully visited.
  ReadStatus VisitMemory(size_t consolidated_item_index,
                         size_t consolidated_offset,
                         size_t consolidated_size,
                         const MemoryVisitor& visitor) const;

  // Reads memory from the given item into the given buffer. This is a simple
  // wrapper of VisitMemory. Returns the same values as VisitMemory, except we
  // don't return CANCELLED_BY_VISITOR.
  // Precondition: memory_out must be a valid pointer to memory with a size of
  //               at least consolidated_size.
  ReadStatus ReadMemory(size_t consolidated_item_index,
                        size_t consolidated_offset,
                        size_t consolidated_size,
                        void* memory_out) const;

 private:
  friend class base::RefCountedThreadSafe<BlobConsolidation>;
  ~BlobConsolidation();

  size_t total_memory_;
  std::set<std::string> referenced_blobs_;
  std::vector<ConsolidatedItem> consolidated_items_;

  DISALLOW_COPY_AND_ASSIGN(BlobConsolidation);
};

}  // namespace content
#endif  // CONTENT_CHILD_BLOB_STORAGE_BLOB_CONSOLIDATION_H_
