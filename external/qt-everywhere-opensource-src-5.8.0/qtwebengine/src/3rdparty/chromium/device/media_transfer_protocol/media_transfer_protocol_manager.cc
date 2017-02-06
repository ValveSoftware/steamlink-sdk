// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/media_transfer_protocol/media_transfer_protocol_manager.h"

#include <algorithm>
#include <map>
#include <memory>
#include <queue>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "dbus/bus.h"
#include "device/media_transfer_protocol/media_transfer_protocol_daemon_client.h"
#include "device/media_transfer_protocol/mtp_file_entry.pb.h"
#include "device/media_transfer_protocol/mtp_storage_info.pb.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/dbus_thread_manager.h"
#endif

namespace device {

namespace {

MediaTransferProtocolManager* g_media_transfer_protocol_manager = NULL;

// When reading directory entries, this is the number of entries for
// GetFileInfo() to read in one operation. If set too low, efficiency goes down
// slightly due to the overhead of D-Bus calls. If set too high, then slow
// devices may trigger a D-Bus timeout.
// The value below is a good initial estimate.
const size_t kFileInfoToFetchChunkSize = 25;

// On the first call to GetFileInfo, the offset to use is 0.
const size_t kInitialOffset = 0;

// The MediaTransferProtocolManager implementation.
class MediaTransferProtocolManagerImpl : public MediaTransferProtocolManager {
 public:
  explicit MediaTransferProtocolManagerImpl(
      scoped_refptr<base::SequencedTaskRunner> task_runner)
      : weak_ptr_factory_(this) {
#if defined(OS_CHROMEOS)
    DCHECK(!task_runner.get());
#else
    DCHECK(task_runner.get());
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    options.connection_type = dbus::Bus::PRIVATE;
    options.dbus_task_runner = task_runner;
    session_bus_ = new dbus::Bus(options);
#endif

    if (GetBus()) {
      // Listen for future mtpd service owner changes, in case it is not
      // available right now. There is no guarantee on Linux or ChromeOS that
      // mtpd is running already.
      mtpd_owner_changed_callback_ = base::Bind(
          &MediaTransferProtocolManagerImpl::FinishSetupOnOriginThread,
          weak_ptr_factory_.GetWeakPtr());
      GetBus()->ListenForServiceOwnerChange(mtpd::kMtpdServiceName,
                                            mtpd_owner_changed_callback_);
      GetBus()->GetServiceOwner(mtpd::kMtpdServiceName,
                                mtpd_owner_changed_callback_);
    }
  }

  ~MediaTransferProtocolManagerImpl() override {
    DCHECK(g_media_transfer_protocol_manager);
    g_media_transfer_protocol_manager = NULL;
    if (GetBus()) {
      GetBus()->UnlistenForServiceOwnerChange(mtpd::kMtpdServiceName,
                                              mtpd_owner_changed_callback_);
    }

#if !defined(OS_CHROMEOS)
    session_bus_->GetDBusTaskRunner()->PostTask(
        FROM_HERE, base::Bind(&dbus::Bus::ShutdownAndBlock, session_bus_));
#endif

    VLOG(1) << "MediaTransferProtocolManager Shutdown completed";
  }

  // MediaTransferProtocolManager override.
  void AddObserver(Observer* observer) override {
    DCHECK(thread_checker_.CalledOnValidThread());
    observers_.AddObserver(observer);
  }

  // MediaTransferProtocolManager override.
  void RemoveObserver(Observer* observer) override {
    DCHECK(thread_checker_.CalledOnValidThread());
    observers_.RemoveObserver(observer);
  }

  // MediaTransferProtocolManager override.
  const std::vector<std::string> GetStorages() const override {
    DCHECK(thread_checker_.CalledOnValidThread());
    std::vector<std::string> storages;
    for (StorageInfoMap::const_iterator it = storage_info_map_.begin();
         it != storage_info_map_.end();
         ++it) {
      storages.push_back(it->first);
    }
    return storages;
  }

  // MediaTransferProtocolManager override.
  const MtpStorageInfo* GetStorageInfo(
      const std::string& storage_name) const override {
    DCHECK(thread_checker_.CalledOnValidThread());
    StorageInfoMap::const_iterator it = storage_info_map_.find(storage_name);
    return it != storage_info_map_.end() ? &it->second : NULL;
  }

