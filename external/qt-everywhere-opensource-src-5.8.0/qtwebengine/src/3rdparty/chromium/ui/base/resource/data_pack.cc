// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/resource/data_pack.h"

#include <errno.h>
#include <utility>

#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_piece.h"

// For details of the file layout, see
// http://dev.chromium.org/developers/design-documents/linuxresourcesandlocalizedstrings

namespace {

static const uint32_t kFileFormatVersion = 4;
// Length of file header: version, entry count and text encoding type.
static const size_t kHeaderLength = 2 * sizeof(uint32_t) + sizeof(uint8_t);

#pragma pack(push,2)
struct DataPackEntry {
  uint16_t resource_id;
  uint32_t file_offset;

  static int CompareById(const void* void_key, const void* void_entry) {
    uint16_t key = *reinterpret_cast<const uint16_t*>(void_key);
    const DataPackEntry* entry =
        reinterpret_cast<const DataPackEntry*>(void_entry);
    if (key < entry->resource_id) {
      return -1;
    } else if (key > entry->resource_id) {
      return 1;
    } else {
      return 0;
    }
  }
};
#pragma pack(pop)

static_assert(sizeof(DataPackEntry) == 6, "size of entry must be six");

// We're crashing when trying to load a pak file on Windows.  Add some error
// codes for logging.
// http://crbug.com/58056
enum LoadErrors {
  INIT_FAILED = 1,
  BAD_VERSION,
  INDEX_TRUNCATED,
  ENTRY_NOT_FOUND,
  HEADER_TRUNCATED,
  WRONG_ENCODING,
  INIT_FAILED_FROM_FILE,

  LOAD_ERRORS_COUNT,
};

}  // namespace

