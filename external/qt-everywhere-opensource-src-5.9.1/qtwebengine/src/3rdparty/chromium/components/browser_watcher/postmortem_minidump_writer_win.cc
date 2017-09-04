// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Note: aside from using windows headers to obtain the definitions of minidump
//     structures, nothing here is windows specific. This seems like the best
//     approach given this code is for temporary experimentation on Windows.
//     Longer term, Crashpad will take over the minidump writing in this case as
//     well.

#include "components/browser_watcher/postmortem_minidump_writer.h"

#include <windows.h>  // NOLINT
#include <dbghelp.h>

#include <map>
#include <type_traits>
#include <vector>

#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/numerics/safe_math.h"
#include "base/strings/string_piece.h"
#include "third_party/crashpad/crashpad/minidump/minidump_extensions.h"

namespace browser_watcher {

namespace {

// The stream type assigned to the minidump stream that holds the serialized
// stability report.
// Note: the value was obtained by adding 1 to the stream type used for holding
// the SyzyAsan proto.
// TODO(manzagop): centralize the stream type definitions to avoid issues.
const uint32_t kStabilityReportStreamType = 0x4B6B0002;

int64_t GetFileOffset(base::File* file) {
  DCHECK(file);
  return file->Seek(base::File::FROM_CURRENT, 0LL);
}

// Returns true if the file is empty, and false if the file is not empty or if
// there is an error.
bool IsFileEmpty(base::File* file) {
  DCHECK(file);
  int64_t end = file->Seek(base::File::FROM_END, 0LL);
  return end == 0LL;
}

// A class with functionality for writing minimal minidump containers to wrap
// postmortem stability reports.
// TODO(manzagop): remove this class once Crashpad takes over writing postmortem
//     minidumps.
// TODO(manzagop): revisit where the module information should be transported,
//     in the protocol buffer or in a module stream.
class PostmortemMinidumpWriter {
 public:
  PostmortemMinidumpWriter();
  ~PostmortemMinidumpWriter();

  // Write to |minidump_file| a minimal minidump that wraps |report|. Returns
  // true on success, false otherwise.
  // Note: the caller owns |minidump_file| and is responsible for keeping it
  // valid for this object's lifetime. |minidump_file| is expected to be empty
  // and a binary stream.
  bool WriteDump(base::PlatformFile minidump_file,
                 const StabilityReport& report,
                 const MinidumpInfo& minidump_info);

 private:
  // An offset within a minidump file. Note: using this type to avoid including
  // windows.h and relying on the RVA type.
  using FilePosition = uint32_t;

  // The minidump header is always located at the head.
  static const FilePosition kHeaderPos = 0U;

  bool WriteDumpImpl(const StabilityReport& report,
                     const MinidumpInfo& minidump_info);

  bool AppendCrashpadInfo(const crashpad::UUID& client_id,
                          const crashpad::UUID& report_id,
                          const std::map<std::string, std::string>& crash_keys);

  bool AppendCrashpadDictionaryEntry(
      const std::string& key,
      const std::string& value,
      std::vector<crashpad::MinidumpSimpleStringDictionaryEntry>* entries);

  // Allocate |size_bytes| within the minidump. On success, |pos| contains the
  // location of the allocation. Returns true on success, false otherwise.
  bool Allocate(size_t size_bytes, FilePosition* pos);

  // Seeks |cursor_|. The seek operation is kept separate from the write in
  // order to make the call explicit. Seek operations can be costly and should
  // be avoided.
  bool SeekCursor(FilePosition destination);

  // Write to pre-allocated space.
  // Note: |pos| must match |cursor_|.
  template <class DataType>
  bool Write(FilePosition pos, const DataType& data);
  bool WriteBytes(FilePosition pos, size_t size_bytes, const char* data);

  // Allocate space for and write the contents of |data|. On success, |pos|
  // contains the location of the write. Returns true on success, false
  // otherwise.
  template <class DataType>
  bool Append(const DataType& data, FilePosition* pos);
  template <class DataType>
  bool AppendVec(const std::vector<DataType>& data, FilePosition* pos);
  bool AppendUtf8String(base::StringPiece data, FilePosition* pos);
  bool AppendBytes(base::StringPiece data, FilePosition* pos);

  void RegisterDirectoryEntry(uint32_t stream_type,
                              FilePosition pos,
                              uint32_t size);

  // The next allocatable FilePosition.
  FilePosition next_available_byte_;

  // Storage for the directory during writes.
  std::vector<MINIDUMP_DIRECTORY> directory_;

  // The file to write to. Only valid within the scope of a call to WriteDump.
  base::File* minidump_file_;

