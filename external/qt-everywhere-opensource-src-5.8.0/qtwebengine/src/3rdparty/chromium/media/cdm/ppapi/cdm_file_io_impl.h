// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_PPAPI_CDM_FILE_IO_IMPL_H_
#define MEDIA_CDM_PPAPI_CDM_FILE_IO_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/macros.h"
#include "media/cdm/api/content_decryption_module.h"
#include "ppapi/c/ppb_file_io.h"
#include "ppapi/cpp/file_io.h"
#include "ppapi/cpp/file_ref.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/private/isolated_file_system_private.h"
#include "ppapi/utility/completion_callback_factory.h"

namespace media {

// Due to PPAPI limitations, all functions must be called on the main thread.
//
// Implementation notes about states:
// 1, When a method is called in an invalid state (e.g. Read() before Open() is
//    called, Write() before Open() finishes or Open() after Open()), kError
//    will be returned. The state of |this| will not change.
// 2, When the file is opened by another CDM instance, or when we call Read()/
//    Write() during a pending Read()/Write(), kInUse will be returned. The
//    state of |this| will not change.
// 3, When a pepper operation failed (either synchronously or asynchronously),
//    kError will be returned. The state of |this| will be set to ERROR.
// 4. Any operation in ERROR state will end up with kError.
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

  // After the first successful file read, call |first_file_read_cb| to report
  // the file size. |first_file_read_cb| takes one parameter: the file size in
  // bytes.
  CdmFileIOImpl(cdm::FileIOClient* client,
                PP_Instance pp_instance,
                const pp::CompletionCallback& first_file_read_cb);

  // cdm::FileIO implementation.
  void Open(const char* file_name, uint32_t file_name_size) override;
  void Read() override;
  void Write(const uint8_t* data, uint32_t data_size) override;
  void Close() override;

 private:
  // TODO(xhwang): Introduce more detailed states for UMA logging if needed.
  enum State {
    STATE_UNOPENED,
    STATE_OPENING_FILE_SYSTEM,
    STATE_FILE_SYSTEM_OPENED,
    STATE_READING,
    STATE_WRITING,
    STATE_CLOSED,
    STATE_ERROR
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

  // Helper functions for Open().
  void OpenFileSystem();
  void OnFileSystemOpened(int32_t result, pp::FileSystem file_system);

  // Helper functions for Read().
  void OpenFileForRead();
  void OnFileOpenedForRead(int32_t result);
  void ReadFile();
  void OnFileRead(int32_t bytes_read);

  // Helper functions for Write(). We always write data to a temporary file,
  // then rename the temporary file to the target file. This can prevent data
  // corruption if |this| is Close()'ed while waiting for writing to complete.
  // However, if Close() is called after OpenTempFileForWrite() but before
  // RenameTempFile(), we may still end up with an empty, partially written or
  // fully written temporary file in the file system. This temporary file will
  // be truncated next time OpenTempFileForWrite() is called.

  void OpenTempFileForWrite();
  void OnTempFileOpenedForWrite(int32_t result);
  void WriteTempFile();
  void OnTempFileWritten(int32_t bytes_written);
  // Note: pp::FileRef::Rename() actually does a "move": if the target file
  // exists, Rename() will succeed and the target file will be overwritten.
  // See PepperInternalFileRefBackend::Rename() for implementation detail.
  void RenameTempFile();
  void OnTempFileRenamed(int32_t result);

  // Reset |this| to a clean state.
  void Reset();

  // For real open/read/write errors, Reset() and set the |state_| to ERROR.
  // Calls client_->OnXxxxComplete with kError or kInUse asynchronously. In some
  // cases we could actually call them synchronously, but since these errors
  // shouldn't happen in normal cases, we are not optimizing such cases.
  void OnError(ErrorType error_type);

  // Callback to notify client of error asynchronously.
  void NotifyClientOfError(int32_t result, ErrorType error_type);

  State state_;

  // Non-owning pointer.
  cdm::FileIOClient* const client_;

  const pp::InstanceHandle pp_instance_handle_;

  // Format: /<requested_file_name>
  std::string file_name_;

  // A string ID that uniquely identifies a file in the user's profile.
  // It consists of the origin of the document URL (including scheme, host and
  // port, delimited by colons) and the |file_name_|.
  // For example: http:example.com:8080/foo_file.txt
  std::string file_id_;

  pp::IsolatedFileSystemPrivate isolated_file_system_;
  pp::FileSystem file_system_;

  // Shared between read and write. During read, |file_ref_| refers to the real
  // file to read data from. During write, it refers to the temporary file to
  // write data into.
  pp::FileIO file_io_;
  pp::FileRef file_ref_;

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

  bool first_file_read_reported_;

  // Callback to report the file size in bytes after the first successful read.
  pp::CompletionCallback first_file_read_cb_;

  pp::CompletionCallbackFactory<CdmFileIOImpl> callback_factory_;

  DISALLOW_COPY_AND_ASSIGN(CdmFileIOImpl);
};

}  // namespace media

#endif  // MEDIA_CDM_PPAPI_CDM_FILE_IO_IMPL_H_
