// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/leak_detector/gnu_build_id_reader.h"

#include <elf.h>

#include <algorithm>

#if defined(OS_CHROMEOS)
#include <link.h>  // for dl_iterate_phdr
#else
#error "Getting binary mapping info is not supported on this platform."
#endif  // defined(OS_CHROMEOS)

namespace metrics {
namespace leak_detector {
namespace gnu_build_id_reader {

namespace {

// Contains data passed to dl_iterate_phdr() for reading build ID.
struct ReadBuildIDData {
  // Points to a vector for storing the build ID.
  std::vector<uint8_t>* build_id;
  // Indicates whether build ID was read successfully.
  bool success;
};

// Given a pointer and an offset, add the offset to the pointer and round it up
// to the next uint32_t.
const void* AlignPtrAndOffsetToUint32(const void* ptr, intptr_t offset) {
  uintptr_t addr = reinterpret_cast<uintptr_t>(ptr) + offset;
  uintptr_t aligned_addr =
      (addr + sizeof(uint32_t) - 1) & ~(sizeof(uint32_t) - 1);
  return reinterpret_cast<const void*>(aligned_addr);
}

// Searches for the ELF note containing the build ID within the data range
// specified by [start, end). Returns the build ID in |*result|. If the build ID
// is not found, |*result| will be unchanged. Returns true if the build ID was
// successfully read or false otherwise.
bool GetBuildIdFromNotes(const void* start,
                         const void* end,
                         std::vector<uint8_t>* result) {
  using NoteHeaderPtr = const ElfW(Nhdr)*;
  NoteHeaderPtr note = reinterpret_cast<NoteHeaderPtr>(start);

  while (note < end) {
    const char* name_ptr = reinterpret_cast<const char*>(note + 1);
    if (name_ptr >= end) {
      break;
    }
    // |desc_ptr| points the to the actual build ID data.
    const uint8_t* desc_ptr = reinterpret_cast<const uint8_t*>(
        AlignPtrAndOffsetToUint32(name_ptr, note->n_namesz));
    if (note->n_type == NT_GNU_BUILD_ID &&
        note->n_namesz == sizeof(ELF_NOTE_GNU) &&
        std::equal(name_ptr, name_ptr + sizeof(ELF_NOTE_GNU), ELF_NOTE_GNU)) {
      result->assign(desc_ptr, desc_ptr + note->n_descsz);
      return true;
    }
    NoteHeaderPtr next_ptr = reinterpret_cast<NoteHeaderPtr>(
        AlignPtrAndOffsetToUint32(desc_ptr, note->n_descsz));
    note = next_ptr;
  }
  return false;
}

// Callback for dl_iterate_phdr(). Finds the notes section and looks for the
// build ID in there. |data| points to a ReadBuildIDData struct whose |build_id|
// field should point to a valid std::vector<uint8_t>. Returns the build ID in
// that field, and sets |ReadBuildIDData::success| based on whether the build ID
// was successfully read.
//
// This function always returns 1 to signal the end of dl_iterate_phdr()'s
// iteration, because it is only interested in the first binary iterated by
// dl_iterate_phdr(), which is the current binary.
int FindNotesAndGetBuildID(struct dl_phdr_info* info,
                           size_t /* size */,
                           void* data) {
  uintptr_t mapping_addr = reinterpret_cast<uintptr_t>(info->dlpi_addr);
  const ElfW(Ehdr)* file_header =
      reinterpret_cast<const ElfW(Ehdr)*>(mapping_addr);

  // Make sure that a valid |mapping_addr| was read.
  if (!file_header || file_header->e_phentsize != sizeof(ElfW(Phdr))) {
    return 1;
  }

  // Find the ELF segment header for the NOTES section.
  for (int i = 0; i < info->dlpi_phnum; ++i) {
    const ElfW(Phdr)& segment_header = info->dlpi_phdr[i];
    if (segment_header.p_type == PT_NOTE) {
      const void* note = reinterpret_cast<const void*>(
          mapping_addr + segment_header.p_vaddr);
      const void* note_end = reinterpret_cast<const void*>(
          mapping_addr + segment_header.p_vaddr + segment_header.p_memsz);
      ReadBuildIDData* read_data = reinterpret_cast<ReadBuildIDData*>(data);
      read_data->success =
          GetBuildIdFromNotes(note, note_end, read_data->build_id);
    }
  }
  return 1;
}

}  // namespace

bool ReadBuildID(std::vector<uint8_t>* build_id) {
  ReadBuildIDData data;
  data.build_id = build_id;
  data.success = false;

#if defined(OS_CHROMEOS)
  dl_iterate_phdr(FindNotesAndGetBuildID, &data);
#endif  // defined(OS_CHROMEOS)

  return data.success;
}

}  // namespace gnu_build_id_reader
}  // namespace leak_detector
}  // namespace metrics