  DISALLOW_COPY_AND_ASSIGN(PostmortemMinidumpWriter);
};

PostmortemMinidumpWriter::PostmortemMinidumpWriter()
    : next_available_byte_(0U), minidump_file_(nullptr) {}

PostmortemMinidumpWriter::~PostmortemMinidumpWriter() {
  DCHECK_EQ(nullptr, minidump_file_);
}

bool PostmortemMinidumpWriter::WriteDump(
    base::PlatformFile minidump_platform_file,
    const StabilityReport& report,
    const MinidumpInfo& minidump_info) {
  DCHECK_NE(base::kInvalidPlatformFile, minidump_platform_file);

  DCHECK_EQ(0U, next_available_byte_);
  DCHECK(directory_.empty());
  DCHECK_EQ(nullptr, minidump_file_);

  // We do not own |minidump_platform_file|, but we want to rely on base::File's
  // API, and so we need to duplicate it.
  HANDLE duplicated_handle;
  BOOL duplicate_success = ::DuplicateHandle(
      ::GetCurrentProcess(), minidump_platform_file, ::GetCurrentProcess(),
      &duplicated_handle, 0, FALSE, DUPLICATE_SAME_ACCESS);
  if (!duplicate_success)
    return false;
  base::File minidump_file(duplicated_handle);
  DCHECK(minidump_file.IsValid());
  minidump_file_ = &minidump_file;
  DCHECK_EQ(0LL, GetFileOffset(minidump_file_));
  DCHECK(IsFileEmpty(minidump_file_));

  // Write the minidump, then reset members.
  bool success = WriteDumpImpl(report, minidump_info);
  next_available_byte_ = 0U;
  directory_.clear();
  minidump_file_ = nullptr;

  return success;
}

bool PostmortemMinidumpWriter::WriteDumpImpl(
    const StabilityReport& report,
    const MinidumpInfo& minidump_info) {
  // Allocate space for the header and seek the cursor.
  FilePosition pos = 0U;
  if (!Allocate(sizeof(MINIDUMP_HEADER), &pos))
    return false;
  if (!SeekCursor(sizeof(MINIDUMP_HEADER)))
    return false;
  DCHECK_EQ(kHeaderPos, pos);

  // Write the proto to the file.
  std::string serialized_report;
  if (!report.SerializeToString(&serialized_report))
    return false;
  FilePosition report_pos = 0U;
  if (!AppendBytes(serialized_report, &report_pos))
    return false;

  // The directory entry for the stability report's stream.
  RegisterDirectoryEntry(kStabilityReportStreamType, report_pos,
                         serialized_report.length());

  // Write mandatory crash keys. These will be read by crashpad and used as
  // http request parameters for the upload. Keys and values should match
  // server side configuration.
  // TODO(manzagop): use product and version from the stability report. The
  // current executable's values are an (imperfect) proxy.
  std::map<std::string, std::string> crash_keys = {
      {"prod", minidump_info.product_name + "_Postmortem"},
      {"ver", minidump_info.version_number},
      {"channel", minidump_info.channel_name},
      {"plat", minidump_info.platform}};
  if (!AppendCrashpadInfo(minidump_info.client_id, minidump_info.report_id,
                          crash_keys))
    return false;

  // Write the directory.
  FilePosition directory_pos = 0U;
  if (!AppendVec(directory_, &directory_pos))
    return false;

  // Write the header.
  MINIDUMP_HEADER header;
  header.Signature = MINIDUMP_SIGNATURE;
  header.Version = MINIDUMP_VERSION;
  header.NumberOfStreams = directory_.size();
  header.StreamDirectoryRva = directory_pos;
  if (!SeekCursor(0U))
    return false;
  return Write(kHeaderPos, header);
}

bool PostmortemMinidumpWriter::AppendCrashpadInfo(
    const crashpad::UUID& client_id,
    const crashpad::UUID& report_id,
    const std::map<std::string, std::string>& crash_keys) {
  // Write the crash keys as the contents of a crashpad dictionary.
  std::vector<crashpad::MinidumpSimpleStringDictionaryEntry> entries;
  for (const auto& crash_key : crash_keys) {
    if (!AppendCrashpadDictionaryEntry(crash_key.first, crash_key.second,
                                       &entries)) {
      return false;
    }
  }

  // Write the dictionary's index.
  FilePosition dict_pos = 0U;
  uint32_t entry_count = entries.size();
  if (entry_count > 0) {
    if (!Append(entry_count, &dict_pos))
      return false;
    FilePosition unused_pos = 0U;
    if (!AppendVec(entries, &unused_pos))
      return false;
  }

  MINIDUMP_LOCATION_DESCRIPTOR simple_annotations = {0};
  simple_annotations.DataSize = 0U;
  if (entry_count > 0)
    simple_annotations.DataSize = next_available_byte_ - dict_pos;
  // Note: an RVA of 0 indicates the absence of a dictionary.
  simple_annotations.Rva = dict_pos;

  // Write the crashpad info.
  crashpad::MinidumpCrashpadInfo crashpad_info;
  crashpad_info.version = crashpad::MinidumpCrashpadInfo::kVersion;
  crashpad_info.report_id = report_id;
  crashpad_info.client_id = client_id;
  crashpad_info.simple_annotations = simple_annotations;
  // Note: module_list is left at 0, which means none.

  FilePosition crashpad_pos = 0U;
  if (!Append(crashpad_info, &crashpad_pos))
    return false;

  // Append a directory entry for the crashpad info stream.
  RegisterDirectoryEntry(crashpad::kMinidumpStreamTypeCrashpadInfo,
                         crashpad_pos, sizeof(crashpad::MinidumpCrashpadInfo));

  return true;
}

bool PostmortemMinidumpWriter::AppendCrashpadDictionaryEntry(
    const std::string& key,
    const std::string& value,
    std::vector<crashpad::MinidumpSimpleStringDictionaryEntry>* entries) {
  DCHECK_NE(nullptr, entries);

  FilePosition key_pos = 0U;
  if (!AppendUtf8String(key, &key_pos))
    return false;
  FilePosition value_pos = 0U;
  if (!AppendUtf8String(value, &value_pos))
    return false;

  crashpad::MinidumpSimpleStringDictionaryEntry entry = {0};
  entry.key = key_pos;
  entry.value = value_pos;
  entries->push_back(entry);

  return true;
}

bool PostmortemMinidumpWriter::Allocate(size_t size_bytes, FilePosition* pos) {
  DCHECK(pos);
  *pos = next_available_byte_;

  base::CheckedNumeric<FilePosition> next = next_available_byte_;
  next += size_bytes;
  if (!next.IsValid())
    return false;

  next_available_byte_ += size_bytes;
  return true;
}

bool PostmortemMinidumpWriter::SeekCursor(FilePosition destination) {
  DCHECK_NE(nullptr, minidump_file_);
  DCHECK(minidump_file_->IsValid());

  // Validate the write does not extend past the allocated space.
  if (destination > next_available_byte_)
    return false;

  int64_t new_pos = minidump_file_->Seek(base::File::FROM_BEGIN,
                                         static_cast<int64_t>(destination));
  return new_pos != -1;
}

template <class DataType>
bool PostmortemMinidumpWriter::Write(FilePosition pos, const DataType& data) {
  static_assert(std::is_trivially_copyable<DataType>::value,
                "restricted to trivially copyable");
  return WriteBytes(pos, sizeof(data), reinterpret_cast<const char*>(&data));
}

bool PostmortemMinidumpWriter::WriteBytes(FilePosition pos,
                                          size_t size_bytes,
                                          const char* data) {
  DCHECK(data);
  DCHECK_NE(nullptr, minidump_file_);
  DCHECK(minidump_file_->IsValid());
  DCHECK_EQ(static_cast<int64_t>(pos), GetFileOffset(minidump_file_));

  // Validate the write does not extend past the next available byte.
  base::CheckedNumeric<FilePosition> pos_end = pos;
  pos_end += size_bytes;
  if (!pos_end.IsValid() || pos_end.ValueOrDie() > next_available_byte_)
    return false;

  int size_bytes_signed = static_cast<int>(size_bytes);
  CHECK_LE(0, size_bytes_signed);

  int written_bytes =
      minidump_file_->WriteAtCurrentPos(data, size_bytes_signed);
  if (written_bytes < 0)
    return false;
  return static_cast<size_t>(written_bytes) == size_bytes;
}

template <class DataType>
bool PostmortemMinidumpWriter::Append(const DataType& data, FilePosition* pos) {
  static_assert(std::is_trivially_copyable<DataType>::value,
                "restricted to trivially copyable");
  DCHECK(pos);
  if (!Allocate(sizeof(data), pos))
    return false;
  return Write(*pos, data);
}

template <class DataType>
bool PostmortemMinidumpWriter::AppendVec(const std::vector<DataType>& data,
                                         FilePosition* pos) {
  static_assert(std::is_trivially_copyable<DataType>::value,
                "restricted to trivially copyable");
  DCHECK(!data.empty());
  DCHECK(pos);

  size_t size_bytes = sizeof(DataType) * data.size();
  if (!Allocate(size_bytes, pos))
    return false;
  return WriteBytes(*pos, size_bytes,
                    reinterpret_cast<const char*>(&data.at(0)));
}

bool PostmortemMinidumpWriter::AppendUtf8String(base::StringPiece data,
                                                FilePosition* pos) {
  DCHECK(pos);
  uint32_t string_size = data.size();
  if (!Append(string_size, pos))
    return false;

  FilePosition unused_pos = 0U;
  return AppendBytes(data, &unused_pos);
}

bool PostmortemMinidumpWriter::AppendBytes(base::StringPiece data,
                                           FilePosition* pos) {
  DCHECK(pos);
  if (!Allocate(data.length(), pos))
    return false;
  return WriteBytes(*pos, data.length(), data.data());
}

void PostmortemMinidumpWriter::RegisterDirectoryEntry(uint32_t stream_type,
                                                      FilePosition pos,
                                                      uint32_t size) {
  MINIDUMP_DIRECTORY entry = {0};
  entry.StreamType = stream_type;
  entry.Location.Rva = pos;
  entry.Location.DataSize = size;
  directory_.push_back(entry);
}

}  // namespace

MinidumpInfo::MinidumpInfo() {}

MinidumpInfo::~MinidumpInfo() {}

bool WritePostmortemDump(base::PlatformFile minidump_file,
                         const StabilityReport& report,
                         const MinidumpInfo& minidump_info) {
  PostmortemMinidumpWriter writer;
  return writer.WriteDump(minidump_file, report, minidump_info);
}

}  // namespace browser_watcher