  // MediaTransferProtocolManager override.
  void GetStorageInfoFromDevice(
      const std::string& storage_name,
      const GetStorageInfoFromDeviceCallback& callback) override {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (!ContainsKey(storage_info_map_, storage_name) || !mtp_client_) {
      MtpStorageInfo info;
      callback.Run(info, true /* error */);
      return;
    }
    get_storage_info_from_device_callbacks_.push(callback);
    mtp_client_->GetStorageInfoFromDevice(
        storage_name,
        base::Bind(
            &MediaTransferProtocolManagerImpl::OnGetStorageInfoFromDevice,
            weak_ptr_factory_.GetWeakPtr()),
        base::Bind(
            &MediaTransferProtocolManagerImpl::OnGetStorageInfoFromDeviceError,
            weak_ptr_factory_.GetWeakPtr()));
  }

  // MediaTransferProtocolManager override.
  void OpenStorage(const std::string& storage_name,
                   const std::string& mode,
                   const OpenStorageCallback& callback) override {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (!ContainsKey(storage_info_map_, storage_name) || !mtp_client_) {
      callback.Run(std::string(), true);
      return;
    }
    open_storage_callbacks_.push(callback);
    mtp_client_->OpenStorage(
        storage_name,
        mode,
        base::Bind(&MediaTransferProtocolManagerImpl::OnOpenStorage,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&MediaTransferProtocolManagerImpl::OnOpenStorageError,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  // MediaTransferProtocolManager override.
  void CloseStorage(const std::string& storage_handle,
                    const CloseStorageCallback& callback) override {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (!ContainsKey(handles_, storage_handle) || !mtp_client_) {
      callback.Run(true);
      return;
    }
    close_storage_callbacks_.push(std::make_pair(callback, storage_handle));
    mtp_client_->CloseStorage(
        storage_handle,
        base::Bind(&MediaTransferProtocolManagerImpl::OnCloseStorage,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&MediaTransferProtocolManagerImpl::OnCloseStorageError,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  void CreateDirectory(const std::string& storage_handle,
                       const uint32_t parent_id,
                       const std::string& directory_name,
                       const CreateDirectoryCallback& callback) override {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (!ContainsKey(handles_, storage_handle) || !mtp_client_) {
      callback.Run(true /* error */);
      return;
    }
    create_directory_callbacks_.push(callback);
    mtp_client_->CreateDirectory(
        storage_handle, parent_id, directory_name,
        base::Bind(&MediaTransferProtocolManagerImpl::OnCreateDirectory,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&MediaTransferProtocolManagerImpl::OnCreateDirectoryError,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  // MediaTransferProtocolManager override.
  void ReadDirectory(const std::string& storage_handle,
                     const uint32_t file_id,
                     const size_t max_size,
                     const ReadDirectoryCallback& callback) override {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (!ContainsKey(handles_, storage_handle) || !mtp_client_) {
      callback.Run(std::vector<MtpFileEntry>(),
                   false /* no more entries */,
                   true /* error */);
      return;
    }
    read_directory_callbacks_.push(callback);
    mtp_client_->ReadDirectoryEntryIds(
        storage_handle, file_id,
        base::Bind(&MediaTransferProtocolManagerImpl::
                       OnReadDirectoryEntryIdsToReadDirectory,
                   weak_ptr_factory_.GetWeakPtr(), storage_handle, max_size),
        base::Bind(&MediaTransferProtocolManagerImpl::OnReadDirectoryError,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  // MediaTransferProtocolManager override.
  void ReadFileChunk(const std::string& storage_handle,
                     uint32_t file_id,
                     uint32_t offset,
                     uint32_t count,
                     const ReadFileCallback& callback) override {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (!ContainsKey(handles_, storage_handle) || !mtp_client_) {
      callback.Run(std::string(), true);
      return;
    }
    read_file_callbacks_.push(callback);
    mtp_client_->ReadFileChunk(
        storage_handle, file_id, offset, count,
        base::Bind(&MediaTransferProtocolManagerImpl::OnReadFile,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&MediaTransferProtocolManagerImpl::OnReadFileError,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  void GetFileInfo(const std::string& storage_handle,
                   uint32_t file_id,
                   const GetFileInfoCallback& callback) override {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (!ContainsKey(handles_, storage_handle) || !mtp_client_) {
      callback.Run(MtpFileEntry(), true);
      return;
    }
    std::vector<uint32_t> file_ids;
    file_ids.push_back(file_id);
    get_file_info_callbacks_.push(callback);
    mtp_client_->GetFileInfo(
        storage_handle,
        file_ids,
        kInitialOffset,
        file_ids.size(),
        base::Bind(&MediaTransferProtocolManagerImpl::OnGetFileInfo,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&MediaTransferProtocolManagerImpl::OnGetFileInfoError,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  void RenameObject(const std::string& storage_handle,
                    const uint32_t object_id,
                    const std::string& new_name,
                    const RenameObjectCallback& callback) override {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (!ContainsKey(handles_, storage_handle) || !mtp_client_) {
      callback.Run(true /* error */);
      return;
    }
    rename_object_callbacks_.push(callback);
    mtp_client_->RenameObject(
        storage_handle, object_id, new_name,
        base::Bind(&MediaTransferProtocolManagerImpl::OnRenameObject,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&MediaTransferProtocolManagerImpl::OnRenameObjectError,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  void CopyFileFromLocal(const std::string& storage_handle,
                         const int source_file_descriptor,
                         const uint32_t parent_id,
                         const std::string& file_name,
                         const CopyFileFromLocalCallback& callback) override {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (!ContainsKey(handles_, storage_handle) || !mtp_client_) {
      callback.Run(true /* error */);
      return;
    }
    copy_file_from_local_callbacks_.push(callback);
    mtp_client_->CopyFileFromLocal(
        storage_handle, source_file_descriptor, parent_id, file_name,
        base::Bind(&MediaTransferProtocolManagerImpl::OnCopyFileFromLocal,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&MediaTransferProtocolManagerImpl::OnCopyFileFromLocalError,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  void DeleteObject(const std::string& storage_handle,
                    const uint32_t object_id,
                    const DeleteObjectCallback& callback) override {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (!ContainsKey(handles_, storage_handle) || !mtp_client_) {
      callback.Run(true /* error */);
      return;
    }
    delete_object_callbacks_.push(callback);
    mtp_client_->DeleteObject(
        storage_handle, object_id,
        base::Bind(&MediaTransferProtocolManagerImpl::OnDeleteObject,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&MediaTransferProtocolManagerImpl::OnDeleteObjectError,
                   weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  // Map of storage names to storage info.
  typedef std::map<std::string, MtpStorageInfo> StorageInfoMap;
  typedef std::queue<GetStorageInfoFromDeviceCallback>
      GetStorageInfoFromDeviceCallbackQueue;
  // Callback queues - DBus communication is in-order, thus callbacks are
  // received in the same order as the requests.
  typedef std::queue<OpenStorageCallback> OpenStorageCallbackQueue;
  // (callback, handle)
  typedef std::queue<std::pair<CloseStorageCallback, std::string>
                    > CloseStorageCallbackQueue;
  typedef std::queue<CreateDirectoryCallback> CreateDirectoryCallbackQueue;
  typedef std::queue<ReadDirectoryCallback> ReadDirectoryCallbackQueue;
  typedef std::queue<ReadFileCallback> ReadFileCallbackQueue;
  typedef std::queue<GetFileInfoCallback> GetFileInfoCallbackQueue;
  typedef std::queue<RenameObjectCallback> RenameObjectCallbackQueue;
  typedef std::queue<CopyFileFromLocalCallback> CopyFileFromLocalCallbackQueue;
  typedef std::queue<DeleteObjectCallback> DeleteObjectCallbackQueue;

  void OnStorageAttached(const std::string& storage_name) {
    DCHECK(thread_checker_.CalledOnValidThread());
    mtp_client_->GetStorageInfo(
        storage_name,
        base::Bind(&MediaTransferProtocolManagerImpl::OnGetStorageInfo,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&base::DoNothing));
  }

  void OnStorageDetached(const std::string& storage_name) {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (storage_info_map_.erase(storage_name) == 0) {
      // This can happen for a storage where
      // MediaTransferProtocolDaemonClient::GetStorageInfo() failed.
      // Return to avoid giving observers phantom detach events.
      return;
    }
    FOR_EACH_OBSERVER(Observer,
                      observers_,
                      StorageChanged(false /* detach */, storage_name));
  }

  void OnStorageChanged(bool is_attach, const std::string& storage_name) {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK(mtp_client_);
    if (is_attach)
      OnStorageAttached(storage_name);
    else
      OnStorageDetached(storage_name);
  }

  void OnEnumerateStorages(const std::vector<std::string>& storage_names) {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK(mtp_client_);
    for (size_t i = 0; i < storage_names.size(); ++i) {
      if (ContainsKey(storage_info_map_, storage_names[i])) {
        // OnStorageChanged() might have gotten called first.
        continue;
      }
      OnStorageAttached(storage_names[i]);
    }
  }

  void OnGetStorageInfo(const MtpStorageInfo& storage_info) {
    DCHECK(thread_checker_.CalledOnValidThread());
    const std::string& storage_name = storage_info.storage_name();
    if (ContainsKey(storage_info_map_, storage_name)) {
      // This should not happen, since MediaTransferProtocolManagerImpl should
      // only call EnumerateStorages() once, which populates |storage_info_map_|
      // with the already-attached devices.
      // After that, all incoming signals are either for new storage
      // attachments, which should not be in |storage_info_map_|, or for
      // storage detachments, which do not add to |storage_info_map_|.
      // Return to avoid giving observers phantom detach events.
      NOTREACHED();
      return;
    }

    // New storage. Add it and let the observers know.
    storage_info_map_.insert(std::make_pair(storage_name, storage_info));
    FOR_EACH_OBSERVER(Observer,
                      observers_,
                      StorageChanged(true /* is attach */, storage_name));
  }

  void OnGetStorageInfoFromDevice(const MtpStorageInfo& storage_info) {
    get_storage_info_from_device_callbacks_.front().Run(storage_info,
                                                        false /* no error */);
    get_storage_info_from_device_callbacks_.pop();
  }

  void OnGetStorageInfoFromDeviceError() {
    MtpStorageInfo info;
    get_storage_info_from_device_callbacks_.front().Run(info, true /* error */);
    get_storage_info_from_device_callbacks_.pop();
  }

  void OnOpenStorage(const std::string& handle) {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (!ContainsKey(handles_, handle)) {
      handles_.insert(handle);
      open_storage_callbacks_.front().Run(handle, false);
    } else {
      NOTREACHED();
      open_storage_callbacks_.front().Run(std::string(), true);
    }
    open_storage_callbacks_.pop();
  }

  void OnOpenStorageError() {
    open_storage_callbacks_.front().Run(std::string(), true);
    open_storage_callbacks_.pop();
  }

  void OnCloseStorage() {
    DCHECK(thread_checker_.CalledOnValidThread());
    const std::string& handle = close_storage_callbacks_.front().second;
    if (ContainsKey(handles_, handle)) {
      handles_.erase(handle);
      close_storage_callbacks_.front().first.Run(false);
    } else {
      NOTREACHED();
      close_storage_callbacks_.front().first.Run(true);
    }
    close_storage_callbacks_.pop();
  }

  void OnCloseStorageError() {
    DCHECK(thread_checker_.CalledOnValidThread());
    close_storage_callbacks_.front().first.Run(true);
    close_storage_callbacks_.pop();
  }

  void OnCreateDirectory() {
    DCHECK(thread_checker_.CalledOnValidThread());
    create_directory_callbacks_.front().Run(false /* no error */);
    create_directory_callbacks_.pop();
  }

  void OnCreateDirectoryError() {
    DCHECK(thread_checker_.CalledOnValidThread());
    create_directory_callbacks_.front().Run(true /* error */);
    create_directory_callbacks_.pop();
  }

  void OnReadDirectoryEntryIdsToReadDirectory(
      const std::string& storage_handle,
      const size_t max_size,
      const std::vector<uint32_t>& file_ids) {
    DCHECK(thread_checker_.CalledOnValidThread());

    if (file_ids.empty()) {
      OnGotDirectoryEntries(storage_handle, file_ids, kInitialOffset, max_size,
                            file_ids, std::vector<MtpFileEntry>());
      return;
    }

    std::vector<uint32_t> sorted_file_ids = file_ids;
    std::sort(sorted_file_ids.begin(), sorted_file_ids.end());

    const size_t chunk_size =
        max_size == 0 ? kFileInfoToFetchChunkSize
                      : std::min(max_size, kFileInfoToFetchChunkSize);

    mtp_client_->GetFileInfo(
        storage_handle, file_ids, kInitialOffset, chunk_size,
        base::Bind(&MediaTransferProtocolManagerImpl::OnGotDirectoryEntries,
                   weak_ptr_factory_.GetWeakPtr(), storage_handle, file_ids,
                   kInitialOffset, max_size, sorted_file_ids),
        base::Bind(&MediaTransferProtocolManagerImpl::OnReadDirectoryError,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  void OnGotDirectoryEntries(const std::string& storage_handle,
                             const std::vector<uint32_t>& file_ids,
                             const size_t offset,
                             const size_t max_size,
                             const std::vector<uint32_t>& sorted_file_ids,
                             const std::vector<MtpFileEntry>& file_entries) {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK_EQ(file_ids.size(), sorted_file_ids.size());

    // Use |sorted_file_ids| to sanity check and make sure the results are a
    // subset of the requested file ids.
    for (size_t i = 0; i < file_entries.size(); ++i) {
      std::vector<uint32_t>::const_iterator it =
          std::lower_bound(sorted_file_ids.begin(), sorted_file_ids.end(),
                           file_entries[i].item_id());
      if (it == sorted_file_ids.end()) {
        OnReadDirectoryError();
        return;
      }
    }

    const size_t directory_size =
        max_size == 0 ? file_ids.size() : std::min(file_ids.size(), max_size);
    size_t next_offset = directory_size;
    if (offset < SIZE_MAX - kFileInfoToFetchChunkSize)
      next_offset = std::min(next_offset, offset + kFileInfoToFetchChunkSize);
    bool has_more = next_offset < directory_size;
    read_directory_callbacks_.front().Run(file_entries,
                                          has_more,
                                          false /* no error */);

    if (has_more) {
      const size_t chunk_size =
          std::min(directory_size - next_offset, kFileInfoToFetchChunkSize);

      mtp_client_->GetFileInfo(
          storage_handle, file_ids, next_offset, chunk_size,
          base::Bind(&MediaTransferProtocolManagerImpl::OnGotDirectoryEntries,
                     weak_ptr_factory_.GetWeakPtr(), storage_handle, file_ids,
                     next_offset, max_size, sorted_file_ids),
          base::Bind(&MediaTransferProtocolManagerImpl::OnReadDirectoryError,
                     weak_ptr_factory_.GetWeakPtr()));
      return;
    }
    read_directory_callbacks_.pop();
  }

  void OnReadDirectoryError() {
    DCHECK(thread_checker_.CalledOnValidThread());
    read_directory_callbacks_.front().Run(std::vector<MtpFileEntry>(),
                                          false /* no more entries */,
                                          true /* error */);
    read_directory_callbacks_.pop();
  }

  void OnReadFile(const std::string& data) {
    DCHECK(thread_checker_.CalledOnValidThread());
    read_file_callbacks_.front().Run(data, false);
    read_file_callbacks_.pop();
  }

  void OnReadFileError() {
    DCHECK(thread_checker_.CalledOnValidThread());
    read_file_callbacks_.front().Run(std::string(), true);
    read_file_callbacks_.pop();
  }

  void OnGetFileInfo(const std::vector<MtpFileEntry>& entries) {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (entries.size() == 1) {
      get_file_info_callbacks_.front().Run(entries[0], false /* no error */);
      get_file_info_callbacks_.pop();
    } else {
      OnGetFileInfoError();
    }
  }

  void OnGetFileInfoError() {
    DCHECK(thread_checker_.CalledOnValidThread());
    get_file_info_callbacks_.front().Run(MtpFileEntry(), true);
    get_file_info_callbacks_.pop();
  }

  void OnRenameObject() {
    DCHECK(thread_checker_.CalledOnValidThread());
    rename_object_callbacks_.front().Run(false /* no error */);
    rename_object_callbacks_.pop();
  }

  void OnRenameObjectError() {
    DCHECK(thread_checker_.CalledOnValidThread());
    rename_object_callbacks_.front().Run(true /* error */);
    rename_object_callbacks_.pop();
  }

  void OnCopyFileFromLocal() {
    DCHECK(thread_checker_.CalledOnValidThread());
    copy_file_from_local_callbacks_.front().Run(false /* no error */);
    copy_file_from_local_callbacks_.pop();
  }

  void OnCopyFileFromLocalError() {
    DCHECK(thread_checker_.CalledOnValidThread());
    copy_file_from_local_callbacks_.front().Run(true /* error */);
    copy_file_from_local_callbacks_.pop();
  }

  void OnDeleteObject() {
    DCHECK(thread_checker_.CalledOnValidThread());
    delete_object_callbacks_.front().Run(false /* no error */);
    delete_object_callbacks_.pop();
  }

  void OnDeleteObjectError() {
    DCHECK(thread_checker_.CalledOnValidThread());
    delete_object_callbacks_.front().Run(true /* error */);
    delete_object_callbacks_.pop();
  }

  // Get the Bus object used to communicate with mtpd.
  dbus::Bus* GetBus() {
    DCHECK(thread_checker_.CalledOnValidThread());
#if defined(OS_CHROMEOS)
    return chromeos::DBusThreadManager::Get()->GetSystemBus();
#else
    return session_bus_.get();
#endif
  }

  // Callback to finish initialization after figuring out if the mtpd service
  // has an owner, or if the service owner has changed.
  // |mtpd_service_owner| contains the name of the current owner, if any.
  void FinishSetupOnOriginThread(const std::string& mtpd_service_owner) {
    DCHECK(thread_checker_.CalledOnValidThread());

    if (mtpd_service_owner == current_mtpd_owner_)
      return;

    // In the case of a new service owner, clear |storage_info_map_|.
    // Assume all storages have been disconnected. If there is a new service
    // owner, reconnecting to it will reconnect all the storages as well.

    // Save a copy of |storage_info_map_| keys as |storage_info_map_| can
    // change in OnStorageDetached().
    std::vector<std::string> storage_names;
    for (StorageInfoMap::const_iterator it = storage_info_map_.begin();
         it != storage_info_map_.end();
         ++it) {
      storage_names.push_back(it->first);
    }
    for (size_t i = 0; i != storage_names.size(); ++i)
      OnStorageDetached(storage_names[i]);

    if (mtpd_service_owner.empty()) {
      current_mtpd_owner_.clear();
      mtp_client_.reset();
      return;
    }

    current_mtpd_owner_ = mtpd_service_owner;

    mtp_client_.reset(MediaTransferProtocolDaemonClient::Create(GetBus()));

    // Set up signals and start initializing |storage_info_map_|.
    mtp_client_->ListenForChanges(
        base::Bind(&MediaTransferProtocolManagerImpl::OnStorageChanged,
                   weak_ptr_factory_.GetWeakPtr()));
    mtp_client_->EnumerateStorages(
        base::Bind(&MediaTransferProtocolManagerImpl::OnEnumerateStorages,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&base::DoNothing));
  }

  // Mtpd DBus client.
  std::unique_ptr<MediaTransferProtocolDaemonClient> mtp_client_;

#if !defined(OS_CHROMEOS)
  // And a D-Bus session for talking to mtpd.
  scoped_refptr<dbus::Bus> session_bus_;
#endif

  // Device attachment / detachment observers.
  base::ObserverList<Observer> observers_;

  // Map to keep track of attached storages by name.
  StorageInfoMap storage_info_map_;

  // Set of open storage handles.
  std::set<std::string> handles_;

  dbus::Bus::GetServiceOwnerCallback mtpd_owner_changed_callback_;

  std::string current_mtpd_owner_;

  // Queued callbacks.
  GetStorageInfoFromDeviceCallbackQueue get_storage_info_from_device_callbacks_;
  OpenStorageCallbackQueue open_storage_callbacks_;
  CloseStorageCallbackQueue close_storage_callbacks_;
  CreateDirectoryCallbackQueue create_directory_callbacks_;
  ReadDirectoryCallbackQueue read_directory_callbacks_;
  ReadFileCallbackQueue read_file_callbacks_;
  GetFileInfoCallbackQueue get_file_info_callbacks_;
  RenameObjectCallbackQueue rename_object_callbacks_;
  CopyFileFromLocalCallbackQueue copy_file_from_local_callbacks_;
  DeleteObjectCallbackQueue delete_object_callbacks_;

  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<MediaTransferProtocolManagerImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaTransferProtocolManagerImpl);
};

}  // namespace

// static
MediaTransferProtocolManager* MediaTransferProtocolManager::Initialize(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  DCHECK(!g_media_transfer_protocol_manager);

  g_media_transfer_protocol_manager =
      new MediaTransferProtocolManagerImpl(task_runner);
  VLOG(1) << "MediaTransferProtocolManager initialized";

  return g_media_transfer_protocol_manager;
}

}  // namespace device
