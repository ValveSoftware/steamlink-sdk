// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_MEDIA_TRANSFER_PROTOCOL_MEDIA_TRANSFER_PROTOCOL_MANAGER_H_
#define DEVICE_MEDIA_TRANSFER_PROTOCOL_MEDIA_TRANSFER_PROTOCOL_MANAGER_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"

#if !defined(OS_LINUX)
#error "Only used on Linux and ChromeOS"
#endif

class MtpFileEntry;
class MtpStorageInfo;

namespace base {
class SequencedTaskRunner;
}

namespace device {

// This class handles the interaction with mtpd.
// Other classes can add themselves as observers.
class MediaTransferProtocolManager {
 public:
  // A callback to handle the result of GetStorageInfoFromDevice.
  // The first argument is the returned storage info.
  // The second argument is true if there was an error.
  typedef base::Callback<void(const MtpStorageInfo& storage_info,
                              const bool error)>
      GetStorageInfoFromDeviceCallback;

  // A callback to handle the result of OpenStorage.
  // The first argument is the returned handle.
  // The second argument is true if there was an error.
  typedef base::Callback<void(const std::string& handle,
                              bool error)> OpenStorageCallback;

  // A callback to handle the result of CloseStorage.
  // The argument is true if there was an error.
  typedef base::Callback<void(bool error)> CloseStorageCallback;

  // A callback to handle the result of CreateDirectory.
  // The first argument is true if there was an error.
  typedef base::Callback<void(bool error)> CreateDirectoryCallback;

  // A callback to handle the result of ReadDirectory.
  // The first argument is a vector of file entries.
  // The second argument is true if there are more file entries.
  // The third argument is true if there was an error.
  typedef base::Callback<void(const std::vector<MtpFileEntry>& file_entries,
                              bool has_more,
                              bool error)> ReadDirectoryCallback;

  // A callback to handle the result of ReadFileChunk.
  // The first argument is a string containing the file data.
  // The second argument is true if there was an error.
  typedef base::Callback<void(const std::string& data,
                              bool error)> ReadFileCallback;

  // A callback to handle the result of GetFileInfo.
  // The first argument is a file entry.
  // The second argument is true if there was an error.
  typedef base::Callback<void(const MtpFileEntry& file_entry,
                              bool error)> GetFileInfoCallback;

  // A callback to handle the result of RenameObject.
  // The first argument is true if there was an error.
  typedef base::Callback<void(bool error)> RenameObjectCallback;

  // A callback to handle the result of CopyFileFromLocal.
  // The first argument is true if there was an error.
  typedef base::Callback<void(bool error)> CopyFileFromLocalCallback;

  // A callback to handle the result of DeleteObject.
  // The first argument is true if there was an error.
  typedef base::Callback<void(bool error)> DeleteObjectCallback;

  // Implement this interface to be notified about MTP storage
  // attachment / detachment events.
  class Observer {
   public:
    virtual ~Observer() {}

    // A function called after a MTP storage has been attached / detached.
    virtual void StorageChanged(bool is_attached,
                                const std::string& storage_name) = 0;
  };

  virtual ~MediaTransferProtocolManager() {}

  // Adds an observer.
  virtual void AddObserver(Observer* observer) = 0;

  // Removes an observer.
  virtual void RemoveObserver(Observer* observer) = 0;

  // Returns a vector of available MTP storages.
  virtual const std::vector<std::string> GetStorages() const = 0;

  // On success, returns the metadata for |storage_name|.
  // Otherwise returns NULL.
  virtual const MtpStorageInfo* GetStorageInfo(
      const std::string& storage_name) const = 0;

  // Read the metadata of |storage_name| from device and runs |callback|.
  virtual void GetStorageInfoFromDevice(
      const std::string& storage_name,
      const GetStorageInfoFromDeviceCallback& callback) = 0;

  // Opens |storage_name| in |mode| and runs |callback|.
  virtual void OpenStorage(const std::string& storage_name,
                           const std::string& mode,
                           const OpenStorageCallback& callback) = 0;

  // Close |storage_handle| and runs |callback|.
  virtual void CloseStorage(const std::string& storage_handle,
                            const CloseStorageCallback& callback) = 0;

  // Creates |directory_name| in |parent_id|.
  virtual void CreateDirectory(const std::string& storage_handle,
                               const uint32_t parent_id,
                               const std::string& directory_name,
                               const CreateDirectoryCallback& callback) = 0;

  // Reads directory entries from |file_id| on |storage_handle| and runs
  // |callback|. |max_size| is a maximum number of files to be read.
  virtual void ReadDirectory(const std::string& storage_handle,
                             const uint32_t file_id,
                             const size_t max_size,
                             const ReadDirectoryCallback& callback) = 0;

  // Reads file data from |file_id| on |storage_handle| and runs |callback|.
  // Reads |count| bytes of data starting at |offset|.
  virtual void ReadFileChunk(const std::string& storage_handle,
                             uint32_t file_id,
                             uint32_t offset,
                             uint32_t count,
                             const ReadFileCallback& callback) = 0;

  // Gets the file metadata for |file_id| on |storage_handle| and runs
  // |callback|.
  virtual void GetFileInfo(const std::string& storage_handle,
                           uint32_t file_id,
                           const GetFileInfoCallback& callback) = 0;

  // Renames |object_id| to |new_name|.
  virtual void RenameObject(const std::string& storage_handle,
                            const uint32_t object_id,
                            const std::string& new_name,
                            const RenameObjectCallback& callback) = 0;

  // Copies the file from |source_file_descriptor| to |file_name| on
  // |parent_id|.
  virtual void CopyFileFromLocal(const std::string& storage_handle,
                                 const int source_file_descriptor,
                                 const uint32_t parent_id,
                                 const std::string& file_name,
                                 const CopyFileFromLocalCallback& callback) = 0;

  // Deletes |object_id|.
  virtual void DeleteObject(const std::string& storage_handle,
                            const uint32_t object_id,
                            const DeleteObjectCallback& callback) = 0;

  // Creates and returns the global MediaTransferProtocolManager instance.
  // On Linux, |task_runner| specifies the task runner to process asynchronous
  // operations.
  // On ChromeOS, |task_runner| should just be set to NULL because ChromeOS
  // already has a dedicated message loop proxy.
  static MediaTransferProtocolManager* Initialize(
      scoped_refptr<base::SequencedTaskRunner> task_runner);
};

}  // namespace device

#endif  // DEVICE_MEDIA_TRANSFER_PROTOCOL_MEDIA_TRANSFER_PROTOCOL_MANAGER_H_
