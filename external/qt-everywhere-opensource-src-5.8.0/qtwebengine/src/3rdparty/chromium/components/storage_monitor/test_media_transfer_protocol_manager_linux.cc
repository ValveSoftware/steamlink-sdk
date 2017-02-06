// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/storage_monitor/test_media_transfer_protocol_manager_linux.h"

#include "device/media_transfer_protocol/mtp_file_entry.pb.h"
#include "device/media_transfer_protocol/mtp_storage_info.pb.h"

namespace storage_monitor {

TestMediaTransferProtocolManagerLinux::
TestMediaTransferProtocolManagerLinux() {}

TestMediaTransferProtocolManagerLinux::
~TestMediaTransferProtocolManagerLinux() {}

void TestMediaTransferProtocolManagerLinux::AddObserver(Observer* observer) {}

void TestMediaTransferProtocolManagerLinux::RemoveObserver(
    Observer* observer) {}

const std::vector<std::string>
TestMediaTransferProtocolManagerLinux::GetStorages() const {
    return std::vector<std::string>();
}
const MtpStorageInfo* TestMediaTransferProtocolManagerLinux::GetStorageInfo(
    const std::string& storage_name) const {
  return NULL;
}

void TestMediaTransferProtocolManagerLinux::GetStorageInfoFromDevice(
    const std::string& storage_name,
    const GetStorageInfoFromDeviceCallback& callback) {
  MtpStorageInfo mtp_storage_info;
  callback.Run(mtp_storage_info, true /* error */);
}

void TestMediaTransferProtocolManagerLinux::OpenStorage(
    const std::string& storage_name,
    const std::string& mode,
    const OpenStorageCallback& callback) {
  callback.Run("", true);
}

void TestMediaTransferProtocolManagerLinux::CloseStorage(
    const std::string& storage_handle,
    const CloseStorageCallback& callback) {
  callback.Run(true);
}

void TestMediaTransferProtocolManagerLinux::CreateDirectory(
    const std::string& storage_handle,
    const uint32_t parent_id,
    const std::string& directory_name,
    const CreateDirectoryCallback& callback) {
  callback.Run(true /* error */);
}

void TestMediaTransferProtocolManagerLinux::ReadDirectory(
    const std::string& storage_handle,
    const uint32_t file_id,
    const size_t max_size,
    const ReadDirectoryCallback& callback) {
  callback.Run(std::vector<MtpFileEntry>(),
               false /* no more entries*/,
               true /* error */);
}

void TestMediaTransferProtocolManagerLinux::ReadFileChunk(
    const std::string& storage_handle,
    uint32_t file_id,
    uint32_t offset,
    uint32_t count,
    const ReadFileCallback& callback) {
  callback.Run(std::string(), true);
}

void TestMediaTransferProtocolManagerLinux::GetFileInfo(
    const std::string& storage_handle,
    uint32_t file_id,
    const GetFileInfoCallback& callback) {
  callback.Run(MtpFileEntry(), true);
}

void TestMediaTransferProtocolManagerLinux::RenameObject(
    const std::string& storage_handle,
    const uint32_t object_id,
    const std::string& new_name,
    const RenameObjectCallback& callback) {
  callback.Run(true /* error */);
}

void TestMediaTransferProtocolManagerLinux::CopyFileFromLocal(
    const std::string& storage_handle,
    const int source_file_descriptor,
    const uint32_t parent_id,
    const std::string& file_name,
    const CopyFileFromLocalCallback& callback) {
  callback.Run(true /* error */);
}

void TestMediaTransferProtocolManagerLinux::DeleteObject(
    const std::string& storage_handle,
    const uint32_t object_id,
    const DeleteObjectCallback& callback) {
  callback.Run(true /* error */);
}

}  // namespace storage_monitor