namespace ui {

DataPack::DataPack(ui::ScaleFactor scale_factor)
    : resource_count_(0),
      text_encoding_type_(BINARY),
      scale_factor_(scale_factor),
      has_only_material_design_assets_(false) {
}

DataPack::~DataPack() {
}

bool DataPack::LoadFromPath(const base::FilePath& path) {
  mmap_.reset(new base::MemoryMappedFile);
  if (!mmap_->Initialize(path)) {
    DLOG(ERROR) << "Failed to mmap datapack";
    UMA_HISTOGRAM_ENUMERATION("DataPack.Load", INIT_FAILED,
                              LOAD_ERRORS_COUNT);
    mmap_.reset();
    return false;
  }
  return LoadImpl();
}

bool DataPack::LoadFromFile(base::File file) {
  return LoadFromFileRegion(std::move(file),
                            base::MemoryMappedFile::Region::kWholeFile);
}

bool DataPack::LoadFromFileRegion(
    base::File file,
    const base::MemoryMappedFile::Region& region) {
  mmap_.reset(new base::MemoryMappedFile);
  if (!mmap_->Initialize(std::move(file), region)) {
    DLOG(ERROR) << "Failed to mmap datapack";
    UMA_HISTOGRAM_ENUMERATION("DataPack.Load", INIT_FAILED_FROM_FILE,
                              LOAD_ERRORS_COUNT);
    mmap_.reset();
    return false;
  }
  return LoadImpl();
}

bool DataPack::LoadImpl() {
  // Sanity check the header of the file.
  if (kHeaderLength > mmap_->length()) {
    DLOG(ERROR) << "Data pack file corruption: incomplete file header.";
    UMA_HISTOGRAM_ENUMERATION("DataPack.Load", HEADER_TRUNCATED,
                              LOAD_ERRORS_COUNT);
    mmap_.reset();
    return false;
  }

  // Parse the header of the file.
  // First uint32_t: version; second: resource count;
  const uint32_t* ptr = reinterpret_cast<const uint32_t*>(mmap_->data());
  uint32_t version = ptr[0];
  if (version != kFileFormatVersion) {
    LOG(ERROR) << "Bad data pack version: got " << version << ", expected "
               << kFileFormatVersion;
    UMA_HISTOGRAM_ENUMERATION("DataPack.Load", BAD_VERSION,
                              LOAD_ERRORS_COUNT);
    mmap_.reset();
    return false;
  }
  resource_count_ = ptr[1];

  // third: text encoding.
  const uint8_t* ptr_encoding = reinterpret_cast<const uint8_t*>(ptr + 2);
  text_encoding_type_ = static_cast<TextEncodingType>(*ptr_encoding);
  if (text_encoding_type_ != UTF8 && text_encoding_type_ != UTF16 &&
      text_encoding_type_ != BINARY) {
    LOG(ERROR) << "Bad data pack text encoding: got " << text_encoding_type_
               << ", expected between " << BINARY << " and " << UTF16;
    UMA_HISTOGRAM_ENUMERATION("DataPack.Load", WRONG_ENCODING,
                              LOAD_ERRORS_COUNT);
    mmap_.reset();
    return false;
  }

  // Sanity check the file.
  // 1) Check we have enough entries. There's an extra entry after the last item
  // which gives the length of the last item.
  if (kHeaderLength + (resource_count_ + 1) * sizeof(DataPackEntry) >
      mmap_->length()) {
    LOG(ERROR) << "Data pack file corruption: too short for number of "
                  "entries specified.";
    UMA_HISTOGRAM_ENUMERATION("DataPack.Load", INDEX_TRUNCATED,
                              LOAD_ERRORS_COUNT);
    mmap_.reset();
    return false;
  }
  // 2) Verify the entries are within the appropriate bounds. There's an extra
  // entry after the last item which gives us the length of the last item.
  for (size_t i = 0; i < resource_count_ + 1; ++i) {
    const DataPackEntry* entry = reinterpret_cast<const DataPackEntry*>(
        mmap_->data() + kHeaderLength + (i * sizeof(DataPackEntry)));
    if (entry->file_offset > mmap_->length()) {
      LOG(ERROR) << "Entry #" << i << " in data pack points off end of file. "
                 << "Was the file corrupted?";
      UMA_HISTOGRAM_ENUMERATION("DataPack.Load", ENTRY_NOT_FOUND,
                                LOAD_ERRORS_COUNT);
      mmap_.reset();
      return false;
    }
  }

  return true;
}

bool DataPack::HasResource(uint16_t resource_id) const {
  return !!bsearch(&resource_id, mmap_->data() + kHeaderLength, resource_count_,
                   sizeof(DataPackEntry), DataPackEntry::CompareById);
}

bool DataPack::GetStringPiece(uint16_t resource_id,
                              base::StringPiece* data) const {
  // It won't be hard to make this endian-agnostic, but it's not worth
  // bothering to do right now.
#if defined(__BYTE_ORDER)
  // Linux check
  static_assert(__BYTE_ORDER == __LITTLE_ENDIAN,
                "datapack assumes little endian");
#elif defined(__BIG_ENDIAN__)
  // Mac check
  #error DataPack assumes little endian
#endif

  const DataPackEntry* target = reinterpret_cast<const DataPackEntry*>(
      bsearch(&resource_id, mmap_->data() + kHeaderLength, resource_count_,
              sizeof(DataPackEntry), DataPackEntry::CompareById));
  if (!target) {
    return false;
  }

  const DataPackEntry* next_entry = target + 1;
  // If the next entry points beyond the end of the file this data pack's entry
  // table is corrupt. Log an error and return false. See
  // http://crbug.com/371301.
  if (next_entry->file_offset > mmap_->length()) {
    size_t entry_index = target -
        reinterpret_cast<const DataPackEntry*>(mmap_->data() + kHeaderLength);
    LOG(ERROR) << "Entry #" << entry_index << " in data pack points off end "
               << "of file. This should have been caught when loading. Was the "
               << "file modified?";
    return false;
  }

  size_t length = next_entry->file_offset - target->file_offset;
  data->set(reinterpret_cast<const char*>(mmap_->data() + target->file_offset),
            length);
  return true;
}

base::RefCountedStaticMemory* DataPack::GetStaticMemory(
    uint16_t resource_id) const {
  base::StringPiece piece;
  if (!GetStringPiece(resource_id, &piece))
    return NULL;

  return new base::RefCountedStaticMemory(piece.data(), piece.length());
}

ResourceHandle::TextEncodingType DataPack::GetTextEncodingType() const {
  return text_encoding_type_;
}

ui::ScaleFactor DataPack::GetScaleFactor() const {
  return scale_factor_;
}

bool DataPack::HasOnlyMaterialDesignAssets() const {
  return has_only_material_design_assets_;
}

#if DCHECK_IS_ON()
void DataPack::CheckForDuplicateResources(
    const ScopedVector<ResourceHandle>& packs) {
  for (size_t i = 0; i < resource_count_ + 1; ++i) {
    const DataPackEntry* entry = reinterpret_cast<const DataPackEntry*>(
        mmap_->data() + kHeaderLength + (i * sizeof(DataPackEntry)));
    const uint16_t resource_id = entry->resource_id;
    const float resource_scale = GetScaleForScaleFactor(scale_factor_);
    for (const ResourceHandle* handle : packs) {
      if (HasOnlyMaterialDesignAssets() !=
          handle->HasOnlyMaterialDesignAssets()) {
        continue;
      }
      if (GetScaleForScaleFactor(handle->GetScaleFactor()) != resource_scale)
        continue;
      DCHECK(!handle->HasResource(resource_id)) << "Duplicate resource "
                                                << resource_id << " with scale "
                                                << resource_scale;
    }
  }
}
#endif  // DCHECK_IS_ON()

// static
bool DataPack::WritePack(const base::FilePath& path,
                         const std::map<uint16_t, base::StringPiece>& resources,
                         TextEncodingType textEncodingType) {
  FILE* file = base::OpenFile(path, "wb");
  if (!file)
    return false;

  if (fwrite(&kFileFormatVersion, sizeof(kFileFormatVersion), 1, file) != 1) {
    LOG(ERROR) << "Failed to write file version";
    base::CloseFile(file);
    return false;
  }

  // Note: the python version of this function explicitly sorted keys, but
  // std::map is a sorted associative container, we shouldn't have to do that.
  uint32_t entry_count = resources.size();
  if (fwrite(&entry_count, sizeof(entry_count), 1, file) != 1) {
    LOG(ERROR) << "Failed to write entry count";
    base::CloseFile(file);
    return false;
  }

  if (textEncodingType != UTF8 && textEncodingType != UTF16 &&
      textEncodingType != BINARY) {
    LOG(ERROR) << "Invalid text encoding type, got " << textEncodingType
               << ", expected between " << BINARY << " and " << UTF16;
    base::CloseFile(file);
    return false;
  }

  uint8_t write_buffer = static_cast<uint8_t>(textEncodingType);
  if (fwrite(&write_buffer, sizeof(uint8_t), 1, file) != 1) {
    LOG(ERROR) << "Failed to write file text resources encoding";
    base::CloseFile(file);
    return false;
  }

  // Each entry is a uint16_t + a uint32_t. We have an extra entry after the
  // last item so we can compute the size of the list item.
  uint32_t index_length = (entry_count + 1) * sizeof(DataPackEntry);
  uint32_t data_offset = kHeaderLength + index_length;
  for (std::map<uint16_t, base::StringPiece>::const_iterator it =
           resources.begin();
       it != resources.end(); ++it) {
    uint16_t resource_id = it->first;
    if (fwrite(&resource_id, sizeof(resource_id), 1, file) != 1) {
      LOG(ERROR) << "Failed to write id for " << resource_id;
      base::CloseFile(file);
      return false;
    }

    if (fwrite(&data_offset, sizeof(data_offset), 1, file) != 1) {
      LOG(ERROR) << "Failed to write offset for " << resource_id;
      base::CloseFile(file);
      return false;
    }

    data_offset += it->second.length();
  }

  // We place an extra entry after the last item that allows us to read the
  // size of the last item.
  uint16_t resource_id = 0;
  if (fwrite(&resource_id, sizeof(resource_id), 1, file) != 1) {
    LOG(ERROR) << "Failed to write extra resource id.";
    base::CloseFile(file);
    return false;
  }

  if (fwrite(&data_offset, sizeof(data_offset), 1, file) != 1) {
    LOG(ERROR) << "Failed to write extra offset.";
    base::CloseFile(file);
    return false;
  }

  for (std::map<uint16_t, base::StringPiece>::const_iterator it =
           resources.begin();
       it != resources.end(); ++it) {
    if (fwrite(it->second.data(), it->second.length(), 1, file) != 1) {
      LOG(ERROR) << "Failed to write data for " << it->first;
      base::CloseFile(file);
      return false;
    }
  }

  base::CloseFile(file);

  return true;
}

}  // namespace ui
