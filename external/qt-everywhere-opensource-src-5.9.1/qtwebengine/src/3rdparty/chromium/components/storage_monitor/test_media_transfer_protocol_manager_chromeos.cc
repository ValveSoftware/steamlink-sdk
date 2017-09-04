// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/storage_monitor/test_media_transfer_protocol_manager_chromeos.h"

#include "device/media_transfer_protocol/mtp_file_entry.pb.h"
#include "device/media_transfer_protocol/mtp_storage_info.pb.h"

namespace storage_monitor {

TestMediaTransferProtocolManagerChromeOS::
    TestMediaTransferProtocolManagerChromeOS() {}

TestMediaTransferProtocolManagerChromeOS::
    ~TestMediaTransferProtocolManagerChromeOS() {}

void TestMediaTransferProtocolManagerChromeOS::AddObserver(Observer* observer) {
}

void TestMediaTransferProtocolManagerChromeOS::RemoveObserver(
    Observer* observer) {}

const std::vector<std::string>
TestMediaTransferProtocolManagerChromeOS::GetStorages() const {
  return std::vector<std::string>();
}
const MtpStorageInfo* TestMediaTransferProtocolManagerChromeOS::GetStorageInfo(
    const std::string& storage_name) const {
  return NULL;
}

void TestMediaTransferProtocolManagerChromeOS::GetStorageInfoFromDevice(
    const std::string& storage_name,
    const GetStorageInfoFromDeviceCallback& callback) {
  MtpStorageInfo mtp_storage_info;
  callback.Run(mtp_storage_info, true /* error */);
}

void TestMediaTransferProtocolManagerChromeOS::OpenStorage(
    const std::string& storage_name,
    const std::string& mode,
    const OpenStorageCallback& callback) {
  callback.Run("", true);
}

void TestMediaTransferProtocolManagerChromeOS::CloseStorage(
    const std::string& storage_handle,
    const CloseStorageCallback& callback) {
  callback.Run(true);
}

void TestMediaTransferProtocolManagerChromeOS::CreateDirectory(
    const std::string& storage_handle,
    const uint32_t parent_id,
    const std::string& directory_name,
    const CreateDirectoryCallback& callback) {
  callback.Run(true /* error */);
}

void TestMediaTransferProtocolManagerChromeOS::ReadDirectory(
    const std::string& storage_handle,
    const uint32_t file_id,
    const size_t max_size,
    const ReadDirectoryCallback& callback) {
  callback.Run(std::vector<MtpFileEntry>(), false /* no more entries*/,
               true /* error */);
}

void TestMediaTransferProtocolManagerChromeOS::ReadFileChunk(
    const std::string& storage_handle,
    uint32_t file_id,
    uint32_t offset,
    uint32_t count,
    const ReadFileCallback& callback) {
  callback.Run(std::string(), true);
}

void TestMediaTransferProtocolManagerChromeOS::GetFileInfo(
    const std::string& storage_handle,
    uint32_t file_id,
    const GetFileInfoCallback& callback) {
  callback.Run(MtpFileEntry(), true);
}

void TestMediaTransferProtocolManagerChromeOS::RenameObject(
    const std::string& storage_handle,
    const uint32_t object_id,
    const std::string& new_name,
    const RenameObjectCallback& callback) {
  callback.Run(true /* error */);
}

void TestMediaTransferProtocolManagerChromeOS::CopyFileFromLocal(
    const std::string& storage_handle,
    const int source_file_descriptor,
    const uint32_t parent_id,
    const std::string& file_name,
    const CopyFileFromLocalCallback& callback) {
  callback.Run(true /* error */);
}

void TestMediaTransferProtocolManagerChromeOS::DeleteObject(
    const std::string& storage_handle,
    const uint32_t object_id,
    const DeleteObjectCallback& callback) {
  callback.Run(true /* error */);
}

}  // namespace storage_monitor
