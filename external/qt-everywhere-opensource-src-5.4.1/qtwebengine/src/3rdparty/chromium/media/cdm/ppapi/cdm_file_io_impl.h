// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_PPAPI_CDM_FILE_IO_IMPL_H_
#define MEDIA_CDM_PPAPI_CDM_FILE_IO_IMPL_H_

#include <algorithm>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "media/cdm/ppapi/api/content_decryption_module.h"
#include "ppapi/c/ppb_file_io.h"
#include "ppapi/cpp/file_io.h"
#include "ppapi/cpp/file_ref.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/private/isolated_file_system_private.h"
#include "ppapi/utility/completion_callback_factory.h"

namespace media {

// Due to PPAPI limitations, all functions must be called on the main thread.
class CdmFileIOImpl : public cdm::FileIO {
 public:
  // A class that helps release |file_lock_map_|.
  // There should be only one instance of ResourceTracker in a process. Also,
  // ResourceTracker should outlive all CdmFileIOImpl instances.
  class ResourceTracker {
   public:
    ResourceTracker();
    ~ResourceTracker();
   private:
    DISALLOW_COPY_AND_ASSIGN(ResourceTracker);
  };

  CdmFileIOImpl(cdm::FileIOClient* client, PP_Instance pp_instance);

  // cdm::FileIO implementation.
  virtual void Open(const char* file_name, uint32_t file_name_size) OVERRIDE;
  virtual void Read() OVERRIDE;
  virtual void Write(const uint8_t* data, uint32_t data_size) OVERRIDE;
  virtual void Close() OVERRIDE;

 private:
  enum State {
    FILE_UNOPENED,
    OPENING_FILE_SYSTEM,
    OPENING_FILE,
    FILE_OPENED,
    READING_FILE,
    WRITING_FILE,
    FILE_CLOSED
  };

  enum ErrorType {
    OPEN_WHILE_IN_USE,
    READ_WHILE_IN_USE,
    WRITE_WHILE_IN_USE,
    OPEN_ERROR,
    READ_ERROR,
    WRITE_ERROR
  };

  // Always use Close() to release |this| object.
  virtual ~CdmFileIOImpl();

  // |file_id_| -> |is_file_lock_acquired_| map.
  // Design detail:
  // - We never erase an entry from this map.
  // - Pros: When the same file is read or written repeatedly, we don't need to
  //   insert/erase the entry repeatedly, which is expensive.
  // - Cons: If there are a lot of one-off files used, this map will be
  //   unnecessarily large. But this should be a rare case.
  // - Ideally we could use unordered_map for this. But unordered_set is only
  //   available in C++11.
  typedef std::map<std::string, bool> FileLockMap;

  // File lock map shared by all CdmFileIOImpl objects to prevent read/write
  // race. A CdmFileIOImpl object tries to acquire a lock before opening a
  // file. If the file open failed, the lock is released. Otherwise, the
  // CdmFileIOImpl object holds the lock until Close() is called.
  // TODO(xhwang): Investigate the following cases and make sure we are good:
  // - This assumes all CDM instances run in the same process for a given file
  //   system.
  // - When multiple CDM instances are running in different profiles (e.g.
  //   normal/incognito window, multiple profiles), we may be overlocking.
  static FileLockMap* file_lock_map_;

  // Sets |file_id_|. Returns false if |file_id_| cannot be set (e.g. origin URL
  // cannot be fetched).
  bool SetFileID();

  // Acquires the file lock. Returns true if the lock is successfully acquired.
  // After the lock is acquired, other cdm::FileIO objects in the same process
  // and in the same origin will get kInUse when trying to open the same file.
  bool AcquireFileLock();

  // Releases the file lock so that the file can be opened by other cdm::FileIO
  // objects.
  void ReleaseFileLock();

  void OpenFileSystem();
  void OnFileSystemOpened(int32_t result, pp::FileSystem file_system);
  void OpenFile();
  void OnFileOpened(int32_t result);
  void ReadFile();
  void OnFileRead(int32_t bytes_read);
  void SetLength(uint32_t length);
  void OnLengthSet(int32_t result);
  void WriteFile();
  void OnFileWritten(int32_t bytes_written);

  void CloseFile();

  // Calls client_->OnXxxxComplete with kError asynchronously. In some cases we
  // could actually call them synchronously, but since these errors shouldn't
  // happen in normal cases, we are not optimizing such cases.
  void OnError(ErrorType error_type);

  // Callback to notify client of error asynchronously.
  void NotifyClientOfError(int32_t result, ErrorType error_type);

  State state_;

  // Non-owning pointer.
  cdm::FileIOClient* const client_;

  const pp::InstanceHandle pp_instance_handle_;

  std::string file_name_;

  // A string ID that uniquely identifies a file in the user's profile.
  // It consists of the origin of the document URL (including scheme, host and
  // port, delimited by colons) and the |file_name_|.
  // For example: http:example.com:8080/foo_file.txt
  std::string file_id_;

  pp::IsolatedFileSystemPrivate isolated_file_system_;
  pp::FileSystem file_system_;
  pp::FileIO file_io_;
  pp::FileRef file_ref_;

  pp::CompletionCallbackFactory<CdmFileIOImpl> callback_factory_;

  // A temporary buffer to hold (partial) data to write or the data that has
  // been read. The size of |io_buffer_| is always "bytes to write" or "bytes to
  // read". Use "char" instead of "unit8_t" because PPB_FileIO uses char* for
  // binary data read and write.
  std::vector<char> io_buffer_;

  // Offset into the file for reading/writing data. When writing data to the
  // file, this is also the offset to the |io_buffer_|.
  size_t io_offset_;

  // Buffer to hold all read data requested. This buffer is passed to |client_|
  // when read completes.
  std::vector<char> cumulative_read_buffer_;

  DISALLOW_COPY_AND_ASSIGN(CdmFileIOImpl);
};

}  // namespace media

#endif  // MEDIA_CDM_PPAPI_CDM_FILE_IO_IMPL_H_
