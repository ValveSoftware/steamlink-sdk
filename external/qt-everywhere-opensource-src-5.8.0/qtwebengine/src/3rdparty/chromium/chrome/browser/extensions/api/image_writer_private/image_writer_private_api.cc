// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/browser/extensions/api/file_handlers/app_file_handler_util.h"
#include "chrome/browser/extensions/api/image_writer_private/error_messages.h"
#include "chrome/browser/extensions/api/image_writer_private/image_writer_private_api.h"
#include "chrome/browser/extensions/api/image_writer_private/operation_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"

namespace image_writer_api = extensions::api::image_writer_private;

namespace extensions {

ImageWriterPrivateWriteFromUrlFunction::
    ImageWriterPrivateWriteFromUrlFunction() {
}

ImageWriterPrivateWriteFromUrlFunction::
    ~ImageWriterPrivateWriteFromUrlFunction() {
}

bool ImageWriterPrivateWriteFromUrlFunction::RunAsync() {
  std::unique_ptr<image_writer_api::WriteFromUrl::Params> params(
      image_writer_api::WriteFromUrl::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  GURL url(params->image_url);
  if (!url.is_valid()) {
    error_ = image_writer::error::kUrlInvalid;
    return false;
  }

  std::string hash;
  if (params->options.get() && params->options->image_hash.get()) {
    hash = *params->options->image_hash;
  }

  image_writer::OperationManager::Get(GetProfile())->StartWriteFromUrl(
      extension_id(),
      url,
      hash,
      params->storage_unit_id,
      base::Bind(&ImageWriterPrivateWriteFromUrlFunction::OnWriteStarted,
                 this));
  return true;
}

void ImageWriterPrivateWriteFromUrlFunction::OnWriteStarted(
    bool success,
    const std::string& error) {
  if (!success) {
    error_ = error;
  }

  SendResponse(success);
}

ImageWriterPrivateWriteFromFileFunction::
    ImageWriterPrivateWriteFromFileFunction() {
}

ImageWriterPrivateWriteFromFileFunction::
    ~ImageWriterPrivateWriteFromFileFunction() {
}

bool ImageWriterPrivateWriteFromFileFunction::RunAsync() {
  std::string filesystem_name;
  std::string filesystem_path;
  std::string storage_unit_id;

  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &storage_unit_id));
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(1, &filesystem_name));
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(2, &filesystem_path));

  base::FilePath path;

  if (!extensions::app_file_handler_util::ValidateFileEntryAndGetPath(
          filesystem_name, filesystem_path,
          render_frame_host()->GetProcess()->GetID(), &path, &error_))
    return false;

  image_writer::OperationManager::Get(GetProfile())->StartWriteFromFile(
      extension_id(),
      path,
      storage_unit_id,
      base::Bind(&ImageWriterPrivateWriteFromFileFunction::OnWriteStarted,
                 this));
  return true;
}

void ImageWriterPrivateWriteFromFileFunction::OnWriteStarted(
    bool success,
    const std::string& error) {
  if (!success) {
    error_ = error;
  }
  SendResponse(success);
}

ImageWriterPrivateCancelWriteFunction::ImageWriterPrivateCancelWriteFunction() {
}

ImageWriterPrivateCancelWriteFunction::
    ~ImageWriterPrivateCancelWriteFunction() {
}

bool ImageWriterPrivateCancelWriteFunction::RunAsync() {
  image_writer::OperationManager::Get(GetProfile())->CancelWrite(
      extension_id(),
      base::Bind(&ImageWriterPrivateCancelWriteFunction::OnWriteCancelled,
                 this));
  return true;
}

void ImageWriterPrivateCancelWriteFunction::OnWriteCancelled(
    bool success,
    const std::string& error) {
  if (!success) {
    error_ = error;
  }
  SendResponse(success);
}

ImageWriterPrivateDestroyPartitionsFunction::
    ImageWriterPrivateDestroyPartitionsFunction() {
}

ImageWriterPrivateDestroyPartitionsFunction::
    ~ImageWriterPrivateDestroyPartitionsFunction() {
}

bool ImageWriterPrivateDestroyPartitionsFunction::RunAsync() {
  std::unique_ptr<image_writer_api::DestroyPartitions::Params> params(
      image_writer_api::DestroyPartitions::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  image_writer::OperationManager::Get(GetProfile())->DestroyPartitions(
      extension_id(),
      params->storage_unit_id,
      base::Bind(
          &ImageWriterPrivateDestroyPartitionsFunction::OnDestroyComplete,
          this));
  return true;
}

void ImageWriterPrivateDestroyPartitionsFunction::OnDestroyComplete(
    bool success,
    const std::string& error) {
  if (!success) {
    error_ = error;
  }

  SendResponse(success);
}

ImageWriterPrivateListRemovableStorageDevicesFunction::
  ImageWriterPrivateListRemovableStorageDevicesFunction() {
}

ImageWriterPrivateListRemovableStorageDevicesFunction::
  ~ImageWriterPrivateListRemovableStorageDevicesFunction() {
}

bool ImageWriterPrivateListRemovableStorageDevicesFunction::RunAsync() {
  RemovableStorageProvider::GetAllDevices(
    base::Bind(
      &ImageWriterPrivateListRemovableStorageDevicesFunction::OnDeviceListReady,
      this));
  return true;
}

void ImageWriterPrivateListRemovableStorageDevicesFunction::OnDeviceListReady(
    scoped_refptr<StorageDeviceList> device_list,
    bool success) {
  if (success) {
    results_ =
      image_writer_api::ListRemovableStorageDevices::Results::Create(
        device_list.get()->data);
    SendResponse(true);
  } else {
    error_ = image_writer::error::kDeviceListError;
    SendResponse(false);
  }
}

}  // namespace extensions
